#pragma once

#include "BotRng.hpp"
#include "Checkpoint.hpp"
#include "Domain.hpp"
#include "Profile.hpp"
#include "World.hpp"
#include "GameView.hpp"
#include "LevelGraph.hpp"
#include "NavGraph.hpp"
#include "Pilot.hpp"
#include "Task.hpp"
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class BaseMap;

namespace Bot {

// What the bot has worked out about one path: where Abe can go, and what turned out not to work
struct PathKnowledge final
{
    NavGraph mNav;
    std::set<s32> mBannedEdges;
    // Throws at a mine (its id, the cell thrown from, how) that missed or killed Abe: not tried again
    std::set<std::tuple<u32, s32, s32>> mBadThrows;
};

enum class BotState : u8
{
    eIdle,    // nothing to do
    eRunning,
    eDone,    // what it was asked is done
    eStuck,   // it can't go on: see Why()
};

// Plays the game: see BOT_DESIGN.md. Updated once per main loop iteration, like Automation, and only ever acts
// between frames by choosing the pad buttons for the next one.
class Bot final
{
public:
    Bot();
    ~Bot();

    void Update(BaseMap& map, bool betweenFrames);

    void SetSeed(u64 seed)
    {
        mRng.Seed(seed);
    }

    // One Prim at a time, for scripts and tests
    void Do(const Prim& prim);
    bool PilotBusy() const
    {
        return mPilotActive || mPilotPending;
    }
    PilotStatus PilotResult() const
    {
        return mPilotResult;
    }
    const std::string& PilotWhy() const
    {
        return mPilot.Why();
    }

    // Play by a profile until the game is won or the bot is stuck
    void Start(const Profile& profile);
    // Stop playing: the buttons are let go and it does nothing more
    void Stop();
    // What it is doing and what is around, in a few lines: for the log, a stuck report, a person
    std::string Report() const;

    // One task, then idle again
    void Run(std::unique_ptr<Task> task);
    // Go to the floor cell at a world position (made into a task once the path's nav graph is there)
    void GoTo(s32 x, s32 y, bool half);
    BotState State() const
    {
        return mState;
    }
    const std::string& Why() const
    {
        return mWhy;
    }

    // A line per frame from now on: frame, position, motion, buttons
    void StartTrace()
    {
        mTracing = true;
    }
    const std::string& Trace() const
    {
        return mTrace;
    }
    const std::string& LogText() const
    {
        return mLog;
    }
    // The buttons of every frame that counted (what was tried and taken back isn't in it), one "<bits hex>" a line
    // after a "start <game frame>" line. A line with the top bit set was held while a movie ran, to skip it.
    std::string InputsText() const;

    // For tasks
    const AbeState& Abe() const
    {
        return mAbe;
    }
    NavGraph& Nav()
    {
        return mKnow->mNav;
    }
    BotRng& Rng()
    {
        return mRng;
    }
    const World& GetWorld() const
    {
        return mWorld;
    }
    // The buttons that say hello and follow me in this game
    u32 SpeakHello() const;
    u32 SpeakFollow() const;
    u32 SpeakSorry() const;
    u32 SpeakSlap() const;
    u32 SpeakWork() const;
    u32 SpeakAll() const;
    u32 SpeakWait() const;
    // The cell Abe stands in, or -1
    s32 AbeNode() const;
    void BanEdge(s32 edge);
    // A walk along an edge got nowhere: true from the second time on
    bool NoteBlocked(s32 edge)
    {
        return ++mBlockedWalks[edge] >= 2;
    }
    // A rock is about to be thrown at a mine: what comes of it is kept. A death before one of the other two is
    // called is put down to the throw.
    // Setting off past a drill now. A death soon after is put down to the timing, which the next try shifts.
    void SweepBegun()
    {
        mSweepPending = true;
        mSweepFrame = mAbe.mFrame;
    }
    // Going for a drill's lever: no going for another's on the way there
    void StoppingSweep()
    {
        mSweepStopFrame = static_cast<s32>(mAbe.mFrame);
    }
    bool MayStopSweep() const
    {
        const s32 since = static_cast<s32>(mAbe.mFrame) - mSweepStopFrame;
        return since > 600 || since < 0;
    }
    // Frames Abe is taken to be later (negative: sooner) past a drill than worked out: what the tries so far taught
    s32 SweepLate() const;
    // A throw that missed is taken back (the game put back to before it), so the rock isn't lost: what was
    // learnt from it stays
    void WantMissRewind()
    {
        mMissRewind = true;
    }
    // It is over with nothing learnt from it
    void ThrowOver()
    {
        mThrowPending = false;
    }
    void ThrowBegun(u32 mine, const ThrowSpot& spot);
    void ThrowMissed();
    void ThrowHit();
    bool ThrowIsBad(u32 mine, const ThrowSpot& spot) const;
    // A throw from this far and this level has set a mine off before
    bool ThrowHasWorked(const ThrowSpot& spot) const;
    // The edge Abe is on his way along (-1: none), so a death can be laid at its door: two and it is banned
    void SetCurrentEdge(s32 edge)
    {
        mCurrentEdge = edge;
    }
    NavGraph::EdgeCost EdgeCostFn() const;
    void Log(const std::string& text);
    // See World::QuietFrames, with the beats this run has seen
    s32 QuietFrames(s32 x, s32 y, bool half) const;
    // Its beat has been watched from one end to the other (or far enough to count on)
    bool KnowsBeat(const SligAt& slig) const
    {
        for (const SligBeat& beat : mBeats)
        {
            if (beat.mHalf == slig.mHalf && std::abs(beat.mY - slig.mY) <= 45 && slig.mX >= beat.mMinX - 30 && slig.mX <= beat.mMaxX + 30 && beat.mMaxX - beat.mMinX >= 150)
            {
                return true;
            }
        }
        return false;
    }
    // How many tries from the newest checkpoint ended in a death
    s32 Attempt() const
    {
        return mAttempt;
    }
    // Frames to hold back before this try
    s32 RetryDelay();
    Gait GaitFor(s32 cells);

    s32 Deaths() const
    {
        return mDeaths;
    }
    s32 Restores() const
    {
        return mRestores;
    }

private:
    void EnsureKnowledge(BaseMap& map);
    void Think(BaseMap& map);
    void OnDeath(BaseMap& map);
    void RewindMiss(BaseMap& map);
    void MaybeCheckpoint(BaseMap& map);
    void Stuck(const std::string& why);
    // The next task by the profile's goals. False with mState set when there's nothing more to do or no way to.
    bool Replan(BaseMap& map);
    std::unique_ptr<Task> MakeTask(const Domain& domain, const Goap::Action& action);

    AbeState mAbe;
    BotRng mRng;
    Pilot mPilot;
    Prim mPendingPrim;
    bool mPilotPending = false;
    bool mPilotActive = false;
    // The pilot is being driven by a task, not by Do()
    bool mPilotForTask = false;
    PilotStatus mPilotResult = PilotStatus::eDone;
    std::string mLastPrimFailed;
    u32 mBits = 0;
    bool mWasDriving = false;

    World mWorld;
    Profile mProfile;
    bool mAuto = false;
    // What the current task was planned as, and how often a planned action has failed
    std::string mTaskAction;
    // It is a rescue or a kill: the run goes on without it
    bool mTaskOptional = false;
    // Deaths while at each action
    std::map<std::string, s32> mActionDeaths;
    // How often each action has failed in all, forgiven or not
    std::map<std::string, s32> mFailedTotal;
    std::map<std::string, s32> mFailedActions;
    std::string mPlanText;
    // The plan being followed, by its actions' names, and what it is for: kept while it still holds (see Replan)
    std::vector<std::string> mKeptPlan;
    std::vector<Goap::Fact> mKeptGoal;
    std::string mKeptGoalName;
    bool mKeptForMudokon = false;
    // How many Mudokons could be got at when it was made
    std::size_t mKeptOpen = 0;
    std::set<std::pair<s32, s32>> mVisitedPaths;
    LevelGraph mLevel;
    // Switches (by path and fact key) that have changed during the run, and the one being tried now
    std::set<std::pair<s32, u32>> mExplored;
    u32 mExploring = 0;
    s8 mLastSwitches[256] = {};
    std::string mLastSligs;
    s32 mLastAwakeSligs = 0;
    s32 mDeadFrames = 0;
    s32 mStartWaits = 0;
    bool mThrowPending = false;
    u32 mThrowFrame = 0;
    u32 mLastLogFrame = 0;
    s32 mLastMines = 0;
    bool mMissRewind = false;
    s32 mMissRewinds = 0;
    bool mSweepPending = false;
    u32 mSweepFrame = 0;
    std::map<s32, s32> mBlockedWalks;
    s32 mSweepDeaths = 0;
    s32 mSweepStopFrame = -100000;
    // Deaths per cell (x, y): the same place over and over is not a matter of luck
    std::map<std::pair<s32, s32>, s32> mDeathCells;
    u32 mThrowMine = 0;
    ThrowSpot mThrowSpot;
    // (level, cells, kind) of throws that set a mine off
    std::set<std::tuple<s32, s32, s32>> mThrowHits;
    s32 mCurrentEdge = -1;
    std::map<s32, s32> mEdgeDeaths;
    s32 mLastKilled = 0;
    s32 mLastRescued = 0;
    // Whether the run may take an exit to that path now
    bool ExitAllowed(BaseMap& map, const NavFeature& exit, bool* sideTrip);
    // What each path off the way was found to be worth, by path
    std::map<s32, SideTrip> mSideTrips;
    s32 mLogicWaits = 0;
    // The human option's last dawdle, so two don't follow each other
    bool mDawdled = false;
    bool mSkipPulse = false;
    bool mIsAe = true;

    BotState mState = BotState::eIdle;
    std::string mWhy;
    std::unique_ptr<Task> mTask;
    // Something that can't wait, done before the task goes on: a wired Mudokon tagging along slapped out of it
    std::unique_ptr<Task> mReflex;
    std::map<u32, s32> mReflexTries;
    s32 mSlogTraps = 0;
    std::vector<SligBeat> mBeats;
    u32 mForgaveFrame = 0;
    // Where Abe was when an action last failed: it is left out only while he is still about there
    std::map<std::string, std::pair<s32, s32>> mFailedWhere;
    std::unique_ptr<Task> MakeReflex();
    bool mGoToPending = false;
    s32 mGoToX = 0;
    s32 mGoToY = 0;
    bool mGoToHalf = false;

    // Per level and path
    std::map<std::pair<s32, s32>, PathKnowledge> mKnowledge;
    PathKnowledge* mKnow = nullptr;

    struct Saved final
    {
        Checkpoint mCheckpoint;
        std::size_t mInputCount = 0;
        s32 mAttempts = 0;
        // The frame the last try from here died on: the same one again and nothing done from here changes it
        s32 mLastDeathFrame = -1;
    };
    std::vector<Saved> mCheckpoints;
    bool mRestoring = false;
    s32 mAttempt = 0;
    s32 mDeaths = 0;
    s32 mRestores = 0;
    u32 mLastCheckpointFrame = 0;

    std::vector<u32> mInputs;
    u32 mInputsStartFrame = 0;
    bool mInputsStarted = false;

    bool mTracing = false;
    std::string mTrace;
    std::string mLog;
};

} // namespace Bot
