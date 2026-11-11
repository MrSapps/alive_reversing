#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "Font.hpp"
#include "../relive_lib/Primitives.hpp"

constexpr s16 kMeterBarsXCount = 20;

namespace relive
{
    struct Path_ColourfulMeter;
}

struct ColourfulMeterSaveState final : public SaveStateBase
{
    ColourfulMeterSaveState()
        : SaveStateBase(ReliveTypes::eColourfulMeter, sizeof(*this))
    { }
    s16 mPolysToRenderCount;
    s16 mCount;
    s16 mMinesAlarmCountdown;
    s16 mStartingSwitchState;
    s16 mBarCount;
    bool mStartFilled;
    s16 mTextX;
    s16 mTextY;
};

class ColourfulMeter final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const ColourfulMeterSaveState& state);

    ColourfulMeter(relive::Path_ColourfulMeter* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~ColourfulMeter();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;

private:
    FontContext mFontContext = {};
    AliveFont mFont = {};
    Guid mTlvInfo;
    s16 mTlvX = 0;
    s16 mTlvY = 0;
    PalResource mPal;
    s16 mSwitchId = 0;
    s16 mNumberOfMeterBars = 0;
    s16 mTextX = 0;
    s16 mTextY = 0;
    s16 mPolysToRenderCount = 0;
    s16 mCount = 15;
    s16 mMinesAlarmCountdown = 0;
    s16 mStartingSwitchState = 0;
    s16 mBarCount = 0;
    bool mStartFilled = false;
    Poly_G4 mPolyG4s[kMeterBarsXCount] = {};
};

extern u8 gTotalMeterBars;
extern bool gbDrawMeterCountDown;
