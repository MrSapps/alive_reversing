#pragma once

#include "BaseGameObject.hpp"
#include "../Psx.hpp"
#include "../SaveStateBase.hpp"
#include "../data_conversion/relive_tlvs.hpp"

struct InvisibleSwitchSaveState final : public SaveStateBase
{
    InvisibleSwitchSaveState()
        : SaveStateBase(ReliveTypes::eInvisibleSwitch, sizeof(*this))
    { }
    Guid mTlvId;
    s32 mDelayTimer;
    s16 mState;
};

class InvisibleSwitch final : public BaseGameObject
{
public:
    InvisibleSwitch(relive::Path_InvisibleSwitch* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~InvisibleSwitch();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    bool IsAbeUsingDoor() const;

private:
    s16 mSwitchId = 0;
    relive::reliveSwitchOp mAction = relive::reliveSwitchOp::eSetTrue;
    Guid mTlvId;
    s32 mDelayTimer = 0;
    s32 mActivationDelay = 0;
    PSX_Point mTlvTopLeft = {};
    PSX_Point mTlvBottomRight = {};
    enum class States : s16
    {
        eWaitForTrigger_0 = 0,
        eWaitForDelayTimer_1 = 1,
    };
    States mState = States::eWaitForTrigger_0;
    bool mSetOffAlarm = false;
    relive::Path_InvisibleSwitch::InvisibleSwitchScale mScale = relive::Path_InvisibleSwitch::InvisibleSwitchScale::eHalf;
};
