#include "Bot.hpp"
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
    mStartWaits = 0;
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
            const s32 to = mKnow->mNav.Edges()[step.mEdge].mTo;
            return std::make_unique<GoToTask>(action.mName, [to](s32 node)
                                              { return node == to; });
        }
        case StepKind::eUse:
        {
            const u32 entity = step.mEntity;
            const s32 value = action.mEffects[0].mValue;
            return std::make_unique<ClearSligTask>(entity, [entity, value]() -> std::unique_ptr<Task>
                                                   { return std::make_unique<UseTask>(entity, value); });
        }
        case StepKind::eRescue:
        {
            const u32 entity = step.mEntity;
            const u32 other = step.mOther;
            return std::make_unique<ClearSligTask>(entity, [entity, other]() -> std::unique_ptr<Task>
                                                   { return std::make_unique<RescueTask>(entity, other); });
        }
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
        case StepKind::eLogic:
            break;
    }
    return nullptr;
}

bool Bot::Replan(BaseMap& map)
{
    const PathKnowledge* know = mKnow;
    const s32 abeNode = AbeNode();
    if (abeNode < 0)
    {
        // In the air, on a lift, mid door: look again shortly
        if (++mLogicWaits > 60)
        {
            Stuck("no_plan: Abe isn't on a floor the nav graph knows, at " + std::to_string(mAbe.mX) + "," + std::to_string(mAbe.mY));
            return false;
        }
        mTask = std::make_unique<GoToTask>("wait to land", [](s32)
                                           { return true; });
        mTaskAction.clear();
        return true;
    }

    // A switch tried for the sake of it has changed: don't try it again
    for (s32 i = 0; i < 256; i++)
    {
        if (mWorld.mSwitches[i] != mLastSwitches[i])
        {
            mExplored.insert({mAbe.mPath, Key::Switch(i)});
            mLastSwitches[i] = mWorld.mSwitches[i];
        }
    }
    Domain domain = Domain::Build(mKnow->mNav, mWorld, abeNode, [know](s32 edge)
                                  { return know->mBannedEdges.count(edge) == 0; }, mProfile.mEnding == Ending::eKillAll);
    // Actions that keep failing are left out, and so are ways out that lead back to where the run has been
    std::vector<Goap::Action> actions;
    for (const Goap::Action& a : domain.mActions)
    {
        const auto failed = mFailedActions.find(a.mName);
        if (failed != mFailedActions.end() && failed->second >= 2)
        {
            continue;
        }
        const DomainStep& step = domain.mSteps[a.mTag];
        if (step.mKind == StepKind::eExit)
        {
            const NavFeature& f = mKnow->mNav.Input().mFeatures[mKnow->mNav.Exits()[step.mEdge].mFeature];
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

    // Is there a way out as things stand? Then nothing is done to a Mudokon that would leave none: some are
    // needed where they are (to turn a wheel, or because only rescuing them opens the door)
    const bool wayOut = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, actions).mFound;
    Goap::Plan best;
    std::string bestGoal;
    std::string misses;
    for (const auto& goal : goals)
    {
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
        }
        else if (!plan.mFound)
        {
            misses += goal.first + ": " + plan.mMiss + "; ";
        }
    }
    if (!best.mFound)
    {
        const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, actions);
        if (plan.mFound && !plan.mSteps.empty())
        {
            best = plan;
            bestGoal = "Leave the path";
        }
        else
        {
            // No way out is known yet: change what can be changed, a door it opens may show one. Switches the
            // run has already set once are left alone, so it can't go round in circles.
            for (const Goap::Fact& fact : domain.Reachable())
            {
                if (fact.mKey < 1000 || fact.mKey >= 1000000 || mExplored.count({mAbe.mPath, fact.mKey}))
                {
                    continue;
                }
                const Goap::Plan sub = Goap::Planner::Find(domain.mStart, {fact}, actions);
                if (sub.mFound && !sub.mSteps.empty() && (!best.mFound || sub.mCost < best.mCost))
                {
                    best = sub;
                    bestGoal = "Try switch " + std::to_string(fact.mKey - 1000) + "=" + std::to_string(fact.mValue);
                    mExploring = fact.mKey;
                }
            }
            if (!best.mFound)
            {
                Stuck("no_plan: Leave the path: " + plan.mMiss + ". " + domain.Frontier() + (misses.empty() ? "" : " Also no plan for: " + misses));
                return false;
            }
        }
    }
    else
    {
        mExploring = 0;
    }
    if (bestGoal.rfind("Try switch", 0) != 0)
    {
        mExploring = 0;
    }

    mPlanText.clear();
    for (s32 stepIdx : best.mSteps)
    {
        mPlanText += (mPlanText.empty() ? "" : ", ") + actions[stepIdx].mName;
    }
    const Goap::Action& first = actions[best.mSteps[0]];
    if (first.mName != mTaskAction)
    {
        Log("goal " + bestGoal + " plan=[" + mPlanText + "]" + (misses.empty() ? "" : " (no plan for: " + misses + ")"));
    }
    mTask = MakeTask(domain, first);
    mTaskAction = first.mName;
    mTaskOptional = domain.mSteps[first.mTag].mKind == StepKind::eRescue || domain.mSteps[first.mTag].mKind == StepKind::eKill;
    if (!mTask)
    {
        // A switch that follows from others: it takes the game a moment
        if (++mLogicWaits > 40)
        {
            mFailedActions[first.mName] = 2;
            mLogicWaits = 0;
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
    // A level with no way out known: anywhere new
    return here < 0 && there < 0 && mVisitedPaths.count({destLevel, destPath}) == 0;
}

std::string Bot::Report() const
{
    std::string out = "bot f=" + std::to_string(mAbe.mFrame) + " " + GameView::LevelName(mAbe.mLevel) + "/" + std::to_string(mAbe.mPath) + "/" + std::to_string(mAbe.mCamera) + " profile=" + mProfile.Text()
                    + " state=" + (mState == BotState::eDone ? "done" : mState == BotState::eStuck ? "stuck" : mState == BotState::eRunning ? "running" : "idle") + " deaths=" + std::to_string(mDeaths) + " restores=" + std::to_string(mRestores)
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

NavGraph::EdgeCost Bot::EdgeCostFn() const
{
    const PathKnowledge* know = mKnow;
    const World* world = &mWorld;
    return [know, world](const NavEdge& e) -> s32
    {
        const s32 idx = static_cast<s32>(&e - know->mNav.Edges().data());
        if (know->mBannedEdges.count(idx))
        {
            return -1;
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
        const NavNode* stop = e.mKind == EdgeKind::eLift ? &from : (to.mLiftStop >= 0 && from.mLiftStop < 0 ? &to : nullptr);
        if (stop)
        {
            // (one that isn't about yet, its camera not being loaded, is taken to be there)
            bool known = false;
            bool here = false;
            for (const LiftAt& lift : world->mLifts)
            {
                if (stop->mX >= lift.mX1 && stop->mX <= lift.mX2)
                {
                    known = true;
                    here = here || std::abs(lift.mY - stop->mY) <= 4;
                }
            }
            if (known && !here)
            {
                return -1;
            }
        }
        return e.mCost;
    };
}

s32 Bot::RetryDelay()
{
    if (mAttempt <= 0)
    {
        return 0;
    }
    // Longer each time, never the same twice
    return mAttempt * 17 + mRng.Range(0, 12);
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
        mLevel.Build(mAbe.mLevel, GameView::ReadLevel(map));
        Log("level graph:\n" + mLevel.Text());
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
    mKnow = &it->second;
}

void Bot::Stuck(const std::string& why)
{
    mState = BotState::eStuck;
    mWhy = why;
    mTask.reset();
    Log("STUCK: " + why);
}

void Bot::MaybeCheckpoint(BaseMap& map)
{
    // Standing or crouched only: a save taken hanging from a ledge or holding a lift's rope didn't come back right
    if ((mAbe.mMove != Move::eIdle && mAbe.mMove != Move::eCrouch) || !mAbe.mOnGround || mAbe.mTransition || !Checkpoint::CanSave(map))
    {
        return;
    }
    if (!mCheckpoints.empty() && mAbe.mFrame - mLastCheckpointFrame < kCheckpointEveryFrames)
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
        if (!slig.mPossessed && mKnow->mNav.CameraOf(slig.mX, slig.mY - 5) == mKnow->mNav.CameraOf(mAbe.mX, mAbe.mY - 5))
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
    mCheckpoints.push_back(std::move(saved));
    mLastCheckpointFrame = mAbe.mFrame;
    mAttempt = 0;
}

void Bot::OnDeath(BaseMap& map)
{
    mDeaths++;
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
    if (mCheckpoints.empty())
    {
        Stuck("died with nothing to go back to");
        return;
    }
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
    Log("died (" + std::string(mTask ? mTask->Name() : "no task") + "), back to f=" + std::to_string(saved.mCheckpoint.mFrame) + " try " + std::to_string(mAttempt));
    mInputs.resize(saved.mInputCount);
    saved.mCheckpoint.Load(map);
    mRestoring = true;
    mRestores++;
    mLastCheckpointFrame = saved.mCheckpoint.mFrame;
}

std::unique_ptr<Task> Bot::MakeReflex()
{
    if (!mAuto || !mAbe.Settled() || !mAbe.mOnGround || mAbe.mTransition)
    {
        return nullptr;
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
        if (!mTask)
        {
            if (!mAuto)
            {
                mState = BotState::eDone;
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
    }

    if (mPilotPending)
    {
        mPilot.Start(mPendingPrim, mAbe);
        mPilotPending = false;
        mPilotActive = true;
    }

    // Saving them all: a Mudokon lost is as bad as Abe lost, so back and try again
    if (mAuto && mProfile.mEnding == Ending::eSaveAll && mWorld.mKilled > mLastKilled && !mCheckpoints.empty() && mState == BotState::eRunning)
    {
        Log("a Mudokon died (killed " + std::to_string(mWorld.mKilled) + ")");
        mLastKilled = mWorld.mKilled;
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
    if (mPilotActive)
    {
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
        if (mPilotActive)
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
            for (const SlogAt& slog : mWorld.mSlogs)
            {
                mTrace += " slog" + std::to_string(slog.mX) + "," + std::to_string(slog.mY) + (slog.mAsleep ? "z" : "");
            }
            mTrace += "\n";
        }
    }
}

} // namespace Bot
