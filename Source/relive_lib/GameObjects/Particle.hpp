#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../Layer.hpp"

class Particle final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Particle(FP xpos, FP ypos, AnimResource& res, ResourceManagerWrapper& resMan, bool explosionSizeHack = false);
    ~Particle();
    virtual void VUpdate() override;
public:
    FP mScaleAmount = {};
};

Particle* New_DestroyOrCreateObject_Particle(FP xpos, FP ypos, FP scale, ResourceManagerWrapper& resMan);
Particle* New_Orb_Particle(FP xpos, FP ypos, FP velX, FP velY, FP scale, Layer layer, const RGB16& rgb, ResourceManagerWrapper& resMan);
Particle* New_TintChant_Particle(FP xpos, FP ypos, FP scale, Layer layer, ResourceManagerWrapper& resMan);
void New_Smoke_Particles(FP xpos, FP ypos, FP scale, s16 count, const RGB16& rgb, ResourceManagerWrapper& resMan);
void New_RandomizedChant_Particle(BaseAnimatedWithPhysicsGameObject* pObj, ResourceManagerWrapper& resMan);
Particle* New_TintShiny_Particle(FP xpos, FP ypos, FP scale, Layer layer, ResourceManagerWrapper& resMan);
void New_ShootingZFire_Particle(FP xpos, FP ypos, FP scale, ResourceManagerWrapper& resMan);
void New_ShootingFire_Particle(FP xpos, FP ypos, s8 direction, FP scale, ResourceManagerWrapper& resMan);
