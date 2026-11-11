#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

class BaseAliveGameObject;

namespace AO {

struct FootSwitchSaveState final : public SaveStateBase
{
    FootSwitchSaveState()
        : SaveStateBase(ReliveTypes::eFootSwitch, sizeof(*this))
    { }
    s32 mState;
    Guid mStoodOnMeId;
};

class FootSwitch final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const FootSwitchSaveState& state);

    FootSwitch(relive::Path_FootSwitch* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~FootSwitch();

    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

private:
    ::BaseAliveGameObject* WhoIsStoodOnMe();

private:
    Guid mTlvId;
    enum class States : s16
    {
        eWaitForStepOnMe_0 = 0,
        eWaitForGetOffMe_1 = 1,
    };
    States mState = States::eWaitForStepOnMe_0;
    s16 mSwitchId = 0;
    relive::reliveSwitchOp mAction = relive::reliveSwitchOp::eSetTrue;
    relive::Path_FootSwitch::FootSwitchTriggerBy mTriggeredBy = relive::Path_FootSwitch::FootSwitchTriggerBy::eAbe;
    Guid mStoodOnMeId;
};


} // namespace AO
