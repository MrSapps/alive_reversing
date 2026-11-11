#pragma once

#include "../Types.hpp"
#include <vector>

namespace Bot {

enum class EntityKind : u8
{
    eLever,
    ePullRing,
    eWorkWheel,
    eMudokon,
    eBirdPortal,
    // Sets its switch when all its inputs are on (a multi switch controller, a wheel syncer)
    eSwitchLogic,
    eSlig,
    eSecurityOrb,
    // Sets its switch when Abe looks into it
    eHandStone,
    // Laughing gas fills its camera while its switch is on: Mudokons there go wired
    eLaughingGas,
    // Takes whoever is controlled and stands in it to another one when its switch changes. mInputs: the camera
    // it leads to, its own id, the id of the one it leads to there
    eTeleporter,
    // A mine on the floor: a rock sets it off. mSwitch is the switch the world gives as on once it is gone
    eMine,
    // Rocks come out of it when Abe jumps into it
    eRockSack,
    // A drill: it sweeps its TLV's rectangle and kills what it meets. mSwitch stops or starts it; mInputs: how it
    // takes the switch (0 not at all, 1 runs while it is active, 2 one stroke each time it is), whether it starts
    // on, whether it still runs (slower) when off
    eDrill,
    // A mine car: Abe gets in with up, and it runs along its own lines (floors, walls, ceilings) over whatever
    // is in the way. mLiveX, mLiveY: where it is now. mState: 1 while Abe is in it
    eMineCar,
};

const char* EntityKindName(EntityKind kind);

enum class MudMood : u8
{
    eNormal,
    eUpset, // sad or angry: sorry puts it right
    eWired, // laughing gas: a slap puts it right
    eSick,
};

// Something in the path the bot acts on or has to mind. The first part is from the path's TLVs and never changes,
// the second is how it is now.
struct Entity final
{
    EntityKind mKind = EntityKind::eLever;
    // The same from run to run: camera number x 1000 + its place in the camera's TLVs
    u32 mId = 0;
    s32 mCamera = 0;
    // The TLV's rectangle
    s32 mX1 = 0;
    s32 mY1 = 0;
    s32 mX2 = 0;
    s32 mY2 = 0;
    bool mHalf = false;
    // What it sets (a lever, a ring, a wheel, logic), or the switch a rescued Mudokon sets
    s32 mSwitch = 0;
    // 0 sets it, 1 clears it, 2 toggles it
    s32 mOp = 0;
    s32 mInputs[6] = {};
    // A wheel that only counts while it is being turned
    bool mHoldOnly = false;
    // A portal Mudokons can be sent through
    bool mRescuePortal = false;
    // A Mudokon that can't see: told to follow it walks towards Abe and on past him, until it is told to wait,
    // walks into a wall or falls
    bool mBlind = false;

    // Its TLV says it is gone for good: a Mudokon saved or dead
    bool mGone = false;
    // There is an object for it now, at mLiveX, mLiveY
    bool mLive = false;
    s32 mLiveX = 0;
    s32 mLiveY = 0;
    // The top of what can be hit of it now: low when it ducks
    s32 mLiveTop = 0;
    bool mFollowing = false;
    s32 mState = 0;
    // A live Mudokon's mood
    MudMood mMood = MudMood::eNormal;

    s32 CentreX() const
    {
        return (mX1 + mX2) / 2;
    }
};

// Where a lift's platform is now
struct LiftAt final
{
    s32 mX1 = 0;
    s32 mX2 = 0;
    s32 mY = 0;
};

// A Slig that is about
struct SligAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mFacingLeft = false;
    bool mHalf = false;
    bool mAsleep = false;
    bool mPossessed = false;
    // The game's motion number, for the trace
    s32 mMotion = 0;
    // False for one known only from the path's data: its camera isn't loaded, so it is where it starts
    bool mSeen = true;
};

// A Slog that is about. Awake it runs Abe down wherever he is.
struct SlogAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mAsleep = false;
};

// How far a Slig has been seen to walk on its floor
struct SligBeat final
{
    s32 mY = 0;
    bool mHalf = false;
    s32 mMinX = 0;
    s32 mMaxX = 0;
};

// A UXB that is about
struct UxbAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mHalf = false;
    // A slap now turns it off
    bool mGreen = false;
    bool mOff = false;
};

// Something that goes to and fro and kills what it meets on the way (a drill), as it is now. It is parked at one
// end for a while, goes out to the other and comes straight back.
struct SweepAt final
{
    // The entity it is
    u32 mId = 0;
    // What it kills in now
    s32 mX1 = 0;
    s32 mY1 = 0;
    s32 mX2 = 0;
    s32 mY2 = 0;
    bool mHalf = false;
    // It moves along y, not x; and the way its rectangle goes as the offset grows
    bool mVertical = false;
    s32 mSign = 1;
    // All x256: how far it is from the far end, where it parks, and what it moves a frame
    s32 mOffset = 0;
    s32 mTravel = 0;
    s32 mSpeed = 0;
    // 0 parked, 1 going out (the offset shrinks), 2 coming back
    s32 mPhase = 0;
    // Parked: frames before it can start, and whether it will at all as the switches are
    s32 mPause = 0;
    bool mWillStart = true;

    // Where it kills t frames from now, starting as soon as it may. False if it is parked then.
    bool At(s32 t, s32& x1, s32& y1, s32& x2, s32& y2) const
    {
        s32 phase = mPhase;
        s32 offset = mOffset;
        s32 pause = mPause;
        for (s32 i = 0; i < t; i++)
        {
            if (phase == 0)
            {
                if (pause > 0)
                {
                    pause--;
                }
                else if (mWillStart && mSpeed > 0)
                {
                    phase = 1;
                }
            }
            else if (phase == 1)
            {
                offset -= mSpeed;
                phase = offset <= 0 ? 2 : 1;
            }
            else
            {
                offset += mSpeed;
                phase = offset >= mTravel ? 0 : 2;
            }
        }
        const s32 shift = mSign * ((offset - mOffset) / 256);
        x1 = mX1 + (mVertical ? 0 : shift);
        x2 = mX2 + (mVertical ? 0 : shift);
        y1 = mY1 + (mVertical ? shift : 0);
        y2 = mY2 + (mVertical ? shift : 0);
        return phase != 0;
    }
    // Whether it could kill in a box at any time from frame t1 to t2
    bool Hits(s32 t1, s32 t2, s32 x1, s32 y1, s32 x2, s32 y2) const
    {
        for (s32 t = t1 < 0 ? 0 : t1; t <= t2; t++)
        {
            s32 ax = 0;
            s32 ay = 0;
            s32 bx = 0;
            s32 by = 0;
            if (At(t, ax, ay, bx, by) && ax <= x2 && bx >= x1 && ay <= y2 && by >= y1)
            {
                return true;
            }
        }
        return false;
    }
};

// A rock lying on the floor, to be picked up
struct RockAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mHalf = false;
};

struct World final
{
    // Rocks Abe has on him, and those lying about
    s32 mRocks = 0;
    std::vector<RockAt> mRocksDown;
    // The switch that stands for the hazard with this id (a mine's TLV) being gone: above the game's own 256
    static s32 HazardSwitch(u32 id)
    {
        return 1000 + static_cast<s32>(id);
    }
    std::vector<UxbAt> mUxbs;
    std::vector<SweepAt> mSweeps;
    // Whether a drill runs as the switches are now: known from its TLV, so for one that isn't about too
    bool SweepRuns(const Entity& drill) const
    {
        if (drill.mInputs[0] == 0)
        {
            return true;
        }
        const bool active = (Switch(drill.mSwitch) != 0) != (drill.mInputs[1] != 0);
        return active || (drill.mInputs[0] == 1 && drill.mInputs[2] != 0);
    }
    // The drill that reaches Abe standing at a place and runs as things are (or is still on its last stroke), or
    // null. Its width is not in the path's data: 20 either side of its rectangle covers what was measured.
    // crouched: as he is under a low ceiling or a drill that goes over his head
    const Entity* SweepOver(s32 x, s32 y, bool half, bool crouched = false) const
    {
        const s32 tall = (half ? 30 : 60) / (crouched ? 2 : 1);
        for (const Entity& e : mEntities)
        {
            const s32 reach = e.mX2 - e.mX1 > e.mY2 - e.mY1 ? 20 : 12;
            if (e.mKind != EntityKind::eDrill || e.mHalf != half || x < e.mX1 - reach || x > e.mX2 + reach || y - tall > e.mY2 || y < e.mY1)
            {
                continue;
            }
            const SweepAt* live = SweepOf(e.mId);
            if (SweepRuns(e) || (live && live->mPhase != 0))
            {
                return &e;
            }
        }
        return nullptr;
    }
    // Whether a drill is moving or will be: as its switch is, or still on its last stroke
    bool SweepLive(const Entity& drill) const
    {
        const SweepAt* live = SweepOf(drill.mId);
        return SweepRuns(drill) || (live && live->mPhase != 0);
    }
    const SweepAt* SweepOf(u32 id) const
    {
        for (const SweepAt& s : mSweeps)
        {
            if (s.mId == id)
            {
                return &s;
            }
        }
        return nullptr;
    }
    // Frames of quiet left for talking at (x, y): none while a Slig that walks that floor is on Abe's screen (it
    // turns to a sound it hears there) or faces that way (it shoots at what a Mudokon says, from any distance);
    // else until it gets to the end of its beat and turns. 0 as well when its beat isn't known yet; a big
    // number with no Slig about. screenX1..screenX2: the screen Abe is on.
    static s32 QuietFrames(const std::vector<SligAt>& sligs, const std::vector<SligBeat>& beats, s32 x, s32 y, bool half, s32 screenX1, s32 screenX2)
    {
        s32 quiet = 100000;
        for (const SligAt& slig : sligs)
        {
            const s32 dy = slig.mY - y;
            const s32 dx = slig.mX - x;
            if (slig.mAsleep || slig.mPossessed || slig.mHalf != half || dy > 45 || dy < -45 || dx > 1500 || dx < -1500)
            {
                continue;
            }
            if (dx == 0 || slig.mFacingLeft == (dx > 0) || (slig.mX >= screenX1 && slig.mX < screenX2))
            {
                return 0;
            }
            s32 left = 0;
            for (const SligBeat& beat : beats)
            {
                const s32 by = beat.mY - slig.mY;
                if (beat.mHalf == slig.mHalf && by <= 45 && by >= -45 && slig.mX >= beat.mMinX - 30 && slig.mX <= beat.mMaxX + 30 && beat.mMaxX - beat.mMinX >= 150)
                {
                    // (measured: a beat of 550 pixels takes it 330 frames, so two pixels a frame is on the safe side)
                    left = (slig.mFacingLeft ? slig.mX - beat.mMinX : beat.mMaxX - slig.mX) / 2;
                }
            }
            quiet = left < quiet ? left : quiet;
        }
        return quiet;
    }
    std::vector<SlogAt> mSlogs;
    std::vector<LiftAt> mLifts;
    // Where each lift was when it was last about
    std::vector<LiftAt> mLiftsSeen;
    // Whether a lift is at one of its stops (a floor at y that x is on). One that isn't about, its screen not
    // being loaded, is where it was last seen, or failing that where it starts.
    bool LiftAtStop(s32 x, s32 y, bool startsHere) const
    {
        for (const std::vector<LiftAt>* lifts : {&mLifts, &mLiftsSeen})
        {
            for (const LiftAt& lift : *lifts)
            {
                if (x >= lift.mX1 && x <= lift.mX2)
                {
                    return std::abs(lift.mY - y) <= 4;
                }
            }
        }
        return startsHere;
    }
    std::vector<SligAt> mSligs;
    std::vector<Entity> mEntities;
    // The TLV of each entity, for the reader of the game only
    std::vector<const void*> mTlvs;
    // The path they are from
    const void* mPath = nullptr;
    s8 mSwitches[256] = {};
    s32 mRescued = 0;
    s32 mKilled = 0;

    s32 Switch(s32 id) const
    {
        if (id >= 256)
        {
            for (const Entity& e : mEntities)
            {
                if (e.mKind == EntityKind::eMine && e.mSwitch == id)
                {
                    return e.mGone ? 1 : 0;
                }
            }
            return 0;
        }
        return id >= 0 ? mSwitches[id] : 0;
    }
    // The Sligs to mind: the ones about, and the ones that will be when their camera loads
    std::vector<SligAt> Threats() const
    {
        std::vector<SligAt> out = mSligs;
        for (const Entity& e : mEntities)
        {
            if (e.mKind != EntityKind::eSlig || e.mGone)
            {
                continue;
            }
            // One from a spawner is only there with its switch on
            if (e.mSwitch > 0 && !Switch(e.mSwitch))
            {
                continue;
            }
            bool seen = false;
            for (const SligAt& s : mSligs)
            {
                const s32 dx = s.mX - e.CentreX();
                const s32 dy = s.mY - e.mY2;
                seen = seen || (dx > -450 && dx < 450 && dy > -80 && dy < 80);
            }
            if (!seen)
            {
                SligAt s;
                s.mX = e.CentreX();
                s.mY = e.mY2;
                s.mHalf = e.mHalf;
                s.mAsleep = e.mState == 1;
                s.mFacingLeft = e.mOp == 1;
                s.mSeen = false;
                out.push_back(s);
            }
        }
        return out;
    }
    const Entity* Find(u32 id) const
    {
        for (const Entity& e : mEntities)
        {
            if (e.mId == id)
            {
                return &e;
            }
        }
        return nullptr;
    }
};

} // namespace Bot
