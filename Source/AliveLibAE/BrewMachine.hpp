#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "Font.hpp"

namespace relive
{
    struct Path_BrewMachine;
}

struct BrewMachineSaveState final : public SaveStateBase
{
    BrewMachineSaveState()
        : SaveStateBase(ReliveTypes::eBrewMachine, sizeof(*this))
    { }
    s16 mRemainingBrewCount;
    s16 mBrewMachineCamera;
    s16 mTextX;
    s16 mTextY;
};

class BrewMachine final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const BrewMachineSaveState& state);

    BrewMachine(relive::Path_BrewMachine* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~BrewMachine();
    
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    s16 GetRemainingBrewCount() const { return mRemainingBrewCount; }
    void DecreaseRemainingBrewCount() { mRemainingBrewCount--; }

private:
    FontContext mFontContext = {};
    AliveFont mFont = {};
    s16 mTextX = 0;
    s16 mTextY = 0;
    Guid mTlvInfo;
    s16 mRemainingBrewCount = 0;
    s16 mMaxBrewCount = 0;
    s16 mBrewMachineCamera = 0;
    PalResource mPal;
};
