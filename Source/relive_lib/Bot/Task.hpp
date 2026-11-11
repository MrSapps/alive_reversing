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

// Throw rocks at a mine until it goes off: from one place after another, keeping what missed and what killed
class BlowMineTask final : public Task
{
public:
    explicit BlowMineTask(u32 mine)
        : mMine(mine)
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
    static bool SweepsClear(const NavGraph& nav, const World& world, const std::vector<s32>& path, std::size_t count, s32 late = 0);
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
    // Crouched by a UXB on the way: whoever runs this walk leaves Abe down
    bool Disarming() const
    {
        return mDisarm != nullptr;
    }
    void SetTopGait(Gait gait)
    {
        mTopGait = gait;
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
    s32 mLastEdge = -1;
    s32 mLastFrom = -1;
    s32 mSameSpot = 0;
    s32 mOffGraph = 0;
    s32 mSteps = 0;
    bool mWaited = false;
    // Frames spent waiting for a Slig to look the other way
    s32 mHiding = 0;
    // Frames spent waiting for a drill to be out of the way
    s32 mSweepWait = 0;
    // Pulling the lever that stops the drill in the way
    std::unique_ptr<Task> mStop;
    s32 mStops = 0;
    s32 mRetreats = 0;
    bool mLeading = false;
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

private:
    u32 mEntity;
    s32 mWant;
    s32 mSide = -1;
    std::unique_ptr<GoToTask> mGo;
    bool mArrived = false;
    s32 mTries = 0;
    bool mLooking = false;
    s32 mLookTaps = 0;
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

private:
    u32 mMud;
    bool mCalm;
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
    u32 mMud;
    u32 mPortal;
    std::unique_ptr<GoToTask> mGo;
    std::unique_ptr<RecruitTask> mRecruit;
    s32 mRecruits = 0;
    s32 mPhase = 0;
    s32 mWaits = 0;
    s32 mRespawns = 0;
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
    std::string Name() const override
    {
        return mInner->Name();
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
    s32 mStartPath = -1;
    s32 mStartLevel = -1;
};

} // namespace Bot
