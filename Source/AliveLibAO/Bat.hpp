#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class PathLine;
class SerializedObjectData;

namespace relive
{
    struct Path_Bat;
}

namespace AO {

class Bat final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Bat(relive::Path_Bat* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~Bat();
    
    void LoadAnimations();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

    void FlyTo(FP xpos, FP ypos, FP* xSpeed, FP* ySpeed);

    PathLine* mBatLine = nullptr;
    FP mBatSpeed = {};
    s32 mTimeBeforeMoving = 0;
    Guid mTlvInfo;
    enum class BatStates : s16
    {
        eSetTimer_0 = 0,
        eInit_1 = 1,
        eStartMoving_2 = 2,
        eFlying_3 = 3,
        eAttackTarget_4 = 4,
        eFlyAwayAndDie_5 = 5,
    };
    BatStates mBatState = BatStates::eSetTimer_0;
    s16 mAttackDuration = 0;
    s32 mTimer = 0;
    s32 mAttackDurationTimer = 0;
    FP mBatVelX = {};
    FP mEnemyXPos = {};
    FP mEnemyYPos = {};
    Guid mAttackTarget = {};

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManager& resMan, BaseMap& map);
};

struct BatSaveState final : public SaveStateBase
{
    BatSaveState()
        : SaveStateBase(ReliveTypes::eBat, sizeof(*this))
    { }
    Guid mTlvInfo;
    FP mXPos = {};
    FP mYPos = {};
    FP mVelX = {};
    FP mVelY = {};
    FP mBatVelX = {};
    s16 mCurrentPath = 0;
    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
    FP mSpriteScale = {};
    Scale mScale = Scale::Fg;
    bool bFlipX = false;
    bool mRender = true;
    Bat::BatStates mBatState = Bat::BatStates::eSetTimer_0;
    s32 mTimer = 0;
    s32 mAttackDurationTimer = 0;
    FP mEnemyXPos = {};
    FP mEnemyYPos = {};
    // The track line it is on, by its place in the lines plus 1 (a raycast from where it is can find another at a join)
    s32 mBatLineIdx = 0;
    // Who it is attacking, a stale id stays as in a live game
    Guid mAttackTarget = {};
};

} // namespace AO
