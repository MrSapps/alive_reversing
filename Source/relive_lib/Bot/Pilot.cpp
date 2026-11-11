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
    }
    return "?";
}

void Pilot::Start(const Prim& prim, const AbeState& abe)
{
    mPrim = prim;
    mFrames = 0;
    mPhase = 0;
    mStill = 0;
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

    switch (abe.mMove)
    {
        case Move::eIdle:
            if (there)
            {
                return PilotStatus::eDone;
            }
            // Down, as a tap
            if (mPhase == 0)
            {
                bits = Btn::eDown;
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
    if (abe.mMove == Move::eIdle && ++mStuck > 40)
    {
        return Fail("no lift here");
    }
    bits = dy < 0 ? Btn::eUp : Btn::eDown;
    return PilotStatus::eRunning;
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
            bits = dirBit;
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
