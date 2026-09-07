#include "stdafx.h"
#include "Camera.hpp"
#include "FG1.hpp"

Camera::Camera()
{
    mLevel = EReliveLevelIds::eNone;
}

Camera::~Camera()
{

}

void Camera::On_Loaded(Camera* /*pCamera*/)
{

}

void Camera::CreateFG1(ResourceManagerWrapper& resMan, BaseMap& map)
{
    Fg1Resource fg1Res = resMan.LoadFg1(mLevel, mPath, mCameraNumber);
    if (fg1Res.Any())
    {
        relive_new FG1(fg1Res, mCamRes, resMan, map);
    }
}
