#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

class Leaf final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Leaf(FP xpos, FP ypos, FP xVel, FP yVel, FP scale, ResourceManagerWrapper& resMan);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    bool mHitSomething = false;
};
