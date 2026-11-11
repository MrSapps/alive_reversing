#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_FlintLockFire;
}

namespace AO {

struct FlintLockFireSaveState final : public SaveStateBase
{
    FlintLockFireSaveState()
        : SaveStateBase(ReliveTypes::eFlintLockFire, sizeof(*this))
    { }
    s32 mState;
    AnimationSaveState mGourdAnim;
    AnimationSaveState mFire1Anim;
    AnimationSaveState mFire2Anim;
};

class FlintLockFire final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const FlintLockFireSaveState& state);

    FlintLockFire(relive::Path_FlintLockFire* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~FlintLockFire();

    void LoadAnimations();

    virtual void VStopAudio() override;
    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;

    enum class States : s16
    {
        eDisabled_0 = 0,
        eActivating_1 = 1,
        eActivated_2 = 2
    };
    States mState = States::eDisabled_0;
    s16 mSwitchId = 0;
    Guid mTlvId;
    s32 mFireSound = 0;
    Animation mGourdAnim;
    Animation mFire1Anim;
    Animation mFire2Anim;
};

} // namespace AO
