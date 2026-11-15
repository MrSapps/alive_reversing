#include "Pilot.hpp"
#include <cstdlib>

namespace Bot {

Prim Prim::Wait(s32 frames, u32 bits)
{
    Prim p;
    p.mKind = Kind::eWait;
    p.mFrames = frames;
    p.mBits = bits;
    return p;
}

Prim Prim::MoveTo(s32 x, Gait gait)
{
    Prim p;
    p.mKind = Kind::eMoveTo;
    p.mX = x;
    p.mGait = gait;
    return p;
}

Prim Prim::RunRoll(s32 rollByX, s32 x)
{
    Prim p = MoveTo(x, Gait::eRoll);
    p.mRunUp = rollByX;
    return p;
}

Prim Prim::Face(bool left)
{
    Prim p;
    p.mKind = Kind::eFace;
    p.mFaceLeft = left;
    return p;
}

Prim Prim::Press(u32 bits, s32 maxFrames)
{
    Prim p;
    p.mKind = Kind::ePress;
    p.mBits = bits;
    p.mFrames = maxFrames;
    return p;
}

Prim Prim::WalkOff(bool left)
{
    Prim p;
    p.mKind = Kind::eWalkOff;
    p.mFaceLeft = left;
    return p;
}

Prim Prim::RunJump(s32 takeoffX, bool left)
{
    Prim p;
    p.mKind = Kind::eRunJump;
    p.mX = takeoffX;
    p.mFaceLeft = left;
    return p;
}

Prim Prim::RunWell(s32 nearSideX, bool left)
{
    Prim p;
    p.mKind = Kind::eRunWell;
    p.mX = nearSideX;
    p.mFaceLeft = left;
    return p;
}

Prim Prim::Lift(s32 y)
{
    Prim p;
    p.mKind = Kind::eLift;
    p.mX = y;
    return p;
}

std::string Prim::Text() const
{
    switch (mKind)
    {
        case Kind::eWait:
            return "wait " + std::to_string(mFrames) + (mBits ? " bits=" + std::to_string(mBits) : "");
        case Kind::eMoveTo:
            return std::string(mGait == Gait::eRun ? "run" : mGait == Gait::eSneak ? "sneak" : mGait == Gait::eRoll ? "roll" : "walk") + " to " + std::to_string(mX);
        case Kind::eFace:
            return mFaceLeft ? "face left" : "face right";
        case Kind::ePress:
            return "press " + std::to_string(mBits);
        case Kind::eWalkOff:
            return mFaceLeft ? "walk off left" : "walk off right";
        case Kind::eLift:
            return "lift to " + std::to_string(mX);
        case Kind::eRunJump:
            return std::string("run jump ") + (mFaceLeft ? "left" : "right") + " from " + std::to_string(mX);
        case Kind::eRunWell:
            return std::string("run ") + (mFaceLeft ? "left" : "right") + " into the well at " + std::to_string(mX);
    }
    return "?";
}

void Pilot::Start(const Prim& prim, const AbeState& abe)
{
    mPrim = prim;
    mFrames = 0;
    mPhase = 0;
    mStill = 0;
    mSliding = false;
    mRolled = false;
    mAired = false;
    mStuck = 0;
    mStartX = abe.mX;
    mLastX = abe.mX;
    mStartMotion = abe.mMotion;
    mWhy.clear();
}

PilotStatus Pilot::Fail(const char* why)
{
    mWhy = why;
    return PilotStatus::eFailed;
}

PilotStatus Pilot::Step(const AbeState& abe, u32& bits)
{
    bits = 0;
    if (!abe.mValid)
    {
        return Fail("no abe");
    }
    if (abe.mMove == Move::eDead)
    {
        return Fail("dead");
    }
    mFrames++;

    switch (mPrim.mKind)
    {
        case Prim::Kind::eWait:
            if (mFrames > mPrim.mFrames)
            {
                return PilotStatus::eDone;
            }
            bits = mPrim.mBits;
            return PilotStatus::eRunning;

        case Prim::Kind::eFace:
            if (abe.mTransition)
            {
                return PilotStatus::eRunning;
            }
            if (abe.mMove == Move::eIdle || abe.mMove == Move::eCrouch)
            {
                if (abe.mFacingLeft == mPrim.mFaceLeft)
                {
                    return PilotStatus::eDone;
                }
                // A tap: holding it on would start a walk
                if (mPhase == 0)
                {
                    bits = mPrim.mFaceLeft ? Btn::eLeft : Btn::eRight;
                    mPhase = 1;
                }
                else
                {
                    mPhase = 0;
                }
            }
            if (mFrames > 120)
            {
                return Fail("turn timed out");
            }
            return PilotStatus::eRunning;

        case Prim::Kind::eMoveTo:
            return StepMoveTo(abe, bits);

        case Prim::Kind::ePress:
            return StepPress(abe, bits);

        case Prim::Kind::eWalkOff:
            return StepWalkOff(abe, bits);

        case Prim::Kind::eRunJump:
            return StepRunJump(abe, bits);
        case Prim::Kind::eRunWell:
            return StepRunWell(abe, bits);

        case Prim::Kind::eLift:
            return StepLift(abe, bits);
    }
    return Fail("unknown prim");
}

PilotStatus Pilot::StepRoll(const AbeState& abe, u32& bits)
{
    const s32 grid2 = abe.Grid2();
    const s32 dx = mPrim.mX - abe.mX;
    const s32 dist2 = std::abs(dx) * 2;
    const bool left = dx < 0;
    const u32 dirBit = left ? Btn::eLeft : Btn::eRight;
    const bool there = dist2 * 2 <= grid2;

    if (mFrames > 1500)
    {
        return Fail("roll timed out");
    }
    if (abe.mX == mLastX)
    {
        mStuck++;
    }
    else
    {
        mStuck = 0;
        mLastX = abe.mX;
    }

    // The run-up: the roll button is looked for as the run reaches each cell (Abe's run loop), and he goes
    // down into the roll from there
    if (mPrim.mRunUp != 0 && !mRolled)
    {
        const s32 toRoll2 = (mPrim.mRunUp - abe.mX) * (left ? -1 : 1) * 2;
        if (abe.mMove == Move::eRoll || abe.mMove == Move::eCrouch || toRoll2 < grid2 * 2)
        {
            mRolled = true;
        }
        else if (abe.mMove == Move::eRun)
        {
            bits = dirBit | Btn::eRun;
            if (abe.mFacingLeft == left && toRoll2 <= grid2 * 3)
            {
                bits |= Btn::eFartOrRoll;
            }
            return PilotStatus::eRunning;
        }
        else if (abe.mMove == Move::eIdle || abe.mMove == Move::eWalk || abe.mMove == Move::eTurn)
        {
            if (mStuck > 40)
            {
                return Fail("blocked");
            }
            bits = dirBit | Btn::eRun;
            return PilotStatus::eRunning;
        }
        else
        {
            return PilotStatus::eRunning;
        }
    }

    switch (abe.mMove)
    {
        case Move::eIdle:
            if (there)
            {
                return PilotStatus::eDone;
            }
            // Down, as a tap (or at the low ceiling, which ducks him)
            if (mPhase == 0)
            {
                bits = mPrim.mWalkIn ? dirBit : static_cast<u32>(Btn::eDown);
            }
            mPhase = 1 - mPhase;
            return PilotStatus::eRunning;

        case Move::eCrouch:
            if (there)
            {
                return ++mStill >= 2 ? PilotStatus::eDone : PilotStatus::eRunning;
            }
            mStill = 0;
            if (mStuck > 60)
            {
                return Fail("blocked");
            }
            // Turns first if he faces the other way, then rolls
            bits = dirBit;
            return PilotStatus::eRunning;

        case Move::eRoll:
            // He stops on the next cell after the button is let go
            if (abe.mFacingLeft == left && dist2 * 4 > grid2 * 5)
            {
                bits = dirBit;
            }
            return PilotStatus::eRunning;

        case Move::eRun:
            // Already at a run that way (out of a landing with the way held): down into the roll from it
            if (abe.mFacingLeft == left && !there)
            {
                bits = dirBit | Btn::eRun | Btn::eFartOrRoll;
            }
            return PilotStatus::eRunning;

        default:
            if (mStuck > 300)
            {
                return Fail("stuck");
            }
            return PilotStatus::eRunning;
    }
}

PilotStatus Pilot::StepLift(const AbeState& abe, u32& bits)
{
    if (abe.mTransition)
    {
        return PilotStatus::eRunning;
    }
    if (mFrames > 2000)
    {
        return Fail("lift timed out");
    }
    const s32 dy = mPrim.mX - abe.mY;
    if (std::abs(dy) <= 3)
    {
        // There: he lets go of the rope by himself
        if (abe.mMove == Move::eIdle)
        {
            return ++mStill >= 2 ? PilotStatus::eDone : PilotStatus::eRunning;
        }
        return PilotStatus::eRunning;
    }
    mStill = 0;
    // Carried past it: a stop between the ends, which the lift does not stop at while the rope is pulled (Abe.cpp,
    // MoveLiftUpOrDown: the buttons are looked at on two frames of the pull, some 20 apart at 4 a frame).
    // With nothing held on one of those within 25 of the stop the lift comes to it by itself (LiftPoint.cpp,
    // eMiddleFloor). So from then on the rope is let go 20 from it. (The Mudomo vault's path 4, where he went up
    // and down past the stop at 680 for 1,500 frames.)
    const s32 side = dy < 0 ? -1 : 1;
    if (mPhase == 0 && (mLastX == 1 || mLastX == -1) && mLastX != side && std::abs(dy) < 40)
    {
        mPhase = 1;
        mStartX = 0;
    }
    mLastX = side;
    if (mPhase == 1 && std::abs(dy) <= 20)
    {
        // (stood still short of it for a second: a pull more)
        mStartX = abe.mMove == Move::eLiftIdle ? mStartX + 1 : 0;
        if (mStartX < 30)
        {
            return PilotStatus::eRunning;
        }
        mStartX = 0;
    }
    if (abe.mMove == Move::eIdle && ++mStuck > 40)
    {
        return Fail("no lift here");
    }
    bits = dy < 0 ? Btn::eUp : Btn::eDown;
    return PilotStatus::eRunning;
}

PilotStatus Pilot::StepRunWell(const AbeState& abe, u32& bits)
{
    // Both games: Abe::RunTryEnterWell, from the fifth frame of the slide he stops a run with, if up is held and he
    // is in the well's box. The run can only be stopped at two frames of its sixteen (4 and 12, in AE's
    // Motion_33_RunLoop: 50 pixels apart), and the slide then takes him on: measured, 6 pixels to where it starts,
    // 21 more by its fifth frame and 45 by its last. So the way to run is let go of before the one of those frames
    // that puts the slide over the well.
    if (abe.mTransition)
    {
        return PilotStatus::eRunning;
    }
    const s32 dir = mPrim.mFaceLeft ? -1 : 1;
    const s32 nearSide = mPrim.mX;
    // (half his size in the background: half the well, half the run, half the slide)
    const s32 half = abe.mHalfScale ? 2 : 1;
    const s32 farSide = mPrim.mX + dir * 25 / half;
    if (mPhase == 0)
    {
        if (mFrames > 300)
        {
            return Fail("never got to the well");
        }
        bool letGo = false;
        if (abe.mMove == Move::eRun && abe.mMotion == 33)
        {
            const s32 af = abe.mAnimFrame;
            const s32 frames = af <= 4 ? 4 - af : af <= 12 ? 12 - af : 20 - af;
            // Where the slide would start if the run were stopped at the next frame it can be
            const s32 start = abe.mX + dir * (624 * (frames + 1) / 100) / half;
            letGo = (start - (nearSide - dir * 42 / half)) * dir >= 0 && (start - (farSide - dir * 24 / half)) * dir <= 0;
        }
        else if ((abe.mX - (nearSide - dir * 12 / half)) * dir >= 0)
        {
            // (not in a run by the time he is there: up as he stands in it does it too)
            letGo = true;
        }
        if (letGo)
        {
            mPhase = 1;
            mFrames = 0;
            bits = Btn::eUp;
            return PilotStatus::eRunning;
        }
        bits = (mPrim.mFaceLeft ? Btn::eLeft : Btn::eRight) | Btn::eRun;
        return PilotStatus::eRunning;
    }
    if (mPhase == 1)
    {
        bits = Btn::eUp;
        if (abe.mMove == Move::eBusy || abe.mMove == Move::eAir)
        {
            mPhase = 2;
            mFrames = 0;
        }
        else if (mFrames > 60)
        {
            return Fail("slid past the well");
        }
        return PilotStatus::eRunning;
    }
    if (mFrames > 600)
    {
        return Fail("did not come out of the well");
    }
    return abe.Settled() && mFrames > 4 ? PilotStatus::eDone : PilotStatus::eRunning;
}

PilotStatus Pilot::StepRunJump(const AbeState& abe, u32& bits)
{
    if (abe.mTransition)
    {
        return PilotStatus::eRunning;
    }
    const u32 dirBit = mPrim.mFaceLeft ? Btn::eLeft : Btn::eRight;
    // How far the take-off cell is ahead
    const s32 ahead = mPrim.mFaceLeft ? abe.mX - mPrim.mX : mPrim.mX - abe.mX;
    if (mPhase == 0)
    {
        if (abe.mMove == Move::eAir)
        {
            mPhase = 1;
            mFrames = 0;
            return PilotStatus::eRunning;
        }
        if (mFrames > 200)
        {
            return Fail("no run up");
        }
        if (ahead < -abe.Grid2())
        {
            // Past it and still on the floor: stop
            return abe.Settled() ? Fail("missed the jump") : PilotStatus::eRunning;
        }
        bits = dirBit | Btn::eRun;
        // The run looks for the hop button as it reaches each cell: asked for only on the last stretch before
        // the take-off cell, so he doesn't jump a cell early
        if (abe.mMove == Move::eRun && ahead * 2 <= abe.Grid2() * 2 / 3)
        {
            bits |= Btn::eHop;
        }
        return PilotStatus::eRunning;
    }
    if (mFrames > 600)
    {
        return Fail("did not land");
    }
    if (abe.Settled())
    {
        if (++mStill >= 2)
        {
            return PilotStatus::eDone;
        }
    }
    else
    {
        mStill = 0;
    }
    return PilotStatus::eRunning;
}

PilotStatus Pilot::StepMoveTo(const AbeState& abe, u32& bits)
{
    if (abe.mTransition)
    {
        return PilotStatus::eRunning;
    }
    if (mPrim.mGait == Gait::eRoll)
    {
        return StepRoll(abe, bits);
    }

    const s32 grid2 = abe.Grid2();
    const s32 dx = mPrim.mX - abe.mX;
    const s32 dist2 = std::abs(dx) * 2;
    const bool left = dx < 0;
    const u32 dirBit = left ? Btn::eLeft : Btn::eRight;

    if (mFrames > 3000)
    {
        return Fail("move timed out");
    }

    // No progress while trying: a wall, a closed door, the end of the floor
    if (abe.mX == mLastX)
    {
        mStuck++;
    }
    else
    {
        mStuck = 0;
        mLastX = abe.mX;
    }

    switch (abe.mMove)
    {
        case Move::eIdle:
            // In the cell: half a cell either side of its middle
            if (dist2 * 2 <= grid2)
            {
                if (++mStill >= 2)
                {
                    return PilotStatus::eDone;
                }
                return PilotStatus::eRunning;
            }
            mStill = 0;
            if (mStuck > 40)
            {
                return Fail("blocked");
            }
            bits = dirBit;
            if (mPrim.mGait == Gait::eSneak)
            {
                bits |= Btn::eSneak;
            }
            else if (mPrim.mGait == Gait::eRun && mPrim.mSlide && mFrames < 30 && abe.mFacingLeft == left)
            {
                bits |= Btn::eRun;
                mSliding = true;
            }
            else if (mPrim.mGait == Gait::eRun && dist2 > mKin.mRunLeave2 * grid2 / 2 + grid2)
            {
                bits |= Btn::eRun;
            }
            return PilotStatus::eRunning;

        case Move::eCrouch:
            // Stand up first
            bits = Btn::eUp;
            return PilotStatus::eRunning;

        case Move::eTurn:
            // (with the sneak button too, or he comes out of the turn at a walk: that one loud step woke a
            // Fleech in Necrum's path 6)
            bits = dirBit | (mPrim.mGait == Gait::eSneak ? static_cast<u32>(Btn::eSneak) : 0u);
            // (and with the run button for a run that starts with the turn: he goes from the turn into the run)
            if (mPrim.mGait == Gait::eRun && mPrim.mSlide && mFrames < 30)
            {
                bits |= Btn::eRun;
                mSliding = true;
            }
            return PilotStatus::eRunning;

        case Move::eWalk:
        case Move::eSneak:
        {
            const bool movingLeft = abe.mFacingLeft;
            const s32 stop2 = (abe.mMove == Move::eSneak ? mKin.mSneakStop2 : mKin.mWalkStop2) * grid2 / 2;
            if (movingLeft == left && dist2 > stop2 + grid2 / 4)
            {
                bits = dirBit;
                if (mPrim.mGait == Gait::eSneak)
                {
                    bits |= Btn::eSneak;
                }
                else if (mPrim.mGait == Gait::eRun && dist2 > mKin.mRunLeave2 * grid2 / 2 + grid2)
                {
                    bits |= Btn::eRun;
                }
            }
            return PilotStatus::eRunning;
        }

        case Move::eRun:
        {
            const bool movingLeft = abe.mFacingLeft;
            if (mSliding)
            {
                // (held until a little under four cells are left: let go of then, the slide starts two cells
                // short of where he is to stand. Measured: let go of 3.5 cells short it does, 2.8 short he runs
                // two cells further.)
                if (movingLeft == left && std::abs(dx) * 10 > 36 * (grid2 / 2))
                {
                    bits = dirBit | Btn::eRun;
                }
                return PilotStatus::eRunning;
            }
            if (movingLeft == left && dist2 > mKin.mRunLeave2 * grid2 / 2)
            {
                bits = dirBit | Btn::eRun;
            }
            else if (movingLeft == left && dist2 > mKin.mWalkStop2 * grid2 / 2 + grid2 / 4)
            {
                // Down to a walk for the last cells
                bits = dirBit;
            }
            return PilotStatus::eRunning;
        }

        default:
            if (mStuck > 300)
            {
                return Fail("stuck");
            }
            return PilotStatus::eRunning;
    }
}

PilotStatus Pilot::StepWalkOff(const AbeState& abe, u32& bits)
{
    if (abe.mTransition)
    {
        return PilotStatus::eRunning;
    }
    if (mPrim.mRunOff > 0)
    {
        // (mStill: in the air the frame before; mPhase: the falls landed from)
        bits = (mPrim.mFaceLeft ? Btn::eLeft : Btn::eRight) | Btn::eRun;
        const bool air = abe.mMove == Move::eAir || !abe.mOnGround;
        if (air)
        {
            mStill = 1;
        }
        else if (mStill)
        {
            mStill = 0;
            mPhase++;
        }
        if (mPhase >= mPrim.mRunOff)
        {
            return PilotStatus::eDone;
        }
        if (mFrames > 300)
        {
            return Fail("did not run off");
        }
        return PilotStatus::eRunning;
    }
    if (mPhase == 0)
    {
        if (abe.mMove == Move::eAir || !abe.mOnGround)
        {
            mPhase = 1;
            mFrames = 0;
            return PilotStatus::eRunning;
        }
        if (abe.mX == mLastX)
        {
            if (++mStuck > 40)
            {
                return Fail("blocked");
            }
        }
        else
        {
            mStuck = 0;
            mLastX = abe.mX;
        }
        if (std::abs(abe.mX - mStartX) > 60 || mFrames > 200)
        {
            return Fail("no edge to walk off");
        }
        bits = mPrim.mFaceLeft ? Btn::eLeft : Btn::eRight;
        return PilotStatus::eRunning;
    }
    if (mFrames > 600)
    {
        return Fail("did not land");
    }
    if (LandsInto(abe, bits))
    {
        return PilotStatus::eDone;
    }
    if (abe.Settled())
    {
        if (++mStill >= 2)
        {
            return PilotStatus::eDone;
        }
    }
    else
    {
        mStill = 0;
    }
    return PilotStatus::eRunning;
}

// Landing from a fall or a well with the next move's buttons held: true once he is off into that move
bool Pilot::LandsInto(const AbeState& abe, u32& bits)
{
    if (mPrim.mAfter == 0)
    {
        return false;
    }
    if (!abe.mOnGround)
    {
        mAired = true;
        return false;
    }
    if (!mAired)
    {
        return false;
    }
    bits = mPrim.mAfter;
    return abe.mMove == Move::eRun || abe.mMove == Move::eWalk;
}

PilotStatus Pilot::StepPress(const AbeState& abe, u32& bits)
{
    if (mPhase == 0)
    {
        if (abe.mTransition)
        {
            return PilotStatus::eRunning;
        }
        if (abe.mMotion != mStartMotion)
        {
            mPhase = 1;
            mFrames = 0;
        }
        else if (mFrames > mPrim.mFrames)
        {
            return Fail("no reaction");
        }
        else
        {
            bits = mPrim.mBits;
            return PilotStatus::eRunning;
        }
    }

    if (mFrames > 1500)
    {
        return Fail("did not settle");
    }
    if (!abe.mTransition && LandsInto(abe, bits))
    {
        return PilotStatus::eDone;
    }
    if (!abe.mTransition && abe.Settled())
    {
        if (++mStill >= 2)
        {
            return PilotStatus::eDone;
        }
    }
    else
    {
        mStill = 0;
    }
    return PilotStatus::eRunning;
}

} // namespace Bot
