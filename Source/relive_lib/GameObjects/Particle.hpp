#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../Layer.hpp"
#include "../SaveStateBase.hpp"

class BaseMap;

struct ParticleSaveState final : public SaveStateBase
{
    ParticleSaveState()
        : SaveStateBase(ReliveTypes::eParticle, sizeof(*this))
    { }
    // Not the header type: a BirdPortal sets it on the particles it makes
    ReliveTypes mObjectType;
    AnimatedObjectSaveState mObject;
    FP mScaleAmount;
};

class Particle final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Particle(FP xpos, FP ypos, AnimResource& res, ResourceManager& resMan, BaseMap& map, bool explosionSizeHack = false);
    ~Particle();
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);
public:
    FP mScaleAmount = {};
};

Particle* New_DestroyOrCreateObject_Particle(FP xpos, FP ypos, FP scale, ResourceManager& resMan, BaseMap& map);
Particle* New_Orb_Particle(FP xpos, FP ypos, FP velX, FP velY, FP scale, Layer layer, const RGB16& rgb, ResourceManager& resMan, BaseMap& map);
Particle* New_TintChant_Particle(FP xpos, FP ypos, FP scale, Layer layer, ResourceManager& resMan, BaseMap& map);
void New_Smoke_Particles(FP xpos, FP ypos, FP scale, s16 count, const RGB16& rgb, ResourceManager& resMan, BaseMap& map);
void New_RandomizedChant_Particle(BaseAnimatedWithPhysicsGameObject* pObj, ResourceManager& resMan, BaseMap& map);
Particle* New_TintShiny_Particle(FP xpos, FP ypos, FP scale, Layer layer, ResourceManager& resMan, BaseMap& map);
void New_ShootingZFire_Particle(FP xpos, FP ypos, FP scale, ResourceManager& resMan, BaseMap& map);
void New_ShootingFire_Particle(FP xpos, FP ypos, s8 direction, FP scale, ResourceManager& resMan, BaseMap& map);
