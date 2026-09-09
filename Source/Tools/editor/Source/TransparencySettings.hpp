#pragma once

class TransparencySettings final
{
public:
    int CameraTransparency() const
    {
        return mCameraTransparency;
    }

    int CollisionTransparency() const
    {
        return mCollisionTransparency;
    }

    int MapObjectTransparency() const
    {
        return mMapObjectTransparency;
    }

    void SetCameraTransparency(int value)
    {
        mCameraTransparency = value;
    }

    void SetCollisionTransparency(int value)
    {
        mCollisionTransparency = value;
    }

    void SetMapObjectTransparency(int value)
    {
        mMapObjectTransparency = value;
    }

private:
    int mCameraTransparency = 70;
    int mCollisionTransparency = 90;
    int mMapObjectTransparency = 60;
};
