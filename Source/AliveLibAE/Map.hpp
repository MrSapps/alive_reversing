#pragma once

#include "../relive_lib/MapWrapper.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/BaseMap.hpp"
#include "../relive_lib/Factory.hpp"
#include "Path.hpp"

class BaseGameObject;
class Camera;
struct PathData;
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
    void GetCurrentCamCoords(PSX_Point* pPoint) override;
    void GoTo_Camera() override;
    void ScreenChange() override;
    void Handle_PathTransition() override;
    void VCollectPurpleLightObjects(DynamicArrayT<BaseAnimatedWithPhysicsGameObject>& objects, DynamicArrayT<Particle>& lights) override;
    s32 VPurpleLightFrameCount(s16 bMakeInvisible) override;

    void VClearPendingSaveRestore() override
    {
        mRestoreMapObjectStates = false;
    }

    void Get_map_size(PSX_Point* pPoint);
    static BaseGameObject* FMV_Camera_Change(CamResource& ppBits, Map* pMap, EReliveLevelIds lvlId);
    Camera* Create_Camera(s16 xpos, s16 ypos, s32 a4);
    void Load_Path_Items(Camera* pCamera, relive::Factory::LoadMode loadMode);

private:
    void CreateScreenTransistionForTLV(relive::Path_TLV* pTlv);

public:
    s16 mDoorTransition = 0;
    s16 mTeleporterTransition = 0;

    bool mRestoreMapObjectStates = false;

    Path mPath;
};

extern bool gMap_bDoPurpleLightEffect;
extern const CameraSwapEffects kPathChangeEffectToInternalScreenChangeEffect[10];

s32 MaxGridBlocks(FP scale);
