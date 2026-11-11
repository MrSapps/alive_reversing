#pragma once

#include "../GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct ParticleBurst_Item;

enum class BurstType
{
    eRocks,
    eSticks,
    eBigPurpleSparks,
    eBigRedSparks,
    eGreenSparks,
    eSmallPurpleSparks,
    eMeat,
};

struct ParticleBurstSaveState final : public SaveStateBase
{
    ParticleBurstSaveState()
        : SaveStateBase(ReliveTypes::eParticleBurst, sizeof(*this))
    { }
    // What it was made with
    u32 mRequestedCount;
    s32 mBurstType;
    s32 mFineScale;
    bool mFadeout;
    FP mScale;
    AnimatedObjectSaveState mObject;
    s16 mParticleCount;
    u32 mAliveTimer;
    struct Particle final
    {
        FP mX;
        FP mY;
        FP mZ;
        FP mXSpeed;
        FP mYSpeed;
        FP mZSpeed;
        AnimationSaveState mAnim;
    };
    Particle mParticles[40];
};

class ParticleBurst final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    ParticleBurst(FP xpos, FP ypos, u32 particleCount, FP scale, BurstType type, ResourceManager& resMan, BaseMap& map, s32 fineScale = 13, bool bFadeOut = false);
    ~ParticleBurst();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    // How many it was asked for, which is more than it has in AE
    u32 mRequestedCount = 0;
    FP Random_Speed(FP scale);

private:
    ParticleBurst_Item* mParticleItems = nullptr;
    s16 mParticleCount = 0;
    u32 mAliveTimer = 0;
    BurstType mType = BurstType::eRocks;
    s16 mFineScale = 0;
    bool mFadeout = false;
};
