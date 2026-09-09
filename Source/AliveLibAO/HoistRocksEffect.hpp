#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive {
struct Path_Hoist;
}

namespace AO {

class HoistParticle final : public BaseAnimatedWithPhysicsGameObject
{
public:
    HoistParticle(FP xpos, FP ypos, FP scale, AnimId animId, ResourceManagerWrapper& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    bool mHitGround = false;

private:
    AnimId mAnimId = {};
};

struct HoistRocksEffectSaveState final : public SaveStateBase
{
    HoistRocksEffectSaveState()
        : SaveStateBase(ReliveTypes::eHoist, sizeof(*this))
    { }
    Guid mTlvId;
};

// Not TLV-based - one of these is spawned freely by HoistRocksEffect::VUpdate()
// for each falling rock chunk, so (like Grenade) it gets its own ReliveTypes
// tag and is restored generically alongside everything else rather than
// re-derived from the parent effect (which doesn't track its own particles).
struct HoistParticleSaveState final : public SaveStateBase
{
    HoistParticleSaveState()
        : SaveStateBase(ReliveTypes::eHoistParticle, sizeof(*this))
    { }
    FP mXPos = {};
    FP mYPos = {};
    FP mVelY = {};
    FP mScale = {};
    AnimId mAnimId = {};
    bool mHitGround = false;
    s16 mCurrentPath = 0;
    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
};

class HoistRocksEffect final : public ::BaseGameObject
{
public:
    HoistRocksEffect(relive::Path_Hoist* pTlv, const Guid& tlvInfo, ResourceManagerWrapper& resMan, BaseMap& map);
    ~HoistRocksEffect();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    FP mTlvXPos = {};
    FP mTlvYPos = {};
    Guid mTlvId;
};

} // namespace AO
