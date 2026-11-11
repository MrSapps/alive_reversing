#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

namespace AO {

enum class LeverState : s16
{
    eWaiting_0 = 0,
    ePulled_1 = 1,
    eFinished_2 = 2
};

struct LeverSaveState final : public SaveStateBase
{
    LeverSaveState()
        : SaveStateBase(ReliveTypes::eLever, sizeof(*this))
    { }
    s32 mState;
    bool mPulledFromLeft;
};

class Lever final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const LeverSaveState& state);

    Lever(relive::Path_Lever* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~Lever();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    // New virtual
    virtual s32 VPull(s16 bLeftDirection);

private:
    s16 mSwitchId = 0;
    LeverState mState = LeverState::eWaiting_0;
    Guid mTlvId;
    bool mPulledFromLeft = false;
    relive::reliveSwitchOp mAction = relive::reliveSwitchOp::eSetTrue;
    relive::Path_Lever::LeverSoundType mOnSound = relive::Path_Lever::LeverSoundType::eNone;
    relive::Path_Lever::LeverSoundType mOffSound = relive::Path_Lever::LeverSoundType::eNone;
    relive::Path_Lever::LeverSoundDirection mSoundDirection = relive::Path_Lever::LeverSoundDirection::eLeftAndRight;
};

} // namespace AO
