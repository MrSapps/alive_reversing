#pragma once

#include "BaseGameObject.hpp"
#include "../WorldStatic.hpp"
#include "../SaveStateBase.hpp"
#include "../Font.hpp"

namespace relive
{
    struct Path_GasCountDown;
}

struct GasCountDownSaveState final : public SaveStateBase
{
    GasCountDownSaveState()
        : SaveStateBase(ReliveTypes::eGasCountDown, sizeof(*this))
    { }
    s16 mGasTimeLeftSecs;
    u16 mGasCountdownTimer;
    s16 mGasXPos;
    s16 mGasYPos;
};

class GasCountDown final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const GasCountDownSaveState& state);

    GasCountDown(relive::Path_GasCountDown* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~GasCountDown();
    
    virtual void VScreenChanged() override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VUpdate() override;

private:
    void DealDamage();

private:
    PalResource mPal;
    FontContext mFontContext = {};
    AliveFont mFont = {};
    Guid mTlvId;
    s16 mGasXPos = 0;
    s16 mGasYPos = 0;
    s16 mStartTimerSwitchId = 0;
    s16 mStopTimerSwitchId = 0;
    s16 mGasTimeLeftSecs = 0;
    u16 mGasCountdownTimer = 0;
};

extern WorldStatic<bool> gDeathGasOn;
extern WorldStatic<s32> gDeathGasTimer;
