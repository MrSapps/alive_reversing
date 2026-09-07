#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

class AirExplosion final : public BaseAnimatedWithPhysicsGameObject
{
public:
    AirExplosion(FP xpos, FP ypos, FP explosionScale, bool bSmall, ResourceManagerWrapper& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    void DealBlastDamage(PSX_RECT* pRect);

private:
    FP mExplosionScale = {};
    bool mSmallExplosion = false;
    FP mParticleBurstScale = {};
};
