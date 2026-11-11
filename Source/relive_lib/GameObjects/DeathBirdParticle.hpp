#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct DeathBirdParticleSaveState final : public SaveStateBase
{
    DeathBirdParticleSaveState()
        : SaveStateBase(ReliveTypes::eDeathBird, sizeof(*this))
    { }
    // What it was made with
    s32 mStartTimer;
    bool mPlaySound;
    FP mScale;
    AnimatedObjectSaveState mObject;
    s8 mRandom;
    s8 mState;
};

class DeathBirdParticle final : public BaseAnimatedWithPhysicsGameObject
{
public:
    DeathBirdParticle(FP xpos, FP ypos, s32 startTimer, bool playSound, FP scale, ResourceManager& resMan, BaseMap& map);

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    s8 mRandom = 0;
    enum class States : s8
    {
        eAnimateDeathFlares_0 = 0,
        eTransformStarsToDoves_1 = 1
    };
    States mState = States::eAnimateDeathFlares_0;
    s32 mStartTimer = 0;
    bool mPlaySound = false;
};
