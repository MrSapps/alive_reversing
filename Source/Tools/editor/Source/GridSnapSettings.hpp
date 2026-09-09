#pragma once

#include "IGridPointSnapper.hpp"

class GridSnapSettings final
{
public:
    struct SnapSetting final
    {
        bool mSnapX = false;
        bool mSnapY = false;
    };

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
