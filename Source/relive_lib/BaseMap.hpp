#pragma once

#include "MapWrapper.hpp"
#include "BinaryPath.hpp"
#include "FixedPoint.hpp"
#include "BasePath.hpp"
#include "DynamicArray.hpp"
#include "QuikSaveTypes.hpp"

class Guid;
struct PSX_RECT;
class Camera;
class ResourceManagerWrapper;
class Particle;
class BaseAnimatedWithPhysicsGameObject;

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
    // --- Nested types ---

    enum class CamChangeStates : s16
    {
        eInactive_0 = 0,
        eSliceCam_1 = 1,
        eInstantChange_2 = 2
    };

    // The world transition Abe is currently walking through, looked up by
    // GoTo_Camera() to build the screen change effect. Teleporters don't
    // exist in AO, so eTeleporter_2 is only ever set by the AE Teleporter object.
    enum class PendingTransition : s16
    {
        eNone_0 = 0,
        eDoor_1 = 1,
        eTeleporter_2 = 2,
    };

    // --- Construction ---

    explicit BaseMap(ResourceManagerWrapper& resMan, relive::Factory& factory)
        : mResourceManager(resMan)
        , mFactory(factory)
    {

    }

    virtual ~BaseMap()
    {

    }

    // --- Simple queries ---

    ResourceManagerWrapper& GetResourceManager()
    {
        return mResourceManager;
    }

    std::vector<std::unique_ptr<BinaryPath>>& GetLoadedPaths()
    {
        return mLoadedPaths;
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

    // --- Engine specific virtual interface ---

    virtual s16 GetOverlayId() = 0;

    // The engine specific Path object this map walks.
    virtual BasePath& GetPath() = 0;

    virtual CameraPos Rect_Location_Relative_To_Active_Camera(const PSX_RECT* pRect, s16 width = 0) = 0;
    virtual s16 Get_Camera_World_Rect(CameraPos camIdx, PSX_RECT* pRect) = 0;
    virtual s16 Is_Point_In_Current_Camera(EReliveLevelIds level, s32 path, FP xpos, FP ypos, s16 width) = 0;
    virtual void GetCurrentCamCoords(PSX_Point* pPoint) = 0;
    virtual void GoTo_Camera() = 0;
    virtual void ScreenChange() = 0;
    virtual void Handle_PathTransition() = 0;

    // Which on screen objects get a purple light, and the light particles spawned
    // for them. The two games scan different object lists and cull differently.
    virtual void VCollectPurpleLightObjects(DynamicArrayT<BaseAnimatedWithPhysicsGameObject>& objects, DynamicArrayT<Particle>& lights) = 0;

    // How many frames the purple light effect is rendered for.
    virtual s32 VPurpleLightFrameCount(s16 bMakeInvisible) = 0;

    // Some engines carry additional legacy pending-restore state of their own
    // (AO's SaveGame buffer pointer) that needs clearing alongside
    // mPendingSaveRestore below. Default is a no-op.
    virtual void VClearPendingSaveRestore()
    {
    }

    // --- Path/resource management ---

    BinaryPath* GetPathResourceBlockPtr(u32 pathId);
    void FreePathResourceBlocks();
    void ClearPathResourceBlocks();
    void ReloadPathJsonRequest(const std::string& pathJsonFileName);

    // --- Camera/level transitions ---

    s16 SetActiveCameraDelayed(MapDirections direction, BaseAliveGameObject* pObj, s16 swapEffect);
    Camera* GetCamera(CameraPos pos);
    s16 SetActiveCam(EReliveLevelIds level, s16 path, s16 cam, CameraSwapEffects screenChangeEffect, s16 fmvBaseId, s16 forceChange);
    CameraPos GetDirection(EReliveLevelIds level, s32 path, FP xpos, FP ypos);
    void Get_map_size(PSX_Point* pPoint);
    void Init(EReliveLevelIds level, s16 path, s16 camera, CameraSwapEffects screenChangeEffect, s16 fmvBaseId, s16 forceChange);
    void Shutdown();
    void Reset();

    // --- TLVs ---

    void TLV_Reset(const Guid& tlvId, s16 hiFlags = -1);
    void TLV_Persist(const Guid& tlvId, s16 hiFlags = -1);
    void TLV_Delete(const Guid& tlvId, s16 hiFlags = -1);
    void Set_TLVData(const Guid& tlvId, s16 hiFlags, s8 bSetCreated, s8 bSetDestroyed);
    TlvIterator VTLV_Get_At_Of_Type(s16 xpos, s16 ypos, s16 width, s16 height, ReliveTypes typeToFind);
    TlvIterator TLV_First_Of_Type_In_Camera(ReliveTypes objectType, s16 camX);
    TlvIterator TLV_Get_At(TlvIterator pTlv, FP xpos, FP ypos, FP width, FP height);
    TlvIterator TLV_From_Offset_Lvl_Cam(const Guid& tlvId);
    void Reset_TLVs(u16 pathId);
    TlvIterator Get_First_TLV_For_Offsetted_Camera(s16 cam_x_idx, s16 cam_y_idx);

    // --- Purple light / ambient sound ---

    void RemoveObjectsWithPurpleLight(s16 bMakeInvisible);
    void Start_Sounds_For_Objects_In_Near_Cameras();

    // --- Quicksave ---

    // Writes/reads the bly-flag bytes for every currently loaded TLV whose
    // QuiksaveAttribute says it participates in a quicksave. Shared by AO
    // and AE's QuikSave, since that attribute lives on the one shared TLV
    // struct definitions both engines use.
    void SaveQuicksaveBlyData(SerializedObjectData& pSaveBuffer);
    void RestoreQuicksaveBlyData(SerializedObjectData& pSaveData);

    // --- Data members ---

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

    CameraSwapEffects mCameraSwapEffect = CameraSwapEffects::eInstantChange_0;
    u16 mFmvBaseId = 0;
    MapDirections mMapDirection = MapDirections::eMapLeft_0;
    BaseAliveGameObject* mAliveObj = nullptr;
    CamChangeStates mCamState = CamChangeStates::eInactive_0;

    PendingTransition mPendingTransition = PendingTransition::eNone_0;

    // A quicksave restore pending until the next GoTo_Camera consumes it via
    // GetPath()'s TLV bly-flag data. Shared by both engines' QuikSave, since
    // which TLVs participate is driven by relive::QuiksaveAttribute, common
    // to both. Not to be confused with AO's separate legacy SaveGame restore
    // (see VClearPendingSaveRestore).
    PendingObjectRestoreData* mPendingSaveRestore = nullptr;

    Camera* mCurrentCameras[5] = {};
    Camera* mPreviousCameras[5] = {};

    std::vector<std::unique_ptr<BinaryPath>> mLoadedPaths;
    FP_Point mCameraOffset = {};

protected:
    // Records an accepted object and spawns its light particle.
    void AddPurpleLight(BaseAnimatedWithPhysicsGameObject* pObj, DynamicArrayT<BaseAnimatedWithPhysicsGameObject>& objects, DynamicArrayT<Particle>& lights);

    void Create_FG1s();
    void ScreenChange_Common();

    ResourceManagerWrapper& mResourceManager;
    relive::Factory& mFactory;
    u32 mSoundChannelsMask = 0;
};
