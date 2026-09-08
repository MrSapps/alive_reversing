#pragma once

#include "MapWrapper.hpp"
#include "BinaryPath.hpp"
#include "FixedPoint.hpp"
#include "BasePath.hpp"

class Guid;
struct PSX_RECT;
class Camera;
class ResourceManagerWrapper;

enum class ReliveTypes : s16;

enum class MapDirections : s16
{
    eMapLeft_0 = 0,
    eMapRight_1 = 1,
    eMapTop_2 = 2,
    eMapBottom_3 = 3,
};

enum class CameraSwapEffects : s16
{
    eInstantChange_0 = 0,
    eLeftToRight_1 = 1,     // Left to right
    eRightToLeft_2 = 2,     // Right to left
    eTopToBottom_3 = 3,     // Top to bottom
    eBottomToTop_4 = 4,     // Bottom to top
    ePlay1FMV_5 = 5,        // Play single fmv
    eVerticalSplit_6 = 6,   // Screen splits from the middle and moves out up/down
    eHorizontalSplit_7 = 7, // Screen splits from the middle and moves out left/right
    eBoxOut_8 = 8,          // A rect "grows" out from the centre of the screen
    ePlay2FMVs_9 = 9,       // Play 2 fmvs
    ePlay3FMVs_10 = 10,     // Play 3 fmvs - apparently just taking an array of fmvs is too simple ?
    eUnknown_11 = 11        // Unknown, has special handing in the map object
};

namespace relive
{
    class Factory;
}

class BaseMap
{
public:
    explicit BaseMap(ResourceManagerWrapper& resMan, relive::Factory& factory)
        : mResourceManager(resMan)
        , mFactory(factory)
    {

    }

    ResourceManagerWrapper& GetResourceManager()
    {
        return mResourceManager;
    }

    std::vector<std::unique_ptr<BinaryPath>>& GetLoadedPaths()
    {
        return mLoadedPaths;
    }

    BinaryPath* GetPathResourceBlockPtr(u32 pathId);
    void FreePathResourceBlocks();
    void ClearPathResourceBlocks();

    enum class CamChangeStates : s16
    {
        eInactive_0 = 0,
        eSliceCam_1 = 1,
        eInstantChange_2 = 2
    };

    virtual ~BaseMap()
    {

    }

    bool LevelChanged() const
    {
        return mCurrentLevel != mNextLevel;
    }

    bool PathChanged() const
    {
        return mCurrentPath != mNextPath;
    }

    bool CameraChanged() const
    {
        return mCurrentCamera != mNextCamera;
    }

    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
    s16 mCurrentPath = 0;
    s16 mCurrentCamera = 0;

    EReliveLevelIds mNextLevel = EReliveLevelIds::eNone;
    s16 mNextPath = 0;
    s16 mNextCamera = 0;

    s16 mForceLoad = 0;

    s16 mCamIdxOnX = 0;
    s16 mCamIdxOnY = 0;

    u16 mCamsOnX = 0;
    u16 mCamsOnY = 0;

    bool mFreeAllAnimAndPalts = false;

    s16 mOverlayId = 0;

    virtual s16 GetOverlayId() = 0;

    // The engine specific Path object this map walks.
    virtual BasePath& GetPath() = 0;

    void TLV_Reset(const Guid& tlvId, s16 hiFlags = -1);
    void TLV_Persist(const Guid& tlvId, s16 hiFlags = -1);
    void TLV_Delete(const Guid& tlvId, s16 hiFlags = -1);
    void Set_TLVData(const Guid& tlvId, s16 hiFlags, s8 bSetCreated, s8 bSetDestroyed);

    virtual CameraPos Rect_Location_Relative_To_Active_Camera(const PSX_RECT* pRect, s16 width = 0) = 0;
    virtual s16 Get_Camera_World_Rect(CameraPos camIdx, PSX_RECT* pRect) = 0;

    s16 SetActiveCameraDelayed(MapDirections direction, BaseAliveGameObject* pObj, s16 swapEffect);

    Camera* GetCamera(CameraPos pos);

    s16 SetActiveCam(EReliveLevelIds level, s16 path, s16 cam, CameraSwapEffects screenChangeEffect, s16 fmvBaseId, s16 forceChange);


    virtual s16 Is_Point_In_Current_Camera(EReliveLevelIds level, s32 path, FP xpos, FP ypos, s16 width) = 0;
    CameraPos GetDirection(EReliveLevelIds level, s32 path, FP xpos, FP ypos);
    virtual void GetCurrentCamCoords(PSX_Point* pPoint) = 0;
    virtual void GoTo_Camera() = 0;

    virtual void ScreenChange() = 0;
    virtual void Handle_PathTransition() = 0;
    void Init(EReliveLevelIds level, s16 path, s16 camera, CameraSwapEffects screenChangeEffect, s16 fmvBaseId, s16 forceChange);
    void Shutdown();
    void Reset();

    // A save restore is pending until the next GoTo_Camera consumes it. Each
    // game tracks that request differently, so clearing it is engine specific.
    virtual void VClearPendingSaveRestore() = 0;
    
    TlvIterator VTLV_Get_At_Of_Type(s16 xpos, s16 ypos, s16 width, s16 height, ReliveTypes typeToFind);
    TlvIterator TLV_First_Of_Type_In_Camera(ReliveTypes objectType, s16 camX);
    TlvIterator TLV_Get_At(TlvIterator pTlv, FP xpos, FP ypos, FP width, FP height);
    virtual TlvIterator TLV_From_Offset_Lvl_Cam(const Guid& tlvId);
    TlvIterator Get_First_TLV_For_Offsetted_Camera(s16 cam_x_idx, s16 cam_y_idx);

    void ReloadPathJsonRequest(const std::string& pathJsonFileName);

public:
    CameraSwapEffects mCameraSwapEffect = CameraSwapEffects::eInstantChange_0;
    u16 mFmvBaseId = 0;
    MapDirections mMapDirection = MapDirections::eMapLeft_0;
    BaseAliveGameObject* mAliveObj = nullptr;
    CamChangeStates mCamState = CamChangeStates::eInactive_0;

    Camera* field_2C_camera_array[5] = {};
    Camera* field_40_stru_5[5] = {};

    std::vector<std::unique_ptr<BinaryPath>> mLoadedPaths;
    FP_Point mCameraOffset = {};

protected:
    void Create_FG1s();
    void ScreenChange_Common();

    ResourceManagerWrapper& mResourceManager;
    relive::Factory& mFactory;
    u32 mSoundChannelsMask = 0;
};
