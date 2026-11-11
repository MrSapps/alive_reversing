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
    // The edge Abe is on his way along (-1: none), so a death can be laid at its door: two and it is banned
    void SetCurrentEdge(s32 edge)
    {
        mCurrentEdge = edge;
    }
    NavGraph::EdgeCost EdgeCostFn() const;
    void Log(const std::string& text);
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
    std::map<std::string, s32> mFailedActions;
    std::string mPlanText;
    std::set<std::pair<s32, s32>> mVisitedPaths;
    LevelGraph mLevel;
    // Switches (by path and fact key) that have changed during the run, and the one being tried now
    std::set<std::pair<s32, u32>> mExplored;
    u32 mExploring = 0;
    s8 mLastSwitches[256] = {};
    std::string mLastSligs;
    s32 mDeadFrames = 0;
    s32 mStartWaits = 0;
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
