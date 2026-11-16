#pragma once

#include "BotRng.hpp"
#include "Checkpoint.hpp"
#include "Lookahead.hpp"
#include "Domain.hpp"
#include "ThrowArc.hpp"
#include "Profile.hpp"
#include "World.hpp"
#include "GameView.hpp"
#include "LevelGraph.hpp"
#include "NavGraph.hpp"
#include "Pilot.hpp"
#include "SligCycle.hpp"
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

// What the bot remembers of the run so far: what it has seen of the world, where it has been, what it has found
// to work and not. Looking ahead, it is put back with a save (see Lookahead): what a try that was taken back showed
// did not happen. What it learns from failing (a throw that misses, where he keeps dying) is not here: that stays.
struct BotMemory
{
    s32 mRestFrames = 0;
    u32 mChasedFrame = 0;
    s32 mChasedPath = -1;
    World mWorld;
    // How often each action has failed in all, forgiven or not
    std::map<std::string, s32> mFailedTotal;
    std::map<std::string, s32> mFailedActions;
    // The plan being followed, by its actions' names, and what it is for: kept while it still holds (see Replan)
    std::vector<std::string> mKeptPlan;
    std::vector<Goap::Fact> mKeptGoal;
    std::string mKeptGoalName;
    bool mKeptForMudokon = false;
    // How many Mudokons could be got at when it was made
    std::size_t mKeptOpen = 0;
    std::set<std::pair<s32, s32>> mVisitedPaths;
    LevelGraph mLevel;
    // (the reason there was no getting away from a woken Slig is logged once)
    s32 mFleeWhy = 0;
    // The levels he has been in: going back to one is not getting on
    std::set<s32> mLevelsBeen;
    // Switches (by path and fact key) that have changed during the run, and the one being tried now
    std::set<std::pair<s32, u32>> mExplored;
    // The regions (path, kRegion value) he has stood in, and how often each has been made for
    std::set<std::pair<s32, s32>> mRegionsBeen;
    std::map<std::pair<s32, s32>, s32> mRegionTries;
    std::pair<s32, s32> mRegionGoal{0, 0};
    // The path in which any path not yet visited became good enough to leave by
    s32 mNewPathFor = -1;
    // Where he came into the path he is in. Of two ways out to the same place, the one he has just come in by
    // is not the one meant (a vault's own door is), until nothing else is left to try (mBackFor: in that path)
    std::pair<s32, s32> mArrivedKey{-1, -1};
    s32 mArrivedX = 0;
    s32 mArrivedY = 0;
    s32 mBackFor = -1;
    // How often each path (level, path) has been backed out of
    std::map<std::pair<s32, s32>, s32> mBackOuts;
    u32 mRingDueBy = 0;
    bool mInvisSaved = false;
    bool mInvisChantNow = false;
    bool mInvisChantSoon = false;
    bool mInvisAtOnce = false;
    bool mInvisCross = false;
    bool mInvisRun = false;
    bool mWasInvisible = false;
    SligCycle mSligCycle;
    u32 mDashUntil = 0;
    bool mHoldInvis = false;
    // The frame the power to go unseen was last seen in hand (0: never)
    u32 mChargeFrame = 0;
    u32 mUnseenUse = 0;
    // The path for which why no way out is known has been logged
    s32 mWhyNoWayOut = -1;
    u32 mExploring = 0;
    std::string mLastSligs;
    s32 mLastAwakeSligs = 0;
    s32 mLastMines = 0;
    std::map<s32, s32> mBlockedWalks;
    s32 mLastKilled = 0;
    s32 mLastRescued = 0;
    // What each path off the way was found to be worth, by path
    std::map<s32, SideTrip> mSideTrips;
    s32 mLogicWaits = 0;
    std::map<u32, s32> mReflexTries;
    s32 mSlogTraps = 0;
    s32 mAmbushes = 0;
    s32 mSlogFlees = 0;
    s32 mSlogGuards = 0;
    // Flying Sligs: which way along its track one was last seen to go (it stops at the ends), where the plan's next
    // things to do are, and whether one is to be kept out of the sight of where Abe stands now
    s32 mFlierDir = 0;
    s32 mFlierAt = -1;
    s32 mFlierHides = 0;
    s32 mPerchKills = 0;
    std::vector<SligBeat> mBeats;
    u32 mForgaveFrame = 0;
    // Where Abe was when an action last failed: it is left out only while he is still about there
    std::map<std::string, std::pair<s32, s32>> mFailedWhere;
    u32 mSeenByScrabAt = 0;
    // By Scrab: where it was last, and the frame it came to stand there (0 while it moves or is not known to have)
    std::map<u32, std::pair<s32, u32>> mScrabStill;
    std::set<u32> mScrabMoved;
    std::map<u32, std::pair<s32, s32>> mScrabBeat;
};

// Plays the game: see BOT_DESIGN.md. Updated once per main loop iteration, like Automation, and only ever acts
// between frames by choosing the pad buttons for the next one.
class Bot final : private BotMemory
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
    const NavGraph& Nav() const
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
    void LockSlapped(u32 id)
    {
        mWorld.mLocksSlapped.insert(id);
        // (one that gives the power: its ring is with him within some 80 frames, wherever he has got to)
        const Entity* lock = mWorld.Find(id);
        if (lock && lock->mKind == EntityKind::eSlapLock && lock->mInputs[0] > 0 && !mWorld.mInvisCharge && !mWorld.mInvisible)
        {
            mRingDueBy = mAbe.mFrame + 150;
            mWorld.mInvisComing = true;
        }
    }
    // The power to go invisible is being kept for a step of the plan (not used on Fleeches met before it), and
    // the time to use it has come: that step is this one or the next
    bool InvisSaved() const
    {
        return mInvisSaved;
    }
    bool InvisChantNow() const
    {
        return mInvisChantNow;
    }
    // The step that wants him unseen is the one after the step at hand
    // The step at hand wants him unseen in a path of security orbs: the power is got and used at once
    bool InvisAtOnce() const
    {
        return mInvisAtOnce;
    }
    bool InvisChantSoon() const
    {
        return mInvisChantSoon;
    }
    // The thing that step uses (0: it is a crossing, or there is none)
    // The step the power to go unseen is kept for is a crossing (not a thing to be used unseen)
    bool InvisForCrossing() const
    {
        return mInvisCross;
    }
    // He is unseen by a chant made for a crossing: the way is run against the power's clock
    bool InvisRun() const
    {
        // (or he has just become unseen, and the frame's note of it is not made yet)
        return mInvisRun || (mWorld.mInvisible && !mWasInvisible && mInvisCross);
    }
    // A sortie is on (ClearSligTask): while the Slig of a floor is off his screen and walking away, the way
    // along it is taken at a run or a roll with nothing held back for it. Set a few frames at a time.
    void Dash(u32 untilFrame)
    {
        mDashUntil = untilFrame;
    }
    bool Dashing() const
    {
        return mAbe.mFrame <= mDashUntil && mDashUntil - mAbe.mFrame < 10;
    }
    // What the Sligs that shoot from the background of his screen have been seen to do
    const SligCycle& GetSligCycle() const
    {
        return mSligCycle;
    }
    u32 UnseenUse() const
    {
        return mUnseenUse;
    }
    void ScrabKilled(u32 id)
    {
        mWorld.mScrabsKilled.insert(id);
    }
    void ScrabParked(u32 id, s32 x)
    {
        mWorld.mScrabParked[id] = x;
    }
    // Something has been after him in this path within the last half minute (a pack of Paramites, a crowd of
    // Fleeches): a path that sets them on him once does it again on the next screen, and there is no walking
    // anywhere in it
    void NoteChased()
    {
        mChasedFrame = mAbe.mFrame;
        mChasedPath = mAbe.mPath;
    }
    bool RecentlyChased() const
    {
        return mChasedFrame != 0 && mChasedPath == mAbe.mPath && mAbe.mFrame - mChasedFrame < 900;
    }
    // Which game he is in
    bool IsAe() const
    {
        return mIsAe;
    }
    // A Fleech (its entity) a Scrab was made to kill
    void NoBomb(u32 id)
    {
        mWorld.mNoBomb.insert(id);
    }
    void FleechKilled(u32 id)
    {
        mWorld.mFleechKilled.insert(id);
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
    // The edges a way has to keep to for now, and no others (empty: any). For a stretch that was timed edge by edge.
    void KeepTo(const std::set<s32>& edges)
    {
        mOnlyEdges = edges;
    }
    bool KeepsTo() const
    {
        return !mOnlyEdges.empty();
    }
    // The power to go unseen is not chanted for on the way: the task at hand does that itself, where it wants it
    void HoldInvis(bool hold)
    {
        mHoldInvis = hold;
    }
    bool HoldsInvis() const
    {
        return mHoldInvis;
    }
    // The frame Abe was last where a Scrab that waits before it strikes could see him (0: never). For 210 frames
    // after that it is still after him, and strikes without waiting (Scrab::Brain_1_ChasingEnemy).
    u32 SeenByScrabAt() const
    {
        return mSeenByScrabAt;
    }
    // The ends of a Scrab's beat as seen: the last two places it has stopped at while it was not after him. False
    // until it has been seen to stop at both.
    bool ScrabBeat(u32 id, s32& lo, s32& hi) const
    {
        const auto it = mScrabBeat.find(id);
        if (it == mScrabBeat.end() || std::abs(it->second.second - it->second.first) < 100)
        {
            return false;
        }
        lo = std::min(it->second.first, it->second.second);
        hi = std::max(it->second.first, it->second.second);
        return true;
    }
    // The frame from which a Scrab has stood where it is (0: not known, or it moves)
    u32 ScrabStillSince(u32 id) const
    {
        const auto it = mScrabStill.find(id);
        return it == mScrabStill.end() ? 0 : it->second.second;
    }
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
    // The ending played for wants every Mudokon alive
    bool SavesMudokons() const
    {
        return mProfile.mEnding == Ending::eSaveAll;
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
    // A motion detector's beam would come over Abe before what the pilot is at is done: he keeps still (or comes
    // to a stop) until it has gone by
    bool BeamHolds() const;
    // Held up by a beam for a long time (a step never fits between its passes): a roll of a few cells right
    // behind it instead. True if one was begun.
    bool BeamRoll();
    s32 mBeamHeld = 0;
    bool mBeamRolling = false;
    s32 mBeamToX = 0;
    void ThrowMissed();
    void ThrowHit();
    bool ThrowIsBad(u32 mine, const ThrowSpot& spot) const;
    // The edge Abe is on his way along (-1: none), so a death can be laid at its door: two and it is banned
    void SetCurrentEdge(s32 edge)
    {
        mCurrentEdge = edge;
    }
    // pastScrabs: without the rule that keeps him off a floor a loose Scrab can get at him on
    NavGraph::EdgeCost EdgeCostFn(bool pastScrabs = false) const;
    void Log(const std::string& text);
    // See World::QuietFrames, with the beats this run has seen
    s32 QuietFrames(s32 x, s32 y, bool half) const;
    // Whether a flying Slig is coming Abe's way, is on his screen, or is on the side of him he has to go to
    bool FlierThreat() const;
    // Why the last answer of FlierThreat was yes, for the log
    const std::string& FlierWhy() const
    {
        return mFlierWhy;
    }
    // How many reflexes (things done at once, between the steps of a task) have run
    s32 Reflexes() const
    {
        return mReflexesRun;
    }
    // Its beat has been watched from one end to the other (or far enough to count on)
    // A Slig whose whole beat is on the screen Abe is on: it is never off it, so there is never quiet there
    bool BeatStaysOnScreen(const SligAt& slig) const;
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
    // AE: hurt, between two things to do, and nothing about that could hurt him more where he stands: he waits
    // for his health to come back (180 frames after the last shot) before he goes on. A shot takes a third.
    bool Resting();
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

    Profile mProfile;
    bool mAuto = false;
    // What the current task was planned as, and how often a planned action has failed
    std::string mTaskAction;
    // It is a rescue or a kill: the run goes on without it
    bool mTaskOptional = false;
    // Deaths while at each action
    std::map<std::string, s32> mActionDeaths;
    std::string mPlanText;
    s8 mLastSwitches[256] = {};
    s32 mDeadFrames = 0;
    s32 mStartWaits = 0;
    bool mThrowPending = false;
    u32 mThrowFrame = 0;
    u32 mLastLogFrame = 0;
    bool mMissRewind = false;
    s32 mMissRewinds = 0;
    bool mSweepPending = false;
    u32 mSweepFrame = 0;
    s32 mSweepDeaths = 0;
    s32 mSweepStopFrame = -100000;
    // Deaths per cell (x, y): the same place over and over is not a matter of luck
    std::map<std::pair<s32, s32>, s32> mDeathCells;
    u32 mThrowMine = 0;
    ThrowSpot mThrowSpot;
    // Where the rock thrown last was on each frame
    std::vector<ThrowArc::Point> mThrowSeen;
    s32 mCurrentEdge = -1;
    std::map<s32, s32> mEdgeDeaths;
    // Whether the run may take an exit to that path now
    bool ExitAllowed(BaseMap& map, const NavFeature& exit, bool* sideTrip);
    // The human option's last dawdle, so two don't follow each other
    bool mDawdled = false;
    bool mSkipPulse = false;
    bool mIsAe = true;

    BotState mState = BotState::eIdle;
    std::string mWhy;
    std::unique_ptr<Task> mTask;
    // Something that can't wait, done before the task goes on: a wired Mudokon tagging along slapped out of it
    std::unique_ptr<Task> mReflex;
    std::vector<s32> mPlanNodes;
    mutable std::string mFlierWhy;
    // Whether an invisible zone can be got to from a cell on foot in so many moves; way: how, cost: in about
    // how many frames
    bool ShadowNear(s32 node, s32 maxEdges, std::vector<s32>* way, s32* cost) const;
    s32 mReflexesRun = 0;
    std::unique_ptr<Task> MakeReflex();
    // The reflex for a Slig that has woken beside Abe: off its floor, if a way is at hand
    std::unique_ptr<Task> MakeFlee();
    // Off the floor of a Slog that is after him (or, in the air, that he is coming down next to)
    std::unique_ptr<Task> MakeSlogFlee();
    // The nearest way off a Slog's floor that leads away from it, from a cell of that floor; which way it starts,
    // and whether he would be off before the Slog has him
    bool SlogRefuge(const SlogAt& slog, s32 from, s32& refuge, bool& left, bool& inTime, s32& fromX) const;
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
        // Looking ahead: the tries made from here, and the bot's dice as they were
        Lookahead::Stretch mStretch;
        u64 mRng = 0;
        // What he pressed, a frame at a time, in the first try from here, up to where it failed: the later tries
        // press the same, begun later
        std::vector<u32> mPressed;
        bool mHasPressed = false;
        // What the bot remembered then (see BotMemory), and the edges it had ruled out, by path
        BotMemory mMemory;
        std::map<std::pair<s32, s32>, std::set<s32>> mBanned;
    };
    std::vector<Saved> mCheckpoints;
    bool mRestoring = false;
    s32 mAttempt = 0;
    s32 mDeaths = 0;
    s32 mRestores = 0;
    u32 mLastCheckpointFrame = 0;
    // Looking ahead (see Lookahead): the frames still to hold back in the try under way, his health a frame ago,
    // whether the try has just failed, how many have, how many since a save was last taken further on than any
    // before, and whether being hurt is put up with for now (nothing got through unhurt)
    s32 mHold = 0;
    // The buttons of the try under way that are still to be pressed (see Saved::mPressed), and how far it is
    std::vector<u32> mRepeat;
    std::size_t mRepeatAt = 0;
    s32 mLastHealthFp = 0;
    u32 mLastHurtFrame = 0;
    bool mTryFailed = false;
    // What it failed by, when not by his being hurt
    std::string mTryWhy;
    s32 mTries = 0;
    s32 mTriesSince = 0;
    s32 mTryFrames = 0;
    bool mTolerant = false;
    // How many saves there have to be left when it goes back one: two fewer than at the first failure since it
    // last got somewhere new (0: none has failed since)
    std::size_t mPopFloor = 0;
    s32 mTolerantSaves = 0;
    // What the try under way does differently from how the bot would play it (Lookahead::Way), whether it is a
    // try again at all, and how many saves somewhere new there have been since
    Lookahead::Way mVary = Lookahead::Way::eSame;
    bool mTryingAgain = false;
    s32 mVarySaves = 0;
    void StuckNow(const std::string& why);
    // Where the last saves were taken: one somewhere new is the tries having got him on
    std::set<std::tuple<s32, s32, s32, s32>> mSavePlaces;
    // The frame the try under way began to do something
    u32 mLastTryFrame = 0;
    void TryFailed(BaseMap& map, const std::string& what);

    std::vector<u32> mInputs;
    u32 mInputsStartFrame = 0;
    bool mInputsStarted = false;

    bool mTracing = false;
    std::string mTrace;
    // The move last written to it (written again only when it changes)
    std::string mTraceMove;
    std::string mLog;
    std::set<s32> mOnlyEdges;
};

} // namespace Bot
