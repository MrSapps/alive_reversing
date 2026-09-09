#pragma once

#include <string>
#include <vector>
#include <memory>

#include <QPixmap> // TODO: Not sure if we wanted any Qt stuff in the model

class MapObjectBase;
using UP_MapObjectBase = std::unique_ptr<MapObjectBase>;

struct EditorCamera final
{
    std::string mName;
    int mId = 0;
    int mX = 0;
    int mY = 0;
    std::vector<UP_MapObjectBase> mMapObjects;

    class CameraImageAndLayers final
    {
    public:
        QPixmap mCameraImage;
        QPixmap mForegroundLayer;
        QPixmap mBackgroundLayer;
        QPixmap mForegroundWellLayer;
        QPixmap mBackgroundWellLayer;
    };
    CameraImageAndLayers mCameraImageandLayers;

};
using UP_Camera = std::unique_ptr<EditorCamera>;
