#pragma once

#include "Pilot.hpp"
#include "Domain.hpp"
#include "World.hpp"
#include <array>
#include <functional>
#include <memory>
#include <string>

namespace Bot {

class Bot;

struct TaskStep final
{
    enum class Kind : u8
    {
        ePrim,
        eDone,
        eFailed,
    };
    Kind mKind = Kind::eDone;
    Prim mPrim;
    std::string mWhy;

    static TaskStep Do(const Prim& prim)
    {
        TaskStep s;
        s.mKind = Kind::ePrim;
        s.mPrim = prim;
        return s;
    }
    static TaskStep Done()
    {
        return {};
    }
    static TaskStep Failed(const std::string& why)
    {
        TaskStep s;
        s.mKind = Kind::eFailed;
        s.mWhy = why;
        return s;
    }
};

// One action of a plan, carried out a Prim at a time. Next is asked whenever Abe has finished the last Prim: it
// looks at the world as it is then, so a task can be started again from anywhere after a rewind.
class Task
{
public:
    virtual ~Task() = default;
    virtual std::string Name() const = 0;
    // lastFailed: why the Prim it gave last failed, or empty
    virtual TaskStep Next(Bot& bot, const std::string& lastFailed) = 0;
};

class GoToTask;

// A flying Slig that goes round and round its track: where it was each frame, and from that where it will be.
// (It turns corners and waits at ends, so where it is going now says little about where it is half a second on;
// but left alone it does the same round again, to the pixel.)
class FlierWatch final
{
public:
    // Every frame, with the one watched (nullptr: not to be seen, and what was seen is forgotten)
    void See(s32 frame, const FlierAt* flier);
    // Where it is and faces `ahead` frames on. False while it has not been seen to do a round twice.
    bool Ahead(s32 ahead, s32& x, s32& y, bool& left) const;

private:
    struct Seen final
    {
        s32 mX = 0;
        s32 mY = 0;
        bool mLeft = false;
    };
    std::vector<Seen> mSeen;
    s32 mFrame = -1;
};

// Turn a UXB off: crouch a cell from it, facing it, and slap it as its light turns green
class DisarmTask final : public Task
{
public:
    // stand: the cell to do it from (-1: either side of it)
    DisarmTask(s32 x, s32 y, bool half, s32 stand = -1)
        : mX(x)
        , mY(y)
        , mHalf(half)
        , mStand(stand)
    {
    }
    ~DisarmTask() override;
    std::string Name() const override
    {
        return "Disarm the UXB at " + std::to_string(mX);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // The UXB (still on) that a hop from fromX to toX goes over, or null
    static const UxbAt* Under(const World& world, s32 fromX, s32 toX, s32 y, bool half);

private:
    s32 mX;
    s32 mY;
    bool mHalf;
    s32 mStand;
    std::unique_ptr<GoToTask> mGo;
    bool mWasGreen = true;
    s32 mFrames = 0;
    s32 mSlaps = 0;
    s32 mBeamWaits = 0;
};

// Jump into a rock sack and pick up the rocks that fall out
class GetRocksTask final : public Task
{
public:
    explicit GetRocksTask(u32 sack)
        : mSack(sack)
    {
    }
    ~GetRocksTask() override;
    std::string Name() const override
    {
        return "Get rocks #" + std::to_string(mSack);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    // A boom machine's button has been pressed with a Slig about: off its screen first
    bool mRetreat = false;
    // At a boom machine a Slig guards: in the shadow by it until the Slig turns its back, then out
    bool mPrimed = false;
    bool mHid = false;
    bool mGuardSeen = false;
    bool mGuardAway = false;
    s32 mHidFrames = 0;
    // Where that shadow is, to go back into once the button is pressed
    s32 mHideX = 0;
    // Out of it again for the grenade: whether he has left, and the watch on the Slig's back until he does
    bool mOut = false;
    bool mOutSeen = false;
    bool mOutAway = false;
    s32 mOutFrames = 0;
    s32 mRetreatX = 0;
    // The grenade is in hand and he has been sent into the nearest shadow
    bool mShade = false;
    u32 mSack;
    std::unique_ptr<GoToTask> mGo;
    s32 mJumps = 0;
    s32 mWaits = 0;
    s32 mPicks = 0;
    // Meat with a Paramite on the sack's level: what the first sack drops is left for it, and the meat to keep
    // is had from another sack of that level while it eats (mBaitX: the first sack)
    bool mBaited = false;
    s32 mBaitX = 0;
};

// Get into a mine car, drive it to a cell and get out. A direction held carries the car on round corners, up
// walls and along ceilings, so it is driven by holding the way that moves it nearer and, where that does
// nothing, trying the others in turn.
class RideCarTask final : public Task
{
public:
    RideCarTask(u32 car, s32 dest)
        : mCar(car)
        , mDest(dest)
    {
    }
    ~RideCarTask() override;
    std::string Name() const override
    {
        return "Ride mine car #" + std::to_string(mCar);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mCar;
    s32 mDest;
    std::unique_ptr<GoToTask> mGo;
    s32 mSteps = 0;
    s32 mEnters = 0;
    // The key held, and how many steps it has moved nothing
    u32 mKey = 0;
    s32 mStill = 0;
    s32 mLastX = 0;
    s32 mLastY = 0;
    // The keys that did nothing where the car now stands
    std::vector<u32> mDud;
    // The way the car last went: not straight back that way while anything else moves it
    u32 mWent = 0;
    s32 mExits = 0;
    // With Mudokons to save, one on the car's floor ahead is not run over: stopped short of it (and of a UXB by it),
    // out, the UXB turned off, the Mudokon led back behind the car, and in again
    u32 mAhead = 0;
    std::vector<u32> mAheadGroup;
    u32 mAheadUxb = 0;
    s32 mAsideExits = 0;
    s32 mUxbHeld = 0;
    std::unique_ptr<Task> mAside;
    std::vector<u32> mLed;
};

// Throw rocks at a mine until it goes off: from one place after another, keeping what missed and what killed
class BlowMineTask final : public Task
{
public:
    // from: a cell of the stretch of floor the plan throws from (its region), or -1 for where Abe is
    explicit BlowMineTask(u32 mine, s32 from = -1)
        : mMine(mine)
        , mFrom(from)
    {
    }
    ~BlowMineTask() override;
    std::string Name() const override
    {
        return "Blow up mine #" + std::to_string(mMine);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // What to hold with the throw button for a kind of throw, facing left or right
    static u32 AimBits(ThrowSpot::Kind kind, bool facingLeft);

    // Mines still there
    static s32 LiveMines(const World& world);

private:
    s32 mLost = 0;
    s32 mLiveBefore = 0;
    u32 mMine;
    s32 mFrom = -1;
    std::unique_ptr<GoToTask> mGo;
    // Cells that couldn't be got to
    std::vector<s32> mNoWay;
    s32 mGoNode = -1;
    // 0 getting set, 1 the rock is in his hand, 2 thrown
    s32 mPhase = 0;
    s32 mWait = 0;
    s32 mSettle = 0;
    s32 mThrows = 0;
};

// A grenade from a boom machine thrown at a Slig he can't take over (Domain::GrenadeTargets): to a cell to throw
// from, there until the Slig is where the grenade will go off, thrown, and down on his knees until it has (who
// stands by a blast is thrown over, and that is heard).
class GoToTask;
class GrenadeSligTask final : public Task
{
public:
    // from: a cell of the stretch of floor the plan throws from
    GrenadeSligTask(u32 slig, s32 from)
        : mSlig(slig)
        , mFrom(from)
    {
    }
    ~GrenadeSligTask() override;
    std::string Name() const override
    {
        return "Throw a grenade at Slig#" + std::to_string(mSlig);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

    // Where a Slig that walks its beat (from x, that way, between those ends) is so many frames on
    static s32 SligIn(s32 x, bool left, s32 beatMin, s32 beatMax, s32 frames);
    // A Slig walks this far in a hundred frames
    static constexpr s32 kPace = 220;
    // The frames from the throw begun until the Slig is to be where the grenade goes off
    static constexpr s32 kAhead = 86;

private:
    u32 mSlig;
    s32 mFrom;
    std::unique_ptr<GoToTask> mGo;
    s32 mGoNode = -1;
    std::vector<s32> mNoWay;
    // 0 to the place and until the moment, 1 the grenade is in his hand, 2 thrown
    s32 mPhase = 0;
    s32 mWait = 0;
    s32 mHeld = 0;
    s32 mSettle = 0;
    s32 mLost = 0;
    // For a locker: the Sligs about by it when this began
    s32 mAbout = -1;
    // Where the grenade thrown was last seen on its way
    s32 mSeenX = 0;
    s32 mSeenY = 0;
    // The throw chosen: how, and where it goes off
    ThrowSpot::Kind mKind = ThrowSpot::eUp;
    s32 mLandX = 0;
};

// Take over a naked Slig, crawl it to the button for its kind and press it. Then, with a locker for guns on that
// floor: to the locker, where it comes by a gun and is still his, the other naked ones of the floor shot with it
// before they come by theirs; and burst.
class CrawlerButtonTask final : public Task
{
public:
    CrawlerButtonTask(u32 button, s32 from)
        : mButton(button)
        , mFrom(from)
    {
    }
    ~CrawlerButtonTask() override;
    std::string Name() const override
    {
        return "Press SligButton#" + std::to_string(mButton) + " with a naked Slig";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mButton;
    s32 mFrom;
    std::unique_ptr<GoToTask> mGo;
    s32 mChanted = 0;
    s32 mCrawled = 0;
    s32 mPresses = 0;
    s32 mPressWait = 0;
    bool mMoved = true;
    s32 mShot = 0;
    s32 mQuiet = 0;
    bool mWas = false;
    bool mSaid = false;
};

// Take over a Glukkon (from the cell mFrom, unless he is in one already), walk it into a voice lock's box and say
// what the lock wants said: "Hey" to its "Hey", "Do it" to its "What?" (GlukkonSwitch). He stays in the Glukkon.
// What the Glukkon he is in has told the Sligs of its floor, for the orders that take several: a lift worked for
// it, a Slog shot for it
struct GlukkonOrders final
{
    // Frames nothing more is said for
    s32 mQuiet = 0;
    // "Hey" has been said to the Slig that is to do it
    bool mHey = false;
    s32 mSaid = 0;
    bool mLogged = false;
    // A Slig has been told to come along ("Come here" once it listens: it follows from then on)
    bool mFollow = false;
    s32 mWaited = 0;
    // The frame the Slig's shots last began, whether the order for after them has been given, and the frame of
    // the last order to shoot
    // Frames the Slig has stood still more than two cells behind
    s32 mBehind = 0;
    s32 mFireFrame = -1000;
    bool mFireOrdered = false;
    s32 mOrderFrame = -1000;
};

class VoiceLockTask final : public Task
{
public:
    VoiceLockTask(u32 lock, s32 from)
        : mLock(lock)
        , mFrom(from)
    {
    }
    ~VoiceLockTask() override;
    std::string Name() const override
    {
        return "Answer VoiceLock#" + std::to_string(mLock) + " with a Glukkon";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLock;
    s32 mFrom;
    std::unique_ptr<GoToTask> mGo;
    s32 mChanted = 0;
    s32 mWalked = 0;
    // Frames in the lock's box since it was come into (or since the last try), and the tries
    s32 mInBox = 0;
    s32 mTries = 0;
    bool mWasKnown = false;
    bool mWas = false;
    bool mSaid = false;
    // Frames waited for a teleporter to take the Glukkon away
    s32 mLeft = 0;
    // Frames with no way for it from where it is
    s32 mLost = 0;
    // Frames spent having other Sligs shot on the way
    s32 mKills = 0;
    GlukkonOrders mOrdersGiven;
};

class UseTask;

// A lever pulled to set its drills on the Slogs the map has put to sleep under them, and pulled again once those
// are dead (Domain: eDrillSlogs)
class DrillSlogsTask final : public Task
{
public:
    explicit DrillSlogsTask(u32 lever)
        : mLever(lever)
    {
    }
    ~DrillSlogsTask() override;
    std::string Name() const override
    {
        return "Drill the Slogs with #" + std::to_string(mLever);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLever;
    // 0: the lever on, 1: until the Slogs are dead, 2: the lever off
    s32 mPhase = 0;
    s32 mWaited = 0;
    std::unique_ptr<UseTask> mUse;
};

// A Slig on a floor only a lift goes to, got rid of with the Slig a Glukkon calls for: he says hello to the
// Glukkon from mHello (its alarm sends a naked Slig for pants and after him), goes to the lever of the gate the
// Slig comes through, shuts it behind it, takes it over from there, rides it up the lift and shoots (Domain:
// ePantsSlig). The brewery's first hub.
class PantsSligTask final : public Task
{
public:
    PantsSligTask(u32 target, u32 lever, s32 hello)
        : mTarget(target)
        , mLever(lever)
        , mHello(hello)
    {
    }
    ~PantsSligTask() override;
    std::string Name() const override
    {
        return "Kill Slig #" + std::to_string(mTarget) + " with the Slig a Glukkon calls";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mTarget;
    u32 mLever;
    s32 mHello;
    // 0 to the hello, 1 to the lever, 2 the wait for the Slig and the pull, 3 the chant, 4 to the lift, 5 up it,
    // 6 the shooting, 7 back down, 8 the burst, 9 himself again
    s32 mPhase = 0;
    s32 mWaits = 0;
    s32 mShots = 0;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<UseTask> mUse;
};

// In a Glukkon: go to the cell mStand by a lever, call for help, have the Slig that comes come to it and say "Do
// it", which has a Slig that listens pull the lever a cell from it
class SligLeverTask final : public Task
{
public:
    SligLeverTask(u32 lever, s32 stand)
        : mLever(lever)
        , mStand(stand)
    {
    }
    std::string Name() const override
    {
        return "Have a Slig pull Lever#" + std::to_string(mLever);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLever;
    s32 mStand;
    bool mWasKnown = false;
    bool mWas = false;
    s32 mLost = 0;
    // Frames with no Slog about since the lever, and how many of them say the lever's Slogs are all dead
    s32 mClear = 0;
    static constexpr s32 kSlogsGone = 200;
    // The frames an order to shoot takes to run out: 33 until the Slig shoots at the latest, and 15 of that
    static constexpr s32 kLastShots = 50;
    std::unique_ptr<GoToTask> mGo;
    bool mPerched = false;
    // The gate its lever opens on Sligs that are then shot as they come through (0: none)
    s32 mGateX = 0;
    bool mStandSet = false;
    GlukkonOrders mOrdersGiven;
    s32 mChanted = 0;
    s32 mFrames = 0;
    // Frames until the next thing is said, and what was said last (0 nothing, 1 help, 2 hey, 3 come here, 4 do it)
    s32 mQuiet = 0;
    s32 mSaid = 0;
    s32 mOrders = 0;
};

// A lever, and at once through what the trap door it opens lets through before that shuts by itself
// (StepKind::eTimedDoor): the lever, then the way to the far end of that edge at a run, with what is quick taken
// for cheap (Bot::TimedRun)
class TimedDoorTask final : public Task
{
public:
    TimedDoorTask(u32 lever, s32 want, s32 edge)
        : mLever(lever)
        , mWant(want)
        , mEdge(edge)
    {
    }
    ~TimedDoorTask() override;
    std::string Name() const override
    {
        return "Use #" + std::to_string(mLever) + " and through at once";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLever;
    s32 mWant;
    s32 mEdge;
    std::unique_ptr<Task> mUse;
    std::unique_ptr<GoToTask> mGo;
    bool mPulled = false;
};

// An electric wall has come on across the floor a Slig walks (Domain, StepKind::eZapWait): where he is, until it
// has walked into it
class ZapWaitTask final : public Task
{
public:
    explicit ZapWaitTask(u32 slig)
        : mSlig(slig)
    {
    }
    std::string Name() const override
    {
        return "Wait for the wall to get Slig#" + std::to_string(mSlig);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mSlig;
    s32 mWaits = 0;
};

// The Slogs of a stretch of floor got to come after him (Domain, StepKind::eDrawSlogs): to a cell over their
// screen that they can't get at him in, and there until they are all on their way
class DrawSlogsTask final : public Task
{
public:
    DrawSlogsTask(u32 slog, s32 stand)
        : mSlog(slog)
        , mStand(stand)
    {
    }
    ~DrawSlogsTask() override;
    std::string Name() const override
    {
        return "Draw out Slog#" + std::to_string(mSlog);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mSlog;
    s32 mStand;
    std::unique_ptr<GoToTask> mGo;
    s32 mWaits = 0;
};

// A bone thrown to Slogs from over their floor (Domain, StepKind::eFeedSlogs): to the cell, the throw, and until
// the bone is down among them, or a Slog has gone over the plate it was thrown past
class FeedSlogsTask final : public Task
{
public:
    FeedSlogsTask(u32 slog, s32 stand, u32 aim)
        : mSlog(slog)
        , mStand(stand)
        , mKind(static_cast<ThrowSpot::Kind>((aim & 255) / 2))
        , mLeft(aim % 2 == 1)
        , mPlate(static_cast<s32>(aim >> 8))
    {
    }
    // Slogs that are loose, on the stretch x1..x2 of the floor at floorY
    FeedSlogsTask(s32 x1, s32 x2, s32 floorY, s32 stand, u32 aim)
        : FeedSlogsTask(0, stand, aim)
    {
        mDyn = true;
        mPen = {x1, x2, floorY};
    }
    // (how long he is in a hurry once the bone is down)
    void SetRun(s32 frames)
    {
        mRun = frames;
    }
    ~FeedSlogsTask() override;
    std::string Name() const override
    {
        return mDyn ? "Throw a bone to the Slogs that are loose" : "Throw a bone to Slog#" + std::to_string(mSlog);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mSlog;
    bool mDyn = false;
    std::array<s32, 3> mPen = {};
    s32 mRun = 55;
    s32 mStand;
    ThrowSpot::Kind mKind;
    bool mLeft;
    // The switch of a plate the bone is thrown past, for a Slog to step on (0: none)
    s32 mPlate;
    std::unique_ptr<GoToTask> mGo;
    bool mInHand = false;
    // The cell on the Slogs' floor he goes down to from where he throws (-1: none, or not asked yet), and the
    // way there as it is asked when it would go
    bool mAsked = false;
    s32 mDown = -1;
    std::unique_ptr<GoToTask> mLeap;
    s32 mLeapWaits = 0;
    // The bones he had before the throw (-1: not thrown yet), and the frames since
    s32 mHad = -1;
    s32 mFrames = 0;
};

// Bones thrown away from a stretch of floor whose Slogs are out after him (Domain, StepKind::eFeedAway): two, if
// he has them, from a cell they can't get at him in; then down to the cell by the way onto the stretch, while they
// are at the first; and there until the first is gone and they turn to the second. A Slog finds a bone on the screen
// shown only (Slog::FindBone), and the way on leaves this screen: what he has for it is the bone they are at as he
// goes, so he goes as they start on the last one.
class FeedAwayTask final : public Task
{
public:
    FeedAwayTask(s32 x1, s32 x2, s32 floorY, s32 stand, u32 aim, s32 waitAt)
        : mX1(x1)
        , mX2(x2)
        , mFloorY(floorY)
        , mStand(stand)
        , mAim(aim)
        , mWaitAt(waitAt)
    {
    }
    ~FeedAwayTask() override;
    std::string Name() const override
    {
        return "Throw bones away from the Slogs";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    s32 Lying(const Bot& bot) const;
    s32 mX1;
    s32 mX2;
    s32 mFloorY;
    s32 mStand;
    u32 mAim;
    s32 mWaitAt;
    std::unique_ptr<FeedSlogsTask> mThrow;
    std::unique_ptr<GoToTask> mGo;
    s32 mThrown = 0;
    s32 mWanted = 0;
    s32 mWaits = 0;
};

// A bone thrown to lie under a thing that falls over and over, for the Slog that is after him (Domain,
// StepKind::eBaitSlog): the throw, and then until the thing has come down on the Slog, or the bone is gone
class BaitSlogTask final : public Task
{
public:
    BaitSlogTask(s32 stand, u32 aim, s32 fallerX, s32 floorY)
        : mStand(stand)
        , mAim(aim)
        , mFallerX(fallerX)
        , mFloorY(floorY)
    {
    }
    ~BaitSlogTask() override;
    std::string Name() const override
    {
        return "Bait the Slog under the falling thing at " + std::to_string(mFallerX);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    s32 mStand;
    u32 mAim;
    s32 mFallerX;
    s32 mFloorY;
    std::unique_ptr<FeedSlogsTask> mThrow;
    bool mThrown = false;
    s32 mWaits = 0;
};

// Hold a grenade and throw it late, so that it goes off in the air by a security orb (Domain::OrbTargets)
class GrenadeOrbTask final : public Task
{
public:
    // from: a cell of the stretch of floor the plan throws from
    GrenadeOrbTask(u32 orb, s32 from)
        : mOrb(orb)
        , mFrom(from)
    {
    }
    ~GrenadeOrbTask() override;
    std::string Name() const override
    {
        return "Burst a grenade by SecurityOrb#" + std::to_string(mOrb);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mOrb;
    s32 mFrom;
    std::unique_ptr<GoToTask> mGo;
    s32 mGoNode = -1;
    std::vector<s32> mNoWay;
    // 0 to the place, 1 the grenade is in his hand, 2 thrown
    s32 mPhase = 0;
    s32 mWait = 0;
    s32 mSettle = 0;
    s32 mLost = 0;
    ThrowSpot::Kind mKind = ThrowSpot::eUp;
};

// Walk, hop, climb... to a cell the goal accepts, by the nav graph
class GoToTask final : public Task
{
public:
    // Whether the first `count` edges of a way can be gone now without a drill reaching Abe on any of them: each
    // edge has him in the cell it leaves and the one it leads to for as long as it may take
    // late: frames he is later (or, negative, sooner) than worked out
    // The drill that goes over Abe's head at a cell and is moving or will be, or null
    static const Entity* SweepHigh(const NavGraph& nav, const World& world, s32 node);
    // Where he is a frame of a way: his feet, how tall he is then, and whether he is off the floor
    struct Body final
    {
        s32 mX = 0;
        s32 mY = 0;
        s32 mTall = 0;
        bool mHalf = false;
        // His box about his feet at full scale: how far up, and to the back and the front of the way he goes
        // (and how far down past them: hung from a ledge)
        s32 mUp = 53;
        s32 mDown = 0;
        s32 mBack = 14;
        s32 mFront = 14;
        bool mLeft = false;
    };
    // Where he is each frame of `count` edges of a way from the edge `first` on (rest: and for a moment where it
    // ends), added to out
    static void Bodies(const NavGraph& nav, const std::vector<s32>& path, std::size_t first, std::size_t count, bool rolled, bool rest, std::vector<Body>& out);
    // His box as the flying mines are kept clear of
    static BombCycle::Box BombBody(const Body& body);
    // A cell no flying mine ever reaches him at: standing, or (duck) only crouched
    static bool BombStop(const NavGraph& nav, const BombCycle& bombs, s32 node, bool& duck);
    // A flying mine's track goes over that cell, clear of him only while he is crouched there
    static bool MinesOverhead(Bot& bot, s32 node);
    // Whether the first `count` edges of a way can be gone now without a flying mine meeting him
    // delay: frames he stands where he is first; later: as it will be that many frames from now
    static bool BombsClear(const NavGraph& nav, const BombCycle& bombs, const std::vector<s32>& path, std::size_t count, bool rolled, s32 delay, s32 later = 0);
    // rolled: as it goes if the walking of it is rolled, from a crouch
    static bool SweepsClear(const NavGraph& nav, const World& world, const std::vector<s32>& path, std::size_t count, s32 late = 0, bool rolled = false);
    // The same for a roll from a crouch where he is, frame by frame as it was measured: for a row of drills that
    // SweepsClear's margins never find a moment under
    static bool RollClear(const NavGraph& nav, const World& world, const std::vector<s32>& path, std::size_t count);
    // The drill that reaches a cell and runs as things are, or null
    static const Entity* SweepOver(const NavGraph& nav, const World& world, s32 node);
    // The lever or ring that stops a drill for good and can be got to from a cell without passing any drill
    static const Entity* StopFor(Bot& bot, const Entity& drill, s32 from);
    GoToTask(std::string name, std::function<bool(s32 node)> isGoal);
    // The cell at a world position
    static std::unique_ptr<GoToTask> To(Bot& bot, s32 x, s32 y, bool half);

    std::string Name() const override
    {
        return mName;
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // AE: the Slig that stands guard (awake, not on a beat) on the floor at y within ten cells of x, or null. It
    // turns where it stands, by a wait the game draws, and shoots what it sees in front of it on its floor; what
    // hangs from the end of its floor it does not see (measured in the Barracks' first path, the guard at 2137,1739:
    // hung at 2087 for 150 frames with it turned his way).
    static const SligAt* Sentry(const World& world, s32 x, s32 y, bool half);
    // It has begun to turn its back to x: from now it looks away for as long as it ever does (there, 9 frames
    // of the turn and 30 at least stood, against the 24 it takes him to pull himself up)
    static bool TurnsFrom(const SligAt& slig, s32 x);
    // Hung from the ledge at that cell with such a guard on its floor: now is when to pull himself up. The guard
    // has begun to turn its back, and no other Slig comes to where it sees that floor in the frames it takes him
    // to be up and off it (there, the one whose beat on the floor below ends at 1912: it shot him at 2087,1738 as
    // he came up). why: the Slig waited for.
    static bool UpBehind(const NavGraph& nav, const World& world, s32 ledge, const SligAt** why);
    // From that cell, with such a guard between him and a hoist three cells on that faces him: where the running
    // jump that catches the hoist takes off (0: no such jump). Measured in the Barracks' first path, from a stand
    // at 2087,1739 behind the guard at 2137: the run is one he can jump from two cells on (off at 2137, the hop
    // held from the start), and the jump catches the hoist in the very next cell: hung from 2162,1660 eleven
    // frames after the run began, where walking there and jumping up at it takes some forty with it a cell off.
    // The walkthrough's "hoist up and quickly run and jump up to the next ledge above him".
    // hurry: off the floor of a Slig that has woken, where one further on than the hoist counts too
    static s32 JumpUpPastGuard(const NavGraph& nav, const World& world, s32 cell, const NavEdge& hoistUp, bool hurry = false);
    // The frames from hung to up (24) and to off its floor by a running jump (12), and some over
    static const s32 kUpAndOff = 60;
    // Never faster than this (Mudokons in tow don't stop at wheels when they have to run to keep up)
    // Crouched on purpose for a drill that goes over his head, till the way under it is clear
    bool Ducked() const
    {
        return mSweepDuck;
    }
    // Crouched by a UXB on the way: whoever runs this walk leaves Abe down
    bool Disarming() const
    {
        return mDisarm != nullptr;
    }
    void SetTopGait(Gait gait)
    {
        mTopGait = gait;
    }
    // Run whatever is about: for getting away from what has already seen or heard him
    // Seen, every step of it is sneaked: for a wait with Fleeches about that have taken no notice of him
    void SetQuiet(bool quiet)
    {
        mQuiet = quiet;
    }
    // Frames he spends where he is before the leap out of a cover that is timed by the Sligs in the background
    void SetZHold(s32 frames)
    {
        mZHold = frames;
    }
    void SetHurry(bool hurry)
    {
        mHurry = hurry;
    }
    // With Mudokons in tow: by a way they can come along (they walk and drop off ledges, nothing else), never
    // far ahead of them, never faster than a walk
    void SetLeading(bool leading)
    {
        mLeading = leading;
        if (leading)
        {
            mTopGait = Gait::eWalk;
        }
    }

private:
    // What GoToTask::Next has found by the time it comes to the step along the edge in hand
    struct Way final
    {
        const AbeState& mAbe;
        NavGraph& mNav;
        s32 mCur;
        std::vector<s32>& mPath;
        bool mAtGoal;
        bool mLed;
        bool mZOther;
        std::size_t mSweepStop;
        bool mSweepWalk;
        std::size_t mSweepRoll;
        bool mWasDucked;
        const NavEdge& mFirst;
        bool mOverGap;
    };
    // The step along each kind of edge
    TaskStep RollStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep RunJumpStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep WalkStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep FallStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep HopStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep ClimbStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep HoistDownStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep PortalStep(Bot& bot, const std::string& lastFailed, const Way& way);
    TaskStep TeleportStep(Bot& bot, const std::string& lastFailed, const Way& way);
    std::string mName;
    std::function<bool(s32)> mIsGoal;
    Gait mTopGait = Gait::eRun;
    bool mQuiet = false;
    s32 mLastEdge = -1;
    s32 mReflexes = -1;
    bool mHurry = false;
    // Turned away from a running jump with a one-cell run-up, so that its run starts out of the turn
    bool mJumpTurned = false;
    // The frames waited at the top of a hoist for the Sligs below to walk away
    s32 mDropWait = 0;
    s32 mLastFrom = -1;
    s32 mSameSpot = 0;
    s32 mOffGraph = 0;
    s32 mSteps = 0;
    bool mWaited = false;
    // Frames spent waiting for a Slig to look the other way
    s32 mHiding = 0;
    // How long the nearest Slig of his floor has stood with its face to him (the way out of a shadow)
    s32 mNearStood = 0;
    // A Slig that stands (where, and the way it faces): the frame it was first seen to, and the last it was
    struct Stood final
    {
        s32 mX = 0;
        bool mLeft = false;
        u32 mSince = 0;
        u32 mLast = 0;
    };
    std::vector<Stood> mStood;
    // AE, a screen with Sligs that shoot from its background: the next move between its covers, if there is one
    // to make now
    bool ZWatched(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, TaskStep& step);
    bool ZMove(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, std::size_t far, TaskStep& step);
    // The same walked, from a cover he can't crouch in (on a lift, at the top of a hoist: down is something else there)
    bool ZWalk(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, std::size_t far, TaskStep& step);
    // The way out of a cover is a hop, a drop or a hoist: it waits there for the frame to make it on
    // (`lead`: the cells of floor before it, rolled if he is crouched and walked if he stands)
    bool ZLeap(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, std::size_t lead, TaskStep& step);
    // Such a way was made and the cover it was for is not reached yet
    bool mZLeapt = false;
    // Frames such a way has been held back for flying mines over it
    s32 mZBombWait = 0;
    // Where such a way lands with another to make from there, and the cover a step or two aside of it to go to
    // first (0: none)
    s32 mZSideAt = -1;
    // The cover he is in and the frame he came into it on
    s32 mZCoverCur = -1;
    s32 mZArrived = 0;
    // Leaps out of the cover he is in that would get him shot, left out of the way while another is looked for,
    // and none other found: the best of them is made
    std::set<s32> mZAvoid;
    bool mZSettle = false;
    s32 mZBest = -1;
    s32 mZBestBursts = 0;
    // The frame the leap was last timed on
    s32 mZPlanned = 0;
    s32 mZSideX = 0;
    // The frames waited in cover for them with no period known, and what was last said of it
    s32 mZWaits = 0;
    s32 mZSaid = -1;
    std::string mZWhat;
    // The next roll as last worked out: for which cell, cover and periods, the frame to make it on and where to
    s32 mZFrom = -1;
    s32 mZPeriods = 0;
    s32 mZFar = 0;
    s32 mZGo = 0;
    s32 mZHold = 0;
    // The cell a leap ends at under a cover that only hides him crouched (as worked out, and as gone for)
    s32 mZLowLand = -1;
    s32 mZCrouchAt = -1;
    // With no beat known: the frames waited for each Slig of the background to walk away
    s32 mZAwayWaits = 0;
    s32 mZTo = 0;
    // Chants at a bird portal that has not opened yet
    s32 mPortalChants = 0;
    // Times he was sent to two cells before an open portal
    s32 mPortalBacks = 0;
    // Tries at a teleporter that goes off when its plate is stood on
    s32 mPlateSteps = 0;
    // Steps spent waiting for a Slurg to get out of the way
    s32 mSlurgWaits = 0;
    s32 mHopWaits = 0;
    // Frames waited in a shadow for the Sligs of its floor to walk away
    s32 mShadowWaits = 0;
    // Frames a move through Sligs' sight was put off for a Greeter's beam
    s32 mGreeterWaits = 0;
    // For each Slig about (as World::mSligs has them): the frame it last did anything but stand
    std::vector<s32> mSligMoved;
    s32 mHopFaced = -1;
    s32 mFallerWaits = 0;
    s32 mFallerLeft = -1;
    s32 mDrillWaits = 0;
    s32 mSlogHangs = 0;
    s32 mHopOverAt = -1;
    bool mFallerUnder = false;
    bool mFleechAwakeHere = false;
    s32 mLandWaits = 0;
    // Steps spent at a door waiting for a lock's ghost to open it
    s32 mGhostWaits = 0;
    // Steps spent under a hoist until the Slig that walks the floor over it has its back to him
    s32 mSligWaits = 0;
    // The frames until a Slig that walks the level of this cell can be on its screen with its face to it (9999:
    // none does), from where it is, the way it goes and the ends of its beat
    // (withStand: a Slig that walks away is taken to stand at the end it walks to as long as its beat says at least)
    static s32 SligComes(const Bot& bot, const NavGraph& nav, const NavNode& at, bool withStand = false);
    // Under a ledge a Slig stands on: whether it has been seen with its face to him, has its back to him now and
    // since which frame, the waits for it to turn, and the times he went back to a shadow from there
    bool mLedgeToward = false;
    bool mLedgeAway = false;
    u32 mLedgeSince = 0;
    s32 mLedgeWaits = 0;
    s32 mLedgeRetreats = 0;
    // The last cell he was in that is in a shadow, and the way back to it
    s32 mSafeNode = -1;
    std::unique_ptr<GoToTask> mBack;
    // Waits at a teleporter for the Slig of the floor it goes to to leave
    s32 mTeleWaits = 0;
    // Waits for a Slig that faces where a drop on his way lands to turn or go
    s32 mFacingWaits = 0;
    // Waits for a Scrab loose under a ledge he is about to hang from to be gone from under it
    s32 mScrabUnderWaits = 0;
    // Waits over a floor a Slig walks for it to leave before he drops onto it
    s32 mFallWaits = 0;
    // The flying Slig of the screen he is about to drop onto out of a tunnel, and the frames waited for it to
    // have gone by under the tunnel's end
    FlierWatch mFlierWatch;
    s32 mFlierWaits = 0;
    static bool FlierUnderFall(const Bot& bot, const NavGraph& nav, s32 edge);
public:
    // AE: what a thing that falls over and over (FallingItem) says of the way ahead from the cell cur: go, wait
    // where he is, or walk to the cell beside where it comes down and wait there. His box's sides, whether he
    // stands settled, the frames it had left when last asked (-1: not asked) and how long he has waited.
    struct FallerCall final
    {
        enum Kind
        {
            eGo,
            eWait,
            eBeside
        };
        Kind mKind = eGo;
        // He stands under one: nothing is waited for, and he is to be out from under it at a run
        bool mUnder = false;
        // Go, because it is down before his box is in under it / because it has just smashed
        bool mAllButDown = false;
        bool mSmashed = false;
        s32 mBeside = 0;
        s32 mLeft = 0;
        s32 mNeed = 0;
        s32 mEnters = 0;
        s32 mFar = 0;
        const RockAt* mWhat = nullptr;
    };
    static FallerCall Faller(const NavGraph& nav, const World& world, const std::vector<s32>& path, s32 cur, s32 boxX1, s32 boxX2, bool settled, s32& lastLeft, s32 waits);
    // A Slig in the background of the screen of (x, y) that shoots: awake, not his
    // The way ahead is a walk in under such a thing and out again within a hop's length (three cells) to a cell
    // clear of it: it is hopped over from here ("hop over the impact point of the right-most falling crate")
    static bool HopsOver(const NavGraph& nav, const World& world, const std::vector<s32>& path);
    // The walk ahead goes in under such a thing some cells on: the cell to stop at before it (the last one clear of
    // it), where what to do about it is decided. -1: no such thing on the walk, or he is at that cell.
    static s32 StopBefore(const NavGraph& nav, const World& world, const std::vector<s32>& path);
    // A hoist up at x from the floor at footY to the ledge at ledgeY: some drill is in what he goes through within
    // the frames that takes. unknownHits: with no drill to be seen at all, yes (there is one, by the map)
    // frames: within so many, where it is not the hoist's time
    static bool PastDrills(const World& world, s32 x, s32 footY, s32 ledgeY, bool half, bool unknownHits, s32 frames = 0);
    // Hung from the ledge at (x, y) for so many frames: to stay hung, for a Slog that is on that floor or asleep by it
    static bool HungBySlog(const World& world, s32 x, s32 y, bool half, s32 hung);
    // At a run from Slogs with a drill on its way to the cell he is in: no waiting there
    static bool DrivenOn(const Bot& bot, s32 node);
    // A drill that a switch turns reaches that cell and stands still as things are
    static bool DormantDrillOver(const World& world, s32 x, s32 y, bool half);
    static bool ZShooterAbout(const NavGraph& nav, const World& world, s32 x, s32 y);
    // The hop down to a Slig's level: now, on a back it has just turned (it faced him no more than 8 frames ago).
    // faced: the frame it last faced where he lands, -1 for never seen to
    static bool HopOnBack(bool faces, s32 frame, s32& faced);
    // The hurry of Bot::RunFor: the frame it lasts to when asked for at a frame (no longer than a few seconds: a
    // Slog's eating time is what its map says), and whether it is on (only in the path it was asked in: with three
    // hundred of it he was still at a run in the next path, off a ledge under falling crates)
    static s32 HurryUntil(s32 frame, s32 frames)
    {
        return frame + std::min(frames, 200);
    }
    static bool HurryOn(s32 frame, s32 until, s32 askedPath, s32 path)
    {
        return frame < until && askedPath == path;
    }


private:
    // The Slig on his level and screen that would hear him where he is hidden: the way it faced when last looked
    // at (-1: none there), the frame it turned that way, and whether that turn was seen (not just found so)
    s32 mHushFacing = -1;
    u32 mHushSince = 0;
    bool mHushKnown = false;
    bool mHushLogged = false;
    bool mChaseLogged = false;
    s32 mLureWaits = 0;
    // Frames waited for a Slog under the ledge to leap up, and whether it was in the air the frame before
    s32 mSlogWaits = 0;
    bool mSlogUp = false;
    // Frames waited for a lift that is on its way to the stop he would walk onto, and where it was last
    // Pulling the lever that sends a lift to the stop the way goes onto
    std::unique_ptr<Task> mLiftCall;
    s32 mLiftCalls = 0;
    s32 mLiftComeWaits = 0;
    s32 mLiftComeY = -100000;
    s32 mUnseenWaits = 0;
    // Chants made to go invisible
    s32 mInvisChants = 0;
    s32 mInvisWaits = 0;
    // Frames spent waiting for a drill to be out of the way
    s32 mSweepWait = 0;
    // Frames waited for a drill in a gap to be clear of a running jump over it
    s32 mJumpDrillWaits = 0;
    // Frames waited for a door he is on his way through before it is judged
    s32 mDoorWaits = 0;
    // Frames a hop or a drop that comes down under a thing that falls was put off for it
    s32 mHopFallerWaits = 0;
    // Pulling the lever that stops the drill in the way
    std::unique_ptr<Task> mStop;
    s32 mStops = 0;
    bool mTrapTried = false;
    s32 mRetreats = 0;
    bool mLeading = false;
    // Crouched before a drill, to roll through under it
    bool mSweepDuck = false;
    // Flying mines: frames spent watching them and waiting for them, and crouched where one goes over his head
    s32 mBombWatch = 0;
    s32 mBombWait = 0;
    bool mBombDuck = false;
    // A Slig taken over and walked into a flying mine that is in the way (once a walk)
    std::unique_ptr<Task> mBombSlig;
    bool mBombSligTried = false;
    // The cell he is set to go on from when the mines were out of the way, and until when
    s32 mBombUp = -1;
    u32 mBombUpAt = 0;
    bool BombStep(Bot& bot, s32 cur, const std::vector<s32>& path, std::size_t& stop, std::size_t& roll, TaskStep& step);
    s32 mHangFails = 0;
    // Hung under a Slig that stands guard: the frames waited for it to turn its back, and whether that was said
    s32 mHangWaits = 0;
    bool mHangSaid = false;
    s32 mLiftGlide = 0;
    // Where the lift was a step ago, and the steps it has stood there between two stops
    s32 mLiftGlideY = -100000;
    s32 mLiftStood = 0;
    s32 mLeadWaits = 0;
    std::unique_ptr<DisarmTask> mDisarm;
    s32 mDisarms = 0;
    // After a drop with Mudokons in tow: the cell a few on from the ledge to wait in. They only walk off it after
    // Abe when he is more than two cells away, and lose interest if he stands under them for long.
    s32 mAwayNode = -1;
    s32 mAwayTries = 0;
};

// Pull a lever or a ring, or turn a wheel, until its switch is what was wanted
class UseTask final : public Task
{
public:
    UseTask(u32 entityId, s32 wantValue)
        : mEntity(entityId)
        , mWant(wantValue)
    {
    }
    std::string Name() const override
    {
        return "Use #" + std::to_string(mEntity);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // The switch he has just set is the input of a clock (TimerTrigger) whose output has not followed yet: how
    // many frames it takes at most, 0 when nothing is owed. Such a clock runs only while its screen is shown: the
    // boilers of Bonewerkz, "turn the wheel ... then wait until the door opens".
    static s32 ClockOwed(const World& world, s32 sw, s32 want);
    // A lever: the side to stand on, if there is room there (-1 left of it, 1 right)
    void SetSide(s32 side)
    {
        mSide = side;
    }
    // The side to use it from, whatever side he comes from
    void FixSide(s32 side)
    {
        mSide = side;
        mSideChosen = true;
    }
    // What to hold as he lands, where using it drops him (a lock that opens the trap door he stands on)
    void SetAfter(u32 bits)
    {
        mAfter = bits;
    }

private:
    bool mMissLogged = false;
    s32 mScrabWaits = 0;
    u32 mEntity;
    s32 mWant;
    s32 mSide = -1;
    bool mSideChosen = false;
    u32 mAfter = 0;
    std::unique_ptr<GoToTask> mGo;
    bool mArrived = false;
    s32 mTries = 0;
    bool mLooking = false;
    s32 mLookTaps = 0;
    s32 mDucks = 0;
    s32 mClockWaits = 0;
    s32 mLiftWaits = 0;
};

// Get a Mudokon to follow: go to it, hello, sorry or a slap if its mood needs it, follow me
class RecruitTask final : public Task
{
public:
    // calm: also out of its laughing gas daze, which it follows Abe in but won't work in
    explicit RecruitTask(u32 mudId, bool calm = false)
        : mMud(mudId)
        , mCalm(calm)
    {
    }
    std::string Name() const override
    {
        return "Recruit Mudokon#" + std::to_string(mMud);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // The side of it to talk from (-1 left, 1 right, 0 either): a blind one walks that way
    void SetSide(s32 side)
    {
        mSide = side;
    }
    // Talked to from where Abe is, whatever floor that is: it hears him anywhere on its screen
    // Hello is said to all of them at once ("all o' ya")
    void SetAll(bool all)
    {
        mAll = all;
    }
    void SetRemote(bool remote)
    {
        mRemote = remote;
    }
    // Done once one that works a lever in anger has left it: no follow me
    void SetSorryOnly(bool only)
    {
        mSorryOnly = only;
    }
    // Its way to the Mudokon has him crouched (under a drill, in a tunnel): he stays down
    bool Ducked() const
    {
        return mGo && (mGo->Ducked() || mGo->Disarming());
    }

private:
    u32 mMud;
    bool mCalm;
    s32 mSide = 0;
    bool mRemote = false;
    bool mSorryOnly = false;
    bool mAll = false;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<DisarmTask> mDisarm;
    s32 mDisarms = 0;
    bool mSaidHello = false;
    bool mSaidFollow = false;
    s32 mSinceLine = 0;
    s32 mSlaps = 0;
    s32 mTalks = 0;
    s32 mHeld = 0;
    s32 mCloseIns = 0;
    s32 mWaits = 0;
};

// Talk a Mudokon into following, lead it to a bird portal and chant it through
class RescueTask final : public Task
{
public:
    RescueTask(u32 mudId, u32 portalId)
        : mMud(mudId)
        , mPortal(portalId)
    {
    }
    std::string Name() const override
    {
        if (mOffDrill)
        {
            return "Lead Mudokon#" + std::to_string(mMud) + " from under Drill#" + std::to_string(mOffDrill);
        }
        return "Rescue Mudokon#" + std::to_string(mMud) + " via BirdPortal#" + std::to_string(mPortal);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // Only as far along its way to the portal as takes it out from under the drill, and left there
    void LeadOffDrill(u32 drillId)
    {
        mOffDrill = drillId;
    }
    // Others standing with it go too ("all o' ya"): the place behind the car is that many cells further back
    void LeadGroup(s32 others)
    {
        mGroup = others;
    }

private:
    // A blind Mudokon is walked to the portal's floor a straight stretch at a time: talked to from the side it
    // is to walk to, told to wait where the stretch ends, and dropped through the trap door it stops on.
    // True once it is there, else step is what to do.
    bool LeadBlind(Bot& bot, const Entity& mud, const Entity& portal, const std::string& lastFailed, TaskStep& step);

    // Led off far enough: out from under the drill, or behind the mine car (a cell past it, away from where it was)
    bool OffDone(const Entity& off, const Entity& mud, s32 x, s32 y)
    {
        if (off.mKind == EntityKind::eMineCar)
        {
            const s32 carX = off.mLive ? off.mLiveX : off.CentreX();
            return mBehindDir != 0 && (x - carX) * mBehindDir <= -NavGraph::GridStep(mud.mHalf);
        }
        return !Domain::UnderDrill(off, mud, x, y);
    }

    u32 mMud;
    u32 mPortal;
    u32 mOffDrill = 0;
    s32 mGroup = 0;
    s32 mBehindDir = 0;
    s32 mCarHeld = 0;
    std::unique_ptr<GoToTask> mLedgeGo;
    bool mLedgeGone = false;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<RecruitTask> mRecruit;
    std::unique_ptr<UseTask> mUse;
    s32 mRecruits = 0;
    s32 mPhase = 0;
    s32 mWaits = 0;
    s32 mRespawns = 0;
    bool mBlindThere = false;
    bool mNoStand = false;
    // Pulling the lever of a drill that is between a blind Mudokon and its portal
    std::unique_ptr<UseTask> mDrillStop;
    s32 mBlindPhase = 0;
    s32 mBlindStop = -1;
    s32 mBlindDir = 0;
    s32 mBlindTrap = -1;
    s32 mBlindRide = -1;
    s32 mBlindTries = 0;
    s32 mBlindWaits = 0;
    s32 mBlindLastX = 0;
    s32 mBlindStill = 0;
    bool mBlindAhead = false;
};

// A Slig walks a floor under a drill that comes down once when its ring is pulled: the ring is pulled as the Slig
// walks in under it
class TrapSligTask final : public Task
{
public:
    TrapSligTask(u32 ringId, u32 drillId)
        : mRing(ringId)
        , mDrill(drillId)
    {
    }
    std::string Name() const override
    {
        return "Drop Drill#" + std::to_string(mDrill) + " on the Slig";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // The ring and the drill to do it with for a Slig, where the ring is pulled from a floor the Slig isn't on
    static bool Find(const NavGraph& nav, const World& world, const SligAt& slig, u32& ring, u32& drill);

private:
    // With Mudokons to save, a blind one under the drill is called out from under it first: from the ring, while
    // the Slig is off past the drills, and told to wait once it is clear. True while that is going on.
    bool CallMudOff(Bot& bot, s32 x1, s32 x2, const SligAt* slig, TaskStep& step);

    u32 mRing;
    u32 mDrill;
    std::unique_ptr<GoToTask> mGo;
    s32 mPhase = 0;
    s32 mWaits = 0;
    s32 mPulls = 0;
    s32 mLastX = 0;
    s32 mCallWaits = 0;
    s32 mSinceLine = 1000;
    bool mSaidHello = false;
    // The one called, until it has been told to wait
    u32 mCalled = 0;
};

// A blind Mudokon is called from the wheel's side of it, walks until the wheel stops it, and is told to work
class SendToWheelTask final : public Task
{
public:
    SendToWheelTask(u32 mudId, u32 wheelId)
        : mMud(mudId)
        , mWheel(wheelId)
    {
    }
    std::string Name() const override
    {
        return "Send Mudokon#" + std::to_string(mMud) + " to wheel #" + std::to_string(mWheel);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mMud;
    u32 mWheel;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<RecruitTask> mRecruit;
    s32 mPhase = 0;
    s32 mWaits = 0;
    s32 mLastX = 0;
    s32 mStill = 0;
    s32 mSaid = 0;
};

// Wheels that only count turned together: Mudokons are brought along, stop at the wheels on the way to Abe's, are
// told to work, and Abe turns the last one
class WorkTogetherTask final : public Task
{
public:
    explicit WorkTogetherTask(u32 logicId)
        : mLogic(logicId)
    {
    }
    std::string Name() const override
    {
        return "Work together for SwitchLogic#" + std::to_string(mLogic);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLogic;
    Team mTeam;
    bool mPlanned = false;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<RecruitTask> mRecruit;
    u32 mRecruiting = 0;
    s32 mRecruits = 0;
    s32 mStage = 0;
    s32 mWaits = 0;
    s32 mStill = 0;
    s32 mLastSum = 0;
    s32 mRounds = 0;
    s32 mTurns = 0;
    u32 mMine = 0;
    bool mSaidWork = false;
    // Levers: a Mudokon is led to the cell its lever is pulled from and told to wait there; then Abe's own, with
    // "Work" said as he pulls
    TaskStep Levers(Bot& bot, const std::string& lastFailed);
    std::unique_ptr<Task> mUse;
    s32 mLeverStage = 0;
    s32 mPlaceWaits = 0;
    s32 mLastMudX = 0;
};

// Slap a Mudokon until it is dead
class KillTask final : public Task
{
public:
    explicit KillTask(u32 mudId)
        : mMud(mudId)
    {
    }
    std::string Name() const override
    {
        return "Kill Mudokon#" + std::to_string(mMud);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mMud;
    std::unique_ptr<GoToTask> mGo;
    s32 mSlaps = 0;
    s32 mCloseIns = 0;
    s32 mWaits = 0;
    bool mNoStand = false;
    bool mPause = false;
    s32 mHeld = 0;
};

// Another task, with any Slig that watches the place it works at got rid of first: Abe takes it over from a floor
// it can't see him on and blows it up
class ClearSligTask final : public Task
{
public:
    // shootIt: the entity is a Mudokon to be killed, which the Slig can do as well
    ClearSligTask(u32 entity, std::function<std::unique_ptr<Task>()> make, bool shootIt = false)
        : mEntity(entity)
        , mShootIt(shootIt)
        , mMake(std::move(make))
        , mInner(mMake())
    {
    }
    // A Slig that has woken next to Abe (he came down beside it): chant where he stands, shoot what else is on
    // its floor with it, burst it. Nothing is done after.
    static std::unique_ptr<ClearSligTask> Ambush()
    {
        auto task = std::make_unique<ClearSligTask>(0, []() -> std::unique_ptr<Task>
        {
            return nullptr;
        });
        task->mAmbush = true;
        return task;
    }
    // A Slig on the floor a flying mine comes down to: chant where he stands (out of reach of the blast), and
    // walk it into the mine. Both are gone. (The walkthrough's way with the Barracks' workers' campus: "possess
    // the sleeping Slig. Use him to blow up the Flying Mine.")
    static std::unique_ptr<ClearSligTask> IntoMine(u32 mine)
    {
        std::unique_ptr<ClearSligTask> task = Ambush();
        task->mMine = mine;
        return task;
    }
    ~ClearSligTask() override;
    std::string Name() const override
    {
        return mInner ? mInner->Name() : mMine ? "Walk a Slig into the flying mine" : "Take over the Slig that woke";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // The Slig that would see Abe working at (x, y), or null. Asleep ones and ones not about don't count.
    static const SligAt* Watcher(const World& world, s32 x, s32 y, bool half);
    // A shot of the Slig he is in gets to x on its floor: it stops at the edge of the screen shown (Bullet.cpp)
    static bool ShotReaches(const NavGraph& nav, const SligAt& me, s32 targetX);
    // What to hold as the Slig "me" to shoot what is at x on its floor, or 0 when x is null: nothing left to
    // shoot, time to burst
    static u32 AsSlig(const SligAt& me, const s32* x);
    // Where the next thing to shoot is: another Slig on its floor, then the Mudokon (if it is to die)
    static bool Target(const World& world, const SligAt& me, const Entity* mud, s32& x);
    // Nothing that is to live stands in the way of a shot along the floor at what is at targetX: a Mudokon that
    // has ducked is under it. toDie is the Mudokon that may be hit, or 0
    static bool LevelShotClear(const World& world, const SligAt& me, s32 targetX, u32 toDie);
    // A shot out of the screen from the floor behind would hit what is at targetX (and will be at aheadX when it
    // goes off), not a Mudokon in front
    static bool BehindShotClear(const World& world, const SligAt& me, s32 targetX, s32 aheadX);
    // The teleporter a controlled Slig on the floor from x1 to x2 at y takes next: one that leads to a floor not
    // in seen. Null when there is none. dest is the one it comes out of.
    static const Entity* NextTeleporter(const NavGraph& nav, const World& world, s32 x1, s32 x2, s32 y, bool half, const std::vector<s32>& seen, const Entity** dest);

private:
    u32 mEntity;
    bool mShootIt;
    s32 mShootFrames = 0;
    std::function<std::unique_ptr<Task>()> mMake;
    std::unique_ptr<Task> mInner;
    std::unique_ptr<GoToTask> mGo;
    // 0 the task itself, 1 to the spot, 2 chanting, 3 in the Slig
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mCleared = 0;
    // A raid: a Slig that can be got at is taken over somewhere else and brought here by teleporter, to shoot
    // from the floor behind
    bool FindRaid(Bot& bot, std::function<bool(s32)>& spot) const;
    TaskStep RaidStep(Bot& bot, const SligAt& me);
    bool mRaid = false;
    std::vector<s32> mRaidLines;
    s32 mRaidLine = -1;
    s32 mFloorFrames = 0;
    s32 mPullTries = 0;
    s32 mPullWait = 0;
    bool mTapped = false;
    // Where the Slig to shoot from behind was a step ago, for where it will be when the shot lands
    s32 mLastTargetX = 0;
    bool mHasLastTarget = false;
    // "Look out" said to a Mudokon in the line of fire, and the frames to wait for it to duck
    s32 mLookOuts = 0;
    s32 mLookOutWait = 0;
    u32 ShootLevel(const World& world, const SligAt& me, s32 targetX, s32 frames);
    bool mGaveUp = false;
    bool mAmbush = false;
    // The flying mine the Slig is walked into (0: none), and the frames it has been walked for
    u32 mMine = 0;
    s32 mMineFrames = 0;
    bool mBehind = false;
    // The power to go unseen kept for the last cell before the Slig's beat: 0 not looked at, 1 to the last cell
    // off its level, 2 waiting there for its back, 3 to that cell, 4 chanting, 5 done with
    s32 mPerch = 0;
    s32 mPerchNode = -1;
    s32 mFootNode = -1;
    s32 mPerchFrames = 0;
    s32 mPerchWaits = 0;
    s32 mPerchStand = 0;
    Bot* mHeld = nullptr;
    // A sortie, when the Slig can't be taken over (a security orb) but leaves the thing's screen on its beat: to
    // a place out of its sight right by the thing (a ledge over its floor, a shadow), out to the thing once the
    // Slig has walked off the screen, and back there. 0 not looked at, 1 to that place, 2 waiting there, 3 out
    // to the thing, 4 back, 5 done
    TaskStep Sortie(Bot& bot, const std::string& lastFailed, s32 x, s32 y, bool half);
    s32 mSortie = 0;
    s32 mSortieNode = -1;
    // The frame a Slig that stays on the screen last faced where he waits (-1: not seen to yet)
    s32 mSortieFaced = -1;
    s32 mSortieWaits = 0;

public:
    // An awake Slig of the floor at (x, y) as it is to someone on that screen (AE): 0 nothing to him (asleep,
    // taken over, another floor), 1 on the screen, 2 off it but coming or standing, 3 off it and walking away:
    // it neither sees nor hears what is on the screen until it is back
    static s32 SligAbout(const NavGraph& nav, const SligAt& slig, s32 x, s32 y, bool half);
    // The place for a sortie to the thing at node `use`: the nearest cell by it (three moves at most) that a
    // Slig on that floor doesn't see, on that screen. -1: none.
    static s32 Haven(const NavGraph& nav, const NavGraph::EdgeCost& cost, s32 use);
};

// A floor of Scrabs cleared with one of them: taken over from a ledge, it kills the other Scrabs (a howl gives it
// the power to spin, and what its box is on while it spins dies), stamps on the Fleeches, and is walked over the
// foot switch that shuts a gate behind it before it is let go
class ScrabRampageTask final : public Task
{
public:
    ScrabRampageTask(u32 scrab, s32 spot)
        : mScrab(scrab)
        , mSpot(spot)
    {
    }
    std::string Name() const override
    {
        return "Clear the floor with Scrab #" + std::to_string(mScrab);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mScrab;
    s32 mSpot;
    // 0 to the ledge, 1 chant, 2 hunt, 3 to the trap, 4 let go
    s32 mPhase = 0;
    s32 mFrames = 0;
    // The entity of the Scrab he is in
    u32 mSelf = 0;
    bool mCharged = false;
    s32 mAttacks = 0;
    // Fleeches that got away up a ledge, and runs made at one
    std::vector<u32> mLeft;
    s32 mFleechRuns = 0;
    // 0 not asked for, 1 asked, 2 howling
    s32 mHowl = 0;
    s32 mHowlWait = 0;
    s32 mStamps = 0;
    s32 mTrapX = 0;
    s32 mTrapSwitch = 0;
    std::unique_ptr<GoToTask> mGo;
};

// A Scrab taken over from a ledge and used on a Fleech: the gate between them opened by the lever at hand, the
// Scrab walked to the Fleech and made to stamp on it, walked home again and let go, the gate shut
class ScrabKillTask final : public Task
{
public:
    ScrabKillTask(u32 fleech, u32 scrab, u32 lever)
        : mFleech(fleech)
        , mScrab(scrab)
        , mLever(lever)
    {
    }
    std::string Name() const override
    {
        return "Scrab #" + std::to_string(mScrab) + " kills Fleech #" + std::to_string(mFleech);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mFleech;
    u32 mScrab;
    u32 mLever;
    // 0 open the gate, 1 chant, 2 to the Fleech, 3 stamp, 4 home, 5 let go, 6 shut the gate
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mStamps = 0;
    std::unique_ptr<Task> mUse;
};

// A Scrab taken over from a place on its screen and walked to the Fleeches on the floor of its floor, each stamped
// on. It is left where a gate can be shut on it with the least in its way: at home, or where the Fleeches were
// if that stretch has fewer ways on and off it.
class ScrabHuntTask final : public Task
{
public:
    ScrabHuntTask(u32 scrab, s32 spot)
        : mScrab(scrab)
        , mSpot(spot)
    {
    }
    // (the Fleeches led to it: the ones that are by it when it is taken over, not the ones the map puts there)
    ScrabHuntTask(u32 scrab, s32 spot, std::vector<u32> led)
        : mScrab(scrab)
        , mSpot(spot)
        , mLed(true)
        , mGroup(std::move(led))
    {
    }
    ~ScrabHuntTask() override;
    std::string Name() const override
    {
        return "Scrab #" + std::to_string(mScrab) + " hunts the Fleeches of its floor";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mScrab;
    s32 mSpot;
    // 0 to the spot, 1 chant, 2 hunt, 3 to where it is left, 4 let go
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mStamps = 0;
    s32 mWaits = 0;
    s32 mLine = -1;
    s32 mFloorY = 0;
    s32 mHomeX = 0;
    s32 mLeaveX = 0;
    bool mParked = false;
    bool mCharged = false;
    s32 mHowl = 0;
    s32 mHowlWait = 0;
    s32 mAttacks = 0;
    bool mWrong = false;
    bool mSet = false;
    bool mLed = false;
    s32 mNone = 0;
    std::vector<u32> mGroup;
    std::unique_ptr<GoToTask> mGo;
};

// A ring only a Paramite gets to (it hangs over a floor Abe has no way onto, or one he would not come back from):
// a Paramite of that floor is taken over from a place on its screen that is not on its level, walked under the
// ring, made to jump at it (up: it hangs on and the ring's switch is set as it lets go), and let go. It lives.
// (the walkthrough's way through the Mudomo vaults: "possess the Paramite below you and have him pull the ring,
// then release him")
class ParamitePullTask final : public Task
{
public:
    ParamitePullTask(u32 ring, u32 paramite, s32 spot)
        : mRing(ring)
        , mParamite(paramite)
        , mSpot(spot)
    {
    }
    std::string Name() const override
    {
        return "Paramite #" + std::to_string(mParamite) + " pulls ring #" + std::to_string(mRing);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mRing;
    u32 mParamite;
    s32 mSpot;
    // 0 to the spot, 1 chant, 2 under the ring, 3 pull, 4 let go, 5 himself again
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mPulls = 0;
    s32 mWas = -1;
    std::unique_ptr<GoToTask> mGo;
};

// A Brew drunk at the machine, to have the fart it gives somewhere else
class DrinkTask final : public Task
{
public:
    explicit DrinkTask(u32 brew)
        : mBrew(brew)
    {
    }
    ~DrinkTask() override;
    std::string Name() const override
    {
        return "Drink at #" + std::to_string(mBrew);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mBrew;
    s32 mDrinks = 0;
    std::unique_ptr<GoToTask> mGo;
};

// A Brew drunk at the machine, the fart it gives taken over and flown to a mine or a Slig (FartFlight), and set
// off there
class FartBombTask final : public Task
{
public:
    // brew 0: with the Brew he has in him, from the cell "from"
    FartBombTask(u32 target, u32 brew, s32 from = -1)
        : mTarget(target)
        , mBrew(brew)
        , mFrom(from)
    {
    }
    ~FartBombTask() override;
    std::string Name() const override
    {
        return "Fly a fart to #" + std::to_string(mTarget);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mTarget;
    u32 mBrew;
    s32 mFrom;
    // Where the fart waits for a flying Slig: the end of its track nearest to where it was made
    s32 mParkX = 0;
    s32 mParkY = 0;
    bool mParked = false;
    // Parked between two flying Sligs' tracks, to get both
    bool mBoth = false;
    s32 mSinceBlast = 0;
    // 0 to the machine and drink, 1 fart, 2 chant, 3 fly, 4 chant it off, 5 wait for Abe
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mFarts = 0;
    s32 mDrinks = 0;
    s32 mAfter = 0;
    std::vector<std::pair<s32, s32>> mWay;
    std::size_t mCorner = 0;
    std::unique_ptr<GoToTask> mGo;
};

// A Slig dropped through a trap door: at the door's lever until the Slig is on the door, then the lever.
class SligDropTask final : public Task
{
public:
    SligDropTask(u32 lever, u32 slig, s32 door)
        : mLever(lever)
        , mSlig(slig)
        , mDoor(door)
    {
    }
    ~SligDropTask() override;
    std::string Name() const override
    {
        return "Drop Slig #" + std::to_string(mSlig) + " through the trap door";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLever;
    u32 mSlig;
    s32 mDoor;
    s32 mFrames = 0;
    s32 mPulls = 0;
    s32 mAfter = 0;
    // Where the Slig was a frame ago, the frames a pull takes to open the door (measured on the first, 16 until
    // then), and the frame and the switch's value as the pull under way was begun
    s32 mLastX = 0;
    s32 mLastFrame = 0;
    s32 mDelay = 16;
    s32 mPulledAt = 0;
    s32 mWas = 0;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<UseTask> mUse;
};

// Sligs he can't take over, got rid of with a flying Slig: from a cell where he isn't seen he takes over the one
// its spawner makes, flies it (FlierFlight) to a place off each Slig's level and throws a grenade at it, then
// bursts it. A flying Slig that is shot is not the end: the spawner makes another as he stands there.
class FlierBombTask final : public Task
{
public:
    // lever: not for the Sligs, but to pull that lever with it
    // lock: nor for a lever, but to say that Slig lock's code back to it
    FlierBombTask(u32 spawner, s32 spot, u32 lever = 0, u32 lock = 0)
        : mSpawner(spawner)
        , mSpot(spot)
        , mLever(lever)
        , mLock(lock)
    {
    }
    ~FlierBombTask() override;
    std::string Name() const override
    {
        return mLock ? "Open SligLock#" + std::to_string(mLock) + " with the flying Slig of #" + std::to_string(mSpawner) : mLever ? "Pull #" + std::to_string(mLever) + " with the flying Slig of #" + std::to_string(mSpawner) : "Bomb the Sligs with the flying Slig of #" + std::to_string(mSpawner);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    // A Slig on a level the way goes through was seen to face this way (its back since is all ahead), and the
    // frames waited for one that only showed its back
    bool mSawFace = false;
    s32 mBackWait = 0;
    u32 mSpawner;
    s32 mSpot;
    u32 mLever;
    u32 mLock;
    // For a lock: what was last said of another flying Slig on its screen (0 nothing, 1 behind it, 2 keeping
    // back, 3 thrown), and the grenades thrown at such
    s32 mFoeSaid = 0;
    // Another flying Slig on the next screen on the way, waited for from this one with a grenade: 0 none / 1 at the
    // place or on the way to it / 2 thrown at least once; the place; the frames of it; which track (the first
    // corner of its box) and the tracks done with
    s32 mAmbush = 0;
    s32 mAmbushX = 0;
    s32 mAmbushY = 0;
    s32 mAmbushFrames = 0;
    FlierWatch mPreyWatch;
    // (where it left its way for that, to go back to: 3 is on the way back)
    s32 mAmbushFromX = 0;
    s32 mAmbushFromY = 0;
    // Held (pushing at something) at one place: how often there, and the way out along its own track
    s32 mHeldTimes = 0;
    s32 mHeldX = 0;
    s32 mHeldY = 0;
    s32 mEscape = 0;
    s32 mEscapeX = 0;
    s32 mEscapeY = 0;
    s32 mEscapes = 0;
    // (and how long it has been no nearer where it is going, and how far that was)
    s32 mProgressFrames = 0;
    s32 mProgressLeft = 0;
    // (the box of its track: what is found of a track differs with where the flying Slig is on it, so two are
    // the same track where their boxes meet)
    std::array<s32, 4> mAmbushKey{};
    std::vector<std::array<s32, 4>> mAmbushed;
    s32 mFoeThrown = 0;
    // Where it last was with no other on its screen (to go back to out of the other's sight), and the other as
    // last seen from in front: its place, the way it faced, and whether it is to be come at from behind
    s32 mSafeX = 0;
    s32 mSafeY = 0;
    bool mHasSafe = false;
    s32 mFoeX = 0;
    bool mFoeLeft = false;
    bool mFoeRound = false;
    s32 mFoeRoundFrames = 0;
    // What it has said to the lock so far (SligLockTask::Say)
    bool mHi = false;
    s32 mSaid = 0;
    s32 mLockState = -1;
    // What the lever's switch was when this began
    bool mLeverKnown = false;
    s32 mLeverWas = 0;
    std::unique_ptr<GoToTask> mGo;
    // 0 Abe to the cell, 1 chanting, 2 flying, 3 bursting it, 4 Abe again
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mWaits = 0;
    s32 mAfter = 0;
    s32 mLost = 0;
    // When the last one was lost
    u32 mLostAt = 0;
    // The Sligs to get (Domain::FlierTargets), the frames until they are looked for again, and how many things
    // were known dead when they last were
    std::vector<Domain::FlierTarget> mTargets;
    s32 mTargetsIn = 0;
    std::size_t mKilled = 0;
    // The frames the flying Slig has been off his screen while he chants
    s32 mAway = 0;
    // The Slig it is after, which of the places to throw from, how often all of them have been gone through,
    // and the Sligs given up on
    u32 mTarget = 0;
    std::size_t mShot = 0;
    s32 mRounds = 0;
    std::vector<u32> mGivenUp;
    // 0 the way to where it waits to be found, 1 on the way, 2 waiting there for the moment, 3 to where it
    // throws from, 4 thrown: waiting for it to go off
    s32 mState = 0;
    s32 mStuck = 0;
    s32 mUnstick = 0;
    s32 mLastX = 0;
    s32 mLastY = 0;
    s32 mSince = 0;
    s32 mHeld = 0;
    // How much nearer the Slig than the place to throw from it throws this time (FlierFlight::Shot::mNearer)
    s32 mNearer = 0;
    // The direction buttons it held last frame
    u32 mLastHeld = 0;
    // The frames it has been on the run in to where it waits
    s32 mRunInFrames = 0;
    // The frames since the way to where it throws from was last found
    s32 mLegFrames = 0;
    std::vector<std::pair<s32, s32>> mWay;
    std::size_t mCorner = 0;
    // Where the Slig was a frame ago: where it will be when the grenade comes down
    s32 mLastSligX = 0;
    bool mHasLastSlig = false;
};

// A Slig lock opened with a Slig: called with the lever of its spawner, taken over from by the lever, walked to
// the lock (Domain::SligWay: floors, drops, lifts, teleporters whose levers it pulls), shooting the Sligs it
// walks into, and there it says "Hi" and then the lock's code back to it. It is burst after, unless there is
// another lock shut that it can get to: then it is left as it is for the step that opens that one.
class SligLockTask final : public Task
{
public:
    SligLockTask(u32 lock, u32 slig, u32 lever)
        : mLock(lock)
        , mSlig(slig)
        , mLever(lever)
    {
    }
    ~SligLockTask() override;
    std::string Name() const override
    {
        return "Open SligLock#" + std::to_string(mLock) + " with Slig#" + std::to_string(mSlig);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // What the Slig at the lock says next (a speak button, or 0: nothing yet). lockState: Entity::mState of the
    // lock. code: World::mLockCodes. standing: the Slig stands and can speak. hi, said: what it has said this
    // time round, kept by the caller.
    static u32 Say(s32 lockState, const std::vector<s32>& code, bool standing, bool& hi, s32& said);
    // Whether a Slig may walk off a floor's end down to (x, y), going left or right: every other Slig on that
    // floor and on the screen it lands on is ahead of it there and walks away
    static bool DropClear(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, bool left);
    // On his way along the Slig's floor to where it is taken over from: it is meant to see him and come after
    bool Luring() const
    {
        return mLure != nullptr;
    }
    // The game's number for a Slig that walks (Slig.hpp: Motion_2_Walking)
    static const s32 kSligWalking = 2;

private:
    // Frames it has spent on other Sligs of its floor
    s32 mShot = 0;
    u32 mLock;
    u32 mSlig;
    u32 mLever;
    std::unique_ptr<UseTask> mPull;
    // The way to where it is taken over from, when that is not by the lever (Domain::SligLockPerch)
    std::unique_ptr<GoToTask> mLure;
    bool mLured = false;
    // 0 the lever, 1 the chant, 2 as the Slig
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mWaits = 0;
    s32 mGone = 0;
    s32 mCalls = 0;
    s32 mBurst = 0;
    bool mHi = false;
    s32 mSaid = 0;
    s32 mLastState = 0;
    s32 mTries = 0;
    // A lift being ridden: the button held and where it stops
    u32 mRide = 0;
    s32 mRideY = 0;
    s32 mRideFrames = 0;
    bool mTapped = false;
    s32 mPullTries = 0;
    s32 mPullWait = 0;
    s32 mDropWaits = 0;
    // The frames at most it stands on a floor waiting for that floor's Sligs
    static const s32 kHoldFrames = 1500;
    s32 mHold = 0;
    s32 mShots = 0;
    s32 mLastNode = -1;
    s32 mStuck = 0;
    TaskStep StepTo(const SligAt& me, s32 x);
};

// A lever that makes a Slig level with him (StepKind::eSligCall, Domain::SligCallLedge): pulled, then at a run to
// the foot of the hoist up to the Slig's floor and up to hang there until the Slig has walked off it, up, the
// Slig taken over, a lever pulled with it if one is given, and burst.
class SligCallTask final : public Task
{
public:
    SligCallTask(u32 lever, u32 slig, u32 pull)
        : mLever(lever)
        , mSlig(slig)
        , mPullLever(pull)
    {
    }
    ~SligCallTask() override;
    std::string Name() const override
    {
        return "Call Slig#" + std::to_string(mSlig) + " with Lever#" + std::to_string(mLever);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLever;
    u32 mSlig;
    u32 mPullLever;
    // 0 to the lever, 1 the lever, 2 to the hoist and up to hang, 3 hanging until the Slig is off its floor, 4 up, 5 the chant,
    // 6 as the Slig, 7 the burst
    s32 mPhase = 0;
    s32 mNode = -1;
    s32 mTries = 0;
    s32 mWaits = 0;
    s32 mFrames = 0;
    s32 mPulls = 0;
    s32 mPullWait = 0;
    s32 mShootFrames = 0;
    bool mTapped = false;
    std::unique_ptr<GoToTask> mGo;
};

// Onto a foot switch's cell so that its switch turns over: off it first if he stands on it.
class StepOnTask final : public Task
{
public:
    StepOnTask(u32 plate, s32 on)
        : mPlate(plate)
        , mOn(on)
    {
    }
    ~StepOnTask() override;
    std::string Name() const override
    {
        return "Step on foot switch #" + std::to_string(mPlate);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mPlate;
    s32 mOn;
    s32 mPhase = 0;
    s32 mWas = -1;
    s32 mWaits = 0;
    std::unique_ptr<GoToTask> mGo;
};

// Into a cell that is in a zone which sets a switch when he is in it (an invisible switch)
class SwitchZoneTask final : public Task
{
public:
    SwitchZoneTask(s32 switchId, s32 node)
        : mSwitch(switchId)
        , mNode(node)
    {
    }
    ~SwitchZoneTask() override;
    std::string Name() const override
    {
        return "Walk into the zone of switch " + std::to_string(mSwitch);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // The zone of that switch sets Slogs on him (it is the switch that angers Slogs the map has awake): the way
    // to go through it at a run, away from them (-1 left, 1 right), or 0 if it sets none on him
    static s32 SetsSlogsOn(const World& world, s32 switchId, s32 abeX, s32 abeY);

private:
    s32 mSwitch;
    s32 mNode;
    bool mBegun = false;
    s32 mWaits = 0;
    // Through it at a run, the way held (-1 left, 1 right; 0: walked to and stood in, 2: not looked at yet), and
    // the frames run on since it set its switch
    s32 mRunThrough = 2;
    s32 mRanOn = 0;
    std::unique_ptr<GoToTask> mGo;
};

// A Paramite taken over and walked to every ring it can get to (Domain::ParamiteTour, by Domain::ParamiteRoute:
// floors, webs, hops and drops), each pulled, and let go under the last.
class ParamiteTourTask final : public Task
{
public:
    // clear: no rings; the Fleeches of its floors eaten, the other Paramites there called along, and all of them
    // left at the end of the floor away from the way out
    ParamiteTourTask(u32 paramite, s32 spot, bool clear = false)
        : mParamite(paramite)
        , mSpot(spot)
        , mClear(clear)
    {
    }
    ~ParamiteTourTask() override;
    std::string Name() const override
    {
        return "Paramite #" + std::to_string(mParamite) + " pulls the rings it can get to";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mParamite;
    s32 mSpot;
    // 0 to the spot, 1 chant, 2 on its way to the next ring, 3 pull, 4 let go, 5 himself again
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mPulls = 0;
    s32 mPulled = 0;
    s32 mWait = 0;
    u32 mRing = 0;
    s32 mWas = 0;
    // The leg under way: its kind (-1 none), where it ends, its web, and the frames it has had
    s32 mLeg = -1;
    s32 mLegTo = -1;
    s32 mLegX = 0;
    s32 mLegFrames = 0;
    // The frames spent after Fleeches
    s32 mHunt = 0;
    // A Fleech it could not eat where it stood (its x), left alone for that many turns, and the bites tried
    s32 mSkipPreyX = -100000;
    s32 mSkipPreyFor = 0;
    s32 mEatTries = 0;
    s32 mHuntLastX = -100000;
    s32 mHuntStill = 0;
    // Times it went back for a Fleech of its floor that was still alive when it was to be let go
    s32 mHuntRounds = 0;
    s32 mChaseDx = 0;
    s32 mChaseStill = 0;
    // The frames waited at a gap for a falling rock to go by
    s32 mWaitRock = 0;
    bool mClear = false;
    // Clearing: the Fleeches (their entities) whose places it has been to, the Paramites it has called, what it
    // is saying (0 nothing, then hello, come on, and stay at the end), and where it goes next (-1 none)
    std::set<u32> mBeenTo;
    std::set<u32> mCalled;
    s32 mSay = 0;
    s32 mGoal = -1;
    bool mParked = false;
    std::unique_ptr<GoToTask> mGo;
};

// A lock or lever a lone Paramite keeps by: meat thrown the other way draws it off (it goes for meat on its floor
// on the screen he is on, with no wall between, and eats for a while: Paramite::FindMeat), and then it is used.
class MeatLureTask final : public Task
{
public:
    MeatLureTask(u32 entity, u32 paramite, s32 want)
        : mEntity(entity)
        , mParamite(paramite)
        , mWant(want)
    {
    }
    ~MeatLureTask() override;
    std::string Name() const override
    {
        return "Meat for Paramite #" + std::to_string(mParamite) + ", then use #" + std::to_string(mEntity);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mEntity;
    u32 mParamite;
    s32 mWant;
    // 0 to the place to throw from, 1 turn and take the meat in hand, 2 throw, 3 until it is off, 4 use
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mSpot = -1;
    // The side of the thing he throws from (and to): 1 right of it
    s32 mSide = 0;
    ThrowSpot::Kind mKind = ThrowSpot::eForward;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<UseTask> mUse;
};

// Meat thrown for Paramites that team up on the way to a cell (Domain::PackLevel): from the last cell of the way
// before their level, onto their level on the side the way does not go, and then until they are off after it.
class MeatFeedTask final : public Task
{
public:
    explicit MeatFeedTask(s32 target)
        : mTarget(target)
    {
    }
    ~MeatFeedTask() override;
    std::string Name() const override
    {
        return "Meat for the Paramites on the way";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    s32 mTarget;
    // 0 to the place to throw from, 1 turn and take the meat in hand, 2 throw, 3 until they are off
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mSpot = -1;
    // Where the way comes onto their level, and the way it goes on from there (-1 left)
    s32 mOnto = -1;
    s32 mDir = 0;
    bool mLeft = false;
    ThrowSpot::Kind mKind = ThrowSpot::eUp;
    std::unique_ptr<GoToTask> mGo;
};

// A lever that wakes Fleeches pulled, and they shut in behind a gate two foot switches work (Domain::FleechPen):
// to the switch beyond the gate that opens it, by the way over the top with them after him, a cell off it toward
// the gate until all of them have come down on that side, and back through the gate over the switch that shuts it.
class FleechPenTask final : public Task
{
public:
    explicit FleechPenTask(u32 lever)
        : mLever(lever)
    {
    }
    ~FleechPenTask() override;
    std::string Name() const override
    {
        return "Lever #" + std::to_string(mLever) + ", and its Fleeches shut in";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLever;
    // 0 the lever, 1 to the switch that opens the gate, 2 until they are all on his side of it, 3 back through it
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mWaits = 0;
    s32 mGate = -1;
    s32 mOpen = -1;
    s32 mShut = -1;
    std::vector<u32> mGroup;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<UseTask> mUse;
};

// Fleeches let out with a lever and led to a Scrab that is then taken over to kill them (Domain::LeadFleeches).
// They make for where he is, not along the way he went, and go up a hoist only if they come by it with him
// above: so he is on its top, got to by another way if there is one, until the first of them starts up it, and
// then keeps a few cells ahead of them to where they drop.
class FleechLeadTask final : public Task
{
public:
    explicit FleechLeadTask(u32 lever)
        : mLever(lever)
    {
    }
    ~FleechLeadTask() override;
    std::string Name() const override
    {
        return "Fleeches let out with #" + std::to_string(mLever) + " and led to a Scrab";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    void Keep(Bot& bot, const std::set<s32>& edges);

    u32 mLever;
    // 0 the lever, 1 to the top of the next hoist on their way, 2 there until they come up it, 3 on ahead of them
    // to where they drop, 4 over, 5 to where he chants from, 6 there until they are down, 7 the Scrab
    s32 mPhase = 0;
    Domain::FleechLead mLead;
    // How many of the way's edges are behind him
    std::size_t mAt = 0;
    s32 mWaits = 0;
    s32 mClear = 0;
    Bot* mKept = nullptr;
    std::unique_ptr<UseTask> mUse;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<ScrabHuntTask> mHunt;
};

// The Scrabs of a floor got to run off its end: up the hoist there so that they see him, down again to hang from
// the ledge while they come at a run (one that hangs is not seen, and they keep running at where he was), and up
// as they go over. What comes down is loose on the floor below.
class ScrabLureTask final : public Task
{
public:
    explicit ScrabLureTask(s32 top)
        : mTop(top)
    {
    }
    ~ScrabLureTask() override;
    std::string Name() const override
    {
        return "Lure the Scrabs off the ledge";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    s32 mTop;
    // 0 to the foot of the hoist, 1 up to hang, 2 hanging until they are far off, 3 on top until one runs at him,
    // 4 hanging until it has gone over, 5 up again
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mWaits = 0;
    s32 mTries = 0;
    std::unique_ptr<GoToTask> mGo;
};

// Something Fleeches guard, used while they are away: he shows himself, has them follow him to the foot of a hoist
// and goes back to the thing by another way (Domain::LureFleeches). From the last place to turn back at, where
// they are by then decides whether he goes on.
class FleechLureTask final : public Task
{
public:
    FleechLureTask(u32 entity, s32 value)
        : mEntity(entity)
        , mValue(value)
    {
    }
    ~FleechLureTask() override;
    std::string Name() const override
    {
        return "Lead the Fleeches off";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    void Keep(Bot& bot, const std::set<s32>& edges);

    u32 mEntity;
    s32 mValue;
    // 0 the way is worked out, 1 to where they are led to, 2 towards them until they come, 3 back there, 4 to the
    // last place to turn back at, 5 the thing is used, 6 out of their reach
    s32 mPhase = 0;
    Domain::FleechLure mLure;
    s32 mTarget = -1;
    // The side of the thing it is used from (-1 its left)
    s32 mSide = -1;
    std::size_t mOutAt = 0;
    s32 mWaits = 0;
    s32 mTries = 0;
    u32 mLedAt = 0;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<UseTask> mUse;
    Bot* mKept = nullptr;
};

// A gate worked by its lever until it is shut with every Scrab of its floor on one side of it. With a Scrab on
// each side it is opened while both are on his screen: two that see each other fight, and one dies.
class ScrabPenTask final : public Task
{
public:
    ScrabPenTask(u32 lever, s32 feature, bool left)
        : mLever(lever)
        , mFeature(feature)
        , mLeft(left)
    {
    }
    ~ScrabPenTask() override;
    std::string Name() const override
    {
        return std::string("Shut the Scrabs in ") + (mLeft ? "left" : "right") + " of the gate";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    u32 mLever;
    s32 mFeature;
    // The side the Scrabs are to be on
    bool mLeft;
    s32 mFrames = 0;
    s32 mHold = 0;
    bool mSaid = false;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<Task> mUse;
};

// A way that leads over the floor of a Scrab that waits before it strikes (its attack delay), by a player's
// rule: from somewhere it can't see him (a ledge, a platform) he waits until it is so far off that he is down,
// done and out of its sight again before it could be at him, and only then goes. The floor is crossed a stint at
// a time, from one such place to the next. With an entity, that is used on the way and the nearest safe place
// made for.
class ScrabWaryTask final : public Task
{
public:
    ScrabWaryTask(std::string name, s32 goal, u32 entity, s32 value)
        : mName(std::move(name))
        , mGoal(goal)
        , mEntity(entity)
        , mValue(value)
    {
    }
    ~ScrabWaryTask() override;
    std::string Name() const override
    {
        return mName;
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // Whether the way from where Abe is to a cell (or, with an entity, to where that is used from) is one for
    // this task: he is out of the Scrabs' sight now, the way leads over such a floor, and its first stint can be
    // done in the time the Scrab gives when it is at the far end of its beat
    // note: what was found, for the log
    // (chased: a crossing that is the only way on with something after him; Fleeches awake don't rule it out)
    // The cells a loose Scrab can get at him on. waitingOnly: only those of Scrabs that wait before they strike.
    static std::vector<char> Hot(const Bot& bot, bool waitingOnly = false);
    static bool Wanted(const Bot& bot, s32 goal, u32 entity, std::string* note = nullptr, bool chased = false);

private:
    struct Stint final
    {
        // The edges of the way from where he is: mFirst is the first onto the Scrab's floor (-1: none on the way)
        std::vector<s32> mPath;
        s32 mFirst = -1;
        // The cell where he is out of its sight again, or -1 when the stint ends at the thing to use
        s32 mExit = -1;
        // Where he is on that floor and how many frames in, and the frames in all
        std::vector<std::pair<s32, s32>> mAt;
        s32 mFrames = 0;
        bool mFound = false;
        // After the thing is used: the place out of its sight to make for (the one that leaves most time)
        s32 mRetreat = -1;
        // The cell it ends at, and for a lock the side it is slapped from (0: as it comes)
        s32 mTarget = -1;
        s32 mSide = 0;
        s32 mSpare = 0;
        // The edges of the stint, on to where he is out of sight again: the way that was timed
        std::set<s32> mEdges;
        // The frames from setting out until it can see him: the turn, the jump, the drop
        s32 mLead = 0;
        // The thing is used from a trap door that it opens: the cell on the Scrab's floor he comes down on (-1: no)
        s32 mDrop = -1;
        // The first cell of the Scrab's floor he is on (or drops past, into a well in it)
        s32 mOnto = -1;
    };
    // best: as it would be with the Scrabs at the ends of their beats that suit best
    // face: the way he has to face to use the thing (-1 left, 1 right, 0 any)
    // drop: the cell he falls to when the thing is used (-1: he stays where he uses it)
    static Stint Plan(const Bot& bot, s32 cur, s32 target, bool use, s32 face, s32 drop, const std::vector<char>& hot, bool best);
    // One way, with some edges left out. Plan tries again without the stint of a way whose first stint can't be done
    // in time even at the best moment: the way with the least of the Scrab's floor may lead through its beat.
    static Stint PlanOnce(const Bot& bot, s32 cur, s32 target, bool use, s32 face, s32 drop, const std::vector<char>& hot, bool best, const std::set<s32>& banned);
    // The same for a cell or for an entity, which may have two sides to use it from: the one that leaves most time
    static Stint PlanFor(const Bot& bot, s32 cur, s32 goal, const Entity* entity, const std::vector<char>& hot, bool best);
    static std::vector<Domain::LooseScrab> Scrabs(const World& world, bool waitingOnly = false);
    // The frames to spare with the Scrabs where they are (negative: not enough), or with each put at the end
    // of its beat that suits best
    // note: where the Scrab is and where it is taken to be by then, for the log
    // beatKnown: whether every Scrab that counts has been seen to walk its beat from end to end
    static s32 Spare(const Bot& bot, const Stint& stint, const std::vector<char>& hot, bool best, std::string* note = nullptr, bool* beatKnown = nullptr);

    std::string mName;
    s32 mGoal;
    u32 mEntity;
    s32 mValue;
    bool mUsed = false;
    s32 mWaits = 0;
    bool mSaid = false;
    u32 mWentAt = 0;
    s32 mRetreat = -1;
    // The bot it has told to keep to the stint's edges
    Bot* mKept = nullptr;
    // On the way to where a stint starts from, kept to the edges that are out of the Scrabs' sight
    bool mPrefix = false;
    s32 mPrefixFails = 0;
    s32 mDepth = 0;
    // The teleporter by the landing was gone for once (see Next)
    bool mTeleTried = false;
    s32 mTeleDrop = -1;
    s32 mTeleHeld = 0;
    s32 mTeleLand = -1;
    s32 mTeleOut = -1;
    std::set<s32> mTeleFirst;
    std::set<s32> mTeleOn;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<Task> mUse;
};

// Stood in an invisible zone until the flying Slig that is about has gone by and is behind him, going away
class HideTask final : public Task
{
public:
    std::string Name() const override
    {
        return "Keep out of the flying Slig's sight";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    s32 mFrames = 0;
    s32 mClear = 0;
    // A flying Slig that never leaves the screen: out at a run instead, into a well on the floor of the next thing
    // to do (in one he can't be hurt, from the jump in until he is down again where it throws him out), and from
    // there to the thing. 0: not yet; 1: to the well; 2: into it; 3: in it; 4: the thing. And before the run: 5:
    // to the last cell out of sight; 6: there, until the flying Slig is where a call stops it over its upper run;
    // 7: called, until it hangs still. After 1, 8: at the well until its grenade is in the air.
    s32 mDash = 0;
    s32 mDashWell = -1;
    u32 mDashEntity = 0;
    s32 mDashFrames = 0;
    s32 mDashPresses = 0;
    // Before the run (5 to 7, see HideTask::Next): the last cell of the way that is still out of sight, where the
    // well throws him out, how often he has called, how long the flying Slig has hung still, and how long he has
    // stood at the well for its grenade
    s32 mCallSpot = -1;
    s32 mDashOut = -1;
    s32 mCalls = 0;
    s32 mCallFrames = 0;
    s32 mHung = 0;
    s32 mAtWell = 0;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<Task> mUse;
};

// Off the floor of a Slog that is after him, by the nearest way that leads away from it. Coming down next to one,
// the run starts in the air: with the way held as he lands he runs at once, and the landing is nine frames
// shorter (measured in the Mudanchee trial: the Slog has him on the hoist otherwise).
class SlogFleeTask final : public Task
{
public:
    SlogFleeTask(s32 refuge, bool left, s32 fromX)
        : mRefuge(refuge)
        , mLeft(left)
        , mFromX(fromX)
    {
    }
    std::string Name() const override
    {
        return "Off the Slog's floor";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    s32 mRefuge = -1;
    bool mLeft = false;
    // Where on its floor the way off starts
    s32 mFromX = 0;
    s32 mHeld = 0;
    // The way off is a teleporter set off by a foot switch: -1 not looked at yet, 1 run over without a stop
    s32 mThrough = -1;
    std::unique_ptr<GoToTask> mGo;
};

// Slogs gathering behind a gate on Abe's floor that will open (the Mudanchee trial: a spawner behind three timed
// doors), and a Slig on another floor of his screen with a teleporter that brings it to his: Abe stands behind
// where it comes out, takes it over, runs it to the teleporter and stands it in front of him, facing the gate. It
// shoots what comes out, in bursts (held, the recoil walks it back into Abe, who is then in front of the gun),
// and is burst when no more come.
class SlogGuardTask final : public Task
{
public:
    struct Setup final
    {
        s32 mSligX = 0;
        s32 mSligY = 0;
        // Where on the Slig's floor its teleporter takes it, and where on Abe's floor it comes out
        s32 mPlateX = 0;
        s32 mArriveX = 0;
        // 1: the Slogs come from the right
        s32 mSide = 1;
        // The cell Abe chants from
        s32 mStand = -1;
    };
    static bool Find(const Bot& bot, Setup& setup);
    explicit SlogGuardTask(const Setup& setup)
        : mSetup(setup)
    {
    }
    std::string Name() const override
    {
        return "Hold the Slogs off with a Slig";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    Setup mSetup;
    std::unique_ptr<GoToTask> mGo;
    // 0 to the place, 1 chanting, 2 the Slig to its teleporter, 3 to its post, 4 on guard, 5 bursting it
    s32 mPhase = 0;
    s32 mFrames = 0;
    s32 mQuiet = 0;
    s32 mShot = 0;
    // Slogs seen to come out of the spawner, and how many stood at it the last time he looked
    s32 mCameOut = 0;
    s32 mAtSpawner = -1;
    // Frames waited for Abe to be himself again after the Slig burst
    s32 mBack = 0;
};

// Slogs that run Abe down from a floor he can't get at them on: stand where they come down to him, run back
// past the electric walls on his own floor and throw the lever that turns those on behind him
class SlogTrapTask final : public Task
{
public:
    struct Setup final
    {
        u32 mLever = 0;
        s32 mSwitch = 0;
        // The walls' extent, and the floor they are on
        s32 mX1 = 0;
        s32 mX2 = 0;
        s32 mY = 0;
        // -1: the lever is to the left of the walls, so the Slogs are lured on the right
        s32 mLeverSide = 0;
    };
    // The walls and lever on the floor at y that would do, if there are any
    static bool Find(const NavGraph& nav, const World& world, s32 y, bool half, Setup& setup);
    // A Slog that is on Abe's floor, or on its way down to it
    static bool Coming(const World& world, s32 abeY);
    static bool AnyAwake(const World& world);

    explicit SlogTrapTask(const Setup& setup)
        : mSetup(setup)
    {
    }
    std::string Name() const override
    {
        return "Trap the Slogs";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    Setup mSetup;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<UseTask> mUse;
    // 0 to the lure spot, 1 luring, 2 to the lever, 3 watching them fry, 4 and 5 walls off again
    s32 mPhase = 0;
    s32 mWaits = 0;
    s32 mCells = 2;
    s32 mRounds = 0;
};

// Leave the path by one of the nav graph's exits
class ExitTask final : public Task
{
public:
    explicit ExitTask(s32 exitIdx)
        : mExit(exitIdx)
    {
    }
    std::string Name() const override
    {
        return "Exit " + std::to_string(mExit);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    s32 mExit;
    std::unique_ptr<GoToTask> mGo;
    bool mArrived = false;
    s32 mTries = 0;
    s32 mGhostWaits = 0;
    // A bird portal: the way to where it is chanted open from, and the chants made
    std::unique_ptr<GoToTask> mPortalGo;
    bool mAtChant = false;
    s32 mPortalChants = 0;
    s32 mStartPath = -1;
    s32 mStartLevel = -1;
};

} // namespace Bot
