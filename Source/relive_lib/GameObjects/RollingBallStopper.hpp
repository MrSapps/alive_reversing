#pragma once

#include "BaseAliveGameObject.hpp"
#include "../SaveStateBase.hpp"

class PathLine;

namespace relive
{
    struct Path_RollingBallStopper;
}

struct RollingBallStopperSaveState final : public SaveStateBase
{
    RollingBallStopperSaveState()
        : SaveStateBase(ReliveTypes::eRollingBallStopper, sizeof(*this))
    { }
    s32 mState;
};

class RollingBallStopper final : public ::BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const RollingBallStopperSaveState& state);

    RollingBallStopper(relive::Path_RollingBallStopper* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~RollingBallStopper();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    Guid mTlvInfo;
    enum class States : s16
    {
        eWaitForTrigger,
        eMoveStopper,
        eMovingDone
    };
    States mState = States::eWaitForTrigger;
    u16 mStopperSwitchId = 0;
    u16 mBallSwitchId = 0;
    PathLine* mCollisionLine = nullptr;
};
