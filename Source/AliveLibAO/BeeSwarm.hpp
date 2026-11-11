#pragma once

#include "../relive_lib/Function.hpp"
#include "../relive_lib/WorldStatic.hpp"
#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "Map.hpp"
#include "../relive_lib/AnimationUnknown.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class PathLine;
class BaseAliveGameObject;

namespace AO {

struct BeeSwarmParticle final
{
    FP mXPos;
    FP mYPos;
    s8 field_8_angle;
    s8 field_9_angle_speed;
    s32 mTimer;
    AnimationUnknown field_10_anim;
};
ALIVE_ASSERT_SIZEOF(BeeSwarmParticle, 0x80);


struct BeeSwarmParticles final
{
    BeeSwarmParticle bees[25];
};

struct BeeSwarmSaveState final : public SaveStateBase
{
    BeeSwarmSaveState()
        : SaveStateBase(ReliveTypes::eBeeSwarm, sizeof(*this))
    { }
    s16 mMaxBeesCount;
    s16 mCurrentBeesCount;
    FP mSpawnXPos;
    FP mSpawnYPos;
    FP mChaseTargetX;
    FP mChaseTargetY;
    FP mSpeed;
    FP mPosOffset;
    s16 mSwarmState;
    s32 mTotalChaseTime;
    FP mRectX;
    FP mRectY;
    FP mRectW;
    FP mRectH;
    Guid mChaseTarget;
    s32 mAliveTimer;
    s32 mDamageOrPainSoundTimer;
    s32 mUpdateChaseTimer;
    // The track line it follows, by its place in the lines plus 1
    s32 mLineIdx;
    FP mLineFollowSpeed;
    struct Bee final
    {
        FP mXPos;
        FP mYPos;
        s8 mAngle;
        s8 mAngleSpeed;
        s32 mTimer;
        AnimationSaveState mAnim;
    };
    Bee mBees[25];
};

class BeeSwarm final : public BaseAnimatedWithPhysicsGameObject
{
public:
    // For a save: made with none counted, the count comes back with the rest of the variables
    struct RestoreTag final
    { };
    BeeSwarm(RestoreTag, ResourceManager& resMan, BaseMap& map);

    BeeSwarm(FP xpos, FP ypos, FP speed, s32 numBees, s32 totalChaseTime, ResourceManager& resMan, BaseMap& map);
    ~BeeSwarm();

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VRender(OrderingTable& ot) override;

    void FollowLine(PathLine* pLine, FP target_x, FP target_y, FP speed);
    void Chase(::BaseAliveGameObject* pChaseTarget);
    void ToFlyAwayAndDie();

    BeeSwarmParticles field_E4_bees = {};
    s16 mMaxBeesCount = 0;
    s16 mCurrentBeesCount = 0;
    FP field_D68_xpos = {};
    FP field_D6C_ypos = {};
    FP mChaseTargetX = {};
    FP mChaseTargetY = {};
    FP field_D78_speed = {};
    FP field_D7C_pos_offset = {};

    enum class BeeSwarmStates : s16
    {
        eIdle_0 = 0,
        eAttackChase_1 = 1,
        eFollowPathLines_2 = 2,
        eFlyAwayAndDie_3 = 3,
    };
    BeeSwarmStates mSwarmState = BeeSwarmStates::eIdle_0;

    s32 mTotalChaseTime = 0;

    // TODO: Might be a rect
    FP mRectX = {};
    FP mRectY = {};
    FP mRectW = {};
    FP mRectH = {};

    Guid mChaseTarget = {};
    s32 mAliveTimer = 0;
    s32 field_DA0_do_damage_or_pain_sound_timer = 0;
    s32 field_DA4_update_chase_timer = 0;
    PathLine* mLine = nullptr;
    FP mLineFollowSpeed = {};
};

extern WorldStatic<s16> gBeeInstanceCount;
extern WorldStatic<s16> gBeesNearAbe;

} // namespace AO
