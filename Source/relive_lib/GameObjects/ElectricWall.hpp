#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../data_conversion/relive_tlvs.hpp"

struct ElectricWallSaveState final : public SaveStateBase
{
    ElectricWallSaveState()
        : SaveStateBase(ReliveTypes::eElectricWall, sizeof(*this))
    { }
    s32 mSoundTimer;
};

class ElectricWall final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const ElectricWallSaveState& state);

    ElectricWall(relive::Path_ElectricWall* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~ElectricWall();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

private:
    Guid mTlvInfo;
    s16 mSwitchId = 0;
    relive::Path_ElectricWall::ElectricWallStartState mStartState = relive::Path_ElectricWall::ElectricWallStartState::eOff;
    s32 mSoundTimer = 0;
};
