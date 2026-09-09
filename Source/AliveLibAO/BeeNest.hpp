#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;
enum class EReliveLevelIds : s16;

namespace relive
{
    struct Path_BeeNest;
}

namespace AO {

enum class BeeNestStates : u16
{
    eWaitForTrigger_0,
    eResetIfDead_1
};

class BeeSwarm;

class BeeNest final : public ::BaseGameObject
{
public:
    BeeNest(relive::Path_BeeNest* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~BeeNest();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

    FP mBeeSwarmX = {};
    FP mBeeSwarmY = {};
    Guid mTlvInfo;
    u16 mSwitchId = 0;
    s16 mSwarmSize = 0;
    u16 mTotalChaseTime = 0;
    BeeNestStates mState = BeeNestStates::eWaitForTrigger_0;
    FP mSpeed = {};
    Guid mBeeSwarm = {};

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);
};

// mBeeSwarmX/Y, mSwitchId, mSwarmSize, mTotalChaseTime and mSpeed are all
// derived once from the TLV in the constructor and never change, so
// reconstructing from the TLV recovers them for free - only mState is
// meaningful runtime state. mBeeSwarm (the active chase swarm) isn't
// restorable since BeeSwarm has no save-state of its own, so a save taken
// mid-chase (eResetIfDead_1) restores into eWaitForTrigger_0 instead.
struct BeeNestSaveState final : public SaveStateBase
{
    BeeNestSaveState()
        : SaveStateBase(ReliveTypes::eBeeNest, sizeof(*this))
    { }
    Guid mTlvInfo;
    BeeNestStates mState = BeeNestStates::eWaitForTrigger_0;
};

} // namespace AO
