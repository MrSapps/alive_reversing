#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct AirExplosionSaveState final : public SaveStateBase
{
    AirExplosionSaveState()
        : SaveStateBase(ReliveTypes::eAirExplosion, sizeof(*this))
    { }
    bool mSmallExplosion;
    AnimatedObjectSaveState mObject;
    FP mExplosionScale;
    FP mParticleBurstScale;
};

class AirExplosion final : public BaseAnimatedWithPhysicsGameObject
{
public:
    AirExplosion(FP xpos, FP ypos, FP explosionScale, bool bSmall, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    // For a save: made without the screen shake, blast, sound and other things it does as it starts
    struct RestoreTag final
    { };
    AirExplosion(RestoreTag, bool bSmall, ResourceManager& resMan, BaseMap& map);

    void DealBlastDamage(PSX_RECT* pRect);

private:
    FP mExplosionScale = {};
    bool mSmallExplosion = false;
    FP mParticleBurstScale = {};
};
