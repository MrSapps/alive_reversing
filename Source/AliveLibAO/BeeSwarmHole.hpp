#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/Psx.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace AO {

class BeeSwarmHole final : public ::BaseGameObject
{
public:
    BeeSwarmHole(relive::Path_BeeSwarmHole* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);

    virtual void VUpdate() override;

    Guid mTlvId;
    PSX_RECT field_14_rect = {};
    s32 mStartIntervalTimer = 0;
    u16 mStartInterval = 0;
    relive::Path_BeeSwarmHole::MovementType mMovementType = relive::Path_BeeSwarmHole::MovementType::eHover;
    s16 field_26_bees_amount = 0;
    u16 field_28_chase_time = 0;
    u16 field_2A_speed = 0;

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);
};

// Every field except mStartIntervalTimer is derived once from the TLV in
// the constructor and never changes, so reconstructing from the TLV
// recovers them for free.
struct BeeSwarmHoleSaveState final : public SaveStateBase
{
    BeeSwarmHoleSaveState()
        : SaveStateBase(ReliveTypes::eBeeSwarmHole, sizeof(*this))
    { }
    Guid mTlvId;
    s32 mStartIntervalTimer = 0;
};

} // namespace AO
