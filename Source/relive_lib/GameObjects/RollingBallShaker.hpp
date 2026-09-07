#pragma once

#include "BaseGameObject.hpp"
#include "../Primitives.hpp"

class RollingBallShaker final : public ::BaseGameObject
{
public:
    RollingBallShaker(ResourceManagerWrapper& resMan);
    ~RollingBallShaker();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;

    Prim_ScreenOffset mPrimScreenOffset = {};
    s16 mShakeTableIdx = 0;
    bool mStopShaking = false;
};
