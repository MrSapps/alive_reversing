#pragma once

#include "IPointSnapper.hpp"
#include "SnapSetting.hpp"

class SnapSettings final
{
public:
    const SnapSetting& MapObjectSnapping() const
    {
        return mMapObjectSnappingSettings;
    }

    SnapSetting& MapObjectSnapping()
    {
        return mMapObjectSnappingSettings;
    }

    SnapSetting& CollisionSnapping()
    {
        return mCollisionSnappingSettings;
    }

    const SnapSetting& CollisionSnapping() const
    {
        return mCollisionSnappingSettings;
    }

private:
    SnapSetting mMapObjectSnappingSettings;
    SnapSetting mCollisionSnappingSettings;
};
