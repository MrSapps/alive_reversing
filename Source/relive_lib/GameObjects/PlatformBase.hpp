#pragma once

#include "BaseAliveGameObject.hpp"

namespace relive {
class Path_TLV;
}

class Map;

class PlatformBase : public BaseAliveGameObject
{
public:
    static void MakeArray();
    static void FreeArray();
    static DynamicArrayT<BaseGameObject>& Platforms();

    PlatformBase(ResourceManager& resMan, BaseMap& map);
    ~PlatformBase();

    // While a restore puts the objects back, they add themselves to the platform they stand on again, which a saved
    // platform has counted already
    static void SetRestoring(bool restoring);
    static bool IsRestoring();

    virtual void VAdd(BaseAliveGameObject* pObj);
    virtual void VRemove(BaseAliveGameObject* pObj);

    // Puts the line of each platform with something on it where the platform is: it was done when that stepped on, which
    // a restore doesn't do again
    static void SyncLinesAfterRestore();

    // The line a character on it stands on
    PathLine* GetPlatformLine() const { return mPlatformBaseCollisionLine; }
protected:
    void AddDynamicCollision(AnimId animId, relive::Path_TLV* pTlv, const Guid& tlvId);
    void SyncCollisionLinePosition();

protected:
    s32 mPlatformBaseCount = 0;
    s16 mPlatformBaseXOffset = 0;
    s16 mPlatformBaseWidthOffset = 0;
    s16 mPlatformBaseYOffset = 0;
    s16 mPlatformBaseHeightOffset = 0;
    PathLine* mPlatformBaseCollisionLine = nullptr;
    Guid mPlatformBaseTlvInfo;

private:
    static DynamicArrayT<BaseGameObject>* sPlatformsArray;
};

