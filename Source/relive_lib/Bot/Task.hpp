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

// Walk, hop, climb... to a cell the goal accepts, by the nav graph
class GoToTask final : public Task
{
public:
    GoToTask(std::string name, std::function<bool(s32 node)> isGoal);
    // The cell at a world position
    static std::unique_ptr<GoToTask> To(Bot& bot, s32 x, s32 y, bool half);

    std::string Name() const override
    {
        return mName;
    }
    TaskStep Next(Bot& bot, const std::string& lastFailed) override;
    // Never faster than this (Mudokons in tow don't stop at wheels when they have to run to keep up)
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
    bool mLeading = false;
    s32 mLeadWaits = 0;
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
    bool mSaidHello = false;
    s32 mSlaps = 0;
    s32 mTalks = 0;
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
