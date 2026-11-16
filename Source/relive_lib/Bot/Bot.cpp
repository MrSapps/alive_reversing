#include "Bot.hpp"
#include "ThrowArc.hpp"
#include "../BaseMap.hpp"
#include "../GameType.hpp"
#include "../logger.hpp"
#include "../../AliveLibAE/Input.hpp"
#include <cstdio>

namespace Bot {

// Marks a line of the input log as held while a modal (a movie) ran, not for a game frame
static const u32 kModalInput = 0x80000000u;
static const s32 kMaxCheckpoints = 8;
static const s32 kAttemptsPerCheckpoint = 6;
static const u32 kCheckpointEveryFrames = 90;
// Looking ahead: how often a save is taken with a Slig awake on the screen, and how many tries may fail without a
// save somewhere new before being hurt is put up with (and as many again before it is given up)
static const u32 kCheckpointEveryFramesWatched = 30;
static const s32 kTriesPerObstacle = 600;
// The same in frames of the game played in tries that failed: for a way through unhurt, and for one at all
static const s32 kFramesPerObstacle = 6000;
static const s32 kPopsPerObstacleDying = 8;
static const s32 kFramesPerObstacleDying = 60000;
// and how many saves it goes back for a way through unhurt before that
static const s32 kPopsPerObstacle = 2;

Bot::Bot() = default;

Bot::~Bot()
{
    Input().mAutomationInput = 0;
}

void Bot::Do(const Prim& prim)
{
    mPendingPrim = prim;
    mPilotPending = true;
    mPilotForTask = false;
}

void Bot::Run(std::unique_ptr<Task> task)
{
    mTask = std::move(task);
    mState = BotState::eRunning;
    mWhy.clear();
    mAttempt = 0;
    mCheckpoints.clear();
    mLastCheckpointFrame = 0;
}

void Bot::Start(const Profile& profile)
{
    mProfile = profile;
    mRng.Seed(profile.mSeed);
    mAuto = true;
    mTask.reset();
    mState = BotState::eRunning;
    mWhy.clear();
    mAttempt = 0;
    mCheckpoints.clear();
    mLastCheckpointFrame = 0;
    mFailedActions.clear();
    mHold = 0;
    mLastHealthFp = 0;
    mLastHurtFrame = 0;
    mTryFailed = false;
    mTries = 0;
    mTriesSince = 0;
    mTolerant = false;
    mVary = Lookahead::Way::eSame;
    mTryingAgain = false;
    mSavePlaces.clear();
    mPopFloor = 0;
    mTryFrames = 0;
    mRepeat.clear();
    mRepeatAt = 0;
    mStartWaits = 0;
    // (the level's graph depends on where the profile says the run has been: made again, in case something was
    // done with the bot before the profile was given, as a script's bot_goto does)
    mLevel = LevelGraph();
    Log("start profile " + profile.Text());
}

void Bot::Stop()
{
    mAuto = false;
    mTask.reset();
    mPilotActive = false;
    mPilotPending = false;
    mGoToPending = false;
    mState = BotState::eIdle;
}

u32 Bot::SpeakHello() const
{
    return Btn::eSpeak1;
}

u32 Bot::SpeakFollow() const
{
    return Btn::eSpeak2;
}

u32 Bot::SpeakSorry() const
{
    return Btn::eSpeak7;
}

u32 Bot::SpeakSlap() const
{
    return Btn::eSpeak5;
}

u32 Bot::SpeakWork() const
{
    return Btn::eSpeak4;
}

u32 Bot::SpeakAll() const
{
    return Btn::eSpeak6;
}

u32 Bot::SpeakWait() const
{
    return Btn::eSpeak3;
}

std::unique_ptr<Task> Bot::MakeTask(const Domain& domain, const Goap::Action& action)
{
    const DomainStep& step = domain.mSteps[action.mTag];
    switch (step.mKind)
    {
        case StepKind::eCross:
        {
            // (or the cell past the Scrab's reach that a crossing onto its floor makes for)
            const s32 to = step.mOther != 0 ? static_cast<s32>(step.mOther) - 1 : mKnow->mNav.Edges()[step.mEdge].mTo;
            std::string note;
            bool wary = ScrabWaryTask::Wanted(*this, to, 0, &note, step.mOther != 0);
            // (with Fleeches awake a Scrab's floor is not waited at, unless there is no other way at all)
            if (!wary && step.mOther == 0 && AbeNode() >= 0)
            {
                std::vector<s32> plain;
                if (!mKnow->mNav.FindPath(AbeNode(), to, EdgeCostFn(), plain))
                {
                    wary = ScrabWaryTask::Wanted(*this, to, 0, &note, true);
                }
            }
            if (!note.empty())
            {
                Log(note + (wary ? "" : ": not enough, so by the old way"));
            }
            if (wary)
            {
                return std::make_unique<ScrabWaryTask>(action.mName, to, 0, 0);
            }
            return std::make_unique<GoToTask>(action.mName, [to](s32 node)
                                              { return node == to; });
        }
        case StepKind::eUse:
        {
            const u32 entity = step.mEntity;
            const s32 value = action.mEffects[0].mValue;
            std::string note;
            const bool wary = ScrabWaryTask::Wanted(*this, -1, entity, &note);
            if (!note.empty())
            {
                Log(note + (wary ? "" : ": not enough, so by the old way"));
            }
            if (wary)
            {
                return std::make_unique<ScrabWaryTask>(action.mName, -1, entity, value);
            }
            // (a lever used from the trap door on its right: Domain)
            const bool right = step.mOther == 1;
            return std::make_unique<ClearSligTask>(entity, [entity, value, right]() -> std::unique_ptr<Task>
                                                   {
                                                       auto use = std::make_unique<UseTask>(entity, value);
                                                       if (right)
                                                       {
                                                           use->FixSide(1);
                                                       }
                                                       return use;
                                                   });
        }
        case StepKind::eRescue:
        {
            const u32 entity = step.mEntity;
            const u32 other = step.mOther;
            return std::make_unique<ClearSligTask>(entity, [entity, other]() -> std::unique_ptr<Task>
                                                   { return std::make_unique<RescueTask>(entity, other); });
        }
        case StepKind::eSend:
            return std::make_unique<SendToWheelTask>(step.mEntity, step.mOther);
        case StepKind::eExit:
            return std::make_unique<ExitTask>(step.mEdge);
        case StepKind::eTogether:
            return std::make_unique<WorkTogetherTask>(step.mEntity);
        case StepKind::eKill:
        {
            const u32 entity = step.mEntity;
            return std::make_unique<ClearSligTask>(entity, [entity]() -> std::unique_ptr<Task>
                                                   { return std::make_unique<KillTask>(entity); }, true);
        }
        case StepKind::eRocks:
            return std::make_unique<GetRocksTask>(step.mEntity);
        case StepKind::eBlow:
            return std::make_unique<BlowMineTask>(step.mEntity, step.mEdge);
        case StepKind::eDisarm:
        {
            const Entity* uxb = mWorld.Find(step.mEntity);
            const NavNode& stand = mKnow->mNav.Nodes()[step.mEdge];
            return std::make_unique<DisarmTask>(uxb ? uxb->CentreX() : stand.mX, stand.mY, stand.mHalf, step.mEdge);
        }
        case StepKind::eFartBomb:
            return std::make_unique<FartBombTask>(step.mEntity, step.mOther, step.mEdge);
        case StepKind::eDrink:
            return std::make_unique<DrinkTask>(step.mEntity);
        case StepKind::eGrenade:
            return std::make_unique<GrenadeSligTask>(step.mEntity, step.mEdge);
        case StepKind::eFlierBomb:
            return std::make_unique<FlierBombTask>(step.mEntity, step.mEdge);
        case StepKind::eFlierPull:
            return std::make_unique<FlierBombTask>(step.mEntity, step.mEdge, step.mOther);
        case StepKind::eFlierLock:
            return std::make_unique<FlierBombTask>(step.mEntity, step.mEdge, 0, step.mOther);
        case StepKind::eSligLock:
            return std::make_unique<SligLockTask>(step.mEntity, step.mOther, static_cast<u32>(step.mEdge));
        case StepKind::eRide:
            return std::make_unique<RideCarTask>(step.mEntity, step.mEdge);
        case StepKind::eScrabRampage:
            return std::make_unique<ScrabRampageTask>(step.mEntity, step.mEdge);
        case StepKind::eScrabKill:
            return std::make_unique<ScrabKillTask>(step.mEntity, step.mOther, static_cast<u32>(step.mEdge));
        case StepKind::eScrabHunt:
            return std::make_unique<ScrabHuntTask>(step.mEntity, step.mEdge);
        case StepKind::eScrabLure:
            return std::make_unique<ScrabLureTask>(step.mEdge);
        case StepKind::eMeatUse:
            return std::make_unique<MeatLureTask>(step.mEntity, step.mOther, action.mEffects[0].mKey == Key::kInvis ? 1 : action.mEffects[0].mValue);
        case StepKind::eParamitePull:
            return std::make_unique<ParamitePullTask>(step.mEntity, step.mOther, step.mEdge);
        case StepKind::eFleechLure:
            return std::make_unique<FleechLureTask>(step.mEntity, 1);
        case StepKind::eParamiteTour:
            return std::make_unique<ParamiteTourTask>(step.mOther, step.mEdge);
        case StepKind::eSligDrop:
            return std::make_unique<SligDropTask>(step.mEntity, step.mOther, step.mEdge);
        case StepKind::eStepOn:
        {
            return std::make_unique<StepOnTask>(step.mEntity, step.mEdge);
        }
        case StepKind::eParamiteClear:
            return std::make_unique<ParamiteTourTask>(step.mOther, step.mEdge, true);
        case StepKind::eMeatFeed:
            return std::make_unique<MeatFeedTask>(step.mEdge);
        case StepKind::eFleechPen:
            return std::make_unique<FleechPenTask>(step.mEntity);
        case StepKind::eFleechLead:
            return std::make_unique<FleechLeadTask>(step.mEntity);
        case StepKind::eScrabPen:
            return std::make_unique<ScrabPenTask>(step.mEntity, static_cast<s32>(step.mOther), step.mEdge != 0);
        case StepKind::eLogic:
            break;
    }
    return nullptr;
}

bool Bot::Replan(BaseMap& map)
{
    const PathKnowledge* know = mKnow;
    const s32 abeNode = AbeNode();
    if (abeNode < 0 || (mAbe.mMove == Move::eAir && !mAbe.mOnGround))
    {
        // In the air (out of a bird portal as well, which leaves him at a floor's height before he has landed), on
        // a lift, mid door: look again shortly
        // (out of a bird portal he hangs in the air for a second or two)
        if (++mLogicWaits > (abeNode < 0 ? 60 : 400))
        {
            Stuck("no_plan: Abe isn't on a floor the nav graph knows, at " + std::to_string(mAbe.mX) + "," + std::to_string(mAbe.mY));
            return false;
        }
        mTask = std::make_unique<GoToTask>("wait to land", [](s32)
                                           { return true; });
        mTaskAction.clear();
        return true;
    }

    // A mine has gone since the last look: throws that had nothing to hit but the mine in front may work now
    const s32 mines = BlowMineTask::LiveMines(mWorld);
    if (mines < mLastMines)
    {
        for (auto it = mFailedActions.begin(); it != mFailedActions.end();)
        {
            it = it->first.compare(0, 8, "Blow up ") == 0 ? mFailedActions.erase(it) : std::next(it);
        }
        // (and the mines given up on are worth another go)
        for (const Entity& e : mWorld.mEntities)
        {
            if (e.mKind == EntityKind::eMine && !e.mGone)
            {
                mExplored.erase({mAbe.mPath, Key::Switch(e.mSwitch)});
            }
        }
    }
    mLastMines = mines;

    // A switch tried for the sake of it has changed: don't try it again
    for (s32 i = 0; i < 256; i++)
    {
        if (mWorld.mSwitches[i] != mLastSwitches[i])
        {
            mExplored.insert({mAbe.mPath, Key::Switch(i)});
            mLastSwitches[i] = mWorld.mSwitches[i];
        }
    }
    const auto allowed = [know](s32 edge)
    {
        if (know->mBannedEdges.count(edge) != 0)
        {
            return false;
        }
        // (running jumps taken together are for the way there, with a flying Slig about: to the plan they are
        // the same crossings as the jumps one by one)
        if (know->mNav.Edges()[edge].mX2 != 0)
        {
            return false;
        }
        // (onto a floor a loose Scrab can get at him on: Domain::Build wants him unseen for it)
        return true;
    };
    Domain domain = Domain::Build(mKnow->mNav, mWorld, abeNode, allowed, mProfile.mEnding == Ending::eKillAll);
    // A flying Slig that was to be got rid of before its stretch is gone into (FeatureKind::eDanger), with no fart
    // that can be got flown at it: got by as it is after all. The Mines' path 2 has a Brew machine and a flying
    // Slig, and the machine is past the Slig.
    bool passed = false;
    for (Entity& e : mWorld.mEntities)
    {
        if (e.mKind != EntityKind::eFlier || e.mInputs[0] != 1 || mWorld.Switch(e.mSwitch))
        {
            continue;
        }
        // (no way to get one flown at it from where he is, that is: the plan for just that)
        bool fart = false;
        for (const DomainStep& s : domain.mSteps)
        {
            fart = fart || (s.mKind == StepKind::eFartBomb && s.mEntity == e.mId);
        }
        fart = fart && Goap::Planner::Find(domain.mStart, {{Key::Switch(e.mSwitch), 1}}, domain.mActions).mFound;
        if (!fart)
        {
            Log("no fart can be flown at FlyingSlig#" + std::to_string(e.mId) + ": got by as it is");
            e.mInputs[0] = 0;
            passed = true;
        }
    }
    if (passed)
    {
        domain = Domain::Build(mKnow->mNav, mWorld, abeNode, allowed, mProfile.mEnding == Ending::eKillAll);
    }
    // Actions that keep failing are left out, and so are ways out that lead back to where the run has been
    std::vector<Goap::Action> actions;
    if (mArrivedKey != std::pair<s32, s32>{mAbe.mLevel, mAbe.mPath})
    {
        mArrivedKey = {mAbe.mLevel, mAbe.mPath};
        mArrivedX = mAbe.mX;
        mArrivedY = mAbe.mY;
        // (out of the path he backed out of: it may be come into again, by another way, and backed out of again)
        if (mBackFor != mAbe.mPath)
        {
            mBackFor = -1;
        }
    }
    for (const Goap::Action& a : domain.mActions)
    {
        const auto failed = mFailedActions.find(a.mName);
        if (failed != mFailedActions.end() && failed->second >= 2)
        {
            // For good if it killed Abe; else until he is somewhere else (no route from one place says
            // nothing about another)
            const auto deaths = mActionDeaths.find(a.mName);
            const auto where = mFailedWhere.find(a.mName);
            const bool moved = where != mFailedWhere.end() && (std::abs(where->second.first - mAbe.mX) > 400 || std::abs(where->second.second - mAbe.mY) > 150);
            // (and not for ever: what has failed six times over is left alone)
            const auto total = mFailedTotal.find(a.mName);
            if ((deaths != mActionDeaths.end() && deaths->second >= 3) || !moved || (total != mFailedTotal.end() && total->second >= 6))
            {
                continue;
            }
            mFailedActions.erase(failed);
        }
        const DomainStep& step = domain.mSteps[a.mTag];
        if (step.mKind == StepKind::eExit)
        {
            const NavFeature& f = mKnow->mNav.Input().mFeatures[mKnow->mNav.Exits()[step.mEdge].mFeature];
            // The way he came in, with another way out to the same place: that other one is the one meant
            if (mBackFor != mAbe.mPath && mKnow->mNav.Exits()[step.mEdge].mNode >= 0 && std::abs((f.mX1 + f.mX2) / 2 - mArrivedX) <= 40 && std::abs(f.mY2 - mArrivedY) <= 40)
            {
                bool twin = false;
                for (const NavExit& other : mKnow->mNav.Exits())
                {
                    const NavFeature& g = mKnow->mNav.Input().mFeatures[other.mFeature];
                    twin = twin || (other.mFeature != mKnow->mNav.Exits()[step.mEdge].mFeature && g.mDestLevel == f.mDestLevel && g.mDestPath == f.mDestPath);
                }
                if (twin)
                {
                    continue;
                }
            }
            // (backing out: the way he came in, wherever in the level it leads)
            if (mBackFor == mAbe.mPath && mKnow->mNav.Exits()[step.mEdge].mNode >= 0 && std::abs((f.mX1 + f.mX2) / 2 - mArrivedX) <= 40 && std::abs(f.mY2 - mArrivedY) <= 40)
            {
                actions.push_back(a);
                continue;
            }
            bool sideTrip = false;
            if (!ExitAllowed(map, f, &sideTrip))
            {
                continue;
            }
            if (sideTrip)
            {
                // Before the way on, which may not lead back
                actions.push_back(a);
                actions.back().mCost = 1;
                continue;
            }
        }
        actions.push_back(a);
    }

    // The goals in the order they matter: the Mudokons this ending rescues (the cheapest first), then the way out
    std::vector<std::pair<std::string, std::vector<Goap::Fact>>> goals;
    u32 mudIndex = 0;
    for (const Entity& e : mWorld.mEntities)
    {
        if (e.mKind != EntityKind::eMudokon)
        {
            continue;
        }
        if (!e.mGone && mProfile.Rescues(mudIndex))
        {
            goals.push_back({"Save Mudokon#" + std::to_string(e.mId), {{Key::Mud(e.mId), 1}}});
        }
        else if (!e.mGone && mProfile.Kills(mudIndex))
        {
            goals.push_back({"Kill Mudokon#" + std::to_string(e.mId), {{Key::Mud(e.mId), 2}}});
        }
        mudIndex++;
    }

    // The ones that look nearest first: after a few that can be done the rest aren't worth the thinking
    std::vector<std::pair<s32, std::size_t>> order;
    std::string unreachable;
    for (std::size_t i = 0; i < goals.size(); i++)
    {
        const s32 est = Goap::Planner::Short(domain.mStart, goals[i].second, actions);
        if (est < 0)
        {
            unreachable += goals[i].first + ": can't be had from here; ";
            continue;
        }
        order.push_back({est, i});
    }
    std::stable_sort(order.begin(), order.end());

    // The plan in hand still holds from here (what is left of it can be done as things are and gets its goal):
    // on with it, without thinking it all out again
    // (not when a Mudokon can be got at that couldn't be when it was made, and the plan is for something else)
    const bool keep = mKeptForMudokon || order.size() <= mKeptOpen;
    Goap::Plan best;
    std::string bestGoal;
    std::string misses;
    for (std::size_t skip = 0; keep && skip < mKeptPlan.size() && !best.mFound; skip++)
    {
        Goap::Plan kept;
        Goap::State state = domain.mStart;
        bool holds = true;
        for (std::size_t i = skip; i < mKeptPlan.size() && holds; i++)
        {
            holds = false;
            for (std::size_t a = 0; a < actions.size(); a++)
            {
                if (actions[a].mName == mKeptPlan[i] && Goap::Planner::Can(state, actions[a]))
                {
                    const Goap::State before = state;
                    Goap::Planner::Do(state, actions[a]);
                    holds = !(state == before);
                    kept.mSteps.push_back(static_cast<s32>(a));
                    kept.mCost += std::max(1, actions[a].mCost);
                    break;
                }
            }
        }
        if (holds && !domain.mStart.HasAll(mKeptGoal) && state.HasAll(mKeptGoal))
        {
            kept.mFound = true;
            best = kept;
            bestGoal = mKeptGoalName;
        }
    }
    const bool keptOn = best.mFound;

    misses += unreachable;
    const std::size_t openGoals = order.size();
    if (keptOn)
    {
        order.clear();
    }
    // Is there a way out as things stand? Then nothing is done to a Mudokon that would leave none: some are
    // needed where they are (to turn a wheel, or because only rescuing them opens the door)
    const Goap::Plan leave = keptOn ? Goap::Plan{} : Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, actions);
    const bool wayOut = leave.mFound;
    s32 plansFound = 0;
    std::vector<Goap::Fact> bestFacts;
    for (const auto& entry : order)
    {
        if (plansFound >= 3)
        {
            break;
        }
        const auto& goal = goals[entry.second];
        // Something else has a use for this Mudokon as it is (its rescue sets a switch, or it is one of the hands
        // for a set of wheels): it is left for that, whether or not the whole way out is known yet
        bool wanted = false;
        const Goap::Fact asItIs{goal.second[0].mKey, 0};
        for (const Goap::Action& a : actions)
        {
            bool needs = false;
            bool same = false;
            bool more = false;
            for (const Goap::Fact& p : a.mPre)
            {
                needs = needs || p == asItIs;
            }
            for (const Goap::Fact& e : a.mEffects)
            {
                same = same || e == goal.second[0];
                more = more || e.mKey != asItIs.mKey;
            }
            wanted = wanted || (needs && !same && more);
        }
        if (wanted)
        {
            misses += goal.first + ": something else needs it as it is; ";
            continue;
        }
        const Goap::Plan plan = Goap::Planner::Find(domain.mStart, goal.second, actions);
        if (plan.mFound && !plan.mSteps.empty() && (!best.mFound || plan.mCost < best.mCost))
        {
            if (wayOut && !Goap::Planner::Find(Goap::Planner::Apply(domain.mStart, plan, actions), {{Key::kExited, 1}}, actions).mFound)
            {
                misses += goal.first + ": the way out needs it as it is; ";
                continue;
            }
            best = plan;
            bestGoal = goal.first;
            bestFacts = goal.second;
            plansFound++;
        }
        else if (!plan.mFound)
        {
            misses += goal.first + ": " + plan.mMiss + "; ";
        }
    }
    // What a door of another path wants and can be set here: that is what he is in this path for
    for (const s32 sw : best.mFound ? std::vector<s32>() : mLevel.Wanted(mAbe.mPath))
    {
        const std::vector<Goap::Fact> facts{{Key::Switch(sw), 1}};
        if (mWorld.Switch(sw) != 0 || mErrandsLeft.count({mAbe.mPath, sw}) != 0)
        {
            continue;
        }
        const Goap::Plan plan = Goap::Planner::Find(domain.mStart, facts, actions);
        if (plan.mFound && !plan.mSteps.empty() && (!best.mFound || plan.mCost < best.mCost))
        {
            best = plan;
            bestGoal = "Set switch " + std::to_string(sw) + " for a door of another path";
            bestFacts = facts;
        }
    }
    // (and while one of them is still to be set here, the way out is not what he wants: he looks for a way to it)
    bool errandOpen = false;
    for (const s32 sw : mLevel.Errands(mAbe.mPath))
    {
        errandOpen = errandOpen || (mWorld.Switch(sw) == 0 && mErrandsLeft.count({mAbe.mPath, sw}) == 0);
    }
    if (!best.mFound)
    {
        const Goap::Plan& plan = leave;
        if (plan.mFound && !plan.mSteps.empty() && !errandOpen)
        {
            best = plan;
            bestGoal = "Leave the path";
            bestFacts = {{Key::kExited, 1}};
        }
        else
        {
            // No way out is known yet: change what can be changed, a door it opens may show one. Switches the
            // run has already set once are left alone, so it can't go round in circles.
            // (the nearest looking first, and the first two that can be done are enough to choose from)
            s32 others = 0;
            for (const Goap::Action& a : actions)
            {
                others += a.mName.rfind("Cross ", 0) == 0 ? 0 : 1;
            }
            if (mWhyNoWayOut != mAbe.mPath || mWhyCount != others)
            {
                mWhyNoWayOut = mAbe.mPath;
                mWhyCount = others;
                // (with what can be done here other than cross, to see which need nothing gives)
                std::string can;
                for (const Goap::Action& a : actions)
                {
                    if (a.mName.rfind("Cross ", 0) == 0)
                    {
                        continue;
                    }
                    can += "\n  " + a.mName + " needs";
                    for (const Goap::Fact& f : a.mPre)
                    {
                        can += " " + std::to_string(f.mKey) + "=" + std::to_string(f.mValue);
                    }
                    can += " gives";
                    for (const Goap::Fact& f : a.mEffects)
                    {
                        can += " " + std::to_string(f.mKey) + "=" + std::to_string(f.mValue);
                    }
                }
                Log("no way out known: " + plan.mMiss + ". " + domain.Frontier() + can);
            }
            std::vector<std::pair<s32, Goap::Fact>> tries;
            for (const Goap::Fact& fact : domain.Reachable())
            {
                if (fact.mKey < 1000 || fact.mKey >= 1000000 || mExplored.count({mAbe.mPath, fact.mKey}))
                {
                    continue;
                }
                // (a UXB is turned off where the way goes through it, not to see what that does: it does nothing else)
                bool uxb = false;
                for (const Entity& e : mWorld.mEntities)
                {
                    uxb = uxb || (e.mKind == EntityKind::eUxb && Key::Switch(e.mSwitch) == fact.mKey);
                }
                if (uxb)
                {
                    continue;
                }
                const s32 est = Goap::Planner::Short(domain.mStart, {fact}, actions);
                if (est >= 0)
                {
                    tries.push_back({est, fact});
                }
            }
            std::stable_sort(tries.begin(), tries.end(), [](const std::pair<s32, Goap::Fact>& a, const std::pair<s32, Goap::Fact>& b)
                             { return a.first < b.first; });
            for (const auto& entry : tries)
            {
                if (plansFound >= 2)
                {
                    break;
                }
                const Goap::Fact fact = entry.second;
                const Goap::Plan sub = Goap::Planner::Find(domain.mStart, {fact}, actions);
                if (sub.mFound && !sub.mSteps.empty() && (!best.mFound || sub.mCost < best.mCost))
                {
                    best = sub;
                    bestGoal = "Try switch " + std::to_string(fact.mKey - 1000) + "=" + std::to_string(fact.mValue);
                    bestFacts = {fact};
                    mExploring = fact.mKey;
                    plansFound++;
                }
                else if (!sub.mFound)
                {
                    misses += "switch " + std::to_string(fact.mKey - 1000) + ": " + sub.mMiss + "; ";
                }
            }
            // Nothing left to switch, and the way out of the level this path has is not to be had: the level
            // goes on through its other paths (Feeco's lobby, whose own way out is behind gates that the ends
            // of two other paths open)
            if (!best.mFound && !errandOpen && mLevel.Shut(mAbe.mPath))
            {
                Log("the way out of the level from this path is shut: on through the level's other paths (" + std::to_string(mLevel.Distance(mAbe.mPath)) + " away)");
                mTask = std::make_unique<GoToTask>("think again", [](s32)
                                                   { return true; });
                mTaskAction.clear();
                return true;
            }
            // Nothing left to switch, and a way out of this path into one of the level he has not been in, from
            // which the level goes on: that before any wandering here (the Mudomo vaults' path 7: its way on, a
            // bird portal, is only got to by coming back into it through path 10)
            if (!best.mFound && !errandOpen && mNewPathFor != mAbe.mPath && mKnow)
            {
                bool onward = false;
                for (const NavExit& x : mKnow->mNav.Exits())
                {
                    const NavFeature& f = mKnow->mNav.Input().mFeatures[x.mFeature];
                    onward = onward || (f.mDestPath >= 0 && (f.mDestLevel < 0 || f.mDestLevel == mAbe.mLevel) && mVisitedPaths.count({mAbe.mLevel, f.mDestPath}) == 0 && mLevel.Distance(f.mDestPath) >= 0);
                }
                if (onward)
                {
                    mNewPathFor = mAbe.mPath;
                    Log("no way on from here: any path of this level not yet been in will do (" + leave.mMiss + ". " + domain.Frontier() + ")");
                    mTask = std::make_unique<GoToTask>("think again", [](s32)
                                                       { return true; });
                    mTaskAction.clear();
                    return true;
                }
            }
            // Nothing left to switch: somewhere he has not been yet (through a door that is open now, say). The
            // way out may show from there.
            mRegionsBeen.insert({mAbe.mPath, domain.mStart.Get(Key::kRegion)});
            for (auto it = mCellsBeen.lower_bound({mAbe.mLevel * 1000 + mAbe.mPath, 0}); it != mCellsBeen.end() && it->first == mAbe.mLevel * 1000 + mAbe.mPath; ++it)
            {
                if (it->second < static_cast<s32>(domain.mRegions.size()))
                {
                    mRegionsBeen.insert({mAbe.mPath, domain.mRegions[it->second] + 1});
                }
            }
            if (!best.mFound)
            {
                for (s32 pass = 0; pass < 2 && !best.mFound; pass++)
                {
                const bool doorPass = pass == 0;
                for (s32 region : domain.ReachableRegions())
                {
                    const std::pair<s32, s32> key{mAbe.mPath, region};
                    if (mRegionsBeen.count(key) || mRegionTries[key] >= 3 || plansFound >= 2)
                    {
                        continue;
                    }
                    // (a cell or two with nothing leading on is no place to look for a way out: the cell a lock
                    // stands in, beyond the trap door it is slapped from)
                    {
                        s32 cells = 0;
                        for (s32 r : domain.mRegions)
                        {
                            cells += r + 1 == region ? 1 : 0;
                        }
                        if (cells <= 2)
                        {
                            continue;
                        }
                    }
                    const Goap::Fact fact{Key::kRegion, region};
                    const Goap::Plan sub = Goap::Planner::Find(domain.mStart, {fact}, actions);
                    // Through a door or a well first: along a floor nothing has led him onto is where what the
                    // doors were shut against lives (Fleeches, in the Mudanchee vault)
                    bool byDoor = false;
                    for (s32 stepIdx : sub.mSteps)
                    {
                        const DomainStep& st = domain.mSteps[actions[stepIdx].mTag];
                        const EdgeKind kind = st.mKind == StepKind::eCross ? mKnow->mNav.Edges()[st.mEdge].mKind : EdgeKind::eWalk;
                        byDoor = byDoor || kind == EdgeKind::eDoor || kind == EdgeKind::eWell || kind == EdgeKind::ePortal;
                    }
                    if (sub.mFound && !sub.mSteps.empty() && byDoor != doorPass)
                    {
                        continue;
                    }
                    if (sub.mFound && !sub.mSteps.empty() && (!best.mFound || sub.mCost < best.mCost))
                    {
                        best = sub;
                        bestGoal = "Go somewhere new (region " + std::to_string(region) + ")";
                        bestFacts = {fact};
                        mRegionGoal = key;
                        plansFound++;
                    }
                }
                }
                if (best.mFound)
                {
                    mRegionTries[mRegionGoal]++;
                }
            }
            if (!best.mFound && errandOpen)
            {
                // No way to what the door elsewhere wants: that is given up, and the path is left like any other
                for (const s32 sw : mLevel.Errands(mAbe.mPath))
                {
                    if (mWorld.Switch(sw) == 0 && mErrandsLeft.insert({mAbe.mPath, sw}).second)
                    {
                        Log("no way to set switch " + std::to_string(sw) + " for the door of another path: given up (" + misses + ")");
                    }
                }
                mTask = std::make_unique<GoToTask>("think again", [](s32)
                                                   { return true; });
                mTaskAction.clear();
                return true;
            }
            if (!best.mFound)
            {
                // What failed from somewhere else may work from here: once in a while everything but what
                // killed Abe gets another go
                if (!mFailedActions.empty() && mAbe.mFrame - mForgaveFrame > 900)
                {
                    mForgaveFrame = mAbe.mFrame;
                    for (auto it = mFailedActions.begin(); it != mFailedActions.end();)
                    {
                        const auto deaths = mActionDeaths.find(it->first);
                        const auto total = mFailedTotal.find(it->first);
                        const bool forGood = (deaths != mActionDeaths.end() && deaths->second >= 3) || (total != mFailedTotal.end() && total->second >= 6);
                        it = forGood ? std::next(it) : mFailedActions.erase(it);
                    }
                    Log("no plan: trying again what failed before");
                    mTask = std::make_unique<GoToTask>("think again", [](s32)
                                                       { return true; });
                    mTaskAction.clear();
                    return true;
                }
                // The way on shut by switches nothing here sets (a hub whose door wants its side paths done): into
                // a path of this level he has not been in yet
                if (mNewPathFor != mAbe.mPath)
                {
                    mNewPathFor = mAbe.mPath;
                    Log("no way on from here: any path of this level not yet been in will do");
                    mTask = std::make_unique<GoToTask>("think again", [](s32)
                                                       { return true; });
                    mTaskAction.clear();
                    return true;
                }
                // Nothing at all: back out the way he came in
                // (three times at most from one path: he can come into it again by another door)
                if (mBackFor != mAbe.mPath && mBackOuts[{mAbe.mLevel, mAbe.mPath}] < 3)
                {
                    mBackOuts[{mAbe.mLevel, mAbe.mPath}]++;
                    mBackFor = mAbe.mPath;
                    Log("no way on from here: back out the way he came in");
                    mTask = std::make_unique<GoToTask>("think again", [](s32)
                                                       { return true; });
                    mTaskAction.clear();
                    return true;
                }
                // (what there was to plan with, for whoever reads the log)
                Log("the actions there were:\n" + domain.Text());
                Stuck("no_plan: Leave the path: " + plan.mMiss + ". " + domain.Frontier() + (misses.empty() ? "" : " Also no plan for: " + misses));
                return false;
            }
        }
    }
    else if (!keptOn)
    {
        mExploring = 0;
    }
    if (bestGoal.rfind("Try switch", 0) != 0)
    {
        mExploring = 0;
    }

    mPlanText.clear();
    if (!keptOn)
    {
        mKeptGoal = bestFacts;
        mKeptGoalName = bestGoal;
        mKeptForMudokon = !bestFacts.empty() && bestFacts[0].mKey >= 1000000;
        mKeptOpen = openGoals;
    }
    // The power to go invisible, and a step ahead that wants him unseen: kept for that, and used as late as can
    // be (it lasts some 390 frames and the way from there to safety is about that long): before that step or
    // the one that leads into it. Not in a path with security orbs: there it is used where it is got (the
    // screens after have an orb each, the walkthrough's "chant on this screen").
    mInvisSaved = false;
    mInvisChantNow = false;
    mInvisChantSoon = false;
    mInvisAtOnce = false;
    mInvisCross = false;
    mUnseenUse = 0;
    {
        bool orbs = false;
        for (const Entity& e : mWorld.mEntities)
        {
            orbs = orbs || (e.mKind == EntityKind::eSecurityOrb && !e.mGone);
        }
        std::size_t done = 0;
        // (the step at hand, if it is a crossing that ends where a Scrab gets at him: that one leads into it)
        bool leadsIn = false;
        for (std::size_t i = 0; !orbs && i < best.mSteps.size(); i++)
        {
            const Goap::Action& a = actions[best.mSteps[i]];
            if (done == 0 && domain.mSteps[a.mTag].mKind == StepKind::eCross && mKnow)
            {
                const NavNode& to = mKnow->mNav.Nodes()[mKnow->mNav.Edges()[domain.mSteps[a.mTag].mEdge].mTo];
                leadsIn = Domain::HuntedAt(mKnow->mNav, mWorld, to.mX, to.mY, to.mHalf);
            }
            bool unseen = false;
            for (const Goap::Fact& f : a.mPre)
            {
                unseen = unseen || (f.mKey == Key::kInvisNow && f.mValue == 1);
            }
            if (unseen)
            {
                mInvisSaved = true;
                // (a crossing: when it is the step at hand, and then where the way goes onto the Scrab's floor,
                // see GoToTask. Anything else: from the step before it as well, if that one ends on the Scrab's
                // floor. One that only leads over another Scrab's floor on its way does not: that floor is
                // crossed by its Scrab's beat, and the power kept for the thing itself.)
                const bool cross = domain.mSteps[a.mTag].mKind == StepKind::eCross;
                mInvisChantNow = done == 0 || (!cross && done == 1 && leadsIn);
                mInvisChantSoon = done <= 1;
                mUnseenUse = cross ? 0 : domain.mSteps[a.mTag].mEntity;
                mInvisCross = cross;
                break;
            }
            // (what the game does by itself is no step of his)
            done += domain.mSteps[a.mTag].mKind == StepKind::eLogic ? 0 : 1;
        }
    }
    // (in a path with orbs: got and used at once when the step at hand is the one that wants him unseen)
    if (!mInvisSaved)
    {
        for (std::size_t i = 0; i < best.mSteps.size(); i++)
        {
            const Goap::Action& a = actions[best.mSteps[i]];
            if (domain.mSteps[a.mTag].mKind == StepKind::eLogic)
            {
                continue;
            }
            if (std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::kInvisNow, 1}) != a.mPre.end())
            {
                mInvisSaved = true;
                mInvisChantNow = true;
                mInvisChantSoon = true;
                mInvisAtOnce = true;
                mInvisCross = domain.mSteps[a.mTag].mKind == StepKind::eCross;
            }
            break;
        }
    }
    mKeptPlan.clear();
    for (s32 stepIdx : best.mSteps)
    {
        mPlanText += (mPlanText.empty() ? "" : ", ") + actions[stepIdx].mName;
        mKeptPlan.push_back(actions[stepIdx].mName);
    }
    // Where the next two things to do are (the crossings on the way to them don't count)
    mPlanNodes.clear();
    for (s32 stepIdx : best.mSteps)
    {
        const DomainStep& planStep = domain.mSteps[actions[stepIdx].mTag];
        if (planStep.mKind == StepKind::eCross || planStep.mKind == StepKind::eLogic)
        {
            continue;
        }
        if (planStep.mKind == StepKind::eExit && planStep.mEdge >= 0 && planStep.mEdge < static_cast<s32>(mKnow->mNav.Exits().size()))
        {
            mPlanNodes.push_back(mKnow->mNav.Exits()[planStep.mEdge].mNode);
        }
        else if (const Entity* target = mWorld.Find(planStep.mEntity))
        {
            mPlanNodes.push_back(Domain::UseNode(mKnow->mNav, *target, nullptr));
        }
        else
        {
            mPlanNodes.push_back(-1);
        }
        if (mPlanNodes.size() >= 2)
        {
            break;
        }
    }
    const Goap::Action& first = actions[best.mSteps[0]];
    if (first.mName != mTaskAction)
    {
        Log("goal " + bestGoal + " plan=[" + mPlanText + "]" + (misses.empty() ? "" : " (no plan for: " + misses + ")"));
    }
    // (the task before it gone first: what it held the route to is let go of before the next is looked for)
    mTask.reset();
    mTask = MakeTask(domain, first);
    // Crossings on foot that follow one another the same way along a floor are gone in one go, not a stop
    // at each
    if (mTask && domain.mSteps[first.mTag].mKind == StepKind::eCross && mKnow->mNav.Edges()[domain.mSteps[first.mTag].mEdge].mKind == EdgeKind::eWalk)
    {
        const NavGraph& nav = mKnow->mNav;
        const NavEdge& firstEdge = nav.Edges()[domain.mSteps[first.mTag].mEdge];
        s32 to = firstEdge.mTo;
        for (std::size_t i = 1; i < best.mSteps.size(); i++)
        {
            const DomainStep& next = domain.mSteps[actions[best.mSteps[i]].mTag];
            if (next.mKind == StepKind::eLogic)
            {
                // (what the game does by itself)
                continue;
            }
            if (next.mKind != StepKind::eCross)
            {
                break;
            }
            const NavEdge& e = nav.Edges()[next.mEdge];
            // (in AO a walk goes on from one screen's floor to the next's over the gap between cameras, and the
            // first cell past the gap is no place to stop: a step back from it is a step back over the gap.
            // Rupture Farms' path 15, to and fro between 7783 and 8437 for a thousand frames.)
            if (e.mKind != EdgeKind::eWalk || e.mLeft != firstEdge.mLeft || (mIsAe && nav.Nodes()[e.mTo].mLine != nav.Nodes()[firstEdge.mTo].mLine))
            {
                break;
            }
            to = e.mTo;
        }
        if (to != firstEdge.mTo)
        {
            mTask = std::make_unique<GoToTask>(first.mName, [to](s32 node)
                                               { return node == to; });
        }
    }
    // And with a crowd after him (three Fleeches or more, or Paramites on the hunt) every crossing that follows is
    // gone in the same go, whatever kind: a stop to think at the end of each is where they have him (the Mudomo
    // trial: 120 frames lost that way over its first three screens)
    const Goap::Action* doing = &first;
    // (not a crossing that is timed against a Scrab, nor with one loose: those are waited at)
    bool scrabs = false;
    for (const ScrabAt& scrab : mWorld.mScrabs)
    {
        scrabs = scrabs || !scrab.mPossessed;
    }
    if (mTask && domain.mSteps[first.mTag].mKind == StepKind::eCross && domain.mSteps[first.mTag].mOther == 0 && !scrabs)
    {
        s32 fleeches = 0;
        bool hunted = false;
        for (const RockAt& f : mWorld.mFleechesAwake)
        {
            fleeches += f.mChasing && f.mHalf == mAbe.mHalfScale && std::abs(f.mX - mAbe.mX) < 600 && std::abs(f.mY - mAbe.mY) < 250 ? 1 : 0;
        }
        for (const ParamiteAt& p : mWorld.mParamites)
        {
            hunted = hunted || (!p.mPossessed && p.mBrain == 2 && p.mHalf == mAbe.mHalfScale);
        }
        if (fleeches >= 3 || hunted || RecentlyChased())
        {
            const NavGraph& nav = mKnow->mNav;
            s32 to = nav.Edges()[domain.mSteps[first.mTag].mEdge].mTo;
            s32 merged = 0;
            for (std::size_t i = 1; i < best.mSteps.size(); i++)
            {
                const DomainStep& next = domain.mSteps[actions[best.mSteps[i]].mTag];
                if (next.mKind == StepKind::eLogic)
                {
                    continue;
                }
                // (a thing to use or the way out at the end of them: that step itself, which goes there by the
                // same way and does not stop where the last crossing ends)
                if ((next.mKind == StepKind::eUse || next.mKind == StepKind::eExit) && doing == &first)
                {
                    doing = &actions[best.mSteps[i]];
                    break;
                }
                if (next.mKind != StepKind::eCross || next.mOther != 0)
                {
                    break;
                }
                to = nav.Edges()[next.mEdge].mTo;
                merged++;
            }
            if (doing != &first)
            {
                Log("a crowd after him: straight on to " + doing->mName);
                mTask = MakeTask(domain, *doing);
            }
            else if (merged > 0)
            {
                Log("a crowd after him: the next " + std::to_string(merged + 1) + " crossings in one go, to " + std::to_string(nav.Nodes()[to].mX) + "," + std::to_string(nav.Nodes()[to].mY));
                mTask = std::make_unique<GoToTask>(first.mName, [to](s32 node)
                                                   { return node == to; });
            }
        }
    }
    mTaskAction = doing->mName;
    mTaskOptional = domain.mSteps[doing->mTag].mKind == StepKind::eRescue || domain.mSteps[doing->mTag].mKind == StepKind::eKill;
    if (!mTask)
    {
        // A switch that follows from others: it takes the game a moment
        if (++mLogicWaits > 40)
        {
            mFailedActions[first.mName] = 2;
            mLogicWaits = 0;
        }
        // (where the step after it is, if that is the way out: a door a slapped lock opens is waited for at the
        // door, got to quietly. Where he slapped it is where what guards it finds him.)
        s32 waitAt = -1;
        for (std::size_t i = 1; i < best.mSteps.size() && waitAt < 0; i++)
        {
            const DomainStep& next = domain.mSteps[actions[best.mSteps[i]].mTag];
            if (next.mKind == StepKind::eLogic)
            {
                continue;
            }
            if (next.mKind == StepKind::eExit && next.mEdge >= 0 && next.mEdge < static_cast<s32>(mKnow->mNav.Exits().size()))
            {
                waitAt = mKnow->mNav.Exits()[next.mEdge].mNode;
            }
            break;
        }
        if (waitAt >= 0 && waitAt != abeNode && !mWorld.mFleechesAwake.empty())
        {
            std::unique_ptr<GoToTask> go = std::make_unique<GoToTask>("wait for " + first.mName + " at the way out", [waitAt](s32 node)
                                                                      { return node == waitAt; });
            go->SetQuiet(true);
            mTask = std::move(go);
            return true;
        }
        mTask = std::make_unique<GoToTask>("wait for " + first.mName, [](s32)
                                           { return true; });
        mPilot.Start(Prim::Wait(15), mAbe);
        mPilotActive = true;
        mPilotForTask = true;
        return true;
    }
    mLogicWaits = 0;
    return true;
}

bool Bot::ExitAllowed(BaseMap& map, const NavFeature& exit, bool* sideTrip)
{
    const s32 destLevel = exit.mDestLevel;
    const s32 destPath = exit.mDestPath;
    *sideTrip = false;
    if (destLevel != mAbe.mLevel)
    {
        return true;
    }
    // A door elsewhere wants a switch that is got at through one of the ways out of here (a hub and the paths
    // around it): that way, and no other while there is one
    const std::set<s32> errands = mLevel.ErrandExits(mAbe.mPath, [this](s32 path, s32 sw)
                                                     { return mWorld.Switch(sw) == 0 && mErrandsLeft.count({path, sw}) == 0; });
    if (!errands.empty())
    {
        return errands.count(destPath) != 0;
    }
    const s32 here = mLevel.Distance(mAbe.mPath);
    const s32 there = mLevel.Distance(destPath);
    if (there >= 0 && (here < 0 || there < here))
    {
        return true;
    }
    // Off the way (a secret area, a dead end): only for an ending that wants Mudokons, only once, and only if
    // looking at that path says there's something there to be had and a way back from it
    if (mProfile.mEnding != Ending::eAny && mLevel.Mudokons(destPath) > 0 && mVisitedPaths.count({destLevel, destPath}) == 0)
    {
        auto known = mSideTrips.find(destPath);
        if (known == mSideTrips.end())
        {
            SideTrip trip;
            NavInput input;
            World world;
            if (GameView::ReadPath(map, destPath, input, world))
            {
                NavGraph nav;
                nav.Build(input);
                // Where Abe comes in: the door or well this one is paired with, else anywhere on its camera
                s32 entry = -1;
                for (const NavFeature& f : nav.Input().mFeatures)
                {
                    const bool pair = (exit.mKind == FeatureKind::eDoor && f.mKind == FeatureKind::eDoor && f.mA == exit.mB) || (exit.mKind == FeatureKind::eWellExpress && (f.mKind == FeatureKind::eWellExpress || f.mKind == FeatureKind::eWellLocal) && f.mB == exit.mA);
                    if (pair && f.mCamera == exit.mDestCamera && entry < 0)
                    {
                        entry = nav.NodeAt((f.mX1 + f.mX2) / 2, f.mY2, f.mHalf, 60);
                    }
                }
                for (std::size_t i = 0; entry < 0 && i < nav.Nodes().size(); i++)
                {
                    for (const NavFeature& f : nav.Input().mFeatures)
                    {
                        if (f.mCamera == exit.mDestCamera && nav.CameraOf(f.mX1, f.mY1) == nav.CameraOf(nav.Nodes()[i].mX, nav.Nodes()[i].mY - 5))
                        {
                            entry = static_cast<s32>(i);
                            break;
                        }
                    }
                }
                const LevelGraph& level = mLevel;
                const s32 level_ = mAbe.mLevel;
                trip = Domain::WeighSideTrip(nav, world, entry, [&level, level_, there](const NavFeature& f)
                                             { return f.mDestLevel != level_ || (level.Distance(f.mDestPath) >= 0 && (there < 0 || level.Distance(f.mDestPath) < there)); });
            }
            known = mSideTrips.emplace(destPath, trip).first;
            const s32 worth = mProfile.mEnding == Ending::eKillAll ? trip.mMudokons : trip.mRescuable;
            Log("path " + std::to_string(destPath) + " is off the way: " + std::to_string(trip.mThere) + " Mudokons there, " + std::to_string(trip.mMudokons) + " of them in reach, " + std::to_string(trip.mRescuable) + " with a bird portal, " + (trip.mWayBack ? "a way back" : "no way back") + ": " + (worth > 0 && trip.mWayBack ? "worth the detour" : "not worth going"));
        }
        const SideTrip& trip = known->second;
        // A kill only needs Abe next to the Mudokon; a rescue needs a portal too
        const s32 worth = mProfile.mEnding == Ending::eKillAll ? trip.mMudokons : trip.mRescuable;
        if (worth > 0 && trip.mWayBack)
        {
            *sideTrip = true;
            return true;
        }
        return false;
    }
    if (mNewPathFor == mAbe.mPath && mVisitedPaths.count({destLevel, destPath}) == 0)
    {
        return true;
    }
    // A level with no way out known: anywhere new
    return here < 0 && there < 0 && mVisitedPaths.count({destLevel, destPath}) == 0;
}

std::string Bot::Report() const
{
    std::string out = "bot f=" + std::to_string(mAbe.mFrame) + " " + GameView::LevelName(mAbe.mLevel) + "/" + std::to_string(mAbe.mPath) + "/" + std::to_string(mAbe.mCamera) + " profile=" + mProfile.Text()
                    + " state=" + (mState == BotState::eDone ? "done" : mState == BotState::eStuck ? "stuck" : mState == BotState::eRunning ? "running" : "idle") + " deaths=" + std::to_string(mDeaths) + " restores=" + std::to_string(mRestores) + (mProfile.mLook ? " tries=" + std::to_string(mTries) : "")
                    + " rescued=" + std::to_string(mWorld.mRescued) + " killed=" + std::to_string(mWorld.mKilled) + "\n";
    if (!mWhy.empty())
    {
        out += "why " + mWhy + "\n";
    }
    out += "task " + (mTask ? mTask->Name() : std::string("none")) + " plan=[" + mPlanText + "]\n";
    out += "abe x=" + std::to_string(mAbe.mX) + " y=" + std::to_string(mAbe.mY) + " " + MoveName(mAbe.mMove) + " face=" + (mAbe.mFacingLeft ? "L" : "R") + " node=" + std::to_string(AbeNode()) + "\n";
    for (const Entity& e : mWorld.mEntities)
    {
        // What is on Abe's screen, and every Mudokon
        if (e.mCamera != mAbe.mCamera && e.mKind != EntityKind::eMudokon)
        {
            continue;
        }
        out += std::string(EntityKindName(e.mKind)) + "#" + std::to_string(e.mId) + " x=" + std::to_string(e.mLive ? e.mLiveX : e.CentreX()) + " y=" + std::to_string(e.mLive ? e.mLiveY : e.mY2);
        if (e.mKind == EntityKind::eMudokon)
        {
            out += e.mGone ? " gone" : e.mFollowing ? " following" : e.mLive ? " here" : " away";
        }
        if (e.mSwitch)
        {
            out += " sw" + std::to_string(e.mSwitch) + "=" + std::to_string(mWorld.Switch(e.mSwitch));
        }
        out += "\n";
    }
    for (const LiftAt& lift : mWorld.mLifts)
    {
        out += "Lift x=" + std::to_string(lift.mX1) + ".." + std::to_string(lift.mX2) + " y=" + std::to_string(lift.mY) + "\n";
    }
    for (const SligAt& slig : mWorld.mSligs)
    {
        out += "Slig x=" + std::to_string(slig.mX) + " y=" + std::to_string(slig.mY) + (slig.mFacingLeft ? " face=L" : " face=R") + (slig.mAsleep ? " asleep" : "") + (slig.mPossessed ? " possessed" : "") + "\n";
    }
    // The last of the log
    std::size_t from = mLog.size();
    for (s32 lines = 0; lines < 25 && from > 0; lines++)
    {
        from = mLog.rfind('\n', from - 2);
        if (from == std::string::npos)
        {
            from = 0;
            break;
        }
    }
    out += "log:\n" + mLog.substr(from == 0 ? 0 : from + 1);
    return out;
}

void Bot::GoTo(s32 x, s32 y, bool half)
{
    mGoToPending = true;
    mGoToX = x;
    mGoToY = y;
    mGoToHalf = half;
    mState = BotState::eRunning;
    mWhy.clear();
}

void Bot::Log(const std::string& text)
{
    char head[96];
    snprintf(head, sizeof(head), "f=%u %d,%d ", mAbe.mFrame, mAbe.mX, mAbe.mY);
    mLog += head;
    mLog += text;
    mLog += "\n";
    LOG_INFO("[bot] %s%s", head, text.c_str());
    mLastLogFrame = mAbe.mFrame;
}

std::string Bot::InputsText() const
{
    std::string out = "start " + std::to_string(mInputsStartFrame) + "\n";
    char line[16];
    for (u32 bits : mInputs)
    {
        snprintf(line, sizeof(line), "%x\n", bits);
        out += line;
    }
    return out;
}

s32 Bot::AbeNode() const
{
    if (!mKnow || !mAbe.mValid)
    {
        return -1;
    }
    return mKnow->mNav.NodeAt(mAbe.mX, mAbe.mY, mAbe.mHalfScale, 8);
}

void Bot::BanEdge(s32 edge)
{
    if (mKnow && edge >= 0)
    {
        const NavEdge& e = mKnow->mNav.Edges()[edge];
        Log("ban " + std::string(EdgeKindName(e.mKind)) + " edge " + std::to_string(mKnow->mNav.Nodes()[e.mFrom].mX) + "," + std::to_string(mKnow->mNav.Nodes()[e.mFrom].mY) + " -> " + std::to_string(mKnow->mNav.Nodes()[e.mTo].mX) + "," + std::to_string(mKnow->mNav.Nodes()[e.mTo].mY));
        mKnow->mBannedEdges.insert(edge);
    }
}

NavGraph::EdgeCost Bot::EdgeCostFn(bool pastScrabs, bool inTheOpen) const
{
    const PathKnowledge* know = mKnow;
    const World* world = &mWorld;
    const std::set<s32>* only = &mOnlyEdges;
    // (the power to go unseen in hand makes a Scrab's floor a way like any other, unless it is kept for a step of
    // the plan that is not at hand yet)
    const bool charged = !(mInvisSaved && !mInvisChantNow);
    const bool chanted = mChargeFrame != 0 && mAbe.mFrame - mChargeFrame < 60;
    // (unseen for a crossing over a floor Sligs watch: the quick ways are the cheap ones)
    const bool unseenRun = mWorld.mInvisible && InvisRun();
    const auto base = [know, world, only, pastScrabs, charged, chanted, unseenRun](const NavEdge& e) -> s32
    {
        const s32 idx = static_cast<s32>(&e - know->mNav.Edges().data());
        if (know->mBannedEdges.count(idx) || (!only->empty() && !only->count(idx)))
        {
            return -1;
        }
        // Running jumps with no stop between: only with a flying Slig about, which gets him where he stops
        if (e.mX2 != 0)
        {
            bool flier = false;
            const NavNode& at = know->mNav.Nodes()[e.mFrom];
            for (const FlierAt& f : world->mFliers)
            {
                flier = flier || (f.mHalf == at.mHalf && std::abs(f.mY - at.mY) < 220 && std::abs(f.mX - at.mX) < 1000);
            }
            if (!flier)
            {
                return -1;
            }
        }
        // Shut now
        s32 condSwitch = 0;
        s32 condValue = 0;
        if (know->mNav.Condition(e, &condSwitch, &condValue) && world->Switch(condSwitch) != condValue)
        {
            return -1;
        }
        // Onto a lift's platform, or riding it: only where the lift is now
        const NavNode& from = know->mNav.Nodes()[e.mFrom];
        const NavNode& to = know->mNav.Nodes()[e.mTo];
        // Onto a floor a loose Scrab can get at him on
        // (not on a stretch that was timed against where the Scrab is, nor when such a stretch is looked for)
        if (!pastScrabs && only->empty() && Domain::ScrabReaches(know->mNav, *world, to.mX, to.mY, to.mHalf, charged) && !Domain::ScrabReaches(know->mNav, *world, from.mX, from.mY, from.mHalf, charged))
        {
            return -1;
        }
        // Onto a floor Sligs watch with no shadow to go by (Domain::SligWatched): only unseen
        // (with the power in hand too, whatever step it is kept for: the chant comes before he is on it,
        // GoToTask. The Mudanchee trial's last room, where it is kept for the locks on the Sligs' level.)
        const bool unseenNow = world->mInvisible || world->mInvisCharge || world->mInvisComing || chanted;
        if (only->empty() && !unseenNow && Domain::SligWatched(know->mNav, *world, to.mX, to.mY, to.mHalf) && !Domain::SligWatched(know->mNav, *world, from.mX, from.mY, from.mHalf))
        {
            return -1;
        }
        // A running jump up a hoist: the way to go with a Scrab on his floor after him, and not otherwise
        if (e.mKind == EdgeKind::eRunJump && e.mFeature >= 0 && know->mNav.Input().mFeatures[e.mFeature].mKind == FeatureKind::eHoist)
        {
            // (unseen on a floor Sligs watch: the power runs out, and the jump is quicker than the stop and the
            // climb. Feeco's path 3, up the block at 1312,2020.)
            if (unseenRun && Domain::SligWatched(know->mNav, *world, from.mX, from.mY, from.mHalf))
            {
                return 20;
            }
            for (const ScrabAt& scrab : world->mScrabs)
            {
                if (!scrab.mPossessed && scrab.mHalf == from.mHalf && std::abs(scrab.mY - from.mY) <= 30 && std::abs(scrab.mX - from.mX) < 700)
                {
                    return 20;
                }
            }
            // (or with Fleeches awake and after him: the climb they would catch him on is the slow way. Unseen
            // nothing is after him.)
            for (const RockAt& f : world->mFleechesAwake)
            {
                if (!world->mInvisible && f.mHalf == from.mHalf && std::abs(f.mX - from.mX) <= 750 && std::abs(f.mY - from.mY) <= 260)
                {
                    return 20;
                }
            }
            // (or with Paramites on his floor: two together run him down, faster than he runs, and the stop to
            // climb is where they get him. Measured in the Mudomo hub at 2137,460.)
            for (const ParamiteAt& para : world->mParamites)
            {
                // (unseen as well: the power runs out, and the climb is what there is no time for. The Mudomo
                // vaults' path 10.)
                if (!para.mPossessed && !para.Hidden() && para.mHalf == from.mHalf && std::abs(para.mY - from.mY) <= 40 && std::abs(para.mX - from.mX) < 700)
                {
                    return 20;
                }
            }
        }
        // (a ride needs no such check: he is only ever on the platform where the lift is, and a way that rides
        // it twice has it somewhere else by the second time)
        const NavNode* stop = to.mLiftStop >= 0 && from.mLiftStop < 0 ? &to : nullptr;
        if (stop)
        {
            const NavFeature& at = know->mNav.Input().mFeatures[stop->mLiftStop];
            if (!world->LiftAtStop(at.mX1, at.mX2, stop->mY, at.mA != 0))
            {
                return -1;
            }
        }
        // A jump over the shaft of a lift: only while the lift is away
        if (e.mShaft >= 0)
        {
            const NavFeature& shaft = know->mNav.Input().mFeatures[e.mShaft];
            if (world->LiftAtStop(shaft.mX1, shaft.mX2, shaft.mY1, shaft.mA != 0))
            {
                return -1;
            }
        }
        // A hop or a running jump along a floor with a drill at work over a cell of it on the way: no (it can only
        // be timed a cell at a time, which is what walking and rolling are for)
        // (a jump up to a ledge has its run-up on the floor it starts from)
        // (nor a running jump whose flight goes through where a drill travels: in the gap it jumps, the drill
        // that goes up and down there is not under him but in his way. The Mines' path 4, from 2562,1400 over
        // the gap at 2487: drilled in the air.)
        if (e.mKind == EdgeKind::eRunJump && from.mHalf == to.mHalf)
        {
            const s32 cell = NavGraph::GridStep(from.mHalf);
            const s32 scale = from.mHalf ? 2 : 1;
            for (const NavFeature& f : know->mNav.Input().mFeatures)
            {
                if (f.mKind == FeatureKind::eSweep && f.mHalf == from.mHalf && f.mX2 > std::min(from.mX, to.mX) + cell / 2 && f.mX1 < std::max(from.mX, to.mX) - cell / 2 &&
                    f.mY2 >= std::min(from.mY, to.mY) - 60 / scale && f.mY1 <= std::max(from.mY, to.mY))
                {
                    return -1;
                }
            }
        }
        if ((e.mKind == EdgeKind::eRunJump || e.mKind == EdgeKind::eHop) && from.mHalf == to.mHalf)
        {
            const s32 cell = NavGraph::GridStep(from.mHalf);
            for (s32 x = std::min(from.mX, to.mX) + cell; x < std::max(from.mX, to.mX); x += cell)
            {
                // (a cell of floor he runs or lands on: a drill down in a gap he jumps is under him)
                if (know->mNav.NodeAt(x, from.mY, from.mHalf, 10) >= 0 && world->SweepOver(x, from.mY, from.mHalf))
                {
                    return -1;
                }
            }
        }
        // A walk off a ledge down into a shadow on a floor Sligs are awake on: the landing is heard and they turn
        // to it (Feeco's path 9, off 1037,340: both turned and shot). Let down from his hands at a hoist he lands
        // softly, so that is the way where there is one. (AE's only, as measured.)
        // (unseen it is no matter, and the drop is the quicker of the two)
        if (know->mNav.Input().mIsAe && !unseenRun && e.mKind == EdgeKind::eFall && to.mY > from.mY + 30)
        {
            bool hoist = false;
            for (s32 i = from.mFirstEdge; i < from.mFirstEdge + from.mEdgeCount; i++)
            {
                const NavEdge& other = know->mNav.Edges()[i];
                hoist = hoist || (other.mKind == EdgeKind::eHoistDown && know->mNav.Nodes()[other.mTo].mLine == to.mLine);
            }
            for (const SligAt& s : world->mSligs)
            {
                if (hoist && !s.mAsleep && !s.mPossessed && s.mHalf == to.mHalf && std::abs(s.mY - to.mY) <= 30 && std::abs(s.mX - to.mX) <= 700)
                {
                    return e.mCost + 300;
                }
            }
        }
        // A standing hop along a floor with a flying Slig about: the stop before it and after is what the Slig
        // gets him in, so a running jump is the way where there is one (it cruises at his run's pace and comes
        // after him at about half that. Feeco's path 7: walked and hopped along its corridor he was blown up at
        // 937,980.)
        if (e.mKind == EdgeKind::eHop && to.mY == from.mY)
        {
            for (const FlierAt& f : world->mFliers)
            {
                if (f.mHalf == from.mHalf && std::abs(f.mY - from.mY) < 220 && std::abs(f.mX - from.mX) < 1000)
                {
                    return e.mCost + 30;
                }
            }
        }
        // A hop off the end of a floor, unseen where Sligs watch: the stop to hop is time the power does not give,
        // so the run off the end is the way where there is one
        if (e.mKind == EdgeKind::eHop && to.mY > from.mY + 20 && unseenRun && Domain::SligWatched(know->mNav, *world, to.mX, to.mY, to.mHalf))
        {
            return e.mCost + 60;
        }
        // A hop down onto a floor a pack of Paramites hunts him on, within 150 px of one of them: he lands
        // standing, and they have him before he is off again. Measured in the Mudomo vaults' path 7, from
        // 1212,1400 onto 1312,1520 with one at 1212: dead 12 frames after landing. (At 150 he gets away: the
        // way in's hop from 1687,360.)
        if (e.mKind == EdgeKind::eHop && to.mY > from.mY + 30 && !world->mInvisible && Domain::ParamitePack(know->mNav, *world, to.mX, to.mY, to.mHalf))
        {
            for (const ParamiteAt& para : world->mParamites)
            {
                if (!para.mPossessed && !para.Hidden() && para.mHalf == to.mHalf && std::abs(para.mY - to.mY) <= 40 && std::abs(para.mX - to.mX) < 150)
                {
                    return e.mCost + 400;
                }
            }
        }
        // Past a drill takes waiting for it, and nerve
        if (to.mSweep >= 0 && GoToTask::SweepOver(know->mNav, *world, e.mTo))
        {
            return e.mCost + 150;
        }
        // Down onto a floor in front of a Slig that is awake and faces where he lands: it shoots before he is off
        // its floor again. Measured in the Mines' path 2 at 837,880, two cells in front of one: shot 37 frames
        // after he lands, at the hoist one cell on. (One asleep that the landing wakes is another matter: it takes
        // 96 frames in the Mines' path 10, where the way on is such a landing and a run to a well: Bot::MakeFlee.)
        // (unseen it is no matter)
        // (hung from the end of its floor he is not seen: GoToTask::Sentry)
        if (!unseenNow && !to.mHidden && !e.mHung && std::abs(to.mY - from.mY) * (to.mHalf ? 2 : 1) > 30 && (e.mKind == EdgeKind::eFall || e.mKind == EdgeKind::eHop || e.mKind == EdgeKind::eHoistDown || e.mKind == EdgeKind::eWell))
        {
            const s32 scale = to.mHalf ? 2 : 1;
            // (one not seen yet, its screen not loaded, is where and how the map starts it)
            std::vector<SligAt> sligs = world->mSligs;
            for (const Entity& ent : world->mEntities)
            {
                if (ent.mKind != EntityKind::eSlig || ent.mGone || ent.mLive)
                {
                    continue;
                }
                bool seen = false;
                for (const SligAt& live : world->mSligs)
                {
                    seen = seen || (live.mHalf == ent.mHalf && std::abs(live.mY - ent.mY2) <= 45 && std::abs(live.mX - ent.CentreX()) < 400);
                }
                if (!seen)
                {
                    SligAt at;
                    at.mX = ent.CentreX();
                    at.mY = ent.mY2;
                    at.mHalf = ent.mHalf;
                    at.mAsleep = ent.mState == 1;
                    at.mFacingLeft = ent.mOp == 1;
                    at.mSeen = false;
                    sligs.push_back(at);
                }
            }
            for (const SligAt& slig : sligs)
            {
                if (slig.mPossessed || slig.mHalf != to.mHalf || std::abs(slig.mY - to.mY) * scale > 30)
                {
                    continue;
                }
                const s32 off = std::abs(slig.mX - to.mX) * scale;
                if (!slig.mAsleep && off < 250 && (to.mX < slig.mX) == slig.mFacingLeft)
                {
                    return -1;
                }
            }
        }
        // Up a hoist with a Fleech awake on the screen: it is up after him in 25 frames and has him as he hangs.
        // A well or a door on the same screen is the way to be rid of it. (A running jump up a hoist is a climb
        // like the other: with only the one made dear, Necrum's path 1 turned back from the hoist to its well.)
        if (!world->mInvisible && (e.mKind == EdgeKind::eHoistUp || (e.mKind == EdgeKind::eRunJump && e.mFeature >= 0 && know->mNav.Input().mFeatures[e.mFeature].mKind == FeatureKind::eHoist)))
        {
            for (const RockAt& f : world->mFleechesAwake)
            {
                if (f.mHalf == from.mHalf && know->mNav.CameraOf(f.mX, f.mY - 5) == know->mNav.CameraOf(from.mX, from.mY - 5))
                {
                    return e.mCost + 2000;
                }
            }
        }
        // Coming down from a fall is heard (Abe::Motion_16_LandSoft: not after letting go of a ledge), and a Fleech
        // asleep close by wakes at it
        if (!world->mInvisible && (e.mKind == EdgeKind::eFall || (e.mKind == EdgeKind::eHop && to.mY > from.mY + 20)))
        {
            for (const RockAt& f : world->mFleechesAsleep)
            {
                if (f.mHalf == to.mHalf && std::abs(f.mY - to.mY) <= 90 && std::abs(f.mX - to.mX) < 150)
                {
                    return e.mCost + 3000;
                }
            }
        }
        // Close by a bird portal its birds fly off, and that is the end of it until the screen comes round again
        // (however he gets there: a well that throws him out beside it does the same)
        {
            for (const Entity& p : world->mEntities)
            {
                if (p.mKind == EntityKind::eBirdPortal && p.mRescuePortal && p.mHalf == to.mHalf && std::abs(p.mY2 - to.mY) < 30 && std::abs(p.CentreX() - to.mX) < (to.mHalf ? 37 : 75))
                {
                    return e.mCost + 3000;
                }
            }
        }
        return e.mCost;
    };
    // Sligs that shoot from the background: a cell they see him on costs more than one in cover, and one on
    // their beat (seen whichever way they face) more again, so that of two ways the one with less of the open in
    // it is gone
    bool zSligs = false;
    for (const SligAt& slig : mWorld.mSligs)
    {
        zSligs = zSligs || (slig.mZShoot && !slig.mPossessed);
    }
    if (!inTheOpen || !zSligs || mWorld.mInvisible)
    {
        return base;
    }
    return [know, world, base](const NavEdge& e) -> s32
    {
        s32 cost = base(e);
        const NavNode& to = know->mNav.Nodes()[e.mTo];
        if (cost < 0 || to.mHalf)
        {
            return cost;
        }
        const auto seen = [&](s32 x, s32 y)
        {
            s32 sum = 0;
            const s32 cam = know->mNav.CameraOf(x, y - 5);
            for (const SligAt& slig : world->mSligs)
            {
                if (slig.mZShoot && !slig.mPossessed && know->mNav.CameraOf(slig.mX, slig.mY - 5) == cam)
                {
                    sum += x >= slig.mBeatMin && x <= slig.mBeatMax ? 90 : 30;
                }
            }
            return sum;
        };
        // (a running jump or a hop is as much of the open as the cells it goes over: the rifle range's fourth
        // screen, where one long running jump past two covers looked cheaper than the rolls between them)
        if (e.mKind == EdgeKind::eRunJump || e.mKind == EdgeKind::eHop)
        {
            const NavNode& from = know->mNav.Nodes()[e.mFrom];
            const s32 cell = NavGraph::GridStep(from.mHalf);
            const s32 cells = std::abs(to.mX - from.mX) / cell;
            for (s32 k = 1; k < cells; k++)
            {
                cost += seen(from.mX + (to.mX < from.mX ? -k : k) * cell, from.mY);
            }
        }
        if (to.mZLow || to.mZCover)
        {
            return cost;
        }
        return cost + seen(to.mX, to.mY);
    };
}

s32 Bot::RetryDelay()
{
    if (mAttempt <= 0 || mProfile.mLook)
    {
        return 0;
    }
    // Longer each time, never the same twice
    return mAttempt * 17 + mRng.Range(0, 12);
}

s32 Bot::QuietFrames(s32 x, s32 y, bool half) const
{
    // The screen Abe is on: as far as the same camera goes either way
    const NavGraph& nav = mKnow->mNav;
    const s32 cam = nav.CameraOf(mAbe.mX, mAbe.mY - 5);
    s32 x1 = mAbe.mX;
    s32 x2 = mAbe.mX + 1;
    while (x1 > mAbe.mX - 2000 && nav.CameraOf(x1 - 25, mAbe.mY - 5) == cam)
    {
        x1 -= 25;
    }
    while (x2 < mAbe.mX + 2000 && nav.CameraOf(x2 + 25, mAbe.mY - 5) == cam)
    {
        x2 += 25;
    }
    return World::QuietFrames(mWorld.mSligs, mBeats, x, y, half, x1 - 25, x2 + 25);
}

bool Bot::BeatStaysOnScreen(const SligAt& slig) const
{
    const NavGraph& nav = mKnow->mNav;
    const s32 cam = nav.CameraOf(mAbe.mX, mAbe.mY - 5);
    s32 lo = slig.mBeatMin;
    s32 hi = slig.mBeatMax;
    if (hi <= lo)
    {
        for (const SligBeat& beat : mBeats)
        {
            if (beat.mHalf == slig.mHalf && std::abs(beat.mY - slig.mY) <= 45 && slig.mX >= beat.mMinX - 30 && slig.mX <= beat.mMaxX + 30 && beat.mMaxX - beat.mMinX >= 150)
            {
                lo = beat.mMinX;
                hi = beat.mMaxX;
            }
        }
    }
    return hi > lo && nav.CameraOf(lo, slig.mY - 5) == cam && nav.CameraOf(hi, slig.mY - 5) == cam && nav.CameraOf(slig.mX, slig.mY - 5) == cam;
}

Gait Bot::GaitFor(s32 cells)
{
    // Runs when it's worth it; a person isn't as exact about when that is
    // With a Slog up and about there is no time to stroll
    for (const SlogAt& slog : mWorld.mSlogs)
    {
        if (!slog.mAsleep && std::abs(slog.mX - mAbe.mX) < 2500 && std::abs(slog.mY - mAbe.mY) < 600)
        {
            return cells >= 2 ? Gait::eRun : Gait::eWalk;
        }
    }
    const s32 runFrom = mProfile.mHuman ? 5 + static_cast<s32>(mRng.Below(5)) : 6;
    return cells >= runFrom ? Gait::eRun : Gait::eWalk;
}

void Bot::EnsureKnowledge(BaseMap& map)
{
    if (mLevel.Level() != mAbe.mLevel)
    {
        const std::vector<PathSummary> paths = GameView::ReadLevel(map);
        for (const PathSummary& p : paths)
        {
            for (const auto& exit : p.mExits)
            {
                if (std::find(mProfile.mBeen.begin(), mProfile.mBeen.end(), GameView::LevelName(exit.first)) != mProfile.mBeen.end())
                {
                    mLevelsBeen.insert(exit.first);
                }
            }
        }
        mLevel.Build(mAbe.mLevel, paths, mLevelsBeen);
        mLevelsBeen.insert(mAbe.mLevel);
        Log("level graph:\n" + mLevel.Text());
    }
    if (mErrandPath != mAbe.mPath)
    {
        // What the path he has left could have set for a door elsewhere and has not: given up
        for (const s32 sw : mErrandPath >= 0 ? mLevel.Errands(mErrandPath) : std::vector<s32>())
        {
            if (mWorld.Switch(sw) == 0 && mErrandsLeft.insert({mErrandPath, sw}).second)
            {
                Log("left path " + std::to_string(mErrandPath) + " with switch " + std::to_string(sw) + " not set");
            }
        }
        mErrandPath = mAbe.mPath;
    }
    const std::pair<s32, s32> key{mAbe.mLevel, mAbe.mPath};
    auto it = mKnowledge.find(key);
    if (it == mKnowledge.end())
    {
        PathKnowledge know;
        know.mNav.Build(GameView::BuildNavInput(map));
        it = mKnowledge.emplace(key, std::move(know)).first;
        mVisitedPaths.insert(key);
        Log("nav graph for " + GameView::LevelName(mAbe.mLevel) + " " + std::to_string(mAbe.mPath) + ": " + std::to_string(it->second.mNav.Nodes().size()) + " cells, " + std::to_string(it->second.mNav.Edges().size()) + " edges");
    }
    if (mKnow && mKnow != &it->second && mPilotActive && mAuto)
    {
        // In another path: whatever was held to get here (the way on through an exit) is let go of
        mPilot.Start(Prim::Wait(1), mAbe);
    }
    mKnow = &it->second;
}

void Bot::Stuck(const std::string& why)
{
    // Looking ahead, nothing left to do is a try that failed, like one he was hurt in: what led here is taken
    // back and done another way, for as long as there is one
    if (mAuto && mProfile.mLook && !mCheckpoints.empty() && mState == BotState::eRunning)
    {
        Log("this try ends with nothing to do: " + why);
        mTryWhy = "nothing to do";
        mTryFailed = true;
        mPilotActive = false;
        mBits = 0;
        return;
    }
    StuckNow(why);
}

void Bot::StuckNow(const std::string& why)
{
    mState = BotState::eStuck;
    mWhy = why;
    mTask.reset();
    Log("STUCK: " + why);
}

bool Bot::Resting()
{
    if (!mIsAe || mAbe.mHealthFp >= 0x10000)
    {
        mRestFrames = 0;
        return false;
    }
    const s32 at = AbeNode();
    if (mAbe.mHealthFp <= 0 || !mAbe.Settled() || !mAbe.mOnGround || mWorld.mInvisible || mRestFrames > 400 || !mKnow || at < 0)
    {
        return false;
    }
    const NavGraph& nav = mKnow->mNav;
    const NavNode& here = nav.Nodes()[at];
    const s32 cam = nav.CameraOf(mAbe.mX, mAbe.mY - 5);
    for (const SligAt& slig : mWorld.mSligs)
    {
        if (slig.mPossessed || slig.mAsleep)
        {
            continue;
        }
        const bool onScreen = nav.CameraOf(slig.mX, slig.mY - 5) == cam;
        // (one in the background sees him anywhere on its screen out of cover; one on his floor, out of the
        // shadow, and it is on his screen before long from the next)
        if (slig.mZShoot ? onScreen && !here.mZCover : slig.mHalf == mAbe.mHalfScale && std::abs(slig.mY - mAbe.mY) <= 45 && !here.mHidden && (onScreen || std::abs(slig.mX - mAbe.mX) < 600))
        {
            return false;
        }
    }
    for (const SlogAt& slog : mWorld.mSlogs)
    {
        if (!slog.mAsleep && std::abs(slog.mX - mAbe.mX) < 600 && std::abs(slog.mY - mAbe.mY) < 260)
        {
            return false;
        }
    }
    for (const FlierAt& flier : mWorld.mFliers)
    {
        if (nav.CameraOf(flier.mX, flier.mY - 5) == cam)
        {
            return false;
        }
    }
    if (!mWorld.mScrabs.empty() || !mWorld.mParamites.empty() || !mWorld.mFleechesAwake.empty())
    {
        return false;
    }
    if (mRestFrames++ == 0)
    {
        Log("hurt: waits where nothing sees him for his health to come back");
    }
    return true;
}

void Bot::MaybeCheckpoint(BaseMap& map)
{
    // Standing or crouched only: a save taken hanging from a ledge or holding a lift's rope didn't come back right
    // (looking ahead it is taken hanging too, and with Sligs about: a save nothing gets on from is found out by
    // the tries from it, and the one before it gone back to)
    const bool look = mAuto && mProfile.mLook;
    const bool hung = look && mAbe.mMove == Move::eHang;
    if (!hung && ((mAbe.mMove != Move::eIdle && mAbe.mMove != Move::eCrouch) || !mAbe.mOnGround))
    {
        return;
    }
    if (mAbe.mTransition || mHold > 0 || !Checkpoint::CanSave(map))
    {
        return;
    }
    bool watched = false;
    for (const SligAt& slig : mWorld.mSligs)
    {
        watched = watched || (look && !slig.mPossessed && !slig.mAsleep && mKnow->mNav.CameraOf(slig.mX, slig.mY - 5) == mKnow->mNav.CameraOf(mAbe.mX, mAbe.mY - 5));
    }
    if (!mCheckpoints.empty() && mAbe.mFrame - mLastCheckpointFrame < (watched ? kCheckpointEveryFramesWatched : kCheckpointEveryFrames))
    {
        return;
    }
    // (but not hurt, nor just after: what hurt him may not be done)
    if (look && !mTolerant && (mAbe.mHealthFp < 0x10000 || (mLastHurtFrame != 0 && mAbe.mFrame - mLastHurtFrame < 60)))
    {
        return;
    }
    // Nor with a Slog awake and near: it would be saved with its teeth in him
    for (const SlogAt& slog : mWorld.mSlogs)
    {
        if (!slog.mAsleep && std::abs(slog.mX - mAbe.mX) < 400 && std::abs(slog.mY - mAbe.mY) < 200)
        {
            return;
        }
    }
    // Not with a Slig awake on the screen: the next moment may be Abe's last, and a checkpoint is a promise that
    // there's a way on from it
    // Nor with one asleep there (what wakes it may already be on its way), nor with a laughing Mudokon in tow
    for (const SligAt& slig : mWorld.mSligs)
    {
        if (!look && !slig.mPossessed && mKnow->mNav.CameraOf(slig.mX, slig.mY - 5) == mKnow->mNav.CameraOf(mAbe.mX, mAbe.mY - 5))
        {
            return;
        }
    }
    for (const Entity& e : mWorld.mEntities)
    {
        if (e.mKind == EntityKind::eMudokon && e.mLive && e.mFollowing && e.mMood == MudMood::eWired)
        {
            return;
        }
    }
    if (static_cast<s32>(mCheckpoints.size()) >= kMaxCheckpoints)
    {
        // Thin them out rather than forget the oldest: the one whose neighbours are closest together goes, so
        // there's always something from well before whatever went wrong
        std::size_t drop = 1;
        for (std::size_t i = 1; i + 1 < mCheckpoints.size(); i++)
        {
            if (mCheckpoints[i + 1].mCheckpoint.mFrame - mCheckpoints[i - 1].mCheckpoint.mFrame < mCheckpoints[drop + 1].mCheckpoint.mFrame - mCheckpoints[drop - 1].mCheckpoint.mFrame)
            {
                drop = i;
            }
        }
        mCheckpoints.erase(mCheckpoints.begin() + static_cast<std::ptrdiff_t>(drop));
    }
    Saved saved;
    saved.mCheckpoint.Save(map);
    saved.mInputCount = mInputs.size();
    saved.mRng = mRng.State();
    if (look)
    {
        saved.mMemory = *this;
        for (const auto& know : mKnowledge)
        {
            saved.mBanned[know.first] = know.second.mBannedEdges;
        }
    }
    mCheckpoints.push_back(std::move(saved));
    mLastCheckpointFrame = mAbe.mFrame;
    mAttempt = 0;
    if (look)
    {
        // Somewhere no save of this run was taken yet: the tries have got him on. (Every place is kept: with only
        // the last few, ground gone over again from a save further back looked new, and the tries never ran out.
        // Nor is a long while with no try failed getting on: a tactic that waits out a Slig's beat before each
        // go at it waited 1,700 frames each time, and went at the Barracks' second screen for 50,000 frames.)
        const std::tuple<s32, s32, s32, s32> place(mAbe.mLevel * 1000 + mAbe.mPath, mAbe.mX / 25, mAbe.mY / 20, static_cast<s32>(mAbe.mMove));
        if (mSavePlaces.insert(place).second)
        {
            mTriesSince = 0;
            mTryFrames = 0;
            mPopFloor = 0;
            if (mTryingAgain && ++mVarySaves >= 2)
            {
                Log("that try got on" + std::string(mVary == Lookahead::Way::eAgain ? " (planned anew)" : mVary == Lookahead::Way::eQuiet ? " (sneaking)" : mVary == Lookahead::Way::eFast ? " (running)" : ""));
                mTryingAgain = false;
                mVary = Lookahead::Way::eSame;
            }
            if (mTolerant && ++mTolerantSaves >= 2)
            {
                mTolerant = false;
                Log("got on: being hurt is a failed try again");
            }
        }
    }
}

s32 Bot::SweepLate() const
{
    static const s32 kLate[] = {0, 6, -6, 12, -12, 18, 24, -18};
    return kLate[mSweepDeaths % 8];
}

bool Bot::BeamRoll()
{
    const Prim& prim = mPilot.Current();
    const bool crouched = mAbe.mMove == Move::eCrouch;
    // (crouched for the roll, what the task does next is stand him up again: the roll goes where the move that
    // was held up was going)
    const bool standsUp = prim.mKind == Prim::Kind::ePress && (prim.mBits & Btn::eUp) && mBeamRolling && crouched;
    const bool moves = prim.mKind == Prim::Kind::eMoveTo && prim.mGait != Gait::eRoll;
    if (!mKnow || !(moves || standsUp) || (mAbe.mMove != Move::eIdle && !crouched) || !(mBeamHeld > 150 || (mBeamRolling && crouched)))
    {
        return false;
    }
    // A roll and a hop are heard (so is a walk: only sneaking isn't), and a Slig asleep on the screen wakes
    for (const SligAt& slig : mWorld.mSligs)
    {
        if (slig.mAsleep && slig.mHalf == mAbe.mHalfScale && mKnow->mNav.CameraOf(slig.mX, slig.mY - 5) == mKnow->mNav.CameraOf(mAbe.mX, mAbe.mY - 5))
        {
            mBeamRolling = false;
            return false;
        }
    }
    if (mBeamRolling && mBeamHeld > 500)
    {
        // No roll fits either: as it was
        mBeamRolling = false;
        return false;
    }
    const NavGraph& nav = mKnow->mNav;
    const s32 cell = NavGraph::GridStep(mAbe.mHalfScale);
    const s32 toX = moves ? prim.mX : mBeamToX;
    const s32 dir = toX > mAbe.mX ? 1 : -1;
    const s32 here = AbeNode();
    // As far as the floor is plain and the move wants to go, three cells at most
    s32 cells = 0;
    for (s32 k = 1; k <= 3 && here >= 0; k++)
    {
        const s32 n = nav.NodeAt(mAbe.mX + dir * k * cell, mAbe.mY, mAbe.mHalfScale, 8);
        if (n < 0 || nav.Nodes()[n].mHazard || nav.Nodes()[n].mLine != nav.Nodes()[here].mLine || (toX - nav.Nodes()[n].mX) * dir < 0)
        {
            break;
        }
        cells = k;
    }
    if (!crouched)
    {
        // Standing: a hop of three cells if that fits (over whatever lies between, onto plain floor), else down
        // into a crouch to roll from
        const s32 land = here >= 0 ? nav.NodeAt(mAbe.mX + dir * 3 * cell, mAbe.mY, mAbe.mHalfScale, 8) : -1;
        const bool onward = land >= 0 && !nav.Nodes()[land].mHazard && nav.Nodes()[land].mLine == nav.Nodes()[here].mLine && (toX - nav.Nodes()[land].mX) * dir >= 0;
        if (onward && mAbe.mFacingLeft == (dir < 0) && mWorld.BeamClear(mAbe.mX, mAbe.mY, mAbe.mHalfScale, dir, 3 * cell, 24))
        {
            Log("hopping on behind the motion detector's beam");
            mPilot.Start(Prim::Press(Btn::eHop), mAbe);
            return true;
        }
        if (cells >= 1 && mWorld.BeamClear(mAbe.mX, mAbe.mY, mAbe.mHalfScale, 0, 0, 9))
        {
            Log("crouching to roll on behind the motion detector's beam");
            mBeamRolling = true;
            mBeamToX = toX;
            mPilot.Start(Prim::Press(Btn::eDown), mAbe);
            return true;
        }
        return false;
    }
    for (; cells >= 1; cells--)
    {
        if (mWorld.BeamClear(mAbe.mX, mAbe.mY, mAbe.mHalfScale, dir, cells * cell, 4 * cells + (crouched ? 4 : 12)))
        {
            Log("rolling " + std::to_string(cells) + " cells behind the motion detector's beam");
            mBeamRolling = true;
            mBeamToX = toX;
            mPilot.Start(Prim::MoveTo(mAbe.mX + dir * cells * cell, Gait::eRoll), mAbe);
            return true;
        }
    }
    return false;
}

bool Bot::BeamHolds() const
{
    if (mWorld.mBeams.empty() || !mAbe.mValid)
    {
        return false;
    }
    // Only where holding still is a thing he can do: standing, crouched, or walking (he stops at the next cell)
    const bool still = mAbe.mMove == Move::eIdle || mAbe.mMove == Move::eCrouch;
    if (!still && mAbe.mMove != Move::eWalk && mAbe.mMove != Move::eSneak)
    {
        return false;
    }
    const Prim& prim = mPilot.Current();
    const s32 facing = mAbe.mFacingLeft ? -1 : 1;
    s32 dir = 0;
    s32 travel = 0;
    s32 frames = 12;
    switch (prim.mKind)
    {
        case Prim::Kind::eWait:
            if (prim.mBits == 0)
            {
                return false;
            }
            frames = std::max(12, prim.mFrames);
            break;
        case Prim::Kind::eMoveTo:
            dir = prim.mX > mAbe.mX ? 1 : -1;
            // (a cell at a time: 25 on and about 13 frames to be standing again)
            travel = prim.mGait == Gait::eRun ? 120 : 25;
            frames = prim.mGait == Gait::eRun ? 24 : 13;
            if (prim.mGait == Gait::eRoll)
            {
                // (begun by BeamRoll when it fitted: it is let run)
                return false;
            }
            break;
        case Prim::Kind::eFace:
            break;
        case Prim::Kind::ePress:
            if (mBeamRolling && mAbe.mMove == Move::eCrouch && (prim.mBits & Btn::eUp))
            {
                // Crouched to roll on: not stood up again while that may still be done
                return true;
            }
            if (mBeamRolling || mBeamHeld == 0)
            {
                // (begun by BeamRoll when it fitted, or already under way)
                if (!still)
                {
                    return false;
                }
            }
            if (prim.mBits & Btn::eHop)
            {
                dir = facing;
                travel = 75;
                frames = 24;
            }
            else
            {
                frames = 9;
            }
            break;
        case Prim::Kind::eWalkOff:
            dir = prim.mFaceLeft ? -1 : 1;
            travel = 60;
            frames = 32;
            break;
        case Prim::Kind::eRunJump:
            dir = prim.mFaceLeft ? -1 : 1;
            travel = 220;
            frames = 50;
            break;
        case Prim::Kind::eRunWell:
            dir = prim.mFaceLeft ? -1 : 1;
            travel = std::abs(prim.mX - mAbe.mX) + 25;
            frames = 30;
            break;
        case Prim::Kind::eLift:
            return false;
    }
    if (mAbe.mHalfScale)
    {
        travel /= 2;
    }
    return !mWorld.BeamClear(mAbe.mX, mAbe.mY, mAbe.mHalfScale, dir, travel, frames);
}

void Bot::ThrowBegun(u32 mine, const ThrowSpot& spot)
{
    mThrowPending = true;
    mThrowFrame = mAbe.mFrame;
    mThrowMine = mine;
    mThrowSpot = spot;
    mThrowSeen.clear();
}

void Bot::ThrowMissed()
{
    if (mThrowPending && mKnow)
    {
        // What the table said and what the rock did: the first frame they part is where the table is wrong
        const Entity* mine = mWorld.Find(mThrowMine);
        if (mine && mThrowSpot.mNode >= 0 && mThrowSpot.mNode < static_cast<s32>(mKnow->mNav.Nodes().size()))
        {
            const NavNode& from = mKnow->mNav.Nodes()[mThrowSpot.mNode];
            const s32 mineNode = mKnow->mNav.NodeAt(mine->CentreX(), mine->mY2, mine->mHalf, 12);
            const ThrowArc::Point target{mineNode >= 0 ? mKnow->mNav.Nodes()[mineNode].mX : mine->CentreX(), mineNode >= 0 ? mKnow->mNav.Nodes()[mineNode].mY : mine->mY2};
            const ThrowArc::Flight flight = ThrowArc::Throw(mKnow->mNav.Input(), from.mX, from.mY, from.mHalf, target.mX < from.mX, mThrowSpot.mKind, &target);
            std::string text = "missed throw " + std::to_string(static_cast<s32>(mThrowSpot.mKind)) + " from " + std::to_string(from.mX) + "," + std::to_string(from.mY) + " at " + std::to_string(target.mX) + "," + std::to_string(target.mY) + ": to hit on frame " + std::to_string(flight.mHit) + (flight.mSure ? "" : " (not sure)") + "; table";
            for (std::size_t i = 0; i < flight.mPoints.size() && i < 45; i++)
            {
                text += " " + std::to_string(flight.mPoints[i].mX) + "," + std::to_string(flight.mPoints[i].mY);
            }
            text += "; game";
            for (const ThrowArc::Point& p : mThrowSeen)
            {
                text += " " + std::to_string(p.mX) + "," + std::to_string(p.mY);
            }
            Log(text);
        }
        mKnow->mBadThrows.insert(std::make_tuple(mThrowMine, mThrowSpot.mNode, static_cast<s32>(mThrowSpot.mKind)));
    }
    mThrowPending = false;
}

void Bot::ThrowHit()
{
    mThrowPending = false;
}

bool Bot::ThrowIsBad(u32 mine, const ThrowSpot& spot) const
{
    return mKnow && mKnow->mBadThrows.count(std::make_tuple(mine, spot.mNode, static_cast<s32>(spot.mKind))) != 0;
}

void Bot::TryFailed(BaseMap& map, const std::string& what)
{
    mTryFailed = false;
    mTries++;
    mTriesSince++;
    mPilotActive = false;
    mBits = 0;
    mMissRewind = false;
    mHold = 0;
    if (mCheckpoints.empty())
    {
        StuckNow(what + " with nothing to go back to");
        return;
    }
    const s32 frames = static_cast<s32>(mAbe.mFrame - mCheckpoints.back().mCheckpoint.mFrame);
    mCurrentEdge = -1;
    // The first failure since he last got on: from here it goes back two saves at most (the saves a try takes on
    // its way are gone back over freely: they are no further back than where it began)
    if (mPopFloor == 0)
    {
        mPopFloor = mCheckpoints.size() > static_cast<std::size_t>(kPopsPerObstacle) ? mCheckpoints.size() - kPopsPerObstacle : 1;
    }
    const std::string doing = mTask ? mTask->Name() : "no task";
    // Too many without getting anywhere new: no more going back for a way through unhurt
    mTryFrames += frames;
    const bool spent = mTriesSince > kTriesPerObstacle || mTryFrames > (mTolerant ? kFramesPerObstacleDying : kFramesPerObstacle);
    if (spent)
    {
        mTriesSince = 0;
        mTryFrames = 0;
        if (mTolerant)
        {
            StuckNow("no try gets on, hurt or not (" + what + " doing " + doing + ")");
            return;
        }
    }
    // What was pressed in a try the bot played itself is what the later tries from that save press
    const auto pressed = [this](std::size_t from, std::size_t to)
    {
        std::vector<u32> out;
        for (std::size_t i = from; i < to && i < mInputs.size(); i++)
        {
            if ((mInputs[i] & kModalInput) == 0)
            {
                out.push_back(mInputs[i]);
            }
        }
        return out;
    };
    if (!mCheckpoints.back().mHasPressed)
    {
        mCheckpoints.back().mPressed = pressed(mCheckpoints.back().mInputCount, mInputs.size());
        mCheckpoints.back().mHasPressed = true;
    }
    s32 delay = -1;
    std::string givenUp;
    for (;;)
    {
        Saved& saved = mCheckpoints.back();
        delay = Lookahead::Failed(saved.mStretch, static_cast<s32>(mAbe.mFrame - saved.mCheckpoint.mFrame), (mAbe.mX / 25) * 4096 + mAbe.mY / 20);
        if (delay < 0 && !spent)
        {
            // No moment gets that through: something else at each moment
            delay = Lookahead::NextWay(saved.mStretch);
        }
        if (delay >= 0 && !spent)
        {
            break;
        }
        // Begun at every moment and none got through.
        // The same optional thing every time: the run goes on without it
        if (mTask && mTaskOptional && !mTaskAction.empty() && mFailedActions[mTaskAction] < 2 && saved.mStretch.mRounds < 2)
        {
            givenUp = mTaskAction;
            Log("giving up on " + mTaskAction + ": no try at it got through");
            saved.mHasPressed = false;
            delay = Lookahead::Again(saved.mStretch);
            break;
        }
        // Or it went wrong before this save: the try that got here from the one before has failed too. A save or
        // two back, no further: if nothing gets through unhurt from there either, hurt is the best there is
        const bool back = mCheckpoints.size() > 1 && mCheckpoints.size() > mPopFloor && (mTolerant || !spent);
        if (back)
        {
            // (what the bot pressed from the save before to get here, and then from here)
            Saved& before = mCheckpoints[mCheckpoints.size() - 2];
            if (!before.mHasPressed)
            {
                before.mPressed = pressed(before.mInputCount, saved.mInputCount);
                before.mPressed.insert(before.mPressed.end(), saved.mPressed.begin(), saved.mPressed.end());
                before.mHasPressed = true;
            }
            mCheckpoints.pop_back();
            continue;
        }
        if (!mTolerant)
        {
            mTolerant = true;
            mTolerantSaves = 0;
            mTriesSince = 0;
            mTryFrames = 0;
            // (with hurt put up with it goes back further, but not to the start of the run: what took minutes to get
            // through is not played over for a Slig at the end of it)
            mPopFloor = mCheckpoints.size() > static_cast<std::size_t>(kPopsPerObstacleDying) ? mCheckpoints.size() - kPopsPerObstacleDying : 1;
            saved.mStretch = Lookahead::Stretch();
            saved.mHasPressed = false;
            delay = 0;
            Log("nothing gets through unhurt: being hurt is put up with for now");
            break;
        }
        StuckNow("no try gets on from the oldest save (" + what + " doing " + doing + ")");
        return;
    }
    Saved& saved = mCheckpoints.back();
    mHold = delay;
    // (a try the bot plays itself, straight on, has nothing to repeat)
    mRepeat = saved.mHasPressed && saved.mStretch.mDelay > 0 && Lookahead::Repeats(saved.mStretch.mWay) ? saved.mPressed : std::vector<u32>();
    mVary = saved.mStretch.mWay;
    mVarySaves = 0;
    mTryingAgain = true;
    // (back in an earlier path, perhaps: that is no leaving of the one he was in)
    mErrandPath = -1;
    Log("try failed (" + what + ", " + doing + ") " + std::to_string(frames) + " frames in: back to f=" + std::to_string(saved.mCheckpoint.mFrame) + ", holding back " + std::to_string(delay) + (mVary == Lookahead::Way::eAgain ? ", planned anew" : mVary == Lookahead::Way::eQuiet ? ", sneaking" : mVary == Lookahead::Way::eFast ? ", running" : ""));
    mRepeatAt = 0;
    mRng.SetState(saved.mRng);
    static_cast<BotMemory&>(*this) = saved.mMemory;
    for (auto& know : mKnowledge)
    {
        const auto banned = saved.mBanned.find(know.first);
        know.second.mBannedEdges = banned == saved.mBanned.end() ? std::set<s32>() : banned->second;
    }
    mKeptPlan.clear();
    if (!givenUp.empty())
    {
        mFailedActions[givenUp] = 2;
    }
    mLastHealthFp = 0;
    mInputs.resize(saved.mInputCount);
    saved.mCheckpoint.Load(map);
    mRestoring = true;
    mRestores++;
    mLastCheckpointFrame = saved.mCheckpoint.mFrame;
}

void Bot::OnDeath(BaseMap& map)
{
    const bool look = mAuto && mProfile.mLook;
    if (!look)
    {
        mDeaths++;
    }
    if (mThrowPending && mAbe.mFrame - mThrowFrame > 300)
    {
        mThrowPending = false;
    }
    if (mThrowPending || (mThrowFrame != 0 && mAbe.mFrame >= mThrowFrame && mAbe.mFrame - mThrowFrame < 150))
    {
        // The blast reached him, or what the rock woke did, or the rock went on to another mine by him
        Log("a throw at the mine from " + std::to_string(mThrowSpot.mCells) + " cells killed Abe: not from there again");
        mThrowPending = true;
        ThrowMissed();
    }
    if (mSweepPending && mAbe.mFrame - mSweepFrame < 250)
    {
        mSweepDeaths++;
        Log("the drill got Abe: next time as if he were " + std::to_string(SweepLate()) + " frames later than worked out");
    }
    mSweepPending = false;
    if (look)
    {
        TryFailed(map, "died");
        return;
    }
    const s32 deathsHere = ++mDeathCells[{mAbe.mX / 25, mAbe.mY / 20}];
    if (deathsHere == 6 || deathsHere == 12)
    {
        // The same place again and again: what he was doing there doesn't work
        if (mCurrentEdge >= 0 && mKnow)
        {
            BanEdge(mCurrentEdge);
        }
        if (mTask && !mTaskAction.empty())
        {
            mFailedActions[mTaskAction] = 2;
            Log("giving up on " + mTaskAction + ": Abe has died " + std::to_string(deathsHere) + " times at " + std::to_string(mAbe.mX) + "," + std::to_string(mAbe.mY));
        }
    }
    if (deathsHere >= 20)
    {
        Stuck("Abe keeps dying at " + std::to_string(mAbe.mX) + "," + std::to_string(mAbe.mY) + (mTask ? " doing " + mTask->Name() : ""));
        return;
    }
    mPilotActive = false;
    mBits = 0;
    if (mCurrentEdge >= 0 && mKnow)
    {
        const EdgeKind kind = mKnow->mNav.Edges()[mCurrentEdge].mKind;
        if ((kind == EdgeKind::eFall || kind == EdgeKind::eHop || kind == EdgeKind::eRunJump) && ++mEdgeDeaths[mCurrentEdge] >= 2)
        {
            BanEdge(mCurrentEdge);
        }
        mCurrentEdge = -1;
    }
    // Three deaths at a rescue or a kill: not worth a fourth, the run goes on without it
    if (mTask && mTaskOptional && !mTaskAction.empty() && ++mActionDeaths[mTaskAction] >= 3 && mFailedActions[mTaskAction] < 2)
    {
        mFailedActions[mTaskAction] = 2;
        Log("giving up on " + mTaskAction + ": it killed Abe " + std::to_string(mActionDeaths[mTaskAction]) + " times");
    }
    mMissRewind = false;
    if (mCheckpoints.empty())
    {
        Stuck("died with nothing to go back to");
        return;
    }
    // Dead on the very frame the last try from here died on: it was settled before this checkpoint
    if (mCheckpoints.back().mLastDeathFrame == mAbe.mFrame)
    {
        mCheckpoints.back().mAttempts = std::max(mCheckpoints.back().mAttempts, kAttemptsPerCheckpoint);
    }
    mCheckpoints.back().mLastDeathFrame = mAbe.mFrame;
    // Enough tries from here: what went wrong happened earlier
    while (mCheckpoints.size() > 1 && mCheckpoints.back().mAttempts >= kAttemptsPerCheckpoint)
    {
        mCheckpoints.pop_back();
    }
    Saved& saved = mCheckpoints.back();
    if (saved.mAttempts >= kAttemptsPerCheckpoint * 2)
    {
        Stuck("died " + std::to_string(saved.mAttempts) + " times from the oldest checkpoint" + (mTask ? " doing " + mTask->Name() : ""));
        return;
    }
    saved.mAttempts++;
    mAttempt = saved.mAttempts;
    // (back in an earlier path, perhaps: that is no leaving of the one he was in)
    mErrandPath = -1;
    Log("died (" + std::string(mTask ? mTask->Name() : "no task") + "), back to f=" + std::to_string(saved.mCheckpoint.mFrame) + " try " + std::to_string(mAttempt));
    mInputs.resize(saved.mInputCount);
    saved.mCheckpoint.Load(map);
    mRestoring = true;
    mRestores++;
    mLastCheckpointFrame = saved.mCheckpoint.mFrame;
}

void Bot::RewindMiss(BaseMap& map)
{
    mMissRewind = false;
    // To before the rock left his hand: what was saved since has it gone already
    while (mCheckpoints.size() > 1 && mCheckpoints.back().mCheckpoint.mFrame > mThrowFrame)
    {
        mCheckpoints.pop_back();
    }
    if (mCheckpoints.empty() || mCheckpoints.back().mCheckpoint.mFrame > mThrowFrame || mMissRewinds >= 400)
    {
        return;
    }
    mMissRewinds++;
    Saved& saved = mCheckpoints.back();
    Log("taking the missed throw back: to f=" + std::to_string(saved.mCheckpoint.mFrame));
    mPilotActive = false;
    mBits = 0;
    mCurrentEdge = -1;
    mInputs.resize(saved.mInputCount);
    saved.mCheckpoint.Load(map);
    mRestoring = true;
    mRestores++;
    mLastCheckpointFrame = saved.mCheckpoint.mFrame;
}

// How far along a flying Slig's track (its track lines, end to end in the order they are in the path) the place
// nearest to (x, y) is, and how long the track is. False with no track.
static bool TrackAt(const NavInput& in, s32 x, s32 y, s32& along, s32& total, s32* off = nullptr)
{
    static const s32 kTrackLine = 8;
    total = 0;
    s64 best = -1;
    for (const NavLine& line : in.mLines)
    {
        if (line.mType != kTrackLine)
        {
            continue;
        }
        const s32 lx = line.mX2 - line.mX1;
        const s32 ly = line.mY2 - line.mY1;
        const s32 len = std::max(1, std::abs(lx) + std::abs(ly));
        const s64 sq = static_cast<s64>(lx) * lx + static_cast<s64>(ly) * ly;
        s64 t1000 = sq == 0 ? 0 : (static_cast<s64>(x - line.mX1) * lx + static_cast<s64>(y - line.mY1) * ly) * 1000 / sq;
        t1000 = std::max<s64>(0, std::min<s64>(1000, t1000));
        const s64 px = line.mX1 + lx * t1000 / 1000;
        const s64 py = line.mY1 + ly * t1000 / 1000;
        const s64 d = (px - x) * (px - x) + (py - y) * (py - y);
        if (best < 0 || d < best)
        {
            best = d;
            along = total + static_cast<s32>(len * t1000 / 1000);
        }
        total += len;
    }
    if (off && best >= 0)
    {
        // (how far from the track the place is, as the larger of across and up)
        s32 far = 0;
        while (static_cast<s64>(far) * far < best)
        {
            far += 25;
        }
        *off = far;
    }
    return best >= 0;
}

bool Bot::ShadowNear(s32 node, s32 maxEdges, std::vector<s32>* way, s32* cost) const
{
    const NavGraph& nav = mKnow->mNav;
    const NavGraph* graph = &nav;
    const auto shadow = [graph](s32 n)
    {
        return graph->Nodes()[n].mHidden;
    };
    std::vector<s32> path;
    s32 total = 0;
    bool near = node >= 0 && nav.FindPath(node, shadow, EdgeCostFn(), path, &total) && static_cast<s32>(path.size()) <= maxEdges;
    for (s32 edge : path)
    {
        const EdgeKind kind = nav.Edges()[edge].mKind;
        near = near && (kind == EdgeKind::eWalk || kind == EdgeKind::eHoistDown || kind == EdgeKind::eHoistUp || kind == EdgeKind::eFall);
    }
    if (near && way)
    {
        *way = path;
    }
    if (near && cost)
    {
        *cost = total;
    }
    return near;
}

bool Bot::FlierThreat() const
{
    const NavGraph& nav = mKnow->mNav;
    const s32 cam = nav.CameraOf(mAbe.mX, mAbe.mY - 5);
    const s32 abeNode = AbeNode();
    for (const FlierAt& flier : mWorld.mFliers)
    {
        if (flier.mHalf != mAbe.mHalfScale)
        {
            continue;
        }
        // On his screen it sees him as soon as it faces him
        if (nav.CameraOf(flier.mX, flier.mY) == cam)
        {
            mFlierWhy = "it is on his screen";
            return true;
        }
        s32 total = 0;
        s32 at = 0;
        s32 abeAt = 0;
        if (abeNode < 0 || !TrackAt(nav.Input(), flier.mX, flier.mY, at, total) || !TrackAt(nav.Input(), mAbe.mX, mAbe.mY - 40, abeAt, total))
        {
            continue;
        }
        // The frames until it can be at a place on its track: straight there if it is coming, by way of the
        // end it is making for if not.
        const s32 dir = mFlierDir;
        const auto arrives = [at, total, dir](s32 place)
        {
            // (measured: 624 pixels of track in 100 frames at its fastest, 30 to 60 frames at an end)
            const s32 gap = std::abs(place - at);
            const bool coming = dir == 0 || (place - at) * dir > 0;
            return coming ? gap / 6 : ((dir > 0 ? total - at : at) * 2 + gap) / 6 + 30;
        };
        const auto along = [&nav, &total](s32 node)
        {
            s32 place = 0;
            TrackAt(nav.Input(), nav.Nodes()[node].mX, nav.Nodes()[node].mY - 40, place, total);
            return place;
        };
        // What he is out in the open for next: from here (or the zone he is at) to the next invisible zone on
        // his way, or to the thing to do and from it to a zone, or, with none after it, on to the thing after
        // that. It mustn't be anywhere on that stretch before he is through with it.
        s32 frames = 250;
        s32 endAt = abeAt;
        if (!mPlanNodes.empty() && mPlanNodes[0] >= 0)
        {
            std::vector<s32> home;
            const s32 homeLine = ShadowNear(abeNode, 16, &home, nullptr) ? (home.empty() ? nav.Nodes()[abeNode].mLine : nav.Nodes()[nav.Edges()[home.back()].mTo].mLine) : -1;
            std::vector<s32> route;
            s32 routeCost = 0;
            if (nav.FindPath(abeNode, mPlanNodes[0], EdgeCostFn(), route, &routeCost))
            {
                bool found = false;
                s32 gone = 0;
                for (std::size_t i = 0; i < route.size() && !found; i++)
                {
                    const NavEdge& e = nav.Edges()[route[i]];
                    // Out of reach of its track from here on (the run to the way out): that far is all
                    s32 place = 0;
                    s32 off = 0;
                    TrackAt(nav.Input(), nav.Nodes()[e.mTo].mX, nav.Nodes()[e.mTo].mY - 40, place, total, &off);
                    if (off > 300)
                    {
                        frames = gone;
                        found = true;
                        break;
                    }
                    endAt = place;
                    gone += e.mCost;
                    std::vector<s32> way;
                    s32 wayCost = 0;
                    if (ShadowNear(e.mTo, 16, &way, &wayCost))
                    {
                        const s32 line = way.empty() ? nav.Nodes()[e.mTo].mLine : nav.Nodes()[nav.Edges()[way.back()].mTo].mLine;
                        if (line != homeLine)
                        {
                            frames = gone + wayCost;
                            endAt = along(e.mTo);
                            found = true;
                        }
                    }
                }
                if (!found)
                {
                    // (60: the thing itself)
                    frames = routeCost + 60;
                    endAt = along(mPlanNodes[0]);
                    // Then either back to a zone, or on to the next thing for as long as that is within
                    // reach of its track: whichever it leaves time for
                    s32 backCost = 0;
                    const bool back = ShadowNear(mPlanNodes[0], 40, nullptr, &backCost) && arrives(endAt) >= frames + backCost + 60;
                    bool onward = false;
                    std::vector<s32> on;
                    // (as the map is, whatever is shut now: the thing to do may be what opens the way on)
                    const NavGraph::EdgeCost plain = [](const NavEdge& e) -> s32
                    {
                        return e.mCost;
                    };
                    if (!back && mPlanNodes.size() > 1 && mPlanNodes[1] >= 0 && nav.FindPath(mPlanNodes[0], mPlanNodes[1], plain, on))
                    {
                        s32 onCost = 0;
                        s32 onAt = endAt;
                        for (s32 edge : on)
                        {
                            const NavNode& n = nav.Nodes()[nav.Edges()[edge].mTo];
                            s32 place = 0;
                            s32 off = 0;
                            TrackAt(nav.Input(), n.mX, n.mY - 40, place, total, &off);
                            if (off > 300)
                            {
                                break;
                            }
                            onCost += nav.Edges()[edge].mCost;
                            onAt = place;
                        }
                        onward = arrives(endAt) >= frames + 60 && arrives(onAt) >= frames + onCost + 60 && (at - abeAt) * (at - onAt) >= 0;
                    }
                    if (!back && !onward)
                    {
                        mFlierWhy = "at " + std::to_string(at) + " going " + std::to_string(dir) + ", he at " + std::to_string(abeAt) + ": the next thing at " + std::to_string(endAt) + " takes " + std::to_string(frames) + " frames and " + std::to_string(backCost) + " back, it is there in " + std::to_string(arrives(endAt)) + (on.empty() ? ", no way on" : ", way on of " + std::to_string(on.size()));
                        return true;
                    }
                }
            }
        }
        // (where he sets out from he has left at once: only where he ends up counts, which it gets to later
        // than where he is if it comes from behind him)
        // On the stretch already (within a few cells of it), it meets him on it
        if (arrives(endAt) < frames + 60 || (at >= std::min(abeAt, endAt) - 75 && at <= std::max(abeAt, endAt) + 75))
        {
            mFlierWhy = "at " + std::to_string(at) + " going " + std::to_string(dir) + ", he at " + std::to_string(abeAt) + ": the stretch to " + std::to_string(endAt) + " takes " + std::to_string(frames) + " frames, it is there in " + std::to_string(arrives(endAt));
            return true;
        }
    }
    return false;
}

bool Bot::SlogRefuge(const SlogAt& slog, s32 from, s32& refuge, bool& left, bool& inTime, s32& fromX) const
{
    const NavGraph& nav = mKnow->mNav;
    const NavNode& at = nav.Nodes()[from];
    const s32 side = slog.mX >= at.mX ? 1 : -1;
    const s32 floorY = at.mY;
    const NavGraph::EdgeCost all = EdgeCostFn(true);
    const NavGraph* graph = &nav;
    const NavGraph::EdgeCost awayOnly = [&all, graph, side, floorY](const NavEdge& e) -> s32
    {
        const NavNode& a = graph->Nodes()[e.mFrom];
        const NavNode& b = graph->Nodes()[e.mTo];
        // (not towards it along its floor, and only by what is quick)
        if (std::abs(b.mY - floorY) <= 45 && (b.mX - a.mX) * side > 0)
        {
            return -1;
        }
        return e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eRoll || e.mKind == EdgeKind::eWell || e.mKind == EdgeKind::eHoistUp || e.mKind == EdgeKind::eTeleport || e.mKind == EdgeKind::eFall ? all(e) : -1;
    };
    std::vector<s32> way;
    if (!nav.FindPath(from, [graph, floorY](s32 n)
                      { return std::abs(graph->Nodes()[n].mY - floorY) > 45; }, awayOnly, way) || way.empty())
    {
        return false;
    }
    const NavEdge& last = nav.Edges()[way.back()];
    refuge = last.mTo;
    fromX = nav.Nodes()[last.mFrom].mX;
    // (a way with a roll in it is the pilot's from the start: where the run ends is at the first of those)
    for (s32 edge : way)
    {
        if (nav.Edges()[edge].mKind == EdgeKind::eRoll)
        {
            fromX = at.mX;
            break;
        }
    }
    const s32 dist = std::abs(nav.Nodes()[last.mFrom].mX - at.mX);
    left = dist > 0 ? nav.Nodes()[last.mFrom].mX < at.mX : side > 0;
    // He runs 6.2 pixels a frame, and is 14 frames getting off the ground at a hoist, 12 on a teleporter's plate.
    // The Slog runs some 7, leaps at him from 60 off, and is 24 frames turning when he is behind it (measured in
    // the Mudanchee trial: 27 to 30 after he came down behind it, then from 118 to 346 in 36 frames).
    const s32 gap = std::abs(slog.mX - at.mX);
    const s32 abeFrames = dist * 10 / 62 + (last.mKind == EdgeKind::eHoistUp ? 14 : last.mKind == EdgeKind::eTeleport ? 12 : 4);
    const bool facesAway = slog.mFacesLeft != (side > 0);
    const s32 slogFrames = std::max(0, gap + dist - 60) * 10 / 70 + (facesAway ? 24 : 0);
    inTime = abeFrames + 4 < slogFrames;
    return true;
}

std::unique_ptr<Task> Bot::MakeSlogFlee()
{
    if (mSlogFlees >= 12)
    {
        return nullptr;
    }
    const NavGraph& nav = mKnow->mNav;
    for (const SlogAt& slog : mWorld.mSlogs)
    {
        s32 node = -1;
        if (mAbe.mOnGround)
        {
            // One that is up, or about to be, on his floor
            if ((slog.mAsleep && !slog.mWaking) || std::abs(slog.mY - mAbe.mY) > 30 || std::abs(slog.mX - mAbe.mX) >= 500)
            {
                continue;
            }
            node = AbeNode();
        }
        else
        {
            // Coming down next to one, asleep or not: what brings him there may well be what wakes it
            // (judged where he will come down, a few frames before he does)
            if (slog.mY <= mAbe.mY || slog.mY - mAbe.mY > 60 || mAbe.mVelYFp <= 0)
            {
                continue;
            }
            const s32 landX = mAbe.mX + static_cast<s32>(static_cast<s64>(mAbe.mVelXFp) * (slog.mY - mAbe.mY) / mAbe.mVelYFp);
            if (std::abs(slog.mX - landX) >= 60)
            {
                continue;
            }
            node = nav.NodeAt(landX, slog.mY, mAbe.mHalfScale, 30);
        }
        if (node < 0 || Domain::WallBetween(nav, slog.mX, nav.Nodes()[node].mX, nav.Nodes()[node].mY, mAbe.mHalfScale) ||
            Domain::GateBetween(nav, mWorld, slog.mX, nav.Nodes()[node].mX, nav.Nodes()[node].mY, mAbe.mHalfScale))
        {
            continue;
        }
        // (on a floor of its own, a pit between: it does not come over)
        const s32 slogNode = nav.NodeAt(slog.mX, slog.mY, mAbe.mHalfScale, 30);
        if (slogNode >= 0 && nav.Nodes()[slogNode].mLine != nav.Nodes()[node].mLine)
        {
            continue;
        }
        s32 refuge = -1;
        bool left = false;
        bool inTime = false;
        s32 fromX = 0;
        if (!SlogRefuge(slog, node, refuge, left, inTime, fromX) || (mAbe.mOnGround && inTime && (mAbe.mMove == Move::eRun || mAbe.mMove == Move::eRoll) && mAbe.mFacingLeft == left))
        {
            // (at a run away from it with time in hand, or a roll, which is as quick: what he is at is as good)
            continue;
        }
        mSlogFlees++;
        Log(std::string(mAbe.mOnGround ? "a Slog after him" : "coming down next to a Slog") + " at " + std::to_string(slog.mX) + "," + std::to_string(slog.mY) + ": off its floor to " + std::to_string(nav.Nodes()[refuge].mX) + "," + std::to_string(nav.Nodes()[refuge].mY));
        return std::make_unique<SlogFleeTask>(refuge, left, fromX);
    }
    return nullptr;
}

std::unique_ptr<Task> Bot::MakeFlee()
{
    if (std::unique_ptr<Task> slog = MakeSlogFlee())
    {
        return slog;
    }
    // A Slig awake right next to him on his floor, out in the open (he came down beside one that slept, and the
    // landing woke it): it takes a while to come round and shoot, enough to get off its floor if a way is at
    // hand (a well, a hoist). Chanting there is too slow: measured, he is shot 50 frames before it takes.
    // (One that waits before it shoots is run from too: the sneak he was at when it woke is not gone on with.)
    // (hung from the end of its floor he is not seen, and is not on it)
    // (not in a try that sneaks every step: that try is to get by the Slig unheard, and a run from it is heard)
    if (mAmbushes < 8 && !mWorld.mInvisible && mAbe.mMove != Move::eHang && !(mProfile.mLook && mVary == Lookahead::Way::eQuiet))
    {
        const NavGraph& nav = mKnow->mNav;
        const s32 cam = nav.CameraOf(mAbe.mX, mAbe.mY - 5);
        const s32 node = AbeNode();
        const bool hidden = node >= 0 && nav.Nodes()[node].mHidden;
        for (const SligAt& slig : mWorld.mSligs)
        {
            const s32 near = mAbe.mHalfScale ? 100 : 200;
            if (node < 0 || hidden || slig.mAsleep || slig.mPossessed || slig.mHalf != mAbe.mHalfScale || std::abs(slig.mY - mAbe.mY) > 45 || std::abs(slig.mX - mAbe.mX) >= near || nav.CameraOf(slig.mX, slig.mY - 5) != cam)
            {
                continue;
            }
            // (not one that stands with its back to him: he has come up behind it on purpose, and a run from
            // there is what turns it round. The Barracks' path 7: at the boom machine, 150 behind the Slig that
            // stands at 462 for 120 frames each way.)
            if (mIsAe && slig.mMotion == 0 && slig.mFacingLeft == (mAbe.mX > slig.mX))
            {
                continue;
            }
            const NavGraph* graph = &nav;
            const s32 floorY = mAbe.mY;
            const auto away = [graph, floorY](s32 n)
            {
                return std::abs(graph->Nodes()[n].mY - floorY) > 45;
            };
            std::vector<s32> way;
            // (as far as he runs in the time it takes to shoot: measured on the Mines' path 10, shot 96 frames
            // after it woke with the well ten cells on, which is 40 frames at a run)
            // (by the quick ways only: the nearest other floor may be down a hoist, which is no escape)
            const NavGraph::EdgeCost all = EdgeCostFn();
            // (looking ahead a door is one too: he is gone through it sooner than he is up a hoist, and the
            // tries find the moment. The Barracks' path 1: the door at 2037,380, a cell on from where he ran
            // back from the Slig at 2187 every time.)
            const bool doors = mProfile.mLook;
            // (a step off the end of the floor is the quickest of all: 6 frames and he is falling. Not one that
            // ends hung from a ledge or in a well. The Barracks' path 7: the Slig the lever at 400,860 calls
            // comes down a cell from him, and the walkthrough's "immediately run right to fall to the lower
            // level".)
            const auto drop = [](const NavEdge& e)
            {
                return e.mKind == EdgeKind::eFall && !e.mHung && e.mWell < 0;
            };
            const NavGraph::EdgeCost quickOnly = [&all, doors, &drop](const NavEdge& e) -> s32
            {
                return e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eWell || e.mKind == EdgeKind::eHoistUp || drop(e) || (doors && e.mKind == EdgeKind::eDoor) ? all(e) : -1;
            };
            // (cells are half as long at half scale)
            const std::size_t most = mAbe.mHalfScale ? 24 : 12;
            const bool found = nav.FindPath(node, away, quickOnly, way);
            if (!found || way.size() > most)
            {
                if (mFleeWhy++ == 0)
                {
                    Log("a Slig is awake next to Abe at " + std::to_string(slig.mX) + "," + std::to_string(slig.mY) + " and there is no quick way off its floor (" + (found ? std::to_string(way.size()) + " steps" : std::string("none")) + ")");
                }
            }
            if (found && way.size() <= most)
            {
                bool quick = true;
                for (s32 edge : way)
                {
                    const EdgeKind kind = nav.Edges()[edge].mKind;
                    quick = quick && (kind == EdgeKind::eWalk || kind == EdgeKind::eWell || kind == EdgeKind::eHoistUp || drop(nav.Edges()[edge]) || (doors && kind == EdgeKind::eDoor));
                }
                if (quick)
                {
                    mAmbushes++;
                    Log("a Slig is awake next to Abe at " + std::to_string(slig.mX) + "," + std::to_string(slig.mY) + ": off its floor before it shoots");
                    // (to where that quick way ends: left to find any way off the floor, it took the hoist down
                    // behind him in the Barracks' first path, with the well three cells on past the guard)
                    const s32 end = nav.Edges()[way.back()].mTo;
                    auto go = std::make_unique<GoToTask>("off the floor of the Slig that woke", [end](s32 n)
                                                         { return n == end; });
                    // (no creeping: it is awake already, and has him in a second or two)
                    go->SetHurry(true);
                    return go;
                }
            }
        }
    }
    return nullptr;
}

std::unique_ptr<Task> Bot::MakeReflex()
{
    if (mAuto && !mAbe.mOnGround && !mAbe.mTransition)
    {
        return MakeSlogFlee();
    }
    if (!mAuto || !mAbe.mOnGround || mAbe.mTransition)
    {
        return nullptr;
    }
    if (!mAbe.Settled())
    {
        // In mid step only what can't wait: a Slig that has just woken beside him
        const bool stepping = mAbe.mMove == Move::eWalk || mAbe.mMove == Move::eSneak || mAbe.mMove == Move::eRun;
        return stepping ? MakeFlee() : nullptr;
    }
    // Slogs after him, and walls on this floor to fry them with
    if (SlogTrapTask::AnyAwake(mWorld) && mSlogTraps < 6)
    {
        SlogTrapTask::Setup setup;
        if (SlogTrapTask::Find(mKnow->mNav, mWorld, mAbe.mY, mAbe.mHalfScale, setup) && mWorld.Switch(setup.mSwitch) == 0)
        {
            mSlogTraps++;
            Log("Slogs about and electric walls on switch " + std::to_string(setup.mSwitch) + " at " + std::to_string(setup.mX1) + ".." + std::to_string(setup.mX2) + ": luring them into those");
            return std::make_unique<SlogTrapTask>(setup);
        }
    }
    // Slogs gathering behind a gate on his floor, and a Slig that can be brought to it: that shoots them
    if (mSlogGuards < 3)
    {
        SlogGuardTask::Setup setup;
        if (SlogGuardTask::Find(*this, setup))
        {
            mSlogGuards++;
            Log("Slogs behind a gate on this floor, and a Slig at " + std::to_string(setup.mSligX) + "," + std::to_string(setup.mSligY) + " that a teleporter at " + std::to_string(setup.mPlateX) + " brings here: taking it over to shoot them");
            return std::make_unique<SlogGuardTask>(setup);
        }
    }
    // A flying Slig that is coming, or is where he has to go: into an invisible zone, if one is near on this
    // floor, and there until it has gone by and is behind him. (It only sees what is on its screen, in front of
    // it and not in such a zone: FlyingSlig::CanChase.)
    if (!mWorld.mFliers.empty() && FlierThreat() && mFlierHides < 40)
    {
        const NavGraph& nav = mKnow->mNav;
        const s32 node = AbeNode();
        if (node >= 0 && nav.Nodes()[node].mHidden)
        {
            return std::make_unique<HideTask>();
        }
        const NavGraph* graph = &nav;
        const auto shadow = [graph](s32 n)
        {
            return graph->Nodes()[n].mHidden;
        };
        std::vector<s32> way;
        if (ShadowNear(node, 40, &way, nullptr))
        {
            mFlierHides++;
            Log("a flying Slig is about at " + std::to_string(mWorld.mFliers[0].mX) + "," + std::to_string(mWorld.mFliers[0].mY) + ": into the invisible zone " + std::to_string(way.size()) + " cells off until it has gone by");
            return std::make_unique<GoToTask>("into an invisible zone", shadow);
        }
    }
    if (std::unique_ptr<Task> flee = MakeFlee())
    {
        return flee;
    }
    // Sligs awake on his screen, all of them on another level than his (they can't see him there), and no
    // security orb: one is taken over from where he stands, shoots the rest and bursts. Left alone they shoot
    // out of the screen at him as soon as he is in front of them at full size.
    // (not a Slig he has called for a lock: that one is the task's to take over and keep)
    if (mPerchKills < 6 && !dynamic_cast<const SligLockTask*>(mTask.get()))
    {
        const NavGraph& nav = mKnow->mNav;
        const s32 cam = nav.CameraOf(mAbe.mX, mAbe.mY - 5);
        // (not with the power to go unseen in hand or on its way: the chant would spend that and take no Slig. Nor
        // unseen: that time is for what the plan has him do unseen.)
        // (AE only: in AO's Rupture Farms the chant took no Slig and he stood in it for 800 frames)
        bool can = mIsAe && mAbe.mMove == Move::eIdle && !mWorld.mInvisCharge && !mWorld.mInvisComing && !mWorld.mInvisible;
        for (const Entity& e : mWorld.mEntities)
        {
            can = can && !(e.mKind == EntityKind::eSecurityOrb && nav.CameraOf(e.mX1, e.mY1) == cam);
        }
        s32 below = 0;
        for (const SligAt& slig : mWorld.mSligs)
        {
            if (slig.mPossessed || nav.CameraOf(slig.mX, slig.mY - 5) != cam)
            {
                continue;
            }
            if (slig.mHalf != mAbe.mHalfScale)
            {
                continue;
            }
            // (a chant wakes one that sleeps, and one on his own level sees him)
            can = can && !slig.mAsleep && std::abs(slig.mY - mAbe.mY) > (mAbe.mHalfScale ? 35 : 45);
            below++;
        }
        if (can && below > 0)
        {
            mPerchKills++;
            Log(std::to_string(below) + " Slig(s) awake on this screen and none on Abe's level: taking one over from here");
            return ClearSligTask::Ambush();
        }
    }
    // Wired Mudokons follow on their own and laugh, which wakes Sligs: a slap sorts them out, where the gas that
    // did it isn't on the screen (it would wire them again) and no sleeping Slig is there to hear it
    for (const Entity& e : mWorld.mEntities)
    {
        if (e.mKind == EntityKind::eLaughingGas && e.mCamera == mAbe.mCamera && mWorld.Switch(e.mSwitch))
        {
            return nullptr;
        }
    }
    for (const SligAt& slig : mWorld.Threats())
    {
        if (slig.mAsleep && mKnow->mNav.CameraOf(slig.mX, slig.mY - 5) == mKnow->mNav.CameraOf(mAbe.mX, mAbe.mY - 5))
        {
            return nullptr;
        }
    }
    for (const Entity& e : mWorld.mEntities)
    {
        if (e.mKind == EntityKind::eMudokon && e.mLive && e.mFollowing && e.mMood == MudMood::eWired && e.mHalf == mAbe.mHalfScale && std::abs(e.mLiveY - mAbe.mY) < 20 && mReflexTries[e.mId] < 3)
        {
            mReflexTries[e.mId]++;
            Log("a wired Mudokon is tagging along: Mudokon#" + std::to_string(e.mId));
            return std::make_unique<RecruitTask>(e.mId, true);
        }
    }
    return nullptr;
}

void Bot::Think(BaseMap& map)
{
    if (mGoToPending)
    {
        mGoToPending = false;
        Run(GoToTask::To(*this, mGoToX, mGoToY, mGoToHalf));
    }

    // Nothing said for a long while: what it is doing, for whoever reads the log
    if (mAbe.mFrame < mLastLogFrame)
    {
        mLastLogFrame = mAbe.mFrame;
    }
    if (mState == BotState::eRunning && mAbe.mFrame - mLastLogFrame > 600)
    {
        Log(std::string("quiet for a while: task ") + (mTask ? mTask->Name() : "none") + (mReflex ? ", reflex " + mReflex->Name() : "") + ", last did " + mPilot.Current().Text() + (mPilotActive ? "" : " (over)") + (mLastPrimFailed.empty() ? "" : ", which failed: " + mLastPrimFailed) + (mAbe.mTransition ? ", in a transition" : "") + (mRestoring ? ", restoring" : "") + ", abe " + std::to_string(static_cast<s32>(mAbe.mMove)) + " node " + std::to_string(AbeNode()));
    }
    if (mTryFailed)
    {
        const std::string what = mTryWhy.empty() ? "hurt" : mTryWhy;
        mTryWhy.clear();
        TryFailed(map, what);
        if (mRestoring || mState != BotState::eRunning)
        {
            return;
        }
    }
    if (mMissRewind)
    {
        RewindMiss(map);
        if (mRestoring)
        {
            return;
        }
    }

    for (s32 guard = 0; guard < 8 && mState == BotState::eRunning && !mPilotActive; guard++)
    {
        if (mAbe.mMove == Move::eDead)
        {
            // Let what killed him play out first: it isn't in the save, and some of it (an electrocution) would
            // go on to kill the Abe that is put back
            if (++mDeadFrames < 30)
            {
                return;
            }
            mDeadFrames = 0;
            OnDeath(map);
            return;
        }
        mDeadFrames = 0;
        // Nothing before there is something to go back to: the first checkpoint, once he stands
        if (mAuto && mCheckpoints.empty() && mStartWaits < 150)
        {
            MaybeCheckpoint(map);
            if (mCheckpoints.empty())
            {
                mStartWaits++;
                return;
            }
        }
        // Looking ahead: this try begins a little later than the last
        if (mHold > 0)
        {
            mHold--;
            return;
        }
        // Then the same buttons as the first try, to where that failed; from there it is played on as it comes
        if (mRepeatAt < mRepeat.size())
        {
            mBits = mRepeat[mRepeatAt++];
            return;
        }
        if (!mRepeat.empty())
        {
            mRepeat.clear();
            mBits = 0;
        }
        if (!mTask)
        {
            if (!mAuto)
            {
                mState = BotState::eDone;
                return;
            }
            if (Resting())
            {
                return;
            }
            if (!Replan(map) || mPilotActive)
            {
                return;
            }
        }
        MaybeCheckpoint(map);
        if (!mReflex)
        {
            mReflex = MakeReflex();
        }
        if (mReflex)
        {
            const TaskStep step = mReflex->Next(*this, mLastPrimFailed);
            mLastPrimFailed.clear();
            if (step.mKind == TaskStep::Kind::ePrim)
            {
                mPilot.Start(step.mPrim, mAbe);
                mPilotActive = true;
                mPilotForTask = true;
                break;
            }
            Log(std::string(step.mKind == TaskStep::Kind::eDone ? "done: " : "gave up: ") + mReflex->Name() + (step.mWhy.empty() ? "" : ": " + step.mWhy));
            mReflex.reset();
            mReflexesRun++;
            // What was being done is planned anew from where he is now
            mTask.reset();
            mTaskAction.clear();
            mKeptPlan.clear();
            continue;
        }
        // A person doesn't press the next button the frame the last move ends
        if (mAuto && mProfile.mHuman && !mDawdled && mLastPrimFailed.empty() && mAbe.mMove == Move::eIdle && mRng.Chance(15))
        {
            mDawdled = true;
            mPilot.Start(Prim::Wait(mRng.Range(3, 16)), mAbe);
            mPilotActive = true;
            mPilotForTask = true;
            break;
        }
        mDawdled = false;
        const TaskStep step = mTask->Next(*this, mLastPrimFailed);
        mLastPrimFailed.clear();
        switch (step.mKind)
        {
            case TaskStep::Kind::ePrim:
                mPilot.Start(step.mPrim, mAbe);
                mPilotActive = true;
                mPilotForTask = true;
                break;
            case TaskStep::Kind::eDone:
                Log("done: " + mTask->Name());
                mTask.reset();
                break;
            case TaskStep::Kind::eFailed:
                if (mAuto)
                {
                    // Another way, or the same again if it was a fluke: twice and the action is given up
                    Log("action_failed: " + mTask->Name() + ": " + step.mWhy);
                    mFailedActions[mTaskAction]++;
                    mFailedTotal[mTaskAction]++;
                    // (what will be no different from anywhere else or at any other time: for good, at once)
                    if (step.mWhy.find("never leaves the screen") != std::string::npos || step.mWhy.find("the exit didn't lead out of the path") != std::string::npos)
                    {
                        mFailedActions[mTaskAction] = 2;
                        mFailedTotal[mTaskAction] = 6;
                    }
                    mFailedWhere[mTaskAction] = {mAbe.mX, mAbe.mY};
                    mTask.reset();
                }
                else
                {
                    Stuck(mTask->Name() + ": " + step.mWhy);
                }
                break;
        }
    }
}

void Bot::Update(BaseMap& map, bool betweenFrames)
{
    if (mRestoring)
    {
        if (mCheckpoints.back().mCheckpoint.Loading(map, betweenFrames))
        {
            return;
        }
        mRestoring = false;
        // The task starts over from the world as it was
        mPilotActive = false;
        mReflex.reset();
        if (mAuto)
        {
            mTask.reset();
        }
        mLastPrimFailed.clear();
    }

    if (!betweenFrames)
    {
        // A movie: skipped, as a player who has seen it does (a tap, so the button is seen to go down)
        if (mAuto && mState == BotState::eRunning && map.GetActiveModal())
        {
            mSkipPulse = !mSkipPulse;
            Input().mAutomationInput = mSkipPulse ? static_cast<u32>(InputCommands::eUnPause_OrConfirm) : 0;
            if (mInputsStarted)
            {
                mInputs.push_back(Input().mAutomationInput | kModalInput);
            }
        }
        return;
    }

    mAbe = GameView::ReadAbe(map);
    if (mAbe.mValid)
    {
        mIsAe = GetGameType() == GameType::eAe;
        EnsureKnowledge(map);
        GameView::ReadWorld(map, mWorld);
        if (mKnow)
        {
            for (const NavFeature& f : mKnow->mNav.Input().mFeatures)
            {
                // (not where a door of the path waits for the same switch: that has to be seen as it is. The
                // Barracks' path 5: switch 125 is a gate's by the laughing gas, and the wheel's that opens the
                // way out, which he turned and took for not turned.)
                bool doors = false;
                for (const NavFeature& g : mKnow->mNav.Input().mFeatures)
                {
                    doors = doors || (g.mKind == FeatureKind::eDoor && g.mSwitch == f.mSwitch && f.mSwitch > 1);
                }
                if (!doors && f.mKind == FeatureKind::eBarrier && f.mSolid && f.mA == 1 && f.mSwitch > 1 && mWorld.mGatesGone.count(f.mSwitch) == 0 && (mWorld.Switch(f.mSwitch, true) != 0) == f.mStartClosed)
                {
                    mWorld.mGatesGone[f.mSwitch] = mWorld.Switch(f.mSwitch, true);
                }
            }
        }
        // The Sligs that shoot from the background of his screen: watched, to know when they look away
        {
            std::vector<SligCycle::Seen> seen;
            s32 key = -1;
            for (const SligAt& slig : mWorld.mSligs)
            {
                if (slig.mZShoot && !slig.mPossessed && mKnow && mKnow->mNav.CameraOf(slig.mX, slig.mY - 5) == mKnow->mNav.CameraOf(mAbe.mX, mAbe.mY - 5))
                {
                    SligCycle::Seen s;
                    s.mX = slig.mX;
                    s.mFacingLeft = slig.mFacingLeft;
                    // (Slig.hpp: Motion_5_TurnAroundStanding)
                    s.mBlind = slig.mMotion == 5;
                    s.mMotion = slig.mMotion;
                    seen.push_back(s);
                    key = mAbe.mPath * 65536 + mKnow->mNav.CameraOf(slig.mX, slig.mY - 5);
                }
            }
            mSligCycle.Observe(mAbe.mFrame, key, seen);
        }
        // The flying mines: watched, to know where each will be
        if (mKnow && !mBombCycle.HasTracks(mAbe.mLevel * 1000 + mAbe.mPath))
        {
            mBombCycle.SetTracks(mAbe.mLevel * 1000 + mAbe.mPath, BombCycle::Tracks(mKnow->mNav.Input()));
        }
        mBombCycle.Observe(mAbe.mFrame, mAbe.mLevel * 1000 + mAbe.mPath, mWorld.mBombs);
        if (mAbe.mOnGround)
        {
            const s32 on = AbeNode();
            if (on >= 0)
            {
                mCellsBeen.insert({mAbe.mLevel * 1000 + mAbe.mPath, on});
            }
        }
        // The switch that is being tried for the sake of it has changed, if only for a moment (a trap door that
        // shuts itself again): it has been tried
        if (mExploring >= 1000 && mExploring < 1256 && mWorld.mSwitches[mExploring - 1000] != mLastSwitches[mExploring - 1000])
        {
            mExplored.insert({mAbe.mPath, mExploring});
        }
        // On the floor of a Scrab that waits before it strikes, with nothing between: it has seen him
        if (mSeenByScrabAt > mAbe.mFrame)
        {
            // (the run was taken back to before then)
            mSeenByScrabAt = mAbe.mFrame;
        }
        for (const ScrabAt& scrab : mWorld.mScrabs)
        {
            // (how long each has stood: at an end of its beat it stands for a time its TLV gives)
            // (Scrab.hpp: 0 stands. A shriek or a howl on the way is no stop.)
            std::pair<s32, u32>& still = mScrabStill[scrab.mId];
            if (still.first != scrab.mX || still.second > mAbe.mFrame || scrab.mMotion != 0)
            {
                // (seen to move: from now on its standing is timed)
                if (still.first != scrab.mX)
                {
                    mScrabMoved.insert(scrab.mId);
                }
                still = {scrab.mX, 0};
            }
            else if (still.second == 0 && mScrabMoved.count(scrab.mId))
            {
                still.second = mAbe.mFrame;
            }
            else if (still.second != 0 && mAbe.mFrame - still.second == 8)
            {
                // (a stop on its beat, if it is not after him: where it gave up a chase is no end of it. Its beat
                // lies between the last two places it stopped at: one it has left for another, lured there, is
                // learnt again from its next two stops. Eight frames in: it stands for a frame or two after a
                // shriek on its way as well.)
                if (mSeenByScrabAt == 0 || mAbe.mFrame - mSeenByScrabAt > 330)
                {
                    const auto beat = mScrabBeat.find(scrab.mId);
                    if (beat == mScrabBeat.end())
                    {
                        mScrabBeat[scrab.mId] = {scrab.mX, scrab.mX};
                    }
                    else if (std::abs(scrab.mX - beat->second.second) > 60)
                    {
                        beat->second = {beat->second.second, scrab.mX};
                    }
                    else
                    {
                        beat->second.second = scrab.mX;
                    }
                }
            }
        }
        if (mKnow && mAbe.mValid && mAbe.mMove != Move::eHang)
        {
            std::vector<Domain::LooseScrab> scrabs = Domain::LooseScrabs(mWorld, true);
            const std::vector<Domain::LooseScrab> atOnce = Domain::LooseScrabs(mWorld, false);
            scrabs.insert(scrabs.end(), atOnce.begin(), atOnce.end());
            for (const Domain::LooseScrab& scrab : scrabs)
            {
                if (scrab.mHalf == mAbe.mHalfScale && std::abs(scrab.mY - mAbe.mY) * (scrab.mHalf ? 2 : 1) < 45 && !Domain::GateBetween(mKnow->mNav, mWorld, scrab.mX, mAbe.mX, scrab.mY, scrab.mHalf) && !Domain::WallBetween(mKnow->mNav, scrab.mX, mAbe.mX, scrab.mY, scrab.mHalf))
                {
                    mSeenByScrabAt = mAbe.mFrame;
                    // (what it does now is the chase, not its beat: that is learnt again after)
                    mScrabBeat.erase(scrab.mId);
                }
            }
        }
        mWorld.mInvisComing = mAbe.mFrame < mRingDueBy && !mWorld.mInvisCharge && !mWorld.mInvisible;
        // Where a thrown rock really went, to hold against where it was to go if it misses
        if (mThrowPending && !mWorld.mRocksFlying.empty() && mThrowSeen.size() < 60)
        {
            mThrowSeen.push_back({mWorld.mRocksFlying[0].mX, mWorld.mRocksFlying[0].mY});
        }
    }

    // The way a flying Slig goes along its track (kept while it waits at an end of it)
    if (mWorld.mFliers.empty())
    {
        mFlierDir = 0;
        mFlierAt = -1;
    }
    else
    {
        s32 at = 0;
        s32 total = 0;
        if (TrackAt(mKnow->mNav.Input(), mWorld.mFliers[0].mX, mWorld.mFliers[0].mY, at, total))
        {
            if (mFlierAt >= 0 && at != mFlierAt)
            {
                mFlierDir = at > mFlierAt ? 1 : -1;
            }
            mFlierAt = at;
        }
    }

    // Where the Sligs walk, as it shows
    for (const SligAt& slig : mWorld.mSligs)
    {
        if (slig.mAsleep || slig.mPossessed)
        {
            continue;
        }
        bool known = false;
        for (SligBeat& beat : mBeats)
        {
            if (beat.mHalf == slig.mHalf && std::abs(beat.mY - slig.mY) <= 45 && slig.mX >= beat.mMinX - 60 && slig.mX <= beat.mMaxX + 60)
            {
                beat.mMinX = std::min(beat.mMinX, slig.mX);
                beat.mMaxX = std::max(beat.mMaxX, slig.mX);
                known = true;
                break;
            }
        }
        if (!known)
        {
            mBeats.push_back({slig.mY, slig.mHalf, slig.mX, slig.mX});
        }
    }

    // Looking ahead, hurt is as good as dead: the try is over
    if (mAuto && mProfile.mLook && mState == BotState::eRunning && mAbe.mValid)
    {
        if (mLastHealthFp > 0 && mAbe.mHealthFp > 0 && mAbe.mHealthFp < mLastHealthFp)
        {
            mLastHurtFrame = mAbe.mFrame;
            if (!mTolerant && !mCheckpoints.empty())
            {
                mTryFailed = true;
                mPilotActive = false;
                mBits = 0;
            }
        }
        mLastHealthFp = mAbe.mHealthFp;
    }

    if (mPilotPending)
    {
        // A try that goes about it another way: every step of it sneaked, or run
        if (mPendingPrim.mKind == Prim::Kind::eMoveTo && !mPendingPrim.mSlide && mPendingPrim.mRunUp == 0 && mPendingPrim.mGait != Gait::eRoll)
        {
            if (mVary == Lookahead::Way::eQuiet)
            {
                mPendingPrim.mGait = Gait::eSneak;
            }
            else if (mVary == Lookahead::Way::eFast)
            {
                mPendingPrim.mGait = Gait::eRun;
            }
        }
        mPilot.Start(mPendingPrim, mAbe);
        mPilotPending = false;
        mPilotActive = true;
    }

    // Saving them all: a Mudokon lost is as bad as Abe lost, so back and try again
    // (when the power to go unseen was last in hand: chanted for, it is gone a few frames before he is unseen)
    if (mWorld.mInvisCharge)
    {
        mChargeFrame = mAbe.mFrame;
    }
    // (and whether the power he is unseen by was chanted for a crossing: then it is a run against its clock,
    // for as long as it lasts, whatever step is at hand by then)
    if (!mWorld.mInvisible)
    {
        mInvisRun = false;
    }
    else if (!mWasInvisible)
    {
        mInvisRun = mInvisCross;
    }
    mWasInvisible = mWorld.mInvisible;
    if (mAuto && mProfile.mEnding == Ending::eSaveAll && mWorld.mKilled > mLastKilled && !mCheckpoints.empty() && mState == BotState::eRunning)
    {
        Log("a Mudokon died (killed " + std::to_string(mWorld.mKilled) + ")");
        mLastKilled = mWorld.mKilled;
        if (mProfile.mLook)
        {
            TryFailed(map, "a Mudokon died");
            return;
        }
        OnDeath(map);
        mDeaths--;
        return;
    }
    if (mWorld.mKilled != mLastKilled || mWorld.mRescued != mLastRescued)
    {
        Log("Mudokons: rescued " + std::to_string(mWorld.mRescued) + " killed " + std::to_string(mWorld.mKilled));
        mLastKilled = mWorld.mKilled;
        mLastRescued = mWorld.mRescued;
    }

    {
        std::string sligs;
        for (const SligAt& slig : mWorld.mSligs)
        {
            sligs += std::string(slig.mAsleep ? "z" : slig.mFacingLeft ? "L" : "R");
        }
        for (const SlogAt& slog : mWorld.mSlogs)
        {
            sligs += slog.mAsleep ? "s" : "S";
        }
        // A Slig that wasn't there a moment ago (its screen has just come in): whatever Abe was on his way to do
        // was decided without it, so stop and think again
        s32 awake = 0;
        for (const SligAt& slig : mWorld.mSligs)
        {
            awake += slig.mAsleep || slig.mPossessed ? 0 : 1;
        }
        // (not out of the run-up of a running jump: there he would run on off the end of the floor with no jump.
        // The Barracks' path 5: the run on the ledge at 1987,1660 woke the Slig under it, and he went off the
        // ledge's end onto the flying mine below.)
        if (awake > mLastAwakeSligs && mPilotActive && mPilotForTask && mAuto && mAbe.mOnGround && mPilot.Current().mKind != Prim::Kind::eRunJump)
        {
            mPilot.Start(Prim::Wait(2), mAbe);
        }
        mLastAwakeSligs = awake;
        // Coming down next to a Slog: what he was waiting out in the air gives way to the run that has to start
        // as he lands. And one that wakes or comes while he is on his way somewhere is not left until he is there.
        if (mAuto && mPilotActive && !mReflex && !mAbe.mTransition && mState == BotState::eRunning)
        {
            if (std::unique_ptr<Task> flee = MakeSlogFlee())
            {
                const TaskStep step = flee->Next(*this, "");
                if (step.mKind == TaskStep::Kind::ePrim)
                {
                    mReflex = std::move(flee);
                    mPilot.Start(step.mPrim, mAbe);
                    mPilotForTask = true;
                }
            }
        }
        if (sligs != mLastSligs)
        {
            std::string slogs;
            for (const SlogAt& slog : mWorld.mSlogs)
            {
                slogs += " " + std::to_string(slog.mX) + "," + std::to_string(slog.mY) + (slog.mAsleep ? " asleep" : "");
            }
            if (!slogs.empty())
            {
                Log("Slogs:" + slogs);
            }
            std::string text = "Sligs:";
            for (const SligAt& slig : mWorld.mSligs)
            {
                text += " " + std::to_string(slig.mX) + "," + std::to_string(slig.mY) + (slig.mAsleep ? " asleep" : slig.mFacingLeft ? " L" : " R");
            }
            Log(text);
            mLastSligs = sligs;
        }
    }

    mBits = 0;
    if (mWorld.mBeams.empty())
    {
        mBeamRolling = false;
    }
    if (mPilotActive && mAuto && BeamHolds())
    {
        // (nothing held: walking he stops at the next cell, still he stays still)
        mBeamHeld++;
        if (mBeamHeld == 60)
        {
            Log("keeping still for a motion detector, doing " + mPilot.Current().Text());
        }
        if (BeamRoll())
        {
            mBeamHeld = 0;
            mPilotResult = mPilot.Step(mAbe, mBits);
        }
        else if (mBeamHeld > 900)
        {
            // No move fits between its passes here: what was being done has failed
            Log("the motion detector leaves no time to get on");
            mBeamHeld = 0;
            mPilotActive = false;
            mPilotResult = PilotStatus::eFailed;
            if (mPilotForTask)
            {
                mLastPrimFailed = "a motion detector leaves no time to get on";
            }
        }
    }
    else if (mPilotActive)
    {
        mBeamHeld = 0;
        mPilotResult = mPilot.Step(mAbe, mBits);
        if (mPilotResult != PilotStatus::eRunning)
        {
            mPilotActive = false;
            mBits = 0;
            if (mPilotForTask && mPilotResult == PilotStatus::eFailed)
            {
                mLastPrimFailed = mPilot.Why();
            }
        }
    }

    if (mState == BotState::eRunning && !mPilotActive && mKnow)
    {
        Think(map);
        if (mRestoring)
        {
            Input().mAutomationInput = 0;
            return;
        }
        if (mPilotActive && mAuto && BeamHolds())
        {
        }
        else if (mPilotActive)
        {
            // The new Prim's first buttons go in this frame
            mPilotResult = mPilot.Step(mAbe, mBits);
            if (mPilotResult != PilotStatus::eRunning)
            {
                mPilotActive = false;
                mBits = 0;
                if (mPilotResult == PilotStatus::eFailed)
                {
                    mLastPrimFailed = mPilot.Why();
                }
            }
        }
    }

    // Only while it is playing (and once more to let go), so a script's own hold isn't overwritten
    const bool driving = mPilotActive || mState == BotState::eRunning;
    if (driving || mWasDriving)
    {
        Input().mAutomationInput = mBits;
    }
    mWasDriving = driving;
    if (mState == BotState::eRunning)
    {
        if (!mInputsStarted)
        {
            mInputsStarted = true;
            mInputsStartFrame = mAbe.mFrame;
        }
        mInputs.push_back(mBits);
    }

    if (mTracing)
    {
        char line[160];
        snprintf(line, sizeof(line), "%u %d %d %s m=%d af=%d %c vx=%d g=%d in=%x cam=%d\n", mAbe.mFrame, mAbe.mX, mAbe.mY, MoveName(mAbe.mMove), mAbe.mMotion, mAbe.mAnimFrame,
                 mAbe.mFacingLeft ? 'L' : 'R', mAbe.mVelXFp / 656, mAbe.mOnGround ? 1 : 0, Input().mAutomationInput, mAbe.mCamera);
        mTrace += line;
        // (the move the pilot is making, when it changes)
        const std::string move = mPilot.Current().Text() + (mPilot.Current().mRunUp != 0 ? " run-up " + std::to_string(mPilot.Current().mRunUp) : "");
        if (move != mTraceMove)
        {
            mTraceMove = move;
            mTrace.pop_back();
            mTrace += " [" + move + "]\n";
        }
        // (his health while it is down: a Fleech's lick takes 15 in 100 of it)
        if (mAbe.mHealthFp < 0x10000)
        {
            mTrace.pop_back();
            mTrace += " hp=" + std::to_string(mAbe.mHealthFp * 100 / 0x10000) + "\n";
        }
        // (what of him the Sligs in the background have to see none of, while there are such)
        if ((!mWorld.mSligs.empty() && mWorld.mSligs[0].mZShoot) || !mWorld.mBombs.empty())
        {
            mTrace.pop_back();
            mTrace += " box=" + std::to_string(mAbe.mBoxX1 - mAbe.mX) + "," + std::to_string(mAbe.mTop - mAbe.mY) + "," + std::to_string(mAbe.mBoxX2 - mAbe.mX) + "," + std::to_string(mAbe.mBoxBottom - mAbe.mY) + "\n";
        }
        // The flying mines about: the box of each
        for (const BombCycle::Seen& bomb : mWorld.mBombs)
        {
            mTrace.pop_back();
            mTrace += " bm=" + std::to_string(bomb.mX / 256 + bomb.mLeft) + "," + std::to_string(bomb.mY / 256 + bomb.mTop) + "," + std::to_string(bomb.mX / 256 + bomb.mRight) + "," + std::to_string(bomb.mY / 256 + bomb.mBottom) + "\n";
        }
        // The flying Sligs about: where, which way they face and how fast they go
        for (const FlierAt& flier : mWorld.mFliers)
        {
            mTrace.pop_back();
            mTrace += " fl=" + std::to_string(flier.mX) + "," + std::to_string(flier.mY) + (flier.mFacingLeft ? "L" : "R") + std::to_string(flier.mVelX * 10 / 256) + "\n";
        }
        // The Paramites near him: where, doing what (motion, brain and its state), facing which way
        for (const ParamiteAt& paramite : mWorld.mParamites)
        {
            if (std::abs(paramite.mX - mAbe.mX) < 800 && std::abs(paramite.mY - mAbe.mY) < 400)
            {
                mTrace.pop_back();
                mTrace += " para=" + std::to_string(paramite.mX) + "," + std::to_string(paramite.mY) + ",m" + std::to_string(paramite.mMotion) + ",b" + std::to_string(paramite.mBrain) + "." + std::to_string(paramite.mSub) + (paramite.mPossessed ? "P" : "") + (paramite.mFacesLeft ? "L\n" : "R\n");
            }
        }
        // The Scrabs near him: where, doing what, facing which way
        for (const ScrabAt& scrab : mWorld.mScrabs)
        {
            if (std::abs(scrab.mX - mAbe.mX) < 700 && std::abs(scrab.mY - mAbe.mY) < 400)
            {
                mTrace.pop_back();
                mTrace += " scrab=" + std::to_string(scrab.mX) + "," + std::to_string(scrab.mY) + ",m" + std::to_string(scrab.mMotion) + (scrab.mFacesLeft ? "L\n" : "R\n");
            }
        }
        // The Fleeches near him (a: awake, s: asleep), and the invisibility he has or can chant for
        for (s32 pass = 0; pass < 2; pass++)
        {
            for (const RockAt& fleech : pass == 0 ? mWorld.mFleechesAwake : mWorld.mFleechesAsleep)
            {
                if (std::abs(fleech.mX - mAbe.mX) < 500 && std::abs(fleech.mY - mAbe.mY) < 300)
                {
                    mTrace.pop_back();
                    mTrace += " fl=" + std::to_string(fleech.mX) + "," + std::to_string(fleech.mY) + (pass == 0 ? (fleech.mChasing ? "c\n" : "a\n") : "s\n");
                }
            }
        }
        for (const SlurgAt& slurg : mWorld.mSlurgs)
        {
            if (std::abs(slurg.mX - mAbe.mX) < 400 && std::abs(slurg.mY - mAbe.mY) < 200)
            {
                mTrace.pop_back();
                mTrace += " sl=" + std::to_string(slurg.mX1) + ".." + std::to_string(slurg.mX2) + (slurg.mDir < 0 ? "L\n" : slurg.mDir > 0 ? "R\n" : "?\n");
            }
        }
        if (mWorld.mInvisCharge || mWorld.mInvisible)
        {
            mTrace.pop_back();
            mTrace += (mWorld.mInvisible ? " invisible" + std::to_string(mWorld.mInvisLeft) : std::string(" ring")) + "\n";
        }
        // The Mudokons on the screen: where and in which brain state
        if (!mWorld.mEntities.empty())
        {
            mTrace.pop_back();
            for (const Entity& e : mWorld.mEntities)
            {
                if (e.mKind == EntityKind::eMudokon && e.mLive && e.mCamera != -1 && std::abs(e.mLiveX - mAbe.mX) < 400 && std::abs(e.mLiveY - mAbe.mY) < 130)
                {
                    mTrace += " " + std::to_string(e.mLiveX) + ":" + std::to_string(e.mState) + (e.mFollowing ? "f" : "");
                }
            }
            for (const SligAt& slig : mWorld.mSligs)
            {
                mTrace += " slig" + std::to_string(slig.mX) + "," + std::to_string(slig.mY) + (slig.mFacingLeft ? "L" : "R") + "m" + std::to_string(slig.mMotion);
            }
            for (const SlogAt& slog : mWorld.mSlogs)
            {
                mTrace += " slog" + std::to_string(slog.mX) + "," + std::to_string(slog.mY) + (slog.mAsleep ? "z" : "") + "m" + std::to_string(slog.mMotion) + (slog.mFacesLeft ? "L" : "R");
            }
            for (const SweepAt& sweep : mWorld.mSweeps)
            {
                if (std::abs(sweep.mX1 - mAbe.mX) < 400 && std::abs(sweep.mY1 - mAbe.mY) < 260)
                {
                    mTrace += " drill" + std::to_string(sweep.mX1) + "," + std::to_string(sweep.mY1) + "," + std::to_string(sweep.mX2) + "," + std::to_string(sweep.mY2) + "p" + std::to_string(sweep.mPhase) + " top" + std::to_string(mAbe.mTop);
                }
            }
            mTrace += "\n";
        }
    }
}

} // namespace Bot
