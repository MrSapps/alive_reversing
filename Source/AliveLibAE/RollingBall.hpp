#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class PathLine;

namespace relive {
struct Path_RollingBall;
}

class RollingBallShaker;

struct RollingBallSaveState final : public SaveStateBase
{
    RollingBallSaveState()
        : SaveStateBase(ReliveTypes::eRollingBall, sizeof(*this))
    { }
    s32 mState;
    Guid mRollingBallShakerId;
    FP mMaxSpeed;
    FP mAcceleration;
};

class RollingBall final : public ::BaseAliveGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const RollingBallSaveState& state);

    RollingBall(relive::Path_RollingBall* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~RollingBall();

    void LoadAnimations();

    virtual void VUpdate() override;

    void Accelerate();
    void CrushThingsInTheWay();
    bool CollideWithWalls();

    Guid mTlvInfo;
    u16 mReleaseSwitchId = 0;
    enum class States : s16
    {
        eInactive,
        eStartRolling,
        eRolling,
        eFallingAndHittingWall,
        eCrushedBees
    };
    States mState = States::eInactive;
    Guid mRollingBallShakerId;
    FP mMaxSpeed = {};
    FP mAcceleration = {};

private:
    void KillRollingBallShaker();
};
