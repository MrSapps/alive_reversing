#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

class BaseAliveGameObject;

struct FootSwitch_Data final
{
    s32 field_0_frameTableOffset;
    s32 field_4_frameTableOffset;
    s16 field_8_maxH;
    s16 field_A_frameTableOffset;
};
ALIVE_ASSERT_SIZEOF_ALWAYS(FootSwitch_Data, 0xC);


struct FootSwitchSaveState final : public SaveStateBase
{
    FootSwitchSaveState()
        : SaveStateBase(ReliveTypes::eFootSwitch, sizeof(*this))
    { }
    s32 mState;
    Guid mStoodOnMeId;
    bool mCreateSparks;
    bool mFindStander;
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
    BaseAliveGameObject* WhoIsStoodOnMe();

private:
    Guid mTlvId;
    enum class States : s16
    {
        eWaitForStepOnMe = 0,
        eWaitForGetOffMe = 1,
    };
    States mState = States::eWaitForStepOnMe;
    s16 mSwitchId = 0;
    relive::reliveSwitchOp mAction = relive::reliveSwitchOp::eSetTrue;
    relive::Path_FootSwitch::FootSwitchTriggerBy mTriggeredBy = relive::Path_FootSwitch::FootSwitchTriggerBy::eAbe;
    Guid mStoodOnMeId;
    bool mCreateSparks = false;
    bool mFindStander = false;
};
