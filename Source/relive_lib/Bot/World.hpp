#pragma once

#include <algorithm>

#include "../Types.hpp"
#include <map>
#include <set>
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
    // A Fleech where the map puts it: mSwitch is the switch that wakes it (0: none)
    eFleech,
    // A Scrab where the map puts it: it goes for whoever comes onto its floor
    eScrab,
    // A switch in the floor that whoever steps on it sets (mOp: 0 on, 1 off, 2 toggles)
    eFootSwitch,
    // A lock with a ghost in it: slapped, the ghost flies off and its switch comes on when it gets where it is going
    eSlapLock,
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
    // A Paramite where the map puts it. mInputs: how it comes (0 walks its floor, 1 and 2 down a web when its
    // switch comes on), that switch, whether it hisses before it goes for him
    eParamite,
    // A SoulStorm Brew machine: up at it and he drinks one, and the next fart is one that goes off (and can be
    // flown, FartFlight). mInputs: how many Brews it has to start with
    eBrew,
    // A flying Slig where the map puts it (FlierAt is one that is about)
    eFlier,
    // A lock that opens for a Slig that says its code back to it (AE's security door). mSwitch is what it sets.
    // World::mLockCodes has the code. mState, while it is about: 0 idle, 1 waits for "Hi", 2 says its code,
    // 3 waits for the code, 4 open, 5 said no
    eSligLock,
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
    // The ends of its beat, from the bounds set for it (both 0 when not known)
    s32 mBeatMin = 0;
    s32 mBeatMax = 0;
    // The frames at least that it stands at an end of its beat before it turns (AO's; 0 when not known)
    s32 mBeatPause = 0;
    // Asleep, a noise this near (in x) wakes it; 0 when not known: anywhere on its screen then
    s32 mWake = 0;
    // The frames it waits before it shoots at what it has seen
    s32 mSightDelay = 0;
    // The frames at least until it could shoot at something at x on its floor, with the screen that is on going from
    // camX1 to camX2: it has to be on that screen and face that way. 0 when its beat isn't known or x is on it.
    s32 FramesUntilSees(s32 x, s32 camX1, s32 camX2) const
    {
        if (mBeatMax <= mBeatMin || (x >= mBeatMin && x <= mBeatMax))
        {
            return 0;
        }
        const bool right = x > mBeatMax;
        // Where it first is on that screen, walking that way
        const s32 from = right ? std::max(camX1, mBeatMin) : std::min(camX2, mBeatMax);
        s32 walk = 0;
        if (right)
        {
            walk = mFacingLeft ? (mX - mBeatMin) + std::max(0, from - mBeatMin) : std::max(0, from - mX);
        }
        else
        {
            walk = !mFacingLeft ? (mBeatMax - mX) + std::max(0, mBeatMax - from) : std::max(0, mX - from);
        }
        // (measured at half size: 232 pixels in 202 frames)
        return (mHalf ? walk * 10 / 12 : walk * 10 / 24) + mSightDelay;
    }
};

// A flying Slig that is about. It goes along its track from end to end, and goes for Abe (grenades) when it is on
// his screen, faces him and he isn't in an invisible zone.
struct FlierAt final
{
    s32 mX = 0;
    s32 mY = 0;
    // What it moves a frame, x256 (0 while it waits at an end of its track)
    s32 mVelX = 0;
    s32 mVelY = 0;
    bool mHalf = false;
    bool mFacingLeft = false;
};

// A Slog that is about. Awake it runs Abe down wherever he is.
struct SlogAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mAsleep = false;
    // Asleep still, with the switch that sets it on Abe already on
    bool mWaking = false;
    bool mFacesLeft = false;
    // The game's motion number
    s32 mMotion = 0;
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

// A motion detector's beam as it is now: it goes to and fro between two ends and sets the alarm off when it is
// over something that moves. Abe standing or crouching still is not seen.
struct BeamAt final
{
    s32 mX1 = 0;
    s32 mY1 = 0;
    s32 mX2 = 0;
    s32 mY2 = 0;
    s32 mMinX = 0;
    s32 mMaxX = 0;
    // What it moves a frame, x256, the way it goes
    s32 mVel256 = 0;
    // The frames it still waits at an end before it moves, and how long it waits at each
    s32 mPause = 0;
    s32 mEndPause = 0;
    bool mHalf = false;
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
    // A Fleech: it is going up a hoist (it ends on the top of it whatever he does meanwhile)
    bool mClimbing = false;
    // A Fleech: it is after him (awake it may only be walking its beat: Fleech.cpp's patrol brain takes no notice
    // of him until he is on its level in its sight, or makes a noise within six cells of it)
    bool mChasing = false;
};

// A Scrab that is about
struct ScrabAt final
{
    // The entity it is (0: not one of the path's)
    u32 mId = 0;
    s32 mX = 0;
    s32 mY = 0;
    bool mHalf = false;
    // Abe is in it
    bool mPossessed = false;
    // What it is doing (Scrab.hpp: 0 stands, 26 howls)
    s32 mMotion = 0;
    bool mFacesLeft = false;
};

// A Paramite that is about. Alone on its screen it keeps away from Abe and only goes for him cornered; with
// another on the screen they come for him (Paramite.cpp).
struct ParamiteAt final
{
    // The entity it is (0: not one of the path's)
    u32 mId = 0;
    s32 mX = 0;
    s32 mY = 0;
    bool mHalf = false;
    // Abe is in it
    bool mPossessed = false;
    s32 mMotion = 0;
    bool mFacesLeft = false;
    // Its brain (Paramite::BrainIndex: 0 patrol, 2 after him, 3 and 9 not down its web yet, 5 after meat,
    // 8 following a possessed one) and the state of it
    s32 mBrain = 0;
    s32 mSub = 0;
    // Up its web still: not there to be met or taken over
    bool Hidden() const
    {
        return mBrain == 3 || mBrain == 9;
    }
};

// A Slurg: where it is, its box's sides, and which way it crawls (0: not seen to move yet)
struct SlurgAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mHalf = false;
    s32 mX1 = 0;
    s32 mX2 = 0;
    s32 mDir = 0;
};

// The fart that goes off, while there is one
struct FartAt final
{
    bool mLive = false;
    // In hundredths of a pixel, as are its speeds a frame
    s32 mX100 = 0;
    s32 mY100 = 0;
    s32 mVelX100 = 0;
    s32 mVelY100 = 0;
    // Abe flies it
    bool mFlown = false;
    bool mExploded = false;
    s32 mFramesLeft = 0;
    bool mHalf = false;
};

struct World final
{
    // He has drunk a Brew: his next fart goes off
    bool mBrew = false;
    FartAt mFart;
    // Rocks Abe has on him, and those lying about
    s32 mRocks = 0;
    std::vector<RockAt> mRocksDown;
    // The ones in the air
    std::vector<RockAt> mRocksFlying;
    // The rocks that fall over and over where the level drops them (FallingItem): where each is now, mChasing
    // while it is coming down
    std::vector<RockAt> mFallers;
    // The switch that stands for the hazard with this id (a mine's TLV) being gone: above the game's own 256
    static s32 HazardSwitch(u32 id)
    {
        return 1000 + static_cast<s32>(id);
    }
    // A door that opens once all of a set of switches are on (a tasks door) is given a switch of its own that
    // stands for "all of them are": see Switch()
    static s32 DoorSwitch(u32 id)
    {
        return 600000 + static_cast<s32>(id);
    }
    std::vector<UxbAt> mUxbs;
    std::vector<SweepAt> mSweeps;
    // A Slig that waits at least this many frames before it shoots at what it has seen can be run from
    static constexpr s32 kOutrunDelay = 45;
    std::vector<BeamAt> mBeams;
    // Whether Abe can make a move without a motion detector's beam coming over him before it is done: he is at
    // x, y, the move takes him up to travel on in dir (-1, 0, 1) and lasts so many frames
    bool BeamClear(s32 x, s32 y, bool half, s32 dir, s32 travel, s32 frames) const
    {
        for (const BeamAt& b : mBeams)
        {
            if (b.mHalf != half || b.mY1 > y || b.mY2 < y - (half ? 30 : 60))
            {
                continue;
            }
            // Frame by frame: where he is by then against where the beam is, turning round at its ends
            // (the beam has to be 8 inside his box to count)
            const s32 pad = half ? 5 : 10;
            s32 at256 = b.mX1 * 256;
            s32 vel = b.mVel256;
            const s32 width = b.mX2 - b.mX1;
            s32 pause = b.mPause;
            for (s32 t = 0; t <= frames; t++)
            {
                const s32 ax = x + (frames > 0 ? dir * travel * t / frames : 0);
                const s32 bx = at256 / 256;
                if (bx <= ax + pad && bx + width >= ax - pad)
                {
                    return false;
                }
                if (pause > 0)
                {
                    pause--;
                    continue;
                }
                at256 += vel;
                // (it is the beam's middle that goes from end to end)
                const s32 mid = at256 / 256 + width / 2;
                if ((vel > 0 && mid >= b.mMaxX) || (vel < 0 && mid <= b.mMinX))
                {
                    vel = -vel;
                    pause = b.mEndPause;
                }
            }
        }
        return true;
    }
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
    std::vector<FlierAt> mFliers;
    // Abe has the power to go invisible (from a slap lock) and has not used it yet; he is invisible now (Fleeches
    // and the rest take no notice of him: it lasts a few hundred frames); he has had the power in this path
    bool mInvisCharge = false;
    bool mInvisible = false;
    // The frames of it that are left (0 when he is not): what is left on its timer and the 140 he takes to come
    // back into sight after (measured: the timer out at f=1727, seen again at 1869)
    s32 mInvisLeft = 0;
    // The frame its timer ran out at (0: it has not)
    u32 mInvisFadeAt = 0;
    bool mInvisEver = false;
    // A lock that gives the power has been slapped and its ring is on its way to him (the bot's own note)
    bool mInvisComing = false;
    // Slurgs: a step on one bursts it, and the noise wakes every Fleech on the screen. Standing still by one is
    // safe: it is the feet coming down that do it.
    std::vector<ScrabAt> mScrabs;
    std::vector<ParamiteAt> mParamites;
    // The live Scrab an entity stands for, or null
    const ScrabAt* ScrabOf(u32 id) const
    {
        for (const ScrabAt& scrab : mScrabs)
        {
            if (scrab.mId == id && id != 0)
            {
                return &scrab;
            }
        }
        return nullptr;
    }
    // Slap locks (their entities) slapped in this path: the switch follows when the ghost gets to its tomb
    // Slap locks that are about and have not been slapped (as the game has it: a slap can land on something else)
    std::set<u32> mLocksIntact;
    // The ends the map gives the Scrabs' beats (x, floor y): a Scrab on its beat turns at the nearest either side
    std::vector<std::pair<s32, s32>> mScrabBoundsLeft;
    std::vector<std::pair<s32, s32>> mScrabBoundsRight;
    // Where the map lets Slogs out once a switch is on: x, floor y, the switch
    std::vector<std::array<s32, 4>> mSlogSpawners;
    // The switch that where Abe stands is about to set (an invisible switch sets its own a few frames after he
    // is in it), or -1
    s32 mSoonOn = -1;
    std::set<u32> mLocksSlapped;
    // Scrabs (their entities) killed in this path, and where one that was shut in behind a gate was left
    std::set<u32> mScrabsKilled;
    std::map<u32, s32> mScrabParked;
    // Fleeches (their entities) that a Scrab has been made to kill in this path
    std::set<u32> mFleechKilled;
    std::vector<SlurgAt> mSlurgs;
    // Whether a hop or a drop that ends at x (on a floor at y, facing left or not) in so many frames comes down
    // on one: his feet land nine behind and six ahead of where he ends (measured from the game's footstep
    // points), a Slurg's box is where the game says it is, and it crawls on half a pixel a frame meanwhile
    bool SlurgWhereHeLands(s32 x, s32 y, bool half, bool left, s32 frames) const
    {
        const s32 lo = x - (left ? 6 : 9) - 3;
        const s32 hi = x + (left ? 9 : 6) + 3;
        for (const SlurgAt& slurg : mSlurgs)
        {
            if (slurg.mHalf != half || std::abs(slurg.mY - y) * (half ? 2 : 1) > 14)
            {
                continue;
            }
            // (one whose way is not known yet may go either)
            const s32 back = slurg.mDir > 0 ? 0 : frames / 2 + 1;
            const s32 on = slurg.mDir < 0 ? 0 : frames / 2 + 1;
            if (slurg.mX1 - back <= hi && slurg.mX2 + on >= lo)
            {
                return true;
            }
        }
        return false;
    }
    // Whether a foot put down at x on a floor at y may come down on a Slurg (its box is up to 21 either side of
    // it, the feet land up to 10 from where he is)
    bool SlurgBy(s32 x, s32 y, bool half) const
    {
        for (const SlurgAt& slurg : mSlurgs)
        {
            if (slurg.mHalf == half && std::abs(slurg.mY - y) * (half ? 2 : 1) <= 14 && std::abs(slurg.mX - x) * (half ? 2 : 1) < 34)
            {
                return true;
            }
        }
        return false;
    }
    // Fleeches that are awake (asleep or hanging they are no danger until a noise or their switch wakes them)
    std::vector<RockAt> mFleechesAwake;
    // And the ones asleep: a noise within 150 of one (a footstep that is not a sneaking one) angers it, and two
    // in a row wake it
    std::vector<RockAt> mFleechesAsleep;
    // Where the bird portals that are open now are
    std::vector<RockAt> mPortalsOpen;
    std::vector<LiftAt> mLifts;
    // Where each lift was when it was last about
    // What a Slig has to say to each Slig lock, in turn: the numbers of the speak buttons (1 to 8)
    std::map<u32, std::vector<s32>> mLockCodes;
    std::vector<LiftAt> mLiftsSeen;
    // Whether a lift is at one of its stops (a floor at y that x is on). One that isn't about, its screen not
    // being loaded, is where it was last seen, or failing that where it starts.
    // Whether what he carries and throws on this path is meat: its sacks are meat sacks (eRockSack, mInputs[1])
    bool CarriesMeat() const
    {
        for (const Entity& e : mEntities)
        {
            if (e.mKind == EntityKind::eRockSack && e.mInputs[1] == 1)
            {
                return true;
            }
        }
        return false;
    }
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

    // The switches of gates that are taken away for good the first time they open (SlamDoor's delete flag), once
    // seen at the value that opens them: counted as still at it whatever the switch does after. The Mudomo
    // trial's three under the ring, whose switch a self-closing trap door sets back twenty frames later.
    std::map<s32, s32> mGatesGone;
    // real: as the game has it now, without counting a slapped lock's switch as on already
    s32 Switch(s32 id, bool real = false) const
    {
        if (!real)
        {
            const auto gone = mGatesGone.find(id);
            if (gone != mGatesGone.end())
            {
                return gone->second;
            }
        }
        if (id >= 256)
        {
            for (const Entity& e : mEntities)
            {
                if (e.mKind == EntityKind::eMine && e.mSwitch == id)
                {
                    return e.mGone ? 1 : 0;
                }
                if (e.mKind == EntityKind::eFlier && e.mSwitch == id)
                {
                    // (mInputs[0] 0: one that is not to be got rid of, see FeatureKind::eDanger)
                    return e.mGone || mFleechKilled.count(e.mId) != 0 || e.mInputs[0] == 0 ? 1 : 0;
                }
                if (e.mKind == EntityKind::eSwitchLogic && e.mSwitch == id)
                {
                    for (s32 input : e.mInputs)
                    {
                        if (input > 1 && !Switch(input, real))
                        {
                            return 0;
                        }
                    }
                    return 1;
                }
            }
            return 0;
        }
        // (a slapped lock's switch is as good as on: its ghost is on the way, and where the door it opens is
        // is the place to wait for it, not where he stands)
        for (const Entity& e : mEntities)
        {
            if (!real && e.mKind == EntityKind::eSlapLock && e.mSwitch == id && id > 1 && mLocksSlapped.count(e.mId) != 0)
            {
                return 1;
            }
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
