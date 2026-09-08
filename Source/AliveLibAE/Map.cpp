#include "stdafx.h"
#include "Map.hpp"
#include "PathData.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/GameObjects/ScreenManager.hpp"
#include "../relive_lib/Sound/Midi.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "Engine.hpp"
#include "Abe.hpp"
#include "MusicController.hpp"
#include "../relive_lib/GameObjects/BackgroundMusic.hpp"
#include "stdlib.hpp"
#include "QuikSave.hpp"
#include "Sfx.hpp"
#include "CameraSwapper.hpp"
#include "../relive_lib/Events.hpp"
#include "../relive_lib/GameObjects/Particle.hpp"
#include "../relive_lib/Sound/PsxSpuApi.hpp"
#include "../relive_lib/Sys.hpp"
#include "../relive_lib/Camera.hpp"
#include "Path.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/PsxDisplay.hpp"
#include "Factory.hpp"

#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/FatalError.hpp"
#include "../relive_lib/BinaryPath.hpp"



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
        ALIVE_FATAL("Scale should be 0.5 or 1 but got %f", FP_GetDouble(scale));
    }
}


Map::Map(ResourceManagerWrapper& resMan, relive::Factory& factory)
    : BaseMap(resMan, factory)
    , mPath(*this, factory)
{
    Reset();
}



void Map::ScreenChange()
{
    if (mCamState == CamChangeStates::eInactive_0)
    {
        return;
    }

    if (gMap_bDoPurpleLightEffect && mCurrentLevel != EReliveLevelIds::eBoardRoom)
    {
        RemoveObjectsWithPurpleLight(1);
    }

    for (s32 i = 0; i < 2; i++) // Not sure why this is done twice?
    {
        for (s32 j = 0; j < gBaseGameObjects->Size(); j++)
        {
            BaseGameObject* pItem = gBaseGameObjects->ItemAt(j);
            if (!pItem)
            {
                break;
            }

            pItem->VScreenChanged();

            // Did the screen change kill the object?
            if (pItem->GetDead())
            {
                j = gBaseGameObjects->RemoveAt(j);
                relive_delete pItem;
            }
        }
    }

    // TODO: Refactor this logic
    if (!gMap_bDoPurpleLightEffect && mNextLevel == mCurrentLevel)
    {
        ScreenChange_Common();
        return;
    }

    if (LevelChanged())
    {
        SsUtAllKeyOff(0);
    }

    if (mNextLevel != EReliveLevelIds::eNone)
    {
        if (mNextLevel == EReliveLevelIds::eCredits)
        {
            mSoundChannelsMask = 0;
            ScreenChange_Common();
            return;
        }
    }
    else if (mCurrentLevel == EReliveLevelIds::eMenu)
    {
        mSoundChannelsMask = 0;
        ScreenChange_Common();
        return;
    }
    mSoundChannelsMask = SND_MIDI(0, 0, 36, 70, 0, 0);
    ScreenChange_Common();
}



s32 Map::VPurpleLightFrameCount(s16 /*bMakeInvisible*/)
{
    return 12;
}

void Map::VCollectPurpleLightObjects(DynamicArrayT<BaseAnimatedWithPhysicsGameObject>& objects, DynamicArrayT<Particle>& lights)
{
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }

        if (pObj->GetIsBaseAnimatedWithPhysicsObj())
        {
            if (pObj->GetDrawable())
            {
                auto pBaseObj = static_cast<BaseAnimatedWithPhysicsGameObject*>(pObj);

                const PSX_RECT objRect = pBaseObj->VGetBoundingRect();

                if (pBaseObj->GetDoPurpleLightEffect())
                {
                    if (pBaseObj->GetAnimation().GetRender())
                    {
                        if (!pBaseObj->GetDead() && pBaseObj != sControlledCharacter && Rect_Location_Relative_To_Active_Camera(&objRect) == CameraPos::eCamCurrent_0)
                        {
                            AddPurpleLight(pBaseObj, objects, lights);
                        }
                    }
                }
            }
        }
    }
}

void Map::Handle_PathTransition()
{
    relive::Path_PathTransition* pTlv = nullptr;
    if (mAliveObj)
    {
        pTlv = static_cast<relive::Path_PathTransition*>(mPath.VTLV_Get_At_Of_Type(
            FP_GetExponent(mAliveObj->mXPos),
            FP_GetExponent(mAliveObj->mYPos),
            FP_GetExponent(mAliveObj->mXPos),
            FP_GetExponent(mAliveObj->mYPos),
            ReliveTypes::ePathTransition).GetTlv());
    }

    if (mAliveObj && pTlv)
    {
        mNextLevel = pTlv->mNextLevel;
        mNextPath = pTlv->mNextPath;
        mNextCamera = pTlv->mNextCamera;
        mFmvBaseId = pTlv->mMovieId;

        mCameraSwapEffect = kPathChangeEffectToInternalScreenChangeEffect[pTlv->mWipeEffect];

        mAliveObj->mCurrentLevel = pTlv->mNextLevel;
        mAliveObj->mCurrentPath = pTlv->mNextPath;

        // Save value before pPathChangeTLV is deleted
        const auto next_path_scale = pTlv->mNextPathScale;

        GoTo_Camera();

        switch (next_path_scale)
        {
            case relive::reliveScale::eFull:
                gAbe->SetSpriteScale(FP_FromInteger(1));
                gAbe->GetAnimation().SetRenderLayer(Layer::eLayer_AbeMenu_32);
                break;

            case relive::reliveScale::eHalf:
                gAbe->SetSpriteScale(FP_FromDouble(0.5));
                gAbe->GetAnimation().SetRenderLayer(Layer::eLayer_AbeMenu_Half_13);
                break;

            default:
                LOG_ERROR("Invalid scale %d", static_cast<s16>(next_path_scale));
                break;
        }

        CameraPos remapped = CameraPos::eCamInvalid_m1;
        switch (mMapDirection)
        {
            case MapDirections::eMapLeft_0:
                remapped = CameraPos::eCamLeft_3;
                break;
            case MapDirections::eMapRight_1:
                remapped = CameraPos::eCamRight_4;
                break;
            case MapDirections::eMapTop_2:
                remapped = CameraPos::eCamTop_1;
                break;
            case MapDirections::eMapBottom_3:
                remapped = CameraPos::eCamBottom_2;
                break;
        }

        mAliveObj->VOnPathTransition(
            mPath.mPathData->field_A_grid_width * mCamIdxOnX,
            mPath.mPathData->field_C_grid_height * mCamIdxOnY,
            remapped);
    }
    else
    {
        switch (mMapDirection)
        {
            case MapDirections::eMapLeft_0:
                mCamIdxOnX--;
                mCameraSwapEffect = CameraSwapEffects::eRightToLeft_2;
                break;
            case MapDirections::eMapRight_1:
                mCamIdxOnX++;
                mCameraSwapEffect = CameraSwapEffects::eLeftToRight_1;
                break;
            case MapDirections::eMapTop_2:
                mCamIdxOnY--;
                mCameraSwapEffect = CameraSwapEffects::eBottomToTop_4;
                break;
            case MapDirections::eMapBottom_3:
                mCamIdxOnY++;
                mCameraSwapEffect = CameraSwapEffects::eTopToBottom_3;
                break;
            default:
                break;
        }

        const BinaryPath* pPathRes = GetPathResourceBlockPtr(mCurrentPath);
        const char* pCameraName = pPathRes->CameraName(mCamIdxOnX, mCamIdxOnY);

        mNextCamera = static_cast<s16>(pPathRes->CameraNameAsInteger(pCameraName));

        GoTo_Camera();
    }
}

Map::~Map()
{
    Shutdown();
}


void Map::GoTo_Camera()
{
    s16 bShowLoadingIcon = false;
    if (mCurrentLevel != EReliveLevelIds::eMenu && mCurrentLevel != EReliveLevelIds::eCredits && mCurrentLevel != EReliveLevelIds::eNone)
    {
        bShowLoadingIcon = true;
    }

    if (mCameraSwapEffect == CameraSwapEffects::eUnknown_11)
    {
        CamResource nullRes;
        BaseGameObject* pFmvRet = FMV_Camera_Change(nullRes, this, mCurrentLevel);
        for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
        {
            SYS_EventsPump();

            BaseGameObject* pBaseGameObj = gBaseGameObjects->ItemAt(i);
            if (!pBaseGameObj)
            {
                break;
            }

            if (pBaseGameObj->GetDead() && !pBaseGameObj->GetCantKill())
            {
                i = gBaseGameObjects->RemoveAt(i);
                relive_delete pBaseGameObj;
                if (pBaseGameObj == pFmvRet)
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
            }
        }

        if (mSoundChannelsMask)
        {
            SND_Stop_Channels_Mask(mSoundChannelsMask);
        }
        mSoundChannelsMask = SND_MIDI(0, 0, 36, 70, 0, 0);
    }

    if (mCurrentLevel != EReliveLevelIds::eMenu && mCurrentLevel != EReliveLevelIds::eNone)
    {
        if (LevelChanged()
            || mForceLoad
            || (PathChanged() && mCameraSwapEffect == CameraSwapEffects::ePlay1FMV_5))
        {
            mResourceManager.ShowLoadingIcon(*this);
        }
    }

    if (LevelChanged() || PathChanged() || mForceLoad)
    {
        mOverlayId = GetOverlayId();
    }


    if (LevelChanged() || mForceLoad)
    {
        mResourceManager.LoadingLoop(bShowLoadingIcon, this);

        // Free all cameras
        for (s32 i = 0; i < ALIVE_COUNTOF(field_2C_camera_array); i++)
        {
            if (field_2C_camera_array[i])
            {
                relive_delete field_2C_camera_array[i];
                field_2C_camera_array[i] = nullptr;
            }
        }

        if (mCurrentLevel != EReliveLevelIds::eNone)
        {
            if (LevelChanged())
            {
                SND_Reset();
                FreePathResourceBlocks();
                mPath.Free();
            }

        }

        mResourceManager.LoadingLoop(bShowLoadingIcon, this);

        if (mNextLevel == mCurrentLevel)
        {
            MusicController::static_PlayMusic(MusicController::MusicTypes::eNone_0, gAbe, 0, 0);
        }
        else
        {
            // Don't let the force flag make us reload paths for no reason
            mLoadedPaths = mResourceManager.LoadPaths(mNextLevel);

            BinaryPath* pNextPath = GetPathResourceBlockPtr(mNextPath);
            SND_Load_VABS(pNextPath->GetSoundInfo(), Path_Get_Reverb(mNextLevel), mResourceManager, *this); // TODO: Remove hard coded data
            SND_Load_Seqs(gSeqData.mSeqs, pNextPath->GetSoundInfo(), mResourceManager, *this);

            // TODO: Remove hard coded data
            relive_new BackgroundMusic(Path_Get_BackGroundMusicId(mNextLevel), mResourceManager, *this);
        }

        if (!mForceLoad)
        {
            SwitchStates_ClearRange(2, 255);
        }

        if (mFreeAllAnimAndPalts)
        {
            mFreeAllAnimAndPalts = false;
        }
    }

    if (!mNextPath)
    {
        mNextPath = 1;
    }

    const s16 prevPathId = mCurrentPath;
    const EReliveLevelIds prevLevelId = mCurrentLevel;

    mCurrentPath = mNextPath;
    mCurrentLevel = mNextLevel;
    mCurrentCamera = mNextCamera;

    const PathBlyRec* pPathRec_1 = Path_Get_Bly_Record(mNextLevel, mNextPath);

    mPath.Init(
        pPathRec_1->field_4_pPathData,
        mNextLevel,
        mNextPath,
        mNextCamera,
        GetPathResourceBlockPtr(mNextPath));

    BinaryPath* pNextPath = GetPathResourceBlockPtr(mNextPath);
    for (auto& cam : pNextPath->GetCameras())
    {
        if (pNextPath->CameraNameAsInteger(cam->mName.c_str()) == static_cast<u32>(mNextCamera))
        {
            mCamIdxOnX = static_cast<s16>(cam->mX);
            mCamIdxOnY = static_cast<s16>(cam->mY);
            break;
        }
    }

    mCameraOffset.x = FP_FromInteger(mCamIdxOnX * mPath.mPathData->field_A_grid_width);
    mCameraOffset.y = FP_FromInteger(mCamIdxOnY * mPath.mPathData->field_C_grid_height);

    // If map has changed then load new collision info
    if (prevPathId != mCurrentPath || prevLevelId != mCurrentLevel)
    {
        relive_delete gCollisions;
        gCollisions = relive_new Collisions(GetPathResourceBlockPtr(mCurrentPath)->GetCollisions());
    }

    if (mRestoreMapObjectStates)
    {
        QuikSave::RestoreBlyData(QuikSave::gActiveQuicksaveData, mResourceManager, *this);
        mRestoreMapObjectStates = false;
    }

    // Copy camera array and blank out the source
    for (s32 i = 0; i < ALIVE_COUNTOF(field_40_stru_5); i++)
    {
        field_40_stru_5[i] = field_2C_camera_array[i];
        field_2C_camera_array[i] = nullptr;
    }

    field_2C_camera_array[0] = Create_Camera(mCamIdxOnX, mCamIdxOnY, 1);
    field_2C_camera_array[3] = Create_Camera(mCamIdxOnX - 1, mCamIdxOnY, 0);
    field_2C_camera_array[4] = Create_Camera(mCamIdxOnX + 1, mCamIdxOnY, 0);
    field_2C_camera_array[1] = Create_Camera(mCamIdxOnX, mCamIdxOnY - 1, 0);
    field_2C_camera_array[2] = Create_Camera(mCamIdxOnX, mCamIdxOnY + 1, 0);

    // Free resources for each camera
    for (s32 i = 0; i < ALIVE_COUNTOF(field_40_stru_5); i++)
    {
        if (field_40_stru_5[i])
        {
            //pResourceManager->Free_Resources_For_Camera_4656F0(field_40_stru_5[i]);
        }
    }

    mResourceManager.LoadingLoop(bShowLoadingIcon, this);

    // Free each camera itself
    for (s32 i = 0; i < ALIVE_COUNTOF(field_40_stru_5); i++)
    {
        if (field_40_stru_5[i])
        {
            relive_delete field_40_stru_5[i];
            field_40_stru_5[i] = nullptr;
        }
    }

    Load_Path_Items(field_2C_camera_array[0], relive::Factory::LoadMode::ConstructObject_0);
    mResourceManager.LoadingLoop(bShowLoadingIcon, this);
    Load_Path_Items(field_2C_camera_array[3], relive::Factory::LoadMode::ConstructObject_0);
    Load_Path_Items(field_2C_camera_array[4], relive::Factory::LoadMode::ConstructObject_0);
    Load_Path_Items(field_2C_camera_array[1], relive::Factory::LoadMode::ConstructObject_0);
    Load_Path_Items(field_2C_camera_array[2], relive::Factory::LoadMode::ConstructObject_0);

    // Create the screen manager if it hasn't already been done (probably should have always been done by this point though?)
    if (!gScreenManager)
    {
        gScreenManager = relive_new ScreenManager(field_2C_camera_array[0]->mCamRes, &mCameraOffset, GetResourceManager(), *this);
    }

    mPath.Loader_4DB800(mCamIdxOnX, mCamIdxOnY, relive::Factory::LoadMode::ConstructObject_0, ReliveTypes::eNone, mResourceManager, *this); // none = load all

    if (prevPathId != mCurrentPath || prevLevelId != mCurrentLevel)
    {
        if (gAbe)
        {
            if (mCurrentPath == gAbe->mCurrentPath)
            {
                gAbe->VCheckCollisionLineStillValid(10);
            }
        }
    }

    Create_FG1s();

    if (mCameraSwapEffect == CameraSwapEffects::ePlay1FMV_5)
    {
        Map::FMV_Camera_Change(field_2C_camera_array[0]->mCamRes, this, mNextLevel);
    }

    if (mCameraSwapEffect == CameraSwapEffects::eUnknown_11)
    {
        gScreenManager->DecompressCameraToVRam(field_2C_camera_array[0]->mCamRes);
        gScreenManager->EnableRendering();
    }

    if (prevLevelId != mCurrentLevel)
    {
        mResourceManager.LoadingLoop(false);
    }

    if (mCameraSwapEffect != CameraSwapEffects::ePlay1FMV_5 && mCameraSwapEffect != CameraSwapEffects::eUnknown_11)
    {
        if (mDoorTransition)
        {
            // TODO: Add template helpers

            // Door transition
            TlvIterator doorIterator = mPath.TLV_First_Of_Type_In_Camera(ReliveTypes::eDoor, 0);
            while (doorIterator.GetTlv())
            {
                auto pDoorTlv = static_cast<relive::Path_Door*>(doorIterator.GetTlv());
                if (pDoorTlv->mDoorId == gAbe->field_1A0_door_id)
                {
                    CreateScreenTransistionForTLV(doorIterator.GetTlv());
                    break;                    
                }
                doorIterator = Path::TLV_Next_Of_Type(doorIterator, ReliveTypes::eDoor);
            }
        }
        else
        {
            if (!mTeleporterTransition)
            {
                relive_new CameraSwapper(field_2C_camera_array[0]->mCamRes, mResourceManager, *this, mCameraSwapEffect, 368 / 2, 240 / 2);
            }
            else
            {
                // TODO: Add template helpers

                // Teleporter transition
                TlvIterator teleporterIterator = mPath.TLV_First_Of_Type_In_Camera(ReliveTypes::eTeleporter, 0);
                while (teleporterIterator.GetTlv())
                {
                    auto pTeleporterTlv = static_cast<relive::Path_Teleporter*>(teleporterIterator.GetTlv());
                    if (pTeleporterTlv->mTeleporterId == gAbe->field_1A0_door_id)
                    {
                        CreateScreenTransistionForTLV(pTeleporterTlv);
                        break;
                    }
                    teleporterIterator = Path::TLV_Next_Of_Type(teleporterIterator, ReliveTypes::eTeleporter);
                }
            }
        }
    }

    mResourceManager.bHideLoadingIcon = 0;
    mResourceManager.loading_ticks = 0;

    mForceLoad = 0;

    if (mSoundChannelsMask)
    {
        SND_Stop_Channels_Mask(mSoundChannelsMask);
        mSoundChannelsMask = 0;
    }
}






void Map::CreateScreenTransistionForTLV(relive::Path_TLV* pTlv)
{
    // TODO: Refactor
    const s16 doorYDiff = static_cast<s16>(pTlv->mTopLeftY - FP_GetExponent(gScreenManager->CamYPos()));
    const s16 midX = (pTlv->mTopLeftX + pTlv->mBottomRightX) / 2;
    const s16 rightPos = static_cast<s16>(midX - FP_GetExponent(gScreenManager->CamXPos()));
    relive_new CameraSwapper(field_2C_camera_array[0]->mCamRes, mResourceManager, *this, mCameraSwapEffect, rightPos, doorYDiff);
}

void Map::Get_map_size(PSX_Point* pPoint)
{
    pPoint->x = mPath.mPathData->field_4_bTop;
    pPoint->y = mPath.mPathData->field_6_bBottom;
}

void Map::GetCurrentCamCoords(PSX_Point* pPoint)
{
    pPoint->x = mCamIdxOnX * mPath.mPathData->field_A_grid_width;
    pPoint->y = mCamIdxOnY * mPath.mPathData->field_C_grid_height;
}

s16 Map::GetOverlayId()
{
    // TODO: Probably need to redo field_C data as 1 bytes instead of a word
    return Path_Get_Bly_Record(mNextLevel, mNextPath)->mOverlayId & 0xFF;
}


s16 Map::Get_Camera_World_Rect(CameraPos camIdx, PSX_RECT* pRect)
{
    if (camIdx < CameraPos::eCamCurrent_0 || camIdx > CameraPos::eCamRight_4)
    {
        return 0;
    }

    Camera* pCamera = field_2C_camera_array[static_cast<s32>(camIdx)];
    if (!pCamera)
    {
        return 0;
    }

    if (!pRect)
    {
        return 1;
    }

    const s16 xpos = pCamera->mCamXOff * mPath.mPathData->field_A_grid_width;
    const s16 ypos = pCamera->mCamYOff * mPath.mPathData->field_C_grid_height;

    pRect->x = xpos;
    pRect->y = ypos;
    pRect->w = xpos + 368;
    pRect->h = ypos + 240;
    return 1;
}

s16 Map::Is_Point_In_Current_Camera(EReliveLevelIds level, s32 path, FP xpos, FP ypos, s16 width)
{
    const FP calculated_width = (width != 0) ? FP_FromInteger(6) : FP_FromInteger(0);
    if (level != mCurrentLevel || path != mCurrentPath) // TODO: Remove when 100%
    {
        return false;
    }

    PSX_RECT rect = {};
    rect.x = FP_GetExponent(xpos - calculated_width);
    rect.w = FP_GetExponent(calculated_width + xpos);
    rect.y = FP_GetExponent(ypos);
    rect.h = FP_GetExponent(ypos);
    return Rect_Location_Relative_To_Active_Camera(&rect) == CameraPos::eCamCurrent_0;
}


CameraPos Map::Rect_Location_Relative_To_Active_Camera(const PSX_RECT* pRect, s16 /*width*/)
{
    if (EventGet(Event::kEventDeathReset))
    {
        return CameraPos::eCamNone_5;
    }

    const s32 camX = FP_GetExponent(mCameraOffset.x);
    const s32 camY = FP_GetExponent(mCameraOffset.y);

    if (pRect->x > (camX + 368))
    {
        return CameraPos::eCamRight_4;
    }

    if (pRect->y > (camY + 240))
    {
        return CameraPos::eCamBottom_2;
    }

    if (pRect->w >= camX)
    {
        if (pRect->h < camY)
        {
            return CameraPos::eCamTop_1;
        }
        else
        {
            return CameraPos::eCamCurrent_0;
        }
    }

    return CameraPos::eCamLeft_3;
}


BaseGameObject* Map::FMV_Camera_Change(CamResource& ppBits, Map* pMap, EReliveLevelIds lvlId)
{
    if (pMap->mFmvBaseId > 10000u)
    {
        // Trippe FMV
        FmvInfo* pFmvRec1 = Path_Get_FMV_Record(lvlId, pMap->mFmvBaseId / 10000);
        FmvInfo* pFmvRec2 = Path_Get_FMV_Record(lvlId, pMap->mFmvBaseId % 100);
        FmvInfo* pFmvRec3 = Path_Get_FMV_Record(lvlId, pMap->mFmvBaseId / 100 % 100);

        return relive_new CameraSwapper(ppBits, pMap->mResourceManager, *pMap,
                                        pFmvRec1->field_8_flags == 1,
                                        pFmvRec1->field_0_pName,
                                        pFmvRec2->field_8_flags == 1, // TODO: Master branch gobbed
                                        pFmvRec2->field_0_pName,
                                        pFmvRec3->field_8_flags == 1,
                                        pFmvRec3->field_0_pName);
    }
    else if (pMap->mFmvBaseId >= 100u)
    {
        // Double FMV
        FmvInfo* pFmvRec1 = Path_Get_FMV_Record(lvlId, pMap->mFmvBaseId / 100);
        FmvInfo* pFmvRec2 = Path_Get_FMV_Record(lvlId, pMap->mFmvBaseId % 100);
        return relive_new CameraSwapper(ppBits, pMap->mResourceManager, *pMap,
                                              pFmvRec1->field_8_flags == 1,
                                              pFmvRec1->field_0_pName,
                                              pFmvRec2->field_8_flags == 1,
                                              pFmvRec2->field_0_pName);
    }
    else // < 100
    {
        // Single FMV
        FmvInfo* pFmvRec1 = Path_Get_FMV_Record(lvlId, pMap->mFmvBaseId);
        return relive_new CameraSwapper(ppBits, pMap->mResourceManager, *pMap,
                                              pFmvRec1->field_8_flags == 1,
                                              pFmvRec1->field_0_pName);
    }
}

Camera* Map::Create_Camera(s16 xpos, s16 ypos, s32 /*a4*/)
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
    for (s32 i = 0; i < ALIVE_COUNTOF(field_40_stru_5); i++)
    {
        if (field_40_stru_5[i]
            && field_40_stru_5[i]->mLevel == mCurrentLevel
            && field_40_stru_5[i]->mPath == mCurrentPath
            && field_40_stru_5[i]->mCamXOff == xpos
            && field_40_stru_5[i]->mCamYOff == ypos)
        {
            Camera* pTemp = field_40_stru_5[i];
            field_40_stru_5[i] = nullptr;
            return pTemp;
        }
    }

    // Get a pointer to the camera name from the Path resource
    const BinaryPath* pPathData = GetPathResourceBlockPtr(mCurrentPath);
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

void Map::Load_Path_Items(Camera* pCamera, relive::Factory::LoadMode loadMode)
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
            // Async camera load
            pCamera->mCamRes = mResourceManager.LoadCam(pCamera->mLevel, pCamera->mPath, pCamera->mCameraNumber);

            mPath.Loader_4DB800(pCamera->mCamXOff, pCamera->mCamYOff, relive::Factory::LoadMode::LoadResourceFromList_1, ReliveTypes::eNone, mResourceManager, *this); // none = load all
        }
        else
        {
            // Blocking camera load
            // ResourceManager::LoadResourceFile_49C170(pCamera->mCamName, pCamera);
            pCamera->mCamResLoaded = true;
            // pCamera->mCamRes = mResourceManager.LoadCam(pCamera->mLevel, pCamera->mPath, pCamera->mCamera);

            mPath.Loader_4DB800(pCamera->mCamXOff, pCamera->mCamYOff, relive::Factory::LoadMode::LoadResource_2, ReliveTypes::eNone, mResourceManager, *this); // none = load all
        }

    }
}

TlvIterator Map::TLV_From_Offset_Lvl_Cam(const Guid& tlvId)
{
    return mPath.TLV_From_Offset_Lvl_Cam(tlvId);
}


void Map::Reset_TLVs(u16 pathId)
{
    mPath.Reset_TLVs(pathId);
}
