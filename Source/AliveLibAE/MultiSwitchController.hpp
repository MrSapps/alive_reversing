#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"

struct MultiSwitchControllerSaveState final : public SaveStateBase
{
    MultiSwitchControllerSaveState()
        : SaveStateBase(ReliveTypes::eMultiSwitchController, sizeof(*this))
    { }
    s32 mLastSwitchOnTime;
    s32 mAllSwitchesOnOrOffTime;
    bool mIsOn;
};

class MultiSwitchController final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const MultiSwitchControllerSaveState& state);

    MultiSwitchController(relive::Path_MultiSwitchController* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~MultiSwitchController();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    void WaitingForAllOn();
    void WaitingForAllOff();

private:
    u16 mInputSwitchId1 = 0;
    u16 mInputSwitchId2 = 0;
    u16 mInputSwitchId3 = 0;
    u16 mInputSwitchId4 = 0;
    u16 mInputSwitchId5 = 0;
    u16 mInputSwitchId6 = 0;
    u16 mOutputSwitchId = 0;
    relive::reliveSwitchOp mAction = relive::reliveSwitchOp::eSetTrue;
    Guid mTlvId;
    s32 mLastSwitchOnTime = 0;
    s32 mAllSwitchesOnOrOffTime = 0;
    s32 mOnOffDelay = 0;
    bool mIsOn = false;
};
