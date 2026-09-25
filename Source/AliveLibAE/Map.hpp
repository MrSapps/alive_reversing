#pragma once

#include "../relive_lib/MapWrapper.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/BaseMap.hpp"
#include "../relive_lib/Factory.hpp"
#include "Path.hpp"

class BaseGameObject;
class Camera;
class BinaryPath;
enum class LevelIds : s16;
struct PSX_Point;
enum class EReliveLevelIds : s16;
class CamResource;
class ResourceManagerWrapper;

namespace relive
{
    class Path_TLV;
    enum class LoadMode : s16;
}

struct CameraName final
{
    char_type name[8];
};
ALIVE_ASSERT_SIZEOF(CameraName, 8);


class Map final : public BaseMap
{
public:
    Map(ResourceManagerWrapper& resMan, relive::Factory& factory);
    ~Map();

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
    // GoTo_Camera once any FMVs it plays first have finished, up to pending what the new
    // camera's objects need
    void StartLoadCamera();
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

    static BaseGameObject* FMV_Camera_Change(CamResource& ppBits, Map* pMap, EReliveLevelIds lvlId);

private:
    void CreateScreenTransistionForTLV(relive::Path_TLV* pTlv);

public:
    Path mPath;
};

extern bool gMap_bDoPurpleLightEffect;
extern const CameraSwapEffects kPathChangeEffectToInternalScreenChangeEffect[10];
