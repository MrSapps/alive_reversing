#pragma once

#include "../relive_lib/GameObjects/BaseAliveGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class PathLine;
class SerializedObjectData;
enum eLineTypes : s16;

namespace relive
{
    struct Path_RollingBall;
}

class RollingBallShaker;

namespace AO {

struct RollingBallSaveState final : public SaveStateBase
{
    RollingBallSaveState()
        : SaveStateBase(ReliveTypes::eRollingBall, sizeof(*this))
    { }
    FP mXPos = {};
    FP mYPos = {};
    FP mVelX = {};
    FP mVelY = {};
    s16 mCurrentPath = 0;
    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
    FP mSpriteScale = {};
    Scale mScale = Scale::Fg;
    bool bFlipX = false;
    u16 mLastLineYPos = 0;
    eLineTypes mCollisionLineType = eLineTypes::eNone_m1;
    Guid mTlvInfo;
    u16 mReleaseSwitchId = 0;
    s16 mState = 0;
    Guid mRollingBallShakerId;
    FP mMaxSpeed = {};
    FP mAcceleration = {};
};

class RollingBall final : public ::BaseAliveGameObject
{
public:
    RollingBall(relive::Path_RollingBall* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~RollingBall();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

    void Accelerate();
    void CrushThingsInTheWay();

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


} // namespace AO
