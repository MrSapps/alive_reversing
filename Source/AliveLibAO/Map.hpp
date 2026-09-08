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

struct PathData;
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
    void GetCurrentCamCoords(PSX_Point* pPoint) override;
    void GoTo_Camera() override;
    void ScreenChange() override;
    void Handle_PathTransition() override;
    void VCollectPurpleLightObjects(DynamicArrayT<BaseAnimatedWithPhysicsGameObject>& objects, DynamicArrayT<Particle>& lights) override;
    s32 VPurpleLightFrameCount(s16 bMakeInvisible) override;

    void VClearPendingSaveRestore() override
    {
        mSaveData = nullptr;
    }

    static CameraSwapper* FMV_Camera_Change(CamResource& ppBits, Map* pMap, EReliveLevelIds levelId);
    Camera* Create_Camera(s16 xpos, s16 ypos, s32 a4);
    void Load_Path_Items(Camera* pCamera, relive::Factory::LoadMode loadMode);
    void RestoreBlyData(const u8* pSaveData);
    void SaveBlyData(u8* pSaveBuffer);

    s16 mMapChanged = 0;
    u8* mSaveData = nullptr;

    Path mPath;
};


s32 MaxGridBlocks(FP scale);

} // namespace AO

extern bool gMap_bDoPurpleLightEffect;