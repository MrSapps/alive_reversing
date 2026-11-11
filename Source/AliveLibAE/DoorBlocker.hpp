#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_DoorBlocker;
}

struct DoorBlockerSaveState final : public SaveStateBase
{
    DoorBlockerSaveState()
        : SaveStateBase(ReliveTypes::eDoorBlocker, sizeof(*this))
    { }
    bool mDone;
};

class DoorBlocker final : public BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const DoorBlockerSaveState& state);

    DoorBlocker(relive::Path_DoorBlocker* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~DoorBlocker();

    void LoadAnimations();
    virtual void VUpdate() override;

private:
    Guid mTlvId;
    s16 mSwitchId = 0;
    bool mDone = false;
};
