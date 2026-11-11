#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct LeafSaveState final : public SaveStateBase
{
    LeafSaveState()
        : SaveStateBase(ReliveTypes::eLeaf, sizeof(*this))
    { }
    FP mXPos;
    FP mYPos;
    FP mVelX;
    FP mVelY;
    FP mSpriteScale;
    bool mHitSomething;
};

class Leaf final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Leaf(FP xpos, FP ypos, FP xVel, FP yVel, FP scale, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    // Where the leaves are in the table of random bytes they all share
    static u8 GetRandIdx();
    static void SetRandIdx(u8 idx);

private:
    bool mHitSomething = false;
};
