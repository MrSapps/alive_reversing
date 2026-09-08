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
    void ScreenChange() override;

    void GoTo_Camera() override;
    void RemoveObjectsWithPurpleLight(s16 a2);
    void Handle_PathTransition() override;
    void Init(EReliveLevelIds level, s16 path, s16 camera, CameraSwapEffects screenChangeEffect, s16 fmvBaseId, s16 forceChange) override;
    void Shutdown() override;
    void Reset();
    Map(ResourceManagerWrapper& resMan, relive::Factory& factory);
    ~Map();

    void Get_map_size(PSX_Point* pPoint);
    void GetCurrentCamCoords(PSX_Point* pPoint) override;
    s16 GetOverlayId() override;
    CameraPos Rect_Location_Relative_To_Active_Camera(const PSX_RECT* pRect, s16 width = 0) override;
    static BaseGameObject* FMV_Camera_Change(CamResource& ppBits, Map* pMap, EReliveLevelIds lvlId);
    Camera* Create_Camera(s16 xpos, s16 ypos, s32 a4);
    void Load_Path_Items(Camera* pCamera, relive::Factory::LoadMode loadMode);

    void LoadResource(const char_type* pFileName, s32 type, s32 resourceId, relive::Factory::LoadMode loadMode, s16 bDontLoad = 0);

    s16 Is_Point_In_Current_Camera(EReliveLevelIds level, s32 path, FP xpos, FP ypos, s16 width) override;

    s16 Get_Camera_World_Rect(CameraPos camIdx, PSX_RECT* pRect) override;
    TlvIterator TLV_From_Offset_Lvl_Cam(const Guid& tlvId) override;

    void Reset_TLVs(u16 pathId);

private:

    void CreateScreenTransistionForTLV(relive::Path_TLV* pTlv);

public:

    s16 mDoorTransition = 0;
    s16 mTeleporterTransition = 0;

    bool mRestoreMapObjectStates = false;

    BasePath& GetPath() override
    {
        return mPath;
    }

    Path mPath;
};

extern bool gMap_bDoPurpleLightEffect;
extern const CameraSwapEffects kPathChangeEffectToInternalScreenChangeEffect[10];

s32 MaxGridBlocks(FP scale);
