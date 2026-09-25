#pragma once

#include "MapWrapper.hpp"
#include "BinaryPath.hpp"
#include "FixedPoint.hpp"
#include "BasePath.hpp"
#include "DynamicArray.hpp"
#include "QuikSaveTypes.hpp"
#include "Factory.hpp"

class Guid;
struct PSX_RECT;
class Camera;
class ResourceManagerWrapper;
class Particle;
class BaseAnimatedWithPhysicsGameObject;
class BaseGameObject;

enum class ReliveTypes : s16;

// What the screen change steps return, see BaseMap::ScreenChange
enum class ScreenChangeResult
{
    eDone,
    // A modal (purple lights, the endings' FMVs) or loading has to finish first: call it again
    // after
    eWaiting,
};

// What BaseMap::RemoveObjectsWithPurpleLight returns
enum class PurpleLightResult
{
    eNoLights,
    eShowing,
};

// Up to 3 FMVs to play back to back during a camera swap. Replaces the old scheme of
// packing up to 3 small FMV indices into a single decimal number (e.g. 12402 meant
// play FMV 1, then FMV 24, then FMV 2 - decoded via /10000, %100 and /100%100) that
// FMV_Camera_Change used to unpack. An empty name means "no FMV in that slot".
struct FmvIds final
{
    FmvIds() = default;
    explicit FmvIds(std::string fmv1, std::string fmv2 = {}, std::string fmv3 = {})
        : mFmv1(std::move(fmv1))
        , mFmv2(std::move(fmv2))
        , mFmv3(std::move(fmv3))
    {

    }

    std::string mFmv1;
    std::string mFmv2;
    std::string mFmv3;

    bool IsNone() const
    {
        return mFmv1.empty() && mFmv2.empty() && mFmv3.empty();
    }
};

// Largest valid grid-block index for a sprite of the given scale (0.5 or 1.0).
// Same for both engines. An invalid scale logs a warning and returns 0 rather
// than asserting - AO can reach this path in practice (e.g. dying with DDCheat on).
s32 MaxGridBlocks(FP scale);

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
    // Changes to the next camera. It waits for what the new camera's objects need to load
    // before making them, and the game endings' FMV change (eUnknown_11) plays its FMVs first:
    // calling it again once that's done carries on (see mScreenChangeResume).
    virtual ScreenChangeResult GoTo_Camera() = 0;
    // Runs a pending camera change at the end of a frame. While it's waiting on a modal
    // (purple lights, the endings' FMVs) or on loading, Engine::RunFrame calls it again once
    // that's finished and it carries on from where it stopped.
    virtual ScreenChangeResult ScreenChange() = 0;
    virtual ScreenChangeResult Handle_PathTransition() = 0;

    // Restarts the music and ambient sounds once a camera swap has finished, after any purple
    // lights it ends with.
    virtual void VCameraSwapFinished() = 0;

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
    // The editor changed a path's JSON: reloads it if it's loaded. The Engine calls this
    // between frames, and the camera change it needs runs at the start of the next one.
    void ReloadPathJsonRequest(const std::string& pathJsonFileName);

    // --- Camera/level transitions ---

    s16 SetActiveCameraDelayed(MapDirections direction, BaseAliveGameObject* pObj, s16 swapEffect);
    Camera* GetCamera(CameraPos pos);
    Camera* Create_Camera(s16 xpos, s16 ypos, s32 a4);
    void Load_Path_Items(Camera* pCamera, relive::Factory::LoadMode loadMode);
    // Takes the camera image Load_Path_Items pended, waiting for it if it's still loading
    void Finish_Load_Cam(Camera* pCamera);
    // Frees what Load_Path_Items loaded for the camera
    void Free_Resources_For_Camera(Camera* pCamera);
    void GetCurrentCamCoords(PSX_Point* pPoint);
    bool SetActiveCam(EReliveLevelIds level, s16 path, s16 cam, CameraSwapEffects screenChangeEffect, FmvIds fmvIds = {}, bool forceChange = false);
    CameraPos GetDirection(EReliveLevelIds level, s32 path, FP xpos, FP ypos);
    void Get_map_size(PSX_Point* pPoint);
    // The first camera is changed to at the start of the first frame, see
    // ContinueDirectCameraChange
    void Init(EReliveLevelIds level, s16 path, s16 camera, CameraSwapEffects screenChangeEffect, FmvIds fmvIds = {}, bool forceChange = false);

    // Init and path reloads change camera outside of a screen change: the Engine calls this at
    // the start of each frame while one is pending, until it's done.
    bool DirectCameraChangePending() const
    {
        return mDirectCameraChangePending;
    }
    ScreenChangeResult ContinueDirectCameraChange();
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

    // Plays the purple light effect for VPurpleLightFrameCount() frames as a modal (see
    // GetActiveModal). When bMakeInvisible is 0 (the end of a camera swap),
    // VCameraSwapFinished() runs once it's done.
    PurpleLightResult RemoveObjectsWithPurpleLight(s16 bMakeInvisible);
    void Start_Sounds_For_Objects_In_Near_Cameras();

    // --- Modal objects ---

    // A modal object takes over the main loop the way the original game's nested loops did
    // (pause menu, movies, purple lights...): while one is active, Engine::Game_Loop runs only
    // the newest one's VModalUpdate() each iteration and the rest of the game is frozen behind
    // it. See BaseGameObject::StartModal.
    BaseGameObject* GetActiveModal() const
    {
        return mModals.empty() ? nullptr : mModals.back();
    }
    void AddModal(BaseGameObject& obj);
    void RemoveModal(BaseGameObject& obj);
    // For quitting: ends every modal without finishing what it was doing
    void EndAllModals();

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
    FmvIds mFmvIds;

    // Kept in sync with mFmvIds by SetActiveCam - 1 whenever an FMV is queued to play, 0
    // otherwise. Exists only for Source/relive/Exe.cpp's AutoSplitterData ABI, which external
    // tools (speedrun auto-splitters) read via a fixed memory layout and can't be given a
    // string-based field without breaking that ABI.
    u16 mFmvPending = 0;
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
    ScreenChangeResult ScreenChange_Common();

    // The game endings' FMV camera change (eUnknown_11) runs one update pass over every object
    // before the new camera loads, the FMVs playing as the pass reaches them. Runs that pass as
    // a modal, which finishes once the pass has, or once pFmvSwapper has been deleted.
    // bKeepCantKill: AE keeps dead objects that can't be killed yet, AO deletes them.
    void RunFmvCameraChangePass(BaseGameObject* pFmvSwapper, bool bKeepCantKill);

    // Where a screen change that's waiting on a modal carries on from when it's called again
    enum class ScreenChangeResume : s16
    {
        eNone,
        eAfterPurpleLight,
        eAfterFmvPass,
        eAfterPathsLoad,
        eAfterSoundsLoad,
        eAfterCameraLoad,
    };
    ScreenChangeResume mScreenChangeResume = ScreenChangeResume::eNone;

    ResourceManagerWrapper& mResourceManager;
    relive::Factory& mFactory;
    u32 mSoundChannelsMask = 0;

private:
    std::vector<BaseGameObject*> mModals;
    bool mDirectCameraChangePending = false;
};
