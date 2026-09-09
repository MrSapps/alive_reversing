#pragma once


#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_LiftMover;
}

namespace AO {

class LiftPoint;

enum class LiftMoverStates : s16
{
    eInactive_0 = 0,
    eStartMovingDown_1 = 1,
    eMovingDown_2 = 2,
    eStartMovingUp_3 = 3,
    eMovingUp_4 = 4,
    eMovingDone_5 = 5,
};

// Only mTlvId + mState are persisted - mTargetLift is a runtime object id
// that isn't meaningful across a save/load (matches AE's LiftMover, which
// re-resolves its target LiftPoint by the TLV-derived mTargetLiftPointId
// rather than trying to carry the id itself across a restore).
struct LiftMoverSaveState final : public SaveStateBase
{
    LiftMoverSaveState()
        : SaveStateBase(ReliveTypes::eLiftMover, sizeof(*this))
    { }
    Guid mTlvId;
    LiftMoverStates mState = LiftMoverStates::eInactive_0;
};

class LiftMover final : public ::BaseGameObject
{
public:
    LiftMover(relive::Path_LiftMover* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~LiftMover();

    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManagerWrapper& resMan, BaseMap& map);

    LiftPoint* FindLiftPointWithId(s16 id);

    u16 mLiftMoverSwitchId = 0;
    s16 mTargetLiftPointId = 0;
    Guid mTlvId;
    Guid mTargetLift = {};
    FP mLiftSpeed = {};
    LiftMoverStates mState = LiftMoverStates::eInactive_0;
};

} // namespace AO
