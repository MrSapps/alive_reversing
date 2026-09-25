#pragma once

#include <string>


class CameraImageAndLayers final
{
public:
    std::string mCameraImage;
    std::string mForegroundLayer;
    std::string mBackgroundLayer;
    std::string mForegroundWellLayer;
    std::string mBackgroundWellLayer;

    bool HaveFG1Layers() const
    {
        return !mForegroundLayer.empty() || !mBackgroundLayer.empty() || !mForegroundWellLayer.empty() || !mBackgroundWellLayer.empty();
    }
};

