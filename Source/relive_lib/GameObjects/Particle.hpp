#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../Layer.hpp"

class BaseMap;

class Particle final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Particle(FP xpos, FP ypos, AnimResource& res, ResourceManagerWrapper& resMan, BaseMap& map, bool explosionSizeHack = false);
    ~Particle();
    virtual void VUpdate() override;
public:
    FP mScaleAmount = {};
};

Particle* New_DestroyOrCreateObject_Particle(FP xpos, FP ypos, FP scale, ResourceManagerWrapper& resMan, BaseMap& map);
Particle* New_Orb_Particle(FP xpos, FP ypos, FP velX, FP velY, FP scale, Layer layer, const RGB16& rgb, ResourceManagerWrapper& resMan, BaseMap& map);
Particle* New_TintChant_Particle(FP xpos, FP ypos, FP scale, Layer layer, ResourceManagerWrapper& resMan, BaseMap& map);
void New_Smoke_Particles(FP xpos, FP ypos, FP scale, s16 count, const RGB16& rgb, ResourceManagerWrapper& resMan, BaseMap& map);
void New_RandomizedChant_Particle(BaseAnimatedWithPhysicsGameObject* pObj, ResourceManagerWrapper& resMan, BaseMap& map);
Particle* New_TintShiny_Particle(FP xpos, FP ypos, FP scale, Layer layer, ResourceManagerWrapper& resMan, BaseMap& map);
void New_ShootingZFire_Particle(FP xpos, FP ypos, FP scale, ResourceManagerWrapper& resMan, BaseMap& map);
void New_ShootingFire_Particle(FP xpos, FP ypos, s8 direction, FP scale, ResourceManagerWrapper& resMan, BaseMap& map);
