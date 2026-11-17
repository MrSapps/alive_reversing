#include "Task.hpp"
#include "Bot.hpp"
#include "Domain.hpp"
#include "ThrowArc.hpp"
#include "FartFlight.hpp"
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

// AE's Sligs in the background of a screen (SligCycle says what they do and when they shoot). The way along such a
// screen goes from cover to cover: a Z cover he is wholly inside, or the next screen. Through the open between two
// it is rolled, crouched in the cover first so that the roll starts on the frame it is asked for, and at full
// health only: there may be no moment when none of them looks (the Barracks' first screen has three out of step),
// and then it is the one when the fewest do. Their periods are learnt by watching them from the cover.
const SligAt* GoToTask::Sentry(const World& world, s32 x, s32 y, bool half)
{
    const s32 scale = half ? 2 : 1;
    const SligAt* best = nullptr;
    for (const SligAt& slig : world.mSligs)
    {
        // (on that floor, not one a step lower; Slig.hpp's motions: 2 walks, 4 runs)
        if (!slig.mAsleep && !slig.mPossessed && slig.mSeen && slig.mHalf == half && std::abs(slig.mY - y) * scale <= 12 && std::abs(slig.mX - x) * scale <= 250 && slig.mMotion != 2 && slig.mMotion != 4 &&
            (!best || std::abs(slig.mX - x) < std::abs(best->mX - x)))
        {
            best = &slig;
        }
    }
    return best;
}

s32 GoToTask::JumpUpPastGuard(const NavGraph& nav, const World& world, s32 cell, const NavEdge& hoistUp)
{
    if (!nav.Input().mIsAe || hoistUp.mKind != EdgeKind::eHoistUp || hoistUp.mFeature < 0)
    {
        return 0;
    }
    const NavNode& at = nav.Nodes()[cell];
    const NavNode& under = nav.Nodes()[hoistUp.mFrom];
    const NavFeature& hoist = nav.Input().mFeatures[hoistUp.mFeature];
    const s32 step = NavGraph::GridStep(at.mHalf);
    const s32 way = under.mX > at.mX ? 1 : -1;
    if (at.mLine != under.mLine || std::abs(under.mX - at.mX) != 3 * step || (hoist.mDir != 2 && (hoist.mDir == 0) != (way < 0)) || (under.mY - nav.Nodes()[hoistUp.mTo].mY) * (at.mHalf ? 2 : 1) > 130)
    {
        return 0;
    }
    const SligAt* guard = Sentry(world, at.mX, at.mY, at.mHalf);
    return guard && (guard->mX - at.mX) * way > 0 ? at.mX + 2 * step * way : 0;
}

bool GoToTask::TurnsFrom(const SligAt& slig, s32 x)
{
    // (5 turns where it stands, and faces the other way when that is done)
    return slig.mMotion == 5 && (x < slig.mX) == slig.mFacingLeft;
}

bool GoToTask::UpBehind(const NavGraph& nav, const World& world, s32 ledge, const SligAt** why)
{
    const NavNode& at = nav.Nodes()[ledge];
    const SligAt* guard = Sentry(world, at.mX, at.mY, at.mHalf);
    if (why)
    {
        *why = guard;
    }
    if (!guard)
    {
        return true;
    }
    if (!TurnsFrom(*guard, at.mX))
    {
        return false;
    }
    const s32 cam = nav.CameraOf(at.mX, at.mY - 5);
    const s32 step = NavGraph::GridStep(at.mHalf);
    s32 camX1 = at.mX;
    s32 camX2 = at.mX;
    while (camX1 > at.mX - 2000 && nav.CameraOf(camX1 - step, at.mY - 5) == cam)
    {
        camX1 -= step;
    }
    while (camX2 < at.mX + 2000 && nav.CameraOf(camX2 + step, at.mY - 5) == cam)
    {
        camX2 += step;
    }
    for (const SligAt& other : world.mSligs)
    {
        // (one on a floor a step up or down sees him as he comes up: as near as the game's test of the two
        // boxes goes)
        if (&other != guard && !other.mAsleep && !other.mPossessed && other.mHalf == at.mHalf && std::abs(other.mY - at.mY) * (at.mHalf ? 2 : 1) <= 45 && other.FramesUntilSees(at.mX, camX1 - step, camX2 + step) < kUpAndOff)
        {
            if (why)
            {
                *why = &other;
            }
            return false;
        }
    }
    return true;
}

bool GoToTask::ZWatched(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, TaskStep& step)
{
    const AbeState& abe = bot.Abe();
    const NavNode& here = nav.Nodes()[cur];
    if (!bot.IsAe() || here.mHalf || path.empty() || bot.GetWorld().mInvisible)
    {
        return false;
    }
    const s32 cam = nav.CameraOf(here.mX, here.mY - 5);
    bool any = false;
    for (const SligAt& slig : bot.GetWorld().mSligs)
    {
        any = any || (slig.mZShoot && !slig.mPossessed && nav.CameraOf(slig.mX, slig.mY - 5) == cam);
    }
    const NavEdge& first = nav.Edges()[path[0]];
    if (any && mZLeapt && mZSideX != 0 && cur == mZSideAt && here.mX != mZSideX)
    {
        // Landed where the next leap starts: into the cover beside it first
        if (!abe.Settled())
        {
            mSteps--;
            step = TaskStep::Do(Prim::Wait(1));
            return true;
        }
        step = TaskStep::Do(Prim::MoveTo(mZSideX, Gait::eWalk));
        return true;
    }
    if (cur != mZSideAt && here.mZCover)
    {
        mZSideX = 0;
        mZSideAt = -1;
    }
    if (any && (here.mZCover || here.mZLow) && cur != mZCoverCur)
    {
        // (from the open: a step within the cover is the same stay in it)
        if (mZCoverCur < 0)
        {
            mZArrived = static_cast<s32>(abe.mFrame);
            mZAvoid.clear();
            mZSettle = false;
            mZBest = -1;
        }
        mZCoverCur = cur;
    }
    // Hurt and in cover: there until it is over, whatever the way on is
    if (any && !mHurry && abe.mHealthFp < 0x10000 && abe.Settled() && (here.mZCover || (here.mZLow && abe.mMove == Move::eCrouch)))
    {
        if (mZSaid != 1)
        {
            mZSaid = 1;
            bot.Log("hurt: in cover from the Sligs in the background until that is over");
        }
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    }
    else if (!here.mZCover && !here.mZLow)
    {
        mZCoverCur = -1;
    }
    // (a ride on a lift out of a cover as well: the Barracks' path 5, up the shaft between two of them)
    if (any && !mHurry && (first.mKind == EdgeKind::eHop || first.mKind == EdgeKind::eFall || first.mKind == EdgeKind::eHoistDown || first.mKind == EdgeKind::eHoistUp || first.mKind == EdgeKind::eRunJump || first.mKind == EdgeKind::eLift))
    {
        return nav.Nodes()[cur].mZCover && ZLeap(bot, nav, path, cur, 0, step);
    }
    if (!any || mHurry || (first.mKind != EdgeKind::eWalk && first.mKind != EdgeKind::eRoll))
    {
        return false;
    }
    const auto safe = [&](const NavNode& n)
    {
        return n.mZCover || nav.CameraOf(n.mX, n.mY - 5) != cam;
    };
    const auto to = [&](std::size_t i) -> const NavNode&
    {
        return nav.Nodes()[nav.Edges()[path[i]].mTo];
    };
    // The cells ahead on this floor, the same way
    std::size_t run = 0;
    while (run < path.size() && (nav.Edges()[path[run]].mKind == EdgeKind::eWalk || nav.Edges()[path[run]].mKind == EdgeKind::eRoll) && nav.Edges()[path[run]].mLeft == first.mLeft)
    {
        run++;
    }
    std::size_t open = 0;
    while (open < run && safe(to(open)))
    {
        open++;
    }
    if (safe(here))
    {
        mZLeapt = false;
    }
    if (safe(here) && open == run)
    {
        return false;
    }
    // (with no cover where he is nor on the way ahead there is nothing to go between: as ever then. The
    // Mudanchee trial's last room has such a Slig over a floor he only runs along and drops off.)
    bool cover = here.mZCover || here.mZLow;
    for (std::size_t i = 0; i < run; i++)
    {
        cover = cover || to(i).mZCover || to(i).mZLow;
    }
    if (!cover)
    {
        return false;
    }
    if (safe(here) && open > 0)
    {
        // In cover as far as it goes
        if (abe.mMove == Move::eCrouch)
        {
            return false;
        }
        step = TaskStep::Do(Prim::MoveTo(to(open - 1).mX, Gait::eWalk));
        return true;
    }
    // The open ahead, as far as the next cover (or the end of the floor's run)
    std::size_t far = 0;
    while (far + 1 < run && !safe(to(far)) && !to(far).mZLow)
    {
        far++;
    }
    if (!safe(here) && mZLeapt && far < 3 && safe(to(far)) && abe.mMove != Move::eCrouch)
    {
        // Landed a step or two short of the cover the leap was timed for: straight into it, as far as he can
        // walk (under a low one he rolls the rest, below)
        std::size_t walk = 0;
        while (walk <= far && nav.Edges()[path[walk]].mKind == EdgeKind::eWalk)
        {
            walk++;
        }
        if (walk > 0)
        {
            step = TaskStep::Do(Prim::MoveTo(to(walk - 1).mX, Gait::eWalk));
            return true;
        }
    }
    // (under a low cover he is only out of sight crouched)
    const bool hid = safe(here) || (here.mZLow && abe.mMove == Move::eCrouch);
    // A hop, a drop or a hoist at the end of the floor ahead with no cover before it: all of it at once, timed
    // from here. A cell or two from a cover he stands in are walked, more is rolled.
    const bool leapNext = run < path.size() && far + 1 == run && !safe(to(far)) && !to(far).mZLow &&
                          (nav.Edges()[path[run]].mKind == EdgeKind::eHop || nav.Edges()[path[run]].mKind == EdgeKind::eFall || nav.Edges()[path[run]].mKind == EdgeKind::eHoistDown || nav.Edges()[path[run]].mKind == EdgeKind::eHoistUp || nav.Edges()[path[run]].mKind == EdgeKind::eRunJump);
    if (hid && leapNext && here.mZCover && run <= 2 && abe.mMove != Move::eCrouch)
    {
        return ZLeap(bot, nav, path, cur, run, step);
    }
    if (!hid)
    {
        // Out in it: crouched, and on to that cover as they let him
        // (not where down is the way down a hoist: a step on first. The rifle range's fourth screen, 1362,420.)
        bool hoist = false;
        for (const NavEdge& e : nav.Edges())
        {
            hoist = hoist || (e.mFrom == cur && e.mKind == EdgeKind::eHoistDown);
        }
        if (abe.mMove == Move::eIdle && hoist && abe.Settled())
        {
            step = TaskStep::Do(Prim::MoveTo(to(0).mX, Gait::eWalk));
            return true;
        }
        if (abe.mMove == Move::eIdle)
        {
            step = TaskStep::Do(Prim::Press(Btn::eDown));
            return true;
        }
        if (abe.mMove != Move::eCrouch)
        {
            mSteps--;
            step = TaskStep::Do(Prim::Wait(1));
            return true;
        }
        return ZMove(bot, nav, path, cur, far, step);
    }
    const auto wait = [&](s32 said, const std::string& what)
    {
        if (mZSaid != said)
        {
            mZSaid = said;
            bot.Log(what);
        }
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    };
    if (!abe.Settled())
    {
        return wait(mZSaid, "");
    }
    if (abe.mMove == Move::eCrouch && abe.mFacingLeft != first.mLeft)
    {
        step = TaskStep::Do(Prim::Press(Btn::eUp));
        return true;
    }
    if (abe.mMove == Move::eIdle && abe.mFacingLeft != first.mLeft)
    {
        step = TaskStep::Do(Prim::Face(first.mLeft));
        return true;
    }
    // (on a lift, or at the top of a hoist, down is no crouch: walked from there. The rifle range's last screen,
    // off the lift at 612,480: he held on to the lift for good.)
    bool noCrouch = here.mLiftStop >= 0;
    for (const NavEdge& e : nav.Edges())
    {
        noCrouch = noCrouch || (e.mFrom == cur && e.mKind == EdgeKind::eHoistDown);
    }
    if (abe.mMove == Move::eIdle && noCrouch)
    {
        return ZWalk(bot, nav, path, cur, far, step);
    }
    if (abe.mMove == Move::eIdle)
    {
        step = TaskStep::Do(Prim::Press(Btn::eDown));
        return true;
    }
    if (abe.mMove != Move::eCrouch)
    {
        return wait(mZSaid, "");
    }
    if (abe.mHealthFp < 0x10000)
    {
        return wait(1, "hurt: in cover from the Sligs in the background until that is over");
    }
    const SligCycle& cycle = bot.GetSligCycle();
    // (a period and the stretch that has to match it, twice over for one that is slow to come round)
    if (!cycle.Known() && cycle.Watched() < 700)
    {
        return wait(2, "watching the Sligs in the background from cover");
    }
    if (leapNext && cycle.Known())
    {
        return ZLeap(bot, nav, path, cur, run, step);
    }
    return ZMove(bot, nav, path, cur, far, step);
}

bool GoToTask::ZLeap(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, std::size_t lead, TaskStep& step)
{
    const AbeState& abe = bot.Abe();
    const NavNode& here = nav.Nodes()[cur];
    const SligCycle& cycle = bot.GetSligCycle();
    const NavEdge& first = nav.Edges()[path[0]];
    const NavEdge& leap = nav.Edges()[path[lead]];
    const bool rolled = abe.mMove == Move::eCrouch;
    const auto wait = [&](s32 said, const std::string& what)
    {
        if (mZSaid != said)
        {
            mZSaid = said;
            bot.Log(what);
        }
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    };
    // (standing: the cover is one he fits in so, and the leap starts on the frame it is asked for)
    if (lead == 0 && abe.mMove == Move::eCrouch)
    {
        step = TaskStep::Do(Prim::Press(Btn::eUp));
        return true;
    }
    if (!abe.Settled() || (abe.mMove != Move::eIdle && abe.mMove != Move::eCrouch))
    {
        return wait(mZSaid, "");
    }
    if (!rolled && (lead > 0 || first.mKind == EdgeKind::eHop || first.mKind == EdgeKind::eFall) && abe.mFacingLeft != first.mLeft)
    {
        step = TaskStep::Do(Prim::Face(first.mLeft));
        return true;
    }
    // The way he has to face for the leap, where that is known (a hoist that is only climbed from one side)
    bool faced = false;
    bool faceLeft = false;
    if (leap.mKind == EdgeKind::eHop || leap.mKind == EdgeKind::eFall)
    {
        faced = true;
        faceLeft = leap.mLeft;
    }
    else if (leap.mFeature >= 0 && nav.Input().mFeatures[leap.mFeature].mKind == FeatureKind::eHoist && nav.Input().mFeatures[leap.mFeature].mDir != 2)
    {
        faced = true;
        faceLeft = nav.Input().mFeatures[leap.mFeature].mDir == 0;
    }
    if (lead == 0 && faced && abe.mFacingLeft != faceLeft)
    {
        step = TaskStep::Do(Prim::Face(faceLeft));
        return true;
    }
    if (abe.mHealthFp < 0x10000)
    {
        return wait(1, "hurt: in cover from the Sligs in the background until that is over");
    }
    if (!cycle.Known())
    {
        // (they are put off their beat when they shoot: until it is known again, for as long as that can take)
        if (cycle.Watched() < 700 || static_cast<s32>(abe.mFrame) - mZArrived < 700)
        {
            return wait(2, "watching the Sligs in the background from cover");
        }
        return false;
    }
    s32 periods = 0;
    for (s32 i = 0; i < cycle.Count(); i++)
    {
        periods = periods * 1000 + cycle.Period(i);
    }
    // (worked out again every so often while it waits: what they will do is only as good as what they did a
    // period ago)
    if (mZFrom != cur || mZPeriods != periods || static_cast<s32>(abe.mFrame) > mZGo || static_cast<s32>(abe.mFrame) - mZPlanned >= 40)
    {
        // Where he is on each frame of it, as measured in the Barracks' rifle range: a roll takes a frame to
        // begin, four a cell and one to end; up from a crouch 9; a turn 13; a step 3 to begin and 9 a cell; a hop
        // 11 on the spot, 12 in the air and 10 to land; a hoist up 40 and one down 34, where he stands
        const s32 cam = nav.CameraOf(here.mX, here.mY - 5);
        std::vector<SligCycle::Cell> track;
        // (what of him has to be in a cover, as measured: 14 either side and 52 high standing, up to 26 either
        // side and 55 high as he walks or is in the air, 24 ahead, 13 behind and 34 high rolling)
        const auto add = [&](const NavNode& a, const NavNode& b, s32 frames, bool roll = false)
        {
            for (s32 f = 1; f <= frames; f++)
            {
                const s32 x = a.mX + (b.mX - a.mX) * f / frames;
                const s32 y = a.mY + (b.mY - a.mY) * f / frames;
                const bool left = b.mX < a.mX;
                const bool still = &a == &b;
                const s32 side = still ? 14 : 26;
                track.push_back({x, nav.CameraOf(x, y - 5) != cam || nav.ZCovered(x, y, roll ? (left ? -24 : -13) : -side, roll ? (left ? 13 : 24) : side, roll ? 34 : still ? 52 : 55)});
            }
        };
        const auto cells = [](const NavNode& a, const NavNode& b)
        {
            return std::max(1, std::abs(a.mX - b.mX) / NavGraph::GridStep(a.mHalf));
        };
        const NavNode* at = &here;
        if (lead > 0)
        {
            const NavNode& end = nav.Nodes()[nav.Edges()[path[lead - 1]].mTo];
            if (rolled)
            {
                add(here, here, 1, true);
                add(here, end, 4 * cells(here, end), true);
                add(end, end, 1 + 9);
            }
            else
            {
                add(here, here, 3);
                add(here, end, 9 * cells(here, end));
            }
            at = &end;
        }
        const NavNode& from = *at;
        at = &nav.Nodes()[leap.mTo];
        if (lead > 0 && faced && first.mLeft != faceLeft)
        {
            add(from, from, 3 + 13);
        }
        switch (leap.mKind)
        {
            case EdgeKind::eHop:
                add(from, from, 11);
                add(from, *at, 12);
                add(*at, *at, 10);
                break;
            case EdgeKind::eFall:
                add(from, from, 3);
                add(from, *at, 9 + 14);
                add(*at, *at, 10);
                break;
            case EdgeKind::eHoistUp:
                add(from, from, 12);
                add(*at, *at, 28);
                break;
            case EdgeKind::eRunJump:
                // (a run of 6 a frame, and up from his hands if it ends hung from a ledge)
                add(from, from, 6);
                add(from, *at, std::max(6, std::abs(at->mX - from.mX) / 6));
                add(*at, *at, at->mY < from.mY - 20 ? 30 : 8);
                break;
            case EdgeKind::eLift:
                // (measured in the Barracks' path 5: 7 frames until it moves, then 4 a frame)
                add(from, from, 7);
                add(from, *at, std::max(1, std::abs(at->mY - from.mY) / 4));
                add(*at, *at, 4);
                break;
            default:
                add(from, from, 12);
                add(*at, *at, 22);
                break;
        }
        const NavNode& landed = *at;
        // On along the floor into the next cover: walked, and where the cover is too low to stand under, rolled
        // into from the last cell he can stand in (the rifle range's fourth screen: the cutout he hops down
        // for is four cells on from where he lands, and the last of them is under it)
        const NavNode* walked = at;
        bool rolls = false;
        for (std::size_t i = lead + 1; i < path.size() && i < lead + 7 && !at->mZCover && nav.CameraOf(at->mX, at->mY - 5) == cam; i++)
        {
            const NavEdge& on = nav.Edges()[path[i]];
            if (on.mKind == EdgeKind::eWalk && !rolls)
            {
                at = &nav.Nodes()[on.mTo];
                walked = at;
            }
            else if (on.mKind == EdgeKind::eRoll)
            {
                rolls = true;
                at = &nav.Nodes()[on.mTo];
            }
            else
            {
                break;
            }
        }
        mZSideX = 0;
        mZSideAt = -1;
        const bool inOpen = !at->mZCover && nav.CameraOf(at->mX, at->mY - 5) == cam;
        if (inOpen)
        {
            // No cover the way it goes on: one a step or two aside of where the leap lands, if there is one
            at = &landed;
            const s32 cell = NavGraph::GridStep(landed.mHalf);
            for (s32 d = 1; d <= 2 && at == &landed; d++)
            {
                for (s32 way = -1; way <= 1 && at == &landed; way += 2)
                {
                    const s32 side = nav.NodeAt(landed.mX + way * d * cell, landed.mY, landed.mHalf, 4);
                    if (side >= 0 && nav.Nodes()[side].mLine == landed.mLine && nav.Nodes()[side].mZCover)
                    {
                        at = &nav.Nodes()[side];
                    }
                }
            }
            walked = at;
        }
        if (at != &landed)
        {
            // (the cover it was timed for is where he goes when he has landed, whatever the way is then)
            if (walked != &landed)
            {
                add(landed, landed, 3);
                add(landed, *walked, 9 * cells(landed, *walked));
                mZSideX = walked->mX;
                mZSideAt = leap.mTo;
            }
            if (walked != at)
            {
                // (down into a crouch 11, then the roll)
                add(*walked, *walked, 11 + 1);
                add(*walked, *at, 4 * cells(*walked, *at), true);
                add(*at, *at, 1, true);
            }
        }
        else if (inOpen)
        {
            // (it ends in the open: what is shot at him there counts)
            add(*at, *at, 40);
        }
        std::vector<SligCycle::Shooter> shooters;
        for (const SligAt& slig : bot.GetWorld().mSligs)
        {
            // (those Bot watches, in its order)
            if (slig.mZShoot && !slig.mPossessed && nav.CameraOf(slig.mX, slig.mY - 5) == cam)
            {
                shooters.push_back({slig.mZShootDelay, slig.mZShots});
            }
        }
        // (within a while with none of them looking, or else when the fewest do: two at most, as he is at full
        // health)
        // (and with flying mines over the same stretch, only when those let him too)
        const BombCycle& mines = bot.GetBombCycle();
        const std::function<bool(s32)> minesLet = [&](s32 delay)
        {
            return BombsClear(nav, mines, path, lead + 1, rolled, 0, delay);
        };
        const std::function<bool(s32)>* let = mines.Any() ? &minesLet : nullptr;
        SligCycle::Move move = cycle.Through(track, shooters, 600, let);
        if (move.mSpotters == 99)
        {
            // (no time the mines leave in that while: looked at again later)
            mZPlanned = static_cast<s32>(abe.mFrame);
            mZFrom = cur;
            mZPeriods = periods;
            mZGo = static_cast<s32>(abe.mFrame) + 600;
            return wait(3, "out of the cover: the flying mines leave no time for it yet");
        }
        if (move.mSpotters > 0 && !mZSettle && !mZAvoid.count(path[lead]))
        {
            // Shot at on this one whenever it is made: another way out of the cover, if there is one
            mZAvoid.insert(path[lead]);
            if (mZBest < 0 || move.mSpotters < mZBestBursts)
            {
                mZBest = path[lead];
                mZBestBursts = move.mSpotters;
            }
            bot.Log(std::string("out of the cover by ") + EdgeKindName(leap.mKind) + " to " + std::to_string(at->mX) + ": " + std::to_string(move.mSpotters) + " bursts at him at best: looking for another way");
            mZFrom = -1;
            mSteps--;
            step = TaskStep::Do(Prim::Wait(1));
            return true;
        }
        // (a burst can take two thirds of his health: no more than one, unless nothing else has come up in a
        // long while)
        // (a burst can take two thirds of his health: with more than one in it, only if that is the best there
        // is in a long while, and never three)
        if (move.mSpotters > 1)
        {
            const SligCycle::Move later = cycle.Through(track, shooters, 2400, let);
            if (later.mSpotters < move.mSpotters)
            {
                move = later;
            }
            if (move.mSpotters > 2)
            {
                move.mDelay = 2400;
            }
        }
        mZPlanned = static_cast<s32>(abe.mFrame);
        mZFrom = cur;
        mZPeriods = periods;
        mZGo = static_cast<s32>(abe.mFrame) + move.mDelay;
        const std::string what = std::string("out of the cover by ") + EdgeKindName(leap.mKind) + " to " + std::to_string(at->mX) + ": " + (move.mSpotters > 0 ? std::to_string(move.mSpotters) + " bursts at him from the Sligs in the background at best" : "none of the Sligs in the background sees him then") + " in " + std::to_string(move.mDelay) + " frames (periods " + std::to_string(periods) + ", " + std::to_string(track.size()) + " frames of it)";
        if (what.substr(0, 60) != mZWhat || move.mDelay == 0)
        {
            mZWhat = what.substr(0, 60);
            bot.Log(what);
        }
    }
    if (static_cast<s32>(abe.mFrame) < mZGo)
    {
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    }
    // (with flying mines over the same stretch: when both let him. Looked at again a frame on.)
    if (bot.GetBombCycle().Any() && mZBombWait < 300 && !BombsClear(nav, bot.GetBombCycle(), path, lead + 1, rolled, 0))
    {
        mZBombWait++;
        mZFrom = -1;
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    }
    mZBombWait = 0;
    mZFrom = -1;
    mZSaid = -1;
    mZLeapt = true;
    if (lead > 0)
    {
        step = TaskStep::Do(Prim::MoveTo(nav.Nodes()[nav.Edges()[path[lead - 1]].mTo].mX, rolled ? Gait::eRoll : Gait::eWalk));
        return true;
    }
    return false;
}

bool GoToTask::ZWalk(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, std::size_t far, TaskStep& step)
{
    const AbeState& abe = bot.Abe();
    const NavNode& here = nav.Nodes()[cur];
    const SligCycle& cycle = bot.GetSligCycle();
    const NavNode& end = nav.Nodes()[nav.Edges()[path[far]].mTo];
    const auto wait = [&](s32 said, const std::string& what)
    {
        if (mZSaid != said)
        {
            mZSaid = said;
            bot.Log(what);
        }
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    };
    if (abe.mHealthFp < 0x10000)
    {
        return wait(1, "hurt: in cover from the Sligs in the background until that is over");
    }
    if (!cycle.Known() && (cycle.Watched() < 700 || static_cast<s32>(abe.mFrame) - mZArrived < 700))
    {
        return wait(2, "watching the Sligs in the background from cover");
    }
    if (mZFrom != cur || static_cast<s32>(abe.mFrame) > mZGo || static_cast<s32>(abe.mFrame) - mZPlanned >= 40)
    {
        // (a step takes 3 frames to begin and 9 a cell; walking his box is up to 26 either side and 55 high)
        const s32 cam = nav.CameraOf(here.mX, here.mY - 5);
        std::vector<SligCycle::Cell> track;
        for (s32 f = 0; f < 3; f++)
        {
            track.push_back({here.mX, nav.ZCovered(here.mX, here.mY, -14, 14, 52)});
        }
        const s32 frames = 9 * std::max(1, std::abs(end.mX - here.mX) / NavGraph::GridStep(here.mHalf));
        for (s32 f = 1; f <= frames; f++)
        {
            const s32 x = here.mX + (end.mX - here.mX) * f / frames;
            track.push_back({x, nav.CameraOf(x, here.mY - 5) != cam || nav.ZCovered(x, here.mY, -26, 26, 55)});
        }
        std::vector<SligCycle::Shooter> shooters;
        for (const SligAt& slig : bot.GetWorld().mSligs)
        {
            if (slig.mZShoot && !slig.mPossessed && nav.CameraOf(slig.mX, slig.mY - 5) == cam)
            {
                shooters.push_back({slig.mZShootDelay, slig.mZShots});
            }
        }
        SligCycle::Move move = cycle.Through(track, shooters, 600);
        if (move.mSpotters > 1)
        {
            const SligCycle::Move later = cycle.Through(track, shooters, 2400);
            if (later.mSpotters < move.mSpotters)
            {
                move = later;
            }
        }
        mZPlanned = static_cast<s32>(abe.mFrame);
        mZFrom = cur;
        mZGo = static_cast<s32>(abe.mFrame) + move.mDelay;
        const std::string what = "out of the cover walking to " + std::to_string(end.mX) + ": " + (move.mSpotters > 0 ? std::to_string(move.mSpotters) + " bursts at him from the Sligs in the background at best" : "none of the Sligs in the background sees him then") + " in " + std::to_string(move.mDelay) + " frames (" + std::to_string(track.size()) + " frames of it)";
        if (what.substr(0, 60) != mZWhat || move.mDelay == 0)
        {
            mZWhat = what.substr(0, 60);
            bot.Log(what);
        }
    }
    if (static_cast<s32>(abe.mFrame) < mZGo)
    {
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    }
    mZFrom = -1;
    mZSaid = -1;
    mZLeapt = true;
    step = TaskStep::Do(Prim::MoveTo(end.mX, Gait::eWalk));
    return true;
}

// The next roll of the way through the open to the cell `far` along the path, when it is time for it
bool GoToTask::ZMove(Bot& bot, const NavGraph& nav, const std::vector<s32>& path, s32 cur, std::size_t far, TaskStep& step)
{
    const AbeState& abe = bot.Abe();
    const NavNode& here = nav.Nodes()[cur];
    const SligCycle& cycle = bot.GetSligCycle();
    const bool left = nav.Edges()[path[0]].mLeft;
    const s32 cam = nav.CameraOf(here.mX, here.mY - 5);
    s32 periods = 0;
    for (s32 i = 0; i < cycle.Count(); i++)
    {
        periods = periods * 1000 + cycle.Period(i);
    }
    const s32 farX = nav.Nodes()[nav.Edges()[path[far]].mTo].mX;
    // Worked out once for where he is and what is known of them, and kept until it is time
    if (mZFrom != cur || mZPeriods != periods || mZFar != farX || static_cast<s32>(abe.mFrame) > mZGo)
    {
        std::vector<SligCycle::Cell> cells;
        cells.push_back({here.mX, here.mZCover || nav.CameraOf(here.mX, here.mY - 5) != cam});
        for (std::size_t i = 0; i <= far; i++)
        {
            const NavNode& n = nav.Nodes()[nav.Edges()[path[i]].mTo];
            cells.push_back({n.mX, n.mZCover || nav.CameraOf(n.mX, n.mY - 5) != cam});
        }
        // (measured: rolling, his box is from 24 ahead of him to 13 behind and 34 high; crouched, 13 either side)
        const SligCycle::Move move = cycle.Cross(cells, [&](s32 x, bool rolling)
                                                 { return nav.CameraOf(x, here.mY - 5) != cam || nav.ZCovered(x, here.mY, rolling && left ? -24 : -13, rolling && !left ? 24 : 13, 34); });
        mZFrom = cur;
        mZPeriods = periods;
        mZFar = farX;
        mZGo = static_cast<s32>(abe.mFrame) + move.mDelay;
        mZTo = cells[move.mTo].mX;
        // (out in the open there is nothing to wait for: they see him where he is as well as on the way. The
        // rifle range's fourth screen: crouched a cell short of the cutout until he was shot dead.)
        const bool exposed = !here.mZCover && !here.mZLow;
        if (exposed)
        {
            mZGo = static_cast<s32>(abe.mFrame);
        }
        else if (!cycle.Known() && move.mSpotters > 0)
        {
            // No telling what they will do: when none looks as they are now, and after a long wait when no
            // more than one does
            mZWaits++;
            if (move.mSpotters > 1 || mZWaits < 900)
            {
                mZGo = static_cast<s32>(abe.mFrame) + 1;
                mZFrom = -1;
            }
        }
        if (move.mSpotters > 2 && !exposed)
        {
            mZGo = static_cast<s32>(abe.mFrame) + 1;
            mZFrom = -1;
        }
        const std::string what = "to the cover at " + std::to_string(farX) + (mZTo != farX ? " by " + std::to_string(mZTo) : "") + " in " + std::to_string(move.mDelay) + " frames: " + (move.mSpotters > 0 ? std::to_string(move.mSpotters) + " of the Sligs in the background see him at best" : "none of the Sligs in the background looks then") + " (periods " + std::to_string(periods) + ")";
        if (mZWhat != what.substr(0, 28) + std::to_string(mZTo) + std::to_string(move.mSpotters))
        {
            mZWhat = what.substr(0, 28) + std::to_string(mZTo) + std::to_string(move.mSpotters);
            bot.Log(what);
        }
    }
    if (static_cast<s32>(abe.mFrame) < mZGo)
    {
        mSteps--;
        step = TaskStep::Do(Prim::Wait(1));
        return true;
    }
    mZFrom = -1;
    mZWaits = 0;
    mZSaid = -1;
    step = TaskStep::Do(Prim::MoveTo(mZTo, Gait::eRoll));
    return true;
}

s32 GoToTask::SligComes(const Bot& bot, const NavGraph& nav, const NavNode& at)
{
    const s32 camW = nav.Input().mCamW;
    const s32 lo = (at.mX / camW) * camW;
    const s32 hi = lo + camW - 1;
    const s32 cell = NavGraph::GridStep(at.mHalf);
    s32 least = 9999;
    for (const SligAt& slig : bot.GetWorld().mSligs)
    {
        if (slig.mAsleep || slig.mPossessed || !slig.mSeen || slig.mHalf != at.mHalf || std::abs(slig.mY - at.mY) > 10 || slig.mBeatMin >= slig.mBeatMax)
        {
            continue;
        }
        // (it turns two cells short of its beat's ends)
        const s32 endLo = slig.mBeatMin + 2 * cell;
        const s32 endHi = slig.mBeatMax - 2 * cell;
        if (endHi < lo || endLo > hi || endHi - endLo < 2 * cell)
        {
            continue;
        }
        // (how far it walks until it is on his screen with its face to him)
        const auto onto = [lo, hi](s32 x)
        {
            return x < lo ? lo - x : x > hi ? x - hi : 0;
        };
        const bool toward = slig.mFacingLeft == (slig.mX > at.mX);
        s32 walk = onto(slig.mX);
        // (one that stands with its back to it may turn now, for all that can be told: only one that walks
        // away has the rest of its beat to go first)
        if (!toward && slig.mMotion != 0 && slig.mMotion != 5)
        {
            const s32 end = slig.mFacingLeft ? std::min(slig.mX, endLo) : std::max(slig.mX, endHi);
            walk = std::abs(end - slig.mX) + onto(end);
        }
        // (2.75 a frame; running, as it does for a while after a chant, 6.25)
        least = std::min(least, walk * 4 / (slig.mMotion == 4 ? 25 : 11));
    }
    return least;
}

TaskStep GoToTask::Next(Bot& bot, const std::string& lastFailed)
{
    if (mBombSlig)
    {
        const TaskStep step = mBombSlig->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            bot.Log("the Slig did not get to the flying mine: " + step.mWhy);
        }
        mBombSlig.reset();
        return Next(bot, "");
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
    if (mBack)
    {
        const TaskStep step = mBack->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            bot.Log("not back in the shadow: " + step.mWhy);
        }
        mBack.reset();
        mLedgeToward = false;
        mLedgeAway = false;
        return Next(bot, "");
    }
    {
        const s32 at = bot.AbeNode();
        if (at >= 0 && bot.Nav().Nodes()[at].mHidden)
        {
            mSafeNode = at;
        }
    }
    if (mLiftCall)
    {
        const TaskStep step = mLiftCall->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            bot.Log("couldn't call the lift: " + step.mWhy);
        }
        mLiftCall.reset();
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
        // (not where the way down a hoist ends hung from the ledge it leads to: thrown there by a well under it)
        bool down = mLastEdge >= 0 && nav.Edges()[mLastEdge].mKind == EdgeKind::eHoistDown && bot.AbeNode() != nav.Edges()[mLastEdge].mTo;
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
        // Up onto a floor a Slig stands guard on: only as it begins to turn its back
        if (!down && top >= 0 && nav.Input().mIsAe)
        {
            const NavNode& ledge = nav.Nodes()[top];
            const SligAt* guard = nullptr;
            if (!UpBehind(nav, bot.GetWorld(), top, &guard) && mHangWaits < 3000)
            {
                if (!mHangSaid)
                {
                    mHangSaid = true;
                    bot.Log("hung from " + std::to_string(ledge.mX) + "," + std::to_string(ledge.mY) + " under the Slig that stands at " + std::to_string(guard->mX) + ": up as it turns its back, with no other about to see");
                }
                mHangWaits++;
                mSteps--;
                return TaskStep::Do(Prim::Wait(1));
            }
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
            // (AE's stops where it is when the rope is let go: on to the stop the way goes from, the one it is
            // nearest by when both do. The Barracks' first path: the ride up from 2087,1660 was cut short where
            // the screen changes, with a Slig to see on the new one.)
            mLiftStood = lift.mY == mLiftGlideY ? mLiftStood + 1 : 0;
            mLiftGlideY = lift.mY;
            if (mLiftStood >= 4)
            {
                s32 best = -1;
                s32 bestCost = 0;
                for (s32 i = 0; i < static_cast<s32>(nav.Nodes().size()); i++)
                {
                    const NavNode& n = nav.Nodes()[i];
                    std::vector<s32> on;
                    s32 cost = 0;
                    if (n.mLiftStop >= 0 && n.mX >= lift.mX1 && n.mX <= lift.mX2 && n.mHalf == abe.mHalfScale && (mIsGoal(i) || nav.FindPath(i, mIsGoal, bot.EdgeCostFn(), on, &cost)))
                    {
                        cost += std::abs(n.mY - lift.mY);
                        if (best < 0 || cost < bestCost)
                        {
                            best = i;
                            bestCost = cost;
                        }
                    }
                }
                if (best >= 0)
                {
                    mLiftStood = 0;
                    bot.Log("the lift stands between its stops at " + std::to_string(lift.mY) + ": on to " + std::to_string(nav.Nodes()[best].mY));
                    return TaskStep::Do(Prim::Lift(nav.Nodes()[best].mY));
                }
            }
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
                           { return e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eFall || e.mKind == EdgeKind::eLift ? all(e) : -1; }, path)
           && !path.empty();
    }
    if (led && nav.Edges()[path[0]].mKind == EdgeKind::eLift)
    {
        // A lift takes whoever stands on it: the ones that follow are waited for until they do
        bool off = false;
        for (const LiftAt& lift : bot.GetWorld().mLifts)
        {
            if (abe.mX < lift.mX1 || abe.mX > lift.mX2 || std::abs(lift.mY - abe.mY) > 10)
            {
                continue;
            }
            for (const Entity& e : bot.GetWorld().mEntities)
            {
                off = off || (e.mKind == EntityKind::eMudokon && e.mLive && e.mFollowing && (e.mLiveX < lift.mX1 || e.mLiveX > lift.mX2 || std::abs(e.mLiveY - lift.mY) > 10));
            }
        }
        if (off && mLeadWaits < 60)
        {
            mLeadWaits++;
            mSteps--;
            return TaskStep::Do(Prim::Wait(10));
        }
        mLeadWaits = 0;
    }
    bool zOther = false;
    if (!atGoal && !led && (!mZAvoid.empty() || mZSettle) && mZCoverCur >= 0)
    {
        // Without the leaps out of this cover that get him shot (ZLeap), if that is not much further round: a
        // way back the way he came is no other way
        const NavGraph::EdgeCost all = bot.EdgeCostFn(false, true);
        const std::set<s32>& avoid = mZAvoid;
        const auto without = [&](const NavEdge& e) -> s32
        {
            return avoid.count(static_cast<s32>(&e - nav.Edges().data())) ? -1 : all(e);
        };
        std::vector<s32> straight;
        s32 straightCost = 0;
        s32 otherCost = 0;
        zOther = nav.FindPath(cur, mIsGoal, without, path, &otherCost) && !path.empty() && nav.FindPath(cur, mIsGoal, all, straight, &straightCost) && otherCost <= straightCost + 150;
        if (!zOther && !mZSettle)
        {
            // None: the one of them with the least shot at him
            mZAvoid.erase(mZBest);
            mZSettle = true;
            zOther = nav.FindPath(cur, mIsGoal, without, path) && !path.empty();
        }
        if (!zOther)
        {
            path.clear();
        }
    }
    // The way there is a drop down in front of a Slig that faces the landing now, and without it there is none,
    // or only one far longer: waited for where he is until the Slig has turned or gone. The Barracks' path 9:
    // out of the tunnel at 712,340 down to the Mudokons, "when the patrolling Slig below has left"; sent round
    // the other way he rolled back up the tunnel, which the Slig heard each time, and it never walked its beat.
    if (!atGoal && !led && !zOther && nav.Input().mIsAe && mFacingWaits < 800 && !bot.GetWorld().mSligs.empty() && !bot.GetWorld().mInvisible)
    {
        std::vector<s32> now;
        std::vector<s32> later;
        s32 costNow = 0;
        s32 costLater = 0;
        const bool anyNow = nav.FindPath(cur, mIsGoal, bot.EdgeCostFn(false, true), now, &costNow) && !now.empty();
        if (nav.FindPath(cur, mIsGoal, bot.EdgeCostFn(false, true, true), later, &costLater) && !later.empty() && (!anyNow || (later[0] != now[0] && costNow > costLater + 300)))
        {
            if (mFacingWaits++ == 0)
            {
                bot.Log("a Slig faces where the way down lands: waiting at " + std::to_string(nav.Nodes()[cur].mX) + "," + std::to_string(nav.Nodes()[cur].mY) + " for it to turn or go");
            }
            mSteps--;
            return TaskStep::Do(Prim::Wait(3));
        }
    }
    if (!atGoal && !led && !zOther && (!nav.FindPath(cur, mIsGoal, bot.EdgeCostFn(false, true), path) || path.empty()))
    {
        // With a lift on its way to a stop the way goes onto: that is waited for (one a timer sends, after the
        // lever that dropped him here: the Mudomo hub at 3187,1260)
        const NavGraph::EdgeCost all = bot.EdgeCostFn();
        const World& w = bot.GetWorld();
        const auto away = [&nav, &w](const NavEdge& e)
        {
            const NavNode& to = nav.Nodes()[e.mTo];
            return to.mLiftStop >= 0 && nav.Nodes()[e.mFrom].mLiftStop < 0 && !w.LiftAtStop(nav.Input().mFeatures[to.mLiftStop].mX1, nav.Input().mFeatures[to.mLiftStop].mX2, to.mY, nav.Input().mFeatures[to.mLiftStop].mA != 0);
        };
        std::vector<s32> soon;
        if (mLiftComeWaits < 400 && nav.FindPath(cur, mIsGoal, [&](const NavEdge& e) -> s32
                                                 {
                                                     s32 sw = 0;
                                                     s32 value = 0;
                                                     const s32 cost = all(e);
                                                     return cost < 0 && away(e) && !(nav.Condition(e, &sw, &value) && w.Switch(sw) != value) ? e.mCost : cost;
                                                 }, soon))
        {
            for (s32 idx : soon)
            {
                const NavEdge& e = nav.Edges()[idx];
                if (all(e) >= 0 || !away(e))
                {
                    continue;
                }
                const NavNode& stop = nav.Nodes()[e.mTo];
                for (const LiftAt& lift : w.mLifts)
                {
                    if (stop.mX >= lift.mX1 && stop.mX <= lift.mX2)
                    {
                        const bool moves = mLiftComeY != -100000 && lift.mY != mLiftComeY;
                        const bool nearer = std::abs(lift.mY - stop.mY) < std::abs(mLiftComeY - stop.mY);
                        const bool first = mLiftComeY == -100000;
                        mLiftComeY = lift.mY;
                        if (first || (moves && nearer))
                        {
                            if (mLiftComeWaits == 0)
                            {
                                bot.Log("the lift is on its way to " + std::to_string(stop.mX) + "," + std::to_string(stop.mY) + ": waiting for it");
                            }
                            mLiftComeWaits += 2;
                            return TaskStep::Do(Prim::Wait(2));
                        }
                    }
                }
                break;
            }
            // A lift that is at its other stop, with a lever by the way that sends it to this one (a lift mover):
            // the lever is pulled. (Feeco's path 3, where a Slig has ridden it down: "using the lever to summon
            // the lift back up".)
            for (s32 idx : soon)
            {
                const NavEdge& e = nav.Edges()[idx];
                if (all(e) >= 0 || !away(e) || mLiftCalls >= 2)
                {
                    continue;
                }
                const NavNode& stop = nav.Nodes()[e.mTo];
                const s32 moverSwitch = nav.Input().mFeatures[stop.mLiftStop].mSwitch;
                for (const Entity& lever : w.mEntities)
                {
                    if (moverSwitch <= 1 || lever.mKind != EntityKind::eLever || lever.mSwitch != moverSwitch || w.Switch(moverSwitch) != 0)
                    {
                        continue;
                    }
                    const s32 at = Domain::UseNode(nav, lever, nullptr);
                    std::vector<s32> toLever;
                    if (at >= 0 && (at == cur || nav.FindPath(cur, [at](s32 n)
                                                              { return n == at; }, all, toLever)))
                    {
                        mLiftCalls++;
                        mLiftComeWaits = 0;
                        mLiftComeY = -100000;
                        bot.Log("the lift is away from " + std::to_string(stop.mX) + "," + std::to_string(stop.mY) + ": calling it with Lever#" + std::to_string(lever.mId));
                        mLiftCall = std::make_unique<UseTask>(lever.mId, 1);
                        return Next(bot, "");
                    }
                }
                break;
            }
        }
        // (what stops it, for the log: the edges out of what can be got to that the cost function refuses)
        std::string why;
        {
            std::vector<char> got(nav.Nodes().size(), 0);
            std::vector<s32> none;
            nav.FindPath(cur, [&got](s32 n)
                         {
                             got[n] = 1;
                             return false;
                         }, all, none);
            s32 listed = 0;
            for (const NavEdge& e : nav.Edges())
            {
                if (got[e.mFrom] && !got[e.mTo] && all(e) < 0 && listed < 6)
                {
                    listed++;
                    why += std::string(listed == 1 ? " (refused: " : ", ") + EdgeKindName(e.mKind) + " " + std::to_string(nav.Nodes()[e.mFrom].mX) + "," + std::to_string(nav.Nodes()[e.mFrom].mY) + "->" + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY);
                    // (and what of it is known: Sligs watch where it goes, or a switch has it shut)
                    s32 sw = 0;
                    s32 value = 0;
                    const NavNode& to = nav.Nodes()[e.mTo];
                    why += Domain::SligWatched(nav, w, to.mX, to.mY, to.mHalf) ? " watched" : "";
                    why += nav.Condition(e, &sw, &value) && w.Switch(sw) != value ? " shut by " + std::to_string(sw) : "";
                    why += bot.Banned(static_cast<s32>(&e - nav.Edges().data())) ? " banned" : "";
                }
            }
            why += listed ? ")" : "";
        }
        return TaskStep::Failed("no route from " + std::to_string(nav.Nodes()[cur].mX) + "," + std::to_string(nav.Nodes()[cur].mY) + why);
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

    if (!atGoal)
    {
        TaskStep zStep = TaskStep::Done();
        if (ZWatched(bot, nav, path, cur, zStep))
        {
            return zStep;
        }
    }

    // (nor where he has come to is under a flying mine's track, clear of it only crouched: he stays down, and
    // what comes next starts from a crouch. The Barracks' path 5, by the UXB at 537,720.)
    if (atGoal && abe.mMove == Move::eCrouch && MinesOverhead(bot, cur))
    {
        return TaskStep::Done();
    }
    // Crouched after a roll: up again, unless there's more to roll or no room to stand
    // (up from under a mine's track is for BombStep, which times it)
    if (abe.mMove == Move::eCrouch && !nav.Nodes()[cur].mLow && !((mSweepDuck || mBombDuck) && !atGoal) && !MinesOverhead(bot, cur) && (atGoal || (nav.Edges()[path[0]].mKind != EdgeKind::eRoll && !nav.Edges()[path[0]].mCrouched)))
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
    if (bot.GetBombCycle().Any())
    {
        TaskStep step;
        if (BombStep(bot, cur, path, sweepStop, sweepRoll, step))
        {
            return step;
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
        if (w.mInvisComing && !w.mInvisCharge && !w.mInvisible && bot.InvisAtOnce() && mInvisWaits < 120 && !bot.HoldsInvis())
        {
            mInvisWaits++;
            mSteps--;
            return TaskStep::Do(Prim::Wait(2));
        }
        if (w.mInvisCharge && !w.mInvisible && mInvisChants < 30 && !bot.HoldsInvis())
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
                // (the same before a floor Sligs watch that has no shadow; and no chant on a screen with a
                // security orb, so it is the last cell before that floor whose screen has none. Feeco's path 3:
                // "climb to the upper ledge, then chant to become invisible and start running left".)
                const auto orbAt = [&nav, &w](const NavNode& at)
                {
                    for (const Entity& orb : w.mEntities)
                    {
                        if (orb.mKind == EntityKind::eSecurityOrb && !orb.mGone && nav.CameraOf(orb.mX1, orb.mY1) == nav.CameraOf(at.mX, at.mY - 5))
                        {
                            return true;
                        }
                    }
                    return false;
                };
                // (a stretch walked in one go is not stopped in for this: with one such cell left, here)
                s32 laterFree = 0;
                for (std::size_t i = 0; near && i < path.size(); i++)
                {
                    const NavNode& to = nav.Nodes()[nav.Edges()[path[i]].mTo];
                    // (for a crossing only: a thing used unseen by a Slig has its own timing, ClearSligTask)
                    const bool watched = bot.InvisForCrossing() && Domain::SligWatched(nav, w, to.mX, to.mY, to.mHalf);
                    if (watched || Domain::HuntedAt(nav, w, to.mX, to.mY, to.mHalf))
                    {
                        near = i == 0 || (watched && laterFree <= 1);
                        break;
                    }
                    laterFree += orbAt(to) ? 0 : 1;
                }
                near = near && !(bot.InvisForCrossing() && orbAt(from));
            }
            // And whatever the plan says: about to step onto a floor a Scrab can get at him on, from one it cannot
            {
                const NavNode& to = nav.Nodes()[first.mTo];
                // (not on a stretch timed against the Scrab's beat with the power kept for something else)
                near = near || (!(bot.InvisSaved() && bot.KeepsTo()) && Domain::HuntedAt(nav, w, to.mX, to.mY, to.mHalf) && !Domain::HuntedAt(nav, w, from.mX, from.mY, from.mHalf));
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
            // (not with a crowd of them about: the Mudomo trial, where four are let loose behind him and three more
            // wake under the ledge. There is only going on.)
            s32 crowd = 0;
            for (const RockAt& f : bot.GetWorld().mFleechesAwake)
            {
                crowd += f.mHalf == from.mHalf && std::abs(f.mX - from.mX) * scale < 500 ? 1 : 0;
            }
            if (under && !coming && crowd <= 2)
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
    // A Slog awake under the ledge he is about to leave: it leaps up at him, 24 frames at a time in which it does
    // nothing else, and he is down and at a run in as many. So he faces the way he goes and leaves as it leaps.
    // (Slog.cpp: under him it leaps or looks up by turns; once he is within 50 pixels of its floor it comes for him
    // at once if it stands idle. Measured in the Mudanchee trial: left as it stood, it had him as he landed.)
    {
        const NavNode& from = nav.Nodes()[cur];
        const NavNode& down = nav.Nodes()[first.mTo];
        const s32 scale = from.mHalf ? 2 : 1;
        const bool leaves = (first.mKind == EdgeKind::eFall || first.mKind == EdgeKind::eHop) && (down.mY - from.mY) * scale > 40;
        const SlogAt* under = nullptr;
        for (const SlogAt& slog : bot.GetWorld().mSlogs)
        {
            // (not in flight from another: that run is not stopped for this one)
            if (leaves && !mHurry && (mSlogWaits > 0 || abe.Settled()) && !slog.mAsleep && from.mHalf == abe.mHalfScale && std::abs(slog.mY - down.mY) * scale <= 30 && std::abs(slog.mX - from.mX) * scale <= 80 && std::abs(slog.mX - down.mX) * scale <= 200)
            {
                under = &slog;
            }
        }
        if (under && mSlogWaits < 400)
        {
            // (Motion_19_JumpUpwards)
            const bool up = under->mMotion == 19;
            const bool begins = up && !mSlogUp;
            mSlogUp = up;
            if (mSlogWaits++ == 0)
            {
                bot.Log("a Slog awake under this ledge: leaving as it leaps up");
                // (not on the leap it is in the middle of)
                mSlogUp = true;
            }
            if (first.mKind == EdgeKind::eHop && abe.Settled() && abe.mFacingLeft != first.mLeft)
            {
                mSteps--;
                return TaskStep::Do(Prim::Wait(1, first.mLeft ? Btn::eLeft : Btn::eRight));
            }
            if (!begins || !abe.Settled())
            {
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
    // AO: nothing is begun that takes him under a thing that is on its way down from the ceiling, or set off and
    // about to be (Rupture Farms' meat, path 16: the switch box he is in sets it off again each time he comes
    // down into it, as after the hop before). It comes down some 70 wide. Where he is under it already he goes on.
    if (!bot.IsAe())
    {
        const NavNode& here = nav.Nodes()[cur];
        const s32 way = first.mLeft ? -1 : 1;
        const s32 far = nav.Nodes()[first.mTo].mX + (first.mKind == EdgeKind::eWalk ? way * 4 * NavGraph::GridStep(here.mHalf) : 0);
        for (const RockAt& r : bot.GetWorld().mFallers)
        {
            if (r.mChasing && r.mHalf == here.mHalf && r.mY < here.mY && here.mY - r.mY < 450 && std::abs(r.mX - here.mX) > 70 && r.mX >= std::min(here.mX, far) - 70 && r.mX <= std::max(here.mX, far) + 70)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
        }
    }
    // (AO: he doesn't roll out towards a Slig in the background that faces him or is turning, or to nearer than a
    // cell and a half behind one: see the walk below)
    if (first.mKind == EdgeKind::eRoll && !bot.IsAe())
    {
        const NavNode& here = nav.Nodes()[cur];
        const s32 way = first.mLeft ? -1 : 1;
        for (const SligAt& slig : bot.GetWorld().Threats())
        {
            if (slig.mPossessed || slig.mAsleep || slig.mHalf == here.mHalf || nav.CameraOf(slig.mX, slig.mY - 5) != nav.CameraOf(here.mX, here.mY - 5) || (slig.mX - here.mX) * way <= 0)
            {
                continue;
            }
            const bool facesHim = slig.mMotion == 5 || slig.mFacingLeft == (here.mX < slig.mX);
            if (facesHim || (nav.Nodes()[first.mTo].mX - (slig.mX - way * 40)) * way > 0)
            {
                return TaskStep::Do(Prim::Wait(3));
            }
        }
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
            // A Slig awake on this floor and on his screen, and the roll goes through a shadow and out of it: no
            // further than the shadow, and out of it once the Slig is off the screen. (A roll is heard all over
            // the screen: it turns to look, and sees nothing of one who is in the shadow by then. The Barracks'
            // first path, under the overhang at 1562,1760.)
            if (bot.IsAe() && !bot.GetWorld().mInvisible)
            {
                const NavNode& here = nav.Nodes()[cur];
                bool watched = false;
                bool gone = false;
                for (const SligAt& slig : bot.GetWorld().mSligs)
                {
                    // (and one off the screen that stands or comes: it is on it again before long)
                    const s32 about = ClearSligTask::SligAbout(nav, slig, here.mX, here.mY, here.mHalf);
                    watched = watched || about == 1 || (about == 2 && std::abs(slig.mX - here.mX) < 600);
                    gone = gone || about == 3;
                }
                // (the cell he would step out into: on another screen than the Slig, which its beat does not
                // come onto, it is not seen. The Barracks' path 9: in the tunnel at 1137,340, the last cell of the
                // screen of the Slig at 1287, he waited 4,000 frames for it to go.)
                bool watchedNext = false;
                {
                    const NavNode& next = nav.Nodes()[first.mTo];
                    const s32 camW = nav.Input().mCamW;
                    for (const SligAt& slig : bot.GetWorld().mSligs)
                    {
                        const s32 about = ClearSligTask::SligAbout(nav, slig, next.mX, next.mY, next.mHalf);
                        const bool stays = slig.mBeatMax > slig.mBeatMin && (slig.mBeatMax < (next.mX / camW) * camW || slig.mBeatMin >= (next.mX / camW + 1) * camW);
                        watchedNext = watchedNext || about == 1 || (about == 2 && std::abs(slig.mX - next.mX) < 600 && !stays);
                    }
                }
                std::size_t shade = path.size();
                for (std::size_t k = 0; k < last && watched; k++)
                {
                    if (nav.Nodes()[nav.Edges()[path[k]].mTo].mHidden && !nav.Nodes()[nav.Edges()[path[k + 1]].mTo].mHidden)
                    {
                        shade = k;
                    }
                }
                if (shade < last)
                {
                    last = shade;
                }
                else if (watched && watchedNext && here.mHidden && !nav.Nodes()[first.mTo].mHidden && mHiding < 4000)
                {
                    if (mHiding == 0)
                    {
                        bot.Log("in the shadow at " + std::to_string(here.mX) + "," + std::to_string(here.mY) + " until the Slig is off the screen");
                    }
                    mHiding++;
                    mSteps--;
                    return TaskStep::Do(Prim::Wait(1));
                }
                // A sortie with the Slig just gone: rolled on along the floor, which is as fast as a run and
                // has no standing up in it
                if (bot.Dashing() && !watched && gone)
                {
                    while (last + 1 < path.size() && (nav.Edges()[path[last + 1]].mKind == EdgeKind::eWalk || nav.Edges()[path[last + 1]].mKind == EdgeKind::eRoll) && nav.Edges()[path[last + 1]].mLeft == first.mLeft && !nav.Nodes()[nav.Edges()[path[last + 1]].mTo].mHazard)
                    {
                        last++;
                    }
                }
            }
            // (a running jump the same way at the end of the roll: from the roll, with no stop to stand up and
            // take a run-up. Measured in the Mudomo trial: rolled from 2362 he was off at 2462 and hung from the
            // ledge at 2537,620.)
            if (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eRunJump && nav.Edges()[path[last + 1]].mLeft == first.mLeft && sweepStop > last &&
                std::abs(nav.Edges()[path[last + 1]].mX - nav.Nodes()[cur].mX) % (2 * NavGraph::GridStep(nav.Nodes()[cur].mHalf)) == 0)
            {
                mLastEdge = path[last + 1];
                return TaskStep::Do(Prim::RunJump(nav.Edges()[path[last + 1]].mX, first.mLeft));
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
        {
            // A run-up of one cell: only out of a turn does he run at once (from a stand facing the way he is not
            // at a run by the take-off, and goes off the end without the jump: the Mines' path 4, off 2537,1400
            // into the drill's pit, twice). So he is turned away first, and the jump's own run turns him back.
            const bool shortRun = std::abs(first.mX - abe.mX) * (nav.Nodes()[cur].mHalf ? 2 : 1) <= 38;
            if (shortRun && abe.mMove == Move::eIdle && abe.mFacingLeft == first.mLeft && !mJumpTurned)
            {
                mJumpTurned = true;
                return TaskStep::Do(Prim::Face(!first.mLeft));
            }
            if (abe.mFacingLeft != first.mLeft && !(shortRun && mJumpTurned))
            {
                return TaskStep::Do(Prim::Face(first.mLeft));
            }
            mJumpTurned = false;
        }
            mLastEdge = path[0];
            {
                Prim jump = Prim::RunJump(first.mX, first.mLeft);
                // (chased, and six cells of floor to run on where he comes down: on at a run)
                std::size_t on = 1;
                while (on < path.size() && nav.Edges()[path[on]].mKind == EdgeKind::eWalk && nav.Edges()[path[on]].mLeft == first.mLeft)
                {
                    on++;
                }
                jump.mRunOut = bot.RecentlyChased() && first.mFeature < 0 && on >= 7;
                jump.mX2 = first.mX2;
                jump.mX3 = first.mX3;
                return TaskStep::Do(jump);
            }

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
            // (running jumps with no stop between, which are only there with a flying Slig about: straight into
            // them from the run, without the stop at the cell the run-up would begin at)
            if (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eRunJump && nav.Edges()[path[last + 1]].mX2 != 0 && nav.Edges()[path[last + 1]].mLeft == first.mLeft && sweepStop > last && !led)
            {
                const NavEdge& chain = nav.Edges()[path[last + 1]];
                mLastEdge = path[last + 1];
                Prim jump = Prim::RunJump(chain.mX, chain.mLeft);
                jump.mX2 = chain.mX2;
                jump.mX3 = chain.mX3;
                return TaskStep::Do(jump);
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
            // AO: a step from one screen's floor to the next's (the game puts him across what lies between) that
            // leaves fewer than four cells to where this ends is no place for a run: he comes out of it at
            // speed with no room to stop, and off a ledge's end. Up to it as ever, and over it at a walk.
            if (!bot.IsAe())
            {
                const s32 step = NavGraph::GridStep(nav.Nodes()[cur].mHalf);
                for (std::size_t k = 0; k <= last; k++)
                {
                    const NavEdge& e = nav.Edges()[path[k]];
                    if (std::abs(nav.Nodes()[e.mTo].mX - nav.Nodes()[e.mFrom].mX) > 3 * step && last - k < 4)
                    {
                        if (k == 0)
                        {
                            return TaskStep::Do(Prim::MoveTo(nav.Nodes()[nav.Edges()[path[last]].mTo].mX, Gait::eWalk));
                        }
                        last = k - 1;
                        break;
                    }
                }
            }
            // AE: a Slig that stands guard between him and a hoist three cells on: up it by a running jump
            if (last + 1 < path.size() && nav.Edges()[path[last]].mTo == nav.Edges()[path[last + 1]].mFrom)
            {
                const s32 takeoff = JumpUpPastGuard(nav, bot.GetWorld(), cur, nav.Edges()[path[last + 1]]);
                if (takeoff != 0)
                {
                    bot.Log("past the Slig that stands guard at a running jump from " + std::to_string(takeoff) + " that catches the hoist behind it");
                    mLastEdge = path[last + 1];
                    return TaskStep::Do(Prim::RunJump(takeoff, first.mLeft));
                }
            }
            // AE: a Slig that stands guard on this floor between him and a well, its back to him: through it at a
            // run and into the well without stopping. The run is heard, and it turns to it, which is nine frames it
            // sees nothing in; by then he is past it and sliding into the well, where nothing hurts him. Measured
            // in the Barracks' first path, up the hoist at 1987,1400 behind the guard at 1962 with the well at
            // 1912: in the well 12 frames after the run began, not hit. (Hopped, he lands on the well and stands
            // there: dead. Walked or sneaked he is in front of it for a second.)
            if (bot.IsAe() && last + 1 < path.size())
            {
                const NavEdge& into = nav.Edges()[path[last + 1]];
                const NavNode& from = nav.Nodes()[cur];
                const NavNode& end = nav.Nodes()[nav.Edges()[path[last]].mTo];
                const s32 way = first.mLeft ? -1 : 1;
                const SligAt* guard = Sentry(bot.GetWorld(), from.mX, from.mY, from.mHalf);
                if (into.mKind == EdgeKind::eWell && into.mWell >= 0 && guard && (guard->mX - from.mX) * way > 0 && (end.mX - guard->mX) * way > 0 && (end.mX - from.mX) * way >= 3 * NavGraph::GridStep(from.mHalf) &&
                    guard->mFacingLeft == first.mLeft)
                {
                    const NavFeature& well = nav.Input().mFeatures[into.mWell];
                    bot.Log("past the Slig that stands at " + std::to_string(guard->mX) + " at a run and into the well at " + std::to_string(end.mX) + " without stopping");
                    mLastEdge = path[last + 1];
                    return TaskStep::Do(Prim::RunWell(way > 0 ? well.mX1 : well.mX2, way < 0));
                }
            }
            // Sligs on this floor: quietly near a sleeping one, and past one that is awake only where he can't see
            bool sneak = false;
            bool overhead = false;
            bool zRun = false;
            bool zHold = false;
            s32 trailX = 0;
            bool outrun = false;
            const NavNode& here = nav.Nodes()[cur];
            const s32 cell = NavGraph::GridStep(here.mHalf);
            bool dash = false;
            for (const SligAt& slig : bot.GetWorld().Threats())
            {
                // A sortie, and this one has walked off his screen: nothing of it to mind until it is back
                if (bot.IsAe() && bot.Dashing() && slig.mSeen && ClearSligTask::SligAbout(nav, slig, here.mX, here.mY, here.mHalf) == 3)
                {
                    dash = true;
                    continue;
                }
                // A sleeping one hears steps on any floor of its screen; one that is awake sees along its own
                // AO: one awake in the background of his screen shoots into the foreground at what it faces, its
                // delay after it first faces it (Slig.cpp: VIsFacingMe, no invisible zone, no Z cover). Noise has
                // nothing to do with it and bullet walls don't stop it. Its three shots are aimed behind where
                // he runs (Bullet.cpp: his x less two frames of his speed, and 32, 16 and 0 towards the Slig),
                // so they hit one who runs towards it or stands, and miss one who runs away from it. The way
                // by: behind its back while it walks ahead of him, and past it at a run when it stands at the
                // end of its beat, so that it is behind him when it shoots. He doesn't start towards one that
                // faces him. Rupture Farms' path 18, first screen.
                if (!bot.IsAe() && !slig.mPossessed && !slig.mAsleep && slig.mHalf != here.mHalf && nav.CameraOf(slig.mX, slig.mY - 5) == nav.CameraOf(here.mX, here.mY - 5))
                {
                    const s32 way = first.mLeft ? -1 : 1;
                    const bool facesHim = slig.mMotion == 5 || slig.mFacingLeft == (here.mX < slig.mX);
                    const bool ahead = (slig.mX - here.mX) * way > 0;
                    if (!ahead || (!facesHim && slig.mMotion == 0 && std::abs(slig.mX - here.mX) <= 3 * cell))
                    {
                        zRun = true;
                    }
                    else
                    {
                        overhead = true;
                        zHold = facesHim;
                        trailX = slig.mX - way * 40;
                    }
                    continue;
                }
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
                // AO: behind its back is behind it as it walks away from him, and with the time for it. It turns
                // at the end of its beat, two cells short of the bound, after its wait there: what it has left to
                // walk at 2.7 a frame and that wait have to cover his way to the next cover at a sneak's 2.5.
                // He is the slower, so he stays behind it. Path 18's fourth screen: begun as it stood at its
                // beat's end, it turned with him between the two invisible zones.
                // (AE: tried the same with its beat's ends, Feeco's path 9, and it was worse, 7 deaths for 3. What
                // it has there instead: one that stands or turns has not got its back to him, it is about to
                // have its front. One that stood at its beat's end turned as he came out of the shadow at 737
                // for the one at 637, two cells in the open.)
                // (unless it has only just come to stand with its back to all of his way, and stands for long:
                // BeatPause is the least it stands at an end of its beat. The Barracks' path 7: the one by the
                // boom machine walks two cells and stands 120 frames and more each way, never anything else.)
                Stood* stood = nullptr;
                if (slig.mMotion == 0)
                {
                    const u32 now = bot.Abe().mFrame;
                    for (Stood& s : mStood)
                    {
                        if (s.mX == slig.mX && s.mLeft == slig.mFacingLeft)
                        {
                            stood = &s;
                        }
                    }
                    if (!stood)
                    {
                        mStood.push_back({slig.mX, slig.mFacingLeft, now, now});
                        stood = &mStood.back();
                    }
                    if (now - stood->mLast > 12)
                    {
                        stood->mSince = now;
                    }
                    stood->mLast = now;
                }
                if (bot.IsAe() && (slig.mMotion == 0 || slig.mMotion == 5))
                {
                    const s32 need = (static_cast<s32>(lastOpen) + 2) * cell * 10 / 25 + 12;
                    const bool long_ = behind && stood && stood->mSince != stood->mLast && slig.mBeatPause - static_cast<s32>(stood->mLast - stood->mSince) >= need + 12;
                    behind = long_;
                }
                // And two more ways of being out of its sight for as long as the way to the next cover takes (at a
                // sneak's 2.5 a frame), where its beat is known: it walks ahead of him the way he goes, at 2.77,
                // with enough of its beat left (the cells in front of it are behind it by the time he is in
                // them); or it is on another screen than his whole way and will not be on one of them before he
                // is across (AE: it sees what is on its own screen). Feeco's path 9 with its two Sligs out of
                // step: never both with their backs to him, but one leads him while the other is a screen off.
                if (bot.IsAe() && !behind && slig.mBeatMax > slig.mBeatMin)
                {
                    const s32 way = first.mLeft ? -1 : 1;
                    const s32 need = (static_cast<s32>(lastOpen) + 2) * cell * 10 / 25 + 12;
                    const bool walking = slig.mMotion != 0 && slig.mMotion != 5;
                    const s32 left = slig.mFacingLeft ? slig.mX - slig.mBeatMin : slig.mBeatMax - slig.mX;
                    if (walking && slig.mFacingLeft == first.mLeft && (slig.mX - here.mX) * way > cell / 2 && std::max(0, left) * 100 / 277 + slig.mBeatPause >= need)
                    {
                        behind = true;
                    }
                    const s32 camW = nav.Input().mCamW;
                    const s32 farX = nav.Nodes()[nav.Edges()[path[std::min(last, lastOpen + 1)]].mTo].mX;
                    const s32 lo = std::min(here.mX, farX) / camW;
                    const s32 hi = std::max(here.mX, farX) / camW;
                    const s32 at = slig.mX / camW;
                    if (at < lo || at > hi)
                    {
                        // (coming: as far as the edge of the nearest of his screens. Going: its beat's end, its
                        // wait there and the way back first.)
                        const s32 edge = at > hi ? (hi + 1) * camW : lo * camW;
                        const bool coming = slig.mFacingLeft == (at > hi);
                        const s32 frames = coming ? std::abs(slig.mX - edge) * 100 / 277
                                                  : (std::max(0, left) * 2 + std::abs(slig.mX - edge)) * 100 / 277 + slig.mBeatPause;
                        if (frames >= need)
                        {
                            behind = true;
                        }
                    }
                }
                if (!bot.IsAe() && slig.mBeatMax > slig.mBeatMin)
                {
                    const s32 way = first.mLeft ? -1 : 1;
                    const s32 left = slig.mFacingLeft ? slig.mX - (slig.mBeatMin + 2 * cell) : (slig.mBeatMax - 2 * cell) - slig.mX;
                    const s32 need = (static_cast<s32>(lastOpen) + 2) * cell * 10 / 25 + 12;
                    // (ahead of him and going his way, or behind him and going the other)
                    const bool away = slig.mFacingLeft == first.mLeft ? (slig.mX - here.mX) * way > cell / 2 : (here.mX - slig.mX) * way > cell / 2;
                    behind = away && std::max(0, left) * 10 / 27 + slig.mBeatPause >= need;
                }
                const bool safeHere = !open(here);
                // (one not about yet can't be watched: nothing to wait for)
                // (AE's are waited out for longer: two out of step come right only once in a while)
                if (!behind && safeHere && slig.mSeen && mHiding < (bot.IsAe() ? 4000 : 1500))
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
                    // (under something low on the way there it is rolled, all of it: no standing up between)
                    bool low = false;
                    for (s32 k = 1; k <= 8; k++)
                    {
                        const s32 n = nav.NodeAt(here.mX + away * k * cell, here.mY, here.mHalf, 10);
                        if (n < 0 || nav.Nodes()[n].mLine != here.mLine)
                        {
                            break;
                        }
                        low = low || nav.Nodes()[n].mLow;
                        if (nav.Nodes()[n].mHidden && (edge || nav.Nodes()[n].mLow))
                        {
                            mRetreats++;
                            mSteps--;
                            return TaskStep::Do(Prim::MoveTo(nav.Nodes()[n].mX, low ? Gait::eRoll : Gait::eWalk));
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
            // AO: the first cell of the next screen is no place to stop: he comes onto it in the middle of a step
            // and is past it before he can, and turning back for it puts him on the last screen again (path
            // 18's fourth screen: to and fro between the two until the Slig came). One cell on.
            if (!bot.IsAe() && last + 1 < path.size())
            {
                const NavEdge& over = nav.Edges()[path[last]];
                const NavEdge& on = nav.Edges()[path[last + 1]];
                if (std::abs(nav.Nodes()[over.mTo].mX - nav.Nodes()[over.mFrom].mX) > 3 * NavGraph::GridStep(nav.Nodes()[cur].mHalf) && on.mKind == EdgeKind::eWalk && on.mLeft == first.mLeft)
                {
                    last++;
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
            // (and one asleep on the next screen, which is not there to be seen until he is on it: by where the
            // level has it. The Mudomo vault's path 4: the run from 662,1000 to the lever under three of them at
            // 1012 was begun a screen away and woke them.)
            for (const Entity& e : bot.GetWorld().mEntities)
            {
                if (e.mKind != EntityKind::eFleech || e.mGone || e.mInputs[0] == 0 || e.mHalf != here.mHalf || bot.GetWorld().mFleechKilled.count(e.mId) != 0 ||
                    nav.CameraOf(e.CentreX(), e.mY2 - 5) == nav.CameraOf(here.mX, here.mY - 5))
                {
                    continue;
                }
                const s32 reach = (150 + 40) / (here.mHalf ? 2 : 1);
                for (std::size_t i = 0; i <= last; i++)
                {
                    const NavNode& on = nav.Nodes()[nav.Edges()[path[i]].mTo];
                    sneak = sneak || (std::abs(e.CentreX() - on.mX) <= reach && std::abs(e.mY2 - on.mY) <= 200 / (here.mHalf ? 2 : 1) && nav.CameraOf(e.CentreX(), e.mY2 - 5) == nav.CameraOf(on.mX, on.mY - 5));
                }
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
            bool unseenChase = false;
            // (and unseen with a floor Sligs watch on the way: the power runs out, so it is run like a chase,
            // with no stop for a roll, a drop or a climb. Feeco's path 3 gives 235 frames for a way that takes
            // about that.)
            // (when it is a crossing the power is for: a thing used unseen keeps the pace it was timed at)
            for (std::size_t k = 0; bot.GetWorld().mInvisible && bot.InvisRun() && !chased && k < path.size(); k++)
            {
                const NavNode& ahead = nav.Nodes()[nav.Edges()[path[k]].mTo];
                if (Domain::SligWatched(nav, bot.GetWorld(), ahead.mX, ahead.mY, ahead.mHalf))
                {
                    sneak = false;
                    fleech = true;
                    chased = true;
                    unseenChase = true;
                }
            }
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
            bool pack = false;
            // Paramites that are after him (Paramite.cpp, Brain_2_ChasingAbe: a team of them, or one that was
            // set on him) are the same again, and so are ones on their way down a web to be (brain 3, past its
            // wait: the Mudomo vault's path 3, where the first lock's slap lets two down behind him)
            for (const ParamiteAt& p : bot.GetWorld().mParamites)
            {
                if (!p.mPossessed && (p.mBrain == 2 || (p.mBrain == 3 && p.mSub >= 3)) && p.mHalf == here.mHalf && std::abs(p.mY - here.mY) <= 60 && std::abs(p.mX - here.mX) < 700 &&
                    !Domain::GateBetween(nav, bot.GetWorld(), p.mX, here.mX, here.mY, here.mHalf) && !Domain::ParamiteWall(nav, p.mX, here.mX, here.mY, here.mHalf))
                {
                    sneak = false;
                    fleech = true;
                    chased = true;
                    pack = true;
                }
            }
            // And so is a crowd of Fleeches that is after him: one or two can be led about, three or more only
            // run from (the Mudomo trial: four let loose at its lever and more woken on every screen)
            {
                s32 after = 0;
                for (const RockAt& f : bot.GetWorld().mFleechesAwake)
                {
                    after += f.mChasing && f.mHalf == here.mHalf && std::abs(f.mX - here.mX) < 500 && std::abs(f.mY - here.mY) < 200 ? 1 : 0;
                }
                if (after >= 3)
                {
                    sneak = false;
                    fleech = true;
                    chased = true;
                    pack = true;
                }
            }
            // (and once chased in a path, in a hurry for the rest of it: Bot::RecentlyChased)
            // (a pack or a crowd, that is: a Scrab's chase is timed, and its crossings are their own steps)
            if (pack)
            {
                bot.NoteChased();
            }
            else if (bot.RecentlyChased() && !sneak)
            {
                fleech = true;
                chased = true;
            }
            // And no walking the last cells to stop on the right one (a run is left four cells early for that,
            // twenty frames a Scrab does not give): from the run into a roll, which stops where it is told
            if (chased)
            {
                const s32 way = first.mLeft ? -1 : 1;
                // Up a hoist at the end of the run: by the running jump into it from here, if there is one, and
                // not to a stop under it (the walkthrough of the Mudomo trial's last screen: "run right and jump
                // up to the next door". Stopped under the hoist, three Paramites were on him as he stood up.)
                if (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eHoistUp)
                {
                    const s32 top = nav.Edges()[path[last + 1]].mTo;
                    const NavGraph::EdgeCost cost = bot.EdgeCostFn();
                    for (s32 i = here.mFirstEdge; i < here.mFirstEdge + here.mEdgeCount; i++)
                    {
                        const NavEdge& e = nav.Edges()[i];
                        if (e.mKind == EdgeKind::eRunJump && e.mTo == top && e.mLeft == first.mLeft && cost(e) >= 0)
                        {
                            mLastEdge = i;
                            return TaskStep::Do(Prim::RunJump(e.mX, e.mLeft));
                        }
                    }
                }
                // Off the end of the floor at the end of the run, where the way goes on down: without stopping
                // for it (the Mudomo vault's path 3: stopped at the end of the 860 floor to walk off it, the two
                // Paramites behind him had him)
                // (with nothing after him, only where the way goes on the same way below: run off, he comes down a
                // cell or two further out, and back from there is time lost. The Mudanchee trial's last room,
                // off 1762,360 for the teleporter behind him: 33 frames, and the power was gone at the lock.)
                const bool onTheSameWay = last + 2 < path.size() && nav.Edges()[path[last + 2]].mLeft == first.mLeft && (nav.Edges()[path[last + 2]].mKind == EdgeKind::eWalk || nav.Edges()[path[last + 2]].mKind == EdgeKind::eRoll || nav.Edges()[path[last + 2]].mKind == EdgeKind::eRunJump);
                if (last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eFall && nav.Edges()[path[last + 1]].mLeft == first.mLeft && !nav.Edges()[path[last + 1]].mCrouched &&
                    !first.mCrouched && first.mKind == EdgeKind::eWalk && (!unseenChase || onTheSameWay))
                {
                    bot.Log("chased: off the end of the floor at a run");
                    Prim run = Prim::WalkOff(first.mLeft);
                    run.mRunOff = 1;
                    mLastEdge = -1;
                    // (at a run he comes down further out than the walk off that the fall was drawn as: where
                    // the fall's end is what he was making for, anywhere on its floor past it will do, and not
                    // back to it with them behind him)
                    const s32 end = nav.Edges()[path[last + 1]].mTo;
                    if (mIsGoal(end))
                    {
                        const std::function<bool(s32)> was = mIsGoal;
                        const NavGraph* g = &nav;
                        const s32 line = nav.Nodes()[end].mLine;
                        const s32 endX = nav.Nodes()[end].mX;
                        mIsGoal = [was, g, line, endX, way](s32 n)
                        {
                            return was(n) || (n >= 0 && g->Nodes()[n].mLine == line && (g->Nodes()[n].mX - endX) * way > 0 && (g->Nodes()[n].mX - endX) * way <= 150);
                        };
                    }
                    return TaskStep::Do(run);
                }
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
                // (and on under the next low thing a few cells on, and the next: one roll under the lot. The
                // walkthrough of the Mudomo trial: "Duck under the series of rocks. Be sure to let go of the 'Run'
                // button so you don't stand back up between them")
                for (bool more = lowEnd > last; more;)
                {
                    more = false;
                    std::size_t j = lowEnd + 1;
                    while (j < path.size() && j <= lowEnd + 3 && nav.Edges()[path[j]].mKind == EdgeKind::eWalk && nav.Edges()[path[j]].mLeft == first.mLeft)
                    {
                        j++;
                    }
                    if (j < path.size() && j > lowEnd + 1 && nav.Edges()[path[j]].mKind == EdgeKind::eRoll && nav.Edges()[path[j]].mLeft == first.mLeft)
                    {
                        lowEnd = j;
                        while (lowEnd + 1 < path.size() && nav.Edges()[path[lowEnd + 1]].mKind == EdgeKind::eRoll && nav.Edges()[path[lowEnd + 1]].mLeft == first.mLeft)
                        {
                            lowEnd++;
                        }
                        more = true;
                    }
                }
                // (and on from there with a running jump the same way: the run-up, the roll under and the jump are
                // one move. The Mudomo trial: "quickly duck again on the next screen, then immediately jump to the
                // ledge above". Stood up at the end of the roll to take his run-up, the Paramites had him.)
                if (lowEnd + 1 < path.size() && nav.Edges()[path[lowEnd + 1]].mKind == EdgeKind::eRunJump && nav.Edges()[path[lowEnd + 1]].mLeft == first.mLeft)
                {
                    const s32 takeoff = nav.Edges()[path[lowEnd + 1]].mX;
                    // (down in the low part already, or nothing low before the jump: the jump's own run-up, which
                    // is rolled where it has to be. Measured: from a stand at 2362 it rolled under and was off
                    // at 2462. From a run it does not go down by itself: the run into the roll first, to the
                    // first low cell.)
                    if (lowEnd == last ? (takeoff - here.mX) * way >= 3 * cell : (abe.mMove == Move::eCrouch || abe.mMove == Move::eRoll || here.mLow))
                    {
                        mLastEdge = path[lowEnd + 1];
                        Prim jump = Prim::RunJump(takeoff, first.mLeft);
                        std::size_t on = lowEnd + 2;
                        while (on < path.size() && nav.Edges()[path[on]].mKind == EdgeKind::eWalk && nav.Edges()[path[on]].mLeft == first.mLeft)
                        {
                            on++;
                        }
                        jump.mRunOut = nav.Edges()[path[lowEnd + 1]].mFeature < 0 && on >= lowEnd + 8;
                        return TaskStep::Do(jump);
                    }
                    if (lowEnd > last && (to.mX - here.mX) * way >= 3 * cell)
                    {
                        // (to a stop an even number of cells before the take-off: a roll goes two cells at a
                        // time and only comes up into the run at the end of one. Begun at 2387 for the take-off at
                        // 2462 he rolled on off the end of the floor.)
                        const s32 low = nav.Nodes()[nav.Edges()[path[last + 1]].mTo].mX;
                        const s32 pairs = ((takeoff - low) * way + 2 * cell - 1) / (2 * cell);
                        const s32 stop = takeoff - way * 2 * pairs * cell;
                        if ((stop - here.mX) * way >= 3 * cell)
                        {
                            // (and on through the roll into the jump, with no stop there: Pilot::StepRunJump)
                            Prim jump = Prim::RunJump(takeoff, first.mLeft);
                            jump.mRunUp = stop - way * 2 * cell;
                            mLastEdge = path[lowEnd + 1];
                            return TaskStep::Do(jump);
                        }
                    }
                }
                // (through a gate only if it is one that stands open now: Feeco's path 3, the gap its fence is in)
                s32 gateSwitch = 0;
                s32 gateOpen = 0;
                const bool gated = nav.Condition(nav.Edges()[path[lowEnd]], &gateSwitch, &gateOpen);
                if (lowEnd > last && (to.mX - here.mX) * way >= 3 * cell && (!gated || bot.GetWorld().Switch(gateSwitch) == gateOpen))
                {
                    Prim under = Prim::RunRoll(to.mX - way * cell, nav.Nodes()[nav.Edges()[path[lowEnd]].mTo].mX);
                    // (and out of it at a run where the way goes on along the floor: no stop to stand up)
                    // (with six cells of floor to run on at least: into a wall two cells on he is knocked down)
                    std::size_t on = lowEnd + 1;
                    while (on < path.size() && nav.Edges()[path[on]].mKind == EdgeKind::eWalk && nav.Edges()[path[on]].mLeft == first.mLeft)
                    {
                        on++;
                    }
                    under.mRunOut = on >= lowEnd + 7;
                    under.mFaceLeft = first.mLeft;
                    return TaskStep::Do(under);
                }
                if ((to.mX - here.mX) * way >= 6 * cell)
                {
                    return TaskStep::Do(Prim::RunRoll(to.mX - way * 3 * cell, to.mX));
                }
            }
            if (mQuiet && !dash && !bot.GetWorld().mInvisible)
            {
                return TaskStep::Do(Prim::MoveTo(to.mX, Gait::eSneak));
            }
            // (behind a Slig in the background: no nearer than a cell and a half behind it; in front of it: at a run)
            if (overhead && !zRun)
            {
                const s32 way = first.mLeft ? -1 : 1;
                if (zHold)
                {
                    return TaskStep::Do(Prim::Wait(3));
                }
                std::size_t end = path.size();
                for (std::size_t k = 0; k <= last; k++)
                {
                    if ((nav.Nodes()[nav.Edges()[path[k]].mTo].mX - trailX) * way <= 0)
                    {
                        end = k;
                    }
                }
                if (end == path.size())
                {
                    return TaskStep::Do(Prim::Wait(3));
                }
                const s32 x = nav.Nodes()[nav.Edges()[path[end]].mTo].mX;
                return TaskStep::Do(Prim::MoveTo(x, (x - here.mX) * way > 4 * cell ? Gait::eRun : Gait::eWalk));
            }
            if (zRun)
            {
                return TaskStep::Do(Prim::MoveTo(to.mX, Gait::eRun));
            }
            if (outrun || mHurry || fleech || (flier && !sneak) || (dash && !sneak))
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
            s32 moveX = to.mX;
            // (and on to the plate of a teleporter that is next, if it is the cell after: stopped for at the
            // edge of its box and walked onto it is 11 frames more)
            if (bot.GetWorld().mInvisible && last + 1 < path.size() && nav.Edges()[path[last + 1]].mKind == EdgeKind::eTeleport && nav.Edges()[path[last + 1]].mFeature >= 0)
            {
                const NavFeature& tele = nav.Input().mFeatures[nav.Edges()[path[last + 1]].mFeature];
                for (const Entity& e : bot.GetWorld().mEntities)
                {
                    if (e.mKind == EntityKind::eFootSwitch && e.mHalf == tele.mHalf && tele.Contains(e.CentreX(), std::min(e.mY2 - 2, tele.mY2)) && e.mY2 <= tele.mY2 + 12 &&
                        (e.CentreX() - to.mX) * (first.mLeft ? -1 : 1) > 0 && std::abs(e.CentreX() - to.mX) <= cell + 2 && nav.NodeAt(e.CentreX(), to.mY, to.mHalf, 5) >= 0)
                    {
                        moveX = nav.Nodes()[nav.NodeAt(e.CentreX(), to.mY, to.mHalf, 5)].mX;
                    }
                }
            }
            Prim move = Prim::MoveTo(moveX, gait);
            // Unseen the clock is running: a stretch of four cells or six from a stand is run and slid to a stop
            {
                const s32 cells = std::abs(moveX - abe.mX) / cell;
                move.mSlide = gait == Gait::eRun && bot.GetWorld().mInvisible && abe.mMove == Move::eIdle && abe.Settled() && std::abs(abe.mX - here.mX) <= 2 && cells >= 4 && cells <= 8 && cells % 2 == 0;
            }
            return TaskStep::Do(move);
        }

        case EdgeKind::eFall:
        {
            // Down onto a floor a Slig walks: when it cannot be on that screen with its face to where he lands
            // within 110 frames (SligComes), which is as it walks off it. The Barracks' path 9: out of the
            // tunnel at 712,340 "when the patrolling Slig below has left".
            // (and when it is off that screen by the time he lands, 35 frames on: the landing is heard on it.
            // Dropped behind it as it walked away at 700, it turned at the thud and shot.)
            bool hears = false;
            {
                const NavNode& to = nav.Nodes()[first.mTo];
                const s32 camW = nav.Input().mCamW;
                for (const SligAt& slig : bot.GetWorld().mSligs)
                {
                    if (slig.mAsleep || slig.mPossessed || slig.mHalf != to.mHalf || std::abs(slig.mY - to.mY) > 10)
                    {
                        continue;
                    }
                    const bool walks = slig.mMotion != 0 && slig.mMotion != 5;
                    const s32 then = slig.mX + (walks ? (slig.mFacingLeft ? -96 : 96) : 0);
                    hears = hears || slig.mX / camW == to.mX / camW || then / camW == to.mX / camW;
                }
            }
            if (nav.Input().mIsAe && !bot.GetWorld().mInvisible && !nav.Nodes()[first.mTo].mHidden && nav.Nodes()[first.mTo].mY > nav.Nodes()[cur].mY + 30 && mFallWaits < 700 && (hears || SligComes(bot, nav, nav.Nodes()[first.mTo]) < 110))
            {
                if (mFallWaits++ == 0)
                {
                    bot.Log("a Slig walks the floor under " + std::to_string(nav.Nodes()[cur].mX) + "," + std::to_string(nav.Nodes()[cur].mY) + ": down when it has left");
                }
                mSteps--;
                return TaskStep::Do(Prim::Wait(3));
            }
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
            // Down onto a floor a pack of Paramites hunts him on, seen: run off, through every ledge on the way
            // down, and on. A walk off, or a hop with its landing, leaves him standing a cell or two from them.
            {
                s32 falls = 0;
                s32 count = 0;
                // (unseen too: the power's clock is running. Only where the way goes on the same way after the
                // last of them: he comes down at a run.)
                for (std::size_t i = 0; !first.mCrouched && i < path.size() && i < 12; i++)
                {
                    const NavEdge& e = nav.Edges()[path[i]];
                    if ((e.mKind != EdgeKind::eWalk && e.mKind != EdgeKind::eFall) || e.mLeft != first.mLeft)
                    {
                        break;
                    }
                    if (e.mKind == EdgeKind::eFall)
                    {
                        count++;
                        const NavNode& to = nav.Nodes()[e.mTo];
                        const bool on = i + 1 < path.size() && nav.Edges()[path[i + 1]].mLeft == first.mLeft && (nav.Edges()[path[i + 1]].mKind == EdgeKind::eWalk || nav.Edges()[path[i + 1]].mKind == EdgeKind::eRunJump);
                        if (on && Domain::ParamitePack(nav, bot.GetWorld(), to.mX, to.mY, to.mHalf))
                        {
                            falls = count;
                        }
                    }
                }
                if (falls > 0)
                {
                    bot.Log("Paramites on the floor below: off the edge at a run, " + std::to_string(falls) + " down");
                    mLastEdge = -1;
                    Prim run = Prim::WalkOff(first.mLeft);
                    run.mRunOff = falls;
                    return TaskStep::Do(run);
                }
            }
            // (crouched at the end of a roll, with a wall just past the edge: on his feet first. Rolled off he
            // bounces off the wall and comes down on his back: 30 frames, measured in the Mudomo trial at 2112,620,
            // with two Paramites coming down behind him.)
            if (!first.mCrouched && (abe.mMove == Move::eCrouch || abe.mMove == Move::eRoll))
            {
                const NavNode& off = nav.Nodes()[first.mFrom];
                const s32 cell = NavGraph::GridStep(off.mHalf);
                // (not from under something low: there he cannot stand, and it is rolled off as it was drawn)
                if (!off.mLow && Domain::WallBetween(nav, off.mX, off.mX + (first.mLeft ? -3 : 3) * cell, off.mY, off.mHalf))
                {
                    // (out of the roll first: it ends crouched when nothing is held)
                    return TaskStep::Do(abe.mMove == Move::eRoll ? Prim::Wait(1) : Prim::Press(Btn::eUp));
                }
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
            // Up onto a floor a Slig walks, where its beat comes onto the screen he comes up on: not when it can be
            // on that screen and facing him within 90 frames (54 of them the hoist; SligComes). The Barracks'
            // path 7: up at 887,100 as the Slig of 1012 to 1262 came, he was shot the moment it was on his screen.
            if (first.mKind == EdgeKind::eHoistUp && nav.Input().mIsAe)
            {
                const NavNode& top = nav.Nodes()[first.mTo];
                const NavNode& under = nav.Nodes()[first.mFrom];
                const s32 camW = nav.Input().mCamW;
                const s32 cell = NavGraph::GridStep(top.mHalf);
                if (mSligWaits < 100 && SligComes(bot, nav, top) < 90)
                {
                    mSligWaits++;
                    mSteps--;
                    return TaskStep::Do(Prim::Wait(3));
                }
                // Up onto a ledge a Slig stands on and turns: as it turns its back, so that all of the time it
                // stands that way is ahead (its sight is by its box and his: he is in it from the pull up, 30
                // frames on). And one that stands on that level across a gap: when it has its back to him.
                // Waited for under the ledge, which is a floor of its own with a Slig of its own: when that one
                // can be on his screen with its face to him within 75 frames he is off back to the last shadow
                // he was in, to come again when it has gone. The Barracks' path 9 (the trophy room): the Slig
                // at 1162,399 stands 90 to 120 with its back to the hoist at 1212 and 150 to 180 the other way,
                // and the one under it is back every 370.
                bool ledge = false;
                bool go = true;
                for (const SligAt& slig : bot.GetWorld().mSligs)
                {
                    if (slig.mAsleep || slig.mPossessed || !slig.mSeen || slig.mHalf != top.mHalf || std::abs(slig.mY - top.mY) > 10 || slig.mX / camW != top.mX / camW)
                    {
                        continue;
                    }
                    if (slig.mBeatMax - slig.mBeatMin - 4 * cell >= 2 * cell)
                    {
                        continue;
                    }
                    const bool toward = slig.mFacingLeft == (slig.mX > top.mX);
                    const s32 at = nav.NodeAt(slig.mX, slig.mY, slig.mHalf, 12);
                    if (at < 0 || nav.Nodes()[at].mLine != top.mLine)
                    {
                        go = go && !toward;
                        continue;
                    }
                    ledge = true;
                    const u32 now = bot.Abe().mFrame;
                    if (toward)
                    {
                        mLedgeToward = true;
                        mLedgeAway = false;
                    }
                    else if (!mLedgeAway)
                    {
                        mLedgeAway = true;
                        mLedgeSince = now;
                    }
                    go = go && ((!toward && mLedgeToward && now - mLedgeSince <= 8) || (toward && slig.mMotion == 5));
                }
                const s32 comes = ledge ? SligComes(bot, nav, under) : 9999;
                if (ledge && mLedgeWaits < 1500 && (!go || comes < 60))
                {
                    if (comes < 75 && mSafeNode >= 0 && mSafeNode != cur && mLedgeRetreats < 12)
                    {
                        mLedgeRetreats++;
                        bot.Log("a Slig is on its way back to this floor (" + std::to_string(comes) + " frames) before the one on the ledge has turned: back to the shadow at " + std::to_string(nav.Nodes()[mSafeNode].mX) + "," + std::to_string(nav.Nodes()[mSafeNode].mY));
                        const s32 safe = mSafeNode;
                        mBack = std::make_unique<GoToTask>("back to the shadow", [safe](s32 n)
                                                           { return n == safe; });
                        return Next(bot, "");
                    }
                    mLedgeWaits++;
                    mSteps--;
                    return TaskStep::Do(Prim::Wait(1));
                }
                if (ledge)
                {
                    bot.Log("the Slig on the ledge turns its back: up at " + std::to_string(top.mX) + "," + std::to_string(top.mY) + " (the one of this floor " + std::to_string(comes) + " frames off)");
                }
                else if (!go && mSligWaits < 100)
                {
                    mSligWaits++;
                    mSteps--;
                    return TaskStep::Do(Prim::Wait(3));
                }
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
        {
            // Down into a shadow on a floor Sligs walk: he is seen as he drops and lands (not wholly in the
            // shadow until he stands in it), and the landing is heard by every Slig on that screen (AE: a noise
            // is heard on the screen it is made on, Events.cpp), which turns to it. So only while every Slig
            // awake on that floor is walking away from where he comes down and is off that screen. Feeco's
            // path 9, off the ledge at 1037,340: let down as the two came his way he was shot as he landed, and
            // let down behind their backs the nearer one, still on his screen, turned and shot.
            const NavNode& land = nav.Nodes()[first.mTo];
            // (AE's only: AO's Rupture Farms got through as it was, and did not with this)
            if (nav.Input().mIsAe && land.mHidden && mDropWait < 1500)
            {
                for (const SligAt& s : bot.GetWorld().mSligs)
                {
                    if (s.mAsleep || s.mPossessed || s.mHalf != land.mHalf || std::abs(s.mY - land.mY) > 30 || std::abs(s.mX - land.mX) > 700)
                    {
                        continue;
                    }
                    const bool away = s.mFacingLeft == (s.mX < land.mX);
                    const bool room = nav.CameraOf(s.mX, s.mY - 5) != nav.CameraOf(land.mX, land.mY - 5);
                    if (!away || !room)
                    {
                        mDropWait += 2;
                        mSteps--;
                        return TaskStep::Do(Prim::Wait(2));
                    }
                }
            }
            mDropWait = 0;
            mLastEdge = path[0];
            return TaskStep::Do(Prim::Press(Btn::eDown));
        }

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
            // Onto a floor a Slig walks: when it cannot be on that screen with its face to him within 120
            // frames (SligComes), which is as it leaves. The Barracks' path 9: "use it when the bottom Slig has
            // left".
            if (nav.Input().mIsAe && mTeleWaits < 400 && SligComes(bot, nav, nav.Nodes()[first.mTo]) < 120)
            {
                mTeleWaits++;
                mSteps--;
                return TaskStep::Do(Prim::Wait(3));
            }
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

void GoToTask::Bodies(const NavGraph& nav, const std::vector<s32>& path, std::size_t first, std::size_t count, bool rolled, bool rest, std::vector<Body>& out)
{
    // Where Abe is frame by frame: along each edge at an even pace for the frames it takes (measured, see
    // BOT_DESIGN.md)
    EdgeKind before = EdgeKind::eDoor;
    for (std::size_t i = first; i <= first + count && i <= path.size(); i++)
    {
        // (and a moment in the last cell, where what comes next starts)
        const bool last = i == first + count || i == path.size();
        if (last && (i == first || !rest))
        {
            break;
        }
        const NavEdge& e = nav.Edges()[path[last ? i - 1 : i]];
        const NavNode& a = nav.Nodes()[last ? e.mTo : e.mFrom];
        const NavNode& b = nav.Nodes()[e.mTo];
        const EdgeKind kind = rolled && e.mKind == EdgeKind::eWalk ? EdgeKind::eRoll : e.mKind;
        s32 frames = 12;
        if (!last)
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
                case EdgeKind::eLift:
                    // (measured in the Barracks' path 5: 7 frames until it moves, then 4 a frame)
                    frames = 7 + std::max(1, std::abs(b.mY - a.mY) * (a.mHalf ? 2 : 1) / 4);
                    break;
                default:
                    frames = e.mCost;
                    break;
            }
            before = kind;
        }
        const bool climbs = !last && (e.mKind == EdgeKind::eHoistUp || e.mKind == EdgeKind::eHoistDown);
        const s32 scale = a.mHalf ? 2 : 1;
        const bool grabs = !last && (kind == EdgeKind::eHop || kind == EdgeKind::eRunJump) && (a.mY - b.mY) * scale > 20;
        // (measured in the Barracks' path 5: he walks off an edge in 6 frames, falls 0.9 t squared and is 10 on
        // his feet again; a hop is 10 where he stands, 5 level through the air at 15 a frame, then down 3 t + 2.3 t squared, and 9
        // landing)
        const bool falls = !last && kind == EdgeKind::eFall && e.mWell < 0 && !e.mHung;
        const bool hops = !last && kind == EdgeKind::eHop && e.mWell < 0 && !grabs;
        s32 air = 0;
        // (a jump up to a ledge, measured there too: run to where he takes off at 6.25 a frame, or the 10 of a
        // hop where he stands; 3 level through the air, 6 up to the ledge, 3 hung from it and 23 pulling himself up)
        const s32 run = kind == EdgeKind::eRunJump ? std::max(1, std::abs(a.mX - e.mX) * scale * 4 / 25) : 10;
        // (a running jump over a gap that ends hung from the ledge at its own height, measured there as well, off
        // 2012,1660 to the ledge at 2137: 16 frames through the air at 7.6 a frame, up to 31 over where he took
        // off and 15 under it as he catches the ledge, then hung and up as above)
        const bool catches = !last && kind == EdgeKind::eRunJump && !grabs && e.mFeature >= 0 && std::abs(a.mY - b.mY) * scale <= 20 &&
                             (nav.Input().mFeatures[e.mFeature].mKind == FeatureKind::eEdge || nav.Input().mFeatures[e.mFeature].mKind == FeatureKind::eHoist);
        if (grabs)
        {
            frames = run + 35;
        }
        else if (catches)
        {
            frames = run + 16 + 26;
        }
        else if (falls)
        {
            while (9 * air * air < 10 * (b.mY - a.mY) * scale)
            {
                air++;
            }
            frames = 6 + air + 10;
        }
        else if (hops)
        {
            // (10 where he stands, 5 level through the air, and then down)
            s32 drop = 0;
            while (30 * drop + 23 * drop * drop < 10 * (b.mY - a.mY) * scale)
            {
                drop++;
            }
            air = 15 + drop;
            frames = air + 9;
        }
        // (rolled through, or crouched where there is no standing, he is 34 high)
        const bool down = last ? (b.mLow || rolled) : kind == EdgeKind::eRoll;
        const s32 tall = (a.mHalf ? 30 : 60) * (down ? 34 : 60) / 60;
        for (s32 f = 0; f < frames; f++)
        {
            Body body;
            body.mX = a.mX + (b.mX - a.mX) * f / frames;
            // (a hoist: on the lower floor until he jumps, then no lower than he hangs)
            // (measured in the Mines' path 4 at 2062,1500, a drill along the floor 15 to 35 above it: there 3
            // frames before the jump starts, and his feet are 23 up 14 frames into it and out of its reach 17 in)
            const s32 onFloor = e.mKind == EdgeKind::eHoistUp ? frames - 34 : 40;
            const bool hangs = climbs && (e.mKind == EdgeKind::eHoistUp ? f >= onFloor : f < frames - onFloor);
            body.mY = climbs ? (hangs ? std::min(a.mY, b.mY) + tall + 4 : std::max(a.mY, b.mY)) : a.mY + (b.mY - a.mY) * f / frames;
            body.mTall = tall;
            body.mHalf = a.mHalf;
            body.mLeft = e.mLeft;
            // His box about his feet, as it was measured doing each of these (the Barracks' path 5)
            const bool low = down;
            body.mUp = low ? 34 : 55;
            body.mBack = last ? 14 : low ? 24 : 26;
            body.mFront = body.mBack;
            if (last)
            {
                body.mUp = low ? 34 : 53;
            }
            else if (climbs)
            {
                body.mBack = 25;
                body.mFront = 25;
            }
            else if (grabs)
            {
                const s32 dir = b.mX < a.mX ? -1 : 1;
                const s32 off = kind == EdgeKind::eRunJump ? e.mX : a.mX;
                const s32 k = f - run;
                body.mY = a.mY;
                if (k < 0)
                {
                    body.mX = a.mX + (off - a.mX) * f / run;
                    body.mBack = kind == EdgeKind::eRunJump ? 25 : f < 5 ? 23 : f < 9 ? 20 : 14;
                    body.mFront = kind == EdgeKind::eRunJump ? 27 : f < 5 ? 19 : f < 9 ? 30 : 45;
                }
                else if (k < 3)
                {
                    body.mX = off + dir * 13 * (k + 1) / (2 * scale);
                    body.mBack = 20;
                    body.mFront = 27;
                }
                else if (k < 9)
                {
                    const s32 from = off + dir * 19 / scale;
                    body.mX = from + (b.mX - dir * 11 / scale - from) * (k - 2) / 6;
                    body.mY = a.mY + (b.mY + 9 / scale - a.mY) * (k - 2) / 6;
                    body.mUp = 68;
                    body.mBack = 23;
                    body.mFront = 33;
                }
                else
                {
                    // (hung under the ledge on the side he came from, then up over it)
                    body.mX = b.mX;
                    body.mY = b.mY;
                    body.mBack = k < 12 ? 27 : 22;
                    body.mFront = k < 12 ? 6 : 18;
                    body.mUp = k < 12 ? -5 : k < 16 ? 20 : k < 28 ? 36 : 53;
                    body.mDown = k < 12 ? 65 : k < 16 ? 70 : k < 28 ? 34 : 11;
                }
            }
            else if (!last && kind == EdgeKind::eLift)
            {
                body.mX = a.mX;
                body.mY = f < 7 ? a.mY : a.mY + (b.mY - a.mY) * (f - 7) / (frames - 7);
                body.mBack = 23;
                body.mFront = 23;
            }
            else if (catches)
            {
                const s32 dir = b.mX < a.mX ? -1 : 1;
                const s32 k = f - run;
                body.mY = a.mY;
                if (k < 0)
                {
                    body.mX = a.mX + (e.mX - a.mX) * f / run;
                    body.mBack = 25;
                    body.mFront = 27;
                }
                else if (k < 16)
                {
                    const s32 to = b.mX - dir * 11 / scale;
                    body.mX = e.mX + (to - e.mX) * (k + 1) / 16;
                    body.mY = a.mY + (k >= 13 ? 15 : (k - 8) * (k - 8) * 31 / 64 - 31) / scale;
                    body.mUp = 69;
                    body.mBack = 36;
                    body.mFront = 23;
                    body.mDown = 0;
                }
                else
                {
                    const s32 h = k - 16;
                    body.mX = b.mX;
                    body.mY = b.mY;
                    body.mBack = h < 3 ? 27 : 22;
                    body.mFront = h < 3 ? 6 : 18;
                    body.mUp = h < 3 ? -5 : h < 7 ? 20 : h < 19 ? 36 : 53;
                    body.mDown = h < 3 ? 65 : h < 7 ? 70 : h < 19 ? 34 : 11;
                }
            }
            else if (falls)
            {
                const bool flies = f >= 6 && f < 6 + air;
                body.mX = f < 10 ? a.mX + (b.mX - a.mX) * f / 10 : b.mX;
                body.mY = f < 6 ? a.mY : std::min(b.mY, a.mY + 9 * (f - 6) * (f - 6) / (10 * scale));
                body.mUp = flies ? 64 : 59;
                body.mBack = flies ? 35 : 26;
                body.mFront = body.mBack;
            }
            else if (hops)
            {
                // (he leans back, then forward, where he stands before he is off; in the air he trails behind)
                const s32 off = 10;
                const s32 dir = b.mX < a.mX ? -1 : 1;
                const s32 span = std::abs(b.mX - a.mX);
                const s32 level = std::min(span, 15 * 5 / scale);
                body.mX = a.mX;
                body.mY = a.mY;
                if (f < off)
                {
                    body.mBack = f < 5 ? 23 : f < 9 ? 20 : 14;
                    body.mFront = f < 5 ? 19 : f < 9 ? 30 : 45;
                }
                else if (f < off + 5)
                {
                    const s32 k = f - off;
                    body.mX = a.mX + dir * level * (k + 1) / 5;
                    body.mBack = std::min(44, 25 + 10 * k);
                    body.mFront = std::max(6, 37 - 7 * k);
                }
                else if (f < air)
                {
                    const s32 t = f - off - 5 + 1;
                    body.mX = a.mX + dir * (level + (span - level) * t / (air - off - 5));
                    body.mY = std::min(b.mY, a.mY + (30 * t + 23 * t * t) / (10 * scale));
                    body.mUp = 64;
                    body.mBack = 35;
                    body.mFront = 35;
                }
                else
                {
                    body.mX = b.mX;
                    body.mY = b.mY;
                    body.mBack = 26;
                    body.mFront = 26;
                }
            }
            else if (kind != EdgeKind::eWalk && kind != EdgeKind::eRoll)
            {
                body.mUp = 68;
                body.mBack = 35;
                body.mFront = 35;
            }
            out.push_back(body);
        }
    }
}

bool GoToTask::SweepsClear(const NavGraph& nav, const World& world, const std::vector<s32>& path, std::size_t count, s32 late, bool rolled)
{
    // (where he is, give or take kSlack pixels)
    const s32 kSlack = 9;
    std::vector<Body> bodies;
    Bodies(nav, path, 0, count, rolled, true, bodies);
    for (std::size_t k = 0; k < bodies.size(); k++)
    {
        const Body& body = bodies[k];
        const s32 t = static_cast<s32>(k) + late;
        for (const SweepAt& sweep : world.mSweeps)
        {
            s32 x1 = 0;
            s32 y1 = 0;
            s32 x2 = 0;
            s32 y2 = 0;
            if (sweep.mHalf == body.mHalf && sweep.At(t < 0 ? 0 : t, x1, y1, x2, y2) && body.mX + 3 + kSlack >= x1 && body.mX - 3 - kSlack <= x2 && body.mY >= y1 && body.mY - body.mTall <= y2)
            {
                return false;
            }
        }
    }
    return true;
}

BombCycle::Box GoToTask::BombBody(const Body& body)
{
    const s32 scale = body.mHalf ? 2 : 1;
    const s32 left = (body.mLeft ? body.mFront : body.mBack) / scale;
    const s32 right = (body.mLeft ? body.mBack : body.mFront) / scale;
    return BombCycle::Box{body.mX - left, body.mY - body.mUp / scale - 1, body.mX + right, body.mY - 1 + body.mDown / scale};
}

bool GoToTask::BombStop(const NavGraph& nav, const BombCycle& bombs, s32 node, bool& duck)
{
    const NavNode& n = nav.Nodes()[node];
    Body body;
    body.mX = n.mX;
    body.mY = n.mY;
    body.mHalf = n.mHalf;
    body.mUp = n.mLow ? 34 : 53;
    duck = false;
    if (!bombs.Ever(n.mHalf, BombBody(body)))
    {
        return true;
    }
    if (n.mLow || n.mLiftStop >= 0)
    {
        return false;
    }
    body.mUp = 34;
    duck = !bombs.Ever(n.mHalf, BombBody(body));
    return duck;
}

bool GoToTask::BombsClear(const NavGraph& nav, const BombCycle& bombs, const std::vector<s32>& path, std::size_t count, bool rolled, s32 delay, s32 later)
{
    std::vector<Body> bodies;
    // (what he is asked to do he starts on two frames later: measured)
    delay += 2;
    if (!path.empty())
    {
        // (up from a crouch first)
        const NavNode& n = nav.Nodes()[nav.Edges()[path[0]].mFrom];
        Body body;
        body.mX = n.mX;
        body.mY = n.mY;
        body.mHalf = n.mHalf;
        body.mUp = 56;
        bodies.assign(static_cast<std::size_t>(delay), body);
    }
    Bodies(nav, path, 0, count, rolled, true, bodies);
    for (std::size_t k = 0; k < bodies.size(); k++)
    {
        const BombCycle::Box box = BombBody(bodies[k]);
        // (or a frame later than worked out)
        for (s32 slip = 0; slip <= 1; slip++)
        {
            if (bombs.Hits(bodies[k].mHalf, std::max(0, later + static_cast<s32>(k) + slip), box))
            {
                return false;
            }
        }
    }
    return true;
}

// Flying mines on the way: from one place none of them ever reaches (standing, or crouched) to the next in one go,
// when all of that fits in between them. True with the step to take while he waits.
bool GoToTask::MinesOverhead(Bot& bot, s32 node)
{
    bool duck = false;
    return node >= 0 && bot.GetBombCycle().Any() && BombStop(bot.Nav(), bot.GetBombCycle(), node, duck) && duck;
}

bool GoToTask::BombStep(Bot& bot, s32 cur, const std::vector<s32>& path, std::size_t& stop, std::size_t& roll, TaskStep& step)
{
    const NavGraph& nav = bot.Nav();
    const BombCycle& bombs = bot.GetBombCycle();
    const AbeState& abe = bot.Abe();
    mBombDuck = false;
    // The first edge of the way one of them crosses at some time
    std::vector<Body> bodies;
    const std::size_t far = std::min<std::size_t>(path.size(), 24);
    std::size_t in = 0;
    for (; in < far; in++)
    {
        bodies.clear();
        Bodies(nav, path, in, 1, false, in + 1 == path.size(), bodies);
        bool hot = false;
        for (const Body& b : bodies)
        {
            hot = hot || bombs.Ever(b.mHalf, BombBody(b));
        }
        if (hot)
        {
            break;
        }
    }
    if (in == far)
    {
        return false;
    }
    const NavNode& here = nav.Nodes()[cur];
    // The last place before it that none reaches: up to there as ever
    bool duck = false;
    bool found = false;
    std::size_t at = in;
    for (;; at--)
    {
        if (BombStop(nav, bombs, at == 0 ? cur : nav.Edges()[path[at - 1]].mTo, duck))
        {
            found = true;
            break;
        }
        if (at == 0)
        {
            break;
        }
    }
    if (!found)
    {
        // (in their way already: on)
        return false;
    }
    if (at > 0)
    {
        stop = std::min(stop, at);
        return false;
    }
    // From here to the next such place
    std::size_t count = 1;
    bool endDuck = false;
    while (count < path.size() && count < 40 && !BombStop(nav, bombs, nav.Edges()[path[count - 1]].mTo, endDuck))
    {
        count++;
    }
    const bool crouched = abe.mMove == Move::eCrouch && !here.mLow;
    bool go = false;
    // What he does before he is off (measured): up from a crouch in 9, round in 12; and a running jump with a
    // run-up of a cell starts turned away (10 to turn if he isn't) and comes round in 5 as it starts
    s32 before = crouched ? 9 : 0;
    {
        const NavEdge& e = nav.Edges()[path[0]];
        const bool faces = abe.mFacingLeft == e.mLeft;
        if (e.mKind == EdgeKind::eRunJump)
        {
            const bool shortRun = std::abs(e.mX - here.mX) * (here.mHalf ? 2 : 1) <= 38;
            before += shortRun ? (faces ? 15 : 5) : (faces ? 0 : 12);
        }
        else if (!faces && (e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eFall || e.mKind == EdgeKind::eHop))
        {
            before += 12;
        }
    }
    // A mine of that stretch not seen for a lap yet, or not there yet: watched from here
    bodies.clear();
    Bodies(nav, path, 0, count, false, true, bodies);
    // Another way with a mine: a Slig on the floor it comes down to, taken over and walked into it. From where
    // the blast does not reach him (60 to either side of the mine and 60 over it), with nothing to see him
    // chant: the Slig asleep or on another floor, no orb on his screen, and no other Slig about to be taken
    // over in its stead.
    if (!mBombSligTried && abe.mMove == Move::eIdle && !here.mLow)
    {
        const World& world = bot.GetWorld();
        const s32 abeCam = nav.CameraOf(abe.mX, abe.mY - 5);
        bool orb = false;
        for (const Entity& e : world.mEntities)
        {
            orb = orb || (e.mKind == EntityKind::eSecurityOrb && !e.mGone && nav.CameraOf(e.CentreX(), e.mY2 - 5) == abeCam);
        }
        s32 sligs = 0;
        for (const SligAt& s : world.mSligs)
        {
            sligs += s.mPossessed ? 0 : 1;
        }
        for (const BombCycle::Seen& bomb : world.mBombs)
        {
            const s32 bx = bomb.mX / 256;
            const s32 by = bomb.mY / 256;
            bool inWay = false;
            for (const Body& b : bodies)
            {
                inWay = inWay || (b.mHalf == bomb.mHalf && std::abs(b.mX - bx) <= 150 && std::abs(b.mY - by) <= 150);
            }
            if (!inWay || orb || sligs != 1 || std::abs(abe.mX - bx) < 130)
            {
                continue;
            }
            for (const SligAt& s : world.mSligs)
            {
                const s32 scale = s.mHalf ? 2 : 1;
                const s32 from = nav.NodeAt(s.mX, s.mY, s.mHalf, 12);
                const s32 to = nav.NodeAt(bx, s.mY, s.mHalf, 30);
                if (s.mPossessed || s.mHalf != bomb.mHalf || from < 0 || to < 0 || nav.Nodes()[from].mLine != nav.Nodes()[to].mLine || nav.CameraOf(s.mX, s.mY - 5) != abeCam ||
                    by > s.mY + 4 || by < s.mY - 62 / scale || std::abs(s.mX - bx) > 400 || !(s.mAsleep || std::abs(s.mY - abe.mY) * scale > 40))
                {
                    continue;
                }
                mBombSligTried = true;
                bot.Log("a flying mine in the way with a Slig on its floor: the Slig at " + std::to_string(s.mX) + "," + std::to_string(s.mY) + " taken over and walked into it");
                mBombSlig = ClearSligTask::IntoMine(bomb.mId);
                step = mBombSlig->Next(bot, "");
                return true;
            }
        }
    }
    bool watching = false;
    bool missing = false;
    for (const Body& b : bodies)
    {
        watching = watching || bombs.Watching(b.mHalf, BombBody(b));
        missing = missing || bombs.Missing(b.mHalf, BombBody(b));
    }
    missing = missing && mBombWatch < 90;
    if (watching || missing)
    {
        if (mBombWatch == 0 || (watching && mBombWatch < 1000))
        {
            bot.Log("flying mines on the way: watching where they go");
            mBombWatch = 1000;
        }
        mBombWatch += missing && !watching ? 1 : 0;
    }
    else if (crouched)
    {
        std::size_t lead = 0;
        while (lead < count)
        {
            const NavEdge& e = nav.Edges()[path[lead]];
            if ((e.mKind != EdgeKind::eWalk && e.mKind != EdgeKind::eRoll) || e.mLeft != nav.Edges()[path[0]].mLeft || nav.Nodes()[e.mTo].mHazard)
            {
                break;
            }
            lead++;
        }
        if (lead == count && BombsClear(nav, bombs, path, count, true, 0))
        {
            bot.Log("rolling on under the flying mines");
            roll = count;
            go = true;
        }
        else if ((mBombUp == cur && abe.mFrame < mBombUpAt) || BombsClear(nav, bombs, path, count, false, before))
        {
            // Up first: what comes next is done standing
            if (mBombUp != cur || abe.mFrame >= mBombUpAt)
            {
                mBombUp = cur;
                mBombUpAt = abe.mFrame + static_cast<u32>(before) + 6;
            }
            if (mBombWait > 0)
            {
                bot.Log("the flying mines are out of the way for " + std::to_string(count) + " edges: up and on");
            }
            mBombWait = 0;
            mSteps--;
            step = TaskStep::Do(Prim::Press(Btn::eUp));
            return true;
        }
    }
    else if (mBombUp == cur && abe.mFrame < mBombUpAt)
    {
        // (set to go: no second thoughts)
        go = true;
    }
    else
    {
        go = BombsClear(nav, bombs, path, count, false, before);
        if (go && before > 0)
        {
            mBombUp = cur;
            mBombUpAt = abe.mFrame + static_cast<u32>(before) + 6;
        }
    }
    if (go)
    {
        if (mBombWait > 0)
        {
            bot.Log("the flying mines are out of the way for " + std::to_string(count) + " edges: on");
        }
        stop = std::min(stop, count);
        mBombWait = 0;
        return false;
    }
    // Whether it ever will be: the mines as they will be for the next 600 frames
    if (!watching && !missing && mBombWait == 0)
    {
        bool ever = false;
        for (s32 later = 1; later <= 600 && !ever; later++)
        {
            ever = BombsClear(nav, bombs, path, count, false, before, later) || (duck && BombsClear(nav, bombs, path, count, true, 0, later));
        }
        if (!ever)
        {
            // (another way, then)
            bot.Log("the flying mines never leave the time for that");
            bot.BanEdge(path[0]);
            mSteps--;
            step = TaskStep::Do(Prim::Wait(1));
            return true;
        }
    }
    if (mBombWait > 1500)
    {
        bot.BanEdge(path[0]);
        step = TaskStep::Failed("the flying mines leave no time to get through");
        return true;
    }
    if (!watching && !missing && mBombWait++ == 0)
    {
        std::string way;
        for (std::size_t i = 0; i < count; i++)
        {
            const NavEdge& e = nav.Edges()[path[i]];
            way += std::string(" ") + EdgeKindName(e.mKind) + ">" + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY);
        }
        bot.Log(std::string("flying mines cross the way ahead: waiting") + (duck ? " crouched" : "") + " for them to be out of the way (" + std::to_string(count) + " edges:" + way + ", " + std::to_string(bodies.size()) + " frames)");
    }
    mSteps--;
    if (duck)
    {
        mBombDuck = true;
        if (abe.mMove != Move::eCrouch)
        {
            step = abe.Settled() ? TaskStep::Do(Prim::Press(Btn::eDown)) : TaskStep::Do(Prim::Wait(1));
            return true;
        }
    }
    step = TaskStep::Do(Prim::Wait(1));
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

ClearSligTask::~ClearSligTask()
{
    if (mHeld)
    {
        mHeld->HoldInvis(false);
    }
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
        // Picked up out in the open with a Slig awake on that floor and screen: into the nearest shadow before
        // anything else (the walkthrough: "crouch over it to pick it up, and stand and sneak back to the shadows")
        const s32 at = bot.AbeNode();
        if (sack->mInputs[1] == 2 && at >= 0 && !nav.Nodes()[at].mHidden && !mShade)
        {
            bool sligs = false;
            for (const SligAt& slig : world.mSligs)
            {
                sligs = sligs || (!slig.mAsleep && !slig.mPossessed && slig.mHalf == abe.mHalfScale && std::abs(slig.mY - abe.mY) <= 45 && nav.CameraOf(slig.mX, slig.mY - 5) == nav.CameraOf(abe.mX, abe.mY - 5));
            }
            s32 best = -1;
            for (std::size_t n = 0; sligs && n < nav.Nodes().size(); n++)
            {
                const NavNode& node = nav.Nodes()[n];
                if (node.mLine == nav.Nodes()[at].mLine && node.mHidden && !node.mHazard && std::abs(node.mX - abe.mX) <= 100 && (best < 0 || std::abs(node.mX - abe.mX) < std::abs(nav.Nodes()[best].mX - abe.mX)))
                {
                    best = static_cast<s32>(n);
                }
            }
            mShade = true;
            if (best >= 0)
            {
                if (abe.mMove == Move::eCrouch)
                {
                    mShade = false;
                    return TaskStep::Do(Prim::Press(Btn::eUp));
                }
                return TaskStep::Do(Prim::MoveTo(nav.Nodes()[best].mX, Gait::eSneak));
            }
        }
        return TaskStep::Done();
    }
    if (mRetreat && !mGo)
    {
        mRetreat = false;
        if (mRetreatX != 0)
        {
            return TaskStep::Do(Prim::MoveTo(mRetreatX, mRetreatX == mHideX ? Gait::eWalk : Gait::eRun));
        }
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

    // Meat, and a Paramite on the sack's level on this screen: it comes for what falls out (Paramite::FindMeat:
    // any meat in the air on its screen) and is at it as he picks it up, with the end of the floor behind it:
    // it turns on him (the Mudomo vault's path 4, at 812,680: dead 30 frames after the meat was in hand). So
    // what this sack drops is left to it, and the meat to keep is had from another sack of the level while it
    // eats, if there is one.
    if (!mBaited && mJumps > 0 && sack->mInputs[1] == 1)
    {
        bool paramite = false;
        for (const ParamiteAt& p : world.mParamites)
        {
            paramite = paramite || (!p.mPossessed && !p.Hidden() && p.mHalf == sack->mHalf && std::abs(p.mY - nav.Nodes()[cur].mY) * (sack->mHalf ? 2 : 1) <= 30 && nav.CameraOf(p.mX, p.mY - 5) == nav.CameraOf(abe.mX, abe.mY - 5));
        }
        for (const Entity& other : world.mEntities)
        {
            if (paramite && other.mKind == EntityKind::eRockSack && other.mInputs[1] == 1 && other.mId != sack->mId && other.mHalf == sack->mHalf && std::abs(other.mY2 - sack->mY2) <= 20 && Domain::UseNode(nav, other, nullptr) >= 0)
            {
                bot.Log("a Paramite on the level of the sack at " + std::to_string(sack->CentreX()) + ": its meat is left for it, and the meat to keep had from the sack at " + std::to_string(other.CentreX()));
                mBaited = true;
                mBaitX = sack->CentreX();
                mSack = other.mId;
                mJumps = 0;
                mWaits = 0;
                return Next(bot, "");
            }
        }
    }
    // What fell out, once it lies still: from the cell next to it, crouched, like anything picked up
    const RockAt* rock = nullptr;
    for (const RockAt& r : world.mRocksDown)
    {
        // (not what was left as bait: that is nearer the sack it fell from)
        if (mBaited && std::abs(r.mX - mBaitX) < std::abs(r.mX - sack->CentreX()))
        {
            continue;
        }
        if (r.mHalf == sack->mHalf && std::abs(r.mX - sack->CentreX()) < 500 && std::abs(r.mY - nav.Nodes()[cur].mY) < 200)
        {
            rock = &r;
        }
    }
    if (rock)
    {
        const s32 left = nav.NodeAt(rock->mX - cell, rock->mY, rock->mHalf, 12);
        const s32 right = nav.NodeAt(rock->mX + cell, rock->mY, rock->mHalf, 12);
        // Out of the shadow he went back into from a boom machine's button: when the Slig that stands guard has
        // just turned its back again, and sneaked all the way (a walk is heard, and it turns at once: the
        // Barracks' path 7, shot at 664 three cells from the grenade)
        if (mHid && mHideX != 0 && sack->mInputs[1] == 2 && (left >= 0 || right >= 0) && nav.Nodes()[cur].mLine == nav.Nodes()[left >= 0 ? left : right].mLine)
        {
            if (!mOut)
            {
                const SligAt* guard = nullptr;
                for (const SligAt& slig : world.mSligs)
                {
                    if (!slig.mAsleep && !slig.mPossessed && slig.mHalf == sack->mHalf && std::abs(slig.mY - abe.mY) <= 45 && nav.CameraOf(slig.mX, slig.mY - 5) == nav.CameraOf(rock->mX, rock->mY - 5))
                    {
                        guard = &slig;
                    }
                }
                const bool away = guard && guard->mFacingLeft == (guard->mX < abe.mX);
                const bool turned = !guard || (away && mOutSeen && !mOutAway && guard->mMotion == 0);
                mOutSeen = true;
                mOutAway = away;
                if (!turned && ++mOutFrames < 1200)
                {
                    return TaskStep::Do(Prim::Wait(1));
                }
                bot.Log("out of the shadow at " + std::to_string(abe.mX) + " for the grenade at " + std::to_string(rock->mX));
                mOut = true;
            }
            if (cur != left && cur != right)
            {
                const s32 to = left < 0 ? right : right < 0 ? left : std::abs(nav.Nodes()[left].mX - abe.mX) < std::abs(nav.Nodes()[right].mX - abe.mX) ? left : right;
                return TaskStep::Do(Prim::MoveTo(nav.Nodes()[to].mX, Gait::eSneak));
            }
        }
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
    // A boom machine with a Slig that stands guard on its floor and screen, and a shadow by the machine: into
    // the shadow first, and out to the button only as the Slig turns its back. All of the time it then stands
    // that way is ahead, and he needs most of it: the grenade is heard as it lands and the Slig says "What?"
    // and looks, which is slow; but when its own time to turn comes first it turns, sees him and shoots at once
    // (Slig.cpp Brain_15_Idle). Seen both ways in the Barracks' path 7: the button 44 frames after it turned
    // away, off the screen in time; the button 130 frames after, shot at the machine.
    if (sack->mInputs[1] == 2 && mJumps == 0 && !mPrimed)
    {
        const s32 cam = nav.CameraOf(nav.Nodes()[under].mX, nav.Nodes()[under].mY - 5);
        const SligAt* guard = nullptr;
        for (const SligAt& slig : world.mSligs)
        {
            if (!slig.mAsleep && !slig.mPossessed && slig.mHalf == sack->mHalf && std::abs(slig.mY - nav.Nodes()[under].mY) <= 45 && nav.CameraOf(slig.mX, slig.mY - 5) == cam)
            {
                guard = &slig;
            }
        }
        s32 hide = -1;
        for (std::size_t n = 0; guard && n < nav.Nodes().size(); n++)
        {
            const NavNode& node = nav.Nodes()[n];
            if (node.mLine == nav.Nodes()[under].mLine && node.mHidden && !node.mHazard && std::abs(node.mX - nav.Nodes()[under].mX) <= 4 * cell && (hide < 0 || std::abs(node.mX - nav.Nodes()[under].mX) < std::abs(nav.Nodes()[hide].mX - nav.Nodes()[under].mX)))
            {
                hide = static_cast<s32>(n);
            }
        }
        if (hide < 0)
        {
            mPrimed = true;
        }
        else if (cur != hide && !mHid)
        {
            mGo = GoToNode(hide);
            return Next(bot, "");
        }
        else
        {
            mHid = true;
            mHideX = abe.mX;
            const bool away = guard->mFacingLeft == (guard->mX < abe.mX);
            const bool turned = away && mGuardSeen && !mGuardAway && guard->mMotion == 0;
            mGuardSeen = true;
            mGuardAway = away;
            if (turned || ++mHidFrames > 1200)
            {
                bot.Log("the Slig at " + std::to_string(guard->mX) + " has turned its back: out of the shadow at " + std::to_string(abe.mX) + " to the button of the boom machine");
                mPrimed = true;
            }
            else
            {
                if (abe.mMove == Move::eCrouch)
                {
                    return TaskStep::Do(Prim::Press(Btn::eUp));
                }
                return TaskStep::Do(Prim::Wait(1));
            }
        }
    }
    if (cur != under)
    {
        if (mPrimed && mHid && nav.Nodes()[cur].mLine == nav.Nodes()[under].mLine)
        {
            return TaskStep::Do(Prim::MoveTo(nav.Nodes()[under].mX, Gait::eSneak));
        }
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
    // A boom machine with a Slig awake on its floor and screen: the button and off that screen at once, at a run
    // (the machine is heard: it turns to look, and shoots what it has seen even into a shadow). The walkthrough
    // of the Barracks' Slig bunks: "hit the button and run and roll to the right, stopping as soon as you cross
    // to the next screen. Sneak back."
    if (sack->mInputs[1] == 2)
    {
        const s32 cam = nav.CameraOf(abe.mX, abe.mY - 5);
        for (const SligAt& slig : world.mSligs)
        {
            mRetreat = mRetreat || (!slig.mAsleep && !slig.mPossessed && slig.mHalf == abe.mHalfScale && std::abs(slig.mY - abe.mY) <= 45 && nav.CameraOf(slig.mX, slig.mY - 5) == cam);
        }
        if (mRetreat)
        {
            // (to the nearest cell of his floor on another screen, faced before the button is pressed: a turn
            // after it is ten frames the Slig's own turn does not leave)
            s32 best = -1;
            for (std::size_t n = 0; n < nav.Nodes().size(); n++)
            {
                const NavNode& node = nav.Nodes()[n];
                if (node.mLine == nav.Nodes()[cur].mLine && !node.mHazard && nav.CameraOf(node.mX, node.mY - 5) != cam &&
                    (best < 0 || std::abs(node.mX - abe.mX) < std::abs(nav.Nodes()[best].mX - abe.mX)))
                {
                    best = static_cast<s32>(n);
                }
            }
            // (and on past it for two cells more where the floor goes on: he is across the edge of the screen
            // at a run then, not slowing to stand)
            for (s32 more = 0; best >= 0 && more < 2; more++)
            {
                const s32 next = nav.NodeAt(nav.Nodes()[best].mX + (nav.Nodes()[best].mX > abe.mX ? cell : -cell), nav.Nodes()[best].mY, sack->mHalf, 12);
                if (next < 0 || nav.Nodes()[next].mLine != nav.Nodes()[cur].mLine || nav.Nodes()[next].mHazard)
                {
                    break;
                }
                best = next;
            }
            // (but back into the shadow he came out of, when there is one: he is all in it long before the Slig
            // has said its "What?" and looked, and the next screen may be another Slig's. The Barracks' path 7:
            // off to 812 he met the Slig of 1012 to 1262 there, at the near end of its beat.)
            if (mHid && mHideX != 0)
            {
                best = nav.NodeAt(mHideX, nav.Nodes()[cur].mY, sack->mHalf, 12);
            }
            mRetreatX = best >= 0 ? nav.Nodes()[best].mX : 0;
            if (best >= 0 && abe.mFacingLeft != (mRetreatX < abe.mX))
            {
                mRetreat = false;
                mJumps--;
                return TaskStep::Do(Prim::Face(mRetreatX < abe.mX));
            }
            bot.Log("the button of the boom machine at " + std::to_string(sack->CentreX()) + ", and off its screen to " + std::to_string(mRetreatX) + " before the Slig there has turned");
            return TaskStep::Do(Prim::Press(Btn::eUp, 6));
        }
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

GrenadeSligTask::~GrenadeSligTask() = default;

s32 GrenadeSligTask::SligIn(s32 x, bool left, s32 beatMin, s32 beatMax, s32 frames)
{
    if (beatMax <= beatMin)
    {
        return x;
    }
    // Along its beat and back: where it is on the way there and back again
    const s32 length = beatMax - beatMin;
    x = std::max(beatMin, std::min(beatMax, x));
    s32 at = left ? 2 * length - (x - beatMin) : x - beatMin;
    at = (at + frames * kPace / 100) % (2 * length);
    return beatMin + (at <= length ? at : 2 * length - at);
}

TaskStep GrenadeSligTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const Entity* slig = world.Find(mSlig);
    if (!slig)
    {
        return TaskStep::Failed("no such entity");
    }
    // (a locker for guns stands for the Sligs that came by theirs at it: one fewer of them is this grenade's work)
    const bool locker = slig->mKind == EntityKind::ePantsLocker;
    const SligAt* live = nullptr;
    s32 about = 0;
    for (const SligAt& s : world.mSligs)
    {
        if (s.mSeen && !s.mPossessed && s.mHalf == slig->mHalf && std::abs(s.mY - slig->mY2) <= 40 && std::abs(s.mX - slig->CentreX()) <= (locker ? 600 : 300))
        {
            about++;
            if (!live || std::abs(s.mX - slig->CentreX()) < std::abs(live->mX - slig->CentreX()))
            {
                live = &s;
            }
        }
    }
    if (locker && mAbout < 0)
    {
        mAbout = about;
    }
    if (locker ? (about < mAbout && mPhase == 2) || about == 0 : slig->mGone)
    {
        // Let the blast die down, and up off his knees
        if (mSettle++ < 4)
        {
            return TaskStep::Do(Prim::Wait(10));
        }
        if (abe.mMove == Move::eCrouch)
        {
            return TaskStep::Do(Prim::Press(Btn::eUp));
        }
        bot.Log(locker ? "the grenade got a Slig by Locker#" + std::to_string(mSlig) + ": " + std::to_string(about) + " left" : "the grenade got Slig#" + std::to_string(mSlig));
        return TaskStep::Done();
    }
    if (mPhase == 2)
    {
        // Down before it goes off
        mWait += 5;
        for (const RockAt& r : world.mRocksFlying)
        {
            mSeenX = r.mX;
            mSeenY = r.mY;
        }
        // (not where down is the way down off the end of a ledge: he would hang there in sight. The Barracks'
        // path 7, at 537,100: shot on his way down it by the Slig the grenade was for.)
        bool edge = false;
        for (const NavEdge& e : nav.Edges())
        {
            edge = edge || (e.mFrom == bot.AbeNode() && e.mKind == EdgeKind::eHoistDown);
        }
        if (abe.mMove == Move::eIdle && !edge)
        {
            return TaskStep::Do(Prim::Press(Btn::eDown));
        }
        if (mWait < 130)
        {
            return TaskStep::Do(Prim::Wait(5));
        }
        bot.Log("the grenade that was to go off at " + std::to_string(mLandX) + " did not get Slig#" + std::to_string(mSlig) + " (it was last seen on its way at " + std::to_string(mSeenX) + "," + std::to_string(mSeenY) + ")" + (live ? " (it is at " + std::to_string(live->mX) + ")" : ""));
        return TaskStep::Failed("the grenade missed");
    }
    if (world.mRocks <= 0 && mPhase == 0)
    {
        return TaskStep::Failed("no grenade");
    }
    if (mPhase == 1)
    {
        mPhase = 2;
        mWait = 0;
        return TaskStep::Do(Prim::Wait(6, Btn::eThrow | BlowMineTask::AimBits(mKind, abe.mFacingLeft)));
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
    const s32 cur = bot.AbeNode();
    if (cur < 0 || abe.mMove == Move::eHang)
    {
        if (++mLost > 60)
        {
            return TaskStep::Failed("off the nav graph");
        }
        return TaskStep::Do(abe.mMove == Move::eHang ? Prim::Press(Btn::eUp) : Prim::Wait(4));
    }
    mLost = 0;
    const Domain::GrenadeTarget* target = nullptr;
    const std::vector<Domain::GrenadeTarget> targets = Domain::GrenadeTargets(nav.Input(), world);
    for (const Domain::GrenadeTarget& t : targets)
    {
        if (t.mSlig == mSlig)
        {
            target = &t;
        }
    }
    if (!target)
    {
        return TaskStep::Failed("nowhere to throw a grenade at the Slig from");
    }
    // The cell to throw from: on the stretch of floor planned, where he isn't seen first, the nearest of those
    const std::vector<s32> regions = nav.Regions();
    const s32 want = mFrom >= 0 && mFrom < static_cast<s32>(regions.size()) ? regions[mFrom] : -1;
    s32 node = -1;
    s32 best = 0;
    // (one that stands: from where a grenade goes off by where it stands, if there is such a place; a wait
    // anywhere else is for nothing)
    for (s32 pass = live && live->mMotion == 0 ? 0 : 1; pass < 2 && node < 0; pass++)
    {
        for (const Domain::GrenadeShot& shot : target->mShots)
        {
            const s32 n = nav.NodeAt(shot.mX, shot.mY, slig->mHalf, 12);
            if (n < 0 || (want >= 0 && regions[n] != want) || std::find(mNoWay.begin(), mNoWay.end(), n) != mNoWay.end() || (pass == 0 && std::abs(live->mX - shot.mLandX) > Domain::kGrenadeSure - 15))
            {
                continue;
            }
            const s32 score = (shot.mHidden ? 0 : 100000) + std::abs(shot.mX - abe.mX) + std::abs(shot.mY - abe.mY);
            if (node < 0 || score < best)
            {
                node = n;
                best = score;
            }
        }
    }
    if (node < 0)
    {
        return TaskStep::Failed("no place left to throw a grenade at the Slig from");
    }
    if (cur != node)
    {
        mGoNode = node;
        mGo = GoToNode(node);
        return Next(bot, "");
    }
    if (!abe.Settled())
    {
        return TaskStep::Do(Prim::Wait(2));
    }
    if (abe.mMove == Move::eCrouch)
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }
    // The throw from here that goes off where the Slig will be by then
    const Domain::GrenadeShot* shot = nullptr;
    // (its beat as the game gives it, if it is in it: two Sligs of a path with the same number for their bounds
    // are each given the other's. The Barracks' path 7: the one that stands at 462 was said to walk 1000..1275.)
    const bool beat = live && live->mBeatMax > live->mBeatMin && live->mX >= live->mBeatMin - 30 && live->mX <= live->mBeatMax + 30;
    const s32 beatMin = beat ? live->mBeatMin : target->mX1;
    const s32 beatMax = beat ? live->mBeatMax : target->mX2;
    // (one that stands is taken to stay: the ones from a locker stand about)
    const s32 then = live && locker && live->mMotion == 0 ? live->mX : live ? SligIn(live->mX, live->mFacingLeft, beatMin, beatMax, kAhead) : slig->CentreX();
    for (const Domain::GrenadeShot& s : target->mShots)
    {
        if (nav.NodeAt(s.mX, s.mY, slig->mHalf, 12) == node && live && std::abs(then - s.mLandX) <= Domain::kGrenadeSure - 15 && (!shot || std::abs(then - s.mLandX) < std::abs(then - shot->mLandX)))
        {
            shot = &s;
        }
    }
    if (!shot)
    {
        if (mHeld == 0)
        {
            std::string lands;
            for (const Domain::GrenadeShot& g : target->mShots)
            {
                if (nav.NodeAt(g.mX, g.mY, slig->mHalf, 12) == node)
                {
                    lands += " " + std::to_string(g.mLandX);
                }
            }
            bot.Log("a grenade from " + std::to_string(abe.mX) + "," + std::to_string(abe.mY) + " goes off at" + lands + "; Slig#" + std::to_string(mSlig) + (live ? " will be at " + std::to_string(then) + " (it walks " + std::to_string(beatMin) + ".." + std::to_string(beatMax) + ")" : " is not about") + ": waiting");
        }
        if (++mHeld > 400)
        {
            bot.Log("Slig#" + std::to_string(mSlig) + " never comes to where a grenade from " + std::to_string(abe.mX) + "," + std::to_string(abe.mY) + " goes off");
            mNoWay.push_back(node);
            mHeld = 0;
        }
        return TaskStep::Do(Prim::Wait(2));
    }
    if (abe.mFacingLeft != shot->mFaceLeft)
    {
        return TaskStep::Do(Prim::Face(shot->mFaceLeft));
    }
    mPhase = 1;
    mKind = shot->mKind;
    mLandX = shot->mLandX;
    bot.Log("a grenade for Slig#" + std::to_string(mSlig) + " (at " + std::to_string(live->mX) + ", to be at " + std::to_string(then) + "): from " + std::to_string(abe.mX) + "," + std::to_string(abe.mY) + ", throw " + std::to_string(static_cast<s32>(mKind)) + ", to go off at " + std::to_string(mLandX));
    return TaskStep::Do(Prim::Wait(8, Btn::eThrow));
}

CrawlerButtonTask::~CrawlerButtonTask() = default;

TaskStep CrawlerButtonTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const Entity* button = world.Find(mButton);
    if (!button)
    {
        return TaskStep::Failed("no such entity");
    }
    const SligAt* me = nullptr;
    for (const SligAt& s : world.mSligs)
    {
        if (s.mPossessed)
        {
            me = &s;
        }
    }
    const bool pressed = mPresses > 0 && world.Switch(button->mSwitch) != (mWas ? 1 : 0);
    if (me)
    {
        // It has its gun: the naked ones of its floor, and any that came by a gun before it, then burst
        s32 targetX = 0;
        bool target = mShot < 1500 && ClearSligTask::Target(world, *me, nullptr, targetX) && std::abs(targetX - me->mX) <= 380 && Domain::FloorOpen(nav.Input(), world, targetX, me->mX, me->mY, me->mHalf);
        for (const RockAt& crawler : world.mCrawlers)
        {
            if (!target && mShot < 1500 && crawler.mHalf == me->mHalf && std::abs(crawler.mY - me->mY) <= 20 && std::abs(crawler.mX - me->mX) <= 500 && std::abs(crawler.mX - me->mX) >= 30 && Domain::FloorOpen(nav.Input(), world, crawler.mX, me->mX, me->mY, me->mHalf))
            {
                target = true;
                targetX = crawler.mX;
            }
        }
        if (target)
        {
            mShot += 2;
            mQuiet = 0;
            const bool left = targetX < me->mX;
            return TaskStep::Do(Prim::Wait(2, me->mFacingLeft == left ? static_cast<u32>(Btn::eThrow) : static_cast<u32>(left ? Btn::eLeft : Btn::eRight)));
        }
        if ((mQuiet += 2) < 60)
        {
            return TaskStep::Do(Prim::Wait(2));
        }
        return TaskStep::Do(Prim::Wait(20, Btn::eChant));
    }
    if (world.mCrawlerMine)
    {
        if (++mCrawled > 1500)
        {
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        }
        s32 toX = button->CentreX();
        if (pressed)
        {
            // To a locker for guns of this floor, if the way to one is open now
            const Entity* locker = nullptr;
            for (const Entity& e : world.mEntities)
            {
                if (e.mKind == EntityKind::ePantsLocker && e.mHalf == button->mHalf && std::abs(e.mY2 - button->mY2) <= 20 && Domain::FloorOpen(nav.Input(), world, e.CentreX(), world.mCrawlerX, world.mCrawlerY, e.mHalf) &&
                    (!locker || std::abs(e.CentreX() - world.mCrawlerX) < std::abs(locker->CentreX() - world.mCrawlerX)))
                {
                    locker = &e;
                }
            }
            if (!locker || mPresses > 8)
            {
                return TaskStep::Do(Prim::Wait(20, Btn::eChant));
            }
            toX = locker->CentreX();
        }
        // (what it was told to do last, done with)
        if (mPressWait > 0)
        {
            mPressWait -= 2;
            return TaskStep::Do(Prim::Wait(2));
        }
        const s32 dx = toX - world.mCrawlerX;
        // (it goes a cell at a time, and on into the next if a way is held as it comes to the end of one: let go
        // half a cell before)
        if (std::abs(dx) > 14)
        {
            mMoved = true;
            return TaskStep::Do(Prim::Wait(2, dx < 0 ? Btn::eLeft : Btn::eRight));
        }
        // (up counts on the frame it is first held, and only while it lies still: CrawlingSlig.cpp HandleCommon)
        if (mMoved)
        {
            mMoved = false;
            mPressWait = 24;
            return TaskStep::Do(Prim::Wait(2));
        }
        if (!pressed && mPresses == 0)
        {
            mWas = world.Switch(button->mSwitch) != 0;
        }
        mPresses++;
        if (!mSaid || pressed)
        {
            mSaid = !pressed;
            bot.Log(std::string("the naked Slig at ") + std::to_string(world.mCrawlerX) + "," + std::to_string(world.mCrawlerY) + (pressed ? ": at the locker for its gun" : ": at the button"));
        }
        mPressWait = 40;
        return TaskStep::Do(Prim::Wait(3, Btn::eUp));
    }
    // Abe
    if (pressed)
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
    if (!Domain::CrawlerFor(nav.Input(), world, *button))
    {
        return TaskStep::Failed("no naked Slig that can get to the button");
    }
    if (bot.AbeNode() != mFrom)
    {
        if (bot.AbeNode() < 0)
        {
            return TaskStep::Do(Prim::Wait(4));
        }
        mGo = GoToNode(mFrom);
        return Next(bot, "");
    }
    if ((mChanted += 20) > 900)
    {
        return TaskStep::Failed("the chant took no naked Slig");
    }
    return TaskStep::Do(Prim::Wait(20, Btn::eChant));
}

GrenadeOrbTask::~GrenadeOrbTask() = default;

TaskStep GrenadeOrbTask::Next(Bot& bot, const std::string& lastFailed)
{
    const NavGraph& nav = bot.Nav();
    const World& world = bot.GetWorld();
    const AbeState& abe = bot.Abe();
    const Entity* orb = world.Find(mOrb);
    if (!orb)
    {
        return TaskStep::Failed("no such entity");
    }
    if (orb->mGone)
    {
        // Let the blast die down, and up off his knees
        if (mSettle++ < 4)
        {
            return TaskStep::Do(Prim::Wait(10));
        }
        if (abe.mMove == Move::eCrouch)
        {
            return TaskStep::Do(Prim::Press(Btn::eUp));
        }
        bot.Log("the grenade got SecurityOrb#" + std::to_string(mOrb));
        return TaskStep::Done();
    }
    if (mPhase == 2)
    {
        mWait += 5;
        // (not where down is the way down off the end of a ledge: he would hang there in sight. The Barracks'
        // path 7, at 537,100: shot on his way down it by the Slig the grenade was for.)
        bool edge = false;
        for (const NavEdge& e : nav.Edges())
        {
            edge = edge || (e.mFrom == bot.AbeNode() && e.mKind == EdgeKind::eHoistDown);
        }
        if (abe.mMove == Move::eIdle && !edge)
        {
            return TaskStep::Do(Prim::Press(Btn::eDown));
        }
        if (mWait < 130)
        {
            return TaskStep::Do(Prim::Wait(5));
        }
        bot.Log("the grenade did not get SecurityOrb#" + std::to_string(mOrb));
        return TaskStep::Failed("the grenade missed");
    }
    if (world.mRocks <= 0 && mPhase == 0)
    {
        return TaskStep::Failed("no grenade");
    }
    if (mPhase == 1)
    {
        mPhase = 2;
        mWait = 0;
        return TaskStep::Do(Prim::Wait(6, Btn::eThrow | BlowMineTask::AimBits(mKind, abe.mFacingLeft)));
    }
    TaskStep step;
    if (!RunGo(mGo, bot, lastFailed, step))
    {
        return step;
    }
    if (step.mKind == TaskStep::Kind::eFailed && mGoNode >= 0)
    {
        mNoWay.push_back(mGoNode);
    }
    const s32 cur = bot.AbeNode();
    if (cur < 0 || abe.mMove == Move::eHang)
    {
        if (++mLost > 60)
        {
            return TaskStep::Failed("off the nav graph");
        }
        return TaskStep::Do(abe.mMove == Move::eHang ? Prim::Press(Btn::eUp) : Prim::Wait(4));
    }
    mLost = 0;
    const Domain::OrbShot* shot = nullptr;
    const std::vector<Domain::OrbTarget> targets = Domain::OrbTargets(nav.Input(), world);
    const std::vector<s32> regions = nav.Regions();
    const s32 want = mFrom >= 0 && mFrom < static_cast<s32>(regions.size()) ? regions[mFrom] : -1;
    s32 node = -1;
    s32 best = 0;
    for (const Domain::OrbTarget& t : targets)
    {
        if (t.mOrb != mOrb)
        {
            continue;
        }
        // The cell to throw from: on the stretch of floor planned, where he isn't seen first, the nearest
        for (const Domain::OrbShot& s : t.mShots)
        {
            const s32 n = nav.NodeAt(s.mX, s.mY, orb->mHalf, 12);
            if (n < 0 || (want >= 0 && regions[n] != want) || std::find(mNoWay.begin(), mNoWay.end(), n) != mNoWay.end())
            {
                continue;
            }
            const s32 score = (s.mHidden ? 0 : 100000) + std::abs(s.mX - abe.mX) + std::abs(s.mY - abe.mY);
            if (node < 0 || score < best)
            {
                node = n;
                best = score;
                shot = &s;
            }
        }
    }
    if (!shot)
    {
        return TaskStep::Failed("nowhere to throw a grenade at the orb from");
    }
    if (cur != node)
    {
        mGoNode = node;
        mGo = GoToNode(node);
        return Next(bot, "");
    }
    if (!abe.Settled())
    {
        return TaskStep::Do(Prim::Wait(2));
    }
    if (abe.mMove == Move::eCrouch)
    {
        return TaskStep::Do(Prim::Press(Btn::eUp));
    }
    if (abe.mFacingLeft != shot->mFaceLeft)
    {
        return TaskStep::Do(Prim::Face(shot->mFaceLeft));
    }
    mPhase = 1;
    mKind = shot->mKind;
    bot.Log("a grenade for SecurityOrb#" + std::to_string(mOrb) + " (at " + std::to_string(orb->mX1) + "," + std::to_string(orb->mY1) + "): from " + std::to_string(abe.mX) + "," + std::to_string(abe.mY) + ", throw " + std::to_string(static_cast<s32>(mKind)) + " held " + std::to_string(shot->mHold) + " frames, to go off at " + std::to_string(shot->mAtX) + "," + std::to_string(shot->mAtY));
    return TaskStep::Do(Prim::Wait(8 + shot->mHold, Btn::eThrow));
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
        if (u.mHalf == mHalf && std::abs(u.mX - mX) <= 6 && std::abs(u.mY - mY) <= 12)
        {
            uxb = &u;
        }
    }
    if (!uxb)
    {
        // (its screen may not be there yet: over to where it is done from first)
        if (mStand >= 0 && bot.AbeNode() != mStand)
        {
            TaskStep go;
            if (!mGo)
            {
                const s32 stand = mStand;
                mGo = std::make_unique<GoToTask>("to the UXB", [stand](s32 n)
                                                 { return n == stand; });
            }
            if (!RunGo(mGo, bot, lastFailed, go) || go.mKind == TaskStep::Kind::eFailed)
            {
                return go;
            }
        }
        return TaskStep::Failed("the UXB isn't there");
    }
    // (under a flying mine's track he stays down throughout: up is into the mine)
    const bool mines = GoToTask::MinesOverhead(bot, bot.AbeNode());
    if (uxb->mOff)
    {
        if (abe.mMove == Move::eCrouch && !mines)
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
    s32 left = nav.NodeAt(mX - cell, mY, mHalf, 12);
    s32 right = nav.NodeAt(mX + cell, mY, mHalf, 12);
    if (mStand >= 0)
    {
        left = left == mStand ? left : -1;
        right = right == mStand ? right : -1;
    }
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
        // Crouched the wrong way round: up and turn (round where he crouches, under a mine)
        return TaskStep::Do(Prim::Press(mines ? (faceLeft ? Btn::eLeft : Btn::eRight) : Btn::eUp));
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
        // A lock is slapped from the side he comes to it from (the other may be behind a gate): the one the
        // way to is shorter, which is not always the side he is on now (the Mudomo vaults' path 10: the lock at
        // 1037,740 is come to from the left, down from the ledge over it, with no time to walk round it)
        if (entity->mKind == EntityKind::eSlapLock)
        {
            mSide = bot.Abe().mX > entity->CentreX() ? 1 : -1;
            const s32 here = bot.AbeNode();
            const s32 nodes[2] = {Domain::UseNode(bot.Nav(), *entity, nullptr, -1), Domain::UseNode(bot.Nav(), *entity, nullptr, 1)};
            s32 costs[2] = {-1, -1};
            for (s32 i = 0; i < 2 && here >= 0 && nodes[0] >= 0 && nodes[1] >= 0 && nodes[0] != nodes[1]; i++)
            {
                const s32 goal = nodes[i];
                std::vector<s32> way;
                s32 total = 0;
                if (here == goal)
                {
                    costs[i] = 0;
                }
                else if (bot.Nav().FindPath(here, [goal](s32 n)
                                            { return n == goal; }, bot.EdgeCostFn(), way, &total))
                {
                    costs[i] = total;
                }
            }
            if (costs[0] >= 0 && costs[1] >= 0 && costs[0] != costs[1])
            {
                mSide = costs[0] < costs[1] ? -1 : 1;
            }
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
            if (mWant == 1 && mArrived && n.mLiftStop >= 0 && bot.Nav().Input().mFeatures[n.mLiftStop].mSwitch == entity->mSwitch && !bot.GetWorld().LiftAtStop(bot.Nav().Input().mFeatures[n.mLiftStop].mX1, bot.Nav().Input().mFeatures[n.mLiftStop].mX2, n.mY, false) && ++mLiftWaits < 600)
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
    // A sick one hears nothing until it is cured: chanted at, with the power he has in Necrum once both trials
    // are done (the walkthrough: "Jump through the bird portal and chant to cure your friends"). The chant's rings
    // go out as long as it is held and cure every sick one they reach.
    if (mud->mMood == MudMood::eSick)
    {
        if (mTalks++ == 0)
        {
            bot.Log("Mudokon#" + std::to_string(mMud) + " is sick: chanting to cure it");
        }
        if (mTalks > 40)
        {
            return TaskStep::Failed("the Mudokon stayed sick");
        }
        return TaskStep::Do(Prim::Wait(60, Btn::eChant));
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

s32 ClearSligTask::SligAbout(const NavGraph& nav, const SligAt& slig, s32 x, s32 y, bool half)
{
    if (slig.mAsleep || slig.mPossessed || slig.mHalf != half || std::abs(slig.mY - y) > 45)
    {
        return 0;
    }
    if (nav.CameraOf(slig.mX, slig.mY - 5) == nav.CameraOf(x, y - 5))
    {
        return 1;
    }
    // (Slig.hpp: Motion_2_Walking. One that stands off the screen is about to turn, for all that can be told.)
    const bool away = slig.mX > x ? !slig.mFacingLeft : slig.mFacingLeft;
    return away && slig.mMotion == 2 ? 3 : 2;
}

s32 ClearSligTask::Haven(const NavGraph& nav, const NavGraph::EdgeCost& cost, s32 use)
{
    const NavNode& at = nav.Nodes()[use];
    const s32 cam = nav.CameraOf(at.mX, at.mY - 5);
    std::vector<s32> way;
    const auto unseen = [&](s32 n)
    {
        const NavNode& node = nav.Nodes()[n];
        return n != use && !node.mHazard && node.mHalf == at.mHalf && nav.CameraOf(node.mX, node.mY - 5) == cam && (node.mHidden || std::abs(node.mY - at.mY) > 45);
    };
    if (!nav.FindPath(use, unseen, cost, way) || way.empty() || way.size() > 3)
    {
        return -1;
    }
    const s32 haven = nav.Edges()[way.back()].mTo;
    // (and a way back down from it)
    std::vector<s32> back;
    // (as far as a dozen cells: the Barracks' path 9, from the ledge at 412,360 down and along to the lock at 663)
    return nav.FindPath(haven, use, cost, back) && back.size() <= 12 ? haven : -1;
}

TaskStep ClearSligTask::Sortie(Bot& bot, const std::string& lastFailed, s32 x, s32 y, bool half)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const AbeState& abe = bot.Abe();
    // None of that floor's Sligs on its screen or about to be, and one just gone
    bool clear = true;
    bool gone = false;
    for (const SligAt& slig : world.mSligs)
    {
        const s32 about = SligAbout(nav, slig, x, y, half);
        clear = clear && (about == 0 || about == 3);
        gone = gone || about == 3;
    }
    bot.Dash(abe.mFrame + 3);
    const auto giveUp = [&](const std::string& why)
    {
        bot.Log("no sortie: " + why);
        mSortie = 0;
        mGaveUp = true;
        mGo.reset();
        mInner = mMake();
        return mInner->Next(bot, "");
    };
    const NavNode& haven = nav.Nodes()[mSortieNode];
    if (mSortie == 1)
    {
        if (!mGo)
        {
            // A place the thing itself opens the way to is where he goes after it, not before
            std::vector<s32> way;
            if (bot.AbeNode() < 0 || !nav.FindPath(bot.AbeNode(), mSortieNode, bot.EdgeCostFn(), way))
            {
                mSortie = 3;
                mInner = mMake();
                return Sortie(bot, "", x, y, half);
            }
            mGo = GoToNode(mSortieNode);
        }
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return giveUp(step.mWhy);
        }
        mSortie = 2;
        mSortieWaits = 0;
        bot.Log("out of sight at " + std::to_string(haven.mX) + "," + std::to_string(haven.mY) + ": until the Slig has walked off the screen");
    }
    if (mSortie == 2)
    {
        if (!clear || !gone)
        {
            if (++mSortieWaits > 3000)
            {
                return giveUp("the Slig never left the screen");
            }
            return TaskStep::Do(Prim::Wait(1));
        }
        mSortie = 3;
        mInner = mMake();
        bot.Log("the Slig is off the screen: out for " + mInner->Name());
        TaskStep step = mInner->Next(bot, "");
        if (step.mKind != TaskStep::Kind::eDone)
        {
            return step;
        }
    }
    if (mSortie == 3)
    {
        TaskStep step = mInner->Next(bot, lastFailed);
        if (step.mKind != TaskStep::Kind::eDone)
        {
            return step;
        }
        mSortie = 4;
        mGo = GoToNode(mSortieNode);
        TaskStep back;
        if (!RunGo(mGo, bot, "", back))
        {
            return back;
        }
    }
    if (mSortie == 4)
    {
        TaskStep step;
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
    }
    // (back or not: the thing is used)
    mSortie = 5;
    return TaskStep::Done();
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
        // Into the flying mine, while that is there
        if (mMine != 0 && mMineFrames < 900)
        {
            for (const BombCycle::Seen& bomb : world.mBombs)
            {
                if (bomb.mId == mMine)
                {
                    mMineFrames += 6;
                    return TaskStep::Do(Prim::Wait(6, bomb.mX / 256 > me->mX ? Btn::eRight : Btn::eLeft));
                }
            }
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
            bot.Log(mMine ? "the Slig is gone, into the flying mine or burst" : "the Slig that woke is gone");
            return TaskStep::Done();
        }
        mInner = mMake();
        bot.Log("the Slig watching " + mInner->Name() + " is gone");
        return Next(bot, "");
    }

    if (mSortie > 0 && mSortie < 5 && mPhase == 0 && !mAmbush)
    {
        return Sortie(bot, lastFailed, x, y, entity->mHalf);
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
        s32 need = std::abs(abe.mX - x) * 10 / (entity->mHalf ? 12 : 24) + 40;
        // (and out of its sight again after, where there is somewhere near to be: at a run, and 40 for the climb
        // or the jump up. The Barracks' path 9: its three locks one after the other left him at the last of
        // them as the Slig came back.)
        if (bot.IsAe() && !mBehind)
        {
            const s32 use = Domain::UseNode(nav, *entity, nullptr);
            const s32 haven = use >= 0 ? Haven(nav, bot.EdgeCostFn(false, false, true), use) : -1;
            if (haven >= 0)
            {
                need += std::abs(nav.Nodes()[haven].mX - x) * 10 / (entity->mHalf ? 30 : 60) + 40;
            }
        }
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
    // Unseen it watches nothing
    // (and with the power used for this very thing: for as long as it lasts)
    if (mPhase == 0 && world.mInvisible && (mPerch == 0 || mPerch == 5))
    {
        slig = nullptr;
    }
    // With the power to go unseen in hand, or its rings on their way: used as late as can be. It lasts some 440
    // frames, and what the Mudanchee trial has him do unseen takes all of them. So to the last cell of the way
    // that is off the Slig's level, there until its back is turned for long enough, then on to the last cell
    // before its beat and unseen from there.
    if (mPhase == 0 && (slig || mPerch != 0) && mPerch != 5 && entity)
    {
        const auto release = [&]()
        {
            bot.HoldInvis(false);
            mHeld = nullptr;
            mGo.reset();
            mPerch = 5;
        };
        if (mPerch == 0)
        {
            const s32 useNode = Domain::UseNode(nav, *entity, nullptr);
            std::vector<s32> way;
            mPerchNode = -1;
            mFootNode = bot.AbeNode();
            if ((world.mInvisCharge || world.mInvisComing) && !world.mInvisible && slig->mBeatMax > slig->mBeatMin && bot.AbeNode() >= 0 && useNode >= 0 &&
                std::abs(abe.mY - slig->mY) > 45 && nav.FindPath(bot.AbeNode(), useNode, bot.EdgeCostFn(), way))
            {
                s32 frames = 0;
                for (s32 edge : way)
                {
                    const NavEdge& e = nav.Edges()[edge];
                    const NavNode& to = nav.Nodes()[e.mTo];
                    const bool level = to.mHalf == slig->mHalf && std::abs(to.mY - slig->mY) <= 45;
                    // (it turns a cell and a half short of the ends of its zone: measured, at 1437 and 1637 in
                    // a zone from 1400 to 1676)
                    if (level && to.mX >= slig->mBeatMin + 30 && to.mX <= slig->mBeatMax - 30)
                    {
                        break;
                    }
                    // (measured: a hoist is 60 frames, a hop with the turn to it 40, a cell walked 9)
                    const s32 cells = std::max(1, std::abs(to.mX - nav.Nodes()[e.mFrom].mX) / NavGraph::GridStep(to.mHalf));
                    frames += e.mKind == EdgeKind::eHoistUp ? 60 : e.mKind == EdgeKind::eHop ? 40 : e.mKind == EdgeKind::eWalk ? 9 * cells : 2 * Domain::RunFrames(nav, e);
                    if (level)
                    {
                        mPerchNode = e.mTo;
                        mPerchFrames = frames;
                    }
                    else if (mPerchNode < 0)
                    {
                        mFootNode = e.mTo;
                        frames = 0;
                    }
                }
            }
            if (mPerchNode < 0)
            {
                mPerch = 5;
            }
            else
            {
                bot.HoldInvis(true);
                mHeld = &bot;
                mPerch = 1;
                mPerchWaits = 0;
                mGo = GoToNode(mFootNode);
                bot.Log("the power to go unseen is kept until " + std::to_string(nav.Nodes()[mPerchNode].mX) + "," + std::to_string(nav.Nodes()[mPerchNode].mY) + ", the last cell before the Slig's beat (" + std::to_string(slig->mBeatMin) + " to " + std::to_string(slig->mBeatMax) + "): to " +
                        std::to_string(nav.Nodes()[mFootNode].mX) + "," + std::to_string(nav.Nodes()[mFootNode].mY) + " first");
            }
        }
        if (mPerch == 1 || mPerch == 3)
        {
            TaskStep step;
            if (!RunGo(mGo, bot, lastFailed, step))
            {
                return step;
            }
            if (step.mKind == TaskStep::Kind::eFailed)
            {
                bot.Log("not got there: " + step.mWhy);
                release();
            }
            else
            {
                mPerch++;
            }
        }
        if (mPerch == 2)
        {
            // Until it can't see that cell for as long as the way there and the chant take
            const NavNode& perch = nav.Nodes()[mPerchNode];
            s32 perchX1 = perch.mX;
            s32 perchX2 = perch.mX;
            const s32 perchCam = nav.CameraOf(perch.mX, perch.mY - 5);
            while (perchX1 > perch.mX - 2000 && nav.CameraOf(perchX1 - 25, perch.mY - 5) == perchCam)
            {
                perchX1 -= 25;
            }
            while (perchX2 < perch.mX + 2000 && nav.CameraOf(perchX2 + 25, perch.mY - 5) == perchCam)
            {
                perchX2 += 25;
            }
            const SligAt* watcher = slig ? slig : Watcher(world, x, y, entity->mHalf);
            // (it sees him there when it is on that screen and faces him. It walks 2.7 pixels a frame and is some
            // 10 frames turning: measured.)
            s32 left = 0;
            if (watcher)
            {
                const s32 turnLo = watcher->mBeatMin + 37;
                const s32 turnHi = watcher->mBeatMax - 37;
                const bool perchLeft = perch.mX < watcher->mX;
                if (watcher->mFacingLeft == perchLeft)
                {
                    // Coming this way: when it is on his screen
                    left = perchLeft ? std::max(0, watcher->mX - (perchX2 + 25)) * 10 / 27 : std::max(0, (perchX1 - 25) - watcher->mX) * 10 / 27;
                }
                else if (perchLeft)
                {
                    left = (std::max(0, turnHi - watcher->mX) + std::max(0, turnHi - (perchX2 + 25))) * 10 / 27 + 10;
                }
                else
                {
                    left = (std::max(0, watcher->mX - turnLo) + std::max(0, (perchX1 - 25) - turnLo)) * 10 / 27 + 10;
                }
            }
            // (and the chant: he is unseen some 15 frames into it)
            const s32 need = mPerchFrames + 25;
            // (and the rings there by then)
            if (!watcher || left < need || !world.mInvisCharge)
            {
                if (++mPerchWaits > 900)
                {
                    bot.Log("the Slig's back was never turned for long enough: unseen from here");
                    release();
                }
                else
                {
                    return TaskStep::Do(Prim::Wait(1));
                }
            }
            else
            {
                bot.Log("the Slig can't see " + std::to_string(perch.mX) + "," + std::to_string(perch.mY) + " for " + std::to_string(left) + " frames, " + std::to_string(need) + " are needed: there now");
                mPerch = 3;
                mGo = GoToNode(mPerchNode);
                mGo->SetHurry(true);
                TaskStep step;
                if (!RunGo(mGo, bot, lastFailed, step))
                {
                    return step;
                }
            }
        }
        if (mPerch == 4)
        {
            // (its rings still on their way to him)
            if (!world.mInvisible && world.mInvisComing && !world.mInvisCharge && ++mPerchWaits < 300)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (!world.mInvisible && world.mInvisCharge && ++mPerchWaits < 1100)
            {
                return TaskStep::Do(Prim::Wait(1, Btn::eChant));
            }
            // (on his feet again first: what comes next is a run from a stand)
            // (the chant's own motion shows a frame or two after the button)
            if (world.mInvisible && (++mPerchStand <= 3 || abe.mMove != Move::eIdle || !abe.Settled()) && mPerchStand < 60)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            bot.Log("unseen from " + std::to_string(abe.mX) + "," + std::to_string(abe.mY));
            release();
            return mInner->Next(bot, "");
        }
        if (mPerch == 5 && world.mInvisible)
        {
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
            orb = orb || (e.mKind == EntityKind::eSecurityOrb && !e.mGone && nav.CameraOf(e.mX1, e.mY1) == cam);
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
        // (and not with the power to go unseen in hand: the chant would spend that and take no Slig)
        std::vector<s32> way;
        bool close = !orb && !world.mInvisCharge && !world.mInvisComing && !world.mInvisible && bot.AbeNode() >= 0 && nav.FindPath(bot.AbeNode(), spot, bot.EdgeCostFn(), way);
        for (s32 edge : way)
        {
            close = close && std::abs(nav.Nodes()[nav.Edges()[edge].mTo].mX - x) < 600;
        }
        std::function<bool(s32)> raidSpot;
        // With the power to go unseen in hand, and on the Slig's own level (no place off it to keep it for):
        // unseen from where he stands, and the thing done so. The Barracks' path 9: the ring of its first
        // ghost lock is what the other two are slapped under.
        if ((world.mInvisCharge || world.mInvisComing) && !world.mInvisible && mPerch != 4)
        {
            bot.Log("a Slig at " + std::to_string(slig->mX) + "," + std::to_string(slig->mY) + " watches " + mInner->Name() + ", and the power to go unseen is in hand: unseen from here");
            mPerch = 4;
            mPerchWaits = 0;
            return Next(bot, "");
        }
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
            // Not taken over, then: out to the thing while the Slig is off the screen, from right by it
            const s32 use = bot.IsAe() && mSortie == 0 ? Domain::UseNode(nav, *entity, nullptr) : -1;
            // (by the map, with the gates as they will be once the thing is used: what else makes a move dear
            // or barred is for when it is made)
            const s32 own = entity->mSwitch;
            mSortieNode = use >= 0 ? Haven(nav, [&nav, &world, own](const NavEdge& e) -> s32
                                           {
                                               s32 gate = 0;
                                               s32 open = 0;
                                               if (nav.Condition(e, &gate, &open) && (gate == own ? world.Switch(gate) == open : world.Switch(gate) != open))
                                               {
                                                   return -1;
                                               }
                                               return e.mCost + 1;
                                           }, use)
                                   : -1;
            if (mSortieNode >= 0)
            {
                mSortie = 1;
                bot.Log("a Slig at " + std::to_string(slig->mX) + "," + std::to_string(slig->mY) + " watches " + mInner->Name() + " and can't be taken over: from " + std::to_string(nav.Nodes()[mSortieNode].mX) + "," + std::to_string(nav.Nodes()[mSortieNode].mY) + " while it is off the screen");
                return Sortie(bot, lastFailed, x, y, entity->mHalf);
            }
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
        // (one asleep is taken over as well as one awake: only looked for where it is wanted)
        if ((s.mAsleep && mMine == 0) || nav.CameraOf(s.mX, s.mY - 5) != abeCam)
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
            orb = orb || (o.mKind == EntityKind::eSecurityOrb && !o.mGone && nav.CameraOf(o.mX1, o.mY1) == cam);
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
    if (!farWheel)
    {
        return TaskStep::Failed("no wheel");
    }
    const s32 farNode = Domain::UseNode(nav, *farWheel, nullptr);
    const s32 cell = NavGraph::GridStep(farWheel->mHalf);
    const s32 away = farWheel->CentreX() >= mTeam.mFromX ? 1 : -1;
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
            // (with a Mudokon at every wheel already there is nobody to lead along them)
            if (mTeam.mFree.size() < 2)
            {
                mStage = 4;
                return Next(bot, "");
            }
            const Entity* nearWheel = farWheel;
            for (u32 id : mTeam.mFree)
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

TaskStep ParamitePullTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* ring = world.Find(mRing);
    if (!ring)
    {
        return TaskStep::Failed("no such ring");
    }
    if (mWas < 0)
    {
        mWas = world.Switch(ring->mSwitch);
    }
    const ParamiteAt* me = nullptr;
    for (const ParamiteAt& paramite : world.mParamites)
    {
        me = paramite.mPossessed ? &paramite : me;
    }
    if (++mFrames > 3000)
    {
        return TaskStep::Failed("the Paramite took too long");
    }
    const bool pulled = world.Switch(ring->mSwitch) != mWas;
    switch (mPhase)
    {
        case 0:
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
            mFrames = 0;
            bot.Log("chanting to take over a Paramite for the ring at " + std::to_string(ring->CentreX()) + "," + std::to_string(ring->mY1));
            return TaskStep::Do(Prim::Wait(2));
        }
        case 1:
            if (me)
            {
                mPhase = 2;
                bot.Log("in the Paramite at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + ": under the ring at " + std::to_string(ring->CentreX()));
                return TaskStep::Do(Prim::Wait(4));
            }
            if (mFrames > 400)
            {
                return TaskStep::Failed("no Paramite was taken over");
            }
            // (one that comes down a web as he comes on its screen: not there to be taken over until it is down)
            {
                bool down = false;
                for (const ParamiteAt& paramite : world.mParamites)
                {
                    down = down || (paramite.mHalf == ring->mHalf && !paramite.Hidden());
                }
                if (!down && mFrames < 300)
                {
                    return TaskStep::Do(Prim::Wait(1));
                }
            }
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        case 2:
        case 3:
        {
            if (pulled)
            {
                bot.Log("the ring is pulled: letting the Paramite go");
                mPhase = 4;
                mFrames = 0;
                return TaskStep::Do(Prim::Wait(4));
            }
            if (!me)
            {
                return TaskStep::Failed("lost the Paramite");
            }
            const s32 cell = NavGraph::GridStep(me->mHalf);
            // On a web (Paramite.hpp: 38 grabs it, 35 hangs, 36 climbs, 39 and 34 step off): up to its top
            if (me->mMotion == 35 || me->mMotion == 36)
            {
                return TaskStep::Do(Prim::Wait(2, Btn::eUp));
            }
            if (me->mMotion == 38 || me->mMotion == 39 || me->mMotion == 34)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            // The ring over another floor than its own: up the web to it (Domain::WebUp), grabbed from a jump
            // within a cell of it, facing it
            const s32 ringNode = Domain::UseNode(nav, *ring, nullptr);
            const s32 meNode = nav.NodeAt(me->mX, me->mY, me->mHalf, 12);
            s32 webX = 0;
            if (mPhase == 2 && ringNode >= 0 && meNode >= 0 && nav.Nodes()[meNode].mLine != nav.Nodes()[ringNode].mLine && Domain::WebUp(nav, meNode, ringNode, &webX))
            {
                const s32 to = webX - me->mX;
                if (me->mMotion != 0)
                {
                    return TaskStep::Do(Prim::Wait(1, std::abs(to) > 2 * cell ? static_cast<u32>(to > 0 ? Btn::eRight : Btn::eLeft) : 0u));
                }
                if (std::abs(to) < 2)
                {
                    // (right under it: a cell back first)
                    return TaskStep::Do(Prim::Wait(2, me->mFacesLeft ? Btn::eRight : Btn::eLeft));
                }
                if (std::abs(to) > cell || (to < 0) != me->mFacesLeft)
                {
                    return TaskStep::Do(Prim::Wait(2, to > 0 ? Btn::eRight : Btn::eLeft));
                }
                if (++mPulls > 12)
                {
                    return TaskStep::Failed("the Paramite could not get on its web");
                }
                if (mPulls == 1)
                {
                    bot.Log("the Paramite is at its web at " + std::to_string(webX) + " (at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + "): up it");
                }
                return TaskStep::Do(Prim::Wait(3, Btn::eUp));
            }
            if (mPhase == 2)
            {
                mPulls = 0;
            }
            // (Paramite.hpp: motion 0 stands. It walks a cell at a time while a way is held and stops on the
            // next one when let go; the ring has to be straight over it.)
            const s32 dx = ring->CentreX() - me->mX;
            if (std::abs(dx) > cell + cell / 2)
            {
                return TaskStep::Do(Prim::Wait(2, dx > 0 ? Btn::eRight : Btn::eLeft));
            }
            if (me->mMotion != 0)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (std::abs(dx) > cell / 2)
            {
                return TaskStep::Do(Prim::Wait(2, dx > 0 ? Btn::eRight : Btn::eLeft));
            }
            if (mPhase == 2)
            {
                mPhase = 3;
                bot.Log("the Paramite is under the ring at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + ": up at it");
            }
            if (++mPulls > 8)
            {
                return TaskStep::Failed("the Paramite could not pull the ring");
            }
            // (up jumps it at the ring; it hangs on a moment and the switch is set as it lets go)
            return TaskStep::Do(Prim::Wait(mPulls % 2 ? 3 : 40, mPulls % 2 ? static_cast<u32>(Btn::eUp) : 0u));
        }
        case 4:
            if (!me)
            {
                mPhase = 5;
                mFrames = 0;
                return TaskStep::Do(Prim::Wait(2));
            }
            // (on its feet first: it is let go from a stand, the chant held for a second)
            // (Paramite.cpp: motion 29 is the chant under way, which ends as soon as it is let up)
            if (me->mMotion != 0 && me->mMotion != 29 && mFrames < 60)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            return TaskStep::Do(Prim::Wait(me->mMotion == 29 ? 2 : 20, Btn::eChant));
        case 5:
            // (he takes no input until he is out of the chant)
            if (!bot.Abe().Settled() && mFrames < 200)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            return pulled ? TaskStep::Done() : TaskStep::Failed("the ring was not pulled");
    }
    return TaskStep::Failed("lost its place");
}

SligDropTask::~SligDropTask() = default;

TaskStep SligDropTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* lever = world.Find(mLever);
    const Entity* slig = world.Find(mSlig);
    if (!lever || !slig)
    {
        return TaskStep::Failed("no such entity");
    }
    const NavFeature& door = nav.Input().mFeatures[mDoor];
    if (++mFrames > 3000)
    {
        return TaskStep::Failed("the Slig never was on the trap door");
    }
    // The lever under way
    if (mUse)
    {
        // (how long the pull takes to set the switch: what the next one is timed by)
        if (mPulledAt > 0 && world.Switch(lever->mSwitch, true) != mWas)
        {
            mDelay = std::max(4, static_cast<s32>(bot.Abe().mFrame) - mPulledAt);
            mPulledAt = 0;
        }
        const TaskStep step = mUse->Next(bot, lastFailed);
        if (step.mKind == TaskStep::Kind::ePrim)
        {
            return step;
        }
        mUse.reset();
        mAfter = 60;
        return TaskStep::Do(Prim::Wait(1));
    }
    // Any Slig left on the door's floor by it
    const SligAt* on = nullptr;
    bool about = false;
    for (const SligAt& s : world.mSligs)
    {
        if (s.mPossessed || s.mHalf != door.mHalf || std::abs(s.mY - door.mY1) > 30 || std::abs(s.mX - (door.mX1 + door.mX2) / 2) > 400)
        {
            continue;
        }
        about = true;
        // (where it will be when the door opens, at the pace it goes now: pulled as it is on the door it has
        // walked off it by then)
        // (its pace over the frames since it was last looked at)
        const s32 frames = std::max(1, static_cast<s32>(bot.Abe().mFrame) - mLastFrame);
        const s32 pace10 = mLastX != 0 && std::abs(s.mX - mLastX) < 12 * frames ? (s.mX - mLastX) * 10 / frames : 0;
        const s32 pace = pace10;
        // (a meat saw is down far enough to kill some 6 frames after its switch is set, and for as long again)
        const s32 then = s.mX + pace10 * (mDelay + (door.mKind == FeatureKind::eSaw ? 6 : 0)) / 10;
        mLastX = s.mX;
        mLastFrame = static_cast<s32>(bot.Abe().mFrame);
        if (pace != 0 && then >= door.mX1 + 4 && then <= door.mX2 - 4)
        {
            on = &s;
        }
    }
    if (mAfter > 0)
    {
        if (--mAfter > 0)
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        if (slig->mGone || !about)
        {
            bot.Log(door.mKind == FeatureKind::eSaw ? "the Slig is under the saw" : "the Slig is down the trap door");
            bot.FleechKilled(mSlig);
            return TaskStep::Done();
        }
    }
    if (slig->mGone)
    {
        return TaskStep::Done();
    }
    const s32 at = Domain::UseNode(nav, *lever, nullptr);
    if (bot.AbeNode() != at)
    {
        TaskStep step;
        if (!mGo)
        {
            mGo = std::make_unique<GoToTask>("to the trap door's lever", [at](s32 n)
                                             { return n == at; });
        }
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        return TaskStep::Do(Prim::Wait(1));
    }
    if (!on || !bot.Abe().Settled())
    {
        return TaskStep::Do(Prim::Wait(1));
    }
    if (++mPulls > 8)
    {
        return TaskStep::Failed("the trap door did not take the Slig");
    }
    bot.Log("the Slig at " + std::to_string(on->mX) + " will be on the trap door in " + std::to_string(mDelay) + " frames: the lever");
    mPulledAt = static_cast<s32>(bot.Abe().mFrame);
    mWas = world.Switch(lever->mSwitch, true);
    mUse = std::make_unique<UseTask>(mLever, world.Switch(lever->mSwitch) ? 0 : 1);
    return TaskStep::Do(Prim::Wait(1));
}

FartBombTask::~FartBombTask() = default;

TaskStep FartBombTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* target = world.Find(mTarget);
    const Entity* brew = mBrew ? world.Find(mBrew) : nullptr;
    if (!target || (mBrew && !brew) || (!brew && mFrom < 0))
    {
        return TaskStep::Failed("no such entity");
    }
    const FartAt& fart = world.mFart;
    const bool isFlier = target->mKind == EntityKind::eFlier;
    const bool isSlig = target->mKind == EntityKind::eSlig || isFlier;
    // The flying Sligs of the path that are not dead yet, each with the one about that is on its track (the
    // nearest to its lines): the one this is for first
    struct OnTrack final
    {
        const Entity* mEntity = nullptr;
        std::vector<NavLine> mLines;
        const FlierAt* mLive = nullptr;
    };
    std::vector<OnTrack> tracks;
    std::vector<std::pair<s32, s32>> ends;
    if (isFlier)
    {
        for (s32 pass = 0; pass < 2; pass++)
        {
            for (const Entity& e : world.mEntities)
            {
                if (e.mKind != EntityKind::eFlier || (pass == 0) != (e.mId == mTarget) || e.mHalf != target->mHalf || (pass == 1 && (e.mGone || world.mFleechKilled.count(e.mId) != 0)))
                {
                    continue;
                }
                OnTrack t;
                t.mEntity = &e;
                s32 x1 = 0;
                s32 y1 = 0;
                s32 x2 = 0;
                s32 y2 = 0;
                Domain::FlierTrack(nav.Input(), e.CentreX(), (e.mY1 + e.mY2) / 2, x1, y1, x2, y2, pass == 0 ? &ends : nullptr, &t.mLines);
                tracks.push_back(t);
            }
        }
        const auto off = [](const OnTrack& t, const FlierAt& f)
        {
            s32 best = 100000;
            for (const NavLine& line : t.mLines)
            {
                const s64 lx = line.mX2 - line.mX1;
                const s64 ly = line.mY2 - line.mY1;
                const s64 sq = lx * lx + ly * ly;
                s64 t1000 = sq == 0 ? 0 : ((f.mX - line.mX1) * lx + (f.mY - line.mY1) * ly) * 1000 / sq;
                t1000 = std::max<s64>(0, std::min<s64>(1000, t1000));
                const s32 far = std::abs(static_cast<s32>(line.mX1 + lx * t1000 / 1000) - f.mX) + std::abs(static_cast<s32>(line.mY1 + ly * t1000 / 1000) - f.mY);
                best = std::min(best, far);
            }
            return best;
        };
        for (const FlierAt& f : world.mFliers)
        {
            OnTrack* mine = nullptr;
            s32 best = 60;
            for (OnTrack& t : tracks)
            {
                const s32 far = off(t, f);
                if (f.mHalf == target->mHalf && far < best)
                {
                    best = far;
                    mine = &t;
                }
            }
            if (mine)
            {
                mine->mLive = &f;
            }
        }
    }
    const FlierAt* flier = !tracks.empty() ? tracks[0].mLive : nullptr;
    // (a flying Slig's TLV does not say it is dead: it is when the blast has gone off, with its screen still the
    // one shown, and none is on its track. Any other the same blast got is dead as well.)
    if (isFlier && fart.mLive && fart.mExploded && mPhase >= 4 && ++mSinceBlast == 20)
    {
        for (const OnTrack& t : tracks)
        {
            if (!t.mLive && world.mFleechKilled.count(t.mEntity->mId) == 0)
            {
                bot.Log("the fart got FlyingSlig#" + std::to_string(t.mEntity->mId));
                bot.FleechKilled(t.mEntity->mId);
            }
        }
    }
    // (left by a mine and walked away from, not flown: Domain)
    const bool drop = !brew && (target->mKind == EntityKind::eMine || target->mKind == EntityKind::eSlig);
    const bool gone = target->mGone || (isFlier && world.mFleechKilled.count(mTarget) != 0);
    if (++mFrames > 6000)
    {
        return TaskStep::Failed("the fart never got there");
    }
    // Abe again, with nothing in the air
    if (!fart.mLive && mPhase >= 2)
    {
        if (mPhase != 5)
        {
            mPhase = 5;
            mAfter = 0;
        }
        if (++mAfter < 45 || !bot.Abe().Settled())
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        if (gone)
        {
            bot.Log("the fart got " + std::string(EntityKindName(target->mKind)) + "#" + std::to_string(mTarget));
            if (isSlig)
            {
                bot.FleechKilled(mTarget);
            }
            return TaskStep::Done();
        }
        if (mFarts >= 4)
        {
            return TaskStep::Failed("four farts and it is still there");
        }
        bot.Log("the fart did not get " + std::string(EntityKindName(target->mKind)) + "#" + std::to_string(mTarget) + ": another");
        if (!brew)
        {
            return TaskStep::Failed("the fart missed, and that was the Brew he had");
        }
        mPhase = 0;
    }
    if (gone && !fart.mLive)
    {
        if (isSlig)
        {
            bot.FleechKilled(mTarget);
        }
        return TaskStep::Done();
    }
    if (!fart.mLive)
    {
        const s32 at = brew ? Domain::UseNode(nav, *brew, nullptr) : mFrom;
        if (!brew && !world.mBrew)
        {
            return TaskStep::Failed("no Brew in him");
        }
        if (bot.AbeNode() != at)
        {
            TaskStep step;
            if (!mGo)
            {
                mGo = std::make_unique<GoToTask>(brew ? "to the Brew machine" : "to where the fart is made", [at](s32 n)
                                                 { return n == at; });
            }
            if (!RunGo(mGo, bot, lastFailed, step))
            {
                return step;
            }
            if (step.mKind == TaskStep::Kind::eFailed)
            {
                return step;
            }
            return TaskStep::Do(Prim::Wait(1));
        }
        if (!bot.Abe().Settled())
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        if (!world.mBrew)
        {
            if (++mDrinks > 6)
            {
                return TaskStep::Failed("the machine gave no Brew");
            }
            return TaskStep::Do(Prim::Wait(3, Btn::eUp));
        }
        // (his back to the mine: the fart is made behind him)
        if (drop && target->mKind == EntityKind::eMine && bot.Abe().mFacingLeft != (target->CentreX() > bot.Abe().mX))
        {
            return TaskStep::Do(Prim::Face(target->CentreX() > bot.Abe().mX));
        }
        mPhase = 1;
        mSinceBlast = 0;
        mFarts++;
        mGo.reset();
        mWay.clear();
        return TaskStep::Do(Prim::Wait(2, Btn::eFartOrRoll));
    }
    if (drop)
    {
        // Off to where neither the fart's blast nor the mine's reaches, and wait for it there
        mPhase = 2;
        const s32 fx = fart.mX100 / 100;
        const s32 fy = fart.mY100 / 100;
        const s32 mx = target->CentreX();
        const NavNode& here = nav.Nodes()[std::max(0, bot.AbeNode())];
        if (bot.AbeNode() >= 0 && (std::abs(here.mX - fx) < 200 || std::abs(here.mX - mx) < 200) && std::abs(here.mY - fy) < 150)
        {
            TaskStep step;
            if (!mGo)
            {
                const NavGraph* graph = &nav;
                mGo = std::make_unique<GoToTask>("away from the fart", [graph, fx, fy, mx](s32 n)
                                                 {
                                                     const NavNode& node = graph->Nodes()[n];
                                                     return (std::abs(node.mX - fx) >= 200 && std::abs(node.mX - mx) >= 200) || std::abs(node.mY - fy) >= 150;
                                                 });
            }
            if (!RunGo(mGo, bot, lastFailed, step))
            {
                return step;
            }
        }
        // (hung from a ledge over it his legs are in the blast: up. Feeco's lobby, at 1212,1240.)
        if (bot.Abe().mMove == Move::eHang)
        {
            return TaskStep::Do(Prim::Wait(3, Btn::eUp));
        }
        return TaskStep::Do(Prim::Wait(1));
    }
    if (!fart.mFlown)
    {
        // His to fly once the chant has taken
        mPhase = 2;
        return TaskStep::Do(Prim::Wait(1, fart.mExploded ? 0u : static_cast<u32>(Btn::eChant)));
    }
    if (fart.mExploded)
    {
        mPhase = 5;
        return TaskStep::Do(Prim::Wait(1));
    }
    // The middle of what its blast will be, and where that is to be: at a mine, or at the most Sligs that are
    // about on the Slig's floor once its screen is the one shown
    const FartFlight::Point body{fart.mX100 / 100, fart.mY100 / 100 - FartFlight::kBody};
    FartFlight::Point aim{target->CentreX(), target->mY2 - 25};
    if (isFlier)
    {
        // To the end of its track nearest to where the fart was made, where it stops a while each time
        if (!mParked)
        {
            mParked = true;
            mParkX = aim.mX;
            mParkY = aim.mY;
            s32 best = -1;
            for (const std::pair<s32, s32>& end : ends)
            {
                const s32 far = std::abs(end.first - body.mX) + std::abs(end.second - body.mY);
                if (best < 0 || far < best)
                {
                    best = far;
                    mParkX = end.first;
                    mParkY = end.second - 20;
                }
            }
            // With another flying Slig's track going by under or over that end: between the two, so that one
            // blast has both when the other comes by while this one stops there (Feeco's path 7, where the two
            // of its corridor meet at 792 each time round)
            mBoth = false;
            for (std::size_t k = 1; k < tracks.size(); k++)
            {
                for (const NavLine& line : tracks[k].mLines)
                {
                    if (mParkX < std::min(line.mX1, line.mX2) || mParkX > std::max(line.mX1, line.mX2) || line.mX1 == line.mX2)
                    {
                        continue;
                    }
                    const s32 y = line.mY1 + (line.mY2 - line.mY1) * (mParkX - line.mX1) / (line.mX2 - line.mX1) - 20;
                    if (std::abs(y - mParkY) >= 20 && std::abs(y - mParkY) <= 100 && !mBoth)
                    {
                        mBoth = true;
                        // (a blast reaches further up than down)
                        mParkY = (mParkY + y) / 2 + (y > mParkY ? 8 : -8);
                    }
                }
            }
        }
        aim.mX = mParkX;
        aim.mY = mParkY;
    }
    else if (isSlig)
    {
        s32 best = 0;
        for (const SligAt& s : world.mSligs)
        {
            if (!s.mSeen || s.mHalf != target->mHalf || std::abs(s.mY - target->mY2) > 30)
            {
                continue;
            }
            s32 count = 0;
            for (const SligAt& o : world.mSligs)
            {
                count += o.mSeen && o.mHalf == s.mHalf && std::abs(o.mY - s.mY) <= 30 && std::abs(o.mX - s.mX) <= 40 ? 1 : 0;
            }
            if (count > best || (count == best && std::abs(s.mX - body.mX) < std::abs(aim.mX - body.mX)))
            {
                best = count;
                aim.mX = s.mX;
                aim.mY = s.mY - 25;
            }
        }
    }
    if (mPhase != 3 && mPhase != 4)
    {
        mPhase = 3;
        std::vector<FartFlight::Point> corners;
        const std::vector<NavLine> gates = FartFlight::Gates(nav.Input(), target->mHalf, &world);
        if (!FartFlight::Route(nav.Input(), target->mHalf, body, aim, 3000, corners, nullptr, &gates))
        {
            corners.push_back(aim);
        }
        mWay.clear();
        for (const FartFlight::Point& p : corners)
        {
            mWay.push_back({p.mX, p.mY});
        }
        mCorner = 0;
        bot.Log("the fart is his: " + std::to_string(mWay.size()) + " legs to " + std::to_string(aim.mX) + "," + std::to_string(aim.mY));
    }
    const bool still = fart.mVelX100 == 0 && fart.mVelY100 == 0;
    const bool last = mCorner + 1 >= mWay.size();
    const s32 tx = last ? aim.mX : mWay[mCorner].first;
    const s32 ty = last ? aim.mY : mWay[mCorner].second;
    const s32 dx = std::abs(tx - body.mX);
    const s32 dy = std::abs(ty - body.mY);
    // (a flying Slig: where it will be when the blast goes off)
    const bool flierNear = flier && std::abs(flier->mX + flier->mVelX * FartFlight::kFuse / 256 - body.mX) <= 40 && std::abs(flier->mY - 20 + flier->mVelY * FartFlight::kFuse / 256 - body.mY) <= 30;
    bool otherNear = false;
    for (std::size_t k = 1; k < tracks.size(); k++)
    {
        const FlierAt* o = tracks[k].mLive;
        otherNear = otherNear || (o && std::abs(o->mX + o->mVelX * FartFlight::kFuse / 256 - body.mX) <= 45 && o->mY - 20 + o->mVelY * FartFlight::kFuse / 256 >= body.mY - 75 && o->mY - 20 + o->mVelY * FartFlight::kFuse / 256 <= body.mY + 45);
    }
    if (mPhase == 4)
    {
        if (isFlier)
        {
            return TaskStep::Do(Prim::Wait(1, Btn::eChant));
        }
        // Chanted off where it stands: unless what it is for has walked out of its blast meanwhile
        if (isSlig && dx > 45 && fart.mFramesLeft > 120)
        {
            mPhase = 3;
            return TaskStep::Do(Prim::Wait(1));
        }
        return TaskStep::Do(Prim::Wait(1, Btn::eChant));
    }
    if (!last && dx <= 10 && dy <= 10)
    {
        mCorner++;
        return TaskStep::Do(Prim::Wait(1));
    }
    // There, or as good as (a Slig walks on), or out of time: let go and set it off once it stands
    if (last && ((dx <= (isFlier ? 10 : (isSlig ? 30 : 8)) && dy <= (isFlier ? 5 : 12)) || fart.mFramesLeft < 60))
    {
        // (for both while there is time, then for the one)
        if (still && isFlier && fart.mFramesLeft >= 60 && (!flierNear || (mBoth && !otherNear && fart.mFramesLeft >= 260)))
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        if (still)
        {
            mPhase = 4;
            bot.Log("the fart is at " + std::to_string(body.mX) + "," + std::to_string(body.mY) + " with " + std::to_string(fart.mFramesLeft) + " frames left: set off");
            return TaskStep::Do(Prim::Wait(1, Btn::eChant));
        }
        return TaskStep::Do(Prim::Wait(1));
    }
    return TaskStep::Do(Prim::Wait(1, FartFlight::Steer(fart.mX100, fart.mY100, fart.mVelX100, fart.mVelY100, tx, ty + FartFlight::kBody)));
}

FlierBombTask::~FlierBombTask() = default;

// How far back from where it threw the flying Slig goes while the grenade is in the air, and how far out from
// where it waits it comes in from
static const s32 kFlierBack = 35;
static const s32 kFlierRunIn = 25;
// The speed it is flown at, in hundredths of a pixel a frame each way
static const s32 kFlierPace = 300;
// How near where the grenade comes down the Slig has to be when it does: the blast is 30 to each side, and the
// Slig's own box 24 on top of that
static const s32 kFlierBlast = 45;

TaskStep FlierBombTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* spawner = world.Find(mSpawner);
    if (!spawner)
    {
        return TaskStep::Failed("no such entity");
    }
    // (found anew every so often and when one is dead: a way through the air for every place to throw from is
    // a lot to look for)
    if (mTargetsIn <= 0 || mKilled != world.mFleechKilled.size() || mState == 4)
    {
        mTargets = Domain::FlierTargets(nav.Input(), world, *spawner);
        mTargetsIn = 30;
        mKilled = world.mFleechKilled.size();
    }
    mTargetsIn--;
    std::vector<Domain::FlierTarget> targets = mTargets;
    // (the ones that can be got at as things are, and have not been given up on)
    targets.erase(std::remove_if(targets.begin(), targets.end(), [this](const Domain::FlierTarget& t)
                                 { return t.mShots.empty() || std::find(mGivenUp.begin(), mGivenUp.end(), t.mSlig) != mGivenUp.end(); }),
                  targets.end());
    const FlownAt& flown = world.mFlown;
    if (++mFrames > 15000)
    {
        return TaskStep::Failed("the Sligs are still there after 15000 frames");
    }
    // With a lever to pull: that is all it is for, and it is done when the lever's switch has changed
    const Entity* lever = mLever ? world.Find(mLever) : nullptr;
    if (mLever && !lever)
    {
        return TaskStep::Failed("no such lever");
    }
    if (lever && !mLeverKnown)
    {
        mLeverKnown = true;
        mLeverWas = world.Switch(lever->mSwitch);
    }
    // The one flying Slig does all a flying Slig is wanted for before it is burst (there may be no other): sent
    // for the Sligs, a Slig lock that is shut and near is opened first; sent for a lock, the Sligs are bombed
    // after. (The walkthrough of the Barracks' Slig bunks: "Talk to the Slig Lock... then fly your Slig all
    // the way to the bottom, then left, then up the middle of this area. Kill the Sligs along the way".)
    if (!mLock && !mLever && flown.mLive)
    {
        for (const Entity& e : world.mEntities)
        {
            if (!mLock && e.mKind == EntityKind::eSligLock && e.mSwitch > 1 && world.Switch(e.mSwitch) == 0 && world.mLockCodes.count(e.mId) != 0 && e.mHalf == flown.mHalf &&
                std::abs(e.CentreX() - flown.mX100 / 100) + std::abs((e.mY1 + e.mY2) / 2 - flown.mY100 / 100) <= 1200)
            {
                std::vector<std::pair<s32, s32>> corners;
                const std::vector<NavLine> gatesNow = FartFlight::Gates(nav.Input(), flown.mHalf, &world);
                if (Domain::FlierWay(nav.Input(), flown.mHalf, flown.mX100 / 100, flown.mY100 / 100, e.CentreX(), (e.mY1 + e.mY2) / 2, &gatesNow, corners))
                {
                    mLock = e.mId;
                    bot.Log("SligLock#" + std::to_string(e.mId) + " is shut and the flying Slig can get to it: that first");
                }
            }
        }
    }
    // With a Slig lock: that until it is open
    const Entity* lockEntity = mLock ? world.Find(mLock) : nullptr;
    const auto code = world.mLockCodes.find(mLock);
    if (mLock && (!lockEntity || code == world.mLockCodes.end()))
    {
        return TaskStep::Failed("no such lock");
    }
    const Entity* lock = lockEntity && world.Switch(lockEntity->mSwitch) == 0 ? lockEntity : nullptr;
    const bool nothingLeft = mLock ? !lock && targets.empty() : lever ? world.Switch(lever->mSwitch) != mLeverWas : targets.empty();
    if (!flown.mLive)
    {
        // Abe: before it is his, or after it is gone
        if (mPhase == 2 || mPhase == 3)
        {
            if (mPhase == 2)
            {
                mLost++;
                bot.Log("the flying Slig is gone: " + std::to_string(mLost) + " lost");
            }
            mPhase = 4;
            mAfter = 0;
            mState = 0;
            mWay.clear();
        }
        if (mPhase == 4)
        {
            if (++mAfter < 45 || !bot.Abe().Settled())
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            mPhase = 0;
        }
        if (nothingLeft)
        {
            return mGivenUp.empty() ? TaskStep::Done() : TaskStep::Failed("the flying Slig's grenades don't get every Slig");
        }
        if (mLost > 8)
        {
            return TaskStep::Failed("nine flying Sligs lost and the Sligs are still there");
        }
        if (bot.AbeNode() != mSpot)
        {
            TaskStep step;
            if (!mGo)
            {
                mGo = GoToNode(mSpot);
            }
            if (!RunGo(mGo, bot, lastFailed, step))
            {
                return step;
            }
            if (step.mKind == TaskStep::Kind::eFailed)
            {
                return step;
            }
            return TaskStep::Do(Prim::Wait(1));
        }
        // (come up the hoist to it he may hang from it still, out of the flying Slig's sight: up, there is no
        // chanting from a ledge's edge)
        if (bot.Abe().mMove == Move::eHang)
        {
            return TaskStep::Do(Prim::Wait(3, Btn::eUp));
        }
        if (!bot.Abe().Settled() && mPhase != 1)
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        // One on his screen to chant at (the spawner makes it as he stands here)
        const s32 cam = nav.CameraOf(bot.Abe().mX, bot.Abe().mY - 5);
        bool about = false;
        for (const FlierAt& f : world.mFliers)
        {
            about = about || nav.CameraOf(f.mX, f.mY - 20) == cam;
        }
        // (the chant is kept up a while when it flies off the screen: it comes round again, and a chant begun
        // anew each time is never done before it is gone)
        if (!about && (mPhase != 1 || ++mAway > 300))
        {
            mPhase = 0;
            // (one that was lost: the next is not waited for as long. And none to be had from this spawner is
            // kept: what it was wanted for is then done another way, or not)
            if (++mWaits > (mLost > 0 ? 900 : 3000))
            {
                bot.NoFlier(mSpawner);
                return TaskStep::Failed("no flying Slig came to be taken over");
            }
            return TaskStep::Do(Prim::Wait(1));
        }
        if (about)
        {
            mAway = 0;
        }
        if (mPhase != 1)
        {
            mPhase = 1;
            bot.Log("a flying Slig is about: chanting to take it over");
        }
        return TaskStep::Do(Prim::Wait(1, Btn::eChant));
    }

    // The flying Slig
    mWaits = 0;
    if (mPhase != 2 && mPhase != 3)
    {
        mPhase = 2;
        mState = 0;
        bot.Log("in the flying Slig at " + std::to_string(flown.mX100 / 100) + "," + std::to_string(flown.mY100 / 100) + ": " + (lever ? "Lever#" + std::to_string(mLever) + " to pull" : std::to_string(targets.size()) + " Sligs to get"));
        // (the chant let go first, or it would burst)
        return TaskStep::Do(Prim::Wait(2));
    }
    const s32 bx = flown.mX100 / 100;
    const s32 by = flown.mY100 / 100;
    const bool still = flown.mVelX100 == 0 && flown.mVelY100 == 0;
    if (nothingLeft || mPhase == 3)
    {
        if (mPhase != 3)
        {
            mPhase = 3;
            bot.Log(std::string(lock ? "the lock is open" : lever ? "the lever is pulled" : "no Slig left for it") + ": bursting the flying Slig");
        }
        return TaskStep::Do(Prim::Wait(1, Btn::eChant));
    }
    const Domain::FlierTarget* target = nullptr;
    for (const Domain::FlierTarget& t : targets)
    {
        if (t.mSlig == mTarget && !lock && !lever)
        {
            target = &t;
        }
    }
    // A lever is pulled like a grenade is thrown: flown to a place by it, facing it, and action there. From its
    // left first, then from its right (FlierFlight::LeverPull).
    FlierFlight::Shot pull;
    if (lock)
    {
        // (in the middle of its box, come at from one side and then from the other)
        pull.mX = lock->CentreX();
        pull.mY = (lock->mY1 + lock->mY2) / 2;
        pull.mFaceLeft = mShot % 2 == 0;
        pull.mLandX = pull.mX;
        pull.mWaitX = pull.mX;
        pull.mWaitY = pull.mY;
        if (mShot >= 8)
        {
            return TaskStep::Failed("the flying Slig does not get to the lock");
        }
    }
    else if (lever)
    {
        pull = FlierFlight::LeverPull(lever->CentreX(), lever->mY2 - 4, mShot % 2 == 1);
        if (mShot >= 8)
        {
            return TaskStep::Failed("the flying Slig does not get the lever pulled");
        }
    }
    else if (!target)
    {
        if (mTarget != 0)
        {
            bot.Log("the grenade got Slig#" + std::to_string(mTarget));
        }
        // The nearest next
        for (const Domain::FlierTarget& t : targets)
        {
            if (!target || std::abs(t.mX - bx) + std::abs(t.mFloorY - by) < std::abs(target->mX - bx) + std::abs(target->mFloorY - by))
            {
                target = &t;
            }
        }
        mTarget = target->mSlig;
        mShot = 0;
        mRounds = 0;
        mState = 0;
        mLegFrames = 0;
        mHasLastSlig = false;
    }
    // The Slig itself, if it is to be seen: it walks its floor
    const SligAt* live = nullptr;
    for (const SligAt& s : world.mSligs)
    {
        if (target && s.mSeen && !s.mPossessed && s.mHalf == flown.mHalf && std::abs(s.mY - target->mFloorY) <= 25 && std::abs(s.mX - target->mX) <= 150 && (!live || std::abs(s.mX - target->mX) < std::abs(live->mX - target->mX)))
        {
            live = &s;
        }
    }
    if (target && mShot >= target->mShots.size())
    {
        mShot = 0;
        if (++mRounds >= 3)
        {
            bot.Log("no grenade gets Slig#" + std::to_string(mTarget) + ": left");
            mGivenUp.push_back(mTarget);
            bot.NoBomb(mTarget);
            mTarget = 0;
            return TaskStep::Do(Prim::Wait(1));
        }
    }
    const FlierFlight::Shot& shot = target ? target->mShots[mShot] : pull;
    const s32 fuse = shot.mFrames;
    // What is held now is not seen in its speed until the frame after next (measured: the run in to where it
    // waits went 10 pixels too far with each frame's buttons chosen for the speed it had then). So the buttons
    // are chosen for where it will be once the ones held last frame have had their effect. keep: never the
    // button that would turn it round (the way it is not to face).
    const u32 pending = mLastHeld;
    mLastHeld = 0;
    const auto steerTo = [this, &flown, pending](s32 x, s32 y, u32 never = 0)
    {
        s32 velX = flown.mVelX100;
        s32 velY = flown.mVelY100;
        FlierFlight::Step(velX, velY, pending);
        mLastHeld = FlierFlight::Steer(flown.mX100 + velX, flown.mY100 + velY, velX, velY, x, y) & ~never;
        // (never faster than three pixels a frame: at its top speed it is past a corner before it has turned,
        // and off what it flies into it bounces as fast as it came. Measured in the Barracks' path 1, where
        // it went to and fro for good in a pocket between two floors.)
        if (velX > kFlierPace)
        {
            mLastHeld &= ~static_cast<u32>(Btn::eRight);
        }
        if (velX < -kFlierPace)
        {
            mLastHeld &= ~static_cast<u32>(Btn::eLeft);
        }
        if (velY > kFlierPace)
        {
            mLastHeld &= ~static_cast<u32>(Btn::eDown);
        }
        if (velY < -kFlierPace)
        {
            mLastHeld &= ~static_cast<u32>(Btn::eUp);
        }
        return TaskStep::Do(Prim::Wait(1, mLastHeld));
    };
    if (mFrames % 300 == 0)
    {
        bot.Log("the flying Slig is at " + std::to_string(bx) + "," + std::to_string(by) + " going " + std::to_string(flown.mVelX100) + "," + std::to_string(flown.mVelY100) + ", state " + std::to_string(mState) + (lever ? " for the lever from " : " for Slig#" + std::to_string(mTarget) + " from ") + std::to_string(shot.mX) + "," + std::to_string(shot.mY));
    }
    // Another flying Slig on its screen, on the way to a lock: that one bombs the one he flies as soon as it
    // faces it (CanChase: on the screen shown and facing what he controls; measured in the Barracks' path 7,
    // lost 58 frames after it came up into the other's shaft). So it keeps behind the other, a hundred off at its
    // height, and where there is no getting behind it stays where it is with its own height to the other's;
    // and it throws when its grenade would meet the other on its way (thrown 5 over its point at 5.7 a frame
    // along and 6 up, a pixel a frame more down each frame), no nearer than its blast leaves it alone.
    if (lock)
    {
        const s32 myCam = nav.CameraOf(bx, by - 20);
        const FlierAt* foe = nullptr;
        for (const FlierAt& f : world.mFliers)
        {
            if (f.mHalf == flown.mHalf && nav.CameraOf(f.mX, f.mY - 20) == myCam &&
                (!foe || std::abs(f.mX - bx) + std::abs(f.mY - by) < std::abs(foe->mX - bx) + std::abs(foe->mY - by)))
            {
                foe = &f;
            }
        }
        if (foe)
        {
            if (flown.mCanThrow && flown.mFacingLeft == (foe->mX < bx))
            {
                for (s32 t = 3; t <= 18; t++)
                {
                    s32 gx = 0;
                    s32 gy = 0;
                    FlierFlight::GrenadeAt(bx, by, flown.mFacingLeft, t, gx, gy);
                    const s32 fx = foe->mX + foe->mVelX * t / 256;
                    const s32 fy = foe->mY + foe->mVelY * t / 256;
                    if (std::abs(gx - fx) <= 16 && gy >= fy - 22 && gy <= fy + 8 && std::abs(fx - bx) >= 50)
                    {
                        mFoeThrown++;
                        mFoeSaid = 3;
                        bot.Log("another flying Slig at " + std::to_string(foe->mX) + "," + std::to_string(foe->mY) + ": a grenade from " + std::to_string(bx) + "," + std::to_string(by) + " to meet it in " + std::to_string(t) + " frames");
                        return TaskStep::Do(Prim::Wait(1, Btn::eThrow));
                    }
                }
            }
            // (the way to the lock is found anew when it is gone)
            mState = 0;
            mLegFrames = 0;
            const bool east = foe->mFacingLeft;
            const bool behind = (bx > foe->mX) == east;
            const u32 towards = foe->mX < bx ? Btn::eLeft : Btn::eRight;
            if (behind)
            {
                const s32 px = foe->mX + (east ? 100 : -100);
                std::vector<std::pair<s32, s32>> corners;
                const std::vector<NavLine> gatesNow = FartFlight::Gates(nav.Input(), flown.mHalf, &world);
                if (std::abs(px - bx) + std::abs(foe->mY - by) > 12 && Domain::FlierWay(nav.Input(), flown.mHalf, bx, by, px, foe->mY, &gatesNow, corners))
                {
                    if (mFoeSaid != 1)
                    {
                        mFoeSaid = 1;
                        bot.Log("another flying Slig at " + std::to_string(foe->mX) + "," + std::to_string(foe->mY) + ": behind it, to " + std::to_string(px) + "," + std::to_string(foe->mY));
                    }
                    return corners.empty() ? steerTo(px, foe->mY) : steerTo(corners[0].first, corners[0].second);
                }
            }
            // In front of it, and it is not coming at him (then it is waited for with a grenade, at about its
            // height): back off its screen, and round to come onto it from behind where it was
            const bool coming = (foe->mVelX > 0) == (bx > foe->mX) && foe->mVelX != 0;
            if (!behind && !coming && mHasSafe)
            {
                if (mFoeSaid != 4)
                {
                    mFoeSaid = 4;
                    bot.Log("another flying Slig at " + std::to_string(foe->mX) + "," + std::to_string(foe->mY) + " facing " + (foe->mFacingLeft ? "left" : "right") + ": in front of it at " + std::to_string(bx) + "," + std::to_string(by) + ", back to " + std::to_string(mSafeX) + "," + std::to_string(mSafeY));
                }
                mFoeX = foe->mX;
                mFoeLeft = foe->mFacingLeft;
                mFoeRound = true;
                mFoeRoundFrames = 0;
                return steerTo(mSafeX, mSafeY);
            }
            if (mFoeSaid != 2)
            {
                mFoeSaid = 2;
                bot.Log("another flying Slig at " + std::to_string(foe->mX) + "," + std::to_string(foe->mY) + (behind ? ": no way behind it, staying at " : ": coming at him, waited for at ") + std::to_string(bx) + "," + std::to_string(by));
            }
            // (where it is, about the other's height if that is a way off, and facing it)
            if (still && flown.mMotion == 0 && flown.mFacingLeft != (foe->mX < bx) && std::abs(foe->mX - bx) > 30)
            {
                return TaskStep::Do(Prim::Wait(1, towards));
            }
            return steerTo(bx, std::abs(foe->mX - bx) >= 45 ? foe->mY - 15 : by);
        }
        mFoeSaid = 0;
        if (!mFoeRound)
        {
            mSafeX = bx;
            mSafeY = by;
            mHasSafe = true;
        }
        else
        {
            // Off its screen: along to the column just behind where it was, and on from there
            const s32 column = mFoeX + (mFoeLeft ? 3 : -3);
            std::vector<std::pair<s32, s32>> corners;
            const std::vector<NavLine> gatesNow = FartFlight::Gates(nav.Input(), flown.mHalf, &world);
            if (++mFoeRoundFrames > 400 || (std::abs(column - bx) <= 4 && still) || !Domain::FlierWay(nav.Input(), flown.mHalf, bx, by, column, mSafeY, &gatesNow, corners))
            {
                mFoeRound = false;
                mState = 0;
                mLegFrames = 0;
            }
            else
            {
                return corners.empty() ? steerTo(column, mSafeY) : steerTo(corners[0].first, corners[0].second);
            }
        }
    }
    if (mState == 4)
    {
        // Thrown: back to where it waited (and back from where the grenade comes down: the blast reaches the
        // flying Slig where it threw from) until it has gone off, then whether the Slig is still there
        // (dead is when the map says so, which is when what is left of it is gone: that it is not to be seen
        // is not enough, a Slig of a screen that is not the one shown isn't either)
        if (lever)
        {
            // (pulled: it goes to the lever by itself and pulls. Still not after three seconds: again)
            if (++mSince > 90)
            {
                bot.Log("the lever is not pulled from " + std::to_string(shot.mX) + "," + std::to_string(shot.mY) + ": again");
                mShot++;
                mState = 0;
                mLegFrames = 0;
            }
            // (pressed again every few frames while it hangs there: the game takes the press only on a frame
            // it is in its idle motion, which is some frames after it has come to rest)
            if (mSince % 6 == 0 && still && flown.mMotion == 0 && flown.mFacingLeft == shot.mFaceLeft && std::abs(shot.mX - bx) <= 8 && std::abs(shot.mY - by) <= 8)
            {
                return TaskStep::Do(Prim::Wait(1, Btn::eAction));
            }
            return TaskStep::Do(Prim::Wait(1));
        }
        if (++mSince > fuse + 25 && (live || mSince > fuse + 150))
        {
            bot.Log("the grenade from " + std::to_string(shot.mX) + "," + std::to_string(shot.mY) + " did not get Slig#" + std::to_string(mTarget) + (live ? " (it is at " + std::to_string(live->mX) + "," + std::to_string(live->mY) + ", motion " + std::to_string(live->mMotion) + ")" : " (it is not to be seen)") + ": from somewhere else");
            mShot++;
            mState = 0;
            mLegFrames = 0;
        }
        return steerTo(shot.mWaitX + (shot.mFaceLeft ? kFlierBack : -kFlierBack), shot.mWaitY);
    }
    // A Slig shoots what is on its level and in front of it
    const auto seenAt = [&world, &flown, bx](s32 y)
    {
        for (const SligAt& s : world.mSligs)
        {
            if (s.mSeen && !s.mAsleep && !s.mPossessed && s.mHalf == flown.mHalf && y <= s.mY + FlierFlight::kLevelUnder && y >= s.mY - FlierFlight::kLevelOver && s.mFacingLeft == (bx < s.mX))
            {
                return true;
            }
        }
        return false;
    };
    const auto backAt = [&world, &flown, bx](s32 y)
    {
        for (const SligAt& s : world.mSligs)
        {
            if (s.mSeen && !s.mAsleep && !s.mPossessed && s.mHalf == flown.mHalf && y <= s.mY + FlierFlight::kLevelUnder && y >= s.mY - FlierFlight::kLevelOver && s.mFacingLeft != (bx < s.mX))
            {
                return true;
            }
        }
        return false;
    };
    if (mState == 3)
    {
        // Up (or down) to where it throws from, and thrown the moment it is there
        // (when it can: not in the frames it ends its climb in, when the game takes no notice of the button)
        const s32 fromX = shot.mX + (shot.mFaceLeft ? -mNearer : mNearer);
        if (lock)
        {
            // In its box and at rest: it says "Hi", and then the code back to it
            const bool in = bx >= lock->mX1 + 6 && bx <= lock->mX2 - 6 && by >= lock->mY1 + 2 && by <= lock->mY2 - 2;
            if (in && std::abs(shot.mX - bx) <= 25 && std::abs(shot.mY - by) <= 12)
            {
                if (lock->mState != mLockState)
                {
                    mLockState = lock->mState;
                    bot.Log("SligLock#" + std::to_string(mLock) + ": state " + std::to_string(lock->mState) + ", the flying Slig at " + std::to_string(bx) + "," + std::to_string(by));
                }
                mSince = 0;
                const u32 say = SligLockTask::Say(lock->mState, code->second, still && flown.mMotion == 0, mHi, mSaid);
                return TaskStep::Do(Prim::Wait(2, say));
            }
            if (++mSince > 300)
            {
                mShot++;
                mState = 0;
                mLegFrames = 0;
                return TaskStep::Do(Prim::Wait(1));
            }
            return steerTo(shot.mX, shot.mY);
        }
        if (lever)
        {
            // (action, at rest: the game looks for a lever only then)
            if (std::abs(fromX - bx) <= 6 && std::abs(shot.mY - by) <= 6 && still && flown.mMotion == 0 && flown.mFacingLeft == shot.mFaceLeft)
            {
                bot.Log("at " + std::to_string(bx) + "," + std::to_string(by) + " (motion " + std::to_string(flown.mMotion) + ", facing " + (flown.mFacingLeft ? "left" : "right") + "): action, for Lever#" + std::to_string(mLever) + " at " + std::to_string(lever->CentreX()) + "," + std::to_string(lever->mY2 - 4));
                mState = 4;
                mSince = 0;
                return TaskStep::Do(Prim::Wait(1, Btn::eAction));
            }
            // (turned the other way, or past it: out and in again)
            if (++mSince > 90 || (still && flown.mMotion == 0 && flown.mFacingLeft != shot.mFaceLeft))
            {
                mState = 0;
                mLegFrames = 0;
                return TaskStep::Do(Prim::Wait(1));
            }
            // (never a press the other way than it faces: that turns it round, and a lever is only pulled
            // facing it)
            return steerTo(fromX, shot.mY, shot.mFaceLeft ? Btn::eRight : Btn::eLeft);
        }
        if (std::abs(fromX - bx) <= 4 && std::abs(shot.mY - by) <= 3 && flown.mFacingLeft == shot.mFaceLeft && flown.mCanThrow)
        {
            bot.Log("at " + std::to_string(bx) + "," + std::to_string(by) + ": a grenade for Slig#" + std::to_string(mTarget) + ", to come down at " + std::to_string(shot.mLandX + bx - shot.mX) + " in " + std::to_string(fuse) + " frames");
            mState = 4;
            mSince = 0;
            return TaskStep::Do(Prim::Wait(1, Btn::eThrow));
        }
        if (++mSince > 90)
        {
            mState = 0;
            mLegFrames = 0;
        }
        return steerTo(fromX, shot.mY);
    }
    // The way to where it waits is found anew every so often from where it is: it drifts as it is taken over, and
    // off a straight leg it can come up under a ledge it then pushes at for good
    if (mState == 1 && ++mLegFrames >= 40)
    {
        mState = 0;
    }
    if (mState == 0)
    {
        const std::vector<NavLine> gates = FartFlight::Gates(nav.Input(), flown.mHalf, &world);
        // (to a little way out from where it waits: it faces the way it flies, so it is to come to that place
        // flying at the Slig)
        if (!Domain::FlierWay(nav.Input(), flown.mHalf, bx, by, shot.mWaitX + (shot.mFaceLeft ? kFlierRunIn : -kFlierRunIn), shot.mWaitY, &gates, mWay))
        {
            bot.Log("no way through the air from " + std::to_string(bx) + "," + std::to_string(by) + " to " + std::to_string(shot.mWaitX) + "," + std::to_string(shot.mWaitY));
            mShot++;
            mLegFrames = 0;
            return TaskStep::Do(Prim::Wait(1));
        }
        mCorner = 0;
        mState = 1;
        if (mLegFrames == 0)
        {
            mHeld = 0;
            mStuck = 0;
            bot.Log((target ? "Slig#" + std::to_string(mTarget) + " at " + std::to_string(target->mX) + "," + std::to_string(target->mFloorY) : "Lever#" + std::to_string(mLever)) + ": " + std::to_string(mWay.size()) + " legs to " + std::to_string(shot.mWaitX) + "," + std::to_string(shot.mWaitY) + ", to throw from " + std::to_string(shot.mX) + "," + std::to_string(shot.mY));
        }
        mLegFrames = 0;
    }
    if (mState == 1)
    {
        // (the last leg is the run in to where it waits)
        const bool last = mCorner >= mWay.size();
        const s32 tx = last ? shot.mWaitX : mWay[mCorner].first;
        const s32 ty = last ? shot.mWaitY : mWay[mCorner].second;
        // (the last corner is where the run in begins: from rest there, so that it comes to the place slowly
        // and the right way round)
        const bool runIn = mCorner + 1 == mWay.size();
        if (!last && std::abs(tx - bx) <= (runIn ? 5 : 10) && std::abs(ty - by) <= (runIn ? 5 : 10) && (!runIn || still))
        {
            mCorner++;
            mLegFrames = 0;
            mRunInFrames = 0;
            return TaskStep::Do(Prim::Wait(1));
        }
        if (last || runIn)
        {
            mLegFrames = 0;
        }
        if (last && ++mRunInFrames > 90)
        {
            // (not there after three seconds: past it, or turned round. Out and in again)
            mState = 0;
            mLegFrames = 1;
            return TaskStep::Do(Prim::Wait(1));
        }
        // (a little past it will do: it is not steered back, which would turn it round)
        if (last && still && std::abs(tx - bx) <= 6 && std::abs(ty - by) <= 3)
        {
            mState = 2;
            mSince = 0;
            mHeld = 0;
            mHasLastSlig = false;
            return TaskStep::Do(Prim::Wait(1));
        }
        // Off every Slig's level now, the next leg is not begun while it leads onto the level of one that faces
        // this way: it waits where it is for its back
        if (!seenAt(by) && mHeld < 600)
        {
            bool onto = false;
            s32 first = 0;
            for (s32 k = 1; k <= 10 && !onto; k++)
            {
                onto = seenAt(by + (ty - by) * k / 10);
                first = k;
            }
            if (onto)
            {
                if (mHeld++ == 0)
                {
                    bot.Log("a Slig faces this way on the level the way goes through: waiting at " + std::to_string(bx) + "," + std::to_string(by) + " for its back");
                }
                // (as near its level as it can wait unseen, less what it drifts: from far off the way through
                // that level takes longer than a Slig that walks a short beat has its back turned. The Barracks'
                // path 7: from 170 under, 60 frames, and the Slig at 837,620 was round again after 60)
                mSawFace = true;
                mBackWait = 0;
                const s32 near = first - 2;
                if (near >= 1 && std::abs(ty - by) * near / 10 >= 12)
                {
                    return steerTo(bx + (tx - bx) * near / 10, by + (ty - by) * near / 10);
                }
                return steerTo(bx, by);
            }
            // One on a level the leg goes through that has its back this way, and for how long is not known: it
            // may be about to turn (the one at 837,620 stands 36 frames, and the way down past it took 40).
            // Waited for until it has faced this way and turned its back again: then all of that time is ahead.
            s32 back = 0;
            for (s32 k = 1; k <= 10 && back == 0; k++)
            {
                back = backAt(by + (ty - by) * k / 10) ? k : 0;
            }
            if (back != 0 && !mSawFace && mBackWait < 400)
            {
                if (mBackWait++ == 0)
                {
                    bot.Log("a Slig has its back this way on the level the way goes through: waiting at " + std::to_string(bx) + "," + std::to_string(by) + " until it has turned round and back");
                }
                const s32 near = back - 2;
                if (near >= 1 && std::abs(ty - by) * near / 10 >= 12)
                {
                    return steerTo(bx + (tx - bx) * near / 10, by + (ty - by) * near / 10);
                }
                return steerTo(bx, by);
            }
            if (back == 0)
            {
                mSawFace = false;
                mBackWait = 0;
            }
        }
        mHeld = 0;
        // Pushing at something (the end of a ledge its side is under): along to where it is going first
        if (std::abs(bx - mLastX) + std::abs(by - mLastY) <= 1)
        {
            mStuck++;
        }
        else
        {
            mStuck = 0;
        }
        mLastX = bx;
        mLastY = by;
        if (mStuck > 25)
        {
            mUnstick = 25;
            mStuck = 0;
            bot.Log("the flying Slig is held at " + std::to_string(bx) + "," + std::to_string(by) + ": along first");
        }
        if (mUnstick > 0 && !last)
        {
            mUnstick--;
            return steerTo(std::abs(tx - bx) > 20 ? tx : bx + (shot.mFaceLeft ? -40 : 40), by);
        }
        if (last)
        {
            // (on the run in never a press the other way than it is to face: that turns it round)
            return steerTo(tx, ty, shot.mFaceLeft ? Btn::eRight : Btn::eLeft);
        }
        return steerTo(tx, ty);
    }
    // Where it waits: facing the Slig, until nothing sees it where it throws from and the grenade would come
    // down where the Slig will be
    if (std::abs(shot.mWaitX - bx) > 6 || std::abs(shot.mWaitY - by) > 6)
    {
        mState = 0;
        mLegFrames = 1;
        return steerTo(shot.mWaitX, shot.mWaitY);
    }
    if (flown.mFacingLeft != shot.mFaceLeft)
    {
        // (it turns by flying that way: out and in again)
        mState = 0;
        mLegFrames = 1;
        return TaskStep::Do(Prim::Wait(1));
    }
    if (lever || lock)
    {
        mNearer = 0;
        mState = 3;
        mSince = 0;
        return steerTo(shot.mX, shot.mY);
    }
    // (the frames it takes from here to there are a few more for the Slig to walk in)
    const s32 rise = std::abs(shot.mY - shot.mWaitY) / 4 + (shot.mY != shot.mWaitY ? 4 : 0);
    s32 sligX = target->mX;
    if (live)
    {
        sligX = live->mX + (mHasLastSlig ? (live->mX - mLastSligX) * (fuse + rise) : 0);
        mLastSligX = live->mX;
        mHasLastSlig = true;
    }
    // (not for its back: it is on the Slig's level for the few frames it takes to go up, throw and come down
    // again, which is less than a Slig takes to turn and shoot. Measured in the Barracks' path 1: thrown at
    // once it was not shot at, waiting for a back it never got one long enough.)
    // (from nearer the Slig if it stands further on than the grenade comes down)
    const s32 further = shot.mFaceLeft ? shot.mLandX - sligX : sligX - shot.mLandX;
    mNearer = std::max(0, std::min(shot.mNearer, further));
    const s32 lands = shot.mLandX + (shot.mFaceLeft ? -mNearer : mNearer);
    if (++mSince < 600 && std::abs(sligX - lands) > kFlierBlast)
    {
        return steerTo(shot.mWaitX, shot.mWaitY);
    }
    mState = 3;
    mSince = 0;
    return steerTo(shot.mX + (shot.mFaceLeft ? -mNearer : mNearer), shot.mY);
}

DrinkTask::~DrinkTask() = default;

TaskStep DrinkTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const Entity* brew = world.Find(mBrew);
    if (!brew)
    {
        return TaskStep::Failed("no such entity");
    }
    if (world.mBrew)
    {
        return bot.Abe().Settled() ? TaskStep::Done() : TaskStep::Do(Prim::Wait(1));
    }
    const s32 at = Domain::UseNode(bot.Nav(), *brew, nullptr);
    if (bot.AbeNode() != at)
    {
        TaskStep step;
        if (!mGo)
        {
            mGo = std::make_unique<GoToTask>("to the Brew machine", [at](s32 n)
                                             { return n == at; });
        }
        if (!RunGo(mGo, bot, lastFailed, step))
        {
            return step;
        }
        if (step.mKind == TaskStep::Kind::eFailed)
        {
            return step;
        }
        return TaskStep::Do(Prim::Wait(1));
    }
    if (!bot.Abe().Settled())
    {
        return TaskStep::Do(Prim::Wait(1));
    }
    if (++mDrinks > 6)
    {
        return TaskStep::Failed("the machine gave no Brew");
    }
    return TaskStep::Do(Prim::Wait(3, Btn::eUp));
}

SligLockTask::~SligLockTask() = default;

u32 SligLockTask::Say(s32 lockState, const std::vector<s32>& code, bool standing, bool& hi, s32& said)
{
    static const u32 kButtons[] = {Btn::eSpeak1, Btn::eSpeak2, Btn::eSpeak3, Btn::eSpeak4, Btn::eSpeak5, Btn::eSpeak6, Btn::eSpeak7, Btn::eSpeak8};
    if (lockState == 1)
    {
        // It has said "Hi" and waits for one back (SecurityDoor.cpp: 150 frames)
        said = 0;
        if (!hi && standing)
        {
            hi = true;
            return Btn::eSpeak1;
        }
        return 0;
    }
    hi = false;
    if (lockState != 3)
    {
        // (idle, saying its code, open or saying no: nothing to say, and the next time round starts anew)
        said = 0;
        return 0;
    }
    // It has said its code and waits for it: one part after the other, each as soon as the Slig can speak again
    if (standing && said < static_cast<s32>(code.size()) && code[said] >= 1 && code[said] <= 8)
    {
        return kButtons[code[said++] - 1];
    }
    return 0;
}

bool SligLockTask::DropClear(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, bool left)
{
    const s32 cam = nav.CameraOf(x, y - 5);
    for (const SligAt& s : world.mSligs)
    {
        // (a Slig sees what is on its screen: one on the next screen is shot as it comes onto this one)
        if (s.mPossessed || s.mAsleep || s.mHalf != half || std::abs(s.mY - y) > 45 || nav.CameraOf(s.mX, s.mY - 5) != cam)
        {
            continue;
        }
        // Each has to be ahead of where it lands (it lands facing the way it walked off), a couple of cells off
        // at least, and walking away: one that stands is at the end of its beat and about to turn
        const bool ahead = left ? s.mX < x - 60 : s.mX > x + 60;
        if (!ahead || s.mFacingLeft != left || s.mMotion != kSligWalking)
        {
            return false;
        }
    }
    return true;
}

TaskStep SligLockTask::StepTo(const SligAt& me, s32 x)
{
    // Walking, it comes to a stop a cell or two after the key is let go: the last bit is single steps, each a
    // tap from standing
    const s32 grid = NavGraph::GridStep(me.mHalf);
    const bool left = x < me.mX;
    if (std::abs(x - me.mX) > 3 * grid)
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

TaskStep SligLockTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* lock = world.Find(mLock);
    const Entity* slig = world.Find(mSlig);
    const Entity* lever = world.Find(mLever);
    const auto code = world.mLockCodes.find(mLock);
    if (!lock || !slig || !lever || code == world.mLockCodes.end())
    {
        return TaskStep::Failed("no such entity");
    }
    const SligAt* me = nullptr;
    for (const SligAt& s : world.mSligs)
    {
        if (s.mPossessed)
        {
            me = &s;
        }
    }
    const bool open = world.Switch(lock->mSwitch) != 0;
    if (!me)
    {
        if (mPhase >= 2)
        {
            // It was his and is gone: burst, or shot
            if (!bot.Abe().Settled() && mWaits++ < 200)
            {
                return TaskStep::Do(Prim::Wait(2));
            }
            return open ? TaskStep::Done() : TaskStep::Failed("the Slig was lost before the lock opened");
        }
        if (open)
        {
            return TaskStep::Done();
        }
        if (mPull)
        {
            const TaskStep step = mPull->Next(bot, lastFailed);
            if (step.mKind == TaskStep::Kind::ePrim || step.mKind == TaskStep::Kind::eFailed)
            {
                return step;
            }
            mPull.reset();
            mPhase = 1;
            return TaskStep::Do(Prim::Wait(2));
        }
        // (the chant takes one on his screen)
        bool about = false;
        for (const SligAt& s : world.mSligs)
        {
            about = about || (!s.mPossessed && s.mHalf == slig->mHalf && nav.CameraOf(s.mX, s.mY - 5) == nav.CameraOf(bot.Abe().mX, bot.Abe().mY - 5));
        }
        if (!about)
        {
            // (it isn't listed for a moment as it is taken: not given up on at once)
            if (mPhase == 1 && mFrames > 0 && mGone++ < 45)
            {
                return TaskStep::Do(Prim::Wait(2));
            }
            // The lever makes a Slig while its switch is off (the spawner puts it back when its Slig dies)
            if (!world.Switch(slig->mSwitch))
            {
                if (++mCalls > 6)
                {
                    return TaskStep::Failed("six Sligs called and none taken over");
                }
                bot.Log("calling a Slig with Lever#" + std::to_string(lever->mId));
                mPull = std::make_unique<UseTask>(lever->mId, 1);
                mFrames = 0;
                mGone = 0;
                mWaits = 0;
                return TaskStep::Do(Prim::Wait(1));
            }
            mPhase = 1;
            if (++mWaits > 150)
            {
                return TaskStep::Failed("no Slig came");
            }
            return TaskStep::Do(Prim::Wait(2));
        }
        mPhase = 1;
        mGone = 0;
        if (!bot.Abe().Settled() && mFrames == 0)
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        // (two frames at a time: the chant held on after the Slig is his is the chant that bursts it)
        mFrames += 2;
        if (mFrames > 900)
        {
            return TaskStep::Failed("the Slig wouldn't be taken over");
        }
        return TaskStep::Do(Prim::Wait(2, Btn::eChant));
    }

    if (mPhase < 2)
    {
        bot.Log("the Slig is his, at " + std::to_string(me->mX) + "," + std::to_string(me->mY));
        mWaits = 0;
    }
    mPhase = 2;
    const std::vector<NavNode>& nodes = nav.Nodes();
    const std::vector<NavEdge>& edges = nav.Edges();
    if (mRide != 0)
    {
        if (std::abs(me->mY - mRideY) > 3 && mRideFrames < 600)
        {
            mRideFrames += 2;
            return TaskStep::Do(Prim::Wait(2, mRide));
        }
        mRide = 0;
    }
    if (mPullWait > 0)
    {
        mPullWait -= 2;
        return TaskStep::Do(Prim::Wait(2));
    }
    // Another Slig on its floor shoots it on sight: that one first, before the way to the lock and before it
    // is burst (the walkthrough of the Barracks' Slig bunks: "talk to the Slig Lock, then shoot the other
    // Sligs"). And the naked ones that crawl along its floor to a locker for a gun: shot before they get there.
    {
        // (nothing shut between them: a slam door stops the shot)
        const auto clear = [&](s32 x)
        {
            for (const NavFeature& f : nav.Input().mFeatures)
            {
                if (f.mKind == FeatureKind::eBarrier && f.mSolid && f.mHalf == me->mHalf && f.mX2 >= std::min(x, me->mX) && f.mX1 <= std::max(x, me->mX) && f.mY1 <= me->mY && f.mY2 >= me->mY - 40 &&
                    f.mSwitch > 1 && (world.Switch(f.mSwitch) != 0) != f.mStartClosed)
                {
                    return false;
                }
            }
            return true;
        };
        s32 targetX = 0;
        bool target = false;
        // (from as far as it would shoot from: measured there, one that had just come by its gun at 1087 shot
        // the one he had at 889 five frames after it stood)
        if (mShot < 1500 && ClearSligTask::Target(world, *me, nullptr, targetX) && std::abs(targetX - me->mX) <= 380 && clear(targetX))
        {
            target = true;
        }
        for (const RockAt& crawler : world.mCrawlers)
        {
            if (!target && mShot < 1500 && crawler.mHalf == me->mHalf && std::abs(crawler.mY - me->mY) <= 20 && std::abs(crawler.mX - me->mX) <= 300 && std::abs(crawler.mX - me->mX) >= 30 && clear(crawler.mX))
            {
                target = true;
                targetX = crawler.mX;
            }
        }
        if (target)
        {
            mShot += 2;
            const bool left = targetX < me->mX;
            return TaskStep::Do(Prim::Wait(2, me->mFacingLeft == left ? static_cast<u32>(Btn::eThrow) : static_cast<u32>(left ? Btn::eLeft : Btn::eRight)));
        }
        // The lock open: before it is burst, after the naked ones of its floor that are further off (they are on
        // their way to the locker, and he has to come along this floor after)
        for (const RockAt& crawler : world.mCrawlers)
        {
            if (open && mShot < 1500 && crawler.mHalf == me->mHalf && std::abs(crawler.mY - me->mY) <= 20 && std::abs(crawler.mX - me->mX) > 300 && std::abs(crawler.mX - me->mX) <= 700 && clear(crawler.mX))
            {
                mShot += 2;
                return TaskStep::Do(Prim::Wait(2, crawler.mX < me->mX ? Btn::eLeft : Btn::eRight));
            }
        }
    }
    const s32 node = nav.NodeAt(me->mX, me->mY, me->mHalf, 20);
    if (node < 0)
    {
        // In the air, or between cells
        return TaskStep::Do(Prim::Wait(2));
    }
    if (node != mLastNode)
    {
        mLastNode = node;
        mStuck = 0;
    }
    else if (++mStuck > 1500)
    {
        return TaskStep::Failed("the Slig got no further than " + std::to_string(me->mX) + "," + std::to_string(me->mY));
    }
    const auto goalOf = [&nav](const Entity& which)
    {
        return [&nav, &which](s32 n)
        {
            return Domain::AtSligLock(nav, which, n);
        };
    };
    if (open)
    {
        // Another lock it can get to from here with what is open now: left to the step that opens that one
        for (const Entity& other : world.mEntities)
        {
            if (other.mKind == EntityKind::eSligLock && other.mId != mLock && other.mSwitch > 1 && world.Switch(other.mSwitch) == 0 && world.mLockCodes.count(other.mId) != 0 &&
                !Domain::SligWay(nav, node, goalOf(other), nullptr, &world).empty())
            {
                bot.Log("the Slig is kept for SligLock#" + std::to_string(other.mId));
                return TaskStep::Done();
            }
        }
        mBurst += 20;
        if (mBurst > 600)
        {
            return TaskStep::Failed("the Slig wouldn't burst");
        }
        return TaskStep::Do(Prim::Wait(20, Btn::eChant));
    }

    // Other Sligs on its floor. A shot takes the nearest thing in its line anywhere on the screen (Bullet.cpp),
    // and these shoot a Slig that is not their own as soon as they face it: so it does not go to them. It
    // stands where it came onto their floor, faces the way they come from and shoots each as it comes onto the
    // screen. (The walkthrough: "shoot the walking Sligs before they shoot you".)
    {
        const s32 cam = nav.CameraOf(me->mX, me->mY - 5);
        const SligAt* target = nullptr;
        for (const SligAt& s : world.mSligs)
        {
            if (s.mPossessed || s.mHalf != me->mHalf || std::abs(s.mY - me->mY) > 45 || nav.CameraOf(s.mX, s.mY - 5) != cam)
            {
                continue;
            }
            const s32 other = nav.NodeAt(s.mX, me->mY, me->mHalf, 45);
            if (other >= 0 && nodes[other].mLine == nodes[node].mLine && (!target || std::abs(s.mX - me->mX) < std::abs(target->mX - me->mX)))
            {
                target = &s;
            }
        }
        if (target)
        {
            const bool left = target->mX < me->mX;
            if (me->mFacingLeft != left)
            {
                return TaskStep::Do(Prim::Wait(2, left ? Btn::eLeft : Btn::eRight));
            }
            mShots++;
            return TaskStep::Do(Prim::Wait(2, Btn::eThrow));
        }
        const std::vector<u32> onFloor = Domain::SligsMet(nav, world, node, {}, mSlig);
        if (!onFloor.empty() && mHold < kHoldFrames)
        {
            if (mHold == 0)
            {
                bot.Log("the Slig holds " + std::to_string(me->mX) + "," + std::to_string(me->mY) + " until the " + std::to_string(onFloor.size()) + " Sligs of this floor have come to it");
            }
            mHold += 2;
            mStuck = 0;
            // (facing where they are)
            s32 sum = 0;
            for (u32 id : onFloor)
            {
                const Entity* e = world.Find(id);
                sum += e && e->CentreX() < me->mX ? -1 : 1;
            }
            // At the end of the floor away from them, so that they come from one side only (and it is out from
            // under the flying Sligs of Feeco's path 3, whose grenades got it three cells further in: the
            // walkthrough's "wait on the left side on the ground")
            s32 end = node;
            for (bool more = true; more;)
            {
                more = false;
                for (s32 k = 0; k < nodes[end].mEdgeCount; k++)
                {
                    const NavEdge& back = edges[nodes[end].mFirstEdge + k];
                    if (back.mKind == EdgeKind::eWalk && !nodes[back.mTo].mLow && !nodes[back.mTo].mHazard && nodes[back.mTo].mLine == nodes[end].mLine && (nodes[back.mTo].mX < nodes[end].mX) == (sum > 0) && !nav.Condition(back, nullptr, nullptr))
                    {
                        end = back.mTo;
                        more = true;
                        break;
                    }
                }
            }
            if (std::abs(nodes[end].mX - me->mX) > 3)
            {
                return StepTo(*me, nodes[end].mX);
            }
            if (me->mMotion == 0 && me->mFacingLeft != (sum < 0))
            {
                return TaskStep::Do(Prim::Wait(2, sum < 0 ? Btn::eLeft : Btn::eRight));
            }
            return TaskStep::Do(Prim::Wait(2));
        }
    }

    if (Domain::AtSligLock(nav, *lock, node))
    {
        if (lock->mState == 5 && mLastState != 5 && ++mTries > 4)
        {
            return TaskStep::Failed("the lock said no five times");
        }
        if (lock->mState != mLastState)
        {
            bot.Log("SligLock#" + std::to_string(mLock) + ": " + (lock->mState == 1 ? "it says Hi" : lock->mState == 2 ? "it says its code" : lock->mState == 3 ? "it listens" : lock->mState == 4 ? "it opens" : lock->mState == 5 ? "it says no" : "it is quiet"));
        }
        mLastState = lock->mState;
        const u32 say = Say(lock->mState, code->second, me->mMotion == 0, mHi, mSaid);
        return TaskStep::Do(Prim::Wait(2, say));
    }

    const std::vector<s32> way = Domain::SligWay(nav, node, goalOf(*lock), nullptr, &world);
    if (way.empty())
    {
        return TaskStep::Failed("the Slig has no way to the lock from " + std::to_string(me->mX) + "," + std::to_string(me->mY));
    }
    const NavEdge& e = edges[way[0]];
    const NavNode& from = nodes[node];
    const NavNode& to = nodes[e.mTo];
    if (e.mKind == EdgeKind::eLift)
    {
        if (std::abs(me->mX - from.mX) > 3)
        {
            return StepTo(*me, from.mX);
        }
        if (me->mMotion != 0)
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        mRide = to.mY > from.mY ? Btn::eDown : Btn::eUp;
        mRideY = to.mY;
        mRideFrames = 0;
        bot.Log("the Slig rides the lift at " + std::to_string(from.mX) + " to " + std::to_string(to.mY));
        return TaskStep::Do(Prim::Wait(2, mRide));
    }
    if (e.mKind == EdgeKind::eTeleport)
    {
        if (std::abs(me->mX - from.mX) > 3)
        {
            return StepTo(*me, from.mX);
        }
        if (me->mMotion != 0)
        {
            return TaskStep::Do(Prim::Wait(2));
        }
        if (mPullTries >= 6)
        {
            return TaskStep::Failed("the teleporter at " + std::to_string(from.mX) + "," + std::to_string(from.mY) + " didn't take the Slig");
        }
        // Its lever is a cell in front of the Slig, on one side or the other
        const s32 switchId = e.mFeature >= 0 ? nav.Input().mFeatures[e.mFeature].mSwitch : 0;
        bool faceLeft = me->mFacingLeft;
        for (const Entity& l : world.mEntities)
        {
            if (l.mKind == EntityKind::eLever && l.mSwitch == switchId && l.mHalf == me->mHalf && std::abs(l.mY2 - me->mY) <= 30)
            {
                faceLeft = l.CentreX() < me->mX;
            }
        }
        if (mPullTries % 2 == 1)
        {
            faceLeft = !faceLeft;
        }
        if (me->mFacingLeft != faceLeft)
        {
            return TaskStep::Do(Prim::Wait(2, faceLeft ? Btn::eLeft : Btn::eRight));
        }
        mPullTries++;
        mPullWait = 60;
        bot.Log("the Slig pulls the lever of the teleporter at " + std::to_string(from.mX) + "," + std::to_string(from.mY));
        return TaskStep::Do(Prim::Wait(3, Btn::eAction));
    }
    mPullTries = 0;
    if (e.mKind == EdgeKind::eFall)
    {
        // Down onto a floor with Sligs on it: when none of them faces where it lands (the walkthrough: "shoot the
        // walking Sligs before they shoot you (wait until their backs are turned)")
        if (me->mMotion != 0)
        {
            return TaskStep::Do(Prim::Wait(1));
        }
        if (!DropClear(nav, world, to.mX, to.mY, to.mHalf, to.mX < me->mX) && mDropWaits < 2400)
        {
            mStuck = 0;
            if (mDropWaits++ == 0)
            {
                bot.Log("the Slig waits at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + " until no Slig below faces it");
            }
            return TaskStep::Do(Prim::Wait(2));
        }
        return TaskStep::Do(Prim::Wait(2, to.mX < me->mX ? Btn::eLeft : Btn::eRight));
    }
    // A walk: on through the cells that follow the same way. Where it has to stand at the end of them (a lift, a
    // teleporter), the last of it is single steps.
    const bool left = to.mX < from.mX;
    s32 stopX = to.mX;
    bool mustStop = false;
    for (std::size_t i = 1; i < way.size(); i++)
    {
        const NavEdge& next = edges[way[i]];
        // (and before it walks off a floor's end: it may have to wait there)
        if (next.mKind == EdgeKind::eLift || next.mKind == EdgeKind::eTeleport || next.mKind == EdgeKind::eFall)
        {
            mustStop = true;
            break;
        }
        if (next.mKind != EdgeKind::eWalk || (nodes[next.mTo].mX < nodes[next.mFrom].mX) != left)
        {
            break;
        }
        stopX = nodes[next.mTo].mX;
    }
    if (mustStop)
    {
        return StepTo(*me, stopX);
    }
    return TaskStep::Do(Prim::Wait(2, left ? Btn::eLeft : Btn::eRight));
}

StepOnTask::~StepOnTask() = default;

TaskStep StepOnTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* plate = world.Find(mPlate);
    if (!plate)
    {
        return TaskStep::Failed("no such entity");
    }
    if (mWas < 0)
    {
        mWas = world.Switch(plate->mSwitch, true);
    }
    if (world.Switch(plate->mSwitch, true) != mWas)
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
    if (++mWaits > 40)
    {
        return TaskStep::Failed("the foot switch did nothing");
    }
    const s32 on = mOn;
    if (bot.AbeNode() == on && mPhase == 0)
    {
        // (on it already: a cell off it, on the same floor, and back)
        mPhase = 1;
        const NavGraph* g = &nav;
        const s32 cell = NavGraph::GridStep(nav.Nodes()[on].mHalf);
        mGo = std::make_unique<GoToTask>("off the foot switch", [g, on, cell](s32 n)
                                         { return n != on && g->Nodes()[n].mLine == g->Nodes()[on].mLine && std::abs(g->Nodes()[n].mX - g->Nodes()[on].mX) <= cell + 2; });
        return TaskStep::Do(Prim::Wait(1));
    }
    if (bot.AbeNode() != on)
    {
        mPhase = 2;
        mGo = std::make_unique<GoToTask>("onto the foot switch", [on](s32 n)
                                         { return n == on; });
        return TaskStep::Do(Prim::Wait(1));
    }
    return TaskStep::Do(Prim::Wait(1));
}

ParamiteTourTask::~ParamiteTourTask() = default;

TaskStep ParamiteTourTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const ParamiteAt* me = nullptr;
    for (const ParamiteAt& paramite : world.mParamites)
    {
        me = paramite.mPossessed ? &paramite : me;
    }
    if (++mFrames > 6000)
    {
        return TaskStep::Failed("the Paramite took too long");
    }
    switch (mPhase)
    {
        case 0:
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
            mFrames = 0;
            bot.Log("chanting to take over a Paramite for the rings it can get to");
            return TaskStep::Do(Prim::Wait(2));
        }
        case 1:
            if (me)
            {
                mPhase = 2;
                mFrames = 0;
                return TaskStep::Do(Prim::Wait(4));
            }
            if (mFrames > 400)
            {
                return TaskStep::Failed("no Paramite was taken over");
            }
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        case 2:
        case 3:
        {
            if (!me)
            {
                return TaskStep::Failed("lost the Paramite");
            }
            // (what goes wrong while he is in it: it is let go first, and the step fails if no ring was pulled)
            const auto giveUp = [&](const std::string& why)
            {
                bot.Log("the Paramite's round ends: " + why);
                mPhase = 4;
                mFrames = 0;
                return TaskStep::Do(Prim::Wait(4));
            };
            const s32 cell = NavGraph::GridStep(me->mHalf);
            const u32 left = Btn::eLeft;
            const u32 right = Btn::eRight;
            // (a pull under way, and what a pull sets going: a lift takes a moment)
            const Entity* ring = mRing ? world.Find(mRing) : nullptr;
            if (mPhase == 3 && ring && world.Switch(ring->mSwitch) != mWas)
            {
                mPulled++;
                bot.Log("ring #" + std::to_string(mRing) + " is pulled");
                mRing = 0;
                mPhase = 2;
                mWait = 60;
                mLeg = -1;
                return TaskStep::Do(Prim::Wait(1));
            }
            if (mWait > 0)
            {
                mWait--;
                return TaskStep::Do(Prim::Wait(1));
            }
            // On a web (Paramite.hpp: 38 grabs it, 35 hangs, 36 and 37 climb, 39 and 34 step off): up or down it
            // as the leg says, and with no leg up
            if (me->mMotion == 35 || me->mMotion == 36 || me->mMotion == 37)
            {
                // (walked off an end into one on the way down counts as the way down)
                const bool down = mLeg >= 0 && mLegTo >= 0 && nav.Nodes()[mLegTo].mY > me->mY + 12;
                if (++mLegFrames > 900)
                {
                    return giveUp("the Paramite got no further on its web");
                }
                return TaskStep::Do(Prim::Wait(2, down ? Btn::eDown : Btn::eUp));
            }
            if (me->mMotion == 38 || me->mMotion == 39 || me->mMotion == 34 || me->mMotion == 11 || me->mMotion == 17 || me->mMotion == 5 || me->mMotion == 15)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            const s32 at = nav.NodeAt(me->mX, me->mY, me->mHalf, 13);
            // Fleeches awake on its level that it can walk to: after the nearest, and eaten (down, with no web
            // there, eats what is a cell ahead of it: Paramite.cpp, Motion_40_Eating). The walkthrough of the first
            // Mudomo vault: "running up to them and eating them all by repeatedly pressing down". Left alone they
            // go for him where he stands chanting (measured there: bitten to 69 in 100 by the time it was let go).
            {
                const RockAt* prey = nullptr;
                for (const RockAt& f : world.mFleechesAwake)
                {
                    if (f.mHalf != me->mHalf || std::abs(f.mY - me->mY) > 20 || std::abs(f.mX - me->mX) > 700 || Domain::WallBetween(nav, me->mX, f.mX, me->mY, me->mHalf) || Domain::GateBetween(nav, world, me->mX, f.mX, me->mY, me->mHalf))
                    {
                        continue;
                    }
                    // (floor all the way to it)
                    bool floor = true;
                    for (s32 x = std::min(me->mX, f.mX); floor && x <= std::max(me->mX, f.mX); x += cell)
                    {
                        floor = Domain::ParamiteFloor(nav, world, x, me->mY, me->mHalf) >= 0;
                    }
                    // (the one nearest to him where he stands chanting on that floor: those are after him, and
                    // the ones that run from it can wait)
                    const s32 from = std::abs(bot.Abe().mY - me->mY) <= 20 ? bot.Abe().mX : me->mX;
                    if (floor && (!prey || std::abs(f.mX - from) < std::abs(prey->mX - from)))
                    {
                        prey = &f;
                    }
                }
                const bool onFloor = me->mMotion <= 4 || (me->mMotion >= 7 && me->mMotion <= 10) || me->mMotion == 40;
                if (prey && onFloor && at >= 0 && mHunt < 2500)
                {
                    mHunt++;
                    mLeg = -1;
                    const s32 dx = prey->mX - me->mX;
                    if (mHunt % 25 == 1)
                    {
                        bot.Log("Paramite at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + " motion " + std::to_string(me->mMotion) + " after the Fleech at " + std::to_string(prey->mX) + " (" + std::to_string(mHunt) + ")");
                    }
                    const u32 toward = dx > 0 ? right : left;
                    // (from a run it bites its way through all of them it touches, a frame at a time: Motion_31,
                    // had with the action button while the run is held. One at a time with a few of them at it
                    // costs it its life: each of their hits takes 0.15 of it. Measured in the first Mudomo vault:
                    // eating at four of them against the wall, it was dead in 40 frames.)
                    // (only with floor for six cells past the Fleech: a run is not stopped by the end of a floor,
                    // and it went off the end of the first vault's top floor after one, down two floors to where
                    // there was no way back to its ring. Near an end it walks up and eats.)
                    bool room = true;
                    for (s32 k = 1; room && k <= 6; k++)
                    {
                        const s32 x = prey->mX + (dx > 0 ? k : -k) * cell;
                        // (a wall past it is as good as floor: it stops the run. Against a wall is where they
                        // stand and fight, several at once.)
                        if (Domain::WallBetween(nav, prey->mX, x, me->mY, me->mHalf))
                        {
                            break;
                        }
                        room = Domain::ParamiteFloor(nav, world, x, me->mY, me->mHalf) >= 0;
                    }
                    // (nor with him in the way of the run: what it bites at a run it bites whoever it is. Measured
                    // in the first vault: dead at 1728 with the Fleeches between him and the wall.)
                    {
                        const AbeState& abe = bot.Abe();
                        const s32 lo = std::min(me->mX, prey->mX + (dx > 0 ? 6 : -6) * cell) - cell;
                        const s32 hi = std::max(me->mX, prey->mX + (dx > 0 ? 6 : -6) * cell) + cell;
                        if (abe.mHalfScale == me->mHalf && std::abs(abe.mY - me->mY) <= 40 && abe.mX >= lo && abe.mX <= hi)
                        {
                            room = false;
                        }
                    }
                    const u32 run = room ? static_cast<u32>(Btn::eRun) : 0u;
                    // (and nothing held with the floor's end two cells ahead, four at a run: a Fleech that goes
                    // off the end is not followed)
                    {
                        const bool fast = me->mMotion == 3 || me->mMotion == 9 || me->mMotion == 31;
                        bool edge = false;
                        for (s32 k = 1; k <= (fast ? 5 : 2); k++)
                        {
                            const s32 x = me->mX + (dx > 0 ? k : -k) * cell;
                            if (Domain::WallBetween(nav, me->mX, x, me->mY, me->mHalf))
                            {
                                break;
                            }
                            edge = edge || Domain::ParamiteFloor(nav, world, x, me->mY, me->mHalf) < 0;
                        }
                        if (edge && (dx < 0) == me->mFacesLeft)
                        {
                            if (me->mMotion == 0 && std::abs(dx) <= cell * 7 / 4)
                            {
                                return TaskStep::Do(Prim::Wait(12, Btn::eDown));
                            }
                            // (it is let be, and the way gone on with)
                            if (me->mMotion == 0)
                            {
                                mHunt += 50;
                                prey = nullptr;
                            }
                            else
                            {
                                return TaskStep::Do(Prim::Wait(1));
                            }
                        }
                    }
                    if (prey)
                    {
                    if (me->mMotion == 31)
                    {
                        return TaskStep::Do(Prim::Wait(1, room ? toward | run : 0u));
                    }
                    if (me->mMotion != 0 && me->mMotion != 40)
                    {
                        if (std::abs(dx) > cell && (dx < 0) == me->mFacesLeft)
                        {
                            const bool running = me->mMotion == 3 || me->mMotion == 9;
                            if (running && !room)
                            {
                                // (out of the run)
                                return TaskStep::Do(Prim::Wait(1));
                            }
                            const u32 bite = running && std::abs(dx) <= 5 * cell ? static_cast<u32>(Btn::eAction) : 0u;
                            return TaskStep::Do(Prim::Wait(1, toward | (std::abs(dx) > 3 * cell || running ? run : 0u) | bite));
                        }
                        return TaskStep::Do(Prim::Wait(1));
                    }
                    if ((dx < 0) != me->mFacesLeft)
                    {
                        return TaskStep::Do(Prim::Wait(1, toward));
                    }
                    if (std::abs(dx) > cell * 7 / 4)
                    {
                        return TaskStep::Do(Prim::Wait(2, toward | (std::abs(dx) > 3 * cell ? run : 0u)));
                    }
                    return TaskStep::Do(Prim::Wait(12, Btn::eDown));
                    }
                }
            }
            // The leg under way
            if (mLeg >= 0 && at != mLegTo)
            {
                if (++mLegFrames > 900)
                {
                    return giveUp("the Paramite got no further on its way");
                }
                const NavNode& to = nav.Nodes()[mLegTo];
                switch (mLeg)
                {
                    case Domain::ParaLeg::eWalk:
                    {
                        const s32 dx = to.mX - me->mX;
                        if (std::abs(to.mY - me->mY) > 30)
                        {
                            // (not on that floor any more: think again)
                            mLeg = -1;
                            return TaskStep::Do(Prim::Wait(1));
                        }
                        // (at a walk up to Fleeches that sleep: they wake as it comes by, and from a walk it is
                        // stopped among them and eats them where they come down. At a run it was carried on
                        // past them, and they were off to where he stands before it had turned.)
                        bool sleepers = false;
                        for (const RockAt& f : world.mFleechesAsleep)
                        {
                            const s32 ahead = (f.mX - me->mX) * (dx > 0 ? 1 : -1);
                            sleepers = sleepers || (f.mHalf == me->mHalf && std::abs(f.mY - me->mY) < 150 && ahead > -2 * cell && ahead < 14 * cell);
                        }
                        if (std::abs(dx) > 8 * cell && !sleepers)
                        {
                            return TaskStep::Do(Prim::Wait(2, (dx > 0 ? right : left) | Btn::eRun));
                        }
                        if (std::abs(dx) > cell + cell / 2)
                        {
                            return TaskStep::Do(Prim::Wait(2, dx > 0 ? right : left));
                        }
                        if (me->mMotion != 0)
                        {
                            return TaskStep::Do(Prim::Wait(1));
                        }
                        if (std::abs(dx) > cell / 2)
                        {
                            return TaskStep::Do(Prim::Wait(2, dx > 0 ? right : left));
                        }
                        mLeg = -1;
                        return TaskStep::Do(Prim::Wait(1));
                    }
                    case Domain::ParaLeg::eWebUp:
                    {
                        // (grabbed from a jump within a cell of it, facing it)
                        const s32 dx = mLegX - me->mX;
                        if (me->mMotion != 0)
                        {
                            return TaskStep::Do(Prim::Wait(1));
                        }
                        // (a web at the very end of its floor: it is a pixel from it there, and that will do)
                        if (dx == 0)
                        {
                            return TaskStep::Do(Prim::Wait(3, Btn::eUp));
                        }
                        if (std::abs(dx) > cell || (dx < 0) != me->mFacesLeft)
                        {
                            return TaskStep::Do(Prim::Wait(2, dx > 0 ? right : left));
                        }
                        return TaskStep::Do(Prim::Wait(3, Btn::eUp));
                    }
                    case Domain::ParaLeg::eWebDown:
                    case Domain::ParaLeg::eFall:
                    {
                        // (it walks off the end into the web and has hold of it; or off the end and down)
                        const s32 toX = mLeg == Domain::ParaLeg::eWebDown ? mLegX : to.mX;
                        const s32 dx = toX - me->mX;
                        if (std::abs(me->mY - to.mY) <= 12)
                        {
                            mLeg = -1;
                            return TaskStep::Do(Prim::Wait(1));
                        }
                        return TaskStep::Do(Prim::Wait(2, dx >= 0 ? right : left));
                    }
                    default:
                    {
                        // A hop: from a stand, facing the way, with no way held
                        const s32 dx = to.mX - me->mX;
                        if (me->mMotion != 0)
                        {
                            return TaskStep::Do(Prim::Wait(1));
                        }
                        if ((dx < 0) != me->mFacesLeft)
                        {
                            return TaskStep::Do(Prim::Wait(1, dx > 0 ? right : left));
                        }
                        if (std::abs(dx) <= cell)
                        {
                            mLeg = -1;
                            return TaskStep::Do(Prim::Wait(1));
                        }
                        // Rocks that fall into the gap: off as one has gone by under its feet (the walkthrough
                        // of the sixth Mudomo vault: "jump across the gaps between falling rocks"). The next is
                        // at least its wait and its fall away, and the hop is over the gap in half its 14 frames.
                        {
                            bool rocks = false;
                            bool gone = false;
                            for (const RockAt& r : world.mFallers)
                            {
                                if (r.mHalf != me->mHalf || r.mX < std::min(me->mX, to.mX) - cell / 2 || r.mX > std::max(me->mX, to.mX) + cell / 2 || std::abs(r.mY - me->mY) > 400)
                                {
                                    continue;
                                }
                                rocks = true;
                                gone = gone || (r.mChasing && r.mY > me->mY + 8 && r.mY < me->mY + 60);
                            }
                            if (rocks && !gone && ++mWaitRock < 600)
                            {
                                return TaskStep::Do(Prim::Wait(1));
                            }
                            mWaitRock = 0;
                        }
                        return TaskStep::Do(Prim::Wait(2, Btn::eHop));
                    }
                }
            }
            mLeg = -1;
            if (me->mMotion != 0)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (at < 0)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            // The next ring, and the way to it
            if (!mRing)
            {
                const std::vector<u32> tour = Domain::ParamiteTour(nav, world, at);
                if (tour.empty() && mClear && !mParked)
                {
                    // (said in turn, a second apart)
                    if (mSay > 0)
                    {
                        const u32 word = mSay == 1 ? Btn::eSpeak1 : mSay == 2 ? Btn::eSpeak2 : Btn::eSpeak3;
                        const bool stay = mSay == 3;
                        mSay = mSay == 1 ? 2 : 0;
                        mWait = 45;
                        mParked = stay;
                        return TaskStep::Do(Prim::Wait(3, word));
                    }
                    // To where each Fleech of these floors is kept, the nearest first: the ones that are there
                    // come at it or run, and are hunted as they are seen
                    s32 next = -1;
                    s32 nextCost = 0;
                    for (const Entity& f : world.mEntities)
                    {
                        if (f.mKind != EntityKind::eFleech || f.mGone || f.mInputs[1] != 0 || f.mHalf != me->mHalf || mBeenTo.count(f.mId) != 0)
                        {
                            continue;
                        }
                        const s32 on = nav.NodeAt(f.CentreX(), f.mY2, f.mHalf, 30);
                        s32 cost = 0;
                        if (on < 0 || !Domain::ParamiteRoute(nav, world, at, on, nullptr, &cost))
                        {
                            mBeenTo.insert(f.mId);
                            continue;
                        }
                        if (cost <= 3)
                        {
                            mBeenTo.insert(f.mId);
                            continue;
                        }
                        if (next < 0 || cost < nextCost)
                        {
                            next = on;
                            nextCost = cost;
                        }
                    }
                    // Then the other Paramites of these floors: up to each, hello, come on
                    if (next < 0)
                    {
                        for (const Entity& p : world.mEntities)
                        {
                            if (p.mKind != EntityKind::eParamite || p.mGone || p.mHalf != me->mHalf || p.mId == mParamite || mCalled.count(p.mId) != 0)
                            {
                                continue;
                            }
                            s32 px = p.CentreX();
                            s32 py = p.mY2;
                            bool seen = false;
                            for (const ParamiteAt& live : world.mParamites)
                            {
                                if (live.mId == p.mId && !live.mPossessed)
                                {
                                    px = live.mX;
                                    py = live.mY;
                                    seen = !live.Hidden();
                                }
                            }
                            const s32 on = nav.NodeAt(px, py, p.mHalf, 30);
                            s32 cost = 0;
                            if (on < 0 || !Domain::ParamiteRoute(nav, world, at, on, nullptr, &cost))
                            {
                                mCalled.insert(p.mId);
                                continue;
                            }
                            if (cost <= 5)
                            {
                                mCalled.insert(p.mId);
                                if (seen && std::abs(py - me->mY) <= 20)
                                {
                                    if ((px < me->mX) != me->mFacesLeft)
                                    {
                                        mCalled.erase(p.mId);
                                        return TaskStep::Do(Prim::Wait(1, px < me->mX ? left : right));
                                    }
                                    bot.Log("the Paramite at " + std::to_string(me->mX) + " calls the one at " + std::to_string(px) + " to come with it");
                                    mSay = 1;
                                    return TaskStep::Do(Prim::Wait(1));
                                }
                                continue;
                            }
                            next = on;
                            break;
                        }
                    }
                    // Then to the end of the floor away from the way out, and stay
                    if (next < 0)
                    {
                        s32 ends[2] = {at, at};
                        for (s32 dir = 0; dir < 2; dir++)
                        {
                            for (s32 k = 1; k < 400; k++)
                            {
                                const s32 n = Domain::ParamiteFloor(nav, world, me->mX + (dir ? k : -k) * cell, me->mY, me->mHalf);
                                if (n < 0 || Domain::WallBetween(nav, me->mX, nav.Nodes()[n].mX, me->mY, me->mHalf))
                                {
                                    break;
                                }
                                ends[dir] = n;
                            }
                        }
                        s32 out = nav.Nodes()[at].mX;
                        for (const NavExit& x : nav.Exits())
                        {
                            out = x.mNode >= 0 ? nav.Nodes()[x.mNode].mX : out;
                        }
                        const s32 park = std::abs(nav.Nodes()[ends[0]].mX - out) > std::abs(nav.Nodes()[ends[1]].mX - out) ? ends[0] : ends[1];
                        if (std::abs(nav.Nodes()[park].mX - me->mX) <= 2 * cell)
                        {
                            bot.Log("the Paramite is at the end of its floor at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + ": stay, and it is let go");
                            mSay = 3;
                            mPulled++;
                            for (const Entity& f : world.mEntities)
                            {
                                if (f.mKind == EntityKind::eFleech && mBeenTo.count(f.mId) != 0)
                                {
                                    bot.FleechKilled(f.mId);
                                }
                            }
                            return TaskStep::Do(Prim::Wait(1));
                        }
                        next = park;
                    }
                    std::vector<Domain::ParaLeg> legs;
                    if (!Domain::ParamiteRoute(nav, world, at, next, &legs) || legs.empty())
                    {
                        return TaskStep::Do(Prim::Wait(1));
                    }
                    mLeg = legs[0].mKind;
                    mLegTo = legs[0].mTo;
                    mLegX = legs[0].mX;
                    mLegFrames = 0;
                    return TaskStep::Do(Prim::Wait(1));
                }
                if (tour.empty())
                {
                    bot.Log(std::to_string(mPulled) + " rings pulled and no more to get to: letting the Paramite go at " + std::to_string(me->mX) + "," + std::to_string(me->mY));
                    mPhase = 4;
                    mFrames = 0;
                    return TaskStep::Do(Prim::Wait(4));
                }
                mRing = tour[0];
                mWas = world.Switch(world.Find(mRing)->mSwitch);
                mPulls = 0;
                bot.Log("the Paramite at " + std::to_string(me->mX) + "," + std::to_string(me->mY) + " goes for ring #" + std::to_string(mRing) + " at " + std::to_string(world.Find(mRing)->CentreX()));
            }
            ring = world.Find(mRing);
            const s32 under = Domain::UseNode(nav, *ring, nullptr);
            if (at != under || std::abs(ring->CentreX() - me->mX) > cell / 2)
            {
                std::vector<Domain::ParaLeg> legs;
                if (at == under)
                {
                    // (under it but not straight under)
                    const s32 dx = ring->CentreX() - me->mX;
                    return TaskStep::Do(Prim::Wait(2, dx > 0 ? right : left));
                }
                if (!Domain::ParamiteRoute(nav, world, at, under, &legs) || legs.empty())
                {
                    return giveUp("no way for the Paramite to ring #" + std::to_string(mRing));
                }
                mLeg = legs[0].mKind;
                mLegTo = legs[0].mTo;
                mLegX = legs[0].mX;
                mLegFrames = 0;
                static const char* const kNames[] = {"walk", "web up", "web down", "hop", "drop"};
                bot.Log(std::string("Paramite: ") + kNames[mLeg] + " to " + std::to_string(nav.Nodes()[mLegTo].mX) + "," + std::to_string(nav.Nodes()[mLegTo].mY));
                return TaskStep::Do(Prim::Wait(1));
            }
            mPhase = 3;
            if (++mPulls > 8)
            {
                return giveUp("the Paramite could not pull ring #" + std::to_string(mRing));
            }
            // (up jumps it at the ring; it hangs on a moment and the switch is set as it lets go)
            return TaskStep::Do(Prim::Wait(mPulls % 2 ? 3 : 40, mPulls % 2 ? static_cast<u32>(Btn::eUp) : 0u));
        }
        case 4:
            if (!me)
            {
                mPhase = 5;
                mFrames = 0;
                return TaskStep::Do(Prim::Wait(2));
            }
            if (me->mMotion != 0 && me->mMotion != 29 && mFrames < 60)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            return TaskStep::Do(Prim::Wait(me->mMotion == 29 ? 2 : 20, Btn::eChant));
        default:
            if (!bot.Abe().Settled() && mFrames < 200)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            return mPulled > 0 ? TaskStep::Done() : TaskStep::Failed("no ring was pulled");
    }
}

MeatLureTask::~MeatLureTask() = default;

TaskStep MeatLureTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const AbeState& abe = bot.Abe();
    const Entity* entity = world.Find(mEntity);
    const ParamiteAt* para = nullptr;
    for (const ParamiteAt& p : world.mParamites)
    {
        para = p.mId == mParamite ? &p : para;
    }
    const s32 lockNode = entity ? Domain::UseNode(nav, *entity, nullptr) : -1;
    if (!entity || lockNode < 0)
    {
        return TaskStep::Failed("no such entity");
    }
    if (++mFrames > 1500)
    {
        return TaskStep::Failed("the meat took too long");
    }
    const NavNode& lock = nav.Nodes()[lockNode];
    const s32 cell = NavGraph::GridStep(lock.mHalf);
    switch (mPhase)
    {
        case 0:
        {
            if (!para)
            {
                // (not seen yet: its screen comes in on the way)
                if (!mGo)
                {
                    mGo = GoToNode(lockNode);
                }
            }
            else if (mSpot < 0)
            {
                // From the side he comes from, far enough from it that it is not cornered into striking, and
                // thrown on away from it: the meat has to come down on its floor, on the screen he is on
                mSide = abe.mX >= para->mX ? 1 : -1;
                // (as far from the thing as a throw that stays on the screen lets him be: it backs off as he
                // comes, to the end of its floor by the thing)
                for (s32 k = 14; k >= 5 && mSpot < 0; k--)
                {
                    const s32 x = lock.mX + mSide * k * cell;
                    const s32 n = nav.NodeAt(x, lock.mY, lock.mHalf, 6);
                    if (n < 0 || nav.Nodes()[n].mLine != lock.mLine || nav.Nodes()[n].mLow || nav.Nodes()[n].mHazard)
                    {
                        continue;
                    }
                    for (ThrowSpot::Kind kind : {ThrowSpot::eForward, ThrowSpot::eUp, ThrowSpot::eLob})
                    {
                    if (mSpot >= 0)
                    {
                        break;
                    }
                    const ThrowArc::Flight flight = ThrowArc::Throw(nav.Input(), nav.Nodes()[n].mX, lock.mY, lock.mHalf, mSide < 0, kind);
                    for (const ThrowArc::Point& p : flight.mPoints)
                    {
                        if (p.mY >= lock.mY - 12 && (p.mX - nav.Nodes()[n].mX) * mSide > 0)
                        {
                            const s32 land = nav.NodeAt(p.mX, lock.mY, lock.mHalf, 6);
                            if (land >= 0 && nav.Nodes()[land].mLine == lock.mLine && nav.CameraOf(p.mX, lock.mY - 5) == nav.CameraOf(nav.Nodes()[n].mX, lock.mY - 5))
                            {
                                // (Paramite::FindMeat looks for a wall between at the height of its own y above
                                // its feet, which is nowhere: no wall keeps it from seeing the meat)
                                mSpot = n;
                                mKind = kind;
                                bot.Log("meat for the Paramite at " + std::to_string(para->mX) + "," + std::to_string(para->mY) + ": thrown from " + std::to_string(nav.Nodes()[n].mX) + " to about " + std::to_string(p.mX));
                            }
                            bot.Log("meat from " + std::to_string(nav.Nodes()[n].mX) + " kind " + std::to_string(static_cast<s32>(kind)) + " comes down at " + std::to_string(p.mX) + "," + std::to_string(p.mY) + (mSpot >= 0 ? "" : ": no good"));
                            break;
                        }
                    }
                    }
                }
                if (mSpot < 0)
                {
                    return TaskStep::Failed("nowhere to throw the meat from");
                }
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
            if (mSpot < 0)
            {
                return para ? TaskStep::Do(Prim::Wait(1)) : TaskStep::Failed("the Paramite is not there");
            }
            mPhase = 1;
            return TaskStep::Do(Prim::Wait(1));
        }
        case 1:
            if (world.mRocks <= 0)
            {
                return TaskStep::Failed("no meat");
            }
            if (!abe.Settled())
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (abe.mFacingLeft != (mSide < 0))
            {
                return TaskStep::Do(Prim::Face(mSide < 0));
            }
            // (it comes to hand over a few frames, and goes when a direction is held as well)
            mPhase = 2;
            return TaskStep::Do(Prim::Wait(8, Btn::eThrow));
        case 2:
            mPhase = 3;
            mFrames = 0;
            return TaskStep::Do(Prim::Wait(6, Btn::eThrow | BlowMineTask::AimBits(mKind, abe.mFacingLeft)));
        case 3:
            // Until it is past him, at the meat
            if (para && (para->mX - abe.mX) * mSide > cell)
            {
                bot.Log("the Paramite is at the meat at " + std::to_string(para->mX) + ": to #" + std::to_string(mEntity));
                mPhase = 4;
                mUse = std::make_unique<UseTask>(mEntity, mWant);
                mUse->FixSide(mSide);
                return TaskStep::Do(Prim::Wait(1));
            }
            if (mFrames > 300)
            {
                return TaskStep::Failed("the Paramite did not go for the meat");
            }
            return TaskStep::Do(Prim::Wait(1));
        case 4:
            return mUse->Next(bot, lastFailed);
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
    for (const std::array<s32, 4>& spawner : world.mSlogSpawners)
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
            // (he is a good two seconds coming out of the chant, and takes no input until then: what is asked of
            // him before that goes nowhere and is taken for a way that is shut)
            if (!bot.Abe().Settled() && ++mBack < 200)
            {
                return TaskStep::Do(Prim::Wait(1));
            }
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
    // How many the spawner on this floor has in it, and how many have come out (each is seen where it comes out
    // before it runs: SlogSpawner.cpp makes them a good many frames apart)
    s32 holds = 0;
    {
        s32 atSpawner = 0;
        for (const std::array<s32, 4>& spawner : world.mSlogSpawners)
        {
            if (std::abs(spawner[1] - floorY) > 30 || (spawner[0] - abe.mX) * mSetup.mSide <= 0 || std::abs(spawner[0] - abe.mX) > 900)
            {
                continue;
            }
            holds = spawner[3];
            for (const SlogAt& slog : world.mSlogs)
            {
                atSpawner += std::abs(slog.mY - floorY) <= 30 && std::abs(slog.mX - spawner[0]) <= 30 ? 1 : 0;
            }
        }
        if (mAtSpawner < 0)
        {
            // (the ones that came out before he looked: all that are up on that side of him)
            for (const SlogAt& slog : world.mSlogs)
            {
                mCameOut += !slog.mAsleep && std::abs(slog.mY - floorY) <= 30 && (slog.mX - abe.mX) * mSetup.mSide > 0 && std::abs(slog.mX - abe.mX) <= 900 ? 1 : 0;
            }
            mAtSpawner = atSpawner;
        }
        mCameOut += std::max(0, atSpawner - mAtSpawner);
        mAtSpawner = atSpawner;
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
        // (all the spawner holds have come out and none is left: no more will. Else a good while with none.)
        const bool allOut = holds > 0 && mCameOut >= holds && mQuiet > 12;
        if ((mShot > 0 && (mQuiet > 160 || allOut)) || mFrames > 4000)
        {
            bot.Log("no more Slogs: " + std::to_string(mShot) + " bursts shot, " + std::to_string(mCameOut) + " seen to come out of " + std::to_string(holds) + ", the Slig is burst");
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
    // A teleporter a foot switch sets off: run over, not stopped on. It takes him from wherever he is by then, and
    // the stop is what the Slog catches him in (the Mudanchee trial: stopped a cell short of the plate out of a
    // run that looks every other cell, a step to the plate, and bitten there).
    if (mThrough < 0)
    {
        mThrough = 0;
        const NavGraph& nav = bot.Nav();
        for (const NavEdge& e : nav.Edges())
        {
            if (e.mTo != mRefuge || e.mKind != EdgeKind::eTeleport || std::abs(nav.Nodes()[e.mFrom].mX - mFromX) > 2)
            {
                continue;
            }
            const NavNode& plate = nav.Nodes()[e.mFrom];
            for (const Entity& thing : bot.GetWorld().mEntities)
            {
                if (thing.mKind == EntityKind::eFootSwitch && thing.mHalf == plate.mHalf && plate.mX >= thing.mX1 && plate.mX <= thing.mX2 && std::abs(thing.mY2 - plate.mY) <= 30)
                {
                    mThrough = 1;
                }
            }
        }
    }
    if (mThrough == 1 && !mGo)
    {
        const NavNode& there = bot.Nav().Nodes()[mRefuge];
        const bool taken = std::abs(abe.mY - there.mY) <= 30 && std::abs(abe.mX - there.mX) <= 40;
        // (the run kept up until it has: some 13 frames after the switch, measured, and what he arrives in is what
        // he was in)
        if (!taken && ahead > -150 && ++mHeld < 260)
        {
            return TaskStep::Do(Prim::Wait(1, (mLeft ? Btn::eLeft : Btn::eRight) | Btn::eRun));
        }
        if (taken)
        {
            // (still at a run there: what comes next goes on from it. A stop would cost what the run over the
            // plate saved.)
            return TaskStep::Done();
        }
        // (past it and still here: the old way, back to the plate)
        mThrough = 0;
    }
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
    // A bird portal of his own that leads out of the path: chanted open from four cells off or more on its
    // screen, before he comes any nearer (within three cells its doves fly off and it is gone until the screen
    // is come back to), and not from a floor Paramites hunt him on. The Mudomo vaults' path 7: "from on the
    // first ledge, chant to open the bird portal, or else you'll scare the birds away when you climb up".
    const NavFeature& way = nav.Input().mFeatures[exit.mFeature];
    if (way.mKind == FeatureKind::ePortal)
    {
        const s32 px = (way.mX1 + way.mX2) / 2;
        bool open = false;
        for (const RockAt& at : bot.GetWorld().mPortalsOpen)
        {
            open = open || (std::abs(at.mX - px) <= 40 && std::abs(at.mY - way.mY2) <= 120);
        }
        if (!open && !mArrived)
        {
            if (!mAtChant)
            {
                if (!mPortalGo)
                {
                    const NavGraph* g = &nav;
                    const World* w = &bot.GetWorld();
                    const s32 cam = nav.CameraOf(px, way.mY2 - 5);
                    const bool half = way.mHalf;
                    mPortalGo = std::make_unique<GoToTask>("to where the bird portal is chanted open from", [g, w, cam, half, px](s32 n)
                                                           {
                                                               const NavNode& at = g->Nodes()[n];
                                                               const s32 cell = NavGraph::GridStep(half);
                                                               return at.mHalf == half && !at.mLow && !at.mHazard && g->CameraOf(at.mX, at.mY - 5) == cam && std::abs(at.mX - px) >= 4 * cell &&
                                                                      !Domain::ParamitePack(*g, *w, at.mX, at.mY, at.mHalf);
                                                           });
                }
                TaskStep step;
                if (!RunGo(mPortalGo, bot, lastFailed, step))
                {
                    return step;
                }
                if (step.mKind == TaskStep::Kind::eFailed)
                {
                    return step;
                }
                mAtChant = true;
                bot.Log("chanting the bird portal at " + std::to_string(px) + "," + std::to_string(way.mY2) + " open from here");
            }
            if (!abe.Settled())
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (abe.mFacingLeft != (px < abe.mX))
            {
                return TaskStep::Do(Prim::Face(px < abe.mX));
            }
            if (++mPortalChants > 40)
            {
                return TaskStep::Failed("the bird portal did not open");
            }
            return TaskStep::Do(Prim::Wait(20, Btn::eChant));
        }
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
        // (one that lies to a side of him: that way)
        if (abe.mX < f.mX1 || abe.mX > f.mX2)
        {
            return TaskStep::Do(Prim::Wait(40, abe.mX > f.mX2 ? Btn::eLeft : Btn::eRight));
        }
        return TaskStep::Do(Prim::Wait(40, abe.mFacingLeft ? Btn::eLeft : Btn::eRight));
    }
    if (f.mKind == FeatureKind::ePortal)
    {
        // (hopped into from two cells off, facing it: it takes the hop itself)
        if (!abe.Settled())
        {
            mTries--;
            return TaskStep::Do(Prim::Wait(2));
        }
        if (abe.mFacingLeft != (f.mDir != 0))
        {
            mTries--;
            return TaskStep::Do(Prim::Face(f.mDir != 0));
        }
        return TaskStep::Do(Prim::Press(Btn::eHop));
    }
    return TaskStep::Do(Prim::Press(Btn::eUp));
}

ScrabLureTask::~ScrabLureTask() = default;
ScrabPenTask::~ScrabPenTask() = default;

FleechPenTask::~FleechPenTask() = default;

MeatFeedTask::~MeatFeedTask() = default;

TaskStep MeatFeedTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const AbeState& abe = bot.Abe();
    if (++mFrames > 1500)
    {
        return TaskStep::Failed("the meat took too long");
    }
    switch (mPhase)
    {
        case 0:
        {
            if (mSpot < 0)
            {
                std::vector<s32> path;
                const s32 cur = bot.AbeNode();
                if (cur < 0 || !nav.FindPath(cur, mTarget, bot.EdgeCostFn(), path))
                {
                    return TaskStep::Failed("no way to where the meat is for");
                }
                std::size_t at = path.size();
                for (std::size_t i = 0; i < path.size() && at == path.size(); i++)
                {
                    at = Domain::PackLevel(nav, world, nav.Edges()[path[i]].mTo) ? i : at;
                }
                if (at == path.size())
                {
                    bot.Log("no Paramites' level on the way: no meat thrown");
                    return TaskStep::Done();
                }
                mSpot = nav.Edges()[path[at]].mFrom;
                mOnto = nav.Edges()[path[at]].mTo;
                const NavNode& onto = nav.Nodes()[mOnto];
                // The way he goes on their level: to where the way leaves it
                s32 leave = onto.mX;
                for (std::size_t i = at + 1; i < path.size(); i++)
                {
                    const NavNode& to = nav.Nodes()[nav.Edges()[path[i]].mTo];
                    if (std::abs(to.mY - onto.mY) > 30)
                    {
                        break;
                    }
                    leave = to.mX;
                }
                if (leave == onto.mX)
                {
                    leave = nav.Nodes()[mTarget].mX;
                }
                mDir = leave < onto.mX ? -1 : 1;
                // The throw that puts it down on their level, the furthest from the way
                const NavNode& spot = nav.Nodes()[mSpot];
                const s32 cell = NavGraph::GridStep(spot.mHalf);
                s32 best = 0;
                for (ThrowSpot::Kind kind : {ThrowSpot::eUp, ThrowSpot::eLob, ThrowSpot::eForward})
                {
                    const ThrowArc::Flight flight = ThrowArc::Throw(nav.Input(), spot.mX, spot.mY, spot.mHalf, mDir > 0, kind);
                    for (std::size_t i = 1; i < flight.mPoints.size(); i++)
                    {
                        const ThrowArc::Point& p = flight.mPoints[i];
                        if (p.mY <= flight.mPoints[i - 1].mY)
                        {
                            continue;
                        }
                        const s32 land = nav.NodeAt(p.mX, p.mY, spot.mHalf, 14);
                        if (land < 0)
                        {
                            continue;
                        }
                        const NavNode& on = nav.Nodes()[land];
                        const s32 off = (on.mX - onto.mX) * -mDir;
                        bot.Log("meat from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY) + " kind " + std::to_string(static_cast<s32>(kind)) + " comes down at " + std::to_string(on.mX) + "," + std::to_string(on.mY));
                        if (std::abs(on.mY - onto.mY) * (spot.mHalf ? 2 : 1) <= 20 && nav.CameraOf(on.mX, on.mY - 5) == nav.CameraOf(onto.mX, onto.mY - 5) && off >= 2 * cell && off > best)
                        {
                            best = off;
                            mKind = kind;
                        }
                        break;
                    }
                }
                if (best == 0)
                {
                    return TaskStep::Failed("no throw puts the meat on the Paramites' level away from the way");
                }
                mLeft = mDir > 0;
                bot.Log("Paramites team up on the level at " + std::to_string(onto.mX) + "," + std::to_string(onto.mY) + ": meat for them from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY) + ", " + std::to_string(best) + " off the way");
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
            return TaskStep::Do(Prim::Wait(1));
        }
        case 1:
            if (world.mRocks <= 0)
            {
                return TaskStep::Failed("no meat");
            }
            if (!abe.Settled())
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            if (abe.mFacingLeft != mLeft)
            {
                return TaskStep::Do(Prim::Face(mLeft));
            }
            mPhase = 2;
            return TaskStep::Do(Prim::Wait(8, Btn::eThrow));
        case 2:
            mPhase = 3;
            mFrames = 0;
            return TaskStep::Do(Prim::Wait(6, Btn::eThrow | BlowMineTask::AimBits(mKind, abe.mFacingLeft)));
        default:
        {
            // Until every one of them on that level is past where he comes onto it, on the meat's side
            const NavNode& onto = nav.Nodes()[mOnto];
            const s32 cell = NavGraph::GridStep(onto.mHalf);
            s32 seen = 0;
            bool off = true;
            for (const ParamiteAt& p : world.mParamites)
            {
                if (p.mPossessed || p.Hidden() || p.mHalf != onto.mHalf || std::abs(p.mY - onto.mY) * (onto.mHalf ? 2 : 1) > 40 || nav.CameraOf(p.mX, p.mY - 5) != nav.CameraOf(onto.mX, onto.mY - 5))
                {
                    continue;
                }
                seen++;
                off = off && (p.mX - onto.mX) * -mDir > cell;
            }
            if (seen > 0 && off)
            {
                bot.Log("the Paramites are off after the meat: on");
                return TaskStep::Done();
            }
            if (mFrames > 300)
            {
                return TaskStep::Failed("the Paramites did not go for the meat");
            }
            return TaskStep::Do(Prim::Wait(1));
        }
    }
}

TaskStep FleechPenTask::Next(Bot& bot, const std::string& lastFailed)
{
    const World& world = bot.GetWorld();
    const NavGraph& nav = bot.Nav();
    const Entity* lever = world.Find(mLever);
    if (!lever)
    {
        return TaskStep::Failed("no such entity");
    }
    if (++mFrames > 3000)
    {
        return TaskStep::Failed("shutting the Fleeches in took too long");
    }
    if (mGate < 0 && !Domain::FleechPen(nav, world, *lever, &mGate, &mOpen, &mShut, &mGroup))
    {
        return TaskStep::Failed("no gate to shut the Fleeches in behind");
    }
    const NavFeature& gate = nav.Input().mFeatures[mGate];
    const s32 gx = (gate.mX1 + gate.mX2) / 2;
    const NavNode& open = nav.Nodes()[mOpen];
    const NavNode& shut = nav.Nodes()[mShut];
    const s32 cell = NavGraph::GridStep(gate.mHalf);
    const bool beyondRight = open.mX > gx;
    switch (mPhase)
    {
        case 0:
        {
            if (!mUse)
            {
                // (from the side he is off to: they come down where they hang, and that is by the lever)
                mUse = std::make_unique<UseTask>(mLever, 1);
                mUse->FixSide(beyondRight ? 1 : -1);
            }
            // (pulled, and on his feet again: off, without what else the pull might be waited out for)
            const bool pulled = world.Switch(lever->mSwitch, true) != 0 && bot.Abe().Settled();
            const TaskStep step = pulled ? TaskStep::Done() : mUse->Next(bot, lastFailed);
            if (step.mKind != TaskStep::Kind::eDone)
            {
                return step;
            }
            mUse.reset();
            mPhase = 1;
            mGo = GoToNode(mOpen);
            bot.Log("the lever is pulled and its Fleeches are awake: to the switch at " + std::to_string(open.mX) + "," + std::to_string(open.mY) + " that opens the gate at " + std::to_string(gx));
            return TaskStep::Do(Prim::Wait(1));
        }
        case 1:
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
            mPhase = 2;
            mWaits = 0;
            // (a cell off it toward the gate, so that they come down beyond him)
            const s32 near = nav.NodeAt(open.mX + (beyondRight ? -cell : cell), open.mY, open.mHalf, 12);
            if (near >= 0 && std::abs(nav.Nodes()[near].mX - gx) > cell / 2)
            {
                mGo = GoToNode(near);
            }
            return TaskStep::Do(Prim::Wait(1));
        }
        case 2:
        {
            TaskStep step;
            if (!RunGo(mGo, bot, lastFailed, step))
            {
                return step;
            }
            s32 seen = 0;
            bool all = true;
            for (const RockAt& f : world.mFleechesAwake)
            {
                if (f.mHalf != gate.mHalf || std::abs(f.mX - gx) > 600 || std::abs(f.mY - gate.mY2) > 300)
                {
                    continue;
                }
                seen++;
                all = all && (beyondRight ? f.mX > gx : f.mX < gx);
            }
            if (seen == 0 ? ++mWaits < 120 : (!all && ++mWaits < 400))
            {
                return TaskStep::Do(Prim::Wait(1));
            }
            bot.Log(std::to_string(seen) + " Fleeches about, " + (all ? "all" : "not all") + " beyond the gate at " + std::to_string(gx) + ": back through it over the switch at " + std::to_string(shut.mX));
            mPhase = 3;
            // (two cells past the switch that shuts it)
            const s32 past = nav.NodeAt(shut.mX + (beyondRight ? -2 * cell : 2 * cell), shut.mY, shut.mHalf, 12);
            mGo = GoToNode(past >= 0 ? past : mShut);
            return TaskStep::Do(Prim::Wait(1));
        }
        default:
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
            bool in = world.Switch(gate.mSwitch, true) == 0;
            for (const RockAt& f : world.mFleechesAwake)
            {
                if (f.mHalf == gate.mHalf && std::abs(f.mX - gx) <= 600 && std::abs(f.mY - gate.mY2) <= 300)
                {
                    in = in && (beyondRight ? f.mX > gx : f.mX < gx);
                }
            }
            bot.Log(in ? "the gate is shut on the Fleeches" : "the Fleeches are not all behind the gate");
            for (u32 id : mGroup)
            {
                if (in)
                {
                    bot.FleechKilled(id);
                }
            }
            return TaskStep::Done();
        }
    }
}

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
    // the one (the Mudanchee trial: the Fleeches at him on the ledge leave no time to wait for the first). Only
    // with something after him where he waits: else the wait costs nothing, and the other way may be a crossing
    // to another ledge that gets him no nearer (Mudanchee vault path 5: to and fro over the floor for ever).
    if (!best && !bot.GetWorld().mFleechesAwake.empty() && stint.mFound && stint.mFirst >= 0 && Spare(bot, stint, hot, false) < kWaryLeast)
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
