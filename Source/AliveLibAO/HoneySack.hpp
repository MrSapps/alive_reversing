#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_HoneySack;
}

namespace AO {

class BeeSwarm;

struct HoneySackSaveState final : public SaveStateBase
{
    HoneySackSaveState()
        : SaveStateBase(ReliveTypes::eHoneySack, sizeof(*this))
    { }
    s32 mState;
    s16 mHitGround;
    s32 mTimer;
    Guid mBeeSwarm;
    FP mDripTargetX;
    FP mDripTargetY;
    FP mObjectYPos;
    u16 mChaseTime;
};

class HoneySack final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const HoneySackSaveState& state);

    HoneySack(relive::Path_HoneySack* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~HoneySack();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VOnThrowableHit(BaseGameObject* pFrom) override;
    virtual void VUpdate() override;

    Guid mTlvInfo;
    enum class State : s16
    {
        eDripHoney_0 = 0,
        eSetFallAnimation_1 = 1,
        eFallOnGround_2 = 2,
        eUpdateHoneySackOnGround_3 = 3
    };
    State mState = State::eDripHoney_0;
    s16 mHitGround = 0;
    s32 mTimer = 0;
    Guid mBeeSwarm = {};
    FP mDripTargetX = {};
    FP mDripTargetY = {};
    FP mObjectYPos = {};
    u16 mChaseTime = 0;
};


} // namespace AO
