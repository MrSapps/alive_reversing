#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"

class GroundExplosion final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VUpdate() override;

    GroundExplosion(FP x, FP y, FP scale, ResourceManagerWrapper& resMan, BaseMap& map);
private:
    void DealDamageRect(const PSX_RECT* pRect);

    FP mBombSpriteScale = {};
};
