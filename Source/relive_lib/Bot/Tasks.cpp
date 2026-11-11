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
        const bool down = mLastEdge >= 0 && nav.Edges()[mLastEdge].mKind == EdgeKind::eHoistDown;
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

    // What the last edge really did
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
    if (abe.mMove == Move::eCrouch && !nav.Nodes()[cur].mLow && !(mSweepDuck && !atGoal) && (atGoal || nav.Edges()[path[0]].mKind != EdgeKind::eRoll))
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
            if (sweepStop <= last)
            {
                last = sweepStop - 1;
            }
            return TaskStep::Do(Prim::MoveTo(nav.Nodes()[nav.Edges()[path[last]].mTo].mX, Gait::eRoll));
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
                sneak = true;
                if (slig.mAsleep)
                {
                    // Stop before its screen, so whatever has to be put right first (a laughing Mudokon in tow) can be
                    std::size_t firstIn = 0;
                    while (firstIn <= last && nav.CameraOf(nav.Nodes()[nav.Edges()[path[firstIn]].mTo].mX, here.mY - 5) != sligCam)
                    {
                        firstIn++;
                    }
                    // (a few cells before it, to have room for that)
                    if (firstIn > 3 && firstIn <= last && nav.CameraOf(here.mX, here.mY - 5) != sligCam)
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
            const NavNode& to = nav.Nodes()[nav.Edges()[path[last]].mTo];
            Gait gait = bot.GaitFor(static_cast<s32>(last) + 1);
            if (gait == Gait::eRun && mTopGait != Gait::eRun)
            {
                gait = mTopGait;
            }
            if (sneak)
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
            mLastEdge = path[0];
            return TaskStep::Do(Prim::WalkOff(first.mLeft));

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
            mLastEdge = path[0];
            return TaskStep::Do(Prim::Press(Btn::eUp));

        case EdgeKind::eHoistDown:
            mLastEdge = path[0];
            return TaskStep::Do(Prim::Press(Btn::eDown));
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
            const s32 onFloor = e.mKind == EdgeKind::eHoistUp ? frames - 40 : 40;
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

    // The place to try next: what has worked before first
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
            if (bot.ThrowIsBad(mMine, s) || std::find(mNoWay.begin(), mNoWay.end(), s.mNode) != mNoWay.end() || (pass % 2 == 0 && !bot.ThrowHasWorked(s)) || (pass < 4 && here >= 0 && regions[s.mNode] != regions[here]))
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
            // One by where he stands goes off if the rock comes back down on it, and takes him with it
            const NavNode& at = nav.Nodes()[s.mNode];
            for (const Entity& other : world.mEntities)
            {
                blocked = blocked || (other.mKind == EntityKind::eMine && !other.mGone && other.mHalf == at.mHalf && std::abs(other.mY2 - at.mY) <= 50 && std::abs(other.CentreX() - at.mX) < (s.mLevel < 0 ? 125 : 90));
            }
            why[2] += pass == 7 && blocked ? 1 : 0;
            for (s32 need : s.mNeeds)
            {
                blocked = blocked || world.Switch(need) == 0;
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
    if (++mTries > 6)
    {
        return TaskStep::Failed("switch " + std::to_string(entity->mSwitch) + " didn't change");
    }
    switch (entity->mKind)
    {
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
            // A wheel turns for as long as up is held
            return TaskStep::Do(Prim::Wait(45, Btn::eUp));
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
        if (!slig.mAsleep && slig.mHalf == half && std::abs(slig.mY - y) <= 45 && std::abs(slig.mX - x) < 450)
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
    const Entity* entity = world.Find(mEntity);
    if (!entity || mGaveUp || mCleared >= 2)
    {
        return mInner->Next(bot, lastFailed);
    }
    const s32 x = entity->mLive ? entity->mLiveX : entity->CentreX();
    const s32 y = entity->mLive ? entity->mLiveY : entity->mY2;
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
        mInner = mMake();
        bot.Log("the Slig watching " + mInner->Name() + " is gone");
        return Next(bot, "");
    }

    const SligAt* slig = Watcher(world, x, y, entity->mHalf);
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
    if (abe.mMove == Move::eCrouch)
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
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
    if (abe.mMove == Move::eCrouch && !(mGo && mGo->Disarming()))
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
    if (++mTries > 5)
    {
        return TaskStep::Failed("the exit didn't lead out of the path");
    }
    const NavFeature& f = nav.Input().mFeatures[exit.mFeature];
    if (f.mKind == FeatureKind::ePathExit)
    {
        // Walking on through it does it
        return TaskStep::Do(Prim::Wait(40, abe.mFacingLeft ? Btn::eLeft : Btn::eRight));
    }
    return TaskStep::Do(Prim::Press(Btn::eUp));
}

} // namespace Bot
