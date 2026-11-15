#pragma once

#include "Pilot.hpp"
#include "Domain.hpp"
#include "World.hpp"
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

// Turn a UXB off: crouch a cell from it, facing it, and slap it as its light turns green
class DisarmTask final : public Task
{
public:
    DisarmTask(s32 x, s32 y, bool half)
        : mX(x)
        , mY(y)
        , mHalf(half)
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
    std::unique_ptr<GoToTask> mGo;
    bool mWasGreen = true;
    s32 mFrames = 0;
    s32 mSlaps = 0;
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
    u32 mSack;
    std::unique_ptr<GoToTask> mGo;
    s32 mJumps = 0;
    s32 mWaits = 0;
    s32 mPicks = 0;
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

// Walk, hop, climb... to a cell the goal accepts, by the nav graph
class GoToTask final : public Task
{
public:
    // Whether the first `count` edges of a way can be gone now without a drill reaching Abe on any of them: each
    // edge has him in the cell it leaves and the one it leads to for as long as it may take
    // late: frames he is later (or, negative, sooner) than worked out
    // The drill that goes over Abe's head at a cell and is moving or will be, or null
    static const Entity* SweepHigh(const NavGraph& nav, const World& world, s32 node);
    // rolled: as it goes if the walking of it is rolled, from a crouch
    static bool SweepsClear(const NavGraph& nav, const World& world, const std::vector<s32>& path, std::size_t count, s32 late = 0, bool rolled = false);
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
    std::string mName;
    std::function<bool(s32)> mIsGoal;
    Gait mTopGait = Gait::eRun;
    bool mQuiet = false;
    s32 mLastEdge = -1;
    s32 mReflexes = -1;
    bool mHurry = false;
    s32 mLastFrom = -1;
    s32 mSameSpot = 0;
    s32 mOffGraph = 0;
    s32 mSteps = 0;
    bool mWaited = false;
    // Frames spent waiting for a Slig to look the other way
    s32 mHiding = 0;
    // Chants at a bird portal that has not opened yet
    s32 mPortalChants = 0;
    // Tries at a teleporter that goes off when its plate is stood on
    s32 mPlateSteps = 0;
    // Steps spent waiting for a Slurg to get out of the way
    s32 mSlurgWaits = 0;
    bool mFleechAwakeHere = false;
    s32 mLandWaits = 0;
    // Steps spent at a door waiting for a lock's ghost to open it
    s32 mGhostWaits = 0;
    bool mChaseLogged = false;
    s32 mLureWaits = 0;
    // Frames waited for a Slog under the ledge to leap up, and whether it was in the air the frame before
    s32 mSlogWaits = 0;
    bool mSlogUp = false;
    // Frames waited for a lift that is on its way to the stop he would walk onto, and where it was last
    s32 mLiftComeWaits = 0;
    s32 mLiftComeY = -100000;
    s32 mUnseenWaits = 0;
    // Chants made to go invisible
    s32 mInvisChants = 0;
    s32 mInvisWaits = 0;
    // Frames spent waiting for a drill to be out of the way
    s32 mSweepWait = 0;
    // Pulling the lever that stops the drill in the way
    std::unique_ptr<Task> mStop;
    s32 mStops = 0;
    bool mTrapTried = false;
    s32 mRetreats = 0;
    bool mLeading = false;
    // Crouched before a drill, to roll through under it
    bool mSweepDuck = false;
    s32 mHangFails = 0;
    s32 mLiftGlide = 0;
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

private:
    u32 mMud;
    bool mCalm;
    s32 mSide = 0;
    bool mRemote = false;
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
        return "Rescue Mudokon#" + std::to_string(mMud) + " via BirdPortal#" + std::to_string(mPortal);
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;

private:
    // A blind Mudokon is walked to the portal's floor a straight stretch at a time: talked to from the side it
    // is to walk to, told to wait where the stretch ends, and dropped through the trap door it stops on.
    // True once it is there, else step is what to do.
    bool LeadBlind(Bot& bot, const Entity& mud, const Entity& portal, const std::string& lastFailed, TaskStep& step);

    u32 mMud;
    u32 mPortal;
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
    u32 mRing;
    u32 mDrill;
    std::unique_ptr<GoToTask> mGo;
    s32 mPhase = 0;
    s32 mWaits = 0;
    s32 mPulls = 0;
    s32 mLastX = 0;
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
    ~ClearSligTask() override;
    std::string Name() const override
    {
        return mInner ? mInner->Name() : "Take over the Slig that woke";
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // The Slig that would see Abe working at (x, y), or null. Asleep ones and ones not about don't count.
    static const SligAt* Watcher(const World& world, s32 x, s32 y, bool half);
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
