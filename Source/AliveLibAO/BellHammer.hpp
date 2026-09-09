#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_BellHammer;
}

namespace AO {

enum class BellHammerStates : u16
{
    eWaitForActivation_0 = 0,
    eSmashingBell_1 = 1
};

class BellHammer final : public BaseAnimatedWithPhysicsGameObject
{
public:
    BellHammer(relive::Path_BellHammer* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~BellHammer();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

    BellHammerStates mState = BellHammerStates::eWaitForActivation_0;
    s16 mSwitchId = 0;
    Guid mTlvInfo;
    bool mSpawnElum = false;

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);
};

// mSwitchId and position are derived once from the TLV in the constructor
// and never change; mSpawnElum is transient (set and consumed within the
// same VUpdate call, never observed across frames) so it's safe to leave
// at its default. Only mState is meaningful runtime state.
struct BellHammerSaveState final : public SaveStateBase
{
    BellHammerSaveState()
        : SaveStateBase(ReliveTypes::eBellHammer, sizeof(*this))
    { }
    Guid mTlvInfo;
    BellHammerStates mState = BellHammerStates::eWaitForActivation_0;
};

} // namespace AO
