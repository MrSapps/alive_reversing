#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/Animation.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_Hoist;
}

struct HoistRockParticle final
{
    s16 mState;
    FP mXPos;
    FP mYPos;
    FP mVelY;
    Animation mAnim;
};

// The rocks falling from a hoist. Not made from this: the hoist's TLV makes the effect again when a save is
// restored, and this puts back where it was, see HoistRocksEffect::RestoreSaveState.
struct HoistRocksEffectSaveState final : public SaveStateBase
{
    HoistRocksEffectSaveState()
        : SaveStateBase(ReliveTypes::eHoist, sizeof(*this))
    { }
    s32 mTimer;
    struct Rock final
    {
        s16 mState;
        FP mXPos;
        FP mYPos;
        FP mVelY;
        AnimId mAnimId;
        AnimationSaveState mAnim;
    };
    Rock mRocks[4];
};

class HoistRocksEffect final : public BaseGameObject
{
public:
    HoistRocksEffect(relive::Path_Hoist* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~HoistRocksEffect();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreSaveState(const HoistRocksEffectSaveState& state);

    void LoadAnimations();

private:
    s16 mTlvXPos = 0;
    s16 mTlvYPos = 0;
    Guid mTlvId;
    s32 mTimer = 0;
    FP mSpriteScale = {};
    HoistRockParticle mRocks[4] = {};
};
