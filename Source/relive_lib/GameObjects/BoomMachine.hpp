#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

namespace relive
{
    struct Path_BoomMachine;
}

struct BoomMachineSaveState final : public SaveStateBase
{
    BoomMachineSaveState()
        : SaveStateBase(ReliveTypes::eBoomMachine, sizeof(*this))
    { }
    Guid mPipeId;
    bool mIsButtonOn;
};

class BoomMachine final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const BoomMachineSaveState& state);

    BoomMachine(relive::Path_BoomMachine* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~BoomMachine();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual bool VIsButtonOn();
    virtual void VHandleButton();

private:
    void LoadAnimations();

public:
    Guid mTlvId;
    Guid mPipeId;
    bool mIsButtonOn = false;
};
