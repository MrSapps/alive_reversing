#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct StringTable;

namespace relive
{
    struct Path_HintFly;
}

namespace AO {

struct HintFlyParticle;

struct HintFlySaveState final : public SaveStateBase
{
    HintFlySaveState()
        : SaveStateBase(ReliveTypes::eHintFly, sizeof(*this))
    { }
    static constexpr s32 kMaxParticles = 512;
    s32 mTimer;
    s32 mState;
    s16 mScreenX;
    s16 mScreenY;
    s16 mCounter;
    s16 mMsgLength;
    s16 mMsgIdx;
    s16 mHintFlyIdx;
    s16 mTargetCount;
    struct Particle final
    {
        FP mXPos;
        FP mYPos;
        FP mVelX;
        FP mVelY;
        FP mTargetX;
        FP mTargetY;
        s16 mSoundPitch;
        s16 mSoundPitchSpeed;
        s8 mState;
        s8 mAngle;
        s8 mAngleSpeed;
    };
    Particle mParticles[kMaxParticles];
};

class HintFly final : public BaseAnimatedWithPhysicsGameObject
{
public:
    HintFly(relive::Path_HintFly* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~HintFly();

    void InitParticle(HintFlyParticle* pParticle);

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    // The fly the camera makes (from its TLV) is put back once the camera has made it
    static void RestoreFromSaveState(const HintFlySaveState& state);

    void FormWordAndAdvanceToNextWord();
    void UpdateParticles();

    HintFlyParticle* mHintFlyParticle = nullptr;
    s32 mTimer = 0;

    enum class State : s16
    {
        eState_0 = 0,
        eIdleWaitForChanting_1 = 1,
        eState_2 = 2,
        eState_3 = 3,
        eState_4 = 4,
        eState_5 = 5,
        eState_6 = 6,
    };
    State mState = State::eState_0;
    s16 mScreenX = 0;
    s16 mScreenY = 0;
    s16 mCounter = 0;
    s16 mMsgLength = 0;
    s16 mMessageId = 0;
    s16 mMsgIdx = 0;
    s16 mHintFlyIdx = 0;
    s16 mTargetCount = 0;
    Guid mTlvId;
};

} // namespace AO
