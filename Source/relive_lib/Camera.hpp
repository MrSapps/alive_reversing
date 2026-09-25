#pragma once

#include "Primitives.hpp"
#include "MapWrapper.hpp"
#include "ResourceManagerWrapper.hpp"

enum class LevelIds : s16;
class BaseMap;

class Camera final // TODO: May actually just be "ResourceList" ?
{
public:
    Camera();
    ~Camera();
    void CreateFG1(ResourceManagerWrapper& resMan, BaseMap& map);

public:
    CamResource mCamRes;
    s16 mCamXOff = 0;
    s16 mCamYOff = 0;
    s16 mPath = 0;
    EReliveLevelIds mLevel;
    u32 mCameraNumber = 0;
    // mCamRes is loaded (see BaseMap::Finish_Load_Cam)
    bool mCamResLoaded = false;
    // The anims this camera's objects need, kept loaded until the camera is freed (see
    // BaseMap::Load_Path_Items)
    ResourceManagerWrapper::AnimPins mAnimPins;
};
