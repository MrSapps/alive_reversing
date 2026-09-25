#pragma once

#include "../relive_lib/MapWrapper.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/BaseMap.hpp"
#include "../relive_lib/Factory.hpp"
#include "Path.hpp"

enum class ReliveTypes : s16;
class CamResource;
struct PSX_Point;
class BinaryPath;

namespace relive
{
    class Path_TLV;
    enum class LoadMode : s16;
}

class Camera;
class ResourceManagerWrapper;

extern const CameraSwapEffects kPathChangeEffectToInternalScreenChangeEffect[10];

namespace AO {

class CameraSwapper;

struct CameraName final
{
    char_type name[8];
};
ALIVE_ASSERT_SIZEOF(CameraName, 8);

namespace CameraIds::Menu
{
    const s16 eMainMenu_1 = 1;
    const s16 eOptions_2 = 2;
    const s16 eGamespeakGamepad_3 = 3;
    const s16 eMotions_4 = 4;
    const s16 eSound_5 = 5;
    const s16 eLoad_6 = 6;
    const s16 eMotionsGamespeakGamepad_7 = 7;
    const s16 eCopyright_10 = 10;
    const s16 eLoading_21 = 21;
    const s16 eFmvSelect_30 = 30;
    const s16 eLvlSelect_31 = 31;
    const s16 eGamespeakKeyboard_33 = 33;
    const s16 eMotionsGamespeakKeyboard_37 = 37;
    const s16 eController_40 = 40;
    const s16 eControllerConfig_41 = 41;
}


class Map final : public BaseMap
{
public:
    Map(ResourceManagerWrapper& resMan, relive::Factory& factory);

    s16 GetOverlayId() override;

    BasePath& GetPath() override
    {
        return mPath;
    }

    CameraPos Rect_Location_Relative_To_Active_Camera(const PSX_RECT* pRect, s16 width = 0) override;
    s16 Get_Camera_World_Rect(CameraPos camIdx, PSX_RECT* pRect) override;
    s16 Is_Point_In_Current_Camera(EReliveLevelIds level, s32 path, FP xpos, FP ypos, s16 width) override;
    ScreenChangeResult GoTo_Camera() override;
    ScreenChangeResult ScreenChange() override;
    void VCameraSwapFinished() override;
    ScreenChangeResult Handle_PathTransition() override;

private:
    // Tells the objects about the screen change, the part of ScreenChange after the purple lights
    void NotifyObjectsOfScreenChange();
    // GoTo_Camera once any FMVs it plays first have finished, up to pending a new level's
    // paths. Returns eWaiting if it pended them.
    ScreenChangeResult StartLoadCamera();
    // Once the paths are loaded: takes them and pends the new level's sound files
    void LoadPathsAndPendSounds();
    // Carries on once the sound files are loaded (or straight after StartLoadCamera when the
    // level doesn't change), up to pending what the new camera's objects need
    void ContinueLoadCamera();
    // The rest of GoTo_Camera, once the main loop has waited for that loading
    void FinishLoadCamera();
    // After the path transition's camera change, see Handle_PathTransition
    void FinishPathTransition();

    // StartLoadCamera's state kept for FinishLoadCamera
    s16 mLoadCameraPrevPath = 0;
    EReliveLevelIds mLoadCameraPrevLevel = EReliveLevelIds::eNone;

    // Handle_PathTransition's state kept while its camera change waits
    bool mPathTransitionFromTlv = false;
    relive::reliveScale mPathTransitionScale = {};

public:
    void VCollectPurpleLightObjects(DynamicArrayT<BaseAnimatedWithPhysicsGameObject>& objects, DynamicArrayT<Particle>& lights) override;
    s32 VPurpleLightFrameCount(s16 bMakeInvisible) override;

    static CameraSwapper* FMV_Camera_Change(CamResource& ppBits, Map* pMap, EReliveLevelIds levelId);

    s16 mMapChanged = 0;

    Path mPath;
};

} // namespace AO

extern bool gMap_bDoPurpleLightEffect;