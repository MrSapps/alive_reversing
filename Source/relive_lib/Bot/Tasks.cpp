#include "Task.hpp"
#include "Bot.hpp"
#include "Domain.hpp"
#include <algorithm>
#include <cstdlib>

namespace Bot {

GoToTask::GoToTask(std::string name, std::function<bool(s32 node)> isGoal)
    : mName(std::move(name))
    , mIsGoal(std::move(isGoal))
{
}

std::unique_ptr<GoToTask> GoToTask::To(Bot& bot, s32 x, s32 y, bool half)
{
    const s32 target = bot.Nav().NodeAt(x, y, half, 40);
    return std::make_unique<GoToTask>("GoTo " + std::to_string(x) + "," + std::to_string(y), [target](s32 node)
                                      { return node == target && target >= 0; });
}

// What to hold as Abe lands from the fall or the well that is the path's first edge: nothing, but with a Scrab
// loose on the floor he comes down on, the run that is next. Held as his feet touch it goes straight from the
// landing into the run, some ten frames sooner than standing up first, and a Scrab leaves no ten frames.
static u32 LandingBits(const Bot& bot, const NavGraph& nav, const std::vector<s32>& path)
{
    // (not on a stretch that was timed, where it ends with the landing: there is something to do there)
    // (nor unseen: there is nothing to run from)
    if (path.size() < 2 && (bot.KeepsTo() || bot.GetWorld().mInvisible))
    {
        return 0;
    }
    // Which way: the way the path goes on, or where it ends with the landing, away from the Scrab
    s32 way = 0;
    if (path.size() >= 2)
    {
        const NavEdge& next = nav.Edges()[path[1]];
        if (next.mKind != EdgeKind::eWalk && next.mKind != EdgeKind::eRunJump)
        {
            return 0;
        }
        way = next.mLeft ? -1 : 1;
    }
    const NavNode& down = nav.Nodes()[nav.Edges()[path[0]].mTo];
    // Whether the walk he would land into ends at a door: that wants him standing at it, and a well that throws
    // him further than was thought has him run past it (the Mudanchee vault's path 5, its first door)
    bool toDoor = false;
    for (std::size_t i = 1; i < path.size(); i++)
    {
        const EdgeKind kind = nav.Edges()[path[i]].mKind;
        if (kind != EdgeKind::eWalk)
        {
            toDoor = kind == EdgeKind::eDoor;
            break;
        }
    }
    s32 nearest = 700;
    bool loose = false;
    for (const ScrabAt& scrab : bot.GetWorld().mScrabs)
    {
        // (where the way ends with the landing or leads to a door, not from one shut in behind a gate: there is
        // nothing to run from, and something to do where he lands. Else he runs on as before: the Mudanchee
        // vault's path 6 is timed to it.)
        if (!scrab.mPossessed && scrab.mHalf == down.mHalf && std::abs(scrab.mY - down.mY) * (down.mHalf ? 2 : 1) <= 30 && std::abs(scrab.mX - down.mX) < nearest && ((path.size() >= 2 && !toDoor) || !Domain::GateBetween(nav, bot.GetWorld(), scrab.mX, down.mX, down.mY, down.mHalf)))
        {
            nearest = std::abs(scrab.mX - down.mX);
            loose = true;
            if (path.size() < 2)
            {
                way = scrab.mX < down.mX ? 1 : -1;
            }
        }
    }
    if (!loose || way == 0)
    {
        return 0;
    }
    return (way < 0 ? Btn::eLeft : Btn::eRight) | Btn::eRun;
}

TaskStep GoToTask::Next(Bot& bot, const std::string& lastFailed)
{
    if (mDisarm)
    {
        const TaskStep step = mDisarm->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            bot.Log("couldn't turn the UXB off: " + step.mWhy);
        }
        mDisarm.reset();
        return Next(bot, "");
    }

    if (mStop)
    {
        const TaskStep step = mStop->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            bot.Log(mStop->Name() + " didn't work: " + step.mWhy);
        }
        mStop.reset();
        return Next(bot, "");
    }

    const AbeState& abe = bot.Abe();
    NavGraph& nav = bot.Nav();

    if (++mSteps > 400)
    {
        return TaskStep::Failed("too many steps");
    }

    // A try after a death holds back a little first: most of what kills is on a timer or a patrol
    if (!mWaited)
    {
        mWaited = true;
        const s32 delay = bot.RetryDelay();
        if (delay > 0)
        {
            return TaskStep::Do(Prim::Wait(delay));
        }
    }

    if (abe.mMove == Move::eHang)
    {
        // Hanging from a ledge: let go if that's the way down, else climb up
        bool down = mLastEdge >= 0 && nav.Edges()[mLastEdge].mKind == EdgeKind::eHoistDown;
        // (or caught hold of it in a fall past it, with the way on down from its top: the Mudanchee trial, rolled
        // off the well's ledge at 1337,340 into the hoist at 1412)
        if (!down && bot.AbeNode() >= 0)
        {
            std::vector<s32> onDown;
            down = nav.FindPath(bot.AbeNode(), mIsGoal, bot.EdgeCostFn(), onDown) && !onDown.empty() && nav.Edges()[onDown[0]].mKind == EdgeKind::eHoistDown;
        }
        // (here again after a step that failed: crouching at the top of a hoist is climbing down it, so a roll
        // can't start there)
        const s32 top = bot.AbeNode();
        std::vector<s32> on;
        if (!down && !lastFailed.empty() && ++mHangFails >= 2 && top >= 0 && nav.FindPath(top, mIsGoal, bot.EdgeCostFn(), on) && !on.empty() && nav.Edges()[on[0]].mKind == EdgeKind::eRoll)
        {
            bot.Log("no rolling on from the top of the hoist at " + std::to_string(nav.Nodes()[top].mX) + "," + std::to_string(nav.Nodes()[top].mY));
            bot.BanEdge(on[0]);
            mHangFails = 0;
        }
        return TaskStep::Do(Prim::Press(down ? Btn::eDown : Btn::eUp));
    }

    // On a lift that is between two of its stops (a ride that was cut short): it glides on to the next one
    for (const LiftAt& lift : bot.GetWorld().mLifts)
    {
        if (abe.mX < lift.mX1 || abe.mX > lift.mX2 || std::abs(lift.mY - abe.mY) > 6 || mLiftGlide > 150)
        {
            continue;
        }
        bool atStop = false;
        for (const NavNode& n : nav.Nodes())
        {
            atStop = atStop || (n.mLiftStop >= 0 && n.mX >= lift.mX1 && n.mX <= lift.mX2 && std::abs(n.mY - lift.mY) <= 1);
        }
        if (!atStop)
        {
            mLiftGlide++;
            return TaskStep::Do(Prim::Wait(2));
        }
    }
    mLiftGlide = 0;

    const s32 cur = bot.AbeNode();
    if (cur < 0)
    {
        if (abe.mMove == Move::eCrouch)
        {
            return TaskStep::Do(Prim::Press(Btn::eUp));
        }
        if (++mOffGraph > 5)
        {
            return TaskStep::Failed("off the nav graph at " + std::to_string(abe.mX) + "," + std::to_string(abe.mY));
        }
        return TaskStep::Do(Prim::Wait(10));
    }
    mOffGraph = 0;

    // What the last edge really did (not after a reflex has moved him: where he is now is its doing)
    if (bot.Reflexes() != mReflexes)
    {
        mReflexes = bot.Reflexes();
        mLastEdge = -1;
    }
    if (mLastEdge >= 0)
    {
        const NavEdge& e = nav.Edges()[mLastEdge];
        if (cur == mLastFrom)
        {
            bot.Log("edge " + std::string(EdgeKindName(e.mKind)) + " from " + std::to_string(nav.Nodes()[cur].mX) + "," + std::to_string(nav.Nodes()[cur].mY) + " went nowhere" + (lastFailed.empty() ? "" : ": " + lastFailed));
            if (++mSameSpot >= 2)
            {
                bot.BanEdge(mLastEdge);
                mSameSpot = 0;
            }
        }
        else
        {
            mSameSpot = 0;
            if (mLeading && e.mKind == EdgeKind::eFall)
            {
                // On from the ledge, the way the drop went
                s32 at = cur;
                for (s32 k = 0; k < 4; k++)
                {
                    s32 next = -1;
                    const NavNode& n = nav.Nodes()[at];
                    for (s32 i = n.mFirstEdge; i < n.mFirstEdge + n.mEdgeCount; i++)
                    {
                        const NavEdge& w = nav.Edges()[i];
                        if (w.mKind == EdgeKind::eWalk && w.mLeft == e.mLeft && w.mFeature < 0 && !nav.Nodes()[w.mTo].mHazard)
                        {
                            next = w.mTo;
                        }
                    }
                    if (next < 0)
                    {
                        break;
                    }
                    at = next;
                }
                mAwayNode = at;
                mAwayTries = 0;
                mLeadWaits = 0;
            }
            if (cur != e.mTo)
            {
                bot.Log("edge " + std::string(EdgeKindName(e.mKind)) + " led to " + std::to_string(nav.Nodes()[cur].mX) + "," + std::to_string(nav.Nodes()[cur].mY) + ", not " + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY));
                // (a lift ride that stopped at another floor was cut short: it goes where it goes)
                if (e.mKind != EdgeKind::eLift)
                {
                    nav.LearnEdge(mLastEdge, cur);
                }
            }
        }
        mLastEdge = -1;
    }
    else if (!lastFailed.empty() && mLastFrom == cur)
    {
        // A walk that got nowhere: whatever is in the way isn't in the graph
        std::vector<s32> path;
        if (nav.FindPath(cur, mIsGoal, bot.EdgeCostFn(), path) && !path.empty())
        {
            bot.Log("walk from " + std::to_string(nav.Nodes()[cur].mX) + "," + std::to_string(nav.Nodes()[cur].mY) + " " + lastFailed);
            // (once can be anything: a rewind that left him mid-step)
            if (bot.NoteBlocked(path[0]))
            {
                bot.BanEdge(path[0]);
            }
        }
    }

    if (mAwayNode >= 0)
    {
        if (cur != mAwayNode && ++mAwayTries <= 3)
        {
            return TaskStep::Do(Prim::MoveTo(nav.Nodes()[mAwayNode].mX, Gait::eWalk));
        }
        // Until they have dropped down and caught up
        const s32 step = NavGraph::GridStep(abe.mHalfScale);
        bool behind = false;
        for (const Entity& e : bot.GetWorld().mEntities)
        {
            if (e.mKind == EntityKind::eMudokon && e.mLive && e.mFollowing && (std::abs(e.mLiveY - abe.mY) > 10 || std::abs(e.mLiveX - abe.mX) > 3 * step))
            {
                behind = true;
            }
        }
        if (behind && mLeadWaits < 45)
        {
            mLeadWaits++;
            mSteps--;
            return TaskStep::Do(Prim::Wait(10));
        }
        mAwayNode = -1;
        mLeadWaits = 0;
    }

    std::vector<s32> path;
    const bool atGoal = mIsGoal(cur);
    bool led = false;
    if (!atGoal && mLeading)
    {
        const NavGraph::EdgeCost all = bot.EdgeCostFn();
        led = nav.FindPath(cur, mIsGoal, [&all](const NavEdge& e) -> s32
                           { return e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eFall ? all(e) : -1; }, path)
           && !path.empty();
    }
    if (!atGoal && !led && (!nav.FindPath(cur, mIsGoal, bot.EdgeCostFn(), path) || path.empty()))
    {
        return TaskStep::Failed("no route from " + std::to_string(nav.Nodes()[cur].mX) + "," + std::to_string(nav.Nodes()[cur].mY));
    }
    // With Mudokons in tow and a UXB to hop: they can't hop, so it is turned off first
    if (mLeading && !led && !atGoal && mDisarms < 4)
    {
        bool followed = false;
        for (const Entity& e : bot.GetWorld().mEntities)
        {
            followed = followed || (e.mKind == EntityKind::eMudokon && e.mLive && e.mFollowing);
        }
        for (std::size_t i = 0; followed && i < path.size() && i < 40; i++)
        {
            const NavEdge& e = nav.Edges()[path[i]];
            if (e.mKind != EdgeKind::eHop && e.mKind != EdgeKind::eRunJump)
            {
                continue;
            }
            const NavNode& a = nav.Nodes()[e.mFrom];
            const NavNode& b = nav.Nodes()[e.mTo];
            if (const UxbAt* uxb = DisarmTask::Under(bot.GetWorld(), a.mX, b.mX, a.mY, a.mHalf))
            {
                mDisarms++;
                bot.Log("a UXB at " + std::to_string(uxb->mX) + " is in the way of the Mudokons: turning it off");
                mDisarm = std::make_unique<DisarmTask>(uxb->mX, uxb->mY, uxb->mHalf);
                return Next(bot, "");
            }
        }
    }
    if (led)
    {
        // Let them catch up before going on: one left behind on another screen goes back to its work
        const s32 step = NavGraph::GridStep(abe.mHalfScale);
        bool behind = false;
        for (const Entity& e : bot.GetWorld().mEntities)
        {
            if (e.mKind == EntityKind::eMudokon && e.mLive && e.mFollowing && std::abs(e.mLiveY - abe.mY) <= 10 && std::abs(e.mLiveX - abe.mX) > 3 * step)
            {
                behind = true;
            }
        }
        if (behind && mLeadWaits < 60)
        {
            mLeadWaits++;
            mSteps--;
            return TaskStep::Do(Prim::Wait(10));
        }
        mLeadWaits = 0;
    }

    // Crouched after a roll: up again, unless there's more to roll or no room to stand
    if (abe.mMove == Move::eCrouch && !nav.Nodes()[cur].mLow && !(mSweepDuck && !atGoal) && (atGoal || (nav.Edges()[path[0]].mKind != EdgeKind::eRoll && !nav.Edges()[path[0]].mCrouched)))
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }
    if (atGoal)
    {
        return TaskStep::Done();
    }

    // A Slig awake on a floor the way leads along, and a drill over it that a ring drops: that first
    if (!mTrapTried)
    {
        for (const SligAt& slig : bot.GetWorld().mSligs)
        {
            bool onWay = false;
            for (std::size_t i = 0; i < path.size() && i < 60; i++)
            {
                const NavNode& n = nav.Nodes()[nav.Edges()[path[i]].mTo];
                onWay = onWay || (n.mHalf == slig.mHalf && std::abs(n.mY - slig.mY) <= 45 && std::abs(n.mX - slig.mX) < 700);
            }
            u32 ring = 0;
            u32 drill = 0;
            if (onWay && !slig.mAsleep && !slig.mPossessed && TrapSligTask::Find(nav, bot.GetWorld(), slig, ring, drill))
            {
                mTrapTried = true;
                bot.Log("a Slig at " + std::to_string(slig.mX) + "," + std::to_string(slig.mY) + " walks under Drill#" + std::to_string(drill) + ": dropping that on it first");
                mStop = std::make_unique<TrapSligTask>(ring, drill);
                return Next(bot, "");
            }
        }
    }

    // A drill sweeps cells ahead: up to the last one it doesn't reach, then on when all of the way through fits
    // in before it is back
    std::size_t sweepStop = path.size();
    bool sweepWalk = false;
    std::size_t sweepRoll = 0;
    const bool wasDucked = mSweepDuck;
    mSweepDuck = false;
    {
        const World& world = bot.GetWorld();
        std::size_t in = 0;
        while (in < path.size() && in < 40 && !SweepOver(nav, world, nav.Edges()[path[in]].mTo))
        {
            in++;
        }
        const Entity* drill = SweepOver(nav, world, cur);
        const bool inside = drill != nullptr;
        const bool ahead = in < path.size() && in < 40;
        if (ahead && in > 0 && !inside)
        {
            sweepStop = in;
        }
        else if (inside || ahead)
        {
            std::size_t out = 0;
            while (out < path.size() && SweepOver(nav, world, nav.Edges()[path[out]].mTo))
            {
                out++;
            }
            drill = drill ? drill : SweepOver(nav, world, nav.Edges()[path[0]].mTo);
            const std::size_t count = std::min(out + 1, path.size());
            sweepWalk = true;
            // Its lever, if that can be got to without going past it: no timing needed then
            const Entity* lever = !inside && mStops < 2 && bot.MayStopSweep() ? StopFor(bot, *drill, cur) : nullptr;
            if (lever)
            {
                mStops++;
                bot.StoppingSweep();
                bot.Log("stopping Drill#" + std::to_string(drill->mId) + " with " + std::string(EntityKindName(lever->mKind)) + "#" + std::to_string(lever->mId) + " first");
                mStop = std::make_unique<UseTask>(lever->mId, drill->mInputs[1] ? 1 : 0);
                return Next(bot, "");
            }
            if (!world.SweepOf(drill->mId))
            {
                // Not about until its screen is: a cell at a time till it shows
                sweepStop = 1;
            }
            else if (!SweepsClear(nav, world, path, count, bot.SweepLate()))
            {
                // Where its stroke is too short to walk under at any time, a roll may fit: it is quicker and
                // lower. Tried once walking has had its chance.
                // (so is a stretch that starts with a roll: on rolling through what could be walked)
                std::size_t lead = 0;
                while (lead < count && lead < path.size())
                {
                    const NavEdge& e = nav.Edges()[path[lead]];
                    if ((e.mKind != EdgeKind::eWalk && e.mKind != EdgeKind::eRoll) || e.mLeft != nav.Edges()[path[0]].mLeft || nav.Nodes()[e.mTo].mHazard)
                    {
                        break;
                    }
                    lead++;
                }
                const bool walked = lead >= 1 && lead + 1 >= std::min(count, path.size());
                const bool mustRoll = nav.Edges()[path[0]].mKind == EdgeKind::eRoll && lead > 1;
                if (walked && !inside && (wasDucked || mustRoll || mSweepWait >= 70))
                {
                    mSweepDuck = true;
                    if (abe.mMove != Move::eCrouch)
                    {
                        mSweepWait += 2;
                        mSteps--;
                        return abe.Settled() ? TaskStep::Do(Prim::Press(Btn::eDown)) : TaskStep::Do(Prim::Wait(1));
                    }
                    if (SweepsClear(nav, world, path, count, bot.SweepLate(), true))
                    {
                        bot.Log("rolling through under the drill");
                        bot.SweepBegun();
                        mSweepWait = 0;
                        sweepRoll = lead;
                    }
                }
            }
            if (sweepRoll > 0 || !world.SweepOf(drill->mId))
            {
            }
            else if (!SweepsClear(nav, world, path, count, bot.SweepLate()))
            {
                // (where he is may be no better: then on regardless)
                const SweepAt* here = inside ? world.SweepOf(drill->mId) : nullptr;
                const NavNode& n = nav.Nodes()[cur];
                const bool stay = !here || !here->Hits(0, 120, n.mX - 3, n.mY - (n.mHalf ? 30 : 60), n.mX + 3, n.mY);
                if (stay && mSweepWait < 900)
                {
                    if (mSweepWait == 0)
                    {
                        bot.Log("a drill sweeps the way ahead: waiting for it to be out of the way");
                    }
                    if (const SweepAt* at = mSweepWait == 0 ? world.SweepOf(drill->mId) : nullptr)
                    {
                        bot.Log("drill " + std::to_string(at->mX1) + ".." + std::to_string(at->mX2) + " y " + std::to_string(at->mY1) + ".." + std::to_string(at->mY2) + " phase " + std::to_string(at->mPhase) + " off " + std::to_string(at->mOffset / 256) + " speed " + std::to_string(at->mSpeed) + " pause " + std::to_string(at->mPause) + " edges " + std::to_string(count));
                    }
                    mSweepWait += 2;
                    mSteps--;
                    return TaskStep::Do(Prim::Wait(2));
                }
                if (stay)
                {
                    bot.BanEdge(path[0]);
                    return TaskStep::Failed("the drill leaves no time to get through");
                }
            }
            else
            {
                bot.SweepBegun();
            }
            mSweepWait = 0;
        }
    }

    const NavEdge& first = nav.Edges()[path[0]];
    mLastFrom = cur;
    bot.SetCurrentEdge(path[0]);
    // The power to go invisible, and a Fleech near: one awake on his screen, or one on the floor he is on within
    // a screen's width. Chanted for until it takes. (Used no sooner: it lasts 600 frames or so and the way past
    // them is about that long.)
    {
        const World& w = bot.GetWorld();
        const NavNode& from = nav.Nodes()[cur];
        // (the rings of the lock just slapped still on their way to him, and the step at hand wants him unseen:
        // they are waited for here, where it is safe to chant)
        if (w.mInvisComing && !w.mInvisCharge && !w.mInvisible && bot.InvisAtOnce() && mInvisWaits < 120)
        {
            mInvisWaits++;
            mSteps--;
            return TaskStep::Do(Prim::Wait(2));
        }
        if (w.mInvisCharge && !w.mInvisible && mInvisChants < 30)
        {
            const s32 scale = from.mHalf ? 2 : 1;
            bool near = false;
            for (const RockAt& f : w.mFleechesAwake)
            {
                near = near || nav.CameraOf(f.mX, f.mY - 5) == nav.CameraOf(from.mX, from.mY - 5);
            }
            for (const RockAt& f : w.mFleechesAsleep)
            {
                near = near || (f.mHalf == from.mHalf && std::abs(f.mY - from.mY) * scale <= 30 && std::abs(f.mX - from.mX) * scale < 320);
            }
            // (kept for a step of the plan that wants him unseen: then, and not for these)
            if (bot.InvisSaved())
            {
                // (or a step sooner with a Fleech awake on his screen: it will not wait)
                bool awake = false;
                for (const RockAt& f : w.mFleechesAwake)
                {
                    awake = awake || (f.mHalf == from.mHalf && nav.CameraOf(f.mX, f.mY - 5) == nav.CameraOf(from.mX, from.mY - 5));
                }
                near = bot.InvisChantNow() || (awake && bot.InvisChantSoon());
                // (as late as can be: where the way goes onto a floor a Scrab can get at him on, only at the
                // last cell before it)
                for (std::size_t i = 0; near && i < path.size(); i++)
                {
                    const NavNode& to = nav.Nodes()[nav.Edges()[path[i]].mTo];
                    if (Domain::ScrabReaches(nav, w, to.mX, to.mY, to.mHalf, false))
                    {
                        near = i == 0;
                        break;
                    }
                }
            }
            // And whatever the plan says: about to step onto a floor a Scrab can get at him on, from one it cannot
            {
                const NavNode& to = nav.Nodes()[first.mTo];
                // (not on a stretch timed against the Scrab's beat with the power kept for something else)
                near = near || (!(bot.InvisSaved() && bot.KeepsTo()) && Domain::ScrabReaches(nav, w, to.mX, to.mY, to.mHalf, false) && !Domain::ScrabReaches(nav, w, from.mX, from.mY, from.mHalf, false));
            }
            // And with the power kept for nothing: onto the floor of any Scrab that is about, one that waits
            // before it strikes as well (the walkthrough's fifth Mudanchee vault: "climb back up to the Scrab's
            // level once again, turn invisible and run to the door")
            // (not on a stretch timed against its beat: that one does without)
            bool scrabFloor = false;
            if (!near && !bot.InvisSaved() && !bot.KeepsTo())
            {
                const std::vector<char> hot = ScrabWaryTask::Hot(bot);
                scrabFloor = hot[first.mTo] && !hot[cur];
                near = scrabFloor;
            }
            if (near && !abe.Settled())
            {
                // (still moving from what went before, a landing with the way on held: stopped for it first)
                mSteps--;
                return TaskStep::Do(Prim::Wait(1));
            }
            // Kept for a thing a Scrab stands at: not yet (a slap would land on the Scrab, and the power runs out
            // while he waits by it). From out of its reach he waits for it to go.
            if (near && bot.InvisSaved() && bot.UnseenUse() != 0 && !Domain::ScrabReaches(nav, w, from.mX, from.mY, from.mHalf, false))
            {
                const Entity* thing = w.Find(bot.UnseenUse());
                for (const ScrabAt& scrab : w.mScrabs)
                {
                    if (thing && !scrab.mPossessed && scrab.mHalf == thing->mHalf && std::abs(scrab.mY - thing->mY2) * (thing->mHalf ? 2 : 1) <= 30 && std::abs(scrab.mX - thing->CentreX()) * (thing->mHalf ? 2 : 1) <= 90 && mUnseenWaits < 600)
                    {
                        if (mUnseenWaits++ == 0)
                        {
                            bot.Log("a Scrab stands at #" + std::to_string(thing->mId) + ": the chant waits until it has gone");
                        }
                        mSteps--;
                        return TaskStep::Do(Prim::Wait(2));
                    }
                }
            }
            if (near)
            {
                if (mInvisChants++ == 0)
                {
                    bot.Log(bot.InvisSaved() ? "the step that wants him unseen is next: chanting for it" : scrabFloor ? "onto a Scrab's floor with the power to go invisible: chanting for it" : "Fleeches ahead and the power to go invisible: chanting for it");
                }
                mSteps--;
                return TaskStep::Do(Prim::Wait(20, Btn::eChant));
            }
        }
    }
    // A woken Fleech under the ledge he is about to leave, and another climb ahead of him (hanging from a hoist
    // is where it gets him): not until it has started up after him. It then has to come down again, which is
    // worth a couple of seconds. (Fleech.cpp: it climbs by the hoists, about 25 frames a floor.)
    {
        const NavNode& from = nav.Nodes()[cur];
        const NavNode& down = nav.Nodes()[first.mTo];
        const s32 scale = from.mHalf ? 2 : 1;
        bool climbAhead = false;
        for (std::size_t i = 1; i < path.size(); i++)
        {
            climbAhead = climbAhead || nav.Edges()[path[i]].mKind == EdgeKind::eHoistUp;
        }
        const bool leaves = (first.mKind == EdgeKind::eFall || first.mKind == EdgeKind::eHop) && (down.mY - from.mY) * scale > 40;
        if (leaves && climbAhead && mLureWaits < 150)
        {
            bool under = false;
            bool coming = false;
            for (const RockAt& f : bot.GetWorld().mFleechesAwake)
            {
                if (f.mHalf != from.mHalf || nav.CameraOf(f.mX, f.mY - 5) != nav.CameraOf(from.mX, from.mY - 5) || std::abs(f.mX - from.mX) * scale > 150)
                {
                    continue;
                }
                // (on the floor below; or off it and on its way up, or up already)
                under = under || f.mY >= down.mY - 5;
                coming = coming || f.mY < down.mY - 25 / scale;
            }
            if (under && !coming)
            {
                if (mLureWaits++ == 0)
                {
                    bot.Log("a Fleech awake under this ledge: waiting for it to start up before leaving");
                }
                mSteps--;
                return TaskStep::Do(Prim::Wait(1));
            }
        }
    }
    // (where a Scrab that is after him is, for the log)
    if (!mChaseLogged)
    {
        const NavNode& from = nav.Nodes()[cur];
        std::string where;
        for (const ScrabAt& scrab : bot.GetWorld().mScrabs)
        {
            if (!scrab.mPossessed && scrab.mHalf == from.mHalf && std::abs(scrab.mY - from.mY) <= (first.mKind == EdgeKind::eWell ? 400 : 30) && std::abs(scrab.mX - from.mX) < 700)
            {
                where += " " + std::to_string(scrab.mX) + "," + std::to_string(scrab.mY) + " (doing " + std::to_string(scrab.mMotion) + (scrab.mFacesLeft ? ", faces left)" : ", faces right)");
            }
        }
        if (!where.empty())
        {
            mChaseLogged = true;
            bot.Log("a Scrab loose on this floor, at" + where + ", next a " + EdgeKindName(first.mKind));
        }
    }
    // A Slurg where the next step comes down: hopped over if it is just ahead on this floor and the hop comes
    // down clear of every one of them, else waited for (they crawl half a pixel a frame). Not for ever: after a
    // long wait it is trodden on.
    {
        const World& w = bot.GetWorld();
        const NavNode& from = nav.Nodes()[cur];
        const NavNode& next = nav.Nodes()[first.mTo];
        // (only while there is a Fleech asleep to wake and none awake to get away from)
        // (one awake on another screen, behind a door he has shut, is nothing to run from)
        bool awakeHere = false;
        for (const RockAt& f : w.mFleechesAwake)
        {
            // (nor one in the background while he is in the foreground: it goes for nothing of another size)
            awakeHere = awakeHere || (f.mHalf == from.mHalf && nav.CameraOf(f.mX, f.mY - 5) == nav.CameraOf(from.mX, from.mY - 5));
        }
        // (nor is there any waiting once the switch that wakes one of this screen's is on: it is on its way down)
        for (const Entity& e : w.mEntities)
        {
            awakeHere = awakeHere || (e.mKind == EntityKind::eFleech && e.mSwitch > 1 && w.Switch(e.mSwitch) != 0 && nav.CameraOf(e.CentreX(), e.mY2 - 5) == nav.CameraOf(from.mX, from.mY - 5));
        }
        mFleechAwakeHere = awakeHere;
        // A hop or a drop down onto a floor they crawl along: timed for the gap between two of them, however
        // long that takes to come (the Slurgs of Necrum's path 6 go in file, fifty to seventy-five apart)
        const s32 scaleOf = from.mHalf ? 2 : 1;
        const bool lands = (first.mKind == EdgeKind::eHop || first.mKind == EdgeKind::eFall || first.mKind == EdgeKind::eHoistDown) && (next.mY - from.mY) * scaleOf > 20;
        if (lands && !w.mSlurgs.empty() && !w.mFleechesAsleep.empty() && !awakeHere && !w.mInvisible && mLandWaits < 1500)
        {
            // (down a hoist he turns by himself, hangs and lets go: some 45 frames)
            const s32 frames = first.mKind == EdgeKind::eHop ? 24 : first.mKind == EdgeKind::eHoistDown ? 45 : 34;
            if (!abe.Settled() || (abe.mFacingLeft != first.mLeft && first.mKind != EdgeKind::eHoistDown) || w.SlurgWhereHeLands(next.mX, next.mY, next.mHalf, first.mLeft, frames))
            {
                if (abe.Settled() && abe.mFacingLeft != first.mLeft && first.mKind != EdgeKind::eHoistDown)
                {
                    return TaskStep::Do(Prim::Face(first.mLeft));
                }
                if (mLandWaits++ == 0)
                {
                    bot.Log("Slurgs where he would come down at " + std::to_string(next.mX) + "," + std::to_string(next.mY) + ": waiting for a gap between them");
                }
                mSteps--;
                return TaskStep::Do(Prim::Wait(1));
            }
        }
        else if (!w.mSlurgs.empty() && !w.mFleechesAsleep.empty() && !awakeHere && !w.mInvisible && w.SlurgBy(next.mX, next.mY, next.mHalf) && mSlurgWaits < 80)
        {
            const s32 cell = NavGraph::GridStep(from.mHalf);
            const s32 scale = from.mHalf ? 2 : 1;
            const s32 dir = first.mLeft ? -1 : 1;
            const s32 land = nav.NodeAt(from.mX + dir * 3 * cell, from.mY, from.mHalf, 6);
            bool hop = first.mKind == EdgeKind::eWalk && path.size() >= 3 && nav.Edges()[path[1]].mKind == EdgeKind::eWalk && nav.Edges()[path[2]].mKind == EdgeKind::eWalk && nav.Edges()[path[2]].mTo == land && land >= 0 && !nav.Nodes()[land].mLow && !from.mLow;
            for (const SlurgAt& slurg : w.mSlurgs)
            {
                // (the feet come down nine behind and six ahead of where he lands, 75 on)
                const s32 ahead = (slurg.mX - from.mX) * dir * scale;
                if (slurg.mHalf == from.mHalf && std::abs(slurg.mY - from.mY) * scale <= 14 && ahead > 43 && ahead < 109)
                {
                    hop = false;
                }
            }
            if (hop && abe.Settled())
            {
                if (abe.mFacingLeft != first.mLeft)
                {
                    return TaskStep::Do(Prim::Face(first.mLeft));
                }
                bot.Log("a Slurg in the way at " + std::to_string(next.mX) + ": over it");
                mLastEdge = -1;
                return TaskStep::Do(Prim::Press(Btn::eHop));
            }
            if (mSlurgWaits++ == 0)
            {
                bot.Log("a Slurg in the way at " + std::to_string(next.mX) + "," + std::to_string(next.mY) + ": waiting for it to crawl on");
            }
            mSteps--;
            return TaskStep::Do(Prim::Wait(6));
        }
        if (!w.SlurgBy(next.mX, next.mY, next.mHalf))
        {
            mSlurgWaits = 0;
        }
    }
    if (sweepRoll > 0)
    {
        return TaskStep::Do(Prim::MoveTo(nav.Nodes()[nav.Edges()[path[sweepRoll - 1]].mTo].mX, Gait::eRoll));
    }
    switch (first.mKind)
    {
        case EdgeKind::eRoll:
        {
            std::size_t last = 0;
            while (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eRoll && nav.Edges()[path[last + 1]].mLeft == first.mLeft)
            {
                last++;
            }
            // Another low stretch the same way within three cells: rolled on to, not stood up for in between,
            // when there is no time for that (unseen for a while only, or a Fleech awake)
            while (bot.GetWorld().mInvisible || !bot.GetWorld().mFleechesAwake.empty())
            {
                std::size_t j = last + 1;
                while (j < path.size() && j <= last + 3 && nav.Edges()[path[j]].mKind == EdgeKind::eWalk && nav.Edges()[path[j]].mLeft == first.mLeft)
                {
                    j++;
                }
                if (j >= path.size() || j == last + 1 || nav.Edges()[path[j]].mKind != EdgeKind::eRoll || nav.Edges()[path[j]].mLeft != first.mLeft)
                {
                    break;
                }
                last = j;
                while (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eRoll && nav.Edges()[path[last + 1]].mLeft == first.mLeft)
                {
                    last++;
                }
            }
            if (sweepStop <= last)
            {
                last = sweepStop - 1;
            }
            Prim roll = Prim::MoveTo(nav.Nodes()[nav.Edges()[path[last]].mTo].mX, Gait::eRoll);
            // (at the top of a hoist down climbs down it: the Mudanchee vault's path 8, from 187,880)
            // (what the games look for under his feet when down is pressed)
            for (const NavFeature& f : nav.Input().mFeatures)
            {
                const NavNode& at = nav.Nodes()[cur];
                roll.mWalkIn = roll.mWalkIn || (f.mKind == FeatureKind::eHoist && at.mX >= f.mX1 && at.mX <= f.mX2 && at.mY + 16 >= f.mY1 && at.mY + 16 <= f.mY2);
            }
            return TaskStep::Do(roll);
        }

        case EdgeKind::eRunJump:
            if (abe.mFacingLeft != first.mLeft)
            {
                return TaskStep::Do(Prim::Face(first.mLeft));
            }
            mLastEdge = path[0];
            return TaskStep::Do(Prim::RunJump(first.mX, first.mLeft));

        case EdgeKind::eLift:
            mLastEdge = path[0];
            return TaskStep::Do(Prim::Lift(nav.Nodes()[first.mTo].mY));

        case EdgeKind::eWalk:
        {
            // As far as the walk goes the same way
            std::size_t last = 0;
            while (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eWalk && nav.Edges()[path[last + 1]].mLeft == first.mLeft)
            {
                last++;
            }
            if (led)
            {
                last = std::min<std::size_t>(last, 3);
            }
            if (sweepStop <= last)
            {
                last = sweepStop - 1;
            }
            if (sweepRoll > 0)
            {
                return TaskStep::Do(Prim::MoveTo(nav.Nodes()[nav.Edges()[path[std::min(last, sweepRoll - 1)]].mTo].mX, Gait::eRoll));
            }
            // Sligs on this floor: quietly near a sleeping one, and past one that is awake only where he can't see
            bool sneak = false;
            bool outrun = false;
            const NavNode& here = nav.Nodes()[cur];
            const s32 cell = NavGraph::GridStep(here.mHalf);
            for (const SligAt& slig : bot.GetWorld().Threats())
            {
                // A sleeping one hears steps on any floor of its screen; one that is awake sees along its own
                if (slig.mPossessed || (!slig.mAsleep && (slig.mHalf != here.mHalf || std::abs(slig.mY - here.mY) > 45)))
                {
                    continue;
                }
                const s32 sligCam = nav.CameraOf(slig.mX, slig.mY - 5);
                // One that is awake and walking is on Abe's screen before long: a screen and a half of its floor
                // either way is as good as seen
                const s32 reach = slig.mAsleep || !slig.mSeen ? 0 : 600;
                const auto close = [&](s32 x)
                {
                    if (slig.mAsleep && slig.mWake > 0)
                    {
                        // (what wakes it is a noise this near; two cells more for where a step ends)
                        return nav.CameraOf(x, here.mY - 5) == sligCam && std::abs(x - slig.mX) <= slig.mWake + 2 * cell;
                    }
                    return nav.CameraOf(x, here.mY - 5) == sligCam || std::abs(x - slig.mX) < reach;
                };
                const auto open = [&](const NavNode& n)
                {
                    return !n.mHidden && close(n.mX);
                };
                bool near = close(here.mX);
                for (std::size_t i = 0; i <= last; i++)
                {
                    near = near || close(nav.Nodes()[nav.Edges()[path[i]].mTo].mX);
                }
                if (!near)
                {
                    continue;
                }
                // (not past it from its front to behind its back: the wait before it shoots is for what it comes on
                // as it walks. A footstep behind it alerts it, and alerted it turns and shoots at once. The Mines'
                // path 1 at 3187,1400: run past, shot 13 frames after it began to turn, a cell short of the shadow.)
                bool pastIt = false;
                if (!slig.mAsleep && slig.mFacingLeft == (here.mX < slig.mX))
                {
                    for (std::size_t i = 0; i <= last; i++)
                    {
                        const s32 x = nav.Nodes()[nav.Edges()[path[i]].mTo].mX;
                        pastIt = pastIt || ((x < slig.mX) != (here.mX < slig.mX) && !nav.Nodes()[nav.Edges()[path[i]].mTo].mHidden);
                    }
                }
                if (!slig.mAsleep && slig.mSightDelay >= World::kOutrunDelay && !pastIt)
                {
                    // One that waits before it shoots at what it sees is run from: measured, he is off its screen
                    // and up the next hoist before it has caught up, and it shoots at nothing on the way
                    outrun = true;
                    continue;
                }
                sneak = true;
                if (slig.mAsleep)
                {
                    // Stop before its screen, so whatever has to be put right first (a laughing Mudokon in tow) can be
                    std::size_t firstIn = 0;
                    while (firstIn <= last && !close(nav.Nodes()[nav.Edges()[path[firstIn]].mTo].mX))
                    {
                        firstIn++;
                    }
                    // (a few cells before it, to have room for that)
                    if (firstIn > 3 && firstIn <= last && !close(here.mX))
                    {
                        last = firstIn - 4;
                    }
                    continue;
                }
                // The cells ahead up to the first he could see Abe in
                std::size_t firstOpen = 0;
                while (firstOpen <= last && !open(nav.Nodes()[nav.Edges()[path[firstOpen]].mTo]))
                {
                    firstOpen++;
                }
                if (firstOpen > last)
                {
                    continue;
                }
                if (firstOpen > 0)
                {
                    last = firstOpen - 1;
                    continue;
                }
                // Out in the open next: only behind his back, as far as the next cover
                std::size_t lastOpen = 0;
                while (lastOpen + 1 <= last && open(nav.Nodes()[nav.Edges()[path[lastOpen + 1]].mTo]))
                {
                    lastOpen++;
                }
                bool behind = true;
                for (std::size_t i = 0; i <= lastOpen; i++)
                {
                    const s32 x = nav.Nodes()[nav.Edges()[path[i]].mTo].mX;
                    behind = behind && (slig.mFacingLeft ? x > slig.mX + cell / 2 : x < slig.mX - cell / 2);
                }
                const bool safeHere = !open(here);
                // (one not about yet can't be watched: nothing to wait for)
                if (!behind && safeHere && slig.mSeen && mHiding < 1500)
                {
                    mHiding += 6;
                    mSteps--;
                    return TaskStep::Do(Prim::Wait(6));
                }
                // Caught in the open with it coming: back into a shadow on the side away from it, if one is near
                if (!behind && !safeHere && slig.mSeen && mRetreats < 6)
                {
                    const s32 away = slig.mX < here.mX ? 1 : -1;
                    // (the second cell of it, so whoever follows a cell behind is in it too)
                    bool edge = false;
                    for (s32 k = 1; k <= 8; k++)
                    {
                        const s32 n = nav.NodeAt(here.mX + away * k * cell, here.mY, here.mHalf, 10);
                        if (n < 0 || nav.Nodes()[n].mLine != here.mLine)
                        {
                            break;
                        }
                        if (nav.Nodes()[n].mHidden && edge)
                        {
                            mRetreats++;
                            mSteps--;
                            return TaskStep::Do(Prim::MoveTo(nav.Nodes()[n].mX, Gait::eWalk));
                        }
                        edge = nav.Nodes()[n].mHidden;
                    }
                }
                last = std::min(last, lastOpen + 1);
            }
            mHiding = 0;
            // No further than the cell before a Slurg
            for (std::size_t i = 1; i <= last && mSlurgWaits < 80 && !bot.GetWorld().mFleechesAsleep.empty() && !mFleechAwakeHere && !bot.GetWorld().mInvisible; i++)
            {
                const NavNode& cellAt = nav.Nodes()[nav.Edges()[path[i]].mTo];
                if (bot.GetWorld().SlurgBy(cellAt.mX, cellAt.mY, cellAt.mHalf))
                {
                    last = i - 1;
                    break;
                }
            }
            const NavNode& to = nav.Nodes()[nav.Edges()[path[last]].mTo];
            Gait gait = bot.GaitFor(static_cast<s32>(last) + 1);
            if (gait == Gait::eRun && mTopGait != Gait::eRun)
            {
                gait = mTopGait;
            }
            // A sleeping Fleech within earshot of where he is or is going (150, and a step's length more): quietly
            for (const RockAt& fleech : bot.GetWorld().mFleechesAsleep)
            {
                const s32 reach = (150 + 40) / (here.mHalf ? 2 : 1);
                bool hears = std::abs(fleech.mX - here.mX) <= reach;
                for (std::size_t i = 0; i <= last; i++)
                {
                    hears = hears || std::abs(fleech.mX - nav.Nodes()[nav.Edges()[path[i]].mTo].mX) <= reach;
                }
                sneak = sneak || (hears && fleech.mHalf == here.mHalf && nav.CameraOf(fleech.mX, fleech.mY - 5) == nav.CameraOf(here.mX, here.mY - 5));
            }
            // A flying Slig about and no shadow to stand in (the reflex takes him to one if there is): at a run
            bool flier = false;
            for (const FlierAt& f : bot.GetWorld().mFliers)
            {
                flier = flier || (f.mHalf == here.mHalf && std::abs(f.mY - here.mY) < 220 && std::abs(f.mX - here.mX) < 1000);
            }
            // A Fleech awake on his screen: it crawls faster than he walks and slower than he runs
            bool fleech = false;
            for (const RockAt& f : bot.GetWorld().mFleechesAwake)
            {
                fleech = fleech || (f.mHalf == here.mHalf && nav.CameraOf(f.mX, f.mY - 5) == nav.CameraOf(here.mX, here.mY - 5));
            }
            // (invisible, nothing hears or sees him and the clock is running)
            if (bot.GetWorld().mInvisible)
            {
                sneak = false;
                fleech = true;
            }
            if (mFleechAwakeHere)
            {
                sneak = false;
                fleech = true;
            }
            // A Scrab loose on his floor: there is only running
            bool chased = false;
            for (const ScrabAt& scrab : bot.GetWorld().mScrabs)
            {
                // (or one in the air over his floor, on its way down to it: the Mudanchee trial's first, let in by
                // the plate that sends him there)
                const bool dropsIn = scrab.mY < here.mY - 30 && scrab.mY > here.mY - 260 && nav.NodeAt(scrab.mX, scrab.mY, scrab.mHalf, 30) < 0 &&
                                     nav.NodeAt(scrab.mX, here.mY, here.mHalf, 30) >= 0;
                if (!scrab.mPossessed && scrab.mHalf == here.mHalf && (std::abs(scrab.mY - here.mY) <= 30 || dropsIn) && std::abs(scrab.mX - here.mX) < 700)
                {
                    // (one shut in behind a gate is no reason to wake the Fleeches he is sneaking by: the
                    // Mudanchee vault's path 4, where the run to the lock woke the four asleep round it)
                    // (nor one behind a wall)
                    if (Domain::GateBetween(nav, bot.GetWorld(), scrab.mX, here.mX, here.mY, here.mHalf) || Domain::WallBetween(nav, scrab.mX, here.mX, here.mY, here.mHalf))
                    {
                        continue;
                    }
                    sneak = false;
                    fleech = true;
                    chased = true;
                }
            }
            // A Slog up on his floor, or about to be, is the same
            for (const SlogAt& slog : bot.GetWorld().mSlogs)
            {
                const s32 slogNode = nav.NodeAt(slog.mX, slog.mY, here.mHalf, 30);
                if ((!slog.mAsleep || slog.mWaking) && std::abs(slog.mY - here.mY) <= 30 && std::abs(slog.mX - here.mX) < 700 && slogNode >= 0 && nav.Nodes()[slogNode].mLine == here.mLine &&
                    !Domain::GateBetween(nav, bot.GetWorld(), slog.mX, here.mX, here.mY, here.mHalf) && !Domain::WallBetween(nav, slog.mX, here.mX, here.mY, here.mHalf))
                {
                    sneak = false;
                    fleech = true;
                    chased = true;
                }
            }
            // And no walking the last cells to stop on the right one (a run is left four cells early for that,
            // twenty frames a Scrab does not give): from the run into a roll, which stops where it is told
            if (chased)
            {
                const s32 way = first.mLeft ? -1 : 1;
                // Into a well at the end of the run without stopping (the walkthrough of the Mudanchee vault's path
                // 5: "jumping into the well without stopping to position yourself, you have to do it on the fly")
                if (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eWell && nav.Edges()[path[last + 1]].mWell >= 0 && (to.mX - here.mX) * way >= 3 * cell)
                {
                    const NavFeature& well = nav.Input().mFeatures[nav.Edges()[path[last + 1]].mWell];
                    mLastEdge = path[last + 1];
                    return TaskStep::Do(Prim::RunWell(way > 0 ? well.mX1 : well.mX2, way < 0));
                }
                // On under something low: from the run into the roll and through it (the Mudanchee trial's first
                // screen, where the Scrab that drops in behind him is stopped by the block he rolls under)
                std::size_t lowEnd = last;
                while (lowEnd + 1 < path.size() && nav.Edges()[path[lowEnd + 1]].mKind == EdgeKind::eRoll && nav.Edges()[path[lowEnd + 1]].mLeft == first.mLeft)
                {
                    lowEnd++;
                }
                if (lowEnd > last && (to.mX - here.mX) * way >= 3 * cell && nav.Condition(nav.Edges()[path[lowEnd]], nullptr, nullptr) == false)
                {
                    return TaskStep::Do(Prim::RunRoll(to.mX - way * cell, nav.Nodes()[nav.Edges()[path[lowEnd]].mTo].mX));
                }
                if ((to.mX - here.mX) * way >= 6 * cell)
                {
                    return TaskStep::Do(Prim::RunRoll(to.mX - way * 3 * cell, to.mX));
                }
            }
            if (outrun || mHurry || fleech || (flier && !sneak))
            {
                gait = Gait::eRun;
                // And if the way goes on under something low, from the run straight into the roll: stopping to
                // crouch there is where he was shot (the Mines' path 2, at 3287,1240)
                std::size_t rollEnd = last;
                while (rollEnd + 1 < path.size() && nav.Edges()[path[rollEnd + 1]].mKind == EdgeKind::eRoll && nav.Edges()[path[rollEnd + 1]].mLeft == first.mLeft)
                {
                    rollEnd++;
                }
                const s32 dir = first.mLeft ? -1 : 1;
                if (rollEnd > last && (to.mX - here.mX) * dir >= 5 * cell && nav.Condition(nav.Edges()[path[rollEnd]], nullptr, nullptr) == false)
                {
                    return TaskStep::Do(Prim::RunRoll(to.mX - dir * cell, nav.Nodes()[nav.Edges()[path[rollEnd]].mTo].mX));
                }
                // Too short a run for that, with Fleeches led a way that is timed against them: crouched at once
                // and rolled the whole of it (a roll is as quick as a run; walked and then crouched, the 100 px
                // over the Mudanchee vault's path 2 lock took 72 frames)
                if (rollEnd > last && fleech && bot.KeepsTo() && nav.Condition(nav.Edges()[path[rollEnd]], nullptr, nullptr) == false)
                {
                    // (with three cells to run in, from the run into the roll all the same: no stop to crouch)
                    if ((to.mX - here.mX) * dir >= 3 * cell)
                    {
                        return TaskStep::Do(Prim::RunRoll(to.mX - dir * cell, nav.Nodes()[nav.Edges()[path[rollEnd]].mTo].mX));
                    }
                    // (not from over a hoist: down there is the way down it. A cell on first.)
                    bool overHoist = false;
                    for (s32 i = here.mFirstEdge; i < here.mFirstEdge + here.mEdgeCount; i++)
                    {
                        overHoist = overHoist || nav.Edges()[i].mKind == EdgeKind::eHoistDown;
                    }
                    if (overHoist)
                    {
                        return TaskStep::Do(Prim::MoveTo(nav.Nodes()[first.mTo].mX, Gait::eWalk));
                    }
                    return TaskStep::Do(Prim::MoveTo(nav.Nodes()[nav.Edges()[path[rollEnd]].mTo].mX, Gait::eRoll));
                }
            }
            else if (sneak)
            {
                gait = Gait::eSneak;
            }
            else if (sweepWalk)
            {
                // (the pace the timing was worked out for)
                gait = Gait::eWalk;
            }
            return TaskStep::Do(Prim::MoveTo(to.mX, gait));
        }

        case EdgeKind::eFall:
        {
            // (under something low: down first, and it is rolled off)
            if (first.mCrouched && abe.mMove == Move::eIdle)
            {
                if (abe.mFacingLeft != first.mLeft)
                {
                    return TaskStep::Do(Prim::Face(first.mLeft));
                }
                mSteps--;
                return TaskStep::Do(Prim::Press(Btn::eDown, 12));
            }
            mLastEdge = path[0];
            Prim fall = Prim::WalkOff(first.mLeft);
            fall.mAfter = LandingBits(bot, nav, path);
            return TaskStep::Do(fall);
        }

        case EdgeKind::eHop:
            if (abe.mFacingLeft != first.mLeft)
            {
                return TaskStep::Do(Prim::Face(first.mLeft));
            }
            mLastEdge = path[0];
            return TaskStep::Do(Prim::Press(Btn::eHop));

        case EdgeKind::eHoistUp:
        case EdgeKind::eDoor:
        case EdgeKind::eWell:
        {
            // A door that a slapped lock opens, the ghost not at its tomb yet: here is where to wait for it
            s32 sw = 0;
            s32 value = 0;
            if (nav.Condition(first, &sw, &value) && bot.GetWorld().Switch(sw, true) != value && mGhostWaits < 150)
            {
                mGhostWaits++;
                mSteps--;
                return TaskStep::Do(Prim::Wait(6));
            }
            mLastEdge = path[0];
            Prim press = Prim::Press(Btn::eUp);
            if (first.mKind == EdgeKind::eWell)
            {
                press.mAfter = LandingBits(bot, nav, path);
            }
            return TaskStep::Do(press);
        }

        case EdgeKind::eHoistDown:
            mLastEdge = path[0];
            return TaskStep::Do(Prim::Press(Btn::eDown));

        case EdgeKind::ePortal:
        {
            // Chanted open from at least three cells off (nearer, its doves are scared away and it is gone until
            // the screen is come back to), then hopped into from two cells off: it takes the hop itself
            const NavFeature& portal = nav.Input().mFeatures[first.mFeature];
            const s32 px = (portal.mX1 + portal.mX2) / 2;
            const s32 cell = NavGraph::GridStep(nav.Nodes()[cur].mHalf);
            const s32 side = portal.mDir == 0 ? -1 : 1;
            bool open = false;
            for (const RockAt& at : bot.GetWorld().mPortalsOpen)
            {
                open = open || (std::abs(at.mX - px) <= 40 && std::abs(at.mY - portal.mY2) <= 120);
            }
            if (!open)
            {
                if (std::abs(abe.mX - px) < 3 * cell + cell / 2)
                {
                    mSteps--;
                    return TaskStep::Do(Prim::MoveTo(px + side * 4 * cell, Gait::eWalk));
                }
                if (abe.mFacingLeft != first.mLeft)
                {
                    return TaskStep::Do(Prim::Face(first.mLeft));
                }
                if (++mPortalChants > 40)
                {
                    return TaskStep::Failed("the bird portal did not open");
                }
                mSteps--;
                return TaskStep::Do(Prim::Wait(20, Btn::eChant));
            }
            if (std::abs(abe.mX - px) > 2 * cell + cell / 2)
            {
                mSteps--;
                return TaskStep::Do(Prim::MoveTo(px + side * 2 * cell, Gait::eWalk));
            }
            if (abe.mFacingLeft != first.mLeft)
            {
                return TaskStep::Do(Prim::Face(first.mLeft));
            }
            if (!abe.Settled())
            {
                mSteps--;
                return TaskStep::Do(Prim::Wait(4));
            }
            mPortalChants = 0;
            mLastEdge = path[0];
            return TaskStep::Do(Prim::Press(Btn::eHop));
        }

        case EdgeKind::eTeleport:
        {
            // Its lever is a cell to one side of where he stands in it
            const NavFeature& tele = nav.Input().mFeatures[first.mFeature];
            const s32 cell = NavGraph::GridStep(nav.Nodes()[cur].mHalf);
            for (const Entity& e : bot.GetWorld().mEntities)
            {
                if (e.mKind == EntityKind::eLever && e.mSwitch == tele.mSwitch && std::abs(e.CentreX() - abe.mX) <= 2 * cell)
                {
                    const bool left = e.CentreX() < abe.mX;
                    if (abe.mFacingLeft != left)
                    {
                        return TaskStep::Do(Prim::Face(left));
                    }
                    mLastEdge = path[0];
                    return TaskStep::Do(Prim::Press(Btn::eAction));
                }
            }
            // Or it goes off when its plate is stood on (the plate's switch the teleporter's own, or one of those
            // its switch is made of)
            std::set<s32> sets{tele.mSwitch};
            for (const Entity& logic : bot.GetWorld().mEntities)
            {
                if (logic.mKind == EntityKind::eSwitchLogic && logic.mSwitch == tele.mSwitch && tele.mSwitch > 1)
                {
                    sets.insert(std::begin(logic.mInputs), std::end(logic.mInputs));
                }
            }
            sets.erase(0);
            sets.erase(1);
            for (const Entity& e : bot.GetWorld().mEntities)
            {
                if (e.mKind == EntityKind::eFootSwitch && sets.count(e.mSwitch) != 0 && e.mHalf == tele.mHalf && tele.Contains(e.CentreX(), std::min(e.mY2 - 2, tele.mY2)) && e.mY2 <= tele.mY2 + 12)
                {
                    if (++mPlateSteps > 40)
                    {
                        return TaskStep::Failed("the teleporter's plate did nothing");
                    }
                    // (not an edge to judge by where he is a step later: it takes its time, and the game stands
                    // still while the screen changes)
                    if (std::abs(abe.mX - e.CentreX()) <= 3)
                    {
                        return TaskStep::Do(Prim::Wait(5));
                    }
                    return TaskStep::Do(Prim::MoveTo(e.CentreX(), Gait::eWalk));
                }
            }
            return TaskStep::Failed("no lever at the teleporter");
        }
    }
    return TaskStep::Failed("unknown edge");
}

const Entity* GoToTask::StopFor(Bot& bot, const Entity& drill, s32 from)
{
    // One that runs while its switch is as it started, and stays put once it isn't
    if (drill.mInputs[0] != 1 || drill.mInputs[2] != 0)
    {
        return nullptr;
    }
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const s32 want = drill.mInputs[1] ? 1 : 0;
    const NavGraph::EdgeCost all = bot.EdgeCostFn();
    for (const Entity& e : world.mEntities)
    {
        if ((e.mKind != EntityKind::eLever && e.mKind != EntityKind::ePullRing) || e.mSwitch != drill.mSwitch || (e.mOp != 2 && e.mOp != (want ? 0 : 1)))
        {
            continue;
        }
        const s32 node = Domain::UseNode(nav, e, nullptr);
        std::vector<s32> path;
        if (node >= 0 && nav.FindPath(from, node, [&](const NavEdge& edge) -> s32
                                      { return SweepOver(nav, world, edge.mTo) ? -1 : all(edge); }, path))
        {
            return &e;
        }
    }
    return nullptr;
}

const Entity* GoToTask::SweepOver(const NavGraph& nav, const World& world, s32 node)
{
    if (node < 0 || nav.Nodes()[node].mSweep < 0)
    {
        return nullptr;
    }
    const NavNode& n = nav.Nodes()[node];
    return world.SweepOver(n.mX, n.mY, n.mHalf, n.mLow);
}

const Entity* GoToTask::SweepHigh(const NavGraph& nav, const World& world, s32 node)
{
    if (node < 0 || nav.Nodes()[node].mSweepHigh < 0)
    {
        return nullptr;
    }
    const Entity* drill = world.Find(nav.Input().mFeatures[nav.Nodes()[node].mSweepHigh].mId);
    return drill && drill->mKind == EntityKind::eDrill && world.SweepLive(*drill) ? drill : nullptr;
}

bool GoToTask::SweepsClear(const NavGraph& nav, const World& world, const std::vector<s32>& path, std::size_t count, s32 late, bool rolled)
{
    // Where Abe is frame by frame: along each edge at an even pace for the frames it takes (measured, see
    // BOT_DESIGN.md), give or take kSlack pixels
    const s32 kSlack = 9;
    s32 start = late > 0 ? late : 0;
    EdgeKind before = EdgeKind::eDoor;
    for (std::size_t i = 0; i <= count && i <= path.size(); i++)
    {
        // (and a moment in the last cell, where what comes next starts)
        const bool rest = i == count || i == path.size();
        if (rest && i == 0)
        {
            break;
        }
        const NavEdge& e = nav.Edges()[path[rest ? i - 1 : i]];
        const NavNode& a = nav.Nodes()[rest ? e.mTo : e.mFrom];
        const NavNode& b = nav.Nodes()[e.mTo];
        const EdgeKind kind = rolled && e.mKind == EdgeKind::eWalk ? EdgeKind::eRoll : e.mKind;
        s32 frames = 12;
        if (!rest)
        {
            switch (kind)
            {
                case EdgeKind::eWalk:
                    frames = before == EdgeKind::eWalk ? 9 : 17;
                    break;
                case EdgeKind::eRoll:
                    // (6.25 pixels a frame)
                    frames = before == EdgeKind::eRoll ? 4 : 6;
                    break;
                case EdgeKind::eHoistUp:
                case EdgeKind::eHoistDown:
                    // (up from a crouch first; then off the floor, and hanging from the ledge)
                    frames = 54 + (before == EdgeKind::eRoll ? 8 : 0);
                    break;
                default:
                    frames = e.mCost;
                    break;
            }
            before = kind;
        }
        const bool climbs = !rest && (e.mKind == EdgeKind::eHoistUp || e.mKind == EdgeKind::eHoistDown);
        // (rolled through, or crouched where there is no standing, he is 34 high)
        const bool down = rest ? (b.mLow || rolled) : kind == EdgeKind::eRoll;
        const s32 tall = (a.mHalf ? 30 : 60) * (down ? 34 : 60) / 60;
        for (s32 f = 0; f < frames; f++)
        {
            const s32 t = start + f + (late < 0 ? late : 0);
            const s32 x = a.mX + (b.mX - a.mX) * f / frames;
            // (a hoist: on the lower floor until he jumps, then no lower than he hangs)
            // (measured in the Mines' path 4 at 2062,1500, a drill along the floor 15 to 35 above it: there 3
            // frames before the jump starts, and his feet are 23 up 14 frames into it and out of its reach 17 in)
            const s32 onFloor = e.mKind == EdgeKind::eHoistUp ? frames - 34 : 40;
            const bool hangs = climbs && (e.mKind == EdgeKind::eHoistUp ? f >= onFloor : f < frames - onFloor);
            const s32 y = climbs ? (hangs ? std::min(a.mY, b.mY) + tall + 4 : std::max(a.mY, b.mY)) : a.mY + (b.mY - a.mY) * f / frames;
            for (const SweepAt& sweep : world.mSweeps)
            {
                s32 x1 = 0;
                s32 y1 = 0;
                s32 x2 = 0;
                s32 y2 = 0;
                if (sweep.mHalf == a.mHalf && sweep.At(t < 0 ? 0 : t, x1, y1, x2, y2) && x + 3 + kSlack >= x1 && x - 3 - kSlack <= x2 && y >= y1 && y - tall <= y2)
                {
                    return false;
                }
            }
        }
        start += frames;
    }
    return true;
}

// Runs a GoTo inside another task. True when it is over (arrived, or failed with step set).
static bool RunGo(std::unique_ptr<GoToTask>& go, Bot& bot, const std::string& lastFailed, TaskStep& step)
{
    if (!go)
    {
        return true;
    }
    step = go->Next(bot, lastFailed);
    if (step.mKind == TaskStep::Kind::ePrim)
    {
        return false;
    }
    go.reset();
    return true;
}

static std::unique_ptr<GoToTask> GoToNode(s32 node)
{
    return std::make_unique<GoToTask>("GoTo node", [node](s32 n)
                                      { return n == node; });
}

DisarmTask::~DisarmTask() = default;
GetRocksTask::~GetRocksTask() = default;
BlowMineTask::~BlowMineTask() = default;

RideCarTask::~RideCarTask() = default;

TaskStep RideCarTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const Entity* car = world.Find(mCar);
    if (!car || mDest < 0 || mDest >= static_cast<s32>(nav.Nodes().size()))
    {
        return TaskStep::Failed("no such entity");
    }
    if (++mSteps > 900)
    {
        return TaskStep::Failed("the car didn't get there");
    }
    const NavNode& dest = nav.Nodes()[mDest];
    const s32 cell = NavGraph::GridStep(car->mHalf);
    if (car->mState == 0)
    {
        if (mExits > 0)
        {
            return TaskStep::Done();
        }
        // To it and in: up, standing in its cell
        const s32 carX = car->mLive ? car->mLiveX : car->CentreX();
        const s32 carY = car->mLive ? car->mLiveY : car->mY2;
        const s32 at = nav.NodeAt(carX, carY, car->mHalf, 30);
        if (at < 0)
        {
            return TaskStep::Failed("the car isn't on a floor");
        }
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        if (bot.AbeNode() != at)
        {
            mGo = GoToNode(at);
            return Next(bot, "");
        }
        if (++mEnters > 6)
        {
            return TaskStep::Failed("couldn't get into the car");
        }
        bot.Log("getting into the mine car at " + std::to_string(carX) + "," + std::to_string(carY));
        return TaskStep::Do(Prim::Wait(14, Btn::eUp));
    }

    const s32 x = car->mLiveX;
    const s32 y = car->mLiveY;
    const bool moved = std::abs(x - mLastX) + std::abs(y - mLastY) > 2;
    mLastX = x;
    mLastY = y;
    if (std::abs(x - dest.mX) <= cell / 2 + 2 && std::abs(y - dest.mY) <= 12)
    {
        if (moved)
        {
            return TaskStep::Do(Prim::Wait(6));
        }
        if (++mExits > 8)
        {
            return TaskStep::Failed("couldn't get out of the car");
        }
        bot.Log("out of the mine car at " + std::to_string(x) + "," + std::to_string(y));
        return TaskStep::Do(Prim::Wait(8, Btn::eAction));
    }
    if (moved)
    {
        mStill = 0;
        mDud.clear();
        mWent = mKey;
        return TaskStep::Do(Prim::Wait(8, mKey));
    }
    if (mKey && ++mStill < 3)
    {
        return TaskStep::Do(Prim::Wait(8, mKey));
    }
    // It does nothing here: the next way to try. Towards where it is going first, and back the way it came last.
    if (mKey)
    {
        mDud.push_back(mKey);
    }
    const u32 towardsX = dest.mX >= x ? Btn::eRight : Btn::eLeft;
    const u32 awayX = dest.mX >= x ? Btn::eLeft : Btn::eRight;
    const u32 towardsY = dest.mY >= y ? Btn::eDown : Btn::eUp;
    const u32 awayY = dest.mY >= y ? Btn::eUp : Btn::eDown;
    const auto opposite = [](u32 key) -> u32
    {
        return key == Btn::eLeft ? Btn::eRight : key == Btn::eRight ? Btn::eLeft : key == Btn::eUp ? Btn::eDown : key == Btn::eDown ? Btn::eUp : 0;
    };
    const u32 order[4] = {towardsX, towardsY, awayY, awayX};
    u32 next = 0;
    for (s32 pass = 0; pass < 2 && !next; pass++)
    {
        for (u32 key : order)
        {
            if (!next && std::find(mDud.begin(), mDud.end(), key) == mDud.end() && (pass == 1 || key != opposite(mWent)))
            {
                next = key;
            }
        }
    }
    if (!next)
    {
        return TaskStep::Failed("the car goes no way from " + std::to_string(x) + "," + std::to_string(y));
    }
    mKey = next;
    mStill = 0;
    bot.Log("mine car at " + std::to_string(x) + "," + std::to_string(y) + ": trying " + (next == Btn::eLeft ? "left" : next == Btn::eRight ? "right" : next == Btn::eUp ? "up" : "down"));
    return TaskStep::Do(Prim::Wait(8, mKey));
}

TaskStep GetRocksTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const Entity* sack = world.Find(mSack);
    if (!sack)
    {
        return TaskStep::Failed("no such entity");
    }
    if (world.mRocks > 0)
    {
        return TaskStep::Done();
    }
    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }
    const s32 cur = bot.AbeNode();
    if (cur < 0)
    {
        return TaskStep::Do(Prim::Wait(4));
    }
    const s32 cell = NavGraph::GridStep(sack->mHalf);

    // What fell out, once it lies still: from the cell next to it, crouched, like anything picked up
    const RockAt* rock = nullptr;
    for (const RockAt& r : world.mRocksDown)
    {
        if (r.mHalf == sack->mHalf && std::abs(r.mX - sack->CentreX()) < 500 && std::abs(r.mY - nav.Nodes()[cur].mY) < 200)
        {
            rock = &r;
        }
    }
    if (rock)
    {
        const s32 left = nav.NodeAt(rock->mX - cell, rock->mY, rock->mHalf, 12);
        const s32 right = nav.NodeAt(rock->mX + cell, rock->mY, rock->mHalf, 12);
        if (cur != left && cur != right)
        {
            if (left < 0 && right < 0)
            {
                return TaskStep::Failed("nowhere to stand by the rocks");
            }
            mGo = std::make_unique<GoToTask>("to the rocks", [left, right](s32 n)
                                             { return n == left || n == right; });
            return Next(bot, "");
        }
        const bool faceLeft = cur == right;
        if (abe.mFacingLeft != faceLeft)
        {
            return TaskStep::Do(Prim::Face(faceLeft));
        }
        if (abe.mMove != Move::eCrouch)
        {
            return TaskStep::Do(Prim::Press(Btn::eDown));
        }
        if (++mPicks > 6)
        {
            return TaskStep::Failed("the rocks wouldn't be picked up");
        }
        return TaskStep::Do(Prim::Press(Btn::eAction, 20));
    }

    const s32 under = Domain::UseNode(nav, *sack, nullptr);
    if (under < 0)
    {
        return TaskStep::Failed("no floor under the sack");
    }
    // They take a while to fall, roll and come to rest (and after a jump through a sack over a gap Abe is no
    // longer where he jumped from)
    if (mJumps > 0 && mWaits < 30)
    {
        mWaits++;
        return TaskStep::Do(Prim::Wait(10));
    }
    if (cur != under)
    {
        mGo = GoToNode(under);
        return Next(bot, "");
    }
    if (mJumps >= 3)
    {
        return TaskStep::Failed("nothing came out of the sack");
    }
    mJumps++;
    mWaits = 0;
    s32 from = -1;
    s32 takeoffX = 0;
    bool left = false;
    if (std::abs(nav.Nodes()[under].mX - sack->CentreX()) > cell && Domain::SackJump(nav, *sack, &from, &takeoffX, &left))
    {
        // It hangs over a gap: through it with a running jump off the ledge, and down to where the rocks fall
        if (abe.mFacingLeft != left)
        {
            mJumps--;
            return TaskStep::Do(Prim::Face(left));
        }
        bot.Log("running and jumping through the rock sack at " + std::to_string(sack->CentreX()));
        return TaskStep::Do(Prim::RunJump(takeoffX, left));
    }
    bot.Log("jumping into the rock sack at " + std::to_string(sack->CentreX()));
    return TaskStep::Do(Prim::Press(Btn::eUp, 30));
}

u32 BlowMineTask::AimBits(ThrowSpot::Kind kind, bool facingLeft)
{
    switch (kind)
    {
        case ThrowSpot::eUp: return Btn::eUp;
        case ThrowSpot::eLob: return facingLeft ? Btn::eRight : Btn::eLeft;
        case ThrowSpot::eForward: return facingLeft ? Btn::eLeft : Btn::eRight;
        case ThrowSpot::eDown: return Btn::eDown;
        case ThrowSpot::eCrouched: return facingLeft ? Btn::eLeft : Btn::eRight;
    }
    return 0;
}

s32 BlowMineTask::LiveMines(const World& world)
{
    s32 count = 0;
    for (const Entity& e : world.mEntities)
    {
        count += e.mKind == EntityKind::eMine && !e.mGone ? 1 : 0;
    }
    return count;
}

TaskStep BlowMineTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const Entity* mine = world.Find(mMine);
    if (!mine)
    {
        return TaskStep::Failed("no such entity");
    }
    if (mine->mGone)
    {
        bot.ThrowHit();
        // Let the blast die down before walking into it
        if (mSettle++ < 4)
        {
            return TaskStep::Do(Prim::Wait(10));
        }
        return TaskStep::Done();
    }
    if (mPhase == 2)
    {
        // It bounces a few times before it is spent
        mWait += 5;
        if (mWait < 100)
        {
            return TaskStep::Do(Prim::Wait(5));
        }
        mPhase = 0;
        if (LiveMines(world) < mLiveBefore)
        {
            // Not the one aimed at, but one less all the same: that is kept. It was in the rock's way, so the
            // same throw may well do for this one now.
            bot.Log("the rock set off another mine on its way to the one at " + std::to_string(mine->CentreX()));
            bot.ThrowOver();
            return TaskStep::Done();
        }
        bot.Log("the rock missed the mine at " + std::to_string(mine->CentreX()));
        bot.ThrowMissed();
        bot.WantMissRewind();
        return TaskStep::Do(Prim::Wait(1));
    }
    if (world.mRocks <= 0 && mPhase == 0)
    {
        return TaskStep::Failed("out of rocks");
    }
    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        mNoWay.push_back(mGoNode);
    }
    if (abe.mMove == Move::eHang && mPhase == 0)
    {
        // Hanging from a ledge: crouching at the top of a hoist is climbing down it, so no crouched throw
        // from this cell. Up again first.
        if (bot.AbeNode() >= 0 && std::find(mNoWay.begin(), mNoWay.end(), bot.AbeNode()) == mNoWay.end())
        {
            mNoWay.push_back(bot.AbeNode());
        }
        if (++mLost > 20)
        {
            return TaskStep::Failed("left hanging");
        }
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }

    // The place to throw from: one the rock is sure to hit from first
    const std::vector<ThrowSpot> spots = Domain::ThrowSpots(nav, mine->CentreX(), mine->mY2, mine->mHalf, &bot.GetWorld());
    const ThrowSpot* spot = nullptr;
    // From where he can get back from before anywhere else: a drop to a lower ledge is one way, and there may be
    // nothing to throw from down there
    const std::vector<s32> regions = nav.Regions();
    const s32 here = mFrom >= 0 && mFrom < static_cast<s32>(regions.size()) ? mFrom : bot.AbeNode();
    // (and past a running drill only when there is nowhere else)
    // Why there was nowhere, for the log: spots thrown from already, not got to, with a mine too near, and
    // those and the ones with a mine in the way
    s32 why[4] = {0, 0, 0, 0};
    for (s32 pass = 0; pass < 8 && !spot; pass++)
    {
        for (const ThrowSpot& s : spots)
        {
            if (pass == 7)
            {
                why[0] += bot.ThrowIsBad(mMine, s) ? 1 : 0;
                why[1] += std::find(mNoWay.begin(), mNoWay.end(), s.mNode) != mNoWay.end() ? 1 : 0;
            }
            if (bot.ThrowIsBad(mMine, s) || std::find(mNoWay.begin(), mNoWay.end(), s.mNode) != mNoWay.end() || (pass % 2 == 0 && !s.mSure) || (pass < 4 && here >= 0 && regions[s.mNode] != regions[here]))
            {
                continue;
            }
            const Entity* drill = s.mDrill ? world.Find(s.mDrill) : nullptr;
            if ((pass / 2) % 2 == 0 && drill && world.SweepLive(*drill))
            {
                continue;
            }
            // (a mine in between would take the rock)
            bool blocked = false;
            why[2] += pass == 7 && blocked ? 1 : 0;
            for (s32 need : s.mNeeds)
            {
                blocked = blocked || world.Switch(need) == 0;
            }
            for (const std::pair<s32, s32>& open : s.mOpen)
            {
                blocked = blocked || world.Switch(open.first) != open.second;
            }
            if (blocked)
            {
                why[3] += pass == 7 ? 1 : 0;
                continue;
            }
            spot = &s;
            break;
        }
    }
    if (!spot || mThrows >= 30)
    {
        return TaskStep::Failed("nowhere left to throw at the mine from (" + std::to_string(spots.size()) + " places: " + std::to_string(why[0]) + " missed from, " + std::to_string(why[1]) + " not got to, " + std::to_string(why[2]) + " with a mine too near, " + std::to_string(why[3] - why[2]) + " with a mine in the way, " + std::to_string(mThrows) + " throws)");
    }
    const s32 cur = bot.AbeNode();
    if (cur < 0)
    {
        // (put back in the middle of a climb: up it)
        if (++mLost > 60)
        {
            return TaskStep::Failed("off the nav graph");
        }
        return TaskStep::Do(abe.mMove == Move::eHang ? Prim::Press(Btn::eUp) : Prim::Wait(4));
    }
    mLost = 0;
    const bool crouched = spot->mKind == ThrowSpot::eCrouched;
    if (cur != spot->mNode)
    {
        mGoNode = spot->mNode;
        mGo = GoToNode(spot->mNode);
        return Next(bot, "");
    }
    const bool faceLeft = mine->CentreX() < abe.mX;
    if (mPhase == 0)
    {
        if (abe.mFacingLeft != faceLeft)
        {
            return TaskStep::Do(Prim::Face(faceLeft));
        }
        if (crouched != (abe.mMove == Move::eCrouch))
        {
            return TaskStep::Do(Prim::Press(crouched ? Btn::eDown : Btn::eUp));
        }
        if (!abe.Settled())
        {
            return TaskStep::Do(Prim::Wait(2));
        }
        // The rock comes to hand over a few frames, and goes when a direction is held as well
        mPhase = 1;
        mThrows++;
        mLiveBefore = LiveMines(world);
        bot.ThrowBegun(mMine, *spot);
        bot.Log("throwing a rock at the mine at " + std::to_string(mine->CentreX()) + "," + std::to_string(mine->mY2) + " from " + std::to_string(spot->mCells) + " cells" + (spot->mLevel < 0 ? " below" : spot->mLevel > 0 ? " above" : "") + ", throw " + std::to_string(static_cast<s32>(spot->mKind)));
        return TaskStep::Do(Prim::Wait(8, Btn::eThrow));
    }
    mPhase = 2;
    mWait = 0;
    return TaskStep::Do(Prim::Wait(6, Btn::eThrow | AimBits(spot->mKind, abe.mFacingLeft)));
}

const UxbAt* DisarmTask::Under(const World& world, s32 fromX, s32 toX, s32 y, bool half)
{
    const s32 lo = std::min(fromX, toX);
    const s32 hi = std::max(fromX, toX);
    for (const UxbAt& uxb : world.mUxbs)
    {
        if (!uxb.mOff && uxb.mHalf == half && std::abs(uxb.mY - y) <= 20 && uxb.mX > lo && uxb.mX < hi)
        {
            return &uxb;
        }
    }
    return nullptr;
}

TaskStep DisarmTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const AbeState& abe = bot.Abe();
    const UxbAt* uxb = nullptr;
    for (const UxbAt& u : bot.GetWorld().mUxbs)
    {
        if (std::abs(u.mX - mX) <= 6 && std::abs(u.mY - mY) <= 6)
        {
            uxb = &u;
        }
    }
    if (!uxb)
    {
        return TaskStep::Failed("the UXB isn't there");
    }
    if (uxb->mOff)
    {
        if (abe.mMove == Move::eCrouch)
        {
            return TaskStep::Do(Prim::Press(Btn::eUp));
        }
        return TaskStep::Done();
    }

    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }
    // A cell from it, on the side that is nearer by the ways there are
    const s32 cell = NavGraph::GridStep(mHalf);
    const s32 left = nav.NodeAt(mX - cell, mY, mHalf, 12);
    const s32 right = nav.NodeAt(mX + cell, mY, mHalf, 12);
    const s32 cur = bot.AbeNode();
    if (cur < 0)
    {
        return TaskStep::Do(Prim::Wait(4));
    }
    if (cur != left && cur != right)
    {
        if (abe.mMove == Move::eCrouch)
        {
            return TaskStep::Do(Prim::Press(Btn::eUp));
        }
        if (left < 0 && right < 0)
        {
            return TaskStep::Failed("nowhere to stand by the UXB");
        }
        mGo = std::make_unique<GoToTask>("to the UXB", [left, right](s32 n)
                                         { return n == left || n == right; });
        return Next(bot, "");
    }
    const bool faceLeft = cur == right;
    if (abe.mMove != Move::eCrouch)
    {
        if (abe.mFacingLeft != faceLeft)
        {
            return TaskStep::Do(Prim::Face(faceLeft));
        }
        return TaskStep::Do(Prim::Press(Btn::eDown));
    }
    if (abe.mFacingLeft != faceLeft)
    {
        // Crouched the wrong way round: up and turn
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }
    // The slap lands half a dozen frames after the button: on the frame its light goes green there is time
    mFrames++;
    if (mFrames > 900 || mSlaps > 3)
    {
        return TaskStep::Failed("no green light to slap it on");
    }
    const bool fresh = uxb->mGreen && !mWasGreen;
    mWasGreen = uxb->mGreen;
    if (fresh)
    {
        mSlaps++;
        return TaskStep::Do(Prim::Wait(14, Btn::eAction));
    }
    return TaskStep::Do(Prim::Wait(1));
}

TaskStep UseTask::Next(Bot& bot, const std::string& lastFailed)
{
    const Entity* entity = bot.GetWorld().Find(mEntity);
    if (!entity)
    {
        return TaskStep::Failed("no such entity");
    }
    // What it would start a drill over: a Mudokon there is drilled (the Mines' path 4, the lever at 637,1500 with
    // two blind ones on the ledge over it: they are to be led off first)
    if (bot.SavesMudokons() && bot.GetWorld().Switch(entity->mSwitch) != mWant)
    {
        for (const Entity& drill : bot.GetWorld().mEntities)
        {
            if (drill.mKind != EntityKind::eDrill || drill.mSwitch != entity->mSwitch || bot.GetWorld().SweepRuns(drill))
            {
                continue;
            }
            for (const Entity& mud : bot.GetWorld().mEntities)
            {
                const s32 x = mud.mLive ? mud.mLiveX : mud.CentreX();
                const s32 y = mud.mLive ? mud.mLiveY : mud.mY2;
                if (mud.mKind == EntityKind::eMudokon && !mud.mGone && mud.mHalf == drill.mHalf && x >= drill.mX1 - 20 && x <= drill.mX2 + 20 && y >= drill.mY1 && y - (mud.mHalf ? 30 : 60) <= drill.mY2)
                {
                    return TaskStep::Failed("it starts a drill where a Mudokon stands");
                }
            }
        }
    }
    if (mLooking)
    {
        // In a hand stone: a tap to go on to its next picture, until Abe is back
        if (bot.Abe().Settled() && !bot.Abe().mTransition)
        {
            mLooking = false;
        }
        else if (++mLookTaps > 200)
        {
            return TaskStep::Failed("stuck in the hand stone");
        }
        else
        {
            return TaskStep::Do(Prim::Wait(mLookTaps % 2 ? 2 : 6, mLookTaps % 2 ? static_cast<u32>(Btn::eConfirm) : 0u));
        }
    }
    // A lock that makes him invisible has no switch: it is done when he has the power
    // (this lock, slapped by this task: an earlier lock of the path may have given the power before)
    if (entity->mKind == EntityKind::eSlapLock && entity->mInputs[0] > 0 && entity->mSwitch <= 1 && mTries > 0 && (bot.GetWorld().mInvisCharge || bot.GetWorld().mInvisible))
    {
        return bot.Abe().Settled() ? TaskStep::Done() : TaskStep::Do(Prim::Wait(4));
    }
    // With a Fleech awake there is no time to walk round a lever: from the side he is on, if it can be
    if (!mSideChosen)
    {
        mSideChosen = true;
        if (entity->mKind == EntityKind::eLever && !bot.GetWorld().mFleechesAwake.empty())
        {
            mSide = bot.Abe().mX > entity->CentreX() ? 1 : -1;
        }
        // A lock is slapped from the side he comes to it from (the other may be behind a gate)
        if (entity->mKind == EntityKind::eSlapLock)
        {
            mSide = bot.Abe().mX > entity->CentreX() ? 1 : -1;
        }
    }
    bool faceLeft = false;
    const s32 node = Domain::UseNode(bot.Nav(), *entity, &faceLeft, mSide);
    // Under a drill that goes over his head: stood up only for as long as it is away
    const Entity* over = bot.AbeNode() == node ? GoToTask::SweepHigh(bot.Nav(), bot.GetWorld(), node) : nullptr;
    if (bot.GetWorld().Switch(entity->mSwitch) == mWant)
    {
        // A lift it sends for: until that is there
        for (const NavNode& n : bot.Nav().Nodes())
        {
            if (mWant == 1 && mArrived && n.mLiftStop >= 0 && bot.Nav().Input().mFeatures[n.mLiftStop].mSwitch == entity->mSwitch && !bot.GetWorld().LiftAtStop(n.mX, n.mY, false) && ++mLiftWaits < 600)
            {
                return TaskStep::Do(Prim::Wait(2));
            }
        }
        if (over && mArrived && ++mDucks < 400)
        {
            // (it has its stroke to finish)
            return bot.Abe().mMove == Move::eCrouch || !bot.Abe().Settled() ? TaskStep::Do(Prim::Wait(2)) : TaskStep::Do(Prim::Press(Btn::eDown));
        }
        return TaskStep::Done();
    }

    if (node < 0)
    {
        return TaskStep::Failed("nowhere to stand");
    }
    if (!mArrived)
    {
        if (!mGo)
        {
            mGo = GoToNode(node);
        }
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        mArrived = true;
    }

    const AbeState& abe = bot.Abe();
    // (carried on past a lock's cell by the run he landed into: not slapped from wherever he comes to a stop, facing
    // away from it. Back to the cell.)
    if (entity->mKind == EntityKind::eSlapLock && (mTries == 0 || bot.GetWorld().mLocksIntact.count(mEntity) != 0) && (bot.AbeNode() != node || !abe.Settled()))
    {
        if (!abe.Settled() && ++mLiftWaits < 160)
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        if (bot.AbeNode() != node)
        {
            mArrived = false;
        }
        return TaskStep::Do(Prim::Wait(1));
    }
    if (over)
    {
        const SweepAt* at = bot.GetWorld().SweepOf(over->mId);
        const NavNode& n = bot.Nav().Nodes()[node];
        const s32 tall = n.mHalf ? 30 : 60;
        // (standing up, the pull, and down again)
        const s32 need = abe.mMove == Move::eCrouch ? 36 : 28;
        if (!at || at->Hits(0, need, n.mX - 12, n.mY - tall, n.mX + 12, n.mY))
        {
            if (++mDucks > 900)
            {
                return TaskStep::Failed("the drill overhead leaves no time to stand up");
            }
            return abe.mMove == Move::eCrouch || !abe.Settled() ? TaskStep::Do(Prim::Wait(1)) : TaskStep::Do(Prim::Press(Btn::eDown));
        }
        if (abe.mMove == Move::eCrouch)
        {
            bot.Log("the drill overhead is away: up to use #" + std::to_string(mEntity));
        }
    }
    if (abe.mMove == Move::eHang || abe.mMove == Move::eCrouch)
    {
        return TaskStep::Do(Prim::Press(abe.mMove == Move::eHang ? Btn::eDown : Btn::eUp));
    }
    // (a slap that landed on something else, a Scrab that stood at the lock: the lock is as it was, and slapped again)
    const bool missed = entity->mKind == EntityKind::eSlapLock && mTries > 0 && bot.Abe().Settled() && bot.GetWorld().mLocksIntact.count(mEntity) != 0;
    if (missed && !mMissLogged)
    {
        mMissLogged = true;
        bot.Log("the slap did not land on lock #" + std::to_string(mEntity) + ": again");
    }
    if (entity->mKind == EntityKind::eSlapLock && mTries > 0 && !missed)
    {
        // Slapped (it knocks him over): its ghost flies to its tomb by itself and the switch comes on when it
        // is there. He does not stand and wait for that: in the Mudanchee vaults a slap starts a clock.
        // (a slap that dropped him through a trap door, with a way to run held as he landed: done as he is off,
        // with nothing waited for. Whoever asked for that has the next move ready.)
        if (mAfter != 0 && node >= 0 && bot.Abe().mY > bot.Nav().Nodes()[node].mY + 20)
        {
            bot.LockSlapped(mEntity);
            return TaskStep::Done();
        }
        if (!bot.Abe().Settled() && ++mLiftWaits < 80)
        {
            return TaskStep::Do(Prim::Wait(2));
        }
        if (bot.Abe().mMove == Move::eCrouch)
        {
            return TaskStep::Do(Prim::Press(Btn::eUp));
        }
        // (the power to go invisible, if it gives that, comes to him as a ring: that he does wait for, unless
        // the slap has woken Fleeches on his screen: the ring finds him as he goes)
        bool fleechesUp = false;
        for (const RockAt& f : bot.GetWorld().mFleechesAwake)
        {
            fleechesUp = fleechesUp || bot.Nav().CameraOf(f.mX, f.mY - 5) == bot.Nav().CameraOf(bot.Abe().mX, bot.Abe().mY - 5);
        }
        if (entity->mInputs[0] > 0 && !fleechesUp && !bot.GetWorld().mInvisCharge && !bot.GetWorld().mInvisible && ++mLookTaps < 150)
        {
            return TaskStep::Do(Prim::Wait(4));
        }
        bot.LockSlapped(mEntity);
        return TaskStep::Done();
    }
    if (++mTries > 6)
    {
        return TaskStep::Failed("switch " + std::to_string(entity->mSwitch) + " didn't change");
    }
    switch (entity->mKind)
    {
        case EntityKind::eSlapLock:
            if (abe.mFacingLeft != faceLeft)
            {
                mTries--;
                return TaskStep::Do(Prim::Face(faceLeft));
            }
        {
            // (not with a Scrab standing at it: the slap would land on that. Unseen he can wait by it.)
            for (const ScrabAt& scrab : bot.GetWorld().mScrabs)
            {
                const s32 scale = entity->mHalf ? 2 : 1;
                const bool between = (scrab.mX - abe.mX) * (entity->CentreX() - abe.mX) > 0 && std::abs(scrab.mX - abe.mX) <= std::abs(entity->CentreX() - abe.mX) + 40 / scale;
                if (!scrab.mPossessed && scrab.mHalf == entity->mHalf && std::abs(scrab.mY - entity->mY2) * scale <= 30 && between && ++mScrabWaits < 400)
                {
                    mTries--;
                    return TaskStep::Do(Prim::Wait(2));
                }
            }
            Prim slap = Prim::Press(Btn::eSpeak5);
            slap.mAfter = mAfter;
            return TaskStep::Do(slap);
        }
        case EntityKind::eLever:
            if (abe.mFacingLeft != faceLeft)
            {
                return TaskStep::Do(Prim::Face(faceLeft));
            }
            return TaskStep::Do(Prim::Press(Btn::eAction));
        case EntityKind::ePullRing:
            return TaskStep::Do(Prim::Press(Btn::eUp));
        case EntityKind::eHandStone:
            mLooking = true;
            return TaskStep::Do(Prim::Wait(3, Btn::eUp));
        default:
            // A wheel turns for as long as up is held, and its switch goes on once it has turned for its
            // time (measured: he is on it 12 frames after up is pressed)
            return TaskStep::Do(Prim::Wait(entity->mKind == EntityKind::eWorkWheel && entity->mInputs[0] > 0 ? std::min(45, entity->mInputs[0] + 16) : 45, Btn::eUp));
    }
}

// The cells on a Mudokon's floor from minCells to maxCells away from it, either side
static std::unique_ptr<GoToTask> GoNearMud(const NavGraph& nav, const World& world, s32 mudNode, s32 minHalfCells, s32 maxHalfCells, s32 side = 0)
{
    const s32 line = nav.Nodes()[mudNode].mLine;
    const s32 mudX = nav.Nodes()[mudNode].mX;
    const s32 half = NavGraph::GridStep(nav.Nodes()[mudNode].mHalf) / 2;
    return std::make_unique<GoToTask>("to the Mudokon", [&nav, &world, line, mudX, half, minHalfCells, maxHalfCells, side](s32 n)
                                      {
                                          const NavNode& node = nav.Nodes()[n];
                                          const s32 dx = std::abs(node.mX - mudX);
                                          if (node.mLine != line || dx < minHalfCells * half || dx > maxHalfCells * half || (node.mX - mudX) * side < 0)
                                          {
                                              return false;
                                          }
                                          // Not right beside a mine or a UXB: Abe stands there a while, turning and talking
                                          for (s32 side = -1; side <= 1; side += 2)
                                          {
                                              const s32 beside = nav.NodeAt(nav.StepX(node.mX, side, node.mHalf), node.mY, node.mHalf, 8);
                                              if (beside < 0 || !nav.Nodes()[beside].mHazard)
                                              {
                                                  continue;
                                              }
                                              // (a UXB that has been turned off is no more than floor)
                                              bool off = false;
                                              for (const UxbAt& uxb : world.mUxbs)
                                              {
                                                  off = off || (uxb.mOff && uxb.mHalf == node.mHalf && std::abs(uxb.mY - node.mY) <= 10 && std::abs(uxb.mX - nav.Nodes()[beside].mX) <= 8);
                                              }
                                              if (!off)
                                              {
                                                  return false;
                                              }
                                          }
                                          return true;
                                      });
}

// A Slig that is awake walks the Mudokon's floor, on the screen Abe is on with it, and its beat never takes it
// off that screen
static bool SligNeverLeaves(const Bot& bot, const Entity& mud)
{
    const s32 x = mud.mLive ? mud.mLiveX : mud.CentreX();
    const s32 y = mud.mLive ? mud.mLiveY : mud.mY2;
    if (std::abs(bot.Abe().mY - y) > 45 || std::abs(bot.Abe().mX - x) > 450)
    {
        return false;
    }
    for (const SligAt& slig : bot.GetWorld().mSligs)
    {
        if (!slig.mAsleep && !slig.mPossessed && slig.mHalf == mud.mHalf && std::abs(slig.mY - y) <= 45 && std::abs(slig.mX - x) < 450 && bot.BeatStaysOnScreen(slig))
        {
            return true;
        }
    }
    return false;
}

// A Slig walks the floor the Mudokon is on, and shoots at anything said in front of it (the Mudokon's answer
// too): wait, in a shadow if one is near, until it has its back to them with "need" frames to go before it turns.
// True when there is a step to do first.
static bool HoldForSlig(Bot& bot, const Entity& mud, s32 need, std::unique_ptr<GoToTask>& go, s32& held, TaskStep& step)
{
    const s32 x = mud.mLive ? mud.mLiveX : mud.CentreX();
    const s32 y = mud.mLive ? mud.mLiveY : mud.mY2;
    // (only once Abe is on its floor and near: the way there is the walk's own business)
    if (bot.QuietFrames(x, y, mud.mHalf) >= need || held > 3000 || std::abs(bot.Abe().mY - y) > 45 || std::abs(bot.Abe().mX - x) > 450)
    {
        return false;
    }
    // One whose beat never takes it off this screen: there is no quiet to wait for, and going to the Mudokon
    // behind its back does not do. It turns round at a footstep or a word behind it and, alerted, shoots at
    // once: the wait before it shoots (Slig::RespondToEnemyOrPatrol) is only for what it comes on while it
    // walks. The Mines' path 1, Mudokon #33006 and the Slig that walks 3012..3287 on one screen: twelve deaths.
    if (SligNeverLeaves(bot, mud))
    {
        step = TaskStep::Failed("a Slig that never leaves the screen walks the Mudokon's floor");
        return true;
    }
    held += 6;
    const NavGraph& nav = bot.Nav();
    const s32 abeNode = bot.AbeNode();
    // With every Slig's back to him he can stay where he is (it will be off the screen in a moment); with one
    // coming he is better off in a shadow
    bool coming = false;
    for (const SligAt& slig : bot.GetWorld().mSligs)
    {
        if (!slig.mAsleep && !slig.mPossessed && slig.mHalf == mud.mHalf && std::abs(slig.mY - y) <= 45 && std::abs(slig.mX - x) < 1500)
        {
            // (one whose ways aren't known yet may turn any moment)
            coming = coming || slig.mFacingLeft == (slig.mX > bot.Abe().mX) || !bot.KnowsBeat(slig);
        }
    }
    // On his way to the Mudokon behind a Slig's back: on he goes, to be there when it is off the screen
    if (go && !coming)
    {
        return false;
    }
    if (coming && abeNode >= 0 && !nav.Nodes()[abeNode].mHidden && std::abs(nav.Nodes()[abeNode].mY - y) <= 45)
    {
        const NavGraph* graph = &nav;
        const s32 line = nav.Nodes()[abeNode].mLine;
        // Well inside it: whoever follows him stops a cell short, and should be out of sight too
        const auto shadow = [graph, line](s32 n)
        {
            const NavNode& node = graph->Nodes()[n];
            if (!node.mHidden || node.mLine != line)
            {
                return false;
            }
            const s32 cell = NavGraph::GridStep(node.mHalf);
            const s32 a = graph->NodeAt(node.mX - cell, node.mY, node.mHalf, 10);
            const s32 b = graph->NodeAt(node.mX + cell, node.mY, node.mHalf, 10);
            // (narrow ones have no middle: a hidden cell on one side will do)
            return (a >= 0 && graph->Nodes()[a].mHidden) || (b >= 0 && graph->Nodes()[b].mHidden);
        };
        std::vector<s32> way;
        if (nav.FindPath(abeNode, shadow, bot.EdgeCostFn(), way) && way.size() <= 12)
        {
            go = std::make_unique<GoToTask>("into the shadow", shadow);
            step = go->Next(bot, "");
            if (step.mKind == TaskStep::Kind::ePrim)
            {
                return true;
            }
            go.reset();
        }
    }
    step = TaskStep::Do(Prim::Wait(6));
    return true;
}

TaskStep RecruitTask::Next(Bot& bot, const std::string& lastFailed)
{
    const Entity* mud = bot.GetWorld().Find(mMud);
    const AbeState& abe = bot.Abe();
    if (!mud)
    {
        return TaskStep::Failed("no such entity");
    }
    if (mDisarm)
    {
        const TaskStep step = mDisarm->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            bot.Log("couldn't turn the UXB off: " + step.mWhy);
        }
        mDisarm.reset();
        return Next(bot, "");
    }
    if (mud->mGone)
    {
        return TaskStep::Failed("the Mudokon is gone");
    }
    if (mud->mFollowing && (!mCalm || mud->mMood == MudMood::eNormal))
    {
        return TaskStep::Done();
    }
    if (abe.mMove == Move::eCrouch)
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }

    TaskStep step;
    // Enough quiet for what is left to say: all of it before the first word, a line and its answer after.
    // Asked before going on towards it, so a walk out of the shadow is not begun at the wrong time.
    if (HoldForSlig(bot, *mud, mSaidHello ? 45 : 130, mGo, mHeld, step))
    {
        return step;
    }
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }

    const NavGraph& nav = bot.Nav();
    const s32 cell = NavGraph::GridStep(mud->mHalf);
    const s32 mudNode = Domain::UseNode(nav, *mud, nullptr);
    if (mudNode < 0)
    {
        return TaskStep::Failed("the Mudokon isn't on a floor");
    }
    const s32 dx = std::abs((mud->mLive ? mud->mLiveX : mud->CentreX()) - abe.mX);
    const s32 abeNode = bot.AbeNode();
    const bool sameFloor = abeNode >= 0 && nav.Nodes()[abeNode].mLine == nav.Nodes()[mudNode].mLine;

    // Sorry is said with a pat on the back, from the cell next to it. Anything else from a cell or two away.
    // (and so is a slap)
    const bool close = mud->mLive && ((mud->mMood == MudMood::eUpset && mSaidHello) || (mud->mMood == MudMood::eWired && mCalm));
    const s32 minHalf = close ? 1 : 3;
    const s32 maxHalf = close ? 3 : 5;
    const bool wrongSide = (abe.mX - (mud->mLive ? mud->mLiveX : mud->CentreX())) * mSide < 0;
    if (!mRemote && (!sameFloor || dx * 2 < minHalf * cell || dx * 2 > maxHalf * cell || wrongSide))
    {
        if (++mCloseIns > 12)
        {
            return TaskStep::Failed("can't get to the Mudokon");
        }
        // UXBs right by it: it would walk onto one coming over, so they are turned off first
        for (const UxbAt& uxb : bot.GetWorld().mUxbs)
        {
            const s32 mudX = mud->mLive ? mud->mLiveX : mud->CentreX();
            if (!uxb.mOff && mDisarms < 3 && uxb.mHalf == mud->mHalf && std::abs(uxb.mY - nav.Nodes()[mudNode].mY) <= 10 && std::abs(uxb.mX - mudX) <= 3 * cell)
            {
                mDisarms++;
                mCloseIns--;
                bot.Log("a UXB at " + std::to_string(uxb.mX) + " is right by the Mudokon: turning it off");
                mDisarm = std::make_unique<DisarmTask>(uxb.mX, uxb.mY, uxb.mHalf);
                return Next(bot, "");
            }
        }
        mGo = GoNearMud(nav, bot.GetWorld(), mudNode, minHalf, maxHalf, mSide);
        return Next(bot, "");
    }
    if (!mud->mLive)
    {
        // Its camera isn't loaded yet, or it has walked off
        if (++mWaits > 30)
        {
            return TaskStep::Failed("the Mudokon isn't there");
        }
        return TaskStep::Do(Prim::Wait(10));
    }

    const bool faceLeft = mud->mLiveX < abe.mX;
    if (abe.mFacingLeft != faceLeft)
    {
        return TaskStep::Do(Prim::Face(faceLeft));
    }
    // Brain_4_ListeningToAbe: it has stood up and waits to be told what to do
    const bool listening = mud->mState / 100 == 4;
    const bool plain = mud->mMood == MudMood::eNormal || (mud->mMood == MudMood::eWired && !mCalm);
    if (plain && !mud->mFollowing)
    {
        // Hello, then nothing more until it listens (that takes it a few seconds), then follow me at once
        if (!listening && mSaidHello && mSinceLine < 220)
        {
            mSinceLine += 6;
            return TaskStep::Do(Prim::Wait(6));
        }
        if (listening && mSaidFollow && mSinceLine < 90)
        {
            mSinceLine += 6;
            return TaskStep::Do(Prim::Wait(6));
        }
        // At its work it hears a hello only between strokes (Brain_1_Chisle's first states): ducking from a
        // Slig, or getting up, it doesn't
        if (!listening && mud->mState / 100 == 1 && mud->mState % 100 > 2 && mSinceLine < 400)
        {
            mSinceLine += 6;
            return TaskStep::Do(Prim::Wait(6));
        }
        // Not a word until the Slig is off the screen with its back to them, for as long as the answer takes
        if (HoldForSlig(bot, *mud, listening ? 40 : 60, mGo, mHeld, step))
        {
            return step;
        }
        if (++mTalks > 40)
        {
            return TaskStep::Failed("the Mudokon won't follow");
        }
        bot.Log("talk to Mudokon#" + std::to_string(mMud) + " at " + std::to_string(mud->mLiveX) + (listening ? ": follow me" : ": hello") + " state " + std::to_string(mud->mState));
        mSinceLine = 0;
        mSaidHello = !listening;
        mSaidFollow = listening;
        return TaskStep::Do(Prim::Press(listening ? bot.SpeakFollow() : (mAll ? bot.SpeakAll() : bot.SpeakHello()), 6));
    }

    if (HoldForSlig(bot, *mud, mSaidHello ? 45 : 130, mGo, mHeld, step))
    {
        return step;
    }
    if (++mTalks > 40)
    {
        return TaskStep::Failed("the Mudokon won't follow");
    }
    // Every line gets a pause for the answer
    if (mTalks % 2 == 0)
    {
        return TaskStep::Do(Prim::Wait(40));
    }
    bot.Log("talk to Mudokon#" + std::to_string(mMud) + " at " + std::to_string(mud->mLiveX) + " mood " + std::to_string(static_cast<s32>(mud->mMood)) + (mud->mFollowing ? " following" : "") + " state " + std::to_string(mud->mState));
    if (!mSaidHello && !mud->mFollowing)
    {
        mSaidHello = true;
        return TaskStep::Do(Prim::Press(bot.SpeakHello(), 6));
    }
    if (mud->mMood == MudMood::eUpset)
    {
        return TaskStep::Do(Prim::Press(bot.SpeakSorry(), 6));
    }
    if (mud->mMood == MudMood::eWired && mCalm)
    {
        // A slap brings it round, and hurts: if it doesn't take, the laughing gas is still on it
        if (++mSlaps > 2)
        {
            return TaskStep::Failed("the Mudokon stays wired");
        }
        return TaskStep::Do(Prim::Press(bot.SpeakSlap(), 6));
    }
    mSaidHello = false;
    return TaskStep::Do(Prim::Press(bot.SpeakFollow(), 6));
}

const SligAt* ClearSligTask::Watcher(const World& world, s32 x, s32 y, bool half)
{
    for (const SligAt& slig : world.mSligs)
    {
        // On the same floor and within a screen: its beat takes it past
        // (one that waits before it shoots gives time for a pull: it is run from, not got rid of)
        if (!slig.mAsleep && slig.mHalf == half && std::abs(slig.mY - y) <= 45 && std::abs(slig.mX - x) < 450 && slig.mSightDelay < World::kOutrunDelay)
        {
            return &slig;
        }
    }
    return nullptr;
}

bool ClearSligTask::Target(const World& world, const SligAt& me, const Entity* mud, s32& x)
{
    for (const SligAt& other : world.mSligs)
    {
        if (!other.mPossessed && other.mHalf == me.mHalf && std::abs(other.mY - me.mY) <= 45 && std::abs(other.mX - me.mX) < 1500)
        {
            x = other.mX;
            return true;
        }
    }
    if (mud && mud->mLive && !mud->mGone && mud->mHalf == me.mHalf && std::abs(mud->mLiveY - me.mY) <= 45 && std::abs(mud->mLiveX - me.mX) < 1500)
    {
        x = mud->mLiveX;
        return true;
    }
    return false;
}

u32 ClearSligTask::AsSlig(const SligAt& me, const s32* x)
{
    if (!x)
    {
        return 0;
    }
    const s32 dx = *x - me.mX;
    const u32 towards = dx < 0 ? Btn::eLeft : Btn::eRight;
    // Close enough to hit and not so close as to walk into it; a step towards it turns the Slig round too
    if (std::abs(dx) > 200 || me.mFacingLeft != (dx < 0))
    {
        return towards;
    }
    return Btn::eThrow;
}

bool ClearSligTask::LevelShotClear(const World& world, const SligAt& me, s32 targetX, u32 toDie)
{
    // The shot hits what stands in a thin band at the height of the gun: a Mudokon that is to live and is in the
    // way has to be down under it. Working ones duck when a Slig says "look out"
    const s32 gunBottom = me.mY - (me.mHalf ? 6 : 12) + 5;
    for (const Entity& e : world.mEntities)
    {
        const bool spare = e.mKind == EntityKind::eMudokon && e.mLive && !e.mGone && e.mHalf == me.mHalf && e.mId != toDie;
        if (spare && std::abs(e.mLiveY - me.mY) <= 45 && (e.mLiveX - me.mX) * (targetX - me.mX) > 0 && std::abs(e.mLiveX - me.mX) < std::abs(targetX - me.mX) + 25 && e.mLiveTop <= gunBottom + 3)
        {
            return false;
        }
    }
    return true;
}

bool ClearSligTask::BehindShotClear(const World& world, const SligAt& me, s32 targetX, s32 aheadX)
{
    // The shot goes off some frames after the keys, and takes whatever is nearest to the Slig's place on the
    // whole screen, on any floor of it: so the target has to be the nearest now and where it will be by then
    const s32 reach = std::max(std::abs(targetX - me.mX), std::abs(aheadX - me.mX));
    if (reach > 30)
    {
        return false;
    }
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind == EntityKind::eMudokon && e.mLive && !e.mGone && !e.mHalf && std::abs(e.mLiveY - me.mY) < 260 && std::abs(e.mLiveX - me.mX) < reach + 15)
        {
            return false;
        }
    }
    return true;
}

u32 ClearSligTask::ShootLevel(const World& world, const SligAt& me, s32 targetX, s32 frames)
{
    u32 hold = AsSlig(me, &targetX);
    if (hold != Btn::eThrow)
    {
        return hold;
    }
    const bool inWay = !LevelShotClear(world, me, targetX, mShootIt ? mEntity : 0);
    if (!inWay)
    {
        mLookOutWait = 0;
        return Btn::eThrow;
    }
    if (mLookOutWait > 0)
    {
        mLookOutWait -= frames;
        return 0;
    }
    if (mLookOuts < 4 && me.mMotion == 0)
    {
        mLookOuts++;
        mLookOutWait = 50;
        return Btn::eSpeak7;
    }
    if (mLookOuts < 4)
    {
        return 0;
    }
    // It won't duck: walk on past it
    return targetX < me.mX ? Btn::eLeft : Btn::eRight;
}

TaskStep ClearSligTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const AbeState& abe = bot.Abe();
    const Entity* entity = mAmbush ? nullptr : world.Find(mEntity);
    if (mAmbush)
    {
        if (mGaveUp || mCleared >= 1)
        {
            return TaskStep::Done();
        }
        if (mPhase == 0)
        {
            mPhase = 2;
            mFrames = 0;
        }
    }
    else if (!entity || mGaveUp || mCleared >= 2)
    {
        return mInner->Next(bot, lastFailed);
    }
    const s32 x = !entity ? abe.mX : entity->mLive ? entity->mLiveX : entity->CentreX();
    const s32 y = !entity ? abe.mY : entity->mLive ? entity->mLiveY : entity->mY2;
    const s32 cam = nav.CameraOf(x, y - 5);

    const SligAt* me = nullptr;
    for (const SligAt& slig : world.mSligs)
    {
        if (slig.mPossessed)
        {
            me = &slig;
        }
    }
    if (me)
    {
        // In the Slig: let go of the chant first
        if (mPhase != 3)
        {
            mPhase = 3;
            mFrames = 0;
            mShootFrames = 0;
            mLookOuts = 0;
            mLookOutWait = 0;
            mGo.reset();
            bot.Log("in the Slig at " + std::to_string(me->mX) + "," + std::to_string(me->mY));
            return TaskStep::Do(Prim::Wait(45));
        }
        if (mRaid)
        {
            return RaidStep(bot, *me);
        }
        // Shoot what it can get at on its floor, then chant again and it bursts
        s32 targetX = 0;
        if (mShootFrames < 1200 && Target(world, *me, mShootIt ? entity : nullptr, targetX))
        {
            mShootFrames += 6;
            const u32 hold = ShootLevel(world, *me, targetX, 6);
            return TaskStep::Do(Prim::Wait(6, hold));
        }
        mFrames += 20;
        if (mFrames > 600)
        {
            return TaskStep::Failed("the Slig wouldn't burst");
        }
        return TaskStep::Do(Prim::Wait(20, Btn::eChant));
    }
    if (mPhase == 3)
    {
        // It is gone: back in Abe, when he has come round
        if (abe.mMove != Move::eIdle && ++mFrames < 700)
        {
            return TaskStep::Do(Prim::Wait(10));
        }
        mPhase = 0;
        mCleared++;
        mRaid = false;
        if (mAmbush)
        {
            bot.Log("the Slig that woke is gone");
            return TaskStep::Done();
        }
        mInner = mMake();
        bot.Log("the Slig watching " + mInner->Name() + " is gone");
        return Next(bot, "");
    }

    const SligAt* slig = mAmbush ? nullptr : Watcher(world, x, y, entity->mHalf);
    if (mPhase == 0 && slig)
    {
        // Behind its back: its beat is known, and it can't be where it sees the thing before Abe has got there
        // and done with it
        s32 camX1 = x;
        s32 camX2 = x;
        while (camX1 > x - 2000 && nav.CameraOf(camX1 - 25, y - 5) == cam)
        {
            camX1 -= 25;
        }
        while (camX2 < x + 2000 && nav.CameraOf(camX2 + 25, y - 5) == cam)
        {
            camX2 += 25;
        }
        s32 left = 100000;
        for (const SligAt& other : world.mSligs)
        {
            if (!other.mAsleep && !other.mPossessed && other.mHalf == entity->mHalf && std::abs(other.mY - y) <= 45 && std::abs(other.mX - x) < 450)
            {
                left = std::min(left, other.FramesUntilSees(x, camX1 - 25, camX2 + 25));
            }
        }
        const s32 need = std::abs(abe.mX - x) * 10 / (entity->mHalf ? 12 : 24) + 40;
        if (std::abs(abe.mY - y) <= 45 && abe.mHalfScale == entity->mHalf && left >= need)
        {
            if (!mBehind)
            {
                mBehind = true;
                bot.Log("the Slig at " + std::to_string(slig->mX) + "," + std::to_string(slig->mY) + " can't see " + mInner->Name() + " for " + std::to_string(left) + " frames, " + std::to_string(need) + " are needed: behind its back");
            }
            slig = nullptr;
        }
        else if (mBehind)
        {
            // (begun: seen through)
            slig = nullptr;
        }
    }
    if (mPhase == 0)
    {
        if (!slig)
        {
            return mInner->Next(bot, lastFailed);
        }
        // No chanting with a security orb on the screen
        bool orb = false;
        for (const Entity& e : world.mEntities)
        {
            orb = orb || (e.mKind == EntityKind::eSecurityOrb && nav.CameraOf(e.mX1, e.mY1) == cam);
        }
        const s32 sligY = slig->mY;
        const bool half = slig->mHalf;
        const NavGraph* graph = &nav;
        const auto spot = [graph, cam, sligY, half](s32 n)
        {
            const NavNode& node = graph->Nodes()[n];
            return node.mHalf == half && std::abs(node.mY - sligY) > 45 && graph->CameraOf(node.mX, node.mY - 5) == cam;
        };
        // Only a spot close by: a long way round leads past whatever else is about
        std::vector<s32> way;
        bool close = !orb && bot.AbeNode() >= 0 && nav.FindPath(bot.AbeNode(), spot, bot.EdgeCostFn(), way);
        for (s32 edge : way)
        {
            close = close && std::abs(nav.Nodes()[nav.Edges()[edge].mTo].mX - x) < 600;
        }
        std::function<bool(s32)> raidSpot;
        if (close)
        {
            mGo = std::make_unique<GoToTask>("out of the Slig's sight", spot);
        }
        else if (FindRaid(bot, raidSpot))
        {
            // Another Slig, one that can be got at, and teleporters from its floor
            mRaid = true;
            mRaidLines.clear();
            mRaidLine = -1;
            mGo = std::make_unique<GoToTask>("to where another Slig can be taken over", raidSpot);
        }
        else
        {
            // A shadow on its own floor is no good. Measured: IsInInvisibleZone is false for anyone while Abe
            // chants, so a Slig that faces him on his screen and floor shoots, panicking or not. It panics at
            // the chant, runs off the screen and shoots on its way back, about 2 frames before the 90 + 30
            // frames of chanting with it on screen are up.
            bot.Log("a Slig at " + std::to_string(slig->mX) + "," + std::to_string(slig->mY) + " watches " + mInner->Name() + ", and there is nowhere near to chant from");
            mGaveUp = true;
            return mInner->Next(bot, lastFailed);
        }
        mPhase = 1;
        mFrames = 0;
        bot.Log("a Slig at " + std::to_string(slig->mX) + "," + std::to_string(slig->mY) + " watches " + mInner->Name() + ": taking it over first");
    }
    if (mPhase == 1)
    {
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            // Nowhere to chant from: as before, by stealth alone
            bot.Log("no place to chant from: " + step.mWhy);
            mGaveUp = true;
            mPhase = 0;
            mInner = mMake();
            return mInner->Next(bot, "");
        }
        mPhase = 2;
        mFrames = 0;
    }
    if (abe.mMove == Move::eCrouch)
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }
    // Chant while it is on this screen: it takes a couple of seconds of it being there
    mFrames += 20;
    if (mAmbush && mFrames > 400)
    {
        return TaskStep::Failed("the Slig was not taken over");
    }
    if (mFrames > 2400)
    {
        mGaveUp = true;
        mPhase = 0;
        mInner = mMake();
        bot.Log("the Slig never came to be taken over");
        return mInner->Next(bot, "");
    }
    bool here = false;
    const s32 abeCam = nav.CameraOf(abe.mX, abe.mY - 5);
    for (const SligAt& s : world.mSligs)
    {
        if (s.mAsleep || nav.CameraOf(s.mX, s.mY - 5) != abeCam)
        {
            continue;
        }
        here = true;
    }
    if (mAmbush && !here)
    {
        // (it isn't listed for a moment as it is taken: not given up on at once)
        if (++mFloorFrames > 25)
        {
            return TaskStep::Done();
        }
        return TaskStep::Do(Prim::Wait(6));
    }
    return TaskStep::Do(here ? Prim::Wait(20, Btn::eChant) : Prim::Wait(6));
}

static s32 LineAt(const NavGraph& nav, s32 x, s32 y, bool half)
{
    const s32 node = nav.NodeAt(x, y, half, 30);
    return node < 0 ? -1 : nav.Nodes()[node].mLine;
}

const Entity* ClearSligTask::NextTeleporter(const NavGraph& nav, const World& world, s32 x1, s32 x2, s32 y, bool half, const std::vector<s32>& seen, const Entity** dest)
{
    for (const Entity& t : world.mEntities)
    {
        if (t.mKind != EntityKind::eTeleporter || t.mHalf != half || std::abs(t.mY2 - y) > 30 || t.CentreX() < x1 - 15 || t.CentreX() > x2 + 15)
        {
            continue;
        }
        for (const Entity& d : world.mEntities)
        {
            if (d.mKind != EntityKind::eTeleporter || &d == &t || d.mCamera != t.mInputs[0] || d.mInputs[1] != t.mInputs[2])
            {
                continue;
            }
            const s32 line = LineAt(nav, d.CentreX(), d.mY2, d.mHalf);
            if (line >= 0 && std::find(seen.begin(), seen.end(), line) == seen.end())
            {
                if (dest)
                {
                    *dest = &d;
                }
                return &t;
            }
        }
    }
    return nullptr;
}

bool ClearSligTask::FindRaid(Bot& bot, std::function<bool(s32)>& spot) const
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    for (const Entity& e : world.mEntities)
    {
        // (one from a spawner isn't there to be taken over until its screen has been seen)
        if (e.mKind != EntityKind::eSlig || e.mGone || e.mSwitch > 0)
        {
            continue;
        }
        const s32 x = e.CentreX();
        const s32 y = e.mY2;
        const s32 cam = nav.CameraOf(x, y - 5);
        bool orb = false;
        for (const Entity& o : world.mEntities)
        {
            orb = orb || (o.mKind == EntityKind::eSecurityOrb && nav.CameraOf(o.mX1, o.mY1) == cam);
        }
        const s32 line = LineAt(nav, x, y, e.mHalf);
        if (orb || line < 0)
        {
            continue;
        }
        s32 x1 = x;
        s32 x2 = x;
        for (const NavNode& n : nav.Nodes())
        {
            if (n.mLine == line && n.mHalf == e.mHalf)
            {
                x1 = std::min(x1, n.mX);
                x2 = std::max(x2, n.mX);
            }
        }
        if (!NextTeleporter(nav, world, x1, x2, y, e.mHalf, {line}, nullptr))
        {
            continue;
        }
        const NavGraph* graph = &nav;
        spot = [graph, cam, y](s32 n)
        {
            const NavNode& node = graph->Nodes()[n];
            return std::abs(node.mY - y) > 45 && graph->CameraOf(node.mX, node.mY - 5) == cam;
        };
        std::vector<s32> way;
        if (bot.AbeNode() >= 0 && nav.FindPath(bot.AbeNode(), spot, bot.EdgeCostFn(), way))
        {
            bot.Log("a Slig at " + std::to_string(x) + "," + std::to_string(y) + " can be taken over and has a teleporter on its floor: going for it");
            return true;
        }
    }
    return false;
}

TaskStep ClearSligTask::RaidStep(Bot& bot, const SligAt& me)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const s32 grid = NavGraph::GridStep(me.mHalf);
    const s32 line = LineAt(nav, me.mX, me.mY, me.mHalf);
    if (line < 0 || mPullWait > 0)
    {
        // Between teleporters, or off the cells for a moment
        mPullWait = std::max(0, mPullWait - 4);
        return TaskStep::Do(Prim::Wait(4));
    }
    if (line != mRaidLine)
    {
        mRaidLine = line;
        mRaidLines.push_back(line);
        mFloorFrames = 0;
        mPullTries = 0;
        bot.Log("the Slig is on the floor at " + std::to_string(me.mX) + "," + std::to_string(me.mY) + (me.mHalf ? " behind" : ""));
    }
    s32 x1 = me.mX;
    s32 x2 = me.mX;
    for (const NavNode& n : nav.Nodes())
    {
        if (n.mLine == line && n.mHalf == me.mHalf)
        {
            x1 = std::min(x1, n.mX);
            x2 = std::max(x2, n.mX);
        }
    }
    mFloorFrames += 4;

    // What there is to shoot from this floor: on it, or in front of it
    bool have = false;
    bool inFront = false;
    s32 targetX = 0;
    if (mFloorFrames < 2400)
    {
        const auto take = [&](s32 x, bool front)
        {
            if (!have || std::abs(x - me.mX) < std::abs(targetX - me.mX))
            {
                have = true;
                inFront = front;
                targetX = x;
            }
        };
        for (const SligAt& s : world.mSligs)
        {
            if (s.mPossessed)
            {
                continue;
            }
            if (s.mHalf == me.mHalf && std::abs(s.mY - me.mY) <= 45 && s.mX >= x1 - 25 && s.mX <= x2 + 25)
            {
                take(s.mX, false);
            }
            else if (me.mHalf && !s.mHalf && std::abs(s.mY - me.mY) < 120 && s.mX >= x1 - 450 && s.mX <= x2 + 450)
            {
                take(s.mX, true);
            }
        }
        for (const Entity& e : world.mEntities)
        {
            // Mudokons, when they are to die: those back here, and those in front that it can get behind
            if (!mShootIt || e.mKind != EntityKind::eMudokon || !e.mLive || e.mGone)
            {
                continue;
            }
            if (e.mHalf == me.mHalf && std::abs(e.mLiveY - me.mY) <= 45 && e.mLiveX >= x1 - 25 && e.mLiveX <= x2 + 25)
            {
                take(e.mLiveX, false);
            }
            else if (me.mHalf && !e.mHalf && std::abs(e.mLiveY - me.mY) < 120 && e.mLiveX >= x1 && e.mLiveX <= x2)
            {
                take(e.mLiveX, true);
            }
        }
    }
    if (have && !inFront)
    {
        return TaskStep::Do(Prim::Wait(4, ShootLevel(world, me, targetX, 4)));
    }
    if (have)
    {
        // Behind it, and shoot out of the screen: that hits what is within a cell of the Slig's place
        const s32 standX = std::max(x1, std::min(x2, targetX));
        const s32 dx = standX - me.mX;
        if (std::abs(dx) > grid)
        {
            return TaskStep::Do(Prim::Wait(4, dx < 0 ? Btn::eLeft : Btn::eRight));
        }
        s32 ahead = targetX;
        if (mHasLastTarget && std::abs(targetX - mLastTargetX) <= 20)
        {
            ahead = targetX + ((targetX - mLastTargetX) * 10) / 4;
        }
        mLastTargetX = targetX;
        mHasLastTarget = true;
        // (with a Mudokon to kill it is the profile that kills them all: none is spared)
        const bool clear = mShootIt ? std::max(std::abs(targetX - me.mX), std::abs(ahead - me.mX)) <= 30 : BehindShotClear(world, me, targetX, ahead);
        if (clear)
        {
            return TaskStep::Do(Prim::Wait(4, Btn::eDown | Btn::eThrow));
        }
        return TaskStep::Do(Prim::Wait(4));
    }

    // Nothing here: on through the next teleporter
    const Entity* dest = nullptr;
    const Entity* tele = NextTeleporter(nav, world, x1, x2, me.mY, me.mHalf, mRaidLines, &dest);
    if (tele && mPullTries < 6)
    {
        if (me.mX < tele->mX1 || me.mX > tele->mX2)
        {
            const bool left = me.mX > tele->CentreX();
            const s32 off = std::abs(me.mX - tele->CentreX());
            // Walking, it comes to a stop a cell or two after the key is let go: the last bit is single steps,
            // each a tap from standing
            if (off > 3 * grid)
            {
                return TaskStep::Do(Prim::Wait(2, left ? Btn::eLeft : Btn::eRight));
            }
            if (me.mMotion != 0 || mTapped)
            {
                mTapped = false;
                return TaskStep::Do(Prim::Wait(1));
            }
            mTapped = true;
            return TaskStep::Do(Prim::Wait(1, left ? Btn::eLeft : Btn::eRight));
        }
        if (me.mMotion != 0)
        {
            return TaskStep::Do(Prim::Wait(2));
        }
        // Its lever is a cell in front of the Slig, on one side or the other
        const Entity* lever = nullptr;
        for (const Entity& e : world.mEntities)
        {
            if (e.mKind == EntityKind::eLever && e.mSwitch == tele->mSwitch)
            {
                lever = &e;
            }
        }
        bool faceLeft = lever ? lever->CentreX() < me.mX : me.mFacingLeft;
        if (mPullTries % 2 == 1)
        {
            faceLeft = !faceLeft;
        }
        if (me.mFacingLeft != faceLeft)
        {
            return TaskStep::Do(Prim::Wait(2, faceLeft ? Btn::eLeft : Btn::eRight));
        }
        mPullTries++;
        mPullWait = 60;
        bot.Log("the Slig pulls the lever of the teleporter at " + std::to_string(tele->CentreX()) + " (switch " + std::to_string(tele->mSwitch) + ")");
        return TaskStep::Do(Prim::Wait(3, Btn::eAction));
    }

    // Done: chant, and it bursts
    mFrames += 20;
    if (mFrames > 600)
    {
        return TaskStep::Failed("the Slig wouldn't burst");
    }
    return TaskStep::Do(Prim::Wait(20, Btn::eChant));
}

bool SlogTrapTask::Find(const NavGraph& nav, const World& world, s32 y, bool half, Setup& setup)
{
    for (const Entity& lever : world.mEntities)
    {
        if (lever.mKind != EntityKind::eLever || lever.mOp != 0 || lever.mHalf != half || std::abs(lever.mY2 - y) > 30)
        {
            continue;
        }
        bool any = false;
        s32 x1 = 0;
        s32 x2 = 0;
        for (const NavFeature& f : nav.Input().mFeatures)
        {
            // Walls that are off until the lever's switch is on
            if (f.mKind == FeatureKind::eBarrier && !f.mStartClosed && f.mSwitch == lever.mSwitch && std::abs(f.mY1 - y) <= 40)
            {
                x1 = any ? std::min(x1, f.mX1) : f.mX1;
                x2 = any ? std::max(x2, f.mX2) : f.mX2;
                any = true;
            }
        }
        if (!any || (lever.mX2 > x1 && lever.mX1 < x2))
        {
            continue;
        }
        setup.mLever = lever.mId;
        setup.mSwitch = lever.mSwitch;
        setup.mX1 = x1;
        setup.mX2 = x2;
        setup.mY = y;
        setup.mLeverSide = lever.CentreX() < x1 ? -1 : 1;
        return true;
    }
    return false;
}

bool SlogTrapTask::Coming(const World& world, s32 abeY)
{
    for (const SlogAt& slog : world.mSlogs)
    {
        if (!slog.mAsleep && slog.mY > abeY - 105 && slog.mY < abeY + 30)
        {
            return true;
        }
    }
    return false;
}

bool SlogTrapTask::AnyAwake(const World& world)
{
    for (const SlogAt& slog : world.mSlogs)
    {
        if (!slog.mAsleep)
        {
            return true;
        }
    }
    return false;
}

TaskStep SlogTrapTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const AbeState& abe = bot.Abe();
    const Entity* lever = world.Find(mSetup.mLever);
    if (!lever)
    {
        return TaskStep::Failed("no such entity");
    }
    const bool half = lever->mHalf;
    const s32 cell = NavGraph::GridStep(half);
    const s32 away = -mSetup.mLeverSide;
    const s32 wallsFar = away > 0 ? mSetup.mX2 : mSetup.mX1;
    const bool on = world.Switch(mSetup.mSwitch) != 0;

    TaskStep step;
    if (mPhase != 2 && !RunGo(mGo, bot, lastFailed, step))
    {
        // Luring and one comes early: drop everything and run
        if (!(mPhase <= 1 && !on && Coming(world, abe.mY)))
        {
            return step;
        }
        mGo.reset();
    }
    else if (mPhase != 2 && step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }

    switch (mPhase)
    {
        case 0:
        case 1:
        {
            if (!AnyAwake(world))
            {
                return TaskStep::Done();
            }
            if (on)
            {
                mPhase = 3;
                mWaits = 0;
                return Next(bot, "");
            }
            if (Coming(world, abe.mY))
            {
                bot.Log("the Slogs are coming down: to the lever");
                mPhase = 2;
                mUse = std::make_unique<UseTask>(mSetup.mLever, 1);
                // On the side he comes from: no turning round with them at his heels
                mUse->SetSide(away);
                return Next(bot, "");
            }
            const s32 spot = nav.NodeAt(wallsFar + away * mCells * cell, mSetup.mY, half);
            if (spot < 0 || mCells > 40)
            {
                return TaskStep::Failed("the Slogs won't come down");
            }
            if (bot.AbeNode() != spot)
            {
                mPhase = 1;
                mWaits = 0;
                mGo = GoToNode(spot);
                mGo->SetTopGait(Gait::eWalk);
                return Next(bot, "");
            }
            // Ready to run: facing the lever
            if (abe.mFacingLeft != (away > 0))
            {
                return TaskStep::Do(Prim::Face(away > 0));
            }
            mWaits += 4;
            if (mWaits > 60)
            {
                mCells += 1;
                mWaits = 0;
            }
            return TaskStep::Do(Prim::Wait(4));
        }

        case 2:
        {
            if (on)
            {
                mUse.reset();
                mPhase = 3;
                mWaits = 0;
                return Next(bot, "");
            }
            step = mUse->Next(bot, lastFailed);
            if (step.mKind == TaskStep::Kind::eDone)
            {
                mUse.reset();
                mPhase = 3;
                mWaits = 0;
                return TaskStep::Do(Prim::Wait(4));
            }
            return step;
        }

        case 3:
            // They run into the walls after him
            if (Coming(world, abe.mY) && ++mWaits < 40)
            {
                return TaskStep::Do(Prim::Wait(10));
            }
            bot.Log(std::string("the walls are on: ") + (Coming(world, abe.mY) ? "a Slog is still about" : "no Slog left down here"));
            mPhase = 4;
            // Walking out of the lever's stretch and back in turns the walls off again
            mGo = GoToNode(nav.NodeAt(lever->CentreX() + mSetup.mLeverSide * 8 * cell, mSetup.mY, half));
            return Next(bot, "");

        case 4:
            mPhase = 5;
            mGo = GoToNode(Domain::UseNode(nav, *lever, nullptr));
            return Next(bot, "");

        default:
            if (on)
            {
                return TaskStep::Failed("the walls stay on");
            }
            if (AnyAwake(world) && ++mRounds < 5)
            {
                mPhase = 0;
                mCells = 2;
                return Next(bot, "");
            }
            return TaskStep::Done();
    }
}

TaskStep KillTask::Next(Bot& bot, const std::string& lastFailed)
{
    const Entity* mud = bot.GetWorld().Find(mMud);
    const AbeState& abe = bot.Abe();
    if (!mud)
    {
        return TaskStep::Failed("no such entity");
    }
    if (mud->mGone)
    {
        return TaskStep::Done();
    }
    // (crouched on his way where there is no room to stand: the way there knows when he can get up)
    mNoStand = abe.mMove == Move::eCrouch && (mNoStand || lastFailed.find("no reaction") != std::string::npos);
    if (abe.mMove == Move::eCrouch && !mNoStand && !(mGo && mGo->Ducked()) && !GoToTask::SweepOver(bot.Nav(), bot.GetWorld(), bot.AbeNode()))
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }

    // (before he is out in the open on his way to it)
    if (SligNeverLeaves(bot, *mud))
    {
        return TaskStep::Failed("a Slig that never leaves the screen walks the Mudokon's floor");
    }
    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }

    const NavGraph& nav = bot.Nav();
    const s32 cell = NavGraph::GridStep(mud->mHalf);
    const s32 mudNode = Domain::UseNode(nav, *mud, nullptr);
    if (mudNode < 0)
    {
        // Knocked off its feet, or off the floors the graph knows: it may be back
        if (++mWaits > 40)
        {
            return TaskStep::Failed("the Mudokon isn't on a floor");
        }
        return TaskStep::Do(Prim::Wait(10));
    }
    // A slap is heard too, and so is what the Mudokon says to it
    if (HoldForSlig(bot, *mud, 25, mGo, mHeld, step))
    {
        return step;
    }
    const s32 dx = std::abs((mud->mLive ? mud->mLiveX : mud->CentreX()) - abe.mX);
    const s32 abeNode = bot.AbeNode();
    const bool sameFloor = abeNode >= 0 && nav.Nodes()[abeNode].mLine == nav.Nodes()[mudNode].mLine;
    // A slap lands on the cell in front of Abe
    if (!sameFloor || dx * 2 < cell || dx * 2 > 3 * cell)
    {
        if (++mCloseIns > 60)
        {
            return TaskStep::Failed("can't get next to the Mudokon");
        }
        mGo = GoNearMud(nav, bot.GetWorld(), mudNode, 1, 3);
        return Next(bot, "");
    }
    if (!mud->mLive)
    {
        if (++mWaits > 40)
        {
            return TaskStep::Failed("the Mudokon isn't there");
        }
        return TaskStep::Do(Prim::Wait(10));
    }
    const bool faceLeft = mud->mLiveX < abe.mX;
    if (abe.mFacingLeft != faceLeft)
    {
        return TaskStep::Do(Prim::Face(faceLeft));
    }
    if (mPause)
    {
        mPause = false;
        return TaskStep::Do(Prim::Wait(8));
    }
    if (++mSlaps > 60)
    {
        return TaskStep::Failed("slapping didn't kill the Mudokon");
    }
    mPause = true;
    return TaskStep::Do(Prim::Press(bot.SpeakSlap(), 6));
}

TaskStep RescueTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const Entity* mud = world.Find(mMud);
    const Entity* portal = world.Find(mPortal);
    const AbeState& abe = bot.Abe();
    if (!mud || !portal)
    {
        return TaskStep::Failed("no such entity");
    }
    if (mud->mGone)
    {
        return TaskStep::Done();
    }
    // A drill at work between a blind Mudokon and its portal, on the floor it runs along once that opens: stopped
    // first (the Mines' path 4: one between the drills at 1575, 1650 and 1725, its portal at 1812, their levers
    // on the ledge over them)
    if (mDrillStop)
    {
        const TaskStep step = mDrillStop->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        mDrillStop.reset();
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return TaskStep::Failed("couldn't stop the drill between the Mudokon and its portal: " + step.mWhy);
        }
    }
    if (mud->mBlind && mPhase < 4 && bot.AbeNode() >= 0 && abe.Settled())
    {
        const s32 mx = mud->mLive ? mud->mLiveX : mud->CentreX();
        const s32 my = mud->mLive ? mud->mLiveY : mud->mY2;
        const s32 px = portal->CentreX();
        for (const Entity& drill : world.mEntities)
        {
            if (drill.mKind != EntityKind::eDrill || drill.mHalf != mud->mHalf || !world.SweepRuns(drill) || std::abs(portal->mY2 - my) > 20 || drill.mX2 < std::min(mx, px) || drill.mX1 > std::max(mx, px) || drill.mY2 < my - (mud->mHalf ? 30 : 60) || drill.mY1 > my)
            {
                continue;
            }
            // (its lever, by whatever way there is to it: past drills too, which the walk there times)
            const Entity* lever = nullptr;
            const s32 want = drill.mInputs[1] ? 1 : 0;
            for (const Entity& e : world.mEntities)
            {
                std::vector<s32> path;
                const s32 node = (e.mKind == EntityKind::eLever || e.mKind == EntityKind::ePullRing) && e.mSwitch == drill.mSwitch && (e.mOp == 2 || e.mOp == (want ? 0 : 1)) && drill.mInputs[0] == 1 && drill.mInputs[2] == 0 ? Domain::UseNode(bot.Nav(), e, nullptr) : -1;
                if (node >= 0 && bot.Nav().FindPath(bot.AbeNode(), node, bot.EdgeCostFn(), path))
                {
                    lever = &e;
                    break;
                }
            }
            if (lever)
            {
                bot.Log("Drill#" + std::to_string(drill.mId) + " is between the blind Mudokon and its portal: stopping it with " + std::string(EntityKindName(lever->mKind)) + "#" + std::to_string(lever->mId) + " first");
                mGo.reset();
                mDrillStop = std::make_unique<UseTask>(lever->mId, want);
                return Next(bot, "");
            }
            if (bot.SavesMudokons())
            {
                return TaskStep::Failed("a drill at work between the Mudokon and its portal");
            }
        }
    }
    // (crouched on his way where there is no room to stand: the way there knows when he can get up)
    mNoStand = abe.mMove == Move::eCrouch && (mNoStand || lastFailed.find("no reaction") != std::string::npos);
    if (abe.mMove == Move::eCrouch && !mNoStand && !(mGo && (mGo->Disarming() || mGo->Ducked())) && !GoToTask::SweepOver(bot.Nav(), bot.GetWorld(), bot.AbeNode()))
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }

    if (mud->mBlind && mPhase < 4 && !mBlindThere)
    {
        TaskStep lead;
        if (!LeadBlind(bot, *mud, *portal, lastFailed, lead))
        {
            return lead;
        }
        mBlindThere = true;
        bot.Log("the blind Mudokon is on the portal's floor at " + std::to_string(mud->mLiveX));
    }
    if (!mud->mFollowing && mPhase < 4 && !mud->mBlind)
    {
        // Not with Abe (yet, or any more): talk it round
        if (!mRecruit)
        {
            if (++mRecruits > 3)
            {
                return TaskStep::Failed("the Mudokon keeps wandering off");
            }
            mRecruit = std::make_unique<RecruitTask>(mMud);
            mGo.reset();
        }
        const TaskStep step = mRecruit->Next(bot, lastFailed);
        if (step.mKind != TaskStep::Kind::eDone)
        {
            return step;
        }
    }
    mRecruit.reset();

    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }

    const s32 cell = NavGraph::GridStep(mud->mHalf);
    switch (mPhase)
    {
        case 0: // to the portal
        {
            const s32 node = Domain::UseNode(bot.Nav(), *portal, nullptr);
            if (node < 0)
            {
                return TaskStep::Failed("can't stand by the portal");
            }
            mPhase = 3;
            mWaits = 0;
            if (mud->mBlind)
            {
                // It is where it has to be: chanted for from anywhere on the portal's screen that isn't close
                // enough to scare its birds
                const NavGraph* g = &bot.Nav();
                const s32 cam = g->CameraOf(portal->CentreX(), portal->mY2 - 5);
                const s32 px = portal->CentreX();
                const s32 py = portal->mY2;
                mGo = std::make_unique<GoToTask>("to chant from", [g, cam, px, py](s32 n)
                                                 {
                                                     const NavNode& at = g->Nodes()[n];
                                                     return g->CameraOf(at.mX, at.mY - 5) == cam && !at.mHazard && at.mTrapDoor < 0 && (std::abs(at.mX - px) > 110 || std::abs(at.mY - py) > 40);
                                                 });
                return Next(bot, "");
            }
            // Either side of it will do: the one that doesn't mean walking through it (see Bot::EdgeCostFn)
            const s32 side = 3;
            const s32 px = bot.Nav().Nodes()[node].mX < portal->CentreX() ? bot.Nav().Nodes()[node].mX + 2 * side * cell : bot.Nav().Nodes()[node].mX - 2 * side * cell;
            const s32 other = bot.Nav().NodeAt(px, bot.Nav().Nodes()[node].mY, portal->mHalf, 10);
            mGo = std::make_unique<GoToTask>("lead to portal", [node, other](s32 n)
                                             { return n == node || n == other; });
            mGo->SetLeading(true);
            return Next(bot, "");
        }

        case 3: // let it catch up
            if (!mud->mLive)
            {
                return TaskStep::Failed("lost the Mudokon");
            }
            // (a blind one doesn't come: it runs for the portal from where it is once that opens)
            if (!mud->mBlind && std::abs(mud->mLiveX - abe.mX) > 5 * cell && ++mWaits < 40)
            {
                return TaskStep::Do(Prim::Wait(15));
            }
            mPhase = 4;
            mWaits = 0;
            return Next(bot, "");

        case 5: // back from off its screen: start over
            mPhase = 0;
            return Next(bot, "");

        default: // chant
            // Brain_6_Escape: it has seen the portal open and is on its way
            if (++mWaits > 8 && mud->mState / 100 != 6 && mRespawns < 2)
            {
                // No portal: its birds have been scared off (Abe came too close). It is back when its screen
                // is come into again.
                const NavGraph& graph = bot.Nav();
                const NavGraph* g = &graph;
                const s32 cam = graph.CameraOf(portal->CentreX(), portal->mY2 - 5);
                mRespawns++;
                mPhase = 5;
                mWaits = 0;
                bot.Log("no portal opened for " + Name() + ": off its screen and back to bring its birds home");
                mGo = std::make_unique<GoToTask>("off the portal's screen", [g, cam](s32 n)
                                                 { return g->CameraOf(g->Nodes()[n].mX, g->Nodes()[n].mY - 5) != cam; });
                mGo->SetLeading(true);
                return Next(bot, "");
            }
            if (mWaits > 30)
            {
                return TaskStep::Failed("chanting didn't send the Mudokon through");
            }
            return TaskStep::Do(Prim::Wait(40, Btn::eChant));
    }
}

bool TrapSligTask::Find(const NavGraph& nav, const World& world, const SligAt& slig, u32& ring, u32& drill)
{
    for (const Entity& d : world.mEntities)
    {
        // One that comes down when its switch goes on, to the Slig's floor
        if (d.mKind != EntityKind::eDrill || d.mInputs[0] != 2 || d.mHalf != slig.mHalf || d.mY2 < slig.mY - 10 || d.mY1 > slig.mY - 40 || d.mX2 - d.mX1 > d.mY2 - d.mY1)
        {
            continue;
        }
        for (const Entity& r : world.mEntities)
        {
            if ((r.mKind != EntityKind::ePullRing && r.mKind != EntityKind::eLever) || r.mSwitch != d.mSwitch)
            {
                continue;
            }
            const s32 node = Domain::UseNode(nav, r, nullptr);
            if (node >= 0 && std::abs(nav.Nodes()[node].mY - slig.mY) > 45)
            {
                ring = r.mId;
                drill = d.mId;
                return true;
            }
        }
    }
    return false;
}

TaskStep TrapSligTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* ring = world.Find(mRing);
    const Entity* drill = world.Find(mDrill);
    if (!ring || !drill)
    {
        return TaskStep::Failed("no such entity");
    }
    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }
    // Under any of the drills on that switch
    s32 x1 = drill->mX1;
    s32 x2 = drill->mX2;
    for (const Entity& d : world.mEntities)
    {
        if (d.mKind == EntityKind::eDrill && d.mSwitch == drill->mSwitch && d.mHalf == drill->mHalf && std::abs(d.mY2 - drill->mY2) < 20)
        {
            x1 = std::min(x1, d.mX1);
            x2 = std::max(x2, d.mX2);
        }
    }
    const SligAt* slig = nullptr;
    for (const SligAt& s : world.mSligs)
    {
        if (!s.mPossessed && s.mHalf == drill->mHalf && std::abs(s.mY - drill->mY2) <= 45 && std::abs(s.mX - drill->CentreX()) < 900)
        {
            slig = &s;
        }
    }
    const s32 node = Domain::UseNode(nav, *ring, nullptr);
    switch (mPhase)
    {
        case 0:
            if (node < 0)
            {
                return TaskStep::Failed("nowhere to pull from");
            }
            mPhase = 1;
            mGo = GoToNode(node);
            return Next(bot, "");

        case 1: // until it walks in under
        {
            if (!slig)
            {
                // (not about: its screen isn't this one, or it is gone)
                return mPulls > 0 ? TaskStep::Done() : TaskStep::Failed("the Slig isn't about");
            }
            if (bot.Abe().mMove == Move::eCrouch)
            {
                return TaskStep::Do(Prim::Press(Btn::eUp));
            }
            if (++mWaits > 1500)
            {
                return TaskStep::Failed("the Slig never came under the drill");
            }
            // Where it will be when the drill gets down to it: the jump to the ring, the pull, the stroke
            const s32 lead = 22;
            const s32 will = slig->mX + (slig->mX - mLastX) * lead;
            const bool moving = mLastX != 0 && slig->mX != mLastX;
            const bool standsUnder = mLastX == slig->mX && slig->mX >= x1 + 2 && slig->mX <= x2 - 2;
            mLastX = slig->mX;
            if ((moving && will >= x1 + 6 && will <= x2 - 6) || standsUnder)
            {
                bot.Log("pulling the ring with the Slig at " + std::to_string(slig->mX));
                mPhase = 2;
                mWaits = 0;
                mPulls++;
                return TaskStep::Do(Prim::Press(Btn::eUp));
            }
            return TaskStep::Do(Prim::Wait(1));
        }

        default: // did it?
            if (++mWaits < 70)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (!slig)
            {
                bot.Log("the drill got the Slig");
                return TaskStep::Done();
            }
            if (mPulls >= 6)
            {
                return TaskStep::Failed("the drill kept missing the Slig");
            }
            mPhase = 1;
            mWaits = 0;
            mLastX = 0;
            return Next(bot, "");
    }
}

TaskStep SendToWheelTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* mud = world.Find(mMud);
    const Entity* wheel = world.Find(mWheel);
    if (!mud || !wheel)
    {
        return TaskStep::Failed("no such entity");
    }
    if (world.Switch(wheel->mSwitch) == 1)
    {
        return TaskStep::Done();
    }
    if (mud->mGone)
    {
        return TaskStep::Failed("the Mudokon is gone");
    }
    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }
    const s32 mudX = mud->mLive ? mud->mLiveX : mud->CentreX();
    const bool atWheel = mud->mLive && mudX >= wheel->mX1 - 6 && mudX <= wheel->mX2 + 6 && std::abs(mud->mLiveY - wheel->mY2) < 30;
    switch (mPhase)
    {
        case 0: // to where it is called from
        {
            if (atWheel)
            {
                mPhase = 3;
                return Next(bot, "");
            }
            const s32 at = bot.AbeNode();
            if (at < 0 || !Domain::CallSpot(nav, *mud, *wheel, at))
            {
                if (++mWaits > 4)
                {
                    return TaskStep::Failed("can't get to where the Mudokon is called from");
                }
                const NavGraph* g = &nav;
                const Entity m = *mud;
                const Entity w = *wheel;
                mGo = std::make_unique<GoToTask>("to call the Mudokon from", [g, m, w](s32 n)
                                                 { return Domain::CallSpot(*g, m, w, n); });
                return Next(bot, "");
            }
            bot.Log("calling blind Mudokon#" + std::to_string(mMud) + " at " + std::to_string(mudX) + " over to the wheel at " + std::to_string(wheel->CentreX()));
            mRecruit = std::make_unique<RecruitTask>(mMud);
            mRecruit->SetRemote(true);
            mPhase = 1;
            return Next(bot, "");
        }

        case 1: // hello, follow me
        {
            const TaskStep talk = mRecruit->Next(bot, lastFailed);
            if (talk.mKind != TaskStep::Kind::eDone)
            {
                return talk;
            }
            mRecruit.reset();
            mPhase = 2;
            mWaits = 0;
            mStill = 0;
            return Next(bot, "");
        }

        case 2: // on its way
            mStill = mudX == mLastX ? mStill + 1 : 0;
            mLastX = mudX;
            if (atWheel && mStill >= 10)
            {
                mPhase = 3;
                mWaits = 0;
                return Next(bot, "");
            }
            if (++mWaits > 900 || (mStill > 60 && !mud->mFollowing))
            {
                return TaskStep::Failed("the Mudokon stopped at " + std::to_string(mudX) + ", not at the wheel");
            }
            return TaskStep::Do(Prim::Wait(1));

        default: // work
            if (++mWaits > 400)
            {
                return TaskStep::Failed("the Mudokon doesn't turn the wheel");
            }
            if (mWaits % 60 == 1)
            {
                bot.Log("work, said to Mudokon#" + std::to_string(mMud) + " at the wheel, state " + std::to_string(mud->mState));
                return TaskStep::Do(Prim::Wait(2, bot.SpeakWork()));
            }
            return TaskStep::Do(Prim::Wait(1));
    }
}

bool RescueTask::LeadBlind(Bot& bot, const Entity& mud, const Entity& portal, const std::string& lastFailed, TaskStep& step)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const s32 cell = NavGraph::GridStep(mud.mHalf);
    if (mUse)
    {
        step = mUse->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return false;
        }
        mUse.reset();
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            step = TaskStep::Failed("couldn't open the trap door under the Mudokon: " + step.mWhy);
            return false;
        }
        mBlindPhase = 5;
        mBlindWaits = 0;
    }
    if (mRecruit)
    {
        step = mRecruit->Next(bot, lastFailed);
        if (step.mKind != TaskStep::Kind::eDone)
        {
            return false;
        }
        mRecruit.reset();
        mBlindPhase = 2;
        mBlindWaits = 0;
    }
    const s32 mudNode = Domain::UseNode(nav, mud, nullptr);
    const s32 mudX = mud.mLive ? mud.mLiveX : mud.CentreX();

    switch (mBlindPhase)
    {
        case 0: // where to next
        {
            if (mudNode < 0)
            {
                step = TaskStep::Failed("the blind Mudokon isn't on a floor");
                return false;
            }
            std::vector<s32> way;
            if (!Domain::BlindWay(nav, world, mudNode, portal, true, way))
            {
                step = TaskStep::Failed("no way for the blind Mudokon to the portal's floor");
                return false;
            }
            if (way.empty())
            {
                return true;
            }
            if (++mBlindTries > 8)
            {
                step = TaskStep::Failed("the blind Mudokon doesn't get to the portal");
                return false;
            }
            const s32 found = nav.Edges()[way.back()].mTo;
            // The first straight stretch of it
            mBlindDir = 0;
            mBlindTrap = -1;
            mBlindRide = -1;
            mBlindStop = found;
            for (s32 idx : way)
            {
                const NavEdge& e = nav.Edges()[idx];
                const NavNode& from = nav.Nodes()[e.mFrom];
                const NavNode& to = nav.Nodes()[e.mTo];
                if (e.mKind == EdgeKind::eFall && from.mTrapDoor >= 0 && e.mFeature == from.mTrapDoor)
                {
                    mBlindStop = e.mFrom;
                    mBlindTrap = from.mTrapDoor;
                    break;
                }
                if (e.mKind == EdgeKind::eLift)
                {
                    // It stands on the lift, and comes along when Abe works it
                    mBlindStop = e.mFrom;
                    mBlindRide = e.mTo;
                    break;
                }
                const s32 dir = to.mX > from.mX ? 1 : (to.mX < from.mX ? -1 : 0);
                if (mBlindDir != 0 && dir != 0 && dir != mBlindDir)
                {
                    mBlindStop = e.mFrom;
                    break;
                }
                mBlindDir = mBlindDir ? mBlindDir : dir;
            }
            if (mBlindStop == mudNode && mBlindTrap >= 0)
            {
                mBlindPhase = 4;
                return LeadBlind(bot, mud, portal, lastFailed, step);
            }
            if (mBlindStop == mudNode && mBlindRide >= 0)
            {
                bot.Log("taking the blind Mudokon along on the lift");
                mBlindPhase = 6;
                mGo = GoToNode(mBlindRide);
                return LeadBlind(bot, mud, portal, lastFailed, step);
            }
            bot.Log("leading the blind Mudokon from " + std::to_string(mudX) + (mBlindDir < 0 ? " left" : " right") + " to " + std::to_string(nav.Nodes()[mBlindStop].mX) + "," + std::to_string(nav.Nodes()[mBlindStop].mY) + (mBlindTrap >= 0 ? ", a trap door" : ""));
            mRecruit = std::make_unique<RecruitTask>(mMud);
            mRecruit->SetSide(mBlindDir);
            mBlindPhase = 1;
            mBlindAhead = false;
            return LeadBlind(bot, mud, portal, lastFailed, step);
        }

        case 2: // walking: wait is said a cell before where it is to stop
        {
            if (!mud.mLive)
            {
                step = TaskStep::Failed("lost the blind Mudokon");
                return false;
            }
            // It stops when it walks off the screen with its back to Abe: so on ahead of it, if it has far to go
            const NavNode& stop = nav.Nodes()[mBlindStop];
            if (!mBlindAhead)
            {
                mBlindAhead = true;
                if (std::abs(stop.mX - bot.Abe().mX) > 150 && (stop.mX - bot.Abe().mX) * mBlindDir > 0)
                {
                    s32 to = mBlindStop;
                    for (s32 cells = 1; cells <= 3; cells++)
                    {
                        const s32 next = nav.NodeAt(nav.StepX(nav.Nodes()[to].mX, mBlindDir, stop.mHalf), stop.mY, stop.mHalf, 8);
                        if (next < 0 || nav.Nodes()[next].mHazard || nav.Nodes()[next].mTrapDoor >= 0 || nav.Nodes()[next].mLine != stop.mLine)
                        {
                            break;
                        }
                        to = next;
                    }
                    mGo = GoToNode(to);
                }
            }
            if (mGo)
            {
                step = mGo->Next(bot, lastFailed);
                if (step.mKind == TaskStep::Kind::ePrim)
                {
                    return false;
                }
                mGo.reset();
            }
            const s32 ahead = (stop.mX - mudX) * mBlindDir;
            if (mud.mFollowing && ahead > cell + cell / 2 && ++mBlindWaits < 900)
            {
                step = TaskStep::Do(Prim::Wait(1));
                return false;
            }
            mBlindPhase = 3;
            mBlindWaits = 0;
            mBlindStill = 0;
            mBlindLastX = mudX;
            if (mud.mFollowing)
            {
                bot.Log("wait, said to the blind Mudokon at " + std::to_string(mudX));
                step = TaskStep::Do(Prim::Press(bot.SpeakWait(), 6));
                return false;
            }
            return LeadBlind(bot, mud, portal, lastFailed, step);
        }

        case 3: // until it stands
        case 5: // until it has fallen through and stands
        {
            mBlindStill = mudX == mBlindLastX && !mud.mFollowing ? mBlindStill + 1 : 0;
            mBlindLastX = mudX;
            const bool fallen = mudNode >= 0 && nav.Nodes()[mudNode].mTrapDoor != mBlindTrap;
            if ((mBlindStill < 20 || (mBlindPhase == 5 && !fallen)) && ++mBlindWaits < 300)
            {
                step = TaskStep::Do(Prim::Wait(1));
                return false;
            }
            bot.Log("the blind Mudokon stands at " + std::to_string(mudX) + "," + std::to_string(mud.mLiveY));
            if (mBlindPhase == 3 && mBlindTrap >= 0 && mudNode >= 0 && nav.Nodes()[mudNode].mTrapDoor == mBlindTrap)
            {
                mBlindPhase = 4;
            }
            else
            {
                mBlindPhase = 0;
            }
            return LeadBlind(bot, mud, portal, lastFailed, step);
        }

        case 6: // on the lift with it
        {
            if (mGo)
            {
                step = mGo->Next(bot, lastFailed);
                if (step.mKind == TaskStep::Kind::ePrim)
                {
                    return false;
                }
                mGo.reset();
                if (step.mKind == TaskStep::Kind::eFailed)
                {
                    return false;
                }
            }
            mBlindPhase = 0;
            return LeadBlind(bot, mud, portal, lastFailed, step);
        }

        case 4: // on the trap door: open it
        {
            const NavFeature& trap = nav.Input().mFeatures[mBlindTrap];
            for (const Entity& e : world.mEntities)
            {
                if ((e.mKind == EntityKind::eLever || e.mKind == EntityKind::ePullRing) && e.mSwitch == trap.mSwitch && e.mHalf == mud.mHalf)
                {
                    const s32 node = Domain::UseNode(nav, e, nullptr);
                    if (node >= 0 && nav.Nodes()[node].mTrapDoor < 0)
                    {
                        bot.Log("opening the trap door under the blind Mudokon with #" + std::to_string(e.mId));
                        mUse = std::make_unique<UseTask>(e.mId, trap.mStartClosed ? 1 : 0);
                        return LeadBlind(bot, mud, portal, lastFailed, step);
                    }
                }
            }
            step = TaskStep::Failed("nothing opens the trap door under the Mudokon");
            return false;
        }

        default:
            step = TaskStep::Failed("lost track of the blind Mudokon");
            return false;
    }
}

TaskStep WorkTogetherTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const NavGraph& nav = bot.Nav();
    const Entity* logic = world.Find(mLogic);
    if (!logic)
    {
        return TaskStep::Failed("no such entity");
    }
    if (world.Switch(logic->mSwitch) == (logic->mOp == 1 ? 0 : 1))
    {
        return TaskStep::Done();
    }
    if (abe.mMove == Move::eCrouch)
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }
    if (!mPlanned)
    {
        std::vector<Goap::Fact> pre;
        if (!Domain::PlanTogether(nav, world, *logic, mTeam, pre))
        {
            return TaskStep::Failed("not enough Mudokons for the wheels");
        }
        mPlanned = true;
    }

    // The wheel a Mudokon stands at, or null
    const auto wheelOf = [&](const Entity& mud) -> const Entity*
    {
        for (u32 id : mTeam.mWheels)
        {
            const Entity* wheel = world.Find(id);
            if (wheel && mud.mLive && mud.mLiveX >= wheel->mX1 && mud.mLiveX <= wheel->mX2 && std::abs(mud.mLiveY - wheel->mY2) < 30)
            {
                return wheel;
            }
        }
        return nullptr;
    };

    // Following Mudokons stop at the free wheels they pass, and stay there. Two that arrive together stop at the
    // same wheel, so it goes in rounds: whoever isn't at a wheel yet is fetched and led along the wheels again,
    // until one wheel is left for Abe. Then all are told to work at once: they turn for a moment only.
    if (mStage == 0)
    {
        std::vector<const Entity*> taken;
        for (u32 mudId : mTeam.mMuds)
        {
            const Entity* mud = world.Find(mudId);
            if (!mud || mud->mGone)
            {
                return TaskStep::Failed("lost a Mudokon");
            }
            // The first at each wheel has it
            const Entity* wheel = wheelOf(*mud);
            bool first = wheel != nullptr;
            for (const Entity* t : taken)
            {
                first = first && t != wheel;
            }
            if (first)
            {
                taken.push_back(wheel);
                continue;
            }
            if (mud->mFollowing)
            {
                continue;
            }
            if (!mRecruit || mRecruiting != mudId)
            {
                if (++mRecruits > 8 * static_cast<s32>(mTeam.mMuds.size()))
                {
                    return TaskStep::Failed("the Mudokons won't all come along");
                }
                mRecruit = std::make_unique<RecruitTask>(mudId);
                // (all o' ya: the others on the screen listen too, and one "follow me" brings them all)
                mRecruit->SetAll(true);
                mRecruiting = mudId;
            }
            const TaskStep step = mRecruit->Next(bot, lastFailed);
            if (step.mKind == TaskStep::Kind::eDone)
            {
                mRecruit.reset();
                return Next(bot, "");
            }
            return step;
        }
        mRecruit.reset();
        mStage = 1;
    }

    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed)
    {
        return step;
    }

    const Entity* farWheel = world.Find(mTeam.mAbeWheel);
    const Entity* firstMud = world.Find(mTeam.mMuds[0]);
    if (!farWheel || !firstMud)
    {
        return TaskStep::Failed("no wheel");
    }
    const s32 farNode = Domain::UseNode(nav, *farWheel, nullptr);
    const s32 cell = NavGraph::GridStep(farWheel->mHalf);
    const s32 away = farWheel->CentreX() >= firstMud->CentreX() ? 1 : -1;
    // A cell up to so many on from a wheel, by plain floor
    const auto beyond = [&](s32 fromNode, s32 dir, s32 cells)
    {
        for (; cells >= 1; cells--)
        {
            const s32 n = nav.NodeAt(nav.Nodes()[fromNode].mX + dir * cells * cell, nav.Nodes()[fromNode].mY, farWheel->mHalf, 8);
            std::vector<s32> path;
            if (n >= 0 && nav.FindPath(fromNode, n, [](const NavEdge& e)
                                       { return e.mKind == EdgeKind::eWalk && e.mFeature < 0 ? e.mCost : -1; }, path))
            {
                return n;
            }
        }
        return fromNode;
    };

    switch (mStage)
    {
        case 1: // to the side they come from, a little before the first wheel
        {
            const Entity* nearWheel = farWheel;
            for (u32 id : mTeam.mWheels)
            {
                const Entity* wheel = world.Find(id);
                if (wheel && (wheel->CentreX() - nearWheel->CentreX()) * away < 0)
                {
                    nearWheel = wheel;
                }
            }
            mStage = 2;
            mGo = GoToNode(beyond(Domain::UseNode(nav, *nearWheel, nullptr), -away, 2));
            mGo->SetLeading(true);
            return Next(bot, "");
        }

        case 2: // and past the last one, without stopping: the ones that have stopped at a wheel are not waited
                // for, and one that catches Abe up standing among the wheels stops behind him, not at a wheel
            mStage = 3;
            mWaits = 0;
            mStill = 0;
            mGo = GoToNode(beyond(farNode, away, 3));
            // (at a walk: a Mudokon that runs after him doesn't stop at a wheel)
            mGo->SetTopGait(Gait::eWalk);
            return Next(bot, "");

        case 3: // let them settle
        {
            s32 sum = 0;
            for (const Entity& e : world.mEntities)
            {
                sum += e.mKind == EntityKind::eMudokon && e.mLive ? e.mLiveX : 0;
            }
            const bool still = sum == mLastSum;
            mLastSum = sum;
            if ((!still || ++mStill < 2) && ++mWaits < 40)
            {
                return TaskStep::Do(Prim::Wait(15));
            }
            mStage = 4;
            mTurns = 0;
            return Next(bot, "");
        }

        case 4: // who has a wheel
        {
            std::vector<const Entity*> taken;
            for (u32 id : mTeam.mMuds)
            {
                const Entity* mud = world.Find(id);
                const Entity* wheel = mud ? wheelOf(*mud) : nullptr;
                bool have = wheel == nullptr;
                for (const Entity* t : taken)
                {
                    have = have || t == wheel;
                }
                if (!have)
                {
                    taken.push_back(wheel);
                }
            }
            const s32 free = static_cast<s32>(mTeam.mWheels.size() - taken.size());
            const Entity* mine = nullptr;
            for (u32 id : mTeam.mWheels)
            {
                const Entity* wheel = world.Find(id);
                bool isTaken = false;
                for (const Entity* t : taken)
                {
                    isTaken = isTaken || t == wheel;
                }
                if (wheel && !isTaken && (!mine || id == mTeam.mAbeWheel))
                {
                    mine = wheel;
                }
            }
            bot.Log("wheels: " + std::to_string(free) + " of " + std::to_string(mTeam.mWheels.size()) + " without a Mudokon");
            if (free > 1)
            {
                if (++mRounds > 10)
                {
                    return TaskStep::Failed("couldn't get a Mudokon to every wheel");
                }
                mStage = 0;
                return Next(bot, "");
            }
            mMine = mine ? mine->mId : 0;
            mStage = 5;
            mTurns = 0;
            if (mine)
            {
                mGo = GoToNode(Domain::UseNode(nav, *mine, nullptr));
            }
            return Next(bot, "");
        }

        default: // all o' ya (the ones that stood so long they went back to their own work listen again), work, turn
        {
            if (mSaidWork)
            {
                // His own wheel straight away, the button down before he has finished speaking: theirs only count
                // for a moment
                mSaidWork = false;
                return TaskStep::Do(Prim::Wait(130, mMine ? static_cast<u32>(Btn::eUp) : 0u));
            }
            bool listening = true;
            for (u32 id : mTeam.mMuds)
            {
                const Entity* e = world.Find(id);
                // Brain_4_ListeningToAbe, as GameView packs it
                listening = listening && e && e->mLive && (wheelOf(*e) == nullptr || e->mState / 100 == 4);
            }
            if (++mTurns > 24)
            {
                if (++mRounds > 10)
                {
                    return TaskStep::Failed("turning the wheels together didn't do it");
                }
                mStage = 0;
                return Next(bot, "");
            }
            if (!listening)
            {
                // It takes them a while to put their tools down
                mSaidWork = false;
                return mTurns % 2 ? TaskStep::Do(Prim::Press(bot.SpeakAll(), 6)) : TaskStep::Do(Prim::Wait(40));
            }
            mSaidWork = true;
            return TaskStep::Do(Prim::Wait(2, bot.SpeakWork()));
        }
    }
}

TaskStep ScrabRampageTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* first = world.Find(mScrab);
    if (!first)
    {
        return TaskStep::Failed("no such entity");
    }
    const s32 floorNode = nav.NodeAt(first->CentreX(), first->mY2, first->mHalf, 30);
    if (floorNode < 0)
    {
        return TaskStep::Failed("the Scrab is on no floor");
    }
    const s32 line = nav.Nodes()[floorNode].mLine;
    const s32 floorY = nav.Nodes()[floorNode].mY;
    const auto onFloor = [&](const Entity& e)
    {
        const s32 at = nav.NodeAt(e.CentreX(), e.mY2, e.mHalf, 30);
        return at >= 0 && nav.Nodes()[at].mLine == line;
    };
    const ScrabAt* me = nullptr;
    for (const ScrabAt& scrab : world.mScrabs)
    {
        me = scrab.mPossessed ? &scrab : me;
    }
    if (++mFrames > 9000)
    {
        return TaskStep::Failed("the Scrab took too long");
    }
    if (mPhase == 0)
    {
        if (!mGo)
        {
            mGo = GoToNode(mSpot);
        }
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        mPhase = 1;
        bot.Log("over the Scrabs' floor: chanting to take one over");
    }
    if (mPhase == 1)
    {
        if (!me)
        {
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        }
        // Which one it is: the entity it is nearest
        s32 best = 0;
        for (const Entity& e : world.mEntities)
        {
            if (e.mKind == EntityKind::eScrab && onFloor(e) && !world.mScrabsKilled.count(e.mId) && (mSelf == 0 || std::abs(e.CentreX() - me->mX) < best))
            {
                mSelf = e.mId;
                best = std::abs(e.CentreX() - me->mX);
            }
        }
        bot.Log("in Scrab #" + std::to_string(mSelf) + " at " + std::to_string(me->mX) + "," + std::to_string(me->mY));
        mPhase = 2;
        return TaskStep::Do(Prim::Wait(6));
    }
    if (mPhase == 4)
    {
        if (!me)
        {
            bot.Log("out of the Scrab, shut in at " + std::to_string(mTrapX));
            return TaskStep::Done();
        }
        return TaskStep::Do(Prim::Wait(20, Btn::eChant));
    }
    if (!me)
    {
        return TaskStep::Failed("lost the Scrab");
    }
    const auto hold = [&](s32 x, u32 extra, s32 frames) -> TaskStep
    {
        return TaskStep::Do(Prim::Wait(frames, static_cast<u32>(x > me->mX ? Btn::eRight : Btn::eLeft) | extra));
    };
    if (mPhase == 2)
    {
        // What is left to kill, the nearest first: another Scrab that is about or one the map has on this floor
        // and that has not been seen dead; then the Fleeches
        const Entity* target = nullptr;
        for (const Entity& e : world.mEntities)
        {
            const bool scrab = e.mKind == EntityKind::eScrab && e.mId != mSelf && !e.mGone && !world.mScrabsKilled.count(e.mId);
            const bool fleech = e.mKind == EntityKind::eFleech && !e.mGone && !world.mFleechKilled.count(e.mId);
            if ((scrab || fleech) && onFloor(e) && std::find(mLeft.begin(), mLeft.end(), e.mId) == mLeft.end() && (!target || std::abs(e.CentreX() - me->mX) < std::abs(target->CentreX() - me->mX)))
            {
                target = &e;
            }
        }
        if (!target)
        {
            // All dead: to the far side of the foot switch that shuts a gate, coming at it through that gate
            for (const Entity& off : world.mEntities)
            {
                if (off.mKind != EntityKind::eFootSwitch || off.mOp != 1 || !onFloor(off))
                {
                    continue;
                }
                for (const NavFeature& gate : nav.Input().mFeatures)
                {
                    if (gate.mKind == FeatureKind::eBarrier && gate.mSolid && gate.mSwitch == off.mSwitch)
                    {
                        const s32 gx = (gate.mX1 + gate.mX2) / 2;
                        mTrapSwitch = off.mSwitch;
                        mTrapX = off.CentreX() + (off.CentreX() < gx ? -1 : 1) * 40;
                    }
                }
            }
            if (mTrapSwitch == 0)
            {
                return TaskStep::Failed("no gate to shut the Scrab behind");
            }
            bot.Log("the floor is clear: the Scrab to " + std::to_string(mTrapX) + " behind the gate of switch " + std::to_string(mTrapSwitch));
            mPhase = 3;
            return TaskStep::Do(Prim::Wait(4));
        }
        const bool isScrab = target->mKind == EntityKind::eScrab;
        // Where it is now, if it is about
        s32 tx = target->CentreX();
        bool seen = false;
        if (isScrab)
        {
            for (const ScrabAt& other : world.mScrabs)
            {
                if (!other.mPossessed && std::abs(other.mY - floorY) <= 30 && (!seen || std::abs(other.mX - me->mX) < std::abs(tx - me->mX)))
                {
                    tx = other.mX;
                    seen = true;
                }
            }
        }
        else
        {
            for (const std::vector<RockAt>* list : {&world.mFleechesAsleep, &world.mFleechesAwake})
            {
                for (const RockAt& f : *list)
                {
                    if (std::abs(f.mY - floorY) <= 160 && std::abs(f.mX - target->CentreX()) <= 400 && (!seen || std::abs(f.mX - me->mX) < std::abs(tx - me->mX)))
                    {
                        tx = f.mX;
                        seen = true;
                    }
                }
            }
        }
        const s32 dist = std::abs(tx - me->mX);
        if (!seen && dist < 150)
        {
            // Where it should be and nothing there: dead
            bot.Log(std::string(isScrab ? "Scrab #" : "Fleech #") + std::to_string(target->mId) + " is dead");
            if (isScrab)
            {
                bot.ScrabKilled(target->mId);
            }
            else
            {
                bot.FleechKilled(target->mId);
            }
            mCharged = false;
            return TaskStep::Do(Prim::Wait(10));
        }
        // A Fleech that has got up onto a ledge is out of the Scrab's reach for good
        bool upOut = false;
        if (!isScrab && seen)
        {
            for (const std::vector<RockAt>* list : {&world.mFleechesAsleep, &world.mFleechesAwake})
            {
                for (const RockAt& f : *list)
                {
                    upOut = upOut || (f.mX == tx && f.mY < floorY - 40);
                }
            }
        }
        if (upOut)
        {
            bot.Log("Fleech #" + std::to_string(target->mId) + " has climbed out of the Scrab's reach: left");
            mLeft.push_back(target->mId);
            return TaskStep::Do(Prim::Wait(4));
        }
        // (a Fleech that sleeps is come at like a Scrab, with the power and at a run: it wakes as the gate before
        // it opens and is up the nearest hoist in forty frames. One that is awake and near is stamped on.)
        if (!isScrab && (dist <= 60 || mFleechRuns >= 2))
        {
            if (dist > 8)
            {
                return hold(tx, 0, dist > 40 ? 4 : 1);
            }
            if (++mStamps > 60)
            {
                return TaskStep::Failed("the Scrab could not kill the Fleech");
            }
            return TaskStep::Do(Prim::Wait(mStamps % 2 ? 3 : 14, mStamps % 2 ? static_cast<u32>(Btn::eDown) : 0u));
        }
        if (!isScrab && mCharged && dist <= 110)
        {
            mFleechRuns++;
        }
        // Another Scrab: the howl first, from out of its reach, then at it at a run, spinning
        if (!mCharged)
        {
            if (seen && dist < 180)
            {
                // (too near to stand and howl: back off)
                return TaskStep::Do(Prim::Wait(6, static_cast<u32>(tx > me->mX ? Btn::eLeft : Btn::eRight) | Btn::eRun));
            }
            if (!seen && dist > 260)
            {
                return hold(tx, 0, 4);
            }
            if (seen && dist > 320)
            {
                return hold(tx, 0, 4);
            }
            // (the howl is asked for with a press while it stands, and the power is there when it has howled)
            if (mHowl == 0)
            {
                if (me->mMotion != 0)
                {
                    return TaskStep::Do(Prim::Wait(2));
                }
                mHowl = 1;
                bot.Log("the Scrab howls for its power, " + std::to_string(dist) + " from Scrab #" + std::to_string(target->mId));
                return TaskStep::Do(Prim::Wait(2, Btn::eSpeak1));
            }
            if (mHowl == 1)
            {
                mHowl = me->mMotion == 26 ? 2 : (++mHowlWait > 20 ? 0 : 1);
                mHowlWait = mHowl == 1 ? mHowlWait : 0;
                return TaskStep::Do(Prim::Wait(1));
            }
            if (me->mMotion != 0 && ++mHowlWait < 80)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            mHowl = 0;
            mHowlWait = 0;
            mCharged = true;
            mAttacks = 0;
            return TaskStep::Do(Prim::Wait(1));
        }
        if (++mAttacks > 40)
        {
            mCharged = false;
            return TaskStep::Do(Prim::Wait(4));
        }
        if (dist > 110)
        {
            return hold(tx, Btn::eRun, 2);
        }
        mCharged = false;
        return hold(tx, Btn::eRun | Btn::eThrow, 34);
    }
    // To the trap: through the gate its other foot switch opens, over the one that shuts it
    if (std::abs(mTrapX - me->mX) > 12)
    {
        return hold(mTrapX, 0, std::abs(mTrapX - me->mX) > 40 ? 4 : 1);
    }
    bool shut = true;
    for (const NavFeature& gate : nav.Input().mFeatures)
    {
        if (gate.mKind == FeatureKind::eBarrier && gate.mSolid && gate.mSwitch == mTrapSwitch)
        {
            shut = world.Switch(gate.mSwitch) != (gate.mStartClosed ? 1 : 0);
        }
    }
    if (!shut)
    {
        return TaskStep::Failed("the gate did not shut behind the Scrab");
    }
    bot.ScrabParked(mSelf, me->mX);
    mPhase = 4;
    return TaskStep::Do(Prim::Wait(10));
}

TaskStep ScrabKillTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* fleech = world.Find(mFleech);
    const Entity* home = world.Find(mScrab);
    const Entity* lever = world.Find(mLever);
    if (!fleech || !home || !lever)
    {
        return TaskStep::Failed("no such entity");
    }
    s32 open = 1;
    for (const NavFeature& f : nav.Input().mFeatures)
    {
        if (f.mKind == FeatureKind::eBarrier && f.mSolid && f.mSwitch == lever->mSwitch)
        {
            open = f.mStartClosed ? 1 : 0;
        }
    }
    const ScrabAt* me = nullptr;
    for (const ScrabAt& scrab : world.mScrabs)
    {
        me = scrab.mPossessed ? &scrab : me;
    }
    if (++mFrames > 4000)
    {
        return TaskStep::Failed("the Scrab took too long");
    }
    // Along the floor as the Scrab: walking while a direction is held, and it runs on a little when let go
    const auto walkTo = [&](s32 x, s32 within, bool& there, bool far = false) -> TaskStep
    {
        const s32 dx = x - me->mX;
        there = std::abs(dx) <= within;
        return TaskStep::Do(Prim::Wait(there ? 6 : (std::abs(dx) > 40 ? 4 : 1), there ? 0u : static_cast<u32>(dx > 0 ? Btn::eRight : Btn::eLeft)));
    };
    switch (mPhase)
    {
        case 0:
        case 6:
        {
            const s32 want = mPhase == 0 ? open : 1 - open;
            if (world.Switch(lever->mSwitch) == want)
            {
                mUse.reset();
                if (mPhase == 6)
                {
                    return TaskStep::Done();
                }
                mPhase = 1;
                bot.Log("the gate is open: chanting to take the Scrab over");
                return TaskStep::Do(Prim::Wait(2));
            }
            if (!mUse)
            {
                mUse = std::make_unique<UseTask>(mLever, want);
            }
            TaskStep step = mUse->Next(bot, lastFailed);
            if (step.mKind == TaskStep::Kind::eDone)
            {
                mUse.reset();
                return TaskStep::Do(Prim::Wait(2));
            }
            return step;
        }
        case 1:
            if (me)
            {
                mPhase = 2;
                bot.Log("in the Scrab at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + ": to the Fleech at " + std::to_string(fleech->CentreX()));
                return TaskStep::Do(Prim::Wait(4));
            }
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        case 2:
        case 3:
        {
            if (!me)
            {
                return TaskStep::Failed("lost the Scrab");
            }
            // Where the Fleech is now (awake it runs from the Scrab), or that it is gone
            const RockAt* at = nullptr;
            for (const std::vector<RockAt>* list : {&world.mFleechesAsleep, &world.mFleechesAwake})
            {
                for (const RockAt& f : *list)
                {
                    if (f.mHalf == me->mHalf && std::abs(f.mY - me->mY) <= 160 && std::abs(f.mX - fleech->CentreX()) <= 500 && (!at || std::abs(f.mX - me->mX) < std::abs(at->mX - me->mX)))
                    {
                        at = &f;
                    }
                }
            }
            if (!at)
            {
                bot.Log("the Fleech is dead: the Scrab home to " + std::to_string(home->CentreX()));
                bot.FleechKilled(mFleech);
                mPhase = 4;
                return TaskStep::Do(Prim::Wait(4));
            }
            bool there = false;
            const TaskStep walk = walkTo(at->mX, 8, there);
            if (!there)
            {
                return walk;
            }
            if (++mStamps > 40)
            {
                return TaskStep::Failed("the Scrab could not kill the Fleech");
            }
            // (the stamp kills what its box is on, on the tenth frame of it)
            return TaskStep::Do(Prim::Wait(mStamps % 2 ? 3 : 14, mStamps % 2 ? static_cast<u32>(Btn::eDown) : 0u));
        }
        case 4:
        {
            if (!me)
            {
                return TaskStep::Failed("lost the Scrab");
            }
            bool there = false;
            const TaskStep walk = walkTo(home->CentreX(), 20, there);
            if (!there)
            {
                return walk;
            }
            mPhase = 5;
            return TaskStep::Do(Prim::Wait(10));
        }
        case 5:
            if (!me)
            {
                mPhase = 6;
                bot.Log("out of the Scrab: shutting the gate on it");
                return TaskStep::Do(Prim::Wait(20));
            }
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
    }
    return TaskStep::Failed("lost its place");
}

ScrabHuntTask::~ScrabHuntTask() = default;

// How many ways on and off its floor the stretch of it about x has, between the gates either side
static s32 StretchWays(const NavGraph& nav, s32 line, s32 x, s32 y, bool half, s32& lo, s32& hi)
{
    lo = -100000;
    hi = 100000;
    for (const NavFeature& g : nav.Input().mFeatures)
    {
        const s32 gx = (g.mX1 + g.mX2) / 2;
        if (g.mKind == FeatureKind::eBarrier && g.mSolid && g.mHalf == half && std::abs(g.mY2 - y) * (half ? 2 : 1) <= 30)
        {
            lo = gx < x ? std::max(lo, gx) : lo;
            hi = gx > x ? std::min(hi, gx) : hi;
        }
    }
    s32 ways = 0;
    for (const NavEdge& e : nav.Edges())
    {
        const NavNode& a = nav.Nodes()[e.mFrom];
        const NavNode& b = nav.Nodes()[e.mTo];
        const bool off = a.mLine == line && b.mLine != line && a.mX > lo && a.mX < hi;
        const bool on = b.mLine == line && a.mLine != line && b.mX > lo && b.mX < hi;
        ways += off || on ? 1 : 0;
    }
    return ways;
}

TaskStep ScrabHuntTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* home = world.Find(mScrab);
    if (!home)
    {
        return TaskStep::Failed("no such entity");
    }
    if (++mFrames > 6000)
    {
        return TaskStep::Failed("the Scrab took too long");
    }
    const s32 scale = home->mHalf ? 2 : 1;
    if (!mSet)
    {
        mSet = true;
        const ScrabAt* own = world.ScrabOf(mScrab);
        mHomeX = own ? own->mX : home->CentreX();
        const s32 under = nav.NodeAt(mHomeX, own ? own->mY : home->mY2, home->mHalf, 30);
        if (under < 0)
        {
            return TaskStep::Failed("the Scrab is on no floor");
        }
        mLine = nav.Nodes()[under].mLine;
        mFloorY = nav.Nodes()[under].mY;
        for (const Entity& f : world.mEntities)
        {
            if (mLed)
            {
                break;
            }
            const s32 at = f.mKind == EntityKind::eFleech && !f.mGone && f.mInputs[1] == 0 && f.mHalf == home->mHalf && world.mFleechKilled.count(f.mId) == 0 ? nav.NodeAt(f.CentreX(), f.mY2, f.mHalf, 30) : -1;
            if (at >= 0 && nav.Nodes()[at].mLine == mLine && !Domain::WallBetween(nav, mHomeX, f.CentreX(), mFloorY, home->mHalf))
            {
                mGroup.push_back(f.mId);
            }
        }
        if (mGroup.empty())
        {
            return TaskStep::Done();
        }
        mGo = GoToNode(mSpot);
    }
    const ScrabAt* me = nullptr;
    for (const ScrabAt& scrab : world.mScrabs)
    {
        me = scrab.mPossessed ? &scrab : me;
    }
    // Along the floor as the Scrab: walking while a direction is held, and it runs on a little when let go
    const auto walkTo = [&](s32 x, s32 within, bool& there, bool far = false) -> TaskStep
    {
        const s32 dx = x - me->mX;
        there = std::abs(dx) <= within;
        // (at a run while it is far: it slides on some way when let go)
        const u32 run = far && std::abs(dx) * scale > 200 ? static_cast<u32>(Btn::eRun) : 0u;
        return TaskStep::Do(Prim::Wait(there ? 6 : (std::abs(dx) * scale > 40 ? 4 : 1), there ? 0u : static_cast<u32>(dx > 0 ? Btn::eRight : Btn::eLeft) | run));
    };
    if (mPhase == 0)
    {
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        mPhase = 1;
        bot.Log("on Scrab #" + std::to_string(mScrab) + "'s screen: chanting to take it over when it is the one a chant takes");
    }
    if (mPhase == 1)
    {
        if (me)
        {
            if (me->mId != mScrab)
            {
                bot.Log("in Scrab #" + std::to_string(me->mId) + ", not #" + std::to_string(mScrab) + ": letting it go");
                mWrong = true;
                mPhase = 4;
                return TaskStep::Do(Prim::Wait(10));
            }
            bot.Log("in Scrab #" + std::to_string(mScrab) + " at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + ": to the " + std::to_string(mGroup.size()) + " Fleeches of its floor");
            mPhase = 2;
            return TaskStep::Do(Prim::Wait(4));
        }
        // The one a chant takes (Abe::FindObjectToPossess): on his screen, one of his size before any other, the
        // nearest of those
        const AbeState& abe = bot.Abe();
        const s32 cam = nav.CameraOf(abe.mX, abe.mY - 5);
        const ScrabAt* takes = nullptr;
        s64 best = 0;
        for (const ScrabAt& scrab : world.mScrabs)
        {
            if (nav.CameraOf(scrab.mX, scrab.mY - 5) != cam)
            {
                continue;
            }
            const s64 dx = scrab.mX - abe.mX;
            const s64 dy = scrab.mY - abe.mY;
            const s64 dist = dx * dx + dy * dy;
            const bool mine = scrab.mHalf == abe.mHalfScale;
            const bool had = takes && takes->mHalf == abe.mHalfScale;
            if (!takes || (mine && !had) || (mine == had && dist < best))
            {
                takes = &scrab;
                best = dist;
            }
        }
        if (takes && takes->mId == mScrab)
        {
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        }
        if (++mWaits > 150)
        {
            return TaskStep::Failed("Scrab #" + std::to_string(mScrab) + " is not the one a chant would take from here");
        }
        return TaskStep::Do(Prim::Wait(10));
    }
    if (mPhase == 4)
    {
        if (me)
        {
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        }
        if (mWrong)
        {
            return TaskStep::Failed("took over another Scrab");
        }
        if (mParked)
        {
            bot.ScrabParked(mScrab, mLeaveX);
        }
        bot.Log("out of the Scrab, left at " + std::to_string(mLeaveX) + (mParked ? " for a gate to be shut on it" : ", where it was"));
        return TaskStep::Done();
    }
    if (!me)
    {
        return TaskStep::Failed("lost the Scrab");
    }
    if (mPhase == 2)
    {
        // The nearest Fleech it can walk to (awake they run from it and cower)
        const RockAt* at = nullptr;
        for (const std::vector<RockAt>* list : {&world.mFleechesAsleep, &world.mFleechesAwake})
        {
            for (const RockAt& f : *list)
            {
                if (f.mHalf == me->mHalf && std::abs(f.mY - mFloorY) * scale <= 40 && !Domain::WallBetween(nav, me->mX, f.mX, mFloorY, me->mHalf) && !Domain::GateBetween(nav, world, me->mX, f.mX, mFloorY, me->mHalf) && (!at || std::abs(f.mX - me->mX) < std::abs(at->mX - me->mX)))
                {
                    at = &f;
                }
            }
        }
        bool there = false;
        if (!at && mLed)
        {
            // Led to it: the ones that were by it are the ones there were. None left once it has spun through them
            // is all of them dead; none before it has is none that came.
            if (mStamps == 0)
            {
                if (++mNone > 150)
                {
                    return TaskStep::Failed("no Fleech came to the Scrab");
                }
                return TaskStep::Do(Prim::Wait(2));
            }
            if (++mNone < 20)
            {
                return TaskStep::Do(Prim::Wait(2));
            }
            for (u32 id : mGroup)
            {
                bot.FleechKilled(id);
            }
            mParked = false;
            mLeaveX = mHomeX;
            bot.Log("the Fleeches led to it are dead: the Scrab home to " + std::to_string(mLeaveX));
            mPhase = 3;
            return TaskStep::Do(Prim::Wait(4));
        }
        mNone = 0;
        if (!at)
        {
            // None to be seen: to where the farthest of them was, to be sure of it
            s32 far = mHomeX;
            for (u32 id : mGroup)
            {
                const Entity* f = world.Find(id);
                far = f && std::abs(f->CentreX() - mHomeX) > std::abs(far - mHomeX) ? f->CentreX() : far;
            }
            const TaskStep walk = walkTo(far, 30 / scale, there);
            if (!there)
            {
                return walk;
            }
            for (u32 id : mGroup)
            {
                bot.FleechKilled(id);
            }
            // Where to leave it
            s32 lo = 0;
            s32 hi = 0;
            const s32 waysHome = StretchWays(nav, mLine, mHomeX, mFloorY, me->mHalf, lo, hi);
            const s32 waysHere = StretchWays(nav, mLine, me->mX, mFloorY, me->mHalf, lo, hi);
            // (not where there is something he has still to work: a way out he can take unseen, that he cannot)
            bool work = false;
            for (const Entity& e : world.mEntities)
            {
                const bool worked = e.mKind == EntityKind::eLever || e.mKind == EntityKind::ePullRing || e.mKind == EntityKind::eWorkWheel || e.mKind == EntityKind::eHandStone || (e.mKind == EntityKind::eSlapLock && world.mLocksSlapped.count(e.mId) == 0);
                const s32 at = worked && e.mHalf == me->mHalf ? nav.NodeAt(e.CentreX(), e.mY2, e.mHalf, 30) : -1;
                work = work || (at >= 0 && nav.Nodes()[at].mLine == mLine && e.CentreX() > lo && e.CentreX() < hi);
            }
            bool gate = false;
            for (const NavFeature& g : nav.Input().mFeatures)
            {
                const s32 gx = (g.mX1 + g.mX2) / 2;
                gate = gate || (g.mKind == FeatureKind::eBarrier && g.mSolid && g.mHalf == me->mHalf && std::abs(g.mY2 - mFloorY) * scale <= 30 && gx > std::min(mHomeX, me->mX) && gx < std::max(mHomeX, me->mX));
            }
            mParked = gate && !work && waysHere < waysHome;
            mLeaveX = mParked ? far : mHomeX;
            bot.Log("the Fleeches are dead: the Scrab " + std::string(mParked ? "left here at " : "home to ") + std::to_string(mLeaveX) + " (ways on and off: " + std::to_string(waysHere) + " here, " + std::to_string(waysHome) + " at home)");
            mPhase = 3;
            return TaskStep::Do(Prim::Wait(4));
        }
        // Never up to one at a walk: a Fleech with a wall behind it and the Scrab within three cells puts its tongue
        // out and eats it (Fleech's ScaredBrain, eCornered). The howl first, from out of that reach, for the power
        // to spin; then at it at a run, spinning through: the spin kills what it touches, one a frame.
        const s32 dist = std::abs(at->mX - me->mX) * scale;
        const auto hold = [&](u32 extra, s32 frames, bool away = false) -> TaskStep
        {
            return TaskStep::Do(Prim::Wait(frames, static_cast<u32>((at->mX > me->mX) != away ? Btn::eRight : Btn::eLeft) | extra));
        };
        if (!mCharged)
        {
            if (dist < 150)
            {
                return hold(Btn::eRun, 6, true);
            }
            if (dist > 280)
            {
                return hold(dist > 500 ? static_cast<u32>(Btn::eRun) : 0u, 4);
            }
            // (the howl is asked for with a press while it stands, and the power is there when it has howled)
            if (mHowl == 0)
            {
                if (me->mMotion != 0)
                {
                    return TaskStep::Do(Prim::Wait(2));
                }
                mHowl = 1;
                bot.Log("the Scrab howls for its power, " + std::to_string(dist / scale) + " from the nearest Fleech at " + std::to_string(at->mX));
                return TaskStep::Do(Prim::Wait(2, Btn::eSpeak1));
            }
            if (mHowl == 1)
            {
                mHowl = me->mMotion == 26 ? 2 : (++mHowlWait > 20 ? 0 : 1);
                mHowlWait = mHowl == 1 ? mHowlWait : 0;
                return TaskStep::Do(Prim::Wait(1));
            }
            if (me->mMotion != 0 && ++mHowlWait < 80)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            mHowl = 0;
            mHowlWait = 0;
            mCharged = true;
            mAttacks = 0;
            return TaskStep::Do(Prim::Wait(1));
        }
        if (dist > 110 && ++mAttacks < 80)
        {
            return hold(Btn::eRun, 2);
        }
        if (++mStamps > 12)
        {
            return TaskStep::Failed("the Scrab could not kill the Fleeches");
        }
        mCharged = false;
        return hold(Btn::eRun | Btn::eThrow, 34);
    }
    bool there = false;
    const TaskStep walk = walkTo(mLeaveX, 20 / scale, there, true);
    if (!there)
    {
        if (Domain::GateBetween(nav, world, me->mX, mLeaveX, mFloorY, me->mHalf))
        {
            return TaskStep::Failed("a gate shut between the Scrab and where it was to be left");
        }
        return walk;
    }
    mPhase = 4;
    return TaskStep::Do(Prim::Wait(10));
}

bool SlogGuardTask::Find(const Bot& bot, Setup& setup)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const s32 cur = bot.AbeNode();
    if (cur < 0)
    {
        return false;
    }
    const NavNode& at = nav.Nodes()[cur];
    // Slogs up and about on his floor, a gate between
    s32 side = 0;
    for (const SlogAt& slog : world.mSlogs)
    {
        if (!slog.mAsleep && std::abs(slog.mY - at.mY) <= 30 && std::abs(slog.mX - at.mX) < 700 && Domain::GateBetween(nav, world, slog.mX, at.mX, at.mY, at.mHalf))
        {
            side = slog.mX > at.mX ? 1 : -1;
        }
    }
    // Or the place they are let out of, its switch on or about to be
    for (const std::array<s32, 3>& spawner : world.mSlogSpawners)
    {
        if (std::abs(spawner[1] - at.mY) <= 30 && std::abs(spawner[0] - at.mX) < 700 && spawner[2] > 1 && (world.Switch(spawner[2]) != 0 || world.mSoonOn == spawner[2]) &&
            Domain::GateBetween(nav, world, spawner[0], at.mX, at.mY, at.mHalf))
        {
            side = spawner[0] > at.mX ? 1 : -1;
        }
    }
    if (side == 0)
    {
        return false;
    }
    for (const SligAt& slig : world.mSligs)
    {
        if (slig.mPossessed || slig.mHalf != at.mHalf || std::abs(slig.mY - at.mY) <= 45 || nav.CameraOf(slig.mX, slig.mY - 5) != nav.CameraOf(at.mX, at.mY - 5))
        {
            continue;
        }
        // A teleporter from its floor to his, that comes out on his side of the gate
        for (const NavEdge& e : nav.Edges())
        {
            const NavNode& from = nav.Nodes()[e.mFrom];
            const NavNode& to = nav.Nodes()[e.mTo];
            if (e.mKind != EdgeKind::eTeleport || from.mHalf != at.mHalf || std::abs(from.mY - slig.mY) > 20 || std::abs(to.mY - at.mY) > 20 || to.mLine != at.mLine ||
                Domain::GateBetween(nav, world, to.mX, at.mX, at.mY, at.mHalf) || Domain::WallBetween(nav, slig.mX, from.mX, from.mY, from.mHalf))
            {
                continue;
            }
            // (Abe a cell behind where it comes out, away from the gate, if there is one)
            const s32 behind = nav.NodeAt(to.mX - side * NavGraph::GridStep(at.mHalf), at.mY, at.mHalf, 20);
            setup.mSligX = slig.mX;
            setup.mSligY = slig.mY;
            setup.mPlateX = from.mX;
            setup.mArriveX = to.mX;
            setup.mSide = side;
            setup.mStand = behind >= 0 && nav.Nodes()[behind].mLine == at.mLine ? behind : e.mTo;
            return true;
        }
    }
    return false;
}

TaskStep SlogGuardTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const SligAt* me = nullptr;
    for (const SligAt& slig : world.mSligs)
    {
        if (slig.mPossessed)
        {
            me = &slig;
        }
    }
    if (mPhase == 0)
    {
        if (!mGo && bot.AbeNode() != mSetup.mStand)
        {
            mGo = GoToNode(mSetup.mStand);
            mGo->SetHurry(true);
        }
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        mPhase = 1;
        mFrames = 0;
    }
    if (mPhase == 1)
    {
        if (me)
        {
            bot.Log("in the Slig at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + ": to the teleporter at " + std::to_string(mSetup.mPlateX));
            mPhase = 2;
            mFrames = 0;
        }
        else
        {
            mFrames += 20;
            if (mFrames > 500)
            {
                return TaskStep::Failed("the Slig was not taken over");
            }
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        }
    }
    if (!me)
    {
        if (mPhase == 5)
        {
            return TaskStep::Done();
        }
        return TaskStep::Failed("the Slig is gone");
    }
    const s32 floorY = nav.Nodes()[mSetup.mStand].mY;
    const s32 cell = NavGraph::GridStep(me->mHalf);
    if (mPhase == 2)
    {
        if (std::abs(me->mY - floorY) <= 30)
        {
            mPhase = 3;
            mFrames = 0;
        }
        else
        {
            mFrames += 4;
            if (mFrames > 600)
            {
                return TaskStep::Failed("the Slig never got to its teleporter");
            }
            // At a run to the plate, and stood on it until it is taken
            const s32 dx = mSetup.mPlateX - me->mX;
            if (std::abs(dx) <= cell / 2)
            {
                return TaskStep::Do(Prim::Wait(4));
            }
            return TaskStep::Do(Prim::Wait(4, (dx < 0 ? Btn::eLeft : Btn::eRight) | (std::abs(dx) > 3 * cell ? static_cast<u32>(Btn::eRun) : 0u)));
        }
    }
    // The nearest Slog on this floor that it has a clear shot at
    const s32 post = abe.mX + mSetup.mSide * 3 * cell;
    s32 nearest = -1;
    bool any = false;
    for (const SlogAt& slog : world.mSlogs)
    {
        if (slog.mAsleep || std::abs(slog.mY - floorY) > 30 || (slog.mX - abe.mX) * mSetup.mSide <= 0 || std::abs(slog.mX - abe.mX) > 900 || Domain::WallBetween(nav, slog.mX, abe.mX, floorY, me->mHalf))
        {
            continue;
        }
        any = true;
        const s32 gap = (slog.mX - me->mX) * mSetup.mSide;
        if (gap > 0 && !Domain::GateBetween(nav, world, slog.mX, me->mX, floorY, me->mHalf) && (nearest < 0 || gap < nearest))
        {
            nearest = gap;
        }
    }
    const u32 forward = mSetup.mSide > 0 ? Btn::eRight : Btn::eLeft;
    const bool facing = me->mFacingLeft == (mSetup.mSide < 0);
    const s32 short_ = (post - me->mX) * mSetup.mSide;
    if (mPhase == 3)
    {
        mFrames += 4;
        if ((short_ <= cell / 2 && facing) || mFrames > 300)
        {
            bot.Log("the Slig is at its post at " + std::to_string(me->mX) + "," + std::to_string(me->mY));
            mPhase = 4;
            mFrames = 0;
        }
        else if (nearest >= 0 && nearest < 300 && facing)
        {
            // (they are out already: from where it stands)
            mPhase = 4;
        }
        else
        {
            return TaskStep::Do(Prim::Wait(4, forward));
        }
    }
    if (mPhase == 4)
    {
        mFrames += 4;
        if (nearest >= 0 && nearest < 420)
        {
            mQuiet = 0;
            mShot++;
            if (!facing)
            {
                return TaskStep::Do(Prim::Wait(4, forward));
            }
            return TaskStep::Do(Prim::Wait(4, Btn::eThrow));
        }
        // (back to its post once the recoil has walked it towards Abe)
        if (short_ > cell + cell / 2 || !facing)
        {
            return TaskStep::Do(Prim::Wait(4, forward));
        }
        mQuiet = any ? 0 : mQuiet + 4;
        if ((mShot > 0 && mQuiet > 160) || mFrames > 4000)
        {
            bot.Log("no more Slogs: " + std::to_string(mShot) + " bursts shot, the Slig is burst");
            mPhase = 5;
            mFrames = 0;
        }
        else
        {
            return TaskStep::Do(Prim::Wait(4));
        }
    }
    mFrames += 20;
    if (mFrames > 400)
    {
        return TaskStep::Failed("the Slig wouldn't burst");
    }
    return TaskStep::Do(Prim::Wait(20, Btn::eChant));
}

TaskStep SlogFleeTask::Next(Bot& bot, const std::string& lastFailed)
{
    const AbeState& abe = bot.Abe();
    // At a run, the way held from the air on, until the way off is a couple of cells ahead: the rest is the
    // pilot's (which would walk a stretch this short from a standstill)
    const s32 ahead = mLeft ? abe.mX - mFromX : mFromX - abe.mX;
    // (a run looks at what is held every other cell, and with nothing held there slides two cells to a stop: so
    // let go once the look four cells short is past, for the one two cells short. Measured: let go a frame after
    // that one he ran on to the next and stood two cells too far.)
    if (!mGo && (!abe.mOnGround || ahead > 88) && ++mHeld < 200)
    {
        return TaskStep::Do(Prim::Wait(abe.mOnGround ? std::max(1, std::min(8, (ahead - 88) / 7)) : 1, (mLeft ? Btn::eLeft : Btn::eRight) | Btn::eRun));
    }
    if (!mGo && abe.mMove == Move::eRun && ++mHeld < 260)
    {
        return TaskStep::Do(Prim::Wait(1));
    }
    if (!mGo)
    {
        const s32 refuge = mRefuge;
        mGo = std::make_unique<GoToTask>("off the Slog's floor", [refuge](s32 n)
                                         { return n == refuge; });
        mGo->SetHurry(true);
    }
    return mGo->Next(bot, lastFailed);
}

TaskStep HideTask::Next(Bot& bot, const std::string& /*lastFailed*/)
{
    // (clear for a little while, not for a frame: it turns round at the ends of its track)
    mClear = bot.FlierThreat() ? 0 : mClear + 1;
    mFrames += 10;
    if (mClear >= 3)
    {
        bot.Log("the flying Slig has gone by after " + std::to_string(mFrames) + " frames");
        return TaskStep::Done();
    }
    if (mFrames % 500 == 0)
    {
        bot.Log("still out of the flying Slig's sight: " + bot.FlierWhy());
    }
    if (mFrames > 4000)
    {
        return TaskStep::Failed("the flying Slig never went by");
    }
    return TaskStep::Do(Prim::Wait(10));
}

TaskStep ExitTask::Next(Bot& bot, const std::string& lastFailed)
{
    const AbeState& abe = bot.Abe();
    if (mStartPath < 0)
    {
        mStartPath = abe.mPath;
        mStartLevel = abe.mLevel;
    }
    if (abe.mPath != mStartPath || abe.mLevel != mStartLevel)
    {
        return TaskStep::Done();
    }
    NavGraph& nav = bot.Nav();
    if (mExit < 0 || mExit >= static_cast<s32>(nav.Exits().size()))
    {
        return TaskStep::Failed("no such exit");
    }
    const NavExit& exit = nav.Exits()[mExit];
    if (exit.mNode < 0)
    {
        // The game does it
        if (++mTries > 40)
        {
            return TaskStep::Failed("the level loader didn't go off");
        }
        return TaskStep::Do(Prim::Wait(15));
    }
    if (!mArrived)
    {
        if (!mGo)
        {
            mGo = GoToNode(exit.mNode);
        }
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        mArrived = true;
    }
    const NavFeature& f = nav.Input().mFeatures[exit.mFeature];
    // (a door a slapped lock opens: until its ghost is at its tomb)
    if (f.mKind == FeatureKind::eDoor && f.mStartClosed && f.mSwitch > 1 && bot.GetWorld().Switch(f.mSwitch, true) == 0 && bot.GetWorld().Switch(f.mSwitch) != 0 && mGhostWaits < 150)
    {
        mGhostWaits++;
        return TaskStep::Do(Prim::Wait(6));
    }
    if (++mTries > 5)
    {
        return TaskStep::Failed("the exit didn't lead out of the path");
    }
    if (f.mKind == FeatureKind::ePathExit)
    {
        // Walking on through it does it (one under the end of his floor: off that end)
        if (f.mY1 > abe.mY + 20 && bot.AbeNode() >= 0)
        {
            bool floorLeft = false;
            bool floorRight = false;
            const NavNode& at = nav.Nodes()[bot.AbeNode()];
            for (s32 i = at.mFirstEdge; i < at.mFirstEdge + at.mEdgeCount; i++)
            {
                const NavEdge& e = nav.Edges()[i];
                if (e.mKind == EdgeKind::eWalk)
                {
                    (e.mLeft ? floorLeft : floorRight) = true;
                }
            }
            if (floorLeft != floorRight)
            {
                return TaskStep::Do(Prim::Wait(40, floorRight ? Btn::eLeft : Btn::eRight));
            }
        }
        return TaskStep::Do(Prim::Wait(40, abe.mFacingLeft ? Btn::eLeft : Btn::eRight));
    }
    return TaskStep::Do(Prim::Press(Btn::eUp));
}

ScrabLureTask::~ScrabLureTask() = default;
ScrabPenTask::~ScrabPenTask() = default;

TaskStep ScrabLureTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const NavNode& top = nav.Nodes()[mTop];
    s32 foot = -1;
    bool faceLeft = false;
    for (const NavEdge& e : nav.Edges())
    {
        if (e.mKind == EdgeKind::eHoistUp && e.mTo == mTop)
        {
            foot = e.mFrom;
            faceLeft = e.mLeft;
        }
    }
    if (foot < 0)
    {
        return TaskStep::Failed("no hoist up to the ledge");
    }
    if (++mFrames > 6000)
    {
        return TaskStep::Failed("the Scrabs would not be lured");
    }
    // The ones still up there: how near the nearest is, and whether one is coming at a run
    const s32 zone = Domain::ScrabZone(nav, mTop);
    s32 upThere = 0;
    s32 nearest = 100000;
    bool coming = false;
    for (const Domain::LooseScrab& scrab : Domain::LooseScrabs(world))
    {
        const s32 at = nav.NodeAt(scrab.mX, scrab.mY, scrab.mHalf, 30);
        if (at < 0 || Domain::ScrabZone(nav, at) != zone)
        {
            continue;
        }
        upThere++;
        nearest = std::min(nearest, std::abs(scrab.mX - top.mX));
        const ScrabAt* live = world.ScrabOf(scrab.mId);
        // (Scrab.hpp: 2 runs, 10 from a stand into a run, 16 from a walk into one)
        coming = coming || (live && (live->mMotion == 2 || live->mMotion == 10 || live->mMotion == 16) && live->mFacesLeft == (top.mX < live->mX));
    }
    const bool hanging = abe.mMove == Move::eHang;
    const bool onTop = abe.mMove == Move::eIdle && std::abs(abe.mY - top.mY) <= 5 && std::abs(abe.mX - top.mX) <= 8;
    switch (mPhase)
    {
        case 0:
        {
            if (upThere == 0)
            {
                return TaskStep::Done();
            }
            if (!mGo && bot.AbeNode() != foot)
            {
                mGo = GoToNode(foot);
            }
            TaskStep step;
            if (!RunGo(mGo, bot, lastFailed, step))
            {
                return step;
            }
            if (step.mKind == TaskStep::Kind::eFailed)
            {
                return step;
            }
            mPhase = 1;
            return TaskStep::Do(Prim::Wait(1));
        }
        case 1:
            if (hanging)
            {
                mPhase = 2;
                return TaskStep::Do(Prim::Wait(1));
            }
            if (!abe.Settled())
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (++mTries > 8)
            {
                return TaskStep::Failed("could not get up to hang from the ledge");
            }
            if (abe.mFacingLeft != faceLeft)
            {
                return TaskStep::Do(Prim::Face(faceLeft));
            }
            return TaskStep::Do(Prim::Press(Btn::eUp));
        case 2:
            // Not seen while he hangs (Scrab::CanSeeAbe). Up when the nearest is far enough off for him to be down
            // again before it is here: measured, the climb takes 21 frames, back down to hang 24, and a Scrab
            // runs 7.6 pixels a frame
            if (!hanging)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (upThere == 0)
            {
                mPhase = 5;
                return TaskStep::Do(Prim::Press(Btn::eUp));
            }
            if (nearest < 300)
            {
                return TaskStep::Do(Prim::Wait(2));
            }
            bot.Log("up to be seen by the Scrab, " + std::to_string(nearest) + " off");
            mPhase = 3;
            mWaits = 0;
            return TaskStep::Do(Prim::Press(Btn::eUp));
        case 3:
            if (!onTop)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (upThere == 0)
            {
                return TaskStep::Done();
            }
            if (coming || nearest < 200 || ++mWaits > 240)
            {
                if (!coming && nearest >= 200 && ++mTries > 8)
                {
                    return TaskStep::Failed("the Scrab does not come at him");
                }
                bot.Log(coming ? "the Scrab comes at a run: down to hang" : "down to hang again");
                mPhase = 4;
                mWaits = 0;
                return TaskStep::Do(Prim::Press(Btn::eDown));
            }
            return TaskStep::Do(Prim::Wait(1));
        case 4:
            if (!hanging)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (upThere == 0)
            {
                // Up at once: it lands a cell from where he hangs
                bot.Log("the Scrab has gone over the edge: up");
                mPhase = 5;
                mWaits = 0;
                return TaskStep::Do(Prim::Press(Btn::eUp));
            }
            if (!coming && ++mWaits > 30)
            {
                // It gave up, or there is another: again
                mPhase = 2;
            }
            return TaskStep::Do(Prim::Wait(1));
        default:
            if (onTop)
            {
                if (upThere != 0)
                {
                    mPhase = 3;
                    mWaits = 0;
                    return TaskStep::Do(Prim::Wait(1));
                }
                return TaskStep::Done();
            }
            if (hanging && ++mWaits > 40)
            {
                mWaits = 0;
                return TaskStep::Do(Prim::Press(Btn::eUp));
            }
            return TaskStep::Do(Prim::Wait(1));
    }
}

TaskStep ScrabPenTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const Entity* lever = world.Find(mLever);
    if (!lever || mFeature < 0 || mFeature >= static_cast<s32>(nav.Input().mFeatures.size()))
    {
        return TaskStep::Failed("no such lever or gate");
    }
    const NavFeature& gate = nav.Input().mFeatures[mFeature];
    const s32 gx = (gate.mX1 + gate.mX2) / 2;
    const s32 shutValue = gate.mStartClosed ? 0 : 1;
    const bool shut = world.Switch(gate.mSwitch) == shutValue;
    if (++mFrames > 9000)
    {
        return TaskStep::Failed("the Scrabs never were all on one side of the gate");
    }
    if (mUse)
    {
        TaskStep step = mUse->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        mUse.reset();
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        return TaskStep::Do(Prim::Wait(2));
    }
    // At the lever, to be quick with it
    const s32 at = Domain::UseNode(nav, *lever, nullptr);
    if (at < 0)
    {
        return TaskStep::Failed("no place to pull the lever from");
    }
    if (!mGo && bot.AbeNode() != at && bot.Abe().Settled() && std::abs(bot.Abe().mX - nav.Nodes()[at].mX) > 2 * NavGraph::GridStep(nav.Nodes()[at].mHalf))
    {
        mGo = GoToNode(at);
    }
    {
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
    }
    // The Scrabs of the two stretches the gate parts
    const s32 cell = NavGraph::GridStep(gate.mHalf);
    const s32 nl = nav.NodeAt(gate.mX1 - cell / 2, gate.mY2, gate.mHalf, 30);
    const s32 nr = nav.NodeAt(gate.mX2 + cell / 2, gate.mY2, gate.mHalf, 30);
    if (nl < 0 || nr < 0)
    {
        return TaskStep::Failed("no floor on both sides of the gate");
    }
    const s32 zl = Domain::ScrabZone(nav, nl);
    const s32 zr = Domain::ScrabZone(nav, nr);
    s32 count = 0;
    s32 left = 0;
    s32 right = 0;
    bool clearOfGate = true;
    bool onScreen = true;
    const s32 cam = nav.CameraOf(nav.Nodes()[at].mX, nav.Nodes()[at].mY - 5);
    for (const Domain::LooseScrab& scrab : Domain::LooseScrabs(world))
    {
        const s32 under = nav.NodeAt(scrab.mX, scrab.mY, scrab.mHalf, 30);
        if (under < 0 || (Domain::ScrabZone(nav, under) != zl && Domain::ScrabZone(nav, under) != zr))
        {
            // (one in the air on its way down counts as there, and as not yet where it is wanted)
            if (scrab.mHalf == gate.mHalf && std::abs(scrab.mY - gate.mY2) <= 120 && under < 0 && std::abs(scrab.mX - gx) < 1200)
            {
                count++;
                clearOfGate = false;
            }
            continue;
        }
        count++;
        (scrab.mX < gx ? left : right)++;
        const ScrabAt* live = world.ScrabOf(scrab.mId);
        // Far enough from the gate not to be through it before it is shut: the pull takes some 20 frames, and
        // one that comes this way walks 2.3 pixels a frame or runs 7.6
        const bool away = live && live->mFacesLeft == (scrab.mX < gx);
        clearOfGate = clearOfGate && std::abs(scrab.mX - gx) >= (away ? 50 : 220);
        onScreen = onScreen && nav.CameraOf(scrab.mX, scrab.mY - 5) == cam;
    }
    const bool asWanted = mLeft ? right == 0 : left == 0;
    if (shut)
    {
        if (count == 0 || asWanted)
        {
            return TaskStep::Done();
        }
        // One on each side: opened while both are on his screen they fight, and one is left
        if (left != 0 && right != 0 && !onScreen && ++mHold < 1500)
        {
            if (!mSaid)
            {
                mSaid = true;
                bot.Log("at the lever until the Scrabs on both sides of the gate are on this screen: they fight then");
            }
            return TaskStep::Do(Prim::Wait(2));
        }
        bot.Log("opening the gate at " + std::to_string(gx) + " with " + std::to_string(left) + " Scrabs left of it and " + std::to_string(right) + " right");
        mUse = std::make_unique<UseTask>(mLever, 1 - shutValue);
        return TaskStep::Do(Prim::Wait(1));
    }
    if (count != 0 && asWanted && clearOfGate)
    {
        bot.Log("shutting the gate at " + std::to_string(gx) + " on " + std::to_string(count) + " Scrabs " + (mLeft ? "left" : "right") + " of it");
        mUse = std::make_unique<UseTask>(mLever, shutValue);
        return TaskStep::Do(Prim::Wait(1));
    }
    if (count == 0)
    {
        return TaskStep::Done();
    }
    return TaskStep::Do(Prim::Wait(2));
}

FleechLureTask::~FleechLureTask()
{
    if (mKept)
    {
        mKept->KeepTo({});
    }
}

void FleechLureTask::Keep(Bot& bot, const std::set<s32>& edges)
{
    bot.KeepTo(edges);
    mKept = edges.empty() ? nullptr : &bot;
}

// The cell a Fleech is on, or the nearest under it while it climbs or falls
static s32 FleechNode(const NavGraph& nav, const RockAt& f)
{
    const s32 at = nav.NodeAt(f.mX, f.mY, f.mHalf, 30);
    if (at >= 0)
    {
        return at;
    }
    s32 best = -1;
    s32 bestOff = 100000;
    for (std::size_t i = 0; i < nav.Nodes().size(); i++)
    {
        const NavNode& n = nav.Nodes()[i];
        const s32 off = std::abs(n.mX - f.mX) * 3 + std::abs(n.mY - f.mY);
        if (n.mHalf == f.mHalf && std::abs(n.mX - f.mX) <= NavGraph::GridStep(f.mHalf) && off < bestOff)
        {
            bestOff = off;
            best = static_cast<s32>(i);
        }
    }
    return best;
}

// What an edge costs him as the switches stand (own: a switch taken to be on, the one the thing used sets)
static NavGraph::EdgeCost LureCost(const Bot& bot, s32 own)
{
    const NavGraph* nav = &bot.Nav();
    const World* world = &bot.GetWorld();
    return [nav, world, own](const NavEdge& e) -> s32
    {
        s32 condSwitch = 0;
        s32 condValue = 0;
        if (nav->Condition(e, &condSwitch, &condValue) && (condSwitch == own ? 1 : world->Switch(condSwitch)) != condValue)
        {
            return -1;
        }
        return std::max(1, e.mCost);
    };
}

// (frames a Fleech needs to him when he turns back: 150 px, as measured in the Mudanchee vault's path 2, where
// they come into being that far off as their screen comes on, leaves him the climb with a cell or two in hand.
// And the climb they have after him counts: one at the foot of the hoist he stands over is 40 frames off.)
static const s32 kFleechDrawn = 60;
// (a slap and the turn away from it)
static const s32 kFleechUse = 52;
static const s32 kFleechMargin = 5;

TaskStep FleechLureTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const Entity* entity = world.Find(mEntity);
    if (!entity)
    {
        return TaskStep::Failed("no such entity");
    }
    if (mUse)
    {
        TaskStep step = mUse->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        mUse.reset();
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            Keep(bot, {});
            return step;
        }
        mPhase = 6;
        std::set<s32> edges(mLure.mAway.begin(), mLure.mAway.end());
        if (mLure.mAway.empty())
        {
            Keep(bot, {});
            return TaskStep::Done();
        }
        Keep(bot, edges);
        mGo = GoToNode(nav.Edges()[mLure.mAway.back()].mTo);
    }
    const s32 cur = bot.AbeNode();
    // The Fleeches that are about, and how long the nearest needs to the cells in zone
    const auto nearest = [&](const std::vector<s32>& zone) -> s32
    {
        const std::vector<s32> times = Domain::FleechTimes(nav, world, zone);
        s32 least = 100000;
        for (const RockAt& f : world.mFleechesAwake)
        {
            // (one on its way up a hoist gets to the top of it first, whatever he does: measured, 40 frames from
            // the foot)
            s32 up = -1;
            s32 left = 0;
            for (const NavEdge& e : nav.Edges())
            {
                const NavNode& from = nav.Nodes()[e.mFrom];
                const NavNode& to = nav.Nodes()[e.mTo];
                if (f.mClimbing && e.mKind == EdgeKind::eHoistUp && from.mHalf == f.mHalf && std::abs(from.mX - f.mX) <= NavGraph::GridStep(f.mHalf) && f.mY <= from.mY + 5 && f.mY >= to.mY - 5 && (up < 0 || from.mY - to.mY < left))
                {
                    up = static_cast<s32>(&e - nav.Edges().data());
                    left = from.mY - to.mY;
                }
            }
            if (up >= 0)
            {
                const NavNode& to = nav.Nodes()[nav.Edges()[up].mTo];
                least = std::min(least, times[nav.Edges()[up].mTo] + 40 * std::max(0, std::min(left, f.mY - to.mY)) / std::max(1, left));
                continue;
            }
            const s32 at = FleechNode(nav, f);
            if (at >= 0)
            {
                least = std::min(least, times[at]);
            }
        }
        return least;
    };
    if (mPhase == 2 && mGo && cur >= 0 && nearest(Domain::FleechReach(nav, cur)) <= kFleechDrawn)
    {
        // They come: no further
        mGo.reset();
    }
    {
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            Keep(bot, {});
            return step;
        }
    }
    if (cur < 0)
    {
        return TaskStep::Do(Prim::Wait(1));
    }
    if (mPhase == 0)
    {
        mTarget = -1;
        std::vector<s32> homes;
        for (const RockAt& f : world.mFleechesAwake)
        {
            const s32 at = FleechNode(nav, f);
            if (at >= 0 && f.mHalf == entity->mHalf)
            {
                homes.push_back(at);
            }
        }
        if (homes.empty())
        {
            for (const Entity& f : world.mEntities)
            {
                const s32 at = f.mKind == EntityKind::eFleech && !f.mGone && !world.mFleechKilled.count(f.mId) && f.mHalf == entity->mHalf ? nav.NodeAt(f.CentreX(), f.mY2, f.mHalf, 30) : -1;
                if (at >= 0)
                {
                    homes.push_back(at);
                }
            }
        }
        // From the side of it that the way back comes down on (the Mudanchee vault's path 2: used from the well's
        // side he walked there, turned and was 36 frames longer about it)
        mLure = Domain::FleechLure();
        s32 quickest = 0;
        for (s32 side = -1; side <= 1; side += 2)
        {
            const s32 node = Domain::UseNode(nav, *entity, nullptr, side);
            if (node < 0 || (side == 1 && node == mTarget))
            {
                continue;
            }
            // (the nearest to the thing first: the way they come is taken from that one)
            const std::vector<s32> times = Domain::FleechTimes(nav, world, Domain::FleechReach(nav, node));
            std::sort(homes.begin(), homes.end(), [&](s32 a, s32 b)
                      { return times[a] < times[b]; });
            const Domain::FleechLure lure = Domain::LureFleeches(nav, world, homes, node, LureCost(bot, -1), LureCost(bot, entity->mInputs[1] > 1 ? entity->mInputs[1] : -1), -1, cur);
            s32 frames = 0;
            for (s32 edge : lure.mBack)
            {
                frames += Domain::RunFrames(nav, nav.Edges()[edge]);
            }
            if (lure.mFound && (!mLure.mFound || frames < quickest))
            {
                mLure = lure;
                quickest = frames;
                mTarget = node;
                mSide = side;
            }
        }
        if (!mLure.mFound)
        {
            // (a lever just pulled to let them out has not set its switch yet)
            if (++mWaits <= 20)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            return TaskStep::Failed("no way to lead the Fleeches off");
        }
        mWaits = 0;
        const NavNode& bait = nav.Nodes()[mLure.mBait];
        const NavNode& commit = nav.Nodes()[mLure.mCommit];
        bot.Log("the Fleeches are led to " + std::to_string(bait.mX) + "," + std::to_string(bait.mY) + " and the way back is by " + std::to_string(commit.mX) + "," + std::to_string(commit.mY) + ": " + std::to_string(mLure.mLead) + " frames gained on them");
        mPhase = 1;
        mOutAt = 0;
        if (cur != mLure.mBait)
        {
            mGo = GoToNode(mLure.mBait);
            return TaskStep::Do(Prim::Wait(1));
        }
    }
    if (mPhase == 1)
    {
        if (cur != mLure.mBait)
        {
            mGo = GoToNode(mLure.mBait);
            return TaskStep::Do(Prim::Wait(1));
        }
        mPhase = 2;
    }
    if (mPhase == 2)
    {
        if (nearest(Domain::FleechReach(nav, cur)) <= kFleechDrawn)
        {
            mPhase = 3;
            mLedAt = abe.mFrame;
            bot.Log("the Fleeches come: back the other way");
        }
        else if (mOutAt < mLure.mOut.size())
        {
            // Towards them a cell or two at a time, so as to stop when they come
            mOutAt = std::min(mOutAt + 2, mLure.mOut.size());
            Keep(bot, std::set<s32>(mLure.mOut.begin(), mLure.mOut.end()));
            mGo = GoToNode(nav.Edges()[mLure.mOut[mOutAt - 1]].mTo);
            return TaskStep::Do(Prim::Wait(1));
        }
        else if (++mWaits > 600)
        {
            Keep(bot, {});
            return TaskStep::Failed("the Fleeches did not come");
        }
        else
        {
            return TaskStep::Do(Prim::Wait(2));
        }
    }
    if (mPhase == 3)
    {
        Keep(bot, {});
        if (cur != mLure.mBait)
        {
            mGo = GoToNode(mLure.mBait);
            return TaskStep::Do(Prim::Wait(1));
        }
        mPhase = 4;
        if (cur != mLure.mCommit)
        {
            Keep(bot, std::set<s32>(mLure.mBack.begin(), mLure.mBack.end()));
            mGo = GoToNode(mLure.mCommit);
            return TaskStep::Do(Prim::Wait(1));
        }
    }
    if (mPhase == 4)
    {
        if (cur != mLure.mCommit)
        {
            Keep(bot, std::set<s32>(mLure.mBack.begin(), mLure.mBack.end()));
            mGo = GoToNode(mLure.mCommit);
            return TaskStep::Do(Prim::Wait(1));
        }
        // What is left to do, and where they are: the rest of the way, the thing, and into the way out
        s32 need = kFleechUse;
        s32 way = 0;
        s32 out = 0;
        std::vector<s32> zone = Domain::FleechReach(nav, mTarget);
        bool rest = false;
        for (s32 edge : mLure.mBack)
        {
            const NavEdge& e = nav.Edges()[edge];
            rest = rest || e.mFrom == mLure.mCommit;
            if (rest)
            {
                way += Domain::RunFrames(nav, e);
                if (nav.Nodes()[e.mTo].mLine == nav.Nodes()[mTarget].mLine)
                {
                    const std::vector<s32> more = Domain::FleechReach(nav, e.mTo);
                    zone.insert(zone.end(), more.begin(), more.end());
                }
            }
        }
        for (s32 edge : mLure.mAway)
        {
            const NavEdge& e = nav.Edges()[edge];
            if (Domain::FleechFrames(nav, world, e) < 0)
            {
                // (in a well nothing gets at him)
                out += 8;
                break;
            }
            out += Domain::RunFrames(nav, e);
            const std::vector<s32> more = Domain::FleechReach(nav, e.mTo);
            zone.insert(zone.end(), more.begin(), more.end());
        }
        need += way + out;
        const s32 theirs = nearest(zone);
        if (theirs < need + kFleechMargin)
        {
            // Not yet: here until they are further off (on their way up a hoist they are), as long as they are
            // not near him here
            const s32 toHim = nearest(Domain::FleechReach(nav, cur));
            if (toHim > 45 && ++mWaits < 900)
            {
                if (mWaits == 1 || mWaits % 150 == 0)
                {
                    bot.Log("waiting for the Fleeches to be further off: they need " + std::to_string(theirs) + " frames to where he will be, he needs " + std::to_string(need) + " there (" + std::to_string(way) + " the way, " + std::to_string(out) + " out), " + std::to_string(toHim) + " to him here");
                }
                return TaskStep::Do(Prim::Wait(1));
            }
            Keep(bot, {});
            return TaskStep::Failed("the Fleeches were not far enough behind");
        }
        bot.Log("the Fleeches need " + std::to_string(theirs) + " frames to where he will be, he needs " + std::to_string(need) + " there (" + std::to_string(way) + " the way, " + std::to_string(out) + " out)");
        mPhase = 5;
        Keep(bot, std::set<s32>(mLure.mBack.begin(), mLure.mBack.end()));
        mUse = std::make_unique<UseTask>(mEntity, mValue);
        mUse->FixSide(mSide);
        return TaskStep::Do(Prim::Wait(1));
    }
    Keep(bot, {});
    bot.Log("out of the Fleeches' reach " + std::to_string(abe.mFrame - mLedAt) + " frames after they came");
    return TaskStep::Done();
}

FleechLeadTask::~FleechLeadTask()
{
    if (mKept)
    {
        mKept->KeepTo({});
    }
}

void FleechLeadTask::Keep(Bot& bot, const std::set<s32>& edges)
{
    bot.KeepTo(edges);
    mKept = edges.empty() ? nullptr : &bot;
}

TaskStep FleechLeadTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const Entity* lever = world.Find(mLever);
    if (!lever)
    {
        return TaskStep::Failed("no such entity");
    }
    if (mHunt)
    {
        return mHunt->Next(bot, lastFailed);
    }
    if (mPhase == 0 && !mUse)
    {
        mLead = Domain::LeadFleeches(nav, world, Domain::UseNode(nav, *lever, nullptr), lever->mSwitch);
        if (!mLead.mFound)
        {
            return TaskStep::Failed("no way to lead the Fleeches to a Scrab");
        }
        const NavNode& drop = nav.Nodes()[mLead.mDrop];
        const NavNode& land = nav.Nodes()[mLead.mLand];
        const NavNode& spot = nav.Nodes()[mLead.mSpot];
        bot.Log(std::to_string(mLead.mGroup.size()) + " Fleeches to be led to " + std::to_string(drop.mX) + "," + std::to_string(drop.mY) + ", where they drop to Scrab #" + std::to_string(mLead.mScrab) + " at " + std::to_string(land.mX) + "," + std::to_string(land.mY) + "; it is taken over from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY));
        mUse = std::make_unique<UseTask>(mLever, 1);
    }
    if (mUse)
    {
        TaskStep step = mUse->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        mUse.reset();
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        mPhase = 1;
    }
    {
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            Keep(bot, {});
            return step;
        }
    }
    const s32 cur = bot.AbeNode();
    if (cur < 0)
    {
        return TaskStep::Do(Prim::Wait(1));
    }
    const s32 scale = abe.mHalfScale ? 2 : 1;
    if (mPhase == 1)
    {
        // The next hoist they have to come up
        std::size_t h = mAt;
        while (h < mLead.mWay.size() && nav.Edges()[mLead.mWay[h]].mKind != EdgeKind::eHoistUp)
        {
            h++;
        }
        if (h >= mLead.mWay.size())
        {
            mPhase = 3;
            mWaits = 0;
        }
        else
        {
            const s32 theirs = mLead.mWay[h];
            const s32 top = nav.Edges()[theirs].mTo;
            if (cur == top)
            {
                Keep(bot, {});
                mAt = h + 1;
                mPhase = 2;
                mWaits = 0;
                bot.Log("on the top of the hoist they come up: here until the first of them starts up it");
            }
            else
            {
                // By another way than theirs if there is one that is not much longer: at the foot of it with them
                // a few cells behind, and hanging from its top, he is within their reach
                Keep(bot, {});
                const NavGraph::EdgeCost all = bot.EdgeCostFn(true);
                const auto frames = [&](bool other) -> s32
                {
                    std::vector<s32> way;
                    s32 total = 0;
                    return nav.FindPath(cur, top, [&](const NavEdge& e) -> s32
                                        { return all(e) < 0 || (other && static_cast<s32>(&e - nav.Edges().data()) == theirs) ? -1 : Domain::RunFrames(nav, e); }, way, &total) ? total : -1;
                };
                const s32 direct = frames(false);
                const s32 round = frames(true);
                if (round >= 0 && (direct < 0 || round <= direct + 120))
                {
                    std::set<s32> edges;
                    for (s32 i = 0; i < static_cast<s32>(nav.Edges().size()); i++)
                    {
                        if (i != theirs)
                        {
                            edges.insert(i);
                        }
                    }
                    Keep(bot, edges);
                }
                mGo = GoToNode(top);
                return TaskStep::Do(Prim::Wait(1));
            }
        }
    }
    if (mPhase == 2)
    {
        const NavEdge& up = nav.Edges()[mLead.mWay[mAt - 1]];
        const NavNode& foot = nav.Nodes()[up.mFrom];
        const NavNode& top = nav.Nodes()[up.mTo];
        bool coming = false;
        for (const RockAt& f : world.mFleechesAwake)
        {
            if (f.mHalf != top.mHalf)
            {
                continue;
            }
            const s32 dx = std::abs(f.mX - top.mX) * scale;
            const bool here = std::abs(f.mY - top.mY) * scale <= 12 && dx <= 150;
            const bool climbing = dx <= 30 && f.mY > top.mY && (f.mClimbing || f.mY < foot.mY - 8 / scale);
            coming = coming || here || climbing;
        }
        if (!coming && ++mWaits <= 450)
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        bot.Log(coming ? "the first of them is on its way up: on" : "none of them came up: on all the same");
        mPhase = 1;
        mWaits = 0;
        return TaskStep::Do(Prim::Wait(1));
    }
    if (mPhase == 3)
    {
        // A few cells ahead of the nearest of them on his floor (or on its way up to it): they are within reach of
        // him at three cells
        s32 dist = 100000;
        for (const RockAt& f : world.mFleechesAwake)
        {
            if (f.mHalf == abe.mHalfScale && (std::abs(f.mY - abe.mY) * scale <= 25 || f.mClimbing))
            {
                dist = std::min(dist, std::abs(f.mX - abe.mX) * scale);
            }
        }
        if (cur == mLead.mDrop)
        {
            if (dist > 150 && ++mWaits <= 600)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            bot.Log("where they drop, the nearest of them " + std::to_string(dist) + " behind: over");
            Keep(bot, {mLead.mOver});
            mGo = GoToNode(nav.Edges()[mLead.mOver].mTo);
            mPhase = 5;
            return TaskStep::Do(Prim::Wait(1));
        }
        if (dist <= 190 || mAt >= mLead.mWay.size())
        {
            mAt = std::min(mAt + (dist < 130 ? 6 : 3), mLead.mWay.size());
            Keep(bot, std::set<s32>(mLead.mWay.begin(), mLead.mWay.end()));
            mGo = GoToNode(nav.Edges()[mLead.mWay[mAt - 1]].mTo);
            mWaits = 0;
            return TaskStep::Do(Prim::Wait(1));
        }
        if (++mWaits > 900)
        {
            Keep(bot, {});
            return TaskStep::Failed("the Fleeches did not follow");
        }
        return TaskStep::Do(Prim::Wait(1));
    }
    if (mPhase == 5)
    {
        Keep(bot, {});
        if (cur != mLead.mSpot)
        {
            mGo = GoToNode(mLead.mSpot);
            return TaskStep::Do(Prim::Wait(1));
        }
        mPhase = 6;
        mWaits = 0;
        mClear = 0;
    }
    // Until none of them is left up here and they are down by the Scrab
    const NavNode& land = nav.Nodes()[mLead.mLand];
    bool up = false;
    s32 down = 0;
    s32 about = 0;
    for (const RockAt& f : world.mFleechesAwake)
    {
        if (f.mHalf != abe.mHalfScale)
        {
            continue;
        }
        about++;
        up = up || f.mClimbing || (std::abs(f.mY - abe.mY) * scale <= 25 && std::abs(f.mX - abe.mX) * scale <= 450);
        down += std::abs(f.mY - land.mY) * scale <= 15 && std::abs(f.mX - land.mX) * scale <= 250 ? 1 : 0;
    }
    mClear = up ? 0 : mClear + 1;
    if (down > 0 && mClear >= 30)
    {
        bot.Log(std::to_string(down) + " of them down by the Scrab and none up here: the Scrab");
        mHunt = std::make_unique<ScrabHuntTask>(mLead.mScrab, mLead.mSpot, mLead.mGroup);
        return mHunt->Next(bot, "");
    }
    if (about == 0 && mClear >= 60)
    {
        // (it has kicked them dead itself)
        for (u32 id : mLead.mGroup)
        {
            bot.FleechKilled(id);
        }
        bot.Log("none of them left");
        return TaskStep::Done();
    }
    if (++mWaits > 900)
    {
        return TaskStep::Failed("the Fleeches did not drop to the Scrab");
    }
    return TaskStep::Do(Prim::Wait(1));
}

ScrabWaryTask::~ScrabWaryTask()
{
    if (mKept)
    {
        mKept->KeepTo({});
    }
}

std::vector<Domain::LooseScrab> ScrabWaryTask::Scrabs(const World& world, bool waitingOnly)
{
    std::vector<Domain::LooseScrab> scrabs = Domain::LooseScrabs(world, true);
    if (!waitingOnly)
    {
        const std::vector<Domain::LooseScrab> atOnce = Domain::LooseScrabs(world, false);
        scrabs.insert(scrabs.end(), atOnce.begin(), atOnce.end());
    }
    return scrabs;
}

std::vector<char> ScrabWaryTask::Hot(const Bot& bot, bool waitingOnly)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const std::vector<Domain::LooseScrab> scrabs = Scrabs(world, waitingOnly);
    std::vector<char> hot(nav.Nodes().size(), 0);
    for (std::size_t i = 0; i < nav.Nodes().size() && !scrabs.empty(); i++)
    {
        const NavNode& n = nav.Nodes()[i];
        for (const Domain::LooseScrab& scrab : scrabs)
        {
            if (scrab.mHalf == n.mHalf && std::abs(scrab.mY - n.mY) * (n.mHalf ? 2 : 1) <= 30 && !Domain::GateBetween(nav, world, scrab.mX, n.mX, n.mY, n.mHalf) && !Domain::WallBetween(nav, scrab.mX, n.mX, n.mY, n.mHalf))
            {
                hot[i] = 1;
            }
        }
    }
    return hot;
}

// The frames an edge takes him on a Scrab's floor, counted from when it can see him, as measured in the Mudanchee
// vault's path 5 (build-bot/x52.txt): a cell walked is 9 frames and run 4, a turn 13, a hop or a drop down is seen
// for its last 6 frames and stood up from in 10, through a well and out of the next is 39 to on his feet, up a
// hoist from a stand is 20 to hanging, a running jump up one 7 after the run. dir: the way he faces (0: not known).
static s32 WaryFrames(const NavGraph& nav, const std::vector<s32>& path, std::size_t i, s32& dir)
{
    const NavEdge& e = nav.Edges()[path[i]];
    const NavNode& from = nav.Nodes()[e.mFrom];
    const NavNode& to = nav.Nodes()[e.mTo];
    const s32 cell = NavGraph::GridStep(from.mHalf);
    const s32 cells = std::abs(to.mX - from.mX) / std::max(1, cell);
    s32 way = to.mX > from.mX ? 1 : to.mX < from.mX ? -1 : 0;
    s32 frames = 0;
    switch (e.mKind)
    {
        case EdgeKind::eWalk:
        {
            // (a run of walk edges one way: he runs four cells or more)
            s32 run = 1;
            for (std::size_t k = i; k > 0 && nav.Edges()[path[k - 1]].mKind == EdgeKind::eWalk && nav.Edges()[path[k - 1]].mLeft == e.mLeft; k--)
            {
                run++;
            }
            for (std::size_t k = i + 1; k < path.size() && nav.Edges()[path[k]].mKind == EdgeKind::eWalk && nav.Edges()[path[k]].mLeft == e.mLeft; k++)
            {
                run++;
            }
            frames = run >= 4 ? 4 : 9;
            // (out of a landing with the way held he is at a run at once, and rolled it is as quick: measured in
            // the Mudanchee trial, down at 937 and at 887 the other way 17 frames later, the turn in that)
            bool landed = false;
            for (std::size_t k = i; k > 0 && nav.Edges()[path[k - 1]].mKind != EdgeKind::eWell; k--)
            {
                // (or out of a teleporter)
                if (nav.Edges()[path[k - 1]].mKind == EdgeKind::eFall || nav.Edges()[path[k - 1]].mKind == EdgeKind::eTeleport)
                {
                    landed = true;
                    break;
                }
                if (nav.Edges()[path[k - 1]].mKind != EdgeKind::eWalk && nav.Edges()[path[k - 1]].mKind != EdgeKind::eRoll)
                {
                    break;
                }
            }
            if (landed)
            {
                frames = 4;
            }
            // (and getting going)
            if (i == 0 || nav.Edges()[path[i - 1]].mKind != EdgeKind::eWalk || nav.Edges()[path[i - 1]].mLeft != e.mLeft)
            {
                frames += 3;
            }
            break;
        }
        case EdgeKind::eRoll:
            // (a roll is as quick as a run; three frames to go down into it)
            frames = 4 + (i == 0 || nav.Edges()[path[i - 1]].mKind != EdgeKind::eRoll ? 3 : 0);
            break;
        case EdgeKind::eFall:
        case EdgeKind::eHoistDown:
            // (with the way on held as he lands he is off 7 frames sooner)
            frames = e.mKind == EdgeKind::eFall && i + 1 < path.size() && (nav.Edges()[path[i + 1]].mKind == EdgeKind::eWalk || nav.Edges()[path[i + 1]].mKind == EdgeKind::eRunJump) ? 9 : 16;
            break;
        case EdgeKind::eHoistUp:
            way = e.mLeft ? -1 : 1;
            // (and half the climb, for which he still hangs where its box comes by)
            frames = 30;
            break;
        case EdgeKind::eHop:
            frames = cells > 6 ? 39 : 16;
            break;
        case EdgeKind::eRunJump:
            way = e.mLeft ? -1 : 1;
            // (measured: the jump is as quick as the run, 7 frames from the take-off to hanging; and half the
            // climb when it is up to a ledge)
            frames = 3 + 4 * cells + (to.mY < from.mY - 20 ? 10 : 0);
            break;
        case EdgeKind::eWell:
        {
            // (from a run of three cells or more he is in it as he gets there: the run is let go 12 pixels short and
            // the slide takes him in. Else he stops, and the way in is 18 frames.)
            s32 run = 0;
            for (std::size_t k = i; k > 0 && nav.Edges()[path[k - 1]].mKind == EdgeKind::eWalk && (k == i || nav.Edges()[path[k - 1]].mLeft == nav.Edges()[path[i - 1]].mLeft); k--)
            {
                run++;
            }
            way = 0;
            frames = run >= 3 ? 2 : 24;
            break;
        }
        default:
            frames = std::max(6, e.mCost);
            break;
    }
    if (way != 0 && dir != 0 && way != dir && e.mKind != EdgeKind::eHoistDown && e.mKind != EdgeKind::eFall)
    {
        // (the turn out of a landing is 6 frames)
        // (and out of a teleporter 5: measured in the Mudanchee trial, there at f=755 and a cell on at a run by 767)
        frames += i > 0 && (nav.Edges()[path[i - 1]].mKind == EdgeKind::eFall || nav.Edges()[path[i - 1]].mKind == EdgeKind::eTeleport) ? 6 : 13;
    }
    if (way != 0)
    {
        dir = way;
    }
    return frames;
}

// The same without what went before, for finding a way: the turns are counted when the way is timed
static s32 WaryCost(const NavGraph& nav, const NavEdge& e)
{
    const s32 cell = NavGraph::GridStep(nav.Nodes()[e.mFrom].mHalf);
    const s32 cells = std::abs(nav.Nodes()[e.mTo].mX - nav.Nodes()[e.mFrom].mX) / std::max(1, cell);
    switch (e.mKind)
    {
        case EdgeKind::eWalk:
            // (taken to be run)
            return 5;
        case EdgeKind::eFall:
            return 12;
        case EdgeKind::eHoistDown:
            return 16;
        case EdgeKind::eHoistUp:
            return 30;
        case EdgeKind::eHop:
            return cells > 6 ? 39 : 16;
        case EdgeKind::eRunJump:
            return 3 + 4 * cells + 10;
        case EdgeKind::eWell:
            return 10;
        default:
            return std::max(6, e.mCost);
    }
}

// A hop or a fall that ends in a well in the floor: the cell of floor the well is in, or -1. He is in reach of what
// walks that floor for the last frames of the fall.
static s32 WellFloor(const NavGraph& nav, const NavEdge& e)
{
    if ((e.mKind != EdgeKind::eHop && e.mKind != EdgeKind::eFall) || e.mWell < 0)
    {
        return -1;
    }
    const NavFeature& well = nav.Input().mFeatures[e.mWell];
    return nav.NodeAt((well.mX1 + well.mX2) / 2, well.mY2, well.mHalf, 30);
}

ScrabWaryTask::Stint ScrabWaryTask::PlanOnce(const Bot& bot, s32 cur, s32 target, bool use, s32 face, s32 drop, const std::vector<char>& hot, bool best, const std::set<s32>& banned)
{
    const NavGraph& nav = bot.Nav();
    Stint stint;
    const bool forUse = use;
    const NavGraph::EdgeCost all = bot.EdgeCostFn(true);
    const NavGraph::EdgeCost base = [&](const NavEdge& e) -> s32
    {
        return banned.count(static_cast<s32>(&e - nav.Edges().data())) ? -1 : all(e);
    };
    // By the time it takes, and the less of the Scrab's floor the better
    const auto onto = [&](const NavEdge& e) -> s32
    {
        const s32 well = WellFloor(nav, e);
        return hot[e.mTo] ? e.mTo : (well >= 0 && hot[well] ? well : -1);
    };
    const NavGraph::EdgeCost wary = [&](const NavEdge& e) -> s32
    {
        return base(e) < 0 ? -1 : WaryCost(nav, e) * (onto(e) >= 0 ? 10 : 1);
    };
    if (cur != target && !nav.FindPath(cur, target, wary, stint.mPath))
    {
        return stint;
    }
    stint.mFound = true;
    for (std::size_t i = 0; i < stint.mPath.size() && stint.mFirst < 0; i++)
    {
        if (onto(nav.Edges()[stint.mPath[i]]) >= 0)
        {
            stint.mFirst = static_cast<s32>(i);
            stint.mOnto = onto(nav.Edges()[stint.mPath[i]]);
        }
    }
    if (stint.mFirst < 0 && (drop < 0 || !hot[drop]))
    {
        return stint;
    }
    s32 dir = 0;
    s32 t = 0;
    if (stint.mFirst < 0)
    {
        // None of its floor on the way, but the thing drops him onto it (measured: he is through the trap door 10
        // frames after the slap begins, down 11 frames later and on his feet 10 after that; 4 if the way on is held)
        stint.mDrop = drop;
        stint.mOnto = drop;
        stint.mLead = 16 + (face != 0 && (bot.Abe().mFacingLeft ? -1 : 1) != face ? 13 : 0);
        // (which way he runs from there is held as he lands, so no turn is counted)
        dir = 0;
        t = 3;
        stint.mAt.push_back({nav.Nodes()[drop].mX, t});
        for (const NavEdge& e : nav.Edges())
        {
            if (e.mFrom == target && e.mTo == drop)
            {
                stint.mEdges.insert(static_cast<s32>(&e - nav.Edges().data()));
            }
        }
        use = false;
        target = drop;
    }
    else
    {
        // Until it can see him (measured): a hop is 17 frames in the air before it is that low, a walk off the
        // end some 12, down a hoist 36, a well 30; and the turn first if he faces the other way
        const NavEdge& first = nav.Edges()[stint.mPath[stint.mFirst]];
        stint.mLead = first.mKind == EdgeKind::eHop ? 17 : first.mKind == EdgeKind::eFall ? 12 : first.mKind == EdgeKind::eHoistDown ? 36 : first.mKind == EdgeKind::eWell ? 30 : 0;
        if ((first.mKind == EdgeKind::eHop || first.mKind == EdgeKind::eFall) && bot.Abe().mFacingLeft != first.mLeft)
        {
            stint.mLead += 13;
        }
    }
    for (std::size_t i = stint.mFirst < 0 ? stint.mPath.size() : static_cast<std::size_t>(stint.mFirst); i < stint.mPath.size(); i++)
    {
        const NavEdge& e = nav.Edges()[stint.mPath[i]];
        const s32 well = WellFloor(nav, e);
        if (well >= 0 && hot[well] && !hot[e.mTo])
        {
            // (down into a well in its floor: in its reach for the last 6 frames of the fall)
            t += 6;
            stint.mAt.push_back({nav.Nodes()[well].mX, t});
        }
        else if (e.mKind == EdgeKind::eWell && !hot[e.mFrom])
        {
            // (onto its floor by a well: he comes out of the well of that floor that leads back the way he came,
            // which is where it first sees him, and is on his feet where he lands 9 frames later)
            const NavNode& from = nav.Nodes()[e.mFrom];
            const NavNode& to = nav.Nodes()[e.mTo];
            s32 mouth = to.mX;
            for (const NavEdge& g : nav.Edges())
            {
                const NavNode& a = nav.Nodes()[g.mFrom];
                const NavNode& b = nav.Nodes()[g.mTo];
                if (g.mKind == EdgeKind::eWell && hot[g.mFrom] && a.mLine == to.mLine && b.mHalf == from.mHalf && std::abs(b.mX - from.mX) <= 60 && std::abs(b.mY - from.mY) <= 20)
                {
                    mouth = a.mX;
                }
            }
            stint.mAt.push_back({mouth, t});
            t += 9;
            stint.mAt.push_back({to.mX, t});
            dir = to.mX > mouth ? 1 : to.mX < mouth ? -1 : 0;
        }
        else if (e.mKind == EdgeKind::eTeleport)
        {
            // (onto its floor by a teleporter: it sees him as he appears, and he can move a few frames later,
            // facing as he went in). From its floor he is taken some 10 frames after he stops on the plate
            // (measured: stopped at f=725, gone at 735).
            if (hot[e.mFrom])
            {
                t += 9;
                stint.mAt.push_back({nav.Nodes()[e.mFrom].mX, t});
            }
            stint.mAt.push_back({nav.Nodes()[e.mTo].mX, t});
            t += 1;
            stint.mAt.push_back({nav.Nodes()[e.mTo].mX, t});
        }
        else
        {
            t += WaryFrames(nav, stint.mPath, i, dir);
            // (a well takes him where it is, not where it leads)
            stint.mAt.push_back({nav.Nodes()[e.mKind == EdgeKind::eWell ? e.mFrom : e.mTo].mX, t});
        }
        stint.mEdges.insert(stint.mPath[i]);
        if (!hot[e.mTo])
        {
            stint.mExit = e.mTo;
            break;
        }
    }
    if (stint.mExit < 0)
    {
        // It ends on the Scrab's floor: the thing is used (measured: a slap is 36 frames until he can move, and he
        // has to face it), and a place out of its sight made for. Of the places there are and the ways to them
        // (with a running jump or without), the one that leaves most time.
        if (use && face != 0 && dir != face)
        {
            t += dir != 0 ? 13 : 0;
            dir = face;
        }
        t += use ? 39 : 0;
        if (!stint.mAt.empty())
        {
            stint.mAt.back().second = t;
        }
        std::set<s32> outs;
        for (const NavEdge& e : nav.Edges())
        {
            if (hot[e.mFrom] && !hot[e.mTo])
            {
                outs.insert(e.mTo);
            }
        }
        const std::vector<std::pair<s32, s32>> soFar = stint.mAt;
        const std::set<s32> edgesSoFar = stint.mEdges;
        s32 bestSpare = -100000;
        s32 bestFrames = t;
        // How much there is to a place out of its sight: the cells that can be got to from it without setting foot
        // on the Scrab's floor. A ledge with nothing but that floor round it is a corner to be caught in.
        const auto room = [&](s32 from) -> s32
        {
            std::vector<char> seen(nav.Nodes().size(), 0);
            std::vector<s32> open{from};
            seen[from] = 1;
            s32 count = 0;
            while (!open.empty() && count < 40)
            {
                const s32 node = open.back();
                open.pop_back();
                count++;
                const NavNode& n = nav.Nodes()[node];
                for (s32 k = n.mFirstEdge; k < n.mFirstEdge + n.mEdgeCount; k++)
                {
                    const NavEdge& e = nav.Edges()[k];
                    if (!seen[e.mTo] && !hot[e.mTo] && onto(e) < 0 && base(e) >= 0)
                    {
                        seen[e.mTo] = 1;
                        open.push_back(e.mTo);
                    }
                }
            }
            return count;
        };
        for (s32 out : outs)
        {
            for (s32 jump = 0; jump < 2; jump++)
            {
                std::vector<s32> back;
                if (!nav.FindPath(target, out, [&](const NavEdge& e) -> s32
                                  { return (!hot[e.mTo] && e.mTo != out) || base(e) < 0 || (jump == 0 && e.mKind == EdgeKind::eRunJump) ? -1 : WaryCost(nav, e); }, back))
                {
                    continue;
                }
                Stint trial;
                trial.mPath = stint.mPath;
                trial.mFirst = stint.mFirst;
                trial.mLead = stint.mLead;
                trial.mDrop = stint.mDrop;
                trial.mAt = soFar;
                trial.mEdges = edgesSoFar;
                s32 tt = t;
                s32 d = dir;
                // (off a trap door he lands facing the way he slapped: the turn into the run is 8 frames)
                if (stint.mDrop >= 0 && !back.empty() && face != 0 && (nav.Edges()[back[0]].mLeft ? -1 : 1) != face)
                {
                    tt += 8;
                }
                for (std::size_t k = 0; k < back.size(); k++)
                {
                    tt += WaryFrames(nav, back, k, d);
                    trial.mAt.push_back({nav.Nodes()[nav.Edges()[back[k]].mKind == EdgeKind::eWell ? nav.Edges()[back[k]].mFrom : nav.Edges()[back[k]].mTo].mX, tt});
                    trial.mEdges.insert(back[k]);
                }
                trial.mOnto = stint.mOnto;
                // (ranked as it would be at the best moment, so that the choice does not change with where the Scrab
                // is while he waits to use a thing and makes for that place after. A crossing that only ends on
                // its floor goes nowhere after by this plan: there the way out is the one that lies away from
                // where the Scrab is now. A place with room to go on from before a corner, if both can be done.)
                s32 spare = Spare(bot, trial, hot, forUse ? true : best);
                if (spare >= 0 && room(out) >= 20)
                {
                    spare += 10000;
                }
                if (spare > bestSpare)
                {
                    bestSpare = spare;
                    bestFrames = tt;
                    stint.mAt = trial.mAt;
                    stint.mEdges = trial.mEdges;
                    stint.mRetreat = out;
                }
            }
        }
        if (stint.mRetreat < 0)
        {
            // Nowhere to go after: no stint to plan
            stint.mFound = false;
        }
        t = bestFrames;
    }
    stint.mFrames = t;
    return stint;
}

ScrabWaryTask::Stint ScrabWaryTask::PlanFor(const Bot& bot, s32 cur, s32 goal, const Entity* entity, const std::vector<char>& hot, bool best)
{
    const NavGraph& nav = bot.Nav();
    Stint out;
    s32 outRank = -100000;
    const bool sides = entity && entity->mKind == EntityKind::eSlapLock;
    for (s32 side = sides ? -1 : 0; side <= (sides ? 1 : 0); side += 2)
    {
        bool faceLeft = false;
        const s32 target = entity ? Domain::UseNode(nav, *entity, &faceLeft, side == 0 ? -1 : side) : goal;
        // (the cell on that side, not the other one for want of it)
        if (target < 0 || (sides && (nav.Nodes()[target].mX > entity->CentreX()) != (side > 0)))
        {
            continue;
        }
        // (a lock whose slap opens the trap door he slaps it from)
        s32 drop = -1;
        if (sides && entity->mInputs[1] > 1 && nav.Nodes()[target].mTrapDoor >= 0 && nav.Input().mFeatures[nav.Nodes()[target].mTrapDoor].mSwitch == entity->mInputs[1])
        {
            for (const NavEdge& e : nav.Edges())
            {
                if (e.mFrom == target && e.mKind == EdgeKind::eFall && nav.Nodes()[e.mTo].mY > nav.Nodes()[target].mY + 20 && std::abs(nav.Nodes()[e.mTo].mX - nav.Nodes()[target].mX) <= 2)
                {
                    drop = e.mTo;
                }
            }
        }
        Stint stint = Plan(bot, cur, target, entity != nullptr, sides ? (faceLeft ? -1 : 1) : 0, drop, hot, best);
        stint.mTarget = target;
        stint.mSide = side;
        stint.mSpare = stint.mFound ? Spare(bot, stint, hot, best) : -100000;
        // (the side by how it would be at the best moment, so that it does not change with where the Scrab is)
        const s32 rank = stint.mFound ? Spare(bot, stint, hot, true) : -100000;
        if (stint.mFound && (!out.mFound || rank > outRank))
        {
            out = stint;
            outRank = rank;
        }
    }
    return out;
}

s32 ScrabWaryTask::Spare(const Bot& bot, const Stint& stint, const std::vector<char>& hot, bool best, std::string* note, bool* beatKnown)
{
    if (beatKnown)
    {
        *beatKnown = true;
    }
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    if ((stint.mFirst < 0 && stint.mDrop < 0) || stint.mAt.empty())
    {
        return 1000;
    }
    const NavNode& onto = nav.Nodes()[stint.mOnto >= 0 ? stint.mOnto : nav.Edges()[stint.mPath[stint.mFirst]].mTo];
    s32 spare = 1000;
    // (in the background everything is half as far and half as fast)
    const s32 scale = onto.mHalf ? 2 : 1;
    for (const Domain::LooseScrab& scrab : Scrabs(world))
    {
        if (scrab.mHalf != onto.mHalf || std::abs(scrab.mY - onto.mY) * (onto.mHalf ? 2 : 1) > 30 || Domain::GateBetween(nav, world, scrab.mX, onto.mX, onto.mY, onto.mHalf) || Domain::WallBetween(nav, scrab.mX, onto.mX, onto.mY, onto.mHalf))
        {
            continue;
        }
        // The ends of its beat (of the floor it can get about on)
        s32 lo = scrab.mX;
        s32 hi = scrab.mX;
        for (std::size_t i = 0; i < nav.Nodes().size(); i++)
        {
            const NavNode& n = nav.Nodes()[i];
            if (hot[i] && n.mHalf == onto.mHalf && std::abs(n.mY - onto.mY) <= 5)
            {
                lo = std::min(lo, n.mX);
                hi = std::max(hi, n.mX);
            }
        }
        const s32 floorLo = lo;
        const s32 floorHi = hi;
        // (as seen, once it has been seen to stop at both: its TLVs bound it to less than its floor)
        bool seen = false;
        {
            s32 seenLo = 0;
            s32 seenHi = 0;
            if (bot.ScrabBeat(scrab.mId, seenLo, seenHi))
            {
                lo = seenLo;
                hi = seenHi;
                seen = true;
            }
            else
            {
                // Not seen yet: the ends the map gives it, the nearest either side of where the map puts it
                // (it is chased out of them, but it does not walk out of them)
                const Entity* tlv = world.Find(scrab.mId);
                if (tlv)
                {
                    s32 left = -1000000;
                    s32 right = 1000000;
                    for (const std::pair<s32, s32>& b : world.mScrabBoundsLeft)
                    {
                        if (std::abs(b.second - tlv->mY2) <= 30 && b.first <= tlv->CentreX() + 30 && b.first > left)
                        {
                            left = b.first;
                        }
                    }
                    for (const std::pair<s32, s32>& b : world.mScrabBoundsRight)
                    {
                        if (std::abs(b.second - tlv->mY2) <= 30 && b.first >= tlv->CentreX() - 30 && b.first < right)
                        {
                            right = b.first;
                        }
                    }
                    if (left > -1000000 && right < 1000000 && scrab.mX >= left - 30 && scrab.mX <= right + 30)
                    {
                        lo = std::max(lo, left);
                        hi = std::min(hi, right);
                    }
                }
                if (beatKnown && world.ScrabOf(scrab.mId))
                {
                    // (one that is not about yet is where its TLV puts it)
                    *beatKnown = false;
                }
            }
        }
        // (at best: the ends of its beat, and until that has been seen the ends of its floor as well. Chased, or
        // let out, one has been met far outside the ends the map gives it: the Mudanchee vault's path 5. Once it
        // is seen, the best there is to wait for is what its beat gives.)
        std::vector<s32> places{lo, hi};
        if (!seen)
        {
            places.push_back(floorLo);
            places.push_back(floorHi);
        }
        if (!world.ScrabOf(scrab.mId))
        {
            places = {scrab.mX};
        }
        else if (!best)
        {
            // Where it is, and where it may be by the time it can see him: it runs its beat at the pace it runs at
            // him. If it faces this way it comes as soon as it has stood out its time at the end of its beat (how
            // long it has stood there is known once it has been seen to stop); if it faces away it has to get to
            // the end, turn and stand first.
            places = {scrab.mX};
            const ScrabAt* live = world.ScrabOf(scrab.mId);
            const Entity* tlv = world.Find(scrab.mId);
            if (live && tlv)
            {
                const s32 towards = stint.mAt.front().first < scrab.mX ? -1 : 1;
                const s32 pause = towards < 0 ? tlv->mInputs[1] : tlv->mInputs[2];
                const bool atEnd = scrab.mX - lo < 60 / scale || hi - scrab.mX < 60 / scale;
                s32 free = 0;
                if (live->mFacesLeft != (towards < 0))
                {
                    free = 10 + pause;
                }
                else if (atEnd && bot.ScrabStillSince(scrab.mId) != 0)
                {
                    free = std::max(0, pause - static_cast<s32>(bot.Abe().mFrame - bot.ScrabStillSince(scrab.mId)));
                }
                // (no further than the end of its beat: it does not walk out of it before it has seen him)
                places[0] = std::max(std::min(lo, scrab.mX), std::min(std::max(hi, scrab.mX), places[0] + towards * 76 * std::max(0, stint.mLead - free) / (10 * scale)));
                if (note)
                {
                    *note += " (the Scrab at " + std::to_string(scrab.mX) + (live->mFacesLeft ? " facing left" : " facing right") + ", taken to be at " + std::to_string(places[0]) + " when it sees him)";
                }
            }
        }
        // A stint with a teleporter in it puts him somewhere else on its floor at a stroke (behind it, in the
        // Mudanchee trial): where the Scrab is by then is where it has run to after him, so that is played out. It
        // sets off when its delay is over, runs at him at 7.35 pixels a frame, and is 14 frames turning and
        // setting off again when he is suddenly on its other side (measured: he was there at f=755, it began to
        // turn at 766 and moved at 769; from 895 to 1108 in 29 frames). What is to spare is the least it would still need to get to him.
        bool jumps = stint.mFirst >= 0 && nav.Edges()[stint.mPath[stint.mFirst]].mKind == EdgeKind::eTeleport;
        for (std::size_t i = 1; i < stint.mAt.size(); i++)
        {
            jumps = jumps || (stint.mAt[i].second == stint.mAt[i - 1].second && std::abs(stint.mAt[i].first - stint.mAt[i - 1].first) * scale >= 100);
        }
        if (jumps)
        {
            s32 bestSim = -1000;
            // (at best: also wherever on its floor a chase may have left it, which is where it is when the
            // teleporter is the first of the stint)
            std::vector<s32> from = places;
            if (best)
            {
                from.push_back(floorLo);
                from.push_back(floorHi);
                from.push_back(scrab.mX);
            }
            for (s32 place : from)
            {
                s32 sx10 = place * 10;
                s32 lastDir = 0;
                s32 turnUntil = 0;
                s32 least = 1000;
                std::size_t k = 0;
                const s32 endT = stint.mAt.back().second;
                const ScrabAt* live = world.ScrabOf(scrab.mId);
                // (it sees him once he is on its floor)
                const s32 startAt = stint.mAt.front().second + scrab.mDelay + 3 + (live && !best && live->mFacesLeft != (stint.mAt.front().first < place) ? 8 : 0);
                for (s32 t = 0; t <= endT; t++)
                {
                    while (k + 1 < stint.mAt.size() && stint.mAt[k + 1].second <= t)
                    {
                        k++;
                    }
                    s32 ax = stint.mAt[k].first;
                    if (t >= stint.mAt[k].second && k + 1 < stint.mAt.size() && stint.mAt[k + 1].second > stint.mAt[k].second)
                    {
                        ax += (stint.mAt[k + 1].first - ax) * (t - stint.mAt[k].second) / (stint.mAt[k + 1].second - stint.mAt[k].second);
                    }
                    const s32 dir = ax * 10 > sx10 ? 1 : -1;
                    if (lastDir != 0 && dir != lastDir)
                    {
                        turnUntil = t + 14;
                    }
                    lastDir = dir;
                    if (t >= startAt && t >= turnUntil)
                    {
                        sx10 += dir * 74 / scale;
                    }
                    // (until it sets off it is only too near within a cell and a half, where it strikes at once)
                    const s32 gap10 = std::abs(ax * 10 - sx10) * scale;
                    least = std::min(least, t < startAt ? (gap10 <= 400 ? -1000 : 1000) : (gap10 - 450) / 74);
                }
                bestSim = std::max(bestSim, least);
            }
            spare = std::min(spare, bestSim);
            continue;
        }
        s32 bestHere = -1000;
        for (s32 place : places)
        {
            // It runs 7.6 pixels a frame once its delay is over, and kills what its box is on
            // (and one he comes within a cell and a half of strikes at once, delay or none)
            // (nor may it come by where he is on its way there: a Scrab at a run kills what its box is on, a
            // hanging Abe too)
            s32 least = 1000;
            const s32 swept1 = std::min(place, scrab.mX) - 100 / scale;
            const s32 swept2 = std::max(place, scrab.mX) + 100 / scale;
            // (and where he is between the ends of a long edge, a running jump: every cell of it)
            std::vector<std::pair<s32, s32>> every;
            for (std::size_t i = 0; i < stint.mAt.size(); i++)
            {
                if (i > 0)
                {
                    const std::pair<s32, s32>& a = stint.mAt[i - 1];
                    const std::pair<s32, s32>& b = stint.mAt[i];
                    const s32 steps = std::abs(b.first - a.first) * scale / 25;
                    for (s32 k = 1; k < steps && steps <= 40; k++)
                    {
                        every.push_back({a.first + (b.first - a.first) * k / steps, a.second + (b.second - a.second) * k / steps});
                    }
                }
                every.push_back(stint.mAt[i]);
            }
            for (const std::pair<s32, s32>& at : every)
            {
                // (a hundred pixels off is clear of it: the drop next to one that has just got to the end of its beat)
                const bool near = best ? std::abs(place - at.first) < 100 / scale : (at.first > swept1 && at.first < swept2);
                least = std::min(least, near ? -1000 : scrab.mDelay + 3 + std::max(0, std::abs(place - at.first) * scale - 45) * 100 / 735 - at.second);
            }
            bestHere = std::max(bestHere, least);
        }
        spare = std::min(spare, bestHere);
    }
    return spare;
}

// The frames a stint has to have to spare: this many whenever, or the fewer there are at the best moment of the
// Scrab's beat (within kWaryNearBest of that, and no fewer than kWaryLeast), for a chase that is made to be close
static const s32 kWaryMargin = 12;
static const s32 kWaryLeast = 4;
static const s32 kWaryNearBest = 5;

ScrabWaryTask::Stint ScrabWaryTask::Plan(const Bot& bot, s32 cur, s32 target, bool use, s32 face, s32 drop, const std::vector<char>& hot, bool best)
{
    const NavGraph& nav = bot.Nav();
    std::set<s32> banned;
    Stint stint;
    for (s32 tries = 0; tries < 6; tries++)
    {
        stint = PlanOnce(bot, cur, target, use, face, drop, hot, best, banned);
        if (!stint.mFound || stint.mFirst < 0 || Spare(bot, stint, hot, true) >= kWaryLeast)
        {
            break;
        }
        // Not in time even at the best moment: another way, without this stretch
        const std::size_t before = banned.size();
        for (std::size_t i = static_cast<std::size_t>(stint.mFirst); i < stint.mPath.size(); i++)
        {
            banned.insert(stint.mPath[i]);
            if (!hot[nav.Edges()[stint.mPath[i]].mTo])
            {
                break;
            }
        }
        if (banned.size() == before)
        {
            break;
        }
    }
    // A way that will do at its best moment, but not now: if another that sets out from here will do now, that is
    // the one (the Mudanchee trial: the Fleeches at him on the ledge leave no time to wait for the first)
    if (!best && stint.mFound && stint.mFirst >= 0 && Spare(bot, stint, hot, false) < kWaryLeast)
    {
        // (leaving out only the step that goes on from where each came onto its floor, as below)
        std::set<s32> others;
        Stint other = stint;
        for (s32 tries = 0; tries < 6; tries++)
        {
            if (!others.insert(other.mPath[std::min(static_cast<std::size_t>(other.mFirst) + 1, other.mPath.size() - 1)]).second)
            {
                break;
            }
            other = PlanOnce(bot, cur, target, use, face, drop, hot, best, others);
            if (!other.mFound || other.mFirst < 0)
            {
                break;
            }
            if (other.mFirst == 0 && Spare(bot, other, hot, true) >= kWaryLeast && Spare(bot, other, hot, false) >= kWaryLeast)
            {
                return other;
            }
        }
    }
    // Nothing left that way: again, leaving out only the step that goes on from where each way that was too slow
    // came onto its floor, so that another way may come onto it and go off it by the same edges (the Mudanchee
    // trial: not along its floor to the well, but by the teleporter that puts him down behind it and then to the
    // same well)
    if (!stint.mFound && !banned.empty())
    {
        banned.clear();
        for (s32 tries = 0; tries < 6; tries++)
        {
            stint = PlanOnce(bot, cur, target, use, face, drop, hot, best, banned);
            if (!stint.mFound || stint.mFirst < 0 || Spare(bot, stint, hot, true) >= kWaryLeast ||
                !banned.insert(stint.mPath[std::min(static_cast<std::size_t>(stint.mFirst) + 1, stint.mPath.size() - 1)]).second)
            {
                break;
            }
        }
    }
    return stint;
}

bool ScrabWaryTask::Wanted(const Bot& bot, s32 goal, u32 entity, std::string* note, bool chased)
{
    const NavGraph& nav = bot.Nav();
    const s32 cur = bot.AbeNode();
    // (unseen there is no Scrab to wait for, the power runs out while he does: nor when the step at hand is the
    // one the plan has him go unseen for)
    if (cur < 0 || Scrabs(bot.GetWorld()).empty() || bot.GetWorld().mInvisible || (bot.InvisSaved() && bot.InvisChantNow() && bot.GetWorld().mInvisCharge))
    {
        return false;
    }
    // (no waiting for a Scrab with a Fleech after him: one awake of his size and as near as it follows him from,
    // Fleech.cpp's 750 by 260. Those of another screen he left behind are not.)
    // (nor are those that have no way to him)
    std::vector<s32> times;
    for (const RockAt& f : bot.GetWorld().mFleechesAwake)
    {
        if (!chased && f.mHalf == nav.Nodes()[cur].mHalf && std::abs(f.mX - nav.Nodes()[cur].mX) <= 750 && std::abs(f.mY - nav.Nodes()[cur].mY) <= 260)
        {
            if (times.empty())
            {
                times = Domain::FleechTimes(nav, bot.GetWorld(), {cur});
            }
            const s32 at = nav.NodeAt(f.mX, f.mY, f.mHalf, 30);
            if (at < 0 || times[at] < 100000)
            {
                if (note)
                {
                    *note = "over a Scrab's floor: not waited at with a Fleech awake and after him";
                }
                return false;
            }
        }
    }
    const Entity* e = entity ? bot.GetWorld().Find(entity) : nullptr;
    const std::vector<char> hot = Hot(bot);
    if (hot[cur])
    {
        return false;
    }
    const Stint stint = PlanFor(bot, cur, goal, e, hot, true);
    if (!stint.mFound || (stint.mFirst < 0 && stint.mDrop < 0))
    {
        if (note && (chased || !stint.mFound))
        {
            *note = std::string("over a Scrab's floor: ") + (stint.mFound ? "no stretch of it on the way" : "no way found");
        }
        return false;
    }
    // Over the floor of a Scrab that waits before it strikes: this is the way. Over one's that strikes at once:
    // only where there is no other way (the way he is kept off such floors has been enough elsewhere).
    const std::vector<char> waits = Hot(bot, true);
    bool waiting = stint.mDrop >= 0 && waits[stint.mDrop];
    for (s32 edge : stint.mEdges)
    {
        waiting = waiting || waits[nav.Edges()[edge].mTo];
    }
    if (!waiting)
    {
        std::vector<s32> plain;
        // (another way: one that keeps off every floor a Scrab gets at him on)
        const NavGraph::EdgeCost cost = bot.EdgeCostFn();
        if (stint.mDrop < 0 && nav.FindPath(cur, stint.mTarget, [&](const NavEdge& e) -> s32
                                            { return hot[e.mTo] ? -1 : cost(e); }, plain))
        {
            return false;
        }
    }
    if (note)
    {
        *note = "over a Scrab's floor: " + std::to_string(stint.mFrames) + " frames on it, " + std::to_string(stint.mSpare) + " to spare at best";
    }
    return stint.mSpare >= kWaryLeast;
}

TaskStep ScrabWaryTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    if (mUse)
    {
        TaskStep step = mUse->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        mUse.reset();
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        mUsed = true;
    }
    {
        TaskStep step;
        const bool going = mGo != nullptr;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        const bool wasPrefix = going && mPrefix;
        if (wasPrefix)
        {
            mPrefix = false;
            bot.KeepTo({});
            mKept = nullptr;
        }
        // (the way out of its sight after the thing is used: where he ended up will do if it is out of its sight.
        // And a way to where a stint starts that went wrong, a well that led elsewhere: from where he is, again.)
        const bool safeNow = bot.AbeNode() >= 0 && !Hot(bot)[bot.AbeNode()];
        if (going && step.mKind == TaskStep::Kind::eFailed && !((mUsed || (wasPrefix && ++mPrefixFails <= 3)) && safeNow))
        {
            return step;
        }
    }
    // What comes next is started in the same frame: a frame with nothing held stops a run
    const auto next = [&]() -> TaskStep
    {
        if (++mDepth > 6)
        {
            mDepth = 0;
            return TaskStep::Do(Prim::Wait(1));
        }
        const TaskStep step = Next(bot, "");
        mDepth = 0;
        return step;
    };
    const s32 cur = bot.AbeNode();
    if (cur < 0)
    {
        return TaskStep::Do(Prim::Wait(1));
    }
    const std::vector<char> hot = Hot(bot);
    if (mWentAt != 0 && !hot[cur])
    {
        bot.KeepTo({});
        mKept = nullptr;
        bot.Log("off the Scrab's floor " + std::to_string(abe.mFrame - mWentAt) + " frames after setting out");
        mWentAt = 0;
    }
    const Entity* entity = mEntity ? world.Find(mEntity) : nullptr;
    if (mEntity && !entity)
    {
        return TaskStep::Failed("no such entity");
    }
    if (entity ? mUsed : cur == mGoal)
    {
        if (!hot[cur] || !entity)
        {
            return TaskStep::Done();
        }
        // Out of its sight: to the place picked for it, else the nearest way
        if (mRetreat >= 0 && !hot[mRetreat])
        {
            mGo = GoToNode(mRetreat);
            mRetreat = -1;
            return next();
        }
        std::vector<s32> back;
        if (!nav.FindPath(cur, [&](s32 n)
                          { return !hot[n]; }, bot.EdgeCostFn(), back) || back.empty())
        {
            return TaskStep::Done();
        }
        mGo = GoToNode(nav.Edges()[back.back()].mTo);
        return next();
    }
    const Stint stint = PlanFor(bot, cur, mGoal, entity, hot, false);
    if (!stint.mFound)
    {
        if (mWentAt != 0 && !entity && cur != mGoal)
        {
            // Set out and still on its floor where the stint was to end (a gate that was shut when it was planned
            // has opened as he ran over its plate): stopping here is what it leaves no time for, so on to the end
            bot.Log("still on the Scrab's floor where the stint ended: on to the end of it");
            std::vector<s32> on;
            bot.KeepTo({});
            mKept = nullptr;
            if (nav.FindPath(cur, mGoal, bot.EdgeCostFn(true), on))
            {
                bot.KeepTo(std::set<s32>(on.begin(), on.end()));
                mKept = &bot;
            }
            mGo = GoToNode(mGoal);
            return next();
        }
        return TaskStep::Failed("no way there that the Scrab leaves time for");
    }
    const s32 target = stint.mTarget;
    const auto go = [&]() -> TaskStep
    {
        if (entity)
        {
            std::unique_ptr<UseTask> use = std::make_unique<UseTask>(mEntity, mValue);
            if (stint.mSide != 0)
            {
                use->FixSide(stint.mSide);
            }
            if (stint.mDrop >= 0 && stint.mAt.size() >= 2)
            {
                // Off at a run the way out lies as his feet touch
                use->SetAfter((stint.mAt[1].first < stint.mAt[0].first ? Btn::eLeft : Btn::eRight) | Btn::eRun);
            }
            mUse = std::move(use);
        }
        else
        {
            mGo = GoToNode(target);
        }
        return next();
    };
    if (stint.mFirst < 0 && stint.mDrop < 0)
    {
        // None of its floor on the way
        return go();
    }
    if (stint.mFirst < 0 && cur != target)
    {
        // To the trap door it is used from
        mGo = GoToNode(target);
        return TaskStep::Do(Prim::Wait(1));
    }
    if (stint.mFirst > 0 && !hot[cur])
    {
        // To where the stint starts from, by the way that was found: the quickest might lead over its floor
        std::set<s32> edges;
        for (s32 i = 0; i < stint.mFirst; i++)
        {
            edges.insert(stint.mPath[i]);
        }
        bot.KeepTo(edges);
        mKept = &bot;
        mPrefix = true;
        mGo = GoToNode(nav.Edges()[stint.mPath[stint.mFirst - 1]].mTo);
        return next();
    }
    if (!hot[cur])
    {
        const s32 spare = stint.mSpare;
        // (and it is still after him for 210 frames from when it last saw him)
        const bool after = bot.SeenByScrabAt() != 0 && abe.mFrame - bot.SeenByScrabAt() < 230;
        bool beatKnown = false;
        const s32 atBest = Spare(bot, PlanFor(bot, cur, mGoal, entity, hot, true), hot, true, nullptr, &beatKnown);
        const bool enough = spare >= kWaryMargin || (beatKnown && spare >= kWaryLeast && spare >= atBest - kWaryNearBest);
        // Facing the way he will go, so that no turn is in the way when the moment comes
        if (stint.mFirst >= 0 && abe.Settled() && abe.mMove == Move::eIdle)
        {
            const NavEdge& first = nav.Edges()[stint.mPath[stint.mFirst]];
            if ((first.mKind == EdgeKind::eFall || first.mKind == EdgeKind::eHop) && abe.mFacingLeft != first.mLeft)
            {
                return TaskStep::Do(Prim::Face(first.mLeft));
            }
        }
        // A Fleech at him where he waits: there is no waiting for the best moment, the first that will do it is
        // (the Mudanchee trial, where two are let loose on the ledge he drops from)
        bool bitten = false;
        for (const RockAt& f : world.mFleechesAwake)
        {
            bitten = bitten || (f.mHalf == nav.Nodes()[cur].mHalf && std::abs(f.mY - nav.Nodes()[cur].mY) <= 30 && std::abs(f.mX - nav.Nodes()[cur].mX) * (f.mHalf ? 2 : 1) <= 200);
        }
        if (bitten ? spare < kWaryLeast : (after || !enough))
        {
            if (!mSaid)
            {
                mSaid = true;
                bot.Log("waiting for the Scrab to be far enough off: " + std::to_string(stint.mFrames) + " frames on its floor, " + std::to_string(spare) + " to spare now");
            }
            if (++mWaits > 4500)
            {
                return TaskStep::Failed("the Scrab never was far enough off");
            }
            if (mWaits % 150 == 0)
            {
                // What it is waiting on, for whoever reads the log
                std::string note;
                Spare(bot, stint, hot, false, &note);
                std::string at;
                for (const std::pair<s32, s32>& pt : stint.mAt)
                {
                    at += " " + std::to_string(pt.first) + "@" + std::to_string(pt.second);
                }
                bot.Log("still waiting: " + std::to_string(spare) + " to spare now, " + std::to_string(atBest) + " at best" + (beatKnown ? "" : " (its beat not seen from end to end yet)") + (after ? ", it is still after him" : "") + note + ", he would be at" + at);
            }
            return TaskStep::Do(Prim::Wait(2));
        }
        std::string note;
        Spare(bot, stint, hot, false, &note);
        bot.Log("the Scrab is far enough off: " + std::to_string(stint.mFrames) + " frames on its floor, " + std::to_string(spare) + " to spare" + note);
        mSaid = false;
        mWentAt = abe.mFrame;
        mRetreat = stint.mRetreat;
        bot.KeepTo(stint.mEdges);
        mKept = &bot;
    }
    if (stint.mExit >= 0)
    {
        mGo = GoToNode(stint.mExit);
        return next();
    }
    return go();
}

} // namespace Bot
