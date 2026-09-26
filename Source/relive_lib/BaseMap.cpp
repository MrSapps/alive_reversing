#include "BinaryPath.hpp"
#include "stdafx.h"
#include "BaseMap.hpp"
#include "Psx.hpp"
#include "../relive_lib/Engine.hpp"// DestroyObjects
#include "../AliveLibAE/Map.hpp"
#include "../AliveLibAO/Map.hpp"
#include "GameObjects/BaseAliveGameObject.hpp"
#include "data_conversion/relive_tlvs.hpp"
#include "data_conversion/string_util.hpp"
#include "Camera.hpp"
#include "Sound/Midi.hpp"
#include "GameObjects/ScreenManager.hpp"
#include "GameObjects/Particle.hpp"
#include "GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "PsxDisplay.hpp"
#include "Sfx.hpp"
#include "Sys.hpp"
#include "AmbientSound.hpp"
#include "logger.hpp"
#include <algorithm>

bool gMap_bDoPurpleLightEffect = false;

// Map Path_ChangeTLV::field_18_wipe to CameraSwapEffects
const CameraSwapEffects kPathChangeEffectToInternalScreenChangeEffect[10] = {
    CameraSwapEffects::ePlay1FMV_5,
    CameraSwapEffects::eRightToLeft_2,
    CameraSwapEffects::eLeftToRight_1,
    CameraSwapEffects::eBottomToTop_4,
    CameraSwapEffects::eTopToBottom_3,
    CameraSwapEffects::eBoxOut_8,
    CameraSwapEffects::eVerticalSplit_6,
    CameraSwapEffects::eHorizontalSplit_7,
    CameraSwapEffects::eUnknown_11,
    CameraSwapEffects::eInstantChange_0};

BinaryPath* BaseMap::GetPathResourceBlockPtr(u32 pathId)
{
    for (auto& loadedPath : mLoadedPaths)
    {
        if (loadedPath->GetPathId() == pathId)
        {
            return loadedPath.get();
        }
    }
    return nullptr;
}

void BaseMap::FreePathResourceBlocks()
{
    mLoadedPaths.clear();
}

void BaseMap::ClearPathResourceBlocks()
{
    mLoadedPaths.clear();
}

s16 BaseMap::SetActiveCameraDelayed(MapDirections direction, BaseAliveGameObject* pObj, s16 swapEffect)
{
    relive::Path_PathTransition* pPathChangeTLV = nullptr;
    CameraSwapEffects convertedSwapEffect = CameraSwapEffects::eInstantChange_0;
    if (pObj)
    {
        pPathChangeTLV = VTLV_Get_At_Of_Type(
            FP_GetExponent(pObj->mXPos),
            FP_GetExponent(pObj->mYPos),
            FP_GetExponent(pObj->mXPos),
            FP_GetExponent(pObj->mYPos),
            ReliveTypes::ePathTransition).GetTlv<relive::Path_PathTransition>();
    }

    if (pObj && pPathChangeTLV)
    {
        mNextLevel = pPathChangeTLV->mNextLevel;
        mNextPath = pPathChangeTLV->mNextPath;
        mNextCamera = pPathChangeTLV->mNextCamera;
        if (swapEffect < 0)
        {
            // Map the TLV/editor value of screen change to the internal screen change
            convertedSwapEffect = kPathChangeEffectToInternalScreenChangeEffect[pPathChangeTLV->mWipeEffect];
        }
        else
        {
            // If not negative then its an actual swap effect
            convertedSwapEffect = static_cast<CameraSwapEffects>(swapEffect);
        }
    }
    else
    {
        switch (direction)
        {
            case MapDirections::eMapLeft_0:
                if (!GetCamera(CameraPos::eCamLeft_3))
                {
                    return 0;
                }
                break;
            case MapDirections::eMapRight_1:
                if (!GetCamera(CameraPos::eCamRight_4))
                {
                    return 0;
                }
                break;
            case MapDirections::eMapBottom_3:
                if (!GetCamera(CameraPos::eCamBottom_2))
                {
                    return 0;
                }
                break;
            case MapDirections::eMapTop_2:
                if (!GetCamera(CameraPos::eCamTop_1))
                {
                    return 0;
                }
                break;
        }

        mNextPath = mCurrentPath;
        mNextLevel = mCurrentLevel;
        convertedSwapEffect = static_cast<CameraSwapEffects>(swapEffect); // TODO: Correct ??
    }

    mMapDirection = direction;
    mAliveObj = pObj;
    mCamState = CamChangeStates::eSliceCam_1;
    gMap_bDoPurpleLightEffect = false;

    if (convertedSwapEffect == CameraSwapEffects::ePlay1FMV_5 || convertedSwapEffect == CameraSwapEffects::eUnknown_11)
    {
        gMap_bDoPurpleLightEffect = true;
    }

    return 1;
}

Camera* BaseMap::GetCamera(CameraPos pos)
{
    return mCurrentCameras[static_cast<s32>(pos)];
}

Camera* BaseMap::Create_Camera(s16 xpos, s16 ypos, s32 /*a4*/)
{
    // Check min bound
    if (xpos < 0 || ypos < 0)
    {
        return nullptr;
    }

    // Check max bounds
    if (xpos >= mCamsOnX || ypos >= mCamsOnY)
    {
        return nullptr;
    }

    // Return existing camera if we already have one
    for (s32 i = 0; i < ALIVE_COUNTOF(mPreviousCameras); i++)
    {
        if (mPreviousCameras[i]
            && mPreviousCameras[i]->mLevel == mCurrentLevel
            && mPreviousCameras[i]->mPath == mCurrentPath
            && mPreviousCameras[i]->mCamXOff == xpos
            && mPreviousCameras[i]->mCamYOff == ypos)
        {
            Camera* pTemp = mPreviousCameras[i];
            mPreviousCameras[i] = nullptr;
            return pTemp;
        }
    }

    // Get a pointer to the camera name from the Path resource
    const BinaryPath* pPathData = GetPathResourceBlockPtr(mCurrentPath);
    if (!pPathData)
    {
        ALIVE_FATAL("Path %d isn't loaded", mCurrentPath);
    }
    const char* pCamName = pPathData->CameraName(xpos, ypos);

    // Empty/blank camera in the map array
    if (!pCamName || !pCamName[0])
    {
        return nullptr;
    }

    Camera* newCamera = relive_new Camera();

    newCamera->mCamXOff = xpos;
    newCamera->mCamYOff = ypos;

    newCamera->mCamResLoaded = false;

    newCamera->mLevel = mCurrentLevel;
    newCamera->mPath = mCurrentPath;
    newCamera->mCameraNumber = pPathData->CameraNameAsInteger(pCamName);

    return newCamera;
}

void BaseMap::Load_Path_Items(Camera* pCamera, relive::Factory::LoadMode loadMode)
{
    if (!pCamera)
    {
        return;
    }

    // Is camera resource loaded check
    if (!pCamera->mCamResLoaded)
    {
        if (loadMode == relive::Factory::LoadMode::ConstructObject_0)
        {
            // Async camera and FG1 load, Finish_Load_Cam and Create_FG1s use them once the main
            // loop has waited for them. A neighbouring camera's loads aren't waited for, they're
            // used if the camera becomes the current one.
            mResourceManager.PendCam(pCamera->mLevel, pCamera->mPath, pCamera->mCameraNumber);
            mResourceManager.PendFg1(pCamera->mLevel, pCamera->mPath, pCamera->mCameraNumber);

            mResourceManager.BeginAnimPins(pCamera->mAnimPins);
            GetPath().Loader(pCamera->mCamXOff, pCamera->mCamYOff, relive::Factory::LoadMode::LoadResourceFromList_1, ReliveTypes::eNone); // none = load all
            mResourceManager.EndAnimPins();
        }
        else
        {
            // Blocking camera load
            pCamera->mCamResLoaded = true;

            GetPath().Loader(pCamera->mCamXOff, pCamera->mCamYOff, relive::Factory::LoadMode::LoadResource_2, ReliveTypes::eNone); // none = load all
        }
    }
}

void BaseMap::Finish_Load_Cam(Camera* pCamera)
{
    if (!pCamera || pCamera->mCamResLoaded)
    {
        return;
    }

    // Was Camera::On_Loaded
    pCamera->mCamRes = mResourceManager.LoadCam(pCamera->mLevel, pCamera->mPath, pCamera->mCameraNumber);
    pCamera->mCamResLoaded = true;
}

void BaseMap::Free_Resources_For_Camera(Camera* pCamera)
{
    mResourceManager.FreeCam(pCamera->mLevel, pCamera->mPath, pCamera->mCameraNumber);
    mResourceManager.FreeFg1(pCamera->mLevel, pCamera->mPath, pCamera->mCameraNumber);
    mResourceManager.UnpinAnims(pCamera->mAnimPins);
}

void BaseMap::GetCurrentCamCoords(PSX_Point* pPoint)
{
    const PSX_Point gridSize = GetPath().VGetGridSize();
    pPoint->x = mCamIdxOnX * gridSize.x;
    pPoint->y = mCamIdxOnY * gridSize.y;
}

s32 MaxGridBlocks(FP scale)
{
    if (scale == FP_FromDouble(0.5))
    {
        return 30; // (29+1) * 13 (grid block size) for 377/390
    }
    else if (scale == FP_FromInteger(1))
    {
        return 16; // (15+1) * 25 (grid block size) for 375/400
    }
    else
    {
        LOG_WARNING("Scale should be 0.5 or 1 but got %f. This usually occurs when you die with DDCheat on.", FP_GetDouble(scale));
        return 0;
    }
}

bool BaseMap::SetActiveCam(EReliveLevelIds level, s16 path, s16 cam, CameraSwapEffects screenChangeEffect, FmvIds fmvIds, bool forceChange)
{
    if (!forceChange && cam == mCurrentCamera && level == mCurrentLevel && path == mCurrentPath)
    {
        return false;
    }

    mNextCamera = cam;
    mFmvIds = std::move(fmvIds);
    mFmvPending = mFmvIds.IsNone() ? 0 : 1;
    mNextPath = path;
    mNextLevel = level;
    mCameraSwapEffect = screenChangeEffect;
    mCamState = CamChangeStates::eInstantChange_2;

    if (screenChangeEffect == CameraSwapEffects::ePlay1FMV_5 || screenChangeEffect == CameraSwapEffects::eUnknown_11)
    {
        gMap_bDoPurpleLightEffect = true;
    }
    else
    {
        gMap_bDoPurpleLightEffect = false;
    }

    return true;
}

TlvIterator BaseMap::TLV_From_Offset_Lvl_Cam(const Guid& tlvId)
{
    return GetPath().TLV_From_Offset_Lvl_Cam(tlvId);
}

void BaseMap::Reset_TLVs(u16 pathId)
{
    GetPath().Reset_TLVs(pathId);
}

void BaseMap::Get_map_size(PSX_Point* pPoint)
{
    *pPoint = GetPath().VGetMapSize();
}

void BaseMap::Start_Sounds_For_Objects_In_Near_Cameras()
{
    SND_Reset_Ambiance();
    GetPath().Start_Sounds_For_Objects_In_Camera(CameraPos::eCamLeft_3, -1, 0);
    GetPath().Start_Sounds_For_Objects_In_Camera(CameraPos::eCamRight_4, 1, 0);
    GetPath().Start_Sounds_For_Objects_In_Camera(CameraPos::eCamTop_1, 0, -1);
    GetPath().Start_Sounds_For_Objects_In_Camera(CameraPos::eCamBottom_2, 0, 1);
}

void BaseMap::ReloadPathJsonRequest(const std::string& pathJsonFileName)
{
    for (auto& binaryPath : mLoadedPaths)
    {
        if (string_util::endsWith(pathJsonFileName, binaryPath->JsonFileName()))
        {
            LOG_INFO("Reloading paths...");
            mForceLoad = true;
            EReliveLevelIds oldCurrentLevel = mCurrentLevel;
            mNextLevel = oldCurrentLevel;
            mCurrentLevel = EReliveLevelIds::eNone;
            mCameraSwapEffect = CameraSwapEffects::eInstantChange_0; // prevent fmv playback
            DestroyObjects();
            mDirectCameraChangePending = true;
            return;
        }
    }
}

CameraPos BaseMap::GetDirection(EReliveLevelIds level, s32 path, FP xpos, FP ypos)
{
    if (level != mCurrentLevel)
    {
        return CameraPos::eCamInvalid_m1;
    }

    if (path != mCurrentPath)
    {
        return CameraPos::eCamInvalid_m1;
    }

    PSX_RECT rect = {};
    rect.x = FP_GetExponent(xpos);
    rect.w = FP_GetExponent(xpos);
    rect.y = FP_GetExponent(ypos);
    rect.h = FP_GetExponent(ypos);

    CameraPos ret = Rect_Location_Relative_To_Active_Camera(&rect);

    PSX_RECT camWorldRect = {};
    if (!Get_Camera_World_Rect(ret, &camWorldRect))
    {
        return CameraPos::eCamInvalid_m1;
    }

    const FP x = FP_FromInteger(camWorldRect.x);
    const FP y = FP_FromInteger(camWorldRect.y);
    const FP w = FP_FromInteger(camWorldRect.w);
    const FP h = FP_FromInteger(camWorldRect.h);

    switch (ret)
    {
        case CameraPos::eCamCurrent_0:
            return ret;

        case CameraPos::eCamTop_1:
            if (ypos < y || xpos < x || xpos > w)
            {
                return CameraPos::eCamInvalid_m1;
            }
            return ypos > h ? CameraPos::eCamCurrent_0 : ret;

        case CameraPos::eCamBottom_2:
            if (ypos > h || xpos < x || xpos > w)
            {
                return CameraPos::eCamInvalid_m1;
            }
            return ypos < y ? CameraPos::eCamCurrent_0 : ret;

        case CameraPos::eCamLeft_3:
            if (xpos < x || ypos < y || ypos > h)
            {
                return CameraPos::eCamInvalid_m1;
            }
            return xpos > w ? CameraPos::eCamCurrent_0 : ret;

        case CameraPos::eCamRight_4:
            if (xpos > w || ypos < y || ypos > h)
            {
                return CameraPos::eCamInvalid_m1;
            }
            return xpos < x ? CameraPos::eCamCurrent_0 : ret;

        default:
            return CameraPos::eCamInvalid_m1;
    }
}

void BaseMap::Create_FG1s()
{
    Camera* pCamera = mCurrentCameras[0];
    pCamera->CreateFG1(mResourceManager, *this);
}

ScreenChangeResult BaseMap::ScreenChange_Common()
{
    if (mCamState == CamChangeStates::eSliceCam_1)
    {
        if (Handle_PathTransition() == ScreenChangeResult::eWaiting)
        {
            return ScreenChangeResult::eWaiting;
        }
    }
    else if (mCamState == CamChangeStates::eInstantChange_2)
    {
        if (GoTo_Camera() == ScreenChangeResult::eWaiting)
        {
            return ScreenChangeResult::eWaiting;
        }
    }

    mCamState = CamChangeStates::eInactive_0;

    SND_Stop_Channels_Mask(mSoundChannelsMask);
    mSoundChannelsMask = 0;
    return ScreenChangeResult::eDone;
}

void BaseMap::TLV_Reset(const Guid& tlvId, s16 hiFlags)
{
    GetPath().TLV_Reset(tlvId, hiFlags);
}

void BaseMap::TLV_Persist(const Guid& tlvId, s16 hiFlags)
{
    GetPath().TLV_Persist(tlvId, hiFlags);
}

void BaseMap::TLV_Delete(const Guid& tlvId, s16 hiFlags)
{
    GetPath().TLV_Delete(tlvId, hiFlags);
}

void BaseMap::Set_TLVData(const Guid& tlvId, s16 hiFlags, s8 bSetCreated, s8 bSetDestroyed)
{
    GetPath().Set_TLVData(tlvId, hiFlags, bSetCreated, bSetDestroyed);
}

TlvIterator BaseMap::VTLV_Get_At_Of_Type(s16 xpos, s16 ypos, s16 width, s16 height, ReliveTypes typeToFind)
{
    return GetPath().VTLV_Get_At_Of_Type(xpos, ypos, width, height, typeToFind);
}

TlvIterator BaseMap::TLV_First_Of_Type_In_Camera(ReliveTypes objectType, s16 camX)
{
    return GetPath().TLV_First_Of_Type_In_Camera(objectType, camX);
}

TlvIterator BaseMap::TLV_Get_At(TlvIterator pTlv, FP xpos, FP ypos, FP width, FP height)
{
    return GetPath().TLV_Get_At(pTlv, xpos, ypos, width, height);
}

TlvIterator BaseMap::Get_First_TLV_For_Offsetted_Camera(s16 cam_x_idx, s16 cam_y_idx)
{
    return GetPath().Get_First_TLV_For_Offsetted_Camera(cam_x_idx, cam_y_idx);
}

void BaseMap::Reset()
{
    for (s32 i = 0; i < ALIVE_COUNTOF(mCurrentCameras); i++)
    {
        mCurrentCameras[i] = nullptr;
    }

    ClearPathResourceBlocks();

    mFreeAllAnimAndPalts = false;
    mPendingSaveRestore = nullptr;
    VClearPendingSaveRestore();
}

void BaseMap::Init(EReliveLevelIds level, s16 path, s16 camera, CameraSwapEffects screenChangeEffect, FmvIds fmvIds, bool forceChange)
{
    for (s32 i = 0; i < ALIVE_COUNTOF(mCurrentCameras); i++)
    {
        mCurrentCameras[i] = nullptr;
    }

    mOverlayId = -1;

    mCurrentCamera = -1;
    mCurrentPath = -1;
    mCurrentLevel = EReliveLevelIds::eNone;

    SetActiveCam(level, path, camera, screenChangeEffect, std::move(fmvIds), forceChange);
    mCamState = CamChangeStates::eInactive_0;
    mDirectCameraChangePending = true;
}

ScreenChangeResult BaseMap::ContinueDirectCameraChange()
{
    if (GoTo_Camera() == ScreenChangeResult::eWaiting)
    {
        return ScreenChangeResult::eWaiting;
    }

    mDirectCameraChangePending = false;
    return ScreenChangeResult::eDone;
}

void BaseMap::Shutdown()
{
    // Free Path resources
    FreePathResourceBlocks();

    // Free cameras
    for (s32 i = 0; i < ALIVE_COUNTOF(mCurrentCameras); i++)
    {
        if (mCurrentCameras[i])
        {
            relive_delete mCurrentCameras[i];
            mCurrentCameras[i] = nullptr;
        }
    }

    gScreenManager = nullptr;

    // Free
    GetPath().Free();

    Reset();
}

void BaseMap::AddPurpleLight(BaseAnimatedWithPhysicsGameObject* pObj, DynamicArrayT<BaseAnimatedWithPhysicsGameObject>& objects, DynamicArrayT<Particle>& lights)
{
    objects.Push_Back(pObj);

    const PSX_RECT objRect = pObj->VGetBoundingRect();

    const FP k60Scaled = pObj->GetSpriteScale() * FP_FromInteger(60);
    Particle* pPurpleLight = New_DestroyOrCreateObject_Particle(
        FP_FromInteger((objRect.x + objRect.w) / 2),
        FP_FromInteger((objRect.y + objRect.h) / 2) + k60Scaled,
        pObj->GetSpriteScale(), mResourceManager, *this);

    if (pPurpleLight)
    {
        lights.Push_Back(pPurpleLight);
    }
}

// Shows purple lights on the objects that have them for a few frames, the game frozen
// behind it. With bMakeInvisible the objects vanish while it plays (e.g. before a
// teleport), otherwise they reappear.
class PurpleLightEffect final : public BaseGameObject
{
public:
    PurpleLightEffect(DynamicArrayT<BaseAnimatedWithPhysicsGameObject>* pObjectsWithLights, DynamicArrayT<Particle>* pPurpleLights, s16 bMakeInvisible, s32 totalFrames, ResourceManagerWrapper& resMan, BaseMap& map)
        : BaseGameObject(true, 0, resMan, map)
        , mObjectsWithLights(pObjectsWithLights)
        , mPurpleLights(pPurpleLights)
        , mMakeInvisible(bMakeInvisible)
        , mTotalFrames(totalFrames)
    {
        SetSurviveDeathReset(true);
        SetUpdateDuringCamSwap(true);
        StartModal();
    }

    ~PurpleLightEffect()
    {
        DeleteArrays();
    }

    void VScreenChanged() override
    {
        // Can be part of a screen change, so must outlive it
    }

    ModalState VModalUpdate() override
    {
        if (mMakeInvisible && mCounter == 4)
        {
            // Make all the objects that have lights invisible now that the lights have been rendered for a few frames
            for (s32 i = 0; i < mObjectsWithLights->Size(); i++)
            {
                BaseAnimatedWithPhysicsGameObject* pObj = mObjectsWithLights->ItemAt(i);
                if (!pObj)
                {
                    break;
                }
                pObj->GetAnimation().SetRender(false);
            }
        }

        for (s32 i = 0; i < mPurpleLights->Size(); i++)
        {
            Particle* pLight = mPurpleLights->ItemAt(i);
            if (!pLight)
            {
                break;
            }

            if (!pLight->GetDead())
            {
                pLight->VUpdate();
            }
        }

        // TODO/HACK what is the point of the f64 loop? Why not do both in 1 iteration ??
        for (s32 i = 0; i < mPurpleLights->Size(); i++)
        {
            Particle* pLight = mPurpleLights->ItemAt(i);
            if (!pLight)
            {
                break;
            }

            if (!pLight->GetDead())
            {
                pLight->GetAnimation().VDecode();
            }
        }

        for (s32 i = 0; i < gObjListDrawables->Size(); i++)
        {
            BaseGameObject* pDrawable = gObjListDrawables->ItemAt(i);
            if (!pDrawable)
            {
                break;
            }

            if (!pDrawable->GetDead())
            {
                // TODO: Seems strange to check this flag, how did it get in the drawable list if its not a drawable ??
                if (pDrawable->GetDrawable())
                {
                    pDrawable->VRender(gPsxDisplay.mDrawEnv.mOrderingTable);
                }
            }
        }

        gScreenManager->VRender(gPsxDisplay.mDrawEnv.mOrderingTable);
        gPsxDisplay.RenderOrderingTable();

        if (++mCounter < mTotalFrames)
        {
            return ModalState::eRunning;
        }

        if (mMakeInvisible)
        {
            // Make all the objects that had lights visible again
            for (s32 i = 0; i < mObjectsWithLights->Size(); i++)
            {
                BaseAnimatedWithPhysicsGameObject* pObj = mObjectsWithLights->ItemAt(i);
                if (!pObj)
                {
                    break;
                }
                pObj->GetAnimation().SetRender(true);
            }
        }

        DeleteArrays();
        SetDead(true);

        if (!mMakeInvisible)
        {
            // Shown at the end of a camera swap
            mMap.VCameraSwapFinished();
        }
        return ModalState::eFinished;
    }

private:
    void DeleteArrays()
    {
        if (mObjectsWithLights)
        {
            mObjectsWithLights->mUsedSize = 0;
            mPurpleLights->mUsedSize = 0;

            relive_delete mObjectsWithLights;
            relive_delete mPurpleLights;
            mObjectsWithLights = nullptr;
            mPurpleLights = nullptr;
        }
    }

    DynamicArrayT<BaseAnimatedWithPhysicsGameObject>* mObjectsWithLights = nullptr;
    DynamicArrayT<Particle>* mPurpleLights = nullptr;
    const s16 mMakeInvisible = 0;
    const s32 mTotalFrames = 0;
    s32 mCounter = 0;
};

PurpleLightResult BaseMap::RemoveObjectsWithPurpleLight(s16 bMakeInvisible)
{
    auto pObjectsWithLightsArray = relive_new DynamicArrayT<BaseAnimatedWithPhysicsGameObject>(16);

    auto pPurpleLightArray = relive_new DynamicArrayT<Particle>(16);

    VCollectPurpleLightObjects(*pObjectsWithLightsArray, *pPurpleLightArray);

    const s32 kTotal = VPurpleLightFrameCount(bMakeInvisible);
    if (pPurpleLightArray->IsEmpty() || kTotal <= 0)
    {
        pObjectsWithLightsArray->mUsedSize = 0;
        pPurpleLightArray->mUsedSize = 0;

        relive_delete pObjectsWithLightsArray;
        relive_delete pPurpleLightArray;
        return PurpleLightResult::eNoLights;
    }

    SFX_Play_Pitch(relive::SoundEffects::PossessEffect, 40, 2400);

    relive_new PurpleLightEffect(pObjectsWithLightsArray, pPurpleLightArray, bMakeInvisible, kTotal, mResourceManager, *this);
    return PurpleLightResult::eShowing;
}

// See BaseMap::RunFmvCameraChangePass
class FmvCameraChangePass final : public BaseGameObject
{
public:
    FmvCameraChangePass(BaseGameObject* pFmvSwapper, bool bKeepCantKill, ResourceManagerWrapper& resMan, BaseMap& map)
        : BaseGameObject(true, 0, resMan, map)
        , mFmvSwapper(pFmvSwapper)
        , mKeepCantKill(bKeepCantKill)
    {
        SetSurviveDeathReset(true);
        SetUpdateDuringCamSwap(true);
        StartModal();
    }

    void VScreenChanged() override
    {
        // Must outlive the screen change it's part of
    }

    ModalState VModalUpdate() override
    {
        for (; mIdx < gBaseGameObjects->Size(); mIdx++)
        {
            BaseGameObject* pBaseGameObj = gBaseGameObjects->ItemAt(mIdx);
            if (!pBaseGameObj)
            {
                break;
            }

            if (pBaseGameObj == this)
            {
                continue;
            }

            if (pBaseGameObj->GetDead() && !(mKeepCantKill && pBaseGameObj->GetCantKill()))
            {
                mIdx = gBaseGameObjects->RemoveAt(mIdx);
                relive_delete pBaseGameObj;
                if (pBaseGameObj == mFmvSwapper)
                {
                    // FMV trans done
                    break;
                }
            }
            else if (pBaseGameObj->GetUpdatable())
            {
                if (!pBaseGameObj->GetDead() && (!gNumCamSwappers || pBaseGameObj->GetUpdateDuringCamSwap()))
                {
                    const s32 updateDelay = pBaseGameObj->UpdateDelay();
                    if (updateDelay > 0)
                    {
                        pBaseGameObj->SetUpdateDelay(updateDelay - 1);
                    }
                    else
                    {
                        pBaseGameObj->VUpdate();
                    }
                }

                if (mMap.GetActiveModal() != this)
                {
                    // A movie took over, carry on with the next object once it's done
                    mIdx++;
                    return ModalState::eRunning;
                }
            }
        }

        // The screen change that started this carries on now, see GoTo_Camera
        SetDead(true);
        return ModalState::eFinished;
    }

private:
    BaseGameObject* mFmvSwapper = nullptr;
    const bool mKeepCantKill = false;
    s32 mIdx = 0;
};

void BaseMap::RunFmvCameraChangePass(BaseGameObject* pFmvSwapper, bool bKeepCantKill)
{
    relive_new FmvCameraChangePass(pFmvSwapper, bKeepCantKill, mResourceManager, *this);
}

void BaseMap::AddModal(BaseGameObject& obj)
{
    mModals.push_back(&obj);
}

void BaseMap::EndAllModals()
{
    while (!mModals.empty())
    {
        mModals.back()->EndModal();
    }
}

void BaseMap::RemoveModal(BaseGameObject& obj)
{
    mModals.erase(std::remove(mModals.begin(), mModals.end(), &obj), mModals.end());
}
