#pragma once

#include "GameView.hpp"
#include <string>

namespace Bot {

enum class Gait : u8
{
    eWalk,
    eRun,
    eSneak,
    eRoll, // crouched: ends crouched
};

// One thing for Abe to do, small enough to be steered by watching what he does
struct Prim final
{
    enum class Kind : u8
    {
        eWait,   // mFrames frames with mBits held
        eMoveTo, // end standing in the grid cell of mX, by mGait
        eFace,   // turn to mFaceLeft
        ePress,  // hold mBits until Abe starts something (at most mFrames frames), then let it play out
        eWalkOff, // walk off the end of the floor to mFaceLeft and land
        eLift,    // on a lift: ride it to the stop at height mX
        eRunJump, // run to mFaceLeft and jump from the cell of mX, until Abe lands or hangs from a ledge
    };

    Kind mKind = Kind::eWait;
    s32 mX = 0;
    Gait mGait = Gait::eWalk;
    bool mFaceLeft = false;
    u32 mBits = 0;
    s32 mFrames = 0;

    static Prim Wait(s32 frames, u32 bits = 0);
    static Prim MoveTo(s32 x, Gait gait = Gait::eWalk);
    static Prim Face(bool left);
    static Prim Press(u32 bits, s32 maxFrames = 12);
    static Prim WalkOff(bool left);
    static Prim RunJump(s32 takeoffX, bool left);
    static Prim Lift(s32 y);
    std::string Text() const;
};

enum class PilotStatus : u8
{
    eRunning,
    eDone,
    eFailed,
};

// How far Abe goes on after the button is let go, per game: measured, see BOT_DESIGN.md
struct Kinematics final
{
    // From the walk loop's last decision frame to standing still, in grid cells x2
    s32 mWalkStop2 = 2;
    s32 mSneakStop2 = 2;
    // A run is left for a walk this many cells x2 before the target
    s32 mRunLeave2 = 8;
};

// Turns a Prim into pad buttons, a frame at a time
class Pilot final
{
public:
    void Start(const Prim& prim, const AbeState& abe);
    // The buttons to hold for the next frame
    PilotStatus Step(const AbeState& abe, u32& bits);
    const std::string& Why() const
    {
        return mWhy;
    }
    const Prim& Current() const
    {
        return mPrim;
    }

    Kinematics mKin;

private:
    PilotStatus Fail(const char* why);
    PilotStatus StepMoveTo(const AbeState& abe, u32& bits);
    PilotStatus StepPress(const AbeState& abe, u32& bits);
    PilotStatus StepWalkOff(const AbeState& abe, u32& bits);
    PilotStatus StepRoll(const AbeState& abe, u32& bits);
    PilotStatus StepRunJump(const AbeState& abe, u32& bits);
    PilotStatus StepLift(const AbeState& abe, u32& bits);

    Prim mPrim;
    s32 mFrames = 0;
    s32 mPhase = 0;
    s32 mStill = 0;
    s32 mStartX = 0;
    s16 mStartMotion = 0;
    s32 mLastX = 0;
    s32 mStuck = 0;
    std::string mWhy;
};

} // namespace Bot
