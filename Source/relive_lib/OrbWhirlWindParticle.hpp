#pragma once

#include "../relive_lib/Animation.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/data_conversion/guid.hpp"

class BaseGameObject;
class BaseMap;

// What a particle keeps; the object it flies to is known by id
struct OrbWhirlWindParticleState final
{
    AnimationSaveState mAnim;
    Guid mRingTargetId;
    FP mXPosRenderOffset;
    FP mYPosRenderOffset;
    FP mRenderAsScale;
    FP mRadiusOffsetX;
    FP mYPosIncrement;
    FP mCurrentScale;
    FP mRandomScale;
    FP mScaleOffsetFlyToTarget;
    FP mXPosMid;
    FP mYPosMid;
    FP mRadiusX;
    FP mRadiusY;
    FP mMoveY;
    FP mXPos;
    FP mYPos;
    FP mTargetObjScale;
    FP mXPosOffset;
    FP mYPosOffset;
    FP mXPosOffset2;
    FP mYPosOffset2;
    FP mScaleOffsetSpinAtTarget;
    s32 mState;
    s32 mRenderAngle;
    s32 mPositionTimer;
    s16 mCounter;
    s16 mMaxCounter;
    bool mIsActive;
};

class OrbWhirlWindParticle final
{
public:
    // For a save: made with none of the random values, they are in the save
    struct RestoreTag final
    { };
    OrbWhirlWindParticle(RestoreTag, ResourceManager& resMan, BaseGameObject* pOwner);

    OrbWhirlWindParticleState GetState() const;
    void SetState(const OrbWhirlWindParticleState& state);
    // The object it flies to may be made after it
    void ResolveTarget();

    // pOwner is what the animation is known by, in the order the animations are in
    OrbWhirlWindParticle(FP xpos, FP ypos, FP scale, bool isMudokonSpirit, ResourceManager& resMan, BaseMap& map, BaseGameObject* pOwner);
    ~OrbWhirlWindParticle();

    s32 IsActive();
    void Spin(FP xpos, FP ypos, FP scale, BaseGameObject* pObj);
    void ToStop();
    void Update();
    void Render(OrderingTable& ot);

private:
    void CalculateRenderProperties(s16 bStarted);

private:
    bool mIsActive = false;
    Animation mAnim;
    FP mXPosRenderOffset = {};
    FP mYPosRenderOffset = {};
    FP mRenderAsScale = {};
    FP mRadiusOffsetX = {};
    FP mYPosIncrement = {};

    enum class State : s16
    {
        eStart = 0,
        eSpin = 1,
        eFlyToTarget = 2,
        eSpinAtTarget = 3,
        eStop = 4,
    };
    State mState = State::eStart;

    s32 mRenderAngle = 0;
    s16 mCounter = 0;
    s16 mMaxCounter = 0;
    FP mCurrentScale = {};
    FP mRandomScale = {};
    FP mScaleOffsetFlyToTarget = {};
    FP mXPosMid = {};
    FP mYPosMid = {};
    FP mRadiusX = {};
    FP mRadiusY = {};
    s32 mPositionTimer = 0;
    FP mMoveY = {};
    BaseGameObject* mRingTargetObjId = nullptr;
    // Only between a restore and ResolveTarget
    Guid mRestoredRingTargetId;
    FP mXPos = {};
    FP mYPos = {};
    FP mTargetObjScale = {};
    FP mXPosOffset = {};
    FP mYPosOffset = {};
    FP mXPosOffset2 = {};
    FP mYPosOffset2 = {};
    FP mScaleOffsetSpinAtTarget = {};
};
