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

s16 BaseMap::SetActiveCam(EReliveLevelIds level, s16 path, s16 cam, CameraSwapEffects screenChangeEffect, s16 fmvBaseId, s16 forceChange)
{
    if (!forceChange && cam == mCurrentCamera && level == mCurrentLevel && path == mCurrentPath)
    {
        return 0;
    }

    mNextCamera = cam;
    mFmvBaseId = fmvBaseId;
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

    return 1;
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
            DestroyObjects(mResourceManager);
            GoTo_Camera();
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

void BaseMap::ScreenChange_Common()
{
    if (mCamState == CamChangeStates::eSliceCam_1)
    {
        Handle_PathTransition();
    }
    else if (mCamState == CamChangeStates::eInstantChange_2)
    {
        GoTo_Camera();
    }

    mCamState = CamChangeStates::eInactive_0;

    SND_Stop_Channels_Mask(mSoundChannelsMask);
    mSoundChannelsMask = 0;
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

void BaseMap::Init(EReliveLevelIds level, s16 path, s16 camera, CameraSwapEffects screenChangeEffect, s16 fmvBaseId, s16 forceChange)
{
    for (s32 i = 0; i < ALIVE_COUNTOF(mCurrentCameras); i++)
    {
        mCurrentCameras[i] = nullptr;
    }

    mOverlayId = -1;

    mCurrentCamera = -1;
    mCurrentPath = -1;
    mCurrentLevel = EReliveLevelIds::eNone;

    SetActiveCam(level, path, camera, screenChangeEffect, fmvBaseId, forceChange);
    GoTo_Camera();

    mCamState = CamChangeStates::eInactive_0;
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

void BaseMap::RemoveObjectsWithPurpleLight(s16 bMakeInvisible)
{
    auto pObjectsWithLightsArray = relive_new DynamicArrayT<BaseAnimatedWithPhysicsGameObject>(16);

    auto pPurpleLightArray = relive_new DynamicArrayT<Particle>(16);

    VCollectPurpleLightObjects(*pObjectsWithLightsArray, *pPurpleLightArray);

    if (!pPurpleLightArray->IsEmpty())
    {
        SFX_Play_Pitch(relive::SoundEffects::PossessEffect, 40, 2400);

        const s32 kTotal = VPurpleLightFrameCount(bMakeInvisible);
        for (s32 counter = 0; counter < kTotal; counter++)
        {
            if (bMakeInvisible && counter == 4)
            {
                // Make all the objects that have lights invisible now that the lights have been rendered for a few frames
                for (s32 i = 0; i < pObjectsWithLightsArray->Size(); i++)
                {
                    BaseAnimatedWithPhysicsGameObject* pObj = pObjectsWithLightsArray->ItemAt(i);
                    if (!pObj)
                    {
                        break;
                    }
                    pObj->GetAnimation().SetRender(false);
                }
            }

            for (s32 i = 0; i < pPurpleLightArray->Size(); i++)
            {
                Particle* pLight = pPurpleLightArray->ItemAt(i);
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
            for (s32 i = 0; i < pPurpleLightArray->Size(); i++)
            {
                Particle* pLight = pPurpleLightArray->ItemAt(i);
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
            SYS_EventsPump();
            gPsxDisplay.RenderOrderingTable();
        }

        if (bMakeInvisible)
        {
            // Make all the objects that had lights visible again
            for (s32 i = 0; i < pObjectsWithLightsArray->Size(); i++)
            {
                BaseAnimatedWithPhysicsGameObject* pObj = pObjectsWithLightsArray->ItemAt(i);
                if (!pObj)
                {
                    break;
                }
                pObj->GetAnimation().SetRender(true);
            }
        }
    }

    pObjectsWithLightsArray->mUsedSize = 0;
    pPurpleLightArray->mUsedSize = 0;

    relive_delete pObjectsWithLightsArray;
    relive_delete pPurpleLightArray;
}
