#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/OrbWhirlWindParticle.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct OrbWhirlWindSaveState final : public SaveStateBase
{
    OrbWhirlWindSaveState()
        : SaveStateBase(ReliveTypes::eOrbWhirlWind, sizeof(*this))
    { }
    FP mXPos;
    FP mYPos;
    FP mScale;
    s32 mParticleSpawnCounter;
    s32 mState;
    s16 mParticleIdx;
    bool mIsMudokonSpirit;
    OrbWhirlWindParticleState mParticles[16];
};

class OrbWhirlWind final : public BaseGameObject
{
public:
    enum class ParticlesState : s16
    {
        eCreating = 0,
        eCreated = 1,
        eActive = 2
    };

    OrbWhirlWind(FP xpos, FP ypos, FP scale, bool bIsMudokonSpirit, ResourceManager& resMan, BaseMap& map);
    ~OrbWhirlWind();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    virtual void VPostRestore() override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    // For a save: the particles are made from it
    struct RestoreTag final
    { };
    OrbWhirlWind(RestoreTag, ResourceManager& resMan, BaseMap& map);

    void ToSpin(FP xpos, FP ypos, FP scale, BaseGameObject* pObj);
    void ToStop();

private:
    s32 mParticleSpawnCounter = 0;
    ParticlesState mState = ParticlesState::eCreating;
    bool mIsMudokonSpirit = false;
    s16 mParticleIdx = 0;
    OrbWhirlWindParticle* mOrbParticles[16] = {};
    FP mXPos = {};
    FP mYPos = {};
    FP mScale = {};
};
