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
        eRunWell, // run to mFaceLeft at the well whose near side is at mX and into it without stopping: the run is
                  // let go a few pixels short and up held, which takes him in as he slides over it
    };

    Kind mKind = Kind::eWait;
    s32 mX = 0;
    Gait mGait = Gait::eWalk;
    bool mFaceLeft = false;
    u32 mBits = 0;
    s32 mFrames = 0;
    // A move by rolling that is run up to: the cell the roll has to have begun by (0: none). He goes from the
    // run into the roll without stopping.
    s32 mRunUp = 0;
    // A run from a stand that ends in a slide: let go of, he slides two cells from the next cell but one and
    // stands (measured: four cells in 27 frames, six in 35, against 40 and more walked). For a number of cells
    // that is even and at least four, with nothing to wait for on the way.
    bool mSlide = false;
    // A fall or a well: the buttons of the move after it, held as he lands. Both games go from the landing
    // straight into a walk or a run when a direction is held as his feet touch, and skip the rest of it.
    u32 mAfter = 0;
    // A walk off the end of the floor that is a run through this many falls without a stop, done as he lands
    // from the last of them still running (0: a walk off one edge). With something after him on the floor he
    // comes down on: measured in the Mudomo vaults' path 7, from 1187,1400 down two ledges to 1345,1520 in 34
    // frames, against 100 and more a ledge at a time.
    s32 mRunOff = 0;
    // A roll that ends in a run: the run button again once he is past the last cell of it, so that the roll
    // comes up into the run at its end, and done as he runs (no stop to crouch and stand up)
    bool mRunOut = false;
    // A running jump: where the next ones take off, on at a run from each landing (0: none)
    s32 mX2 = 0;
    s32 mX3 = 0;
    // A roll from where down is the way to climb down a hoist: he walks at the low ceiling instead, and both
    // games duck him under it (a wall at his head and none at his knees, where his walk starts)
    bool mWalkIn = false;

    static Prim Wait(s32 frames, u32 bits = 0);
    static Prim MoveTo(s32 x, Gait gait = Gait::eWalk);
    static Prim RunRoll(s32 rollByX, s32 x);
    static Prim Face(bool left);
    static Prim Press(u32 bits, s32 maxFrames = 12);
    static Prim WalkOff(bool left);
    static Prim RunJump(s32 takeoffX, bool left);
    static Prim RunWell(s32 nearSideX, bool left);
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
    bool LandsInto(const AbeState& abe, u32& bits);
    PilotStatus StepRunJump(const AbeState& abe, u32& bits);
    PilotStatus StepRunWell(const AbeState& abe, u32& bits);
    PilotStatus StepLift(const AbeState& abe, u32& bits);

    Prim mPrim;
    s32 mFrames = 0;
    s32 mPhase = 0;
    s32 mStill = 0;
    bool mSliding = false;
    bool mRolled = false;
    // Seen off the ground in this prim
    bool mAired = false;
    s32 mStartX = 0;
    s16 mStartMotion = 0;
    s32 mLastX = 0;
    s32 mStuck = 0;
    std::string mWhy;
};

} // namespace Bot
