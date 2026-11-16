#include "Domain.hpp"
#include "FartFlight.hpp"
#include "ThrowArc.hpp"
#include <algorithm>
#include <cstdlib>
#include <set>
#include <tuple>

namespace Bot {

// How far from its spawner, along and up or down together, a Slig or a lever is one for the flying Slig
static const s32 kFlierReach = 1200;
// How far to each side of its point a flying Slig is, for what it flies past (17, and a little)
static const s32 kFlierSide = 20;
// How far over and under its point it is, for the floors it flies in beside
static const s32 kFlierOver = 24;
static const s32 kFlierUnder = 8;

const char* EntityKindName(EntityKind kind)
{
    switch (kind)
    {
        case EntityKind::eLever: return "Lever";
        case EntityKind::ePullRing: return "PullRing";
        case EntityKind::eWorkWheel: return "WorkWheel";
        case EntityKind::eMudokon: return "Mudokon";
        case EntityKind::eBirdPortal: return "BirdPortal";
        case EntityKind::eSwitchLogic: return "SwitchLogic";
        case EntityKind::eSlig: return "Slig";
        case EntityKind::eSecurityOrb: return "SecurityOrb";
        case EntityKind::eHandStone: return "HandStone";
        case EntityKind::eSlapLock: return "SlapLock";
        case EntityKind::eFleech: return "Fleech";
        case EntityKind::eScrab: return "Scrab";
        case EntityKind::eLaughingGas: return "LaughingGas";
        case EntityKind::eTeleporter: return "Teleporter";
        case EntityKind::eSligLock: return "SligLock";
        case EntityKind::eMine: return "Mine";
        case EntityKind::eUxb: return "UXB";
        case EntityKind::eCrawler: return "Crawler";
        case EntityKind::ePantsLocker: return "Locker";
        case EntityKind::eRockSack: return "RockSack";
        case EntityKind::eBrew: return "Brew";
        case EntityKind::eFlier: return "FlyingSlig";
        case EntityKind::eFlierSpawner: return "FlierSpawner";
        case EntityKind::eDrill: return "Drill";
        case EntityKind::eMineCar: return "MineCar";
        case EntityKind::eParamite: return "Paramite";
        case EntityKind::eFootSwitch: return "FootSwitch";
    }
    return "?";
}

bool Domain::CallSpot(const NavGraph& nav, const Entity& mud, const Entity& wheel, s32 node)
{
    const NavNode& n = nav.Nodes()[node];
    const s32 mudX = mud.mLive ? mud.mLiveX : mud.CentreX();
    const s32 mudY = mud.mLive ? mud.mLiveY : mud.mY2;
    const s32 dir = wheel.CentreX() > mudX ? 1 : -1;
    return n.mHalf == mud.mHalf && !n.mLow && !n.mHazard && n.mTrapDoor < 0 && n.mSweep < 0 && (n.mX - mudX) * dir > 12 && nav.CameraOf(n.mX, n.mY - 5) == nav.CameraOf(mudX, mudY - 5);
}

const Entity* Domain::DrillAt(const NavGraph& nav, const World& world, s32 node, bool high)
{
    const s32 feature = high ? nav.Nodes()[node].mSweepHigh : nav.Nodes()[node].mSweep;
    const Entity* drill = feature >= 0 ? world.Find(nav.Input().mFeatures[feature].mId) : nullptr;
    return drill && drill->mKind == EntityKind::eDrill ? drill : nullptr;
}

bool Domain::BlindWay(const NavGraph& nav, const World& world, s32 from, const Entity& portal, bool strict, std::vector<s32>& way)
{
    way.clear();
    const s32 at = nav.NodeAt(portal.CentreX(), portal.mY2, portal.mHalf, 30);
    if (at < 0 || from < 0)
    {
        return false;
    }
    // Onto a lift's platform, or down or up with it (Abe works it): only where the lift is, or will be brought
    const auto liftLets = [&](const NavEdge& e, bool now)
    {
        const NavNode& a = nav.Nodes()[e.mFrom];
        const NavNode& b = nav.Nodes()[e.mTo];
        const NavNode* stop = e.mKind == EdgeKind::eLift ? &a : (b.mLiftStop >= 0 && a.mLiftStop < 0 ? &b : nullptr);
        if (!stop)
        {
            return true;
        }
        const NavFeature& f = nav.Input().mFeatures[stop->mLiftStop];
        return world.LiftAtStop(f.mX1, f.mX2, stop->mY, f.mA != 0) || (!now && (f.mSwitch > 1 || e.mKind == EdgeKind::eLift));
    };
    std::vector<bool> goal(nav.Nodes().size(), false);
    std::vector<s32> open{at};
    goal[at] = true;
    while (!open.empty())
    {
        const NavNode& n = nav.Nodes()[open.back()];
        open.pop_back();
        for (s32 i = n.mFirstEdge; i < n.mFirstEdge + n.mEdgeCount; i++)
        {
            const NavEdge& e = nav.Edges()[i];
            const NavNode& to = nav.Nodes()[e.mTo];
            // (a mine between it and the portal is no place to send it running from)
            s32 sw = 0;
            s32 value = 0;
            const bool shut = (nav.Condition(e, &sw, &value) && world.Switch(sw) != value) || !liftLets(e, true);
            if (e.mKind == EdgeKind::eWalk && !goal[e.mTo] && !shut && !(to.mHazard && to.mClear < 0) && to.mY == n.mY && std::abs(to.mX - portal.CentreX()) < 300)
            {
                goal[e.mTo] = true;
                open.push_back(e.mTo);
            }
        }
    }
    if (goal[from])
    {
        return true;
    }
    // A drill in the way: not while it runs, and only one a switch stops
    const auto drillLets = [&](s32 node, bool high)
    {
        const Entity* drill = DrillAt(nav, world, node, high);
        if (!drill)
        {
            return true;
        }
        return strict ? !world.SweepLive(*drill) : (drill->mInputs[0] == 1 && drill->mInputs[2] == 0);
    };
    const auto can = [&](const NavEdge& e)
    {
        const NavNode& a = nav.Nodes()[e.mFrom];
        const NavNode& b = nav.Nodes()[e.mTo];
        // (it walks upright where Abe has to roll under a drill)
        const bool under = e.mKind == EdgeKind::eRoll && (a.mSweepHigh >= 0 || b.mSweepHigh >= 0) && (!a.mLow || a.mSweepHigh >= 0) && (!b.mLow || b.mSweepHigh >= 0);
        // (the hole of an open trap door is not something it can be timed into)
        if (e.mKind == EdgeKind::eFall && e.mFeature >= 0 && a.mTrapDoor != e.mFeature && nav.Input().mFeatures[e.mFeature].mKind == FeatureKind::eTrapDoor)
        {
            return false;
        }
        if (!liftLets(e, strict))
        {
            return false;
        }
        if ((b.mHazard && b.mClear < 0) || (e.mKind != EdgeKind::eWalk && e.mKind != EdgeKind::eFall && e.mKind != EdgeKind::eLift && !under) || !drillLets(e.mTo, false) || !drillLets(e.mTo, true))
        {
            return false;
        }
        s32 sw = 0;
        s32 value = 0;
        if (strict && nav.Condition(e, &sw, &value) && world.Switch(sw) != value)
        {
            return e.mKind == EdgeKind::eFall && a.mTrapDoor >= 0 && e.mFeature == a.mTrapDoor;
        }
        return true;
    };
    std::vector<s32> via(nav.Nodes().size(), -1);
    std::vector<s32> queue{from};
    s32 found = -1;
    for (std::size_t q = 0; q < queue.size() && found < 0; q++)
    {
        const NavNode& n = nav.Nodes()[queue[q]];
        for (s32 i = n.mFirstEdge; i < n.mFirstEdge + n.mEdgeCount && found < 0; i++)
        {
            const NavEdge& e = nav.Edges()[i];
            if (e.mTo != from && via[e.mTo] < 0 && can(e))
            {
                via[e.mTo] = i;
                queue.push_back(e.mTo);
                found = goal[e.mTo] ? e.mTo : -1;
            }
        }
    }
    if (found < 0)
    {
        return false;
    }
    for (s32 n = found; n != from; n = nav.Edges()[via[n]].mFrom)
    {
        way.insert(way.begin(), via[n]);
    }
    return true;
}

bool Domain::SackJump(const NavGraph& nav, const Entity& sack, s32* from, s32* takeoffX, bool* left)
{
    // (the run-up a running jump takes, see NavGraph::AddRunJumps)
    const s32 kRunUp = 3;
    const s32 step = NavGraph::GridStep(sack.mHalf);
    const s32 scale = sack.mHalf ? 2 : 1;
    const s32 cx = sack.CentreX();
    s32 bestDist = 0;
    bool found = false;
    for (std::size_t i = 0; i < nav.Nodes().size(); i++)
    {
        const NavNode& n = nav.Nodes()[i];
        // A ledge the sack hangs at head height beside, one to four cells off
        const s32 dist = std::abs(n.mX - cx);
        if (n.mHalf != sack.mHalf || n.mHazard || (n.mY - sack.mY2) * scale < 15 || (n.mY - sack.mY2) * scale > 75 || dist * scale < 25 || dist * scale > 100)
        {
            continue;
        }
        const s32 dir = cx > n.mX ? 1 : -1;
        // No floor on from it towards the sack, and a floor not far below to come down on
        s32 fy = 0;
        if (nav.FloorNear(n.mX + dir * step, n.mY, n.mHalf, 12, &fy) >= 0)
        {
            continue;
        }
        s32 drop = 0;
        if (nav.FloorBelow(cx, n.mY + 5, n.mHalf, &fy, &drop) < 0 || drop * scale > 150 || nav.NodeAt(cx, fy, n.mHalf, 8) < 0)
        {
            continue;
        }
        // Room to get up to speed
        s32 start = static_cast<s32>(i);
        for (s32 k = 0; k < kRunUp && start >= 0; k++)
        {
            const s32 back = nav.NodeAt(nav.Nodes()[start].mX - dir * step, n.mY, n.mHalf, 8);
            start = back >= 0 && nav.Nodes()[back].mLine == n.mLine && !nav.Nodes()[back].mHazard && !nav.Nodes()[back].mLow ? back : -1;
        }
        if (start >= 0 && (!found || dist < bestDist))
        {
            found = true;
            bestDist = dist;
            *from = start;
            *takeoffX = n.mX;
            *left = dir < 0;
        }
    }
    return found;
}

s32 Domain::UseNode(const NavGraph& nav, const Entity& entity, bool* faceLeft, s32 firstSide)
{
    const s32 step = NavGraph::GridStep(entity.mHalf);
    const s32 cx = entity.CentreX();
    bool left = false;
    s32 node = -1;
    switch (entity.mKind)
    {
        case EntityKind::eLever:
        {
            // In its TLV, the lever one cell ahead
            const s32 leverX = nav.SnapUp(cx - step / 2, entity.mHalf);
            for (s32 i = 0; i < 2 && node < 0; i++)
            {
                const s32 side = i == 0 ? firstSide : -firstSide;
                const s32 x = leverX + side * step;
                if (x >= entity.mX1 && x <= entity.mX2)
                {
                    node = nav.NodeAt(x, entity.mY2, entity.mHalf, 30);
                    left = side > 0;
                    // (a cell nothing leads into is no place to stand, walled off as it is: the other side then)
                    bool into = false;
                    for (const NavEdge& e : nav.Edges())
                    {
                        into = into || e.mTo == node;
                    }
                    // (nor is one with a wall between it and the lever: Necrum's path 6, the lever at 412,360)
                    const s32 leverNode = nav.NodeAt(leverX, entity.mY2, entity.mHalf, 30);
                    bool reaches = leverNode < 0 || node < 0;
                    for (const NavEdge& e : nav.Edges())
                    {
                        reaches = reaches || (e.mFrom == node && e.mTo == leverNode) || (e.mFrom == leverNode && e.mTo == node);
                    }
                    if (node >= 0 && i == 0 && (!into || !reaches))
                    {
                        node = -1;
                    }
                    // (nor, with another side to it, the cell a door stands in: every way into it waits on a
                    // switch. The Barracks' first path: Lever#7005 at 2187,1500 with the slam door it opens in
                    // the cell at 2162, pulled from 2212.)
                    if (node >= 0 && i == 0)
                    {
                        // (a door, shut, whose box that cell's middle is in: not an electric wall beside it, which
                        // the lever by it is there to turn off. The Mines' path 3, the Slogs' trap at 2337,1500.)
                        bool barred = false;
                        for (const NavFeature& f : nav.Input().mFeatures)
                        {
                            barred = barred || (f.mKind == FeatureKind::eBarrier && f.mSolid && f.mStartClosed && f.mHalf == entity.mHalf && x >= f.mX1 && x <= f.mX2 && entity.mY2 >= f.mY1 - 10 && entity.mY2 <= f.mY2 + 10);
                        }
                        // (and no way into it that does not wait on a switch: a gate he can walk up to from one
                        // side is where its lever is pulled from as ever)
                        for (const NavEdge& e : nav.Edges())
                        {
                            s32 sw = 0;
                            s32 value = 0;
                            barred = barred && (e.mTo != node || nav.Condition(e, &sw, &value));
                        }
                        const s32 other = nav.NodeAt(leverX - side * step, entity.mY2, entity.mHalf, 30);
                        if (barred && other >= 0 && leverX - side * step >= entity.mX1 && leverX - side * step <= entity.mX2)
                        {
                            node = -1;
                        }
                    }
                }
            }
            break;
        }
        case EntityKind::ePullRing:
        {
            // Under the ring, on the floor he can jump up to it from: he reaches 75 and jumps 22
            s32 fy = 0;
            const s32 ringY = entity.mY1 + entity.mInputs[0];
            if (nav.FloorBelow(cx, ringY, entity.mHalf, &fy) >= 0 && fy - ringY <= (entity.mHalf ? 48 : 96))
            {
                node = nav.NodeAt(cx, fy, entity.mHalf, 8);
            }
            if (node < 0)
            {
                node = nav.NodeAt(cx, entity.mY2, entity.mHalf, 40);
            }
            break;
        }
        case EntityKind::eWorkWheel:
        case EntityKind::eHandStone:
        case EntityKind::eBrew:
            node = nav.NodeAt(cx, entity.mY2, entity.mHalf, 40);
            break;
        case EntityKind::eSlapLock:
        {
            // From the cell beside it, facing it (the slap reaches a cell ahead)
            for (s32 i = 0; i < 2 && node < 0; i++)
            {
                const s32 side = i == 0 ? firstSide : -firstSide;
                node = nav.NodeAt(cx + side * step, entity.mY2, entity.mHalf, 30);
                left = side > 0;
            }
            break;
        }
        case EntityKind::eBirdPortal:
        {
            // A few cells off, either side, with nothing between it and the portal
            const s32 at = nav.NodeAt(cx, entity.mY2, entity.mHalf, 30);
            s32 bestCells = 0;
            for (s32 side = -1; side <= 1 && at >= 0; side += 2)
            {
                s32 cur = at;
                for (s32 cells = 1; cells <= 3; cells++)
                {
                    s32 next = -1;
                    const NavNode& n = nav.Nodes()[cur];
                    for (s32 i = n.mFirstEdge; i < n.mFirstEdge + n.mEdgeCount && next < 0; i++)
                    {
                        const NavEdge& e = nav.Edges()[i];
                        if (e.mKind == EdgeKind::eWalk && nav.Nodes()[e.mTo].mLine == n.mLine && (nav.Nodes()[e.mTo].mX - n.mX) * side > 0)
                        {
                            next = e.mTo;
                        }
                    }
                    if (next < 0)
                    {
                        break;
                    }
                    cur = next;
                    if (cells > bestCells)
                    {
                        bestCells = cells;
                        node = cur;
                        left = side > 0;
                    }
                }
            }
            if (node < 0)
            {
                node = at;
            }
            break;
        }
        case EntityKind::eRockSack:
        {
            // It hangs over the floor: the cell under it
            s32 fy = 0;
            if (nav.FloorBelow(cx, entity.mY1, entity.mHalf, &fy) >= 0 && fy - entity.mY2 <= (entity.mHalf ? 50 : 100))
            {
                node = nav.NodeAt(cx, fy, entity.mHalf, 8);
            }
            if (node < 0)
            {
                s32 takeoffX = 0;
                SackJump(nav, entity, &node, &takeoffX, &left);
            }
            break;
        }
        case EntityKind::eMine:
            node = nav.NodeAt(cx, entity.mY2, entity.mHalf, 30);
            break;
        case EntityKind::eMudokon:
            // Its own cell: where Abe stands to talk to it is up to the task
            node = nav.NodeAt(entity.mLive ? entity.mLiveX : cx, entity.mLive ? entity.mLiveY : entity.mY2, entity.mHalf, 30);
            break;
        default:
            break;
    }
    if (faceLeft)
    {
        *faceLeft = left;
    }
    return node;
}

bool Domain::FlierTrack(const NavInput& in, s32 fx, s32 fy, s32& x1, s32& y1, s32& x2, s32& y2, std::vector<std::pair<s32, s32>>* ends, std::vector<NavLine>* lines)
{
    static const s32 kTrackLine = 8;
    // The line nearest to it, then every line that shares an end with one already taken
    std::vector<bool> taken(in.mLines.size(), false);
    s64 best = -1;
    s32 first = -1;
    for (std::size_t i = 0; i < in.mLines.size(); i++)
    {
        const NavLine& line = in.mLines[i];
        if (line.mType != kTrackLine)
        {
            continue;
        }
        const s64 mx = (line.mX1 + line.mX2) / 2 - fx;
        const s64 my = (line.mY1 + line.mY2) / 2 - fy;
        const s64 dist = mx * mx + my * my;
        if (best < 0 || dist < best)
        {
            best = dist;
            first = static_cast<s32>(i);
        }
    }
    if (first < 0)
    {
        return false;
    }
    taken[first] = true;
    for (bool more = true; more;)
    {
        more = false;
        for (std::size_t i = 0; i < in.mLines.size(); i++)
        {
            const NavLine& a = in.mLines[i];
            if (taken[i] || a.mType != kTrackLine)
            {
                continue;
            }
            for (std::size_t k = 0; k < in.mLines.size() && !taken[i]; k++)
            {
                const NavLine& b = in.mLines[k];
                const auto same = [](s32 px, s32 py, s32 qx, s32 qy)
                {
                    return std::abs(px - qx) <= 2 && std::abs(py - qy) <= 2;
                };
                if (taken[k] && (same(a.mX1, a.mY1, b.mX1, b.mY1) || same(a.mX1, a.mY1, b.mX2, b.mY2) || same(a.mX2, a.mY2, b.mX1, b.mY1) || same(a.mX2, a.mY2, b.mX2, b.mY2)))
                {
                    taken[i] = true;
                    more = true;
                }
            }
        }
    }
    bool any = false;
    std::vector<std::pair<s32, s32>> points;
    for (std::size_t i = 0; i < in.mLines.size(); i++)
    {
        if (!taken[i])
        {
            continue;
        }
        const NavLine& line = in.mLines[i];
        x1 = any ? std::min({x1, line.mX1, line.mX2}) : std::min(line.mX1, line.mX2);
        x2 = any ? std::max({x2, line.mX1, line.mX2}) : std::max(line.mX1, line.mX2);
        y1 = any ? std::min({y1, line.mY1, line.mY2}) : std::min(line.mY1, line.mY2);
        y2 = any ? std::max({y2, line.mY1, line.mY2}) : std::max(line.mY1, line.mY2);
        any = true;
        if (lines)
        {
            lines->push_back(line);
        }
        points.push_back({line.mX1, line.mY1});
        points.push_back({line.mX2, line.mY2});
    }
    if (ends)
    {
        // (an end is a point only one of its lines has)
        ends->clear();
        for (const std::pair<s32, s32>& p : points)
        {
            s32 count = 0;
            for (const std::pair<s32, s32>& q : points)
            {
                count += std::abs(p.first - q.first) <= 2 && std::abs(p.second - q.second) <= 2 ? 1 : 0;
            }
            if (count == 1)
            {
                ends->push_back(p);
            }
        }
    }
    return true;
}

std::vector<ThrowSpot> Domain::ThrowSpots(const NavGraph& nav, s32 x, s32 y, bool half, const World* world)
{
    // Which throw to like when several hit: the ones that roll along the floor are in the air the shortest
    // time, and a throw from the mine's own floor needs no climb
    static const ThrowSpot::Kind kKinds[] = {ThrowSpot::eDown, ThrowSpot::eCrouched, ThrowSpot::eUp, ThrowSpot::eForward, ThrowSpot::eLob};
    const s32 scale = half ? 2 : 1;
    // The mine sits in the middle of its cell, on its floor
    const s32 mineNode = nav.NodeAt(x, y, half, 12);
    const s32 mineX = mineNode >= 0 ? nav.Nodes()[mineNode].mX : x;
    const s32 mineY = mineNode >= 0 ? nav.Nodes()[mineNode].mY : y;
    std::vector<ThrowSpot> spots;
    for (std::size_t i = 0; i < nav.Nodes().size(); i++)
    {
        const NavNode& n = nav.Nodes()[i];
        // (where a mine was is a place to stand once it is gone)
        // (nor where a drill comes by: a throw takes too long)
        if (n.mHalf != half || (n.mHazard && n.mClear < 0) || (n.mSweep >= 0 && (!world || world->SweepOver(n.mX, n.mY, n.mHalf))))
        {
            continue;
        }
        const s32 dy = (n.mY - mineY) * scale;
        const s32 dx = std::abs(n.mX - mineX) * scale;
        if (dx == 0 || dx > 450 || std::abs(dy) > 200)
        {
            continue;
        }
        // Not from inside the blast
        if (dx <= ThrowArc::kBlastX && std::abs(dy) <= ThrowArc::kBlastY)
        {
            continue;
        }
        // Crouching at the top of a hoist is climbing down it
        bool hoistTop = false;
        for (s32 e = n.mFirstEdge; e < n.mFirstEdge + n.mEdgeCount; e++)
        {
            hoistTop = hoistTop || nav.Edges()[e].mKind == EdgeKind::eHoistDown;
        }
        const s32 level = std::abs(dy) <= 10 ? 0 : (dy > 0 ? -1 : 1);
        const s32 cells = (dx + 12) / 25;
        for (std::size_t k = 0; k < sizeof(kKinds) / sizeof(kKinds[0]); k++)
        {
            const ThrowSpot::Kind kind = kKinds[k];
            // (under a low ceiling there is only the crouched throw)
            if ((n.mLow && kind != ThrowSpot::eCrouched) || (hoistTop && kind == ThrowSpot::eCrouched))
            {
                continue;
            }
            const ThrowArc::Point target{mineX, mineY};
            const ThrowArc::Flight flight = ThrowArc::Throw(nav.Input(), n.mX, n.mY, half, mineX < n.mX, kind, &target);
            const s32 hit = flight.mHit;
            if (hit < 0)
            {
                continue;
            }
            // (AO: nor from where the grenade's own blast reaches him, where it lies when it goes off)
            if (!nav.Input().mIsAe && !flight.mPoints.empty())
            {
                const ThrowArc::Point& end = flight.mPoints.back();
                if (std::abs(end.mX - n.mX) * scale <= ThrowArc::kGrenadeKillX && (n.mY - end.mY) * scale >= -ThrowArc::kGrenadeKillUp && (n.mY - end.mY) * scale <= ThrowArc::kGrenadeKillDown)
                {
                    continue;
                }
            }
            ThrowSpot spot;
            spot.mSure = flight.mSure;
            spot.mNode = static_cast<s32>(i);
            spot.mKind = kind;
            spot.mCells = cells;
            spot.mLevel = level;
            // (the ones that may miss last)
            spot.mOrder = (flight.mSure ? 0 : 100000) + (level == 0 ? 0 : 1000) + static_cast<s32>(k) * 100 + hit;
            if (n.mClear >= 0)
            {
                spot.mNeeds.push_back(nav.Input().mFeatures[n.mClear].mSwitch);
            }
            for (const NavNode& other : nav.Nodes())
            {
                if (other.mClear < 0 || other.mHalf != half || (other.mX == mineX && other.mY == mineY))
                {
                    continue;
                }
                // Any blast on the screen knocks Abe over if he is standing (not crouched), and he falls on a mine
                // that is a cell or two from him (measured: Mines path 4, the first throw, one cell behind him)
                const s32 swNear = nav.Input().mFeatures[other.mClear].mSwitch;
                if (kind != ThrowSpot::eCrouched && std::abs(other.mX - n.mX) * scale <= 62 && std::abs(other.mY - n.mY) * scale <= 30 && std::find(spot.mNeeds.begin(), spot.mNeeds.end(), swNear) == spot.mNeeds.end())
                {
                    spot.mNeeds.push_back(swNear);
                }
                // A mine the rock gets to first takes it, and one by where he stands that the rock comes to at
                // all takes him with it: those have to be gone. (A blast sets no other mine off.)
                const s32 first = ThrowArc::HitFrame(flight, other.mX, other.mY, half, false);
                const bool nearHim = std::abs(other.mX - n.mX) * scale <= ThrowArc::kBlastX && std::abs(other.mY - n.mY) * scale <= ThrowArc::kBlastY;
                const s32 sw = nav.Input().mFeatures[other.mClear].mSwitch;
                if (first >= 0 && (first <= hit || nearHim) && std::find(spot.mNeeds.begin(), spot.mNeeds.end(), sw) == spot.mNeeds.end())
                {
                    spot.mNeeds.push_back(sw);
                }
            }
            // A slam door in the rock's way has to be open: shut it is a wall, eighty high, a cell wide
            for (const NavFeature& door : nav.Input().mFeatures)
            {
                if (door.mKind != FeatureKind::eBarrier || !door.mSolid || door.mHalf != half)
                {
                    continue;
                }
                const s32 left = door.mX1 - 5;
                const s32 right = door.mX1 + NavGraph::GridStep(half) + 5;
                const s32 top = door.mY2 - 88 / scale;
                const s32 bottom = door.mY2 + 6;
                bool through = false;
                for (s32 f = 0; f + 1 <= hit && f + 1 < static_cast<s32>(flight.mPoints.size()); f++)
                {
                    const ThrowArc::Point& a = flight.mPoints[f];
                    const ThrowArc::Point& b = flight.mPoints[f + 1];
                    through = through || (std::max(a.mX, b.mX) >= left && std::min(a.mX, b.mX) <= right && std::max(a.mY, b.mY) >= top && std::min(a.mY, b.mY) <= bottom);
                }
                if (through)
                {
                    spot.mOpen.push_back({door.mSwitch, door.mStartClosed ? 1 : 0});
                }
            }
            // (a crouched throw along the floor goes under a drill that is overhead)
            const s32 top = std::min(n.mY, mineY) - (level == 0 && kind == ThrowSpot::eCrouched ? 30 : 70) / scale;
            for (const Entity& drill : world ? world->mEntities : std::vector<Entity>{})
            {
                if (drill.mKind == EntityKind::eDrill && drill.mHalf == half && drill.mX1 <= std::max(n.mX, mineX) && drill.mX2 >= std::min(n.mX, mineX) && drill.mY1 <= std::max(n.mY, mineY) && drill.mY2 >= top)
                {
                    spot.mDrill = drill.mId;
                }
            }
            spots.push_back(spot);
        }
    }
    std::stable_sort(spots.begin(), spots.end(), [](const ThrowSpot& a, const ThrowSpot& b)
                     { return a.mOrder < b.mOrder; });
    return spots;
}

bool Domain::SligWatched(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, std::vector<u32>* watchers, bool* anon)
{
    bool power = world.mInvisCharge || world.mInvisible || world.mInvisComing;
    for (const Entity& e : world.mEntities)
    {
        power = power || (e.mKind == EntityKind::eSlapLock && e.mInputs[0] > 0);
    }
    const s32 node = power ? nav.NodeAt(x, y, half, 12) : -1;
    if (node < 0 || nav.Nodes()[node].mHidden)
    {
        return false;
    }
    const std::vector<NavNode>& nodes = nav.Nodes();
    const s32 cam = nav.CameraOf(x, y - 5);
    const s32 line = nodes[node].mLine;
    for (const NavNode& n : nodes)
    {
        if (n.mHidden && n.mLine == line && n.mHalf == half && nav.CameraOf(n.mX, n.mY - 5) == cam)
        {
            return false;
        }
    }
    bool any = false;
    for (const Entity& e : world.mEntities)
    {
        // (one that starts asleep is another matter. One a spawner makes counts when it is seen there, below:
        // its switch says little, the Mudanchee trial's last room has one on with no Slig on the floor)
        if (e.mKind != EntityKind::eSlig || e.mGone || e.mHalf != half || e.mState == 1 || e.mSwitch > 0 || world.mFleechKilled.count(e.mId) != 0)
        {
            continue;
        }
        const s32 at = SligNode(nav, e);
        if (at >= 0 && nodes[at].mLine == line && nav.CameraOf(nodes[at].mX, nodes[at].mY - 5) == cam)
        {
            any = true;
            if (watchers && std::find(watchers->begin(), watchers->end(), e.mId) == watchers->end())
            {
                watchers->push_back(e.mId);
            }
        }
    }
    for (const SligAt& s : world.mSligs)
    {
        if (s.mPossessed || s.mAsleep || s.mHalf != half || nav.CameraOf(s.mX, s.mY - 5) != cam)
        {
            continue;
        }
        const s32 at = nav.NodeAt(s.mX, s.mY, half, 12);
        if (at >= 0 && nodes[at].mLine == line && !any)
        {
            // (one that has walked here from another screen, or one a spawner made)
            any = true;
            if (anon)
            {
                *anon = true;
            }
        }
    }
    return any;
}

s32 Domain::SligBeat(const NavGraph& nav, const World& world, s32 node)
{
    const NavNode& n = nav.Nodes()[node];
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eSlig || e.mGone || e.mHalf != n.mHalf || e.mState == 1 || e.mSwitch > 0 || world.mFleechKilled.count(e.mId) != 0)
        {
            continue;
        }
        const s32 at = SligNode(nav, e);
        if (at >= 0 && nav.Nodes()[at].mLine == n.mLine)
        {
            return n.mLine;
        }
    }
    return -1;
}

std::vector<s32> Domain::SligWay(const NavGraph& nav, s32 from, const std::function<bool(s32)>& goal, std::vector<Goap::Fact>* gates, const World* world)
{
    const std::vector<NavNode>& nodes = nav.Nodes();
    const std::vector<NavEdge>& edges = nav.Edges();
    if (from < 0 || from >= static_cast<s32>(nodes.size()) || goal(from))
    {
        return {};
    }
    static const s32 kShut = 100000;
    std::vector<s32> cost(nodes.size(), -1);
    std::vector<s32> via(nodes.size(), -1);
    std::set<std::pair<s32, s32>> open;
    cost[from] = 0;
    open.insert({0, from});
    s32 found = -1;
    while (!open.empty())
    {
        const s32 at = open.begin()->second;
        const s32 c = open.begin()->first;
        open.erase(open.begin());
        if (goal(at))
        {
            found = at;
            break;
        }
        for (s32 k = 0; k < nodes[at].mEdgeCount; k++)
        {
            const s32 idx = nodes[at].mFirstEdge + k;
            const NavEdge& e = edges[idx];
            const bool takes = e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eFall || e.mKind == EdgeKind::eLift || e.mKind == EdgeKind::eTeleport;
            if (!takes || nodes[e.mTo].mLow || nodes[e.mTo].mHazard || e.mCrouched)
            {
                continue;
            }
            s32 extra = 0;
            s32 switchId = 0;
            s32 openValue = 0;
            // (a teleporter goes when its lever is pulled: no gate)
            // (nor is a floor Abe is kept off for what is on it shut to a Slig: it shoots what it meets there)
            const bool danger = e.mClear >= 0 && nav.Input().mFeatures[e.mClear].mKind == FeatureKind::eDanger;
            if (e.mKind != EdgeKind::eTeleport && !danger && nav.Condition(e, &switchId, &openValue))
            {
                if (world && !gates)
                {
                    if (world->Switch(switchId) != openValue)
                    {
                        continue;
                    }
                }
                else
                {
                    extra = kShut;
                }
            }
            const s32 next = c + std::max(1, e.mCost) + extra;
            if (cost[e.mTo] < 0 || next < cost[e.mTo])
            {
                if (cost[e.mTo] >= 0)
                {
                    open.erase({cost[e.mTo], e.mTo});
                }
                cost[e.mTo] = next;
                via[e.mTo] = idx;
                open.insert({next, e.mTo});
            }
        }
    }
    std::vector<s32> way;
    for (s32 at = found; at >= 0 && via[at] >= 0; at = edges[via[at]].mFrom)
    {
        way.push_back(via[at]);
    }
    std::reverse(way.begin(), way.end());
    for (s32 idx : way)
    {
        s32 switchId = 0;
        s32 openValue = 0;
        if (gates && edges[idx].mKind != EdgeKind::eTeleport && nav.Condition(edges[idx], &switchId, &openValue))
        {
            const Goap::Fact need{Key::Switch(switchId), openValue};
            if (std::find(gates->begin(), gates->end(), need) == gates->end())
            {
                gates->push_back(need);
            }
        }
    }
    return way;
}

s32 Domain::SligNode(const NavGraph& nav, const Entity& slig)
{
    s32 best = -1;
    const std::vector<NavNode>& nodes = nav.Nodes();
    const s32 scale = slig.mHalf ? 2 : 1;
    for (std::size_t n = 0; n < nodes.size(); n++)
    {
        const NavNode& at = nodes[n];
        // (one a spawner makes comes down from where the spawner is to the floor under it; one the map puts
        // somewhere stands there, and with no floor there it is on none: the Slig in the box over the Mudanchee
        // trial's last room is not on the floor 60 under it)
        const s32 drop = slig.mSwitch > 0 ? 180 : 20;
        if (at.mHalf != slig.mHalf || std::abs(at.mX - slig.CentreX()) > NavGraph::GridStep(slig.mHalf) / 2 || at.mY < slig.mY2 - 8 || (at.mY - slig.mY2) * scale > drop)
        {
            continue;
        }
        if (best < 0 || at.mY < nodes[best].mY)
        {
            best = static_cast<s32>(n);
        }
    }
    return best;
}

bool Domain::AtSligLock(const NavGraph& nav, const Entity& lock, s32 node)
{
    // (SecurityDoor::IsPlayerNear: whoever is controlled, inside its box)
    const NavNode& at = nav.Nodes()[node];
    return at.mHalf == lock.mHalf && at.mX >= lock.mX1 + 4 && at.mX <= lock.mX2 - 4 && at.mY >= lock.mY1 && at.mY <= lock.mY2;
}

std::vector<u32> Domain::SligsMet(const NavGraph& nav, const World& world, s32 from, const std::vector<s32>& way, u32 self)
{
    const std::vector<NavNode>& nodes = nav.Nodes();
    const std::vector<NavEdge>& edges = nav.Edges();
    std::vector<char> on(nodes.size(), 0);
    std::vector<s32> todo;
    const auto take = [&](s32 n)
    {
        if (!on[n])
        {
            on[n] = 1;
            todo.push_back(n);
        }
    };
    take(from);
    for (s32 idx : way)
    {
        take(edges[idx].mTo);
    }
    while (!todo.empty())
    {
        const s32 at = todo.back();
        todo.pop_back();
        for (s32 k = 0; k < nodes[at].mEdgeCount; k++)
        {
            const NavEdge& e = edges[nodes[at].mFirstEdge + k];
            if (e.mKind == EdgeKind::eWalk && !nav.Condition(e, nullptr, nullptr))
            {
                take(e.mTo);
            }
        }
    }
    std::vector<u32> met;
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eSlig || e.mGone || e.mId == self || e.mSwitch > 0)
        {
            continue;
        }
        const s32 node = SligNode(nav, e);
        if (node >= 0 && on[node])
        {
            met.push_back(e.mId);
        }
    }
    return met;
}

bool Domain::ScrabReaches(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, bool charged, std::vector<Goap::Fact>* keep, std::vector<Goap::Fact>* shut, bool* canShut)
{
    if (canShut)
    {
        *canShut = true;
    }
    if (world.mInvisible || (charged && (world.mInvisCharge || world.mInvisComing)))
    {
        return false;
    }
    const s32 scale = half ? 2 : 1;
    bool reaches = false;
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eScrab || e.mGone || e.mHalf != half || world.mScrabsKilled.count(e.mId))
        {
            continue;
        }
        // (the floor it is on now: one lured off a ledge is on the floor below the one the map put it on)
        const ScrabAt* own = world.ScrabOf(e.mId);
        if (std::abs((own ? own->mY : e.mY2) - y) * scale > 30)
        {
            continue;
        }
        const auto parked = world.mScrabParked.find(e.mId);
        // One that waits before it strikes is run from, like a Slig that waits before it shoots (the Mudanchee
        // vault's path 3: "run right ahead of the Scrab and jump up"). Not one that was left shut in: where it
        // is there is nowhere to run to.
        if (e.mInputs[0] >= 30 && parked == world.mScrabParked.end())
        {
            continue;
        }
        // Where it is: where it was left, else where it is seen, else where the map puts it
        s32 sx = e.CentreX();
        bool possessed = false;
        if (parked != world.mScrabParked.end())
        {
            sx = parked->second;
        }
        else if (own)
        {
            sx = own->mX;
            possessed = own->mPossessed;
        }
        else
        {
            s32 best = 450 / scale;
            for (const ScrabAt& live : world.mScrabs)
            {
                if (live.mHalf == half && std::abs(live.mY - e.mY2) * scale <= 30 && std::abs(live.mX - e.CentreX()) < best)
                {
                    best = std::abs(live.mX - e.CentreX());
                    sx = live.mX;
                    possessed = live.mPossessed;
                }
            }
        }
        // (one left shut in has the whole of its stretch, however long)
        if (possessed || (std::abs(sx - x) * scale > 700 && parked == world.mScrabParked.end()))
        {
            continue;
        }
        // The gates between, and of the shut and the open ones the nearest to it
        const NavFeature* shutGate = nullptr;
        const NavFeature* openGate = nullptr;
        for (const NavFeature& f : nav.Input().mFeatures)
        {
            const s32 gx = (f.mX1 + f.mX2) / 2;
            if (f.mKind != FeatureKind::eBarrier || !f.mSolid || f.mHalf != half || std::abs(f.mY2 - y) * scale > 30 || gx <= std::min(sx, x) || gx >= std::max(sx, x))
            {
                continue;
            }
            const NavFeature*& which = world.Switch(f.mSwitch) != (f.mStartClosed ? 1 : 0) ? shutGate : openGate;
            if (!which || std::abs(gx - sx) < std::abs((which->mX1 + which->mX2) / 2 - sx))
            {
                which = &f;
            }
        }
        // (or a wall: its floor may run on behind one)
        if (WallBetween(nav, sx, x, y, half))
        {
            continue;
        }
        if (shutGate)
        {
            if (keep && shutGate->mSwitch > 1)
            {
                keep->push_back({Key::Switch(shutGate->mSwitch), shutGate->mStartClosed ? 0 : 1});
            }
            continue;
        }
        reaches = true;
        if (!keep && !shut && !canShut)
        {
            return true;
        }
        if (openGate && openGate->mSwitch > 1)
        {
            if (shut)
            {
                shut->push_back({Key::Switch(openGate->mSwitch), openGate->mStartClosed ? 0 : 1});
            }
        }
        else if (canShut)
        {
            *canShut = false;
        }
    }
    return reaches;
}

bool Domain::GateBetween(const NavGraph& nav, const World& world, s32 x1, s32 x2, s32 y, bool half)
{
    for (const NavFeature& f : nav.Input().mFeatures)
    {
        const s32 gx = (f.mX1 + f.mX2) / 2;
        if (f.mKind == FeatureKind::eBarrier && f.mSolid && f.mHalf == half && std::abs(f.mY2 - y) * (half ? 2 : 1) <= 30 && gx > std::min(x1, x2) && gx < std::max(x1, x2) && world.Switch(f.mSwitch) != (f.mStartClosed ? 1 : 0))
        {
            return true;
        }
    }
    return false;
}

s32 Domain::FleechFrames(const NavGraph& nav, const World& world, const NavEdge& e, s32 assume)
{
    const NavNode& from = nav.Nodes()[e.mFrom];
    const NavNode& to = nav.Nodes()[e.mTo];
    const s32 scale = from.mHalf ? 2 : 1;
    // (a barrier that is shut stops it as it stops him)
    s32 condSwitch = 0;
    s32 condValue = 0;
    if (e.mFeature >= 0 && nav.Input().mFeatures[e.mFeature].mKind == FeatureKind::eBarrier && nav.Condition(e, &condSwitch, &condValue) && condSwitch != assume && world.Switch(condSwitch) != condValue)
    {
        return -1;
    }
    switch (e.mKind)
    {
        case EdgeKind::eWalk:
        case EdgeKind::eRoll:
            // (Fleech.cpp: a grid cell every seven frames)
            return std::max(1, std::abs(to.mX - from.mX) * scale * 28 / 100);
        case EdgeKind::eFall:
        case EdgeKind::eHoistDown:
            // (off the end of the floor: one that falls more than 300 px dies)
            return (to.mY - from.mY) * scale > 300 ? -1 : 21;
        case EdgeKind::eHoistUp:
            // (measured in the Mudanchee vault's path 2: at the foot of one to on its top in 40 frames. It
            // reaches 110 px.)
            return (from.mY - to.mY) * scale <= 110 ? 40 : -1;
        default:
            return -1;
    }
}

s32 Domain::RunFrames(const NavGraph& nav, const NavEdge& e)
{
    const NavNode& from = nav.Nodes()[e.mFrom];
    const NavNode& to = nav.Nodes()[e.mTo];
    const s32 cells = std::abs(to.mX - from.mX) / std::max(1, NavGraph::GridStep(from.mHalf));
    switch (e.mKind)
    {
        case EdgeKind::eWalk:
        case EdgeKind::eRoll:
            return 4 * std::max(1, cells);
        case EdgeKind::eFall:
        case EdgeKind::eHoistDown:
            return 21;
        case EdgeKind::eHoistUp:
            return 40;
        case EdgeKind::eHop:
            return 16;
        case EdgeKind::eRunJump:
            return 9 + 4 * cells;
        case EdgeKind::eWell:
            return 40;
        default:
            return std::max(6, e.mCost);
    }
}

std::vector<s32> Domain::FleechTimes(const NavGraph& nav, const World& world, const std::vector<s32>& zone, s32 assume, bool away)
{
    std::vector<s32> times(nav.Nodes().size(), 100000);
    for (s32 n : zone)
    {
        times[n] = 0;
    }
    // (edges are kept by where they start: the few hundred there are get gone over until nothing changes)
    for (bool changed = true; changed;)
    {
        changed = false;
        for (const NavEdge& e : nav.Edges())
        {
            const s32 known = away ? e.mFrom : e.mTo;
            const s32 other = away ? e.mTo : e.mFrom;
            const s32 frames = times[known] < 100000 ? FleechFrames(nav, world, e, assume) : -1;
            if (frames >= 0 && times[known] + frames < times[other])
            {
                times[other] = times[known] + frames;
                changed = true;
            }
        }
    }
    return times;
}

std::vector<s32> Domain::FleechReach(const NavGraph& nav, s32 at)
{
    std::vector<s32> zone;
    const NavNode& a = nav.Nodes()[at];
    for (std::size_t i = 0; i < nav.Nodes().size(); i++)
    {
        const NavNode& n = nav.Nodes()[i];
        if (n.mHalf == a.mHalf && std::abs(n.mY - a.mY) * (a.mHalf ? 2 : 1) <= 20 && std::abs(n.mX - a.mX) <= 4 * NavGraph::GridStep(a.mHalf))
        {
            zone.push_back(static_cast<s32>(i));
        }
    }
    return zone;
}

Domain::FleechLure Domain::LureFleeches(const NavGraph& nav, const World& world, const std::vector<s32>& homes, s32 target, const NavGraph::EdgeCost& abe, const NavGraph::EdgeCost& after, s32 assume, s32 from)
{
    FleechLure best;
    if (homes.empty() || target < 0)
    {
        return best;
    }
    const std::vector<s32> toZone = FleechTimes(nav, world, FleechReach(nav, target), assume);
    const NavGraph::EdgeCost fleech = [&](const NavEdge& e) -> s32
    {
        return FleechFrames(nav, world, e, assume);
    };
    // The way out after: to the nearest cell that one of them at the thing would be a long time getting to
    const std::vector<s32> fromZone = FleechTimes(nav, world, FleechReach(nav, target), assume, true);
    std::vector<s32> away;
    nav.FindPath(target, [&](s32 n)
                 { return fromZone[n] >= 150; }, [&](const NavEdge& e) -> s32
                 { return after(e) < 0 ? -1 : RunFrames(nav, e); }, away);
    // From the foot of a hoist: with him over them they make for the hoist and come up it after him, where on a
    // floor he doubles back under they would turn about and be there first
    std::set<s32> feet;
    for (const NavEdge& e : nav.Edges())
    {
        if (e.mKind == EdgeKind::eHoistUp)
        {
            feet.insert(e.mFrom);
        }
    }
    s32 bestThere = 0;
    for (s32 w : feet)
    {
        // Somewhere they can follow him to that is not by the thing itself
        if (toZone[w] < 60 || toZone[w] >= 100000)
        {
            continue;
        }
        // The cells they come by, each from where it is
        std::set<s32> theirs(homes.begin(), homes.end());
        bool come = false;
        s32 soonest = 100000;
        for (s32 home : homes)
        {
            std::vector<s32> out;
            s32 frames = 0;
            if (nav.FindPath(home, w, fleech, out, &frames) && !out.empty())
            {
                come = true;
                soonest = std::min(soonest, frames);
                for (s32 edge : out)
                {
                    theirs.insert(nav.Edges()[edge].mFrom);
                }
            }
        }
        if (!come)
        {
            continue;
        }
        theirs.erase(w);
        // (and he has to be there well before they are: a hoist's time before)
        s32 there = 0;
        if (from >= 0 && from != w)
        {
            std::vector<s32> to;
            if (!nav.FindPath(from, w, [&](const NavEdge& e) -> s32
                              { return abe(e) < 0 ? -1 : RunFrames(nav, e); }, to, &there) || there + 45 > soonest)
            {
                continue;
            }
        }
        // (the way they come, gone the other way round to show himself, where there is one for him: to within 40
        // of their frames of the thing, some 150 px from where they would have him. Without one he waits for
        // them.)
        std::vector<s32> show;
        nav.FindPath(w, [&](s32 n)
                     { return theirs.count(n) != 0 && toZone[n] < 40; }, [&](const NavEdge& e) -> s32
                     { return abe(e) < 0 || (!theirs.count(e.mTo) && e.mTo != w) ? -1 : RunFrames(nav, e); }, show);
        // (not by the way they come, but for its end: by the thing is where they set out from)
        std::vector<s32> back;
        if (!nav.FindPath(w, target, [&](const NavEdge& e) -> s32
                          { return abe(e) < 0 || (theirs.count(e.mTo) && toZone[e.mTo] >= 40) ? -1 : RunFrames(nav, e); }, back) || back.empty() || nav.Edges()[back[0]].mKind != EdgeKind::eHoistUp)
        {
            continue;
        }
        // What they lose on him over it: they follow where they can, and from where they cannot they make for
        // the thing as best they may
        s32 his = 0;
        s32 their = 0;
        s32 commit = w;
        bool following = true;
        for (s32 edge : back)
        {
            const NavEdge& e = nav.Edges()[edge];
            his += RunFrames(nav, e);
            if (following)
            {
                const s32 frames = FleechFrames(nav, world, e, assume);
                if (frames < 0)
                {
                    following = false;
                    their += toZone[e.mFrom];
                }
                else
                {
                    their += frames;
                }
            }
            if (e.mKind != EdgeKind::eWalk && e.mKind != EdgeKind::eRoll)
            {
                commit = e.mFrom;
            }
        }
        const s32 lead = std::min(their, 100000) - his;
        // (of two as good the nearer)
        if (!best.mFound || lead > best.mLead || (lead == best.mLead && there < bestThere))
        {
            bestThere = there;
            best.mFound = true;
            best.mBait = w;
            best.mOut = show;
            best.mBack = back;
            best.mCommit = commit;
            best.mAway = away;
            best.mLead = lead;
        }
    }
    // (he sets out with them some 60 frames behind and needs 64 for a slap, the turn and a cell or two to the way
    // out: with less than this over the way back they are on him as he uses it)
    if (best.mFound && best.mLead < 15)
    {
        best.mFound = false;
    }
    return best;
}

Domain::FleechLead Domain::LeadFleeches(const NavGraph& nav, const World& world, s32 from, s32 assume)
{
    FleechLead best;
    if (from < 0)
    {
        return best;
    }
    // (what an edge costs him with the switch on)
    const NavGraph::EdgeCost abe = [&](const NavEdge& e) -> s32
    {
        s32 condSwitch = 0;
        s32 condValue = 0;
        if (nav.Condition(e, &condSwitch, &condValue) && (condSwitch == assume ? 1 : world.Switch(condSwitch)) != condValue)
        {
            return -1;
        }
        return std::max(1, e.mCost);
    };
    const NavGraph::EdgeCost follow = [&](const NavEdge& e) -> s32
    {
        return abe(e) < 0 ? -1 : FleechFrames(nav, world, e, assume);
    };
    // The ones it lets out: with a way to where he sets it once it is on, and none before
    const std::vector<s32> shut = FleechTimes(nav, world, {from});
    const std::vector<s32> open = FleechTimes(nav, world, {from}, assume);
    const bool half = nav.Nodes()[from].mHalf;
    std::vector<u32> group;
    for (const Entity& f : world.mEntities)
    {
        const s32 at = f.mKind == EntityKind::eFleech && !f.mGone && f.mInputs[1] == 0 && f.mHalf == half && world.mFleechKilled.count(f.mId) == 0 ? nav.NodeAt(f.CentreX(), f.mY2, f.mHalf, 30) : -1;
        if (at >= 0 && shut[at] >= 100000 && open[at] < 100000)
        {
            group.push_back(f.mId);
        }
    }
    if (group.empty())
    {
        return best;
    }
    s32 bestFrames = 0;
    for (std::size_t i = 0; i < nav.Edges().size(); i++)
    {
        const NavEdge& e = nav.Edges()[i];
        const NavNode& a = nav.Nodes()[e.mFrom];
        const NavNode& b = nav.Nodes()[e.mTo];
        const s32 scale = a.mHalf ? 2 : 1;
        if ((e.mKind != EdgeKind::eHop && e.mKind != EdgeKind::eRunJump) || a.mHalf != half || b.mHalf != half || std::abs(a.mY - b.mY) * scale > 10 || abe(e) < 0)
        {
            continue;
        }
        // Where one that cannot follow comes down: off the end of that floor, the way he went
        s32 land = -1;
        for (s32 k = a.mFirstEdge; k < a.mFirstEdge + a.mEdgeCount; k++)
        {
            const NavEdge& g = nav.Edges()[k];
            const NavNode& d = nav.Nodes()[g.mTo];
            if ((g.mKind == EdgeKind::eFall || g.mKind == EdgeKind::eHoistDown) && d.mY > a.mY + 20 / scale && (d.mX - a.mX) * (b.mX - a.mX) >= 0 && FleechFrames(nav, world, g, assume) >= 0 && (land < 0 || g.mKind == EdgeKind::eFall))
            {
                land = g.mTo;
            }
        }
        if (land < 0)
        {
            continue;
        }
        const NavNode& d = nav.Nodes()[land];
        // A Scrab whose floor that is, near enough for them to stay scared of it (Fleech.cpp: within six cells)
        const Entity* scrab = nullptr;
        for (const Entity& s : world.mEntities)
        {
            const s32 under = s.mKind == EntityKind::eScrab && !s.mGone && world.mScrabsKilled.count(s.mId) == 0 && s.mHalf == d.mHalf ? nav.NodeAt(s.CentreX(), s.mY2, s.mHalf, 30) : -1;
            if (under >= 0 && nav.Nodes()[under].mLine == d.mLine && std::abs(d.mX - s.CentreX()) * scale <= 150 && !WallBetween(nav, s.CentreX(), d.mX, d.mY, d.mHalf) && !GateBetween(nav, world, s.CentreX(), d.mX, d.mY, d.mHalf))
            {
                scrab = &s;
            }
        }
        if (!scrab)
        {
            continue;
        }
        std::vector<s32> way;
        s32 frames = 0;
        if (!nav.FindPath(from, e.mFrom, follow, way, &frames) || way.empty())
        {
            continue;
        }
        // Where he chants from: on the Scrab's screen, out of its reach, and not right over where they come down
        const s32 cam = nav.CameraOf(scrab->CentreX(), scrab->mY2 - 5);
        std::vector<s32> on;
        const auto spot = [&](s32 n) -> bool
        {
            const NavNode& at = nav.Nodes()[n];
            return !at.mLow && !at.mHazard && at.mHalf == half && at.mLine != d.mLine && nav.CameraOf(at.mX, at.mY - 5) == cam && std::abs(at.mX - a.mX) * scale > 75 && !ScrabReaches(nav, world, at.mX, at.mY, at.mHalf, false);
        };
        if (!nav.FindPath(e.mTo, spot, [&](const NavEdge& g) -> s32
                          { return abe(g) < 0 || g.mTo == e.mFrom || nav.Nodes()[g.mTo].mLine == d.mLine ? -1 : abe(g); }, on))
        {
            continue;
        }
        if (!best.mFound || frames < bestFrames)
        {
            bestFrames = frames;
            best.mFound = true;
            best.mScrab = scrab->mId;
            best.mWay = way;
            best.mDrop = e.mFrom;
            best.mOver = static_cast<s32>(i);
            best.mLand = land;
            best.mSpot = on.empty() ? e.mTo : nav.Edges()[on.back()].mTo;
            best.mGroup = group;
        }
    }
    return best;
}

s32 Domain::ScrabZone(const NavGraph& nav, s32 node)
{
    const NavNode& n = nav.Nodes()[node];
    s32 gates = 0;
    for (const NavFeature& f : nav.Input().mFeatures)
    {
        if (f.mKind == FeatureKind::eBarrier && f.mSolid && f.mHalf == n.mHalf && std::abs(f.mY2 - n.mY) * (n.mHalf ? 2 : 1) <= 30 && (f.mX1 + f.mX2) / 2 < n.mX)
        {
            gates++;
        }
    }
    return n.mLine * 16 + gates;
}

bool Domain::ParamitePack(const NavGraph& nav, const World& world, s32 x, s32 y, bool half)
{
    if (world.mParamites.empty())
    {
        return false;
    }
    const s32 at = nav.NodeAt(x, y, half, 12);
    if (at < 0)
    {
        return false;
    }
    s32 count = 0;
    for (const ParamiteAt& p : world.mParamites)
    {
        if (p.mPossessed || p.Hidden() || p.mHalf != half)
        {
            continue;
        }
        const s32 on = nav.NodeAt(p.mX, p.mY, p.mHalf, 30);
        count += on >= 0 && nav.Nodes()[on].mLine == nav.Nodes()[at].mLine && !WallBetween(nav, p.mX, x, y, half) && !GateBetween(nav, world, p.mX, x, y, half) ? 1 : 0;
    }
    return count >= 2;
}

bool Domain::ParamiteCornered(const NavGraph& nav, const World& world, s32 x, s32 y, bool half)
{
    // Paramite.cpp, Brain_Patrol_State_1_IdleForAbe: one on its own backs off from him while he is within six
    // cells of it, and turns on him ("HittingAbe") once the end of its floor is within two cells behind it. So a
    // place on its floor with less than those eight cells between it and the end on the Paramite's side is one
    // where it has nowhere left to go (measured: the Mudomo vault's path 12, up the hoist at 587,1100 with the
    // Paramite at 637 and the floor ending at 750: dead 36 frames later).
    const s32 at = nav.NodeAt(x, y, half, 12);
    if (at < 0 || nav.Nodes()[at].mLine < 0)
    {
        return false;
    }
    const NavLine& line = nav.Input().mLines[nav.Nodes()[at].mLine];
    const s32 lo = std::min(line.mX1, line.mX2);
    const s32 hi = std::max(line.mX1, line.mX2);
    const s32 cell = NavGraph::GridStep(half);
    for (const ParamiteAt& p : world.mParamites)
    {
        if (p.mPossessed || p.Hidden() || p.mHalf != half)
        {
            continue;
        }
        const s32 on = nav.NodeAt(p.mX, p.mY, p.mHalf, 30);
        if (on < 0 || nav.Nodes()[on].mLine != nav.Nodes()[at].mLine || WallBetween(nav, p.mX, x, y, half) || GateBetween(nav, world, p.mX, x, y, half))
        {
            continue;
        }
        const s32 room = p.mX >= x ? hi - x : x - lo;
        if (room < 8 * cell)
        {
            return true;
        }
    }
    return false;
}

bool Domain::ParamiteWall(const NavGraph& nav, s32 x1, s32 x2, s32 y, bool half)
{
    const s32 at = y - (half ? 10 : 20);
    for (const NavLine& line : nav.Input().mLines)
    {
        const bool is = half ? (line.mType == 5 || line.mType == 6) : (line.mType == 1 || line.mType == 2);
        if (is && line.mX1 == line.mX2 && line.mX1 > std::min(x1, x2) && line.mX1 < std::max(x1, x2) && std::min(line.mY1, line.mY2) <= at && std::max(line.mY1, line.mY2) >= at)
        {
            return true;
        }
    }
    return false;
}

s32 Domain::ParamiteFloor(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, s32 tolerance)
{
    const s32 n = nav.NodeAt(x, y, half, tolerance);
    const s32 cell = NavGraph::GridStep(half);
    if (n < 0 || std::abs(nav.Nodes()[n].mX - x) > cell / 2 + 1 || GateBetween(nav, world, nav.Nodes()[n].mX - cell / 2, nav.Nodes()[n].mX + cell / 2, nav.Nodes()[n].mY, half))
    {
        return -1;
    }
    if (nav.Nodes()[n].mLiftStop < 0)
    {
        return n;
    }
    for (const LiftAt& lift : world.mLifts)
    {
        if (nav.Nodes()[n].mX >= lift.mX1 && nav.Nodes()[n].mX <= lift.mX2 && std::abs(lift.mY - nav.Nodes()[n].mY) <= 6)
        {
            return n;
        }
    }
    return -1;
}

bool Domain::ParamiteRoute(const NavGraph& nav, const World& world, s32 from, s32 to, std::vector<ParaLeg>* legs, s32* cost)
{
    if (from < 0 || to < 0)
    {
        return false;
    }
    const bool half = nav.Nodes()[from].mHalf;
    const s32 cell = NavGraph::GridStep(half);
    const s32 scale = half ? 2 : 1;
    struct Way final
    {
        s32 mTo;
        s32 mCost;
        ParaLeg::Kind mKind;
        s32 mX;
    };
    // (Paramite.cpp looks for walls 20 over its feet, 10 at half its size: lower than a Scrab, so it goes under
    // blocks that stop one. The Mudomo vault's path 5, under the block at 1268 that hangs to 19 over the floor.)
    const auto wall = [&](s32 x1, s32 x2, s32 y)
    {
        const s32 at = y - 20 / scale;
        for (const NavLine& line : nav.Input().mLines)
        {
            const bool is = half ? (line.mType == 5 || line.mType == 6) : (line.mType == 1 || line.mType == 2);
            if (is && line.mX1 == line.mX2 && line.mX1 > std::min(x1, x2) && line.mX1 < std::max(x1, x2) && std::min(line.mY1, line.mY2) <= at && std::max(line.mY1, line.mY2) >= at)
            {
                return true;
            }
        }
        return false;
    };
    const auto standable = [&](s32 n)
    {
        if (n < 0 || nav.Nodes()[n].mHalf != half)
        {
            return false;
        }
        // (not in a gate that is shut)
        if (GateBetween(nav, world, nav.Nodes()[n].mX - cell / 2, nav.Nodes()[n].mX + cell / 2, nav.Nodes()[n].mY, half))
        {
            return false;
        }
        if (nav.Nodes()[n].mLiftStop < 0)
        {
            return true;
        }
        for (const LiftAt& lift : world.mLifts)
        {
            if (nav.Nodes()[n].mX >= lift.mX1 && nav.Nodes()[n].mX <= lift.mX2 && std::abs(lift.mY - nav.Nodes()[n].mY) <= 6)
            {
                return true;
            }
        }
        return false;
    };
    const auto rocky = [&](s32 n)
    {
        for (const RockAt& r : world.mFallers)
        {
            if (r.mHalf == half && std::abs(r.mX - nav.Nodes()[n].mX) <= 40 / scale)
            {
                return true;
            }
        }
        return false;
    };
    const auto ways = [&](s32 n, std::vector<Way>& out)
    {
        const NavNode& a = nav.Nodes()[n];
        for (s32 dir = -1; dir <= 1; dir += 2)
        {
            const s32 nx = a.mX + dir * cell;
            if (wall(a.mX, nx, a.mY) || GateBetween(nav, world, a.mX, nx, a.mY, half))
            {
                continue;
            }
            const s32 next = nav.NodeAt(nx, a.mY, half, 6);
            const bool on = standable(next) && std::abs(nav.Nodes()[next].mX - nx) <= cell / 2;
            if (on)
            {
                // (not to stand by where rocks fall, if it can be helped: one beside the gap they fall into is
                // under them. The sixth Mudomo vault: dead at 1378 by the rocks at 1393.)
                out.push_back({next, rocky(next) ? 40 : 1, ParaLeg::eWalk, 0});
            }
            // A gap within three cells: a hop to the fourth cell
            bool gap = !on;
            for (s32 k = 2; k <= 3 && !gap; k++)
            {
                const s32 gx = a.mX + dir * k * cell;
                const s32 g = nav.NodeAt(gx, a.mY, half, 6);
                gap = !(standable(g) && std::abs(nav.Nodes()[g].mX - gx) <= cell / 2);
            }
            const s32 hx = a.mX + dir * 4 * cell;
            const s32 over = nav.NodeAt(hx, a.mY, half, 8);
            if (gap && standable(over) && std::abs(nav.Nodes()[over].mX - hx) <= cell / 2 && !wall(a.mX, hx, a.mY) && !GateBetween(nav, world, a.mX, hx, a.mY, half))
            {
                out.push_back({over, rocky(over) ? 45 : 6, ParaLeg::eHop, 0});
            }
            if (on)
            {
                continue;
            }
            // No floor on: off the end to what is under it
            s32 fy = 0;
            s32 height = 0;
            if (nav.FloorBelow(nx, a.mY + 1, half, &fy, &height) >= 0 && height * scale < 170)
            {
                const s32 under = nav.NodeAt(nx, fy, half, 6);
                if (standable(under))
                {
                    out.push_back({under, 4 + height * scale / 25, ParaLeg::eFall, 0});
                }
            }
        }
        for (const NavLine& l : nav.Input().mLines)
        {
            if (l.mType != 8)
            {
                continue;
            }
            const s32 x = (l.mX1 + l.mX2) / 2;
            const s32 top = std::min(l.mY1, l.mY2);
            const s32 bottom = std::max(l.mY1, l.mY2);
            if (std::abs(x - a.mX) > cell)
            {
                continue;
            }
            // (at its top it steps off half a cell to a side, onto a floor within 20 of the top; at its foot onto
            // the floor within 50 under it)
            s32 over = nav.NodeAt(x - cell / 2, top, half, 20);
            over = standable(over) && std::abs(nav.Nodes()[over].mX - x) <= cell ? over : nav.NodeAt(x + cell / 2, top, half, 20);
            // (a web at the very end of the floor under it: looked for half a cell to either side as well)
            s32 fy = 1000000;
            for (s32 off : {0, -cell / 2, cell / 2})
            {
                s32 y = 0;
                if (nav.FloorBelow(x + off, bottom - 25, half, &y) >= 0 && y < fy)
                {
                    fy = y;
                }
            }
            s32 under = fy - bottom <= 50 / scale ? nav.NodeAt(x, fy, half, 13) : -1;
            if (!standable(over) || !standable(under) || std::abs(nav.Nodes()[over].mX - x) > cell || nav.Nodes()[over].mY >= nav.Nodes()[under].mY)
            {
                continue;
            }
            const s32 climb = 8 + (bottom - top) * scale / 25;
            if (n == under)
            {
                out.push_back({over, climb, ParaLeg::eWebUp, x});
            }
            if (n == over)
            {
                out.push_back({under, climb, ParaLeg::eWebDown, x});
            }
        }
    };
    const std::size_t count = nav.Nodes().size();
    std::vector<s32> best(count, 1000000);
    std::vector<s32> prev(count, -1);
    std::vector<Way> by(count, Way{-1, 0, ParaLeg::eWalk, 0});
    std::vector<char> done(count, 0);
    best[from] = 0;
    for (;;)
    {
        s32 at = -1;
        for (std::size_t i = 0; i < count; i++)
        {
            if (!done[i] && best[i] < 1000000 && (at < 0 || best[i] < best[at]))
            {
                at = static_cast<s32>(i);
            }
        }
        if (at < 0 || at == to)
        {
            break;
        }
        done[at] = 1;
        std::vector<Way> out;
        ways(at, out);
        for (const Way& w : out)
        {
            if (best[at] + w.mCost < best[w.mTo])
            {
                best[w.mTo] = best[at] + w.mCost;
                prev[w.mTo] = at;
                by[w.mTo] = w;
            }
        }
    }
    if (best[to] >= 1000000)
    {
        return false;
    }
    if (cost)
    {
        *cost = best[to];
    }
    if (legs)
    {
        std::vector<ParaLeg> back;
        for (s32 n = to; n != from; n = prev[n])
        {
            ParaLeg leg;
            leg.mKind = by[n].mKind;
            leg.mTo = n;
            leg.mX = by[n].mX;
            // (a walk that goes on is one walk)
            if (leg.mKind == ParaLeg::eWalk && !back.empty() && back.back().mKind == ParaLeg::eWalk)
            {
                continue;
            }
            back.push_back(leg);
        }
        legs->assign(back.rbegin(), back.rend());
    }
    return true;
}

std::vector<u32> Domain::ParamiteTour(const NavGraph& nav, const World& world, s32 from)
{
    std::vector<u32> tour;
    s32 at = from;
    for (;;)
    {
        const Entity* next = nullptr;
        s32 nextNode = -1;
        s32 nextCost = 0;
        for (const Entity& ring : world.mEntities)
        {
            if (ring.mKind != EntityKind::ePullRing || ring.mGone || ring.mSwitch <= 1 || world.Switch(ring.mSwitch) != 0 || std::find(tour.begin(), tour.end(), ring.mId) != tour.end())
            {
                continue;
            }
            const s32 under = UseNode(nav, ring, nullptr);
            s32 cost = 0;
            if (under >= 0 && at >= 0 && nav.Nodes()[under].mHalf == nav.Nodes()[at].mHalf && ParamiteRoute(nav, world, at, under, nullptr, &cost) && (!next || cost < nextCost))
            {
                next = &ring;
                nextNode = under;
                nextCost = cost;
            }
        }
        if (!next)
        {
            break;
        }
        tour.push_back(next->mId);
        at = nextNode;
    }
    return tour;
}

bool Domain::PackLevel(const NavGraph& nav, const World& world, s32 node)
{
    const NavNode& n = nav.Nodes()[node];
    const s32 cam = nav.CameraOf(n.mX, n.mY - 5);
    s32 count = 0;
    s32 level = 0;
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eParamite || e.mGone || e.mHalf != n.mHalf || nav.CameraOf(e.CentreX(), e.mY2 - 5) != cam)
        {
            continue;
        }
        count++;
        level += std::abs(e.mY2 - n.mY) * (n.mHalf ? 2 : 1) <= 30 ? 1 : 0;
    }
    return count >= 2 && level >= 1;
}

bool Domain::FleechPen(const NavGraph& nav, const World& world, const Entity& lever, s32* gate, s32* open, s32* shut, std::vector<u32>* group)
{
    std::vector<u32> woken;
    for (const Entity& f : world.mEntities)
    {
        if (f.mKind == EntityKind::eFleech && !f.mGone && world.mFleechKilled.count(f.mId) == 0 && lever.mSwitch > 1 && f.mSwitch == lever.mSwitch && f.mHalf == lever.mHalf)
        {
            woken.push_back(f.mId);
        }
    }
    if (woken.empty())
    {
        return false;
    }
    for (std::size_t fi = 0; fi < nav.Input().mFeatures.size(); fi++)
    {
        const NavFeature& g = nav.Input().mFeatures[fi];
        if (g.mKind != FeatureKind::eBarrier || !g.mSolid || g.mSwitch <= 1 || g.mSwitch == lever.mSwitch || !g.mStartClosed || g.mHalf != lever.mHalf)
        {
            continue;
        }
        const s32 gx = (g.mX1 + g.mX2) / 2;
        const Entity* on = nullptr;
        const Entity* off = nullptr;
        for (const Entity& e : world.mEntities)
        {
            if (e.mKind == EntityKind::eFootSwitch && e.mSwitch == g.mSwitch && e.mHalf == g.mHalf && std::abs(e.mY2 - g.mY2) <= 30)
            {
                on = e.mOp == 0 ? &e : on;
                off = e.mOp == 1 ? &e : off;
            }
        }
        const bool leverLeft = lever.CentreX() < gx;
        if (!on || !off || (on->CentreX() < gx) == leverLeft || (off->CentreX() < gx) != leverLeft)
        {
            continue;
        }
        const s32 a = nav.NodeAt(on->CentreX(), on->mY2, g.mHalf, 30);
        const s32 b = nav.NodeAt(off->CentreX(), off->mY2, g.mHalf, 30);
        if (a < 0 || b < 0)
        {
            continue;
        }
        *gate = static_cast<s32>(fi);
        *open = a;
        *shut = b;
        *group = woken;
        return true;
    }
    return false;
}

bool Domain::WebUp(const NavGraph& nav, s32 from, s32 to, s32* webX)
{
    // Paramite.cpp: one that is played grabs a web (a track line, type 8) within a cell ahead of it as it comes
    // down from a jump, climbs it while up is held, and at its top steps off onto a floor half a cell to a side
    // that lies within 20 of the top.
    const NavNode& a = nav.Nodes()[from];
    const NavNode& b = nav.Nodes()[to];
    if (a.mHalf != b.mHalf || b.mY >= a.mY)
    {
        return false;
    }
    const s32 cell = NavGraph::GridStep(a.mHalf);
    for (const NavLine& l : nav.Input().mLines)
    {
        const s32 x = (l.mX1 + l.mX2) / 2;
        const s32 top = std::min(l.mY1, l.mY2);
        const s32 bottom = std::max(l.mY1, l.mY2);
        if (l.mType != 8 || bottom < a.mY - 20 || top > a.mY - 20 || std::abs(b.mY - top) > 20)
        {
            continue;
        }
        // (the floor at its foot, and the one it steps off onto: half a cell to either side of its top)
        const s32 under = nav.NodeAt(x, a.mY, a.mHalf, 12);
        s32 over = nav.NodeAt(x + cell / 2, b.mY, b.mHalf, 12);
        over = over >= 0 && nav.Nodes()[over].mLine == b.mLine ? over : nav.NodeAt(x - cell / 2, b.mY, b.mHalf, 12);
        // (floor all the way to its foot: another floor's line at the same height counts, and so does a lift's
        // platform between the two)
        bool floor = under >= 0;
        for (s32 fx = std::min(a.mX, x); floor && fx <= std::max(a.mX, x); fx += cell)
        {
            floor = nav.NodeAt(fx, a.mY, a.mHalf, 6) >= 0;
        }
        if (!floor || over < 0 || nav.Nodes()[over].mLine != b.mLine || WallBetween(nav, a.mX, x, a.mY, a.mHalf) || WallBetween(nav, nav.Nodes()[over].mX, b.mX, b.mY, b.mHalf))
        {
            continue;
        }
        if (webX)
        {
            *webX = x;
        }
        return true;
    }
    return false;
}

bool Domain::WallBetween(const NavGraph& nav, s32 x1, s32 x2, s32 y, bool half)
{
    const s32 scale = half ? 2 : 1;
    for (const NavLine& line : nav.Input().mLines)
    {
        const bool wall = half ? (line.mType == 5 || line.mType == 6) : (line.mType == 1 || line.mType == 2);
        if (!wall || line.mX1 != line.mX2 || line.mX1 <= std::min(x1, x2) || line.mX1 >= std::max(x1, x2))
        {
            continue;
        }
        const s32 top = std::min(line.mY1, line.mY2);
        const s32 bottom = std::max(line.mY1, line.mY2);
        // (one that stands on the floor, or the side of a block that hangs lower than a Scrab is tall: Scrab.cpp
        // looks for walls 45 px over its feet. Abe rolls under such a block and the Scrab after him is stopped.)
        if ((top <= y - 20 / scale && bottom >= y - 5) || (top <= y - 45 / scale && bottom >= y - 45 / scale))
        {
            return true;
        }
    }
    return false;
}

std::vector<Domain::LooseScrab> Domain::LooseScrabs(const World& world, bool waiting)
{
    std::vector<LooseScrab> out;
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eScrab || e.mGone || world.mScrabsKilled.count(e.mId) || world.mScrabParked.count(e.mId) || (e.mInputs[0] >= 30) != waiting)
        {
            continue;
        }
        const ScrabAt* own = world.ScrabOf(e.mId);
        if (own && own->mPossessed)
        {
            continue;
        }
        LooseScrab scrab;
        scrab.mId = e.mId;
        scrab.mX = own ? own->mX : e.CentreX();
        scrab.mY = own ? own->mY : e.mY2;
        scrab.mHalf = e.mHalf;
        scrab.mDelay = e.mInputs[0];
        out.push_back(scrab);
    }
    return out;
}

s32 Domain::LureLanding(const NavGraph& nav, s32 top)
{
    const NavNode& t = nav.Nodes()[top];
    const s32 step = NavGraph::GridStep(t.mHalf);
    const auto onLine = [&](s32 x)
    {
        const s32 at = nav.NodeAt(x, t.mY, t.mHalf, 5);
        return at >= 0 && nav.Nodes()[at].mLine == t.mLine;
    };
    const bool left = onLine(t.mX - step);
    const bool right = onLine(t.mX + step);
    if (left == right)
    {
        return -1;
    }
    const s32 dir = left ? 1 : -1;
    // Floor within three cells at its height: it jumps (Scrab::Brain_ChasingEnemy_State_2_Running)
    for (s32 k = 1; k <= 3; k++)
    {
        if (nav.NodeAt(t.mX + dir * k * step, t.mY, t.mHalf, 10) >= 0)
        {
            return -1;
        }
    }
    // A wall there knocks it back
    for (const NavLine& line : nav.Input().mLines)
    {
        const bool wall = t.mHalf ? (line.mType == 5 || line.mType == 6) : (line.mType == 1 || line.mType == 2);
        if (wall && line.mX1 == line.mX2 && (line.mX1 - t.mX) * dir > 0 && (line.mX1 - t.mX) * dir <= 2 * step && std::min(line.mY1, line.mY2) <= t.mY - 5 && std::max(line.mY1, line.mY2) >= t.mY - 20 / (t.mHalf ? 2 : 1))
        {
            return -1;
        }
    }
    // What is under where it leaves the floor
    s32 best = -1;
    for (std::size_t i = 0; i < nav.Nodes().size(); i++)
    {
        const NavNode& n = nav.Nodes()[i];
        // (the nearest floor below, and of its cells the one under where it goes over)
        const s32 over = t.mX + dir * step;
        if (n.mHalf == t.mHalf && std::abs(n.mX - over) <= step && n.mY > t.mY + 20 && (best < 0 || n.mY < nav.Nodes()[best].mY || (n.mY == nav.Nodes()[best].mY && std::abs(n.mX - over) < std::abs(nav.Nodes()[best].mX - over))))
        {
            best = static_cast<s32>(i);
        }
    }
    return best;
}

std::vector<s32> Domain::PulledFirst(const NavGraph& nav, const World& world, const Entity& lever)
{
    // Levers that open a door between them, where pulling them wakes Fleeches (each its own, or the door's
    // switch all of them): the one the door is quickest to get to from is pulled last, so that he is through it
    // before a Fleech has crawled to him. The switches the others set come first.
    std::vector<s32> first;
    const auto wakes = [&](s32 sw)
    {
        for (const Entity& e : world.mEntities)
        {
            if (e.mKind == EntityKind::eFleech && !e.mGone && sw > 1 && e.mSwitch == sw)
            {
                return true;
            }
        }
        return false;
    };
    for (const Entity& logic : world.mEntities)
    {
        if (logic.mKind != EntityKind::eSwitchLogic || std::find(std::begin(logic.mInputs), std::end(logic.mInputs), lever.mSwitch) == std::end(logic.mInputs))
        {
            continue;
        }
        bool fleeches = wakes(logic.mSwitch);
        for (s32 input : logic.mInputs)
        {
            fleeches = fleeches || wakes(input);
        }
        s32 door = -1;
        for (const NavFeature& f : nav.Input().mFeatures)
        {
            if (f.mKind == FeatureKind::eDoor && f.mSwitch == logic.mSwitch && door < 0)
            {
                door = nav.NodeAt((f.mX1 + f.mX2) / 2, f.mY2, f.mHalf, 30);
            }
        }
        if (!fleeches || door < 0)
        {
            continue;
        }
        const NavGraph::EdgeCost any = [](const NavEdge& e)
        {
            return e.mCost;
        };
        // (the last is one of those that wake a Fleech, if any does: the others are done before those)
        bool anyWakes = false;
        for (s32 input : logic.mInputs)
        {
            anyWakes = anyWakes || wakes(input);
        }
        s32 best = 0;
        s32 bestCost = -1;
        // (how far each is from the door, for the order of the rest: the furthest first, a sweep towards the door)
        std::vector<std::pair<s32, s32>> costs;
        for (const Entity& other : world.mEntities)
        {
            if ((other.mKind != EntityKind::eLever && other.mKind != EntityKind::ePullRing) || std::find(std::begin(logic.mInputs), std::end(logic.mInputs), other.mSwitch) == std::end(logic.mInputs) || other.mSwitch <= 1 || (anyWakes && !wakes(other.mSwitch)))
            {
                continue;
            }
            const s32 at = UseNode(nav, other, nullptr);
            std::vector<s32> way;
            s32 cost = 0;
            bool found = at >= 0 && nav.FindPath(at, door, any, way, &cost);
            // (in frames as he really takes them with a Fleech after him: a drop off a ledge is quicker than
            // three cells of floor, which the planner's costs have the other way round)
            cost = 0;
            for (s32 edge : way)
            {
                const NavEdge& e = nav.Edges()[edge];
                const s32 drop = std::abs(nav.Nodes()[e.mTo].mY - nav.Nodes()[e.mFrom].mY);
                cost += e.mKind == EdgeKind::eWalk ? 9 : (e.mKind == EdgeKind::eFall ? 12 + drop / 10 : (e.mKind == EdgeKind::eHop ? 23 : e.mCost));
            }
            if (found)
            {
                costs.push_back({other.mSwitch, cost});
            }
            if (found && (bestCost < 0 || cost < bestCost))
            {
                bestCost = cost;
                best = other.mSwitch;
            }
        }
        // And what wakes none (the walkthrough: "pull the rings first, because the levers wake up the Fleeches")
        // before what wakes one
        if (wakes(lever.mSwitch))
        {
            for (s32 input : logic.mInputs)
            {
                if (input > 1 && input != lever.mSwitch && !wakes(input) && std::find(first.begin(), first.end(), input) == first.end())
                {
                    first.push_back(input);
                }
            }
        }
        s32 own = -1;
        for (const std::pair<s32, s32>& c : costs)
        {
            own = c.first == lever.mSwitch ? c.second : own;
        }
        for (const std::pair<s32, s32>& c : costs)
        {
            if (own >= 0 && c.first != lever.mSwitch && c.second > own && std::find(first.begin(), first.end(), c.first) == first.end())
            {
                first.push_back(c.first);
            }
        }
        if (best == lever.mSwitch)
        {
            for (s32 input : logic.mInputs)
            {
                if (input > 1 && input != lever.mSwitch && std::find(first.begin(), first.end(), input) == first.end())
                {
                    first.push_back(input);
                }
            }
        }
    }
    return first;
}

bool Domain::PlanTogether(const NavGraph& nav, const World& world, const Entity& logic, Team& team, std::vector<Goap::Fact>& pre)
{
    team = {};
    pre.clear();
    if (logic.mKind != EntityKind::eSwitchLogic)
    {
        return false;
    }
    std::vector<const Entity*> wheels;
    for (s32 input : logic.mInputs)
    {
        if (input <= 1)
        {
            continue;
        }
        const Entity* wheel = nullptr;
        for (const Entity& e : world.mEntities)
        {
            if (e.mKind == EntityKind::eWorkWheel && e.mSwitch == input)
            {
                wheel = &e;
            }
        }
        if (!wheel || UseNode(nav, *wheel, nullptr) < 0)
        {
            return false;
        }
        bool have = false;
        for (const Entity* w : wheels)
        {
            have = have || w == wheel;
        }
        if (!have)
        {
            wheels.push_back(wheel);
        }
    }
    if (wheels.size() < 2)
    {
        return false;
    }

    // The wheels a Mudokon stands at already are its own: it is in the team as it stands
    std::vector<const Entity*> manned;
    std::vector<const Entity*> free;
    for (const Entity* w : wheels)
    {
        const Entity* at = nullptr;
        for (const Entity& e : world.mEntities)
        {
            const s32 x = e.mLive ? e.mLiveX : e.CentreX();
            const s32 y = e.mLive ? e.mLiveY : e.mY2;
            const bool taken = std::find(manned.begin(), manned.end(), &e) != manned.end();
            if (!at && !taken && e.mKind == EntityKind::eMudokon && !e.mGone && !e.mBlind && x >= w->mX1 && x <= w->mX2 && std::abs(y - w->mY2) < 30)
            {
                at = &e;
            }
        }
        if (at)
        {
            manned.push_back(at);
        }
        else
        {
            free.push_back(w);
        }
        team.mWheels.push_back(w->mId);
    }

    // The rest but one are for Mudokons brought along: they walk, drop off ledges and ride a lift Abe works,
    // nothing more
    const NavGraph::EdgeCost mudCost = [](const NavEdge& e)
    {
        return e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eFall || e.mKind == EdgeKind::eLift ? e.mCost : -1;
    };
    struct Candidate final
    {
        const Entity* mMud;
        s32 mCost;
        std::vector<s32> mPath;
    };
    std::vector<Candidate> candidates;
    const s32 firstWheel = UseNode(nav, free.empty() ? *wheels[0] : *free[0], nullptr);
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eMudokon || e.mGone || free.size() < 2 || std::find(manned.begin(), manned.end(), &e) != manned.end())
        {
            continue;
        }
        const s32 node = UseNode(nav, e, nullptr);
        Candidate c{&e, 0, {}};
        if (node >= 0 && nav.FindPath(node, firstWheel, mudCost, c.mPath, &c.mCost))
        {
            candidates.push_back(std::move(c));
        }
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
                     { return a.mCost < b.mCost; });
    if (candidates.size() + 1 < free.size())
    {
        return false;
    }
    candidates.resize(free.empty() ? 0 : free.size() - 1);

    // Where the ones brought come onto the wheels' floor: where they stand, or where the last drop or lift of
    // their way lets them off
    const Entity* abeWheel = free.empty() ? wheels[0] : free[0];
    team.mFromX = abeWheel->CentreX();
    if (!candidates.empty())
    {
        team.mFromX = candidates[0].mMud->CentreX();
        for (s32 edge : candidates[0].mPath)
        {
            const NavEdge& e = nav.Edges()[edge];
            team.mFromX = e.mKind == EdgeKind::eWalk ? team.mFromX : nav.Nodes()[e.mTo].mX;
        }
    }
    // Abe's wheel: the one furthest from there, so they pass the others first
    for (const Entity* w : free)
    {
        if (std::abs(w->CentreX() - team.mFromX) > std::abs(abeWheel->CentreX() - team.mFromX))
        {
            abeWheel = w;
        }
        team.mFree.push_back(w->mId);
    }
    for (const Entity* mud : manned)
    {
        team.mMuds.push_back(mud->mId);
        pre.push_back({Key::Mud(mud->mId), 0});
    }
    team.mAbeWheel = abeWheel->mId;
    for (const Candidate& c : candidates)
    {
        team.mMuds.push_back(c.mMud->mId);
        pre.push_back({Key::Mud(c.mMud->mId), 0});
        for (s32 edge : c.mPath)
        {
            const NavEdge& e = nav.Edges()[edge];
            s32 condSwitch = 0;
            s32 condValue = 0;
            if (nav.Condition(e, &condSwitch, &condValue))
            {
                const Goap::Fact fact{Key::Switch(condSwitch), condValue};
                bool have = false;
                for (const Goap::Fact& p : pre)
                {
                    have = have || p == fact;
                }
                if (!have)
                {
                    pre.push_back(fact);
                }
            }
        }
    }
    return true;
}

// The frames of the power to go unseen that a lock guarded by Fleeches is slapped and left with
static const s32 kUnseenUse = 150;

// What a throw through a running drill is taken to cost over one with nothing in the way: more than going round
// to its lever
static const s32 kPastDrillCost = 2500;
// How far a fart is flown at most (it has 900 frames at up to 4 pixels each), and how near a Slig it can get to
// nothing is crossed to on that Slig's floor until it is dead
static const s32 kFartReach = 2600;
static const s32 kFartSligZone = 300;
// The games' line type of a floor a mine car runs on
static const s32 kMineCarFloor = 11;

Domain Domain::Build(const NavGraph& nav, const World& world, s32 abeNode, const std::function<bool(s32 edge)>& edgeUsable, bool kills)
{
    Domain d;
    s32 regionCount = 0;
    d.mRegions = nav.Regions(&regionCount);

    // Switches that go on and off with another (a timer trigger's outputs): whatever sets the one sets them
    std::vector<std::pair<s32, s32>> follows;
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind == EntityKind::eSwitchLogic && e.mHoldOnly && e.mInputs[0] > 1 && e.mSwitch > 1)
        {
            follows.push_back({e.mInputs[0], e.mSwitch});
        }
    }
    const auto add = [&d, &follows](std::string name, std::vector<Goap::Fact> pre, std::vector<Goap::Fact> effects, s32 cost, const DomainStep& step)
    {
        for (std::size_t i = 0; i < effects.size() && !follows.empty(); i++)
        {
            for (const std::pair<s32, s32>& f : follows)
            {
                const Goap::Fact with{Key::Switch(f.second), effects[i].mValue};
                if (effects[i].mKey == Key::Switch(f.first) && std::find(effects.begin(), effects.end(), with) == effects.end())
                {
                    effects.push_back(with);
                }
            }
        }
        Goap::Action a;
        a.mName = std::move(name);
        a.mPre = std::move(pre);
        a.mEffects = std::move(effects);
        a.mCost = cost;
        a.mTag = static_cast<s32>(d.mSteps.size());
        d.mSteps.push_back(step);
        d.mActions.push_back(std::move(a));
    };
    const auto regionFact = [&](s32 node)
    {
        return Goap::Fact{Key::kRegion, d.mRegions[node] + 1};
    };

    for (s32 i = 0; i < 256; i++)
    {
        if (world.mSwitches[i] || world.mGatesGone.count(static_cast<s32>(i)) != 0 ? world.Switch(static_cast<s32>(i), world.mGatesGone.count(static_cast<s32>(i)) == 0) != 0 : false)
        {
            d.mStart.Set(Key::Switch(i), 1);
        }
    }
    if (abeNode >= 0)
    {
        d.mStart.Set(Key::kRegion, d.mRegions[abeNode] + 1);
    }
    d.mStart.Set(world.CarriesMeat() ? Key::kMeat : Key::kRocks, world.mRocks);
    d.mStart.Set(Key::kInvis, world.mInvisEver ? 1 : 0);
    d.mStart.Set(Key::kInvisNow, world.mInvisCharge || world.mInvisible || world.mInvisComing ? 1 : 0);

    // From region to region
    std::set<std::tuple<s32, s32, s32>> seen;
    const std::vector<NavEdge>& edges = nav.Edges();
    for (std::size_t i = 0; i < edges.size(); i++)
    {
        const NavEdge& e = edges[i];
        const s32 rf = d.mRegions[e.mFrom];
        const s32 rt = d.mRegions[e.mTo];
        if (rf == rt || (edgeUsable && !edgeUsable(static_cast<s32>(i))))
        {
            continue;
        }
        s32 condSwitch = 0;
        s32 condValue = 0;
        const bool barrier = nav.Condition(e, &condSwitch, &condValue);
        if (!seen.insert(std::make_tuple(rf, rt, barrier ? e.mFeature : -1)).second)
        {
            continue;
        }
        std::vector<Goap::Fact> pre{regionFact(e.mFrom)};
        if (barrier)
        {
            pre.push_back({Key::Switch(condSwitch), condValue});
        }
        // A teleporter whose switch is made of others (the Mudanchee trial's last: on when three locks are slapped
        // and its plate is stood on): those that nothing in it sets have to be on first
        if (e.mKind == EdgeKind::eTeleport && e.mFeature >= 0)
        {
            const NavFeature& tele = nav.Input().mFeatures[e.mFeature];
            for (const Entity& logic : world.mEntities)
            {
                if (logic.mKind != EntityKind::eSwitchLogic || logic.mSwitch != tele.mSwitch || tele.mSwitch <= 1)
                {
                    continue;
                }
                for (s32 in : logic.mInputs)
                {
                    bool own = in <= 1;
                    for (const Entity& plate : world.mEntities)
                    {
                        own = own || (plate.mKind == EntityKind::eFootSwitch && plate.mSwitch == in && plate.CentreX() >= tele.mX1 && plate.CentreX() <= tele.mX2 && plate.mY2 >= tele.mY1 && plate.mY2 <= tele.mY2 + 12);
                    }
                    if (!own && std::find(pre.begin(), pre.end(), Goap::Fact{Key::Switch(in), 1}) == pre.end())
                    {
                        pre.push_back({Key::Switch(in), 1});
                    }
                }
            }
        }
        // Onto a floor a loose Scrab can get at him on (off one is fine): only unseen, or with a gate shut
        // between them. And onto one that only a shut gate keeps a Scrab from: with that gate still shut, or unseen.
        std::vector<Goap::Fact> other;
        bool both = false;
        {
            const NavNode& from = nav.Nodes()[e.mFrom];
            const NavNode& to = nav.Nodes()[e.mTo];
            std::vector<Goap::Fact> keep;
            std::vector<Goap::Fact> shut;
            bool canShut = false;
            const bool toNow = ScrabReaches(nav, world, to.mX, to.mY, to.mHalf, false, &keep, &shut, &canShut);
            const bool fromNow = ScrabReaches(nav, world, from.mX, from.mY, from.mHalf, false);
            const auto fits = [&](const std::vector<Goap::Fact>& facts)
            {
                bool ok = !facts.empty();
                for (const Goap::Fact& f : facts)
                {
                    ok = ok && !(barrier && f.mKey == Key::Switch(condSwitch) && f.mValue != condValue);
                }
                return ok;
            };
            if (toNow && !fromNow)
            {
                other = pre;
                pre.push_back({Key::kInvisNow, 1});
                keep.insert(keep.end(), shut.begin(), shut.end());
                both = canShut && fits(keep);
                other.insert(other.end(), keep.begin(), keep.end());
            }
            else if (!toNow && !fromNow && !keep.empty())
            {
                other = pre;
                other.push_back({Key::kInvisNow, 1});
                if (fits(keep))
                {
                    both = true;
                    pre.insert(pre.end(), keep.begin(), keep.end());
                }
                else
                {
                    pre = other;
                }
            }
        }
        DomainStep step;
        step.mKind = StepKind::eCross;
        step.mEdge = static_cast<s32>(i);
        // (unseen is the dearer of the two: the power is not spent where a gate does as well)
        const auto unseen = [](const std::vector<Goap::Fact>& facts)
        {
            return std::find(facts.begin(), facts.end(), Goap::Fact{Key::kInvisNow, 1}) != facts.end() ? 300 : 0;
        };
        // (the one that does without the power first: of two ways as good the planner takes the first it finds)
        if (both && unseen(other) != 0)
        {
            std::swap(pre, other);
        }
        // From or onto a floor Sligs watch with no shadow to go by (SligWatched): unseen, or with those Sligs dead
        bool sligFloor = false;
        {
            const NavNode& from = nav.Nodes()[e.mFrom];
            const NavNode& to = nav.Nodes()[e.mTo];
            std::vector<u32> watchers;
            bool anon = false;
            const bool wf = SligWatched(nav, world, from.mX, from.mY, from.mHalf, &watchers, &anon);
            const bool wt = SligWatched(nav, world, to.mX, to.mY, to.mHalf, &watchers, &anon);
            if ((wf || wt) && unseen(pre) == 0 && !both)
            {
                sligFloor = true;
                if (!anon && !watchers.empty())
                {
                    std::vector<Goap::Fact> dead = pre;
                    for (u32 id : watchers)
                    {
                        dead.push_back({Key::Mud(id), 2});
                    }
                    add("Cross " + std::string(EdgeKindName(e.mKind)) + " " + std::to_string(from.mX) + "," + std::to_string(from.mY) + "->" + std::to_string(to.mX) + "," + std::to_string(to.mY), dead, {regionFact(e.mTo)}, 100 + e.mCost, step);
                }
                pre.push_back({Key::kInvisNow, 1});
            }
        }
        if (both)
        {
            add("Cross " + std::string(EdgeKindName(e.mKind)) + " " + std::to_string(nav.Nodes()[e.mFrom].mX) + "," + std::to_string(nav.Nodes()[e.mFrom].mY) + "->" + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY),
                other, {regionFact(e.mTo)}, 100 + e.mCost + unseen(other), step);
        }
        // (onto the floor a Slig walks, from off it: dearer, so that a way round it is taken where there is one)
        const s32 beatTo = sligFloor ? -1 : SligBeat(nav, world, e.mTo);
        const s32 beat = beatTo >= 0 && beatTo != SligBeat(nav, world, e.mFrom) ? kSligBeatCost : 0;
        add("Cross " + std::string(EdgeKindName(e.mKind)) + " " + std::to_string(nav.Nodes()[e.mFrom].mX) + "," + std::to_string(nav.Nodes()[e.mFrom].mY) + "->" + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY),
            pre, {regionFact(e.mTo)}, 100 + e.mCost + (both ? unseen(pre) : 0) + beat, step);
        // Or seen, onto the Scrab's floor and straight off it: to a cell of that floor a few along which the
        // Scrab can't get to (behind a wall, or under a block too low for it), timed against the Scrab
        // (`ScrabWaryTask`). The Mudanchee trial: "once you land, run and roll to the left".
        if (unseen(pre) != 0 && !both && !sligFloor)
        {
            const NavNode& landing = nav.Nodes()[e.mTo];
            const s32 cell = NavGraph::GridStep(landing.mHalf);
            s32 through = -1;
            const auto safe = [&](s32 n)
            {
                return !nav.Nodes()[n].mHazard && !ScrabReaches(nav, world, nav.Nodes()[n].mX, nav.Nodes()[n].mY, nav.Nodes()[n].mHalf, false);
            };
            // (along its floor, by a teleporter of that floor too, and through a gate a plate before it opens)
            {
                std::vector<s32> steps(nav.Nodes().size(), -1);
                std::vector<s32> queue{e.mTo};
                steps[e.mTo] = 0;
                for (std::size_t q = 0; q < queue.size() && through < 0; q++)
                {
                    const s32 n = queue[q];
                    const NavNode& at = nav.Nodes()[n];
                    if (n != e.mTo && steps[n] <= 30 && safe(n))
                    {
                        through = n;
                        // (past the plate that shuts the gate behind him, if there is one just beyond it)
                        for (const NavFeature& plate : nav.Input().mFeatures)
                        {
                            const s32 px = (plate.mX1 + plate.mX2) / 2;
                            // (one that works a gate he has come through)
                            bool gateBefore = false;
                            for (const NavFeature& gate : nav.Input().mFeatures)
                            {
                                const s32 gx = (gate.mX1 + gate.mX2) / 2;
                                gateBefore = gateBefore || (gate.mKind == FeatureKind::eBarrier && gate.mSwitch == plate.mSwitch && gx > std::min(landing.mX, px) && gx < std::max(landing.mX, px));
                            }
                            const s32 past = gateBefore && plate.mKind == FeatureKind::ePlate && plate.mHalf == at.mHalf && std::abs(plate.mY2 - at.mY) <= 30 && std::abs(px - at.mX) <= 5 * cell ?
                                                 nav.NodeAt(px + (px >= at.mX ? cell : -cell), at.mY, at.mHalf, 5) :
                                                 -1;
                            if (past >= 0 && nav.Nodes()[past].mLine == at.mLine && safe(past))
                            {
                                through = past;
                            }
                        }
                        break;
                    }
                    if (steps[n] >= 30)
                    {
                        continue;
                    }
                    for (s32 j = at.mFirstEdge; j < at.mFirstEdge + at.mEdgeCount && through < 0; j++)
                    {
                        const NavEdge& on = edges[j];
                        if (nav.Condition(on, nullptr, nullptr))
                        {
                            continue;
                        }
                        // (off it there by a well or up a ledge)
                        if ((on.mKind == EdgeKind::eWell || on.mKind == EdgeKind::eHoistUp) && nav.Nodes()[on.mTo].mLine != landing.mLine && steps[n] <= 12 && safe(on.mTo))
                        {
                            through = on.mTo;
                        }
                        else if ((on.mKind == EdgeKind::eWalk || on.mKind == EdgeKind::eRoll || on.mKind == EdgeKind::eTeleport) && nav.Nodes()[on.mTo].mLine == landing.mLine && steps[on.mTo] < 0)
                        {
                            steps[on.mTo] = steps[n] + 1;
                            queue.push_back(on.mTo);
                        }
                    }
                }
            }
            if (through >= 0)
            {
                std::vector<Goap::Fact> seenPre;
                for (const Goap::Fact& f : pre)
                {
                    if (!(f == Goap::Fact{Key::kInvisNow, 1}))
                    {
                        seenPre.push_back(f);
                    }
                }
                DomainStep timed = step;
                timed.mOther = static_cast<u32>(through) + 1;
                add("Cross " + std::string(EdgeKindName(e.mKind)) + " " + std::to_string(nav.Nodes()[e.mFrom].mX) + "," + std::to_string(nav.Nodes()[e.mFrom].mY) + "->" + std::to_string(landing.mX) + "," + std::to_string(landing.mY) + " and on to " + std::to_string(nav.Nodes()[through].mX) + " past the Scrab",
                    seenPre, {regionFact(through)}, 700 + e.mCost, timed);
            }
        }
    }

    // The locks Fleeches guard that can be got at by leading them off, worked out first: a lever that lets them
    // out for that may be pulled with them awake behind its gate
    std::map<u32, FleechLure> lures;
    std::set<s32> letOut;
    for (const Entity& entity : world.mEntities)
    {
        const s32 node = entity.mKind == EntityKind::eSlapLock && !entity.mGone && world.mLocksSlapped.count(entity.mId) == 0 ? UseNode(nav, entity, nullptr) : -1;
        FleechLure lure;
                // Or led off: got to follow him away from it and come back to by another way (the walkthrough's
        // way with the first of the Mudanchee vaults)
        if (entity.mKind == EntityKind::eSlapLock && node >= 0)
        {
            std::vector<s32> homes;
            bool guarded = false;
            for (const Entity& f : world.mEntities)
            {
                if (f.mKind != EntityKind::eFleech || f.mGone || world.mFleechKilled.count(f.mId) != 0 || f.mHalf != entity.mHalf)
                {
                    continue;
                }
                const s32 scale = f.mHalf ? 2 : 1;
                const s32 at = nav.NodeAt(f.CentreX(), f.mY2, f.mHalf, 30);
                // (one on the lock's floor within reach of whoever stands at it guards it as well)
                guarded = guarded || (std::abs(f.CentreX() - entity.CentreX()) <= 40 && std::abs(f.mY2 - entity.mY2) <= 120) || (at >= 0 && nav.Nodes()[at].mLine == nav.Nodes()[node].mLine && std::abs(f.CentreX() - nav.Nodes()[node].mX) * scale <= 150);
                if (at >= 0 && std::abs(f.CentreX() - entity.CentreX()) * scale <= 300 && std::abs(f.mY2 - entity.mY2) * scale <= 240)
                {
                    homes.push_back(at);
                }
            }
            if (guarded && !homes.empty())
            {
                const s32 own = entity.mInputs[1] > 1 ? entity.mInputs[1] : -1;
                const auto cost = [&nav, &world](s32 on, s32 open)
                {
                    return [&nav, &world, on, open](const NavEdge& e) -> s32
                    {
                        s32 condSwitch = 0;
                        s32 condValue = 0;
                        if (nav.Condition(e, &condSwitch, &condValue) && condSwitch != open && (condSwitch == on ? 1 : world.Switch(condSwitch)) != condValue)
                        {
                            return -1;
                        }
                        return std::max(1, e.mCost);
                    };
                };
                lure = LureFleeches(nav, world, homes, node, cost(-1, -1), cost(own, -1));
                // Shut in with it they cannot be led anywhere: with the barrier that keeps them in opened
                // first (the walkthrough: "pull the lever on the ground and lead the Fleeches on a chase")
                for (const NavFeature& f : nav.Input().mFeatures)
                {
                    if (lure.mFound || f.mKind != FeatureKind::eBarrier || !f.mSolid || f.mSwitch <= 1 || f.mHalf != entity.mHalf || world.Switch(f.mSwitch) == (f.mStartClosed ? 1 : 0))
                    {
                        continue;
                    }
                    // (from where he lets them out)
                    s32 lever = -1;
                    for (const Entity& l : world.mEntities)
                    {
                        if (l.mKind == EntityKind::eLever && l.mSwitch == f.mSwitch && lever < 0)
                        {
                            lever = UseNode(nav, l, nullptr);
                        }
                    }
                    lure = LureFleeches(nav, world, homes, node, cost(-1, f.mSwitch), cost(own, f.mSwitch), f.mSwitch, lever);
                    lure.mSwitch = f.mSwitch;
                    lure.mValue = f.mStartClosed ? 1 : 0;
                }
            }
        }
        if (lure.mFound)
        {
            lures[entity.mId] = lure;
            if (lure.mSwitch > 1)
            {
                letOut.insert(lure.mSwitch);
            }
        }
    }

    // The Fleeches a Scrab can be taken over and walked to: the ones on the floor of its floor, with every gate
    // between open (the walkthrough's way with the Mudanchee vault's fifth: "possess the Scrab in the background.
    // Have him go right, kill the Fleeches, then return all the way to the left"). Chanted for from a place on its
    // screen that no Scrab gets to. Not where foot switches make a trap of the floor: that is the rampage's.
    std::set<u32> huntable;
    std::set<s32> huntGates;
    for (const Entity& scrab : world.mEntities)
    {
        if (scrab.mKind != EntityKind::eScrab || scrab.mGone || world.mScrabsKilled.count(scrab.mId) != 0)
        {
            continue;
        }
        const ScrabAt* own = world.ScrabOf(scrab.mId);
        const s32 sx = own ? own->mX : scrab.CentreX();
        const s32 sy = own ? own->mY : scrab.mY2;
        const s32 under = nav.NodeAt(sx, sy, scrab.mHalf, 30);
        if (under < 0)
        {
            continue;
        }
        const s32 line = nav.Nodes()[under].mLine;
        const s32 scale = scrab.mHalf ? 2 : 1;
        const auto onLine = [&](const Entity& e)
        {
            const s32 at = e.mHalf == scrab.mHalf ? nav.NodeAt(e.CentreX(), e.mY2, e.mHalf, 30) : -1;
            return at >= 0 && nav.Nodes()[at].mLine == line;
        };
        bool trap = false;
        for (const Entity& f : world.mEntities)
        {
            trap = trap || (f.mKind == EntityKind::eFootSwitch && onLine(f));
        }
        std::vector<Goap::Fact> gates;
        std::vector<Goap::Fact> dead;
        std::vector<u32> group;
        std::vector<s32> ways;
        for (const Entity& f : world.mEntities)
        {
            if (trap || f.mKind != EntityKind::eFleech || f.mGone || f.mInputs[1] != 0 || world.mFleechKilled.count(f.mId) != 0 || !onLine(f) || WallBetween(nav, sx, f.CentreX(), sy, scrab.mHalf))
            {
                continue;
            }
            bool way = true;
            std::vector<Goap::Fact> open;
            for (const NavFeature& g : nav.Input().mFeatures)
            {
                const s32 gx = (g.mX1 + g.mX2) / 2;
                if (g.mKind == FeatureKind::eBarrier && g.mSolid && g.mHalf == scrab.mHalf && std::abs(g.mY2 - sy) * scale <= 30 && gx > std::min(sx, f.CentreX()) && gx < std::max(sx, f.CentreX()))
                {
                    way = way && g.mSwitch > 1;
                    open.push_back({Key::Switch(g.mSwitch), g.mStartClosed ? 1 : 0});
                    ways.push_back(g.mSwitch);
                }
            }
            if (!way)
            {
                continue;
            }
            dead.push_back({Key::Mud(f.mId), 2});
            group.push_back(f.mId);
            for (const Goap::Fact& g : open)
            {
                if (std::find(gates.begin(), gates.end(), g) == gates.end())
                {
                    gates.push_back(g);
                }
            }
        }
        if (dead.empty())
        {
            continue;
        }
        // (from any screen its floor is on: it wanders, and comes to where it sees him. One it is not on now is dearer.)
        const s32 cam = nav.CameraOf(sx, sy - 5);
        std::set<s32> cams;
        for (const NavNode& on : nav.Nodes())
        {
            if (on.mLine == line)
            {
                cams.insert(nav.CameraOf(on.mX, on.mY - 5));
            }
        }
        std::set<std::pair<s32, s32>> from;
        bool any = false;
        for (std::size_t n = 0; n < nav.Nodes().size(); n++)
        {
            const NavNode& spot = nav.Nodes()[n];
            if (spot.mLow || spot.mHazard || spot.mLine == line || cams.count(nav.CameraOf(spot.mX, spot.mY - 5)) == 0 || ScrabReaches(nav, world, spot.mX, spot.mY, spot.mHalf, false))
            {
                continue;
            }
            // (nor on the floor of any other Scrab, one that waits before it strikes among them)
            bool scrabs = false;
            for (const Entity& other : world.mEntities)
            {
                const s32 at = other.mKind == EntityKind::eScrab && !other.mGone && other.mHalf == spot.mHalf ? nav.NodeAt(other.CentreX(), other.mY2, other.mHalf, 30) : -1;
                scrabs = scrabs || (at >= 0 && nav.Nodes()[at].mLine == spot.mLine);
            }
            if (scrabs || !from.insert({d.mRegions[n], nav.CameraOf(spot.mX, spot.mY - 5)}).second)
            {
                continue;
            }
            DomainStep hunt;
            hunt.mKind = StepKind::eScrabHunt;
            hunt.mEntity = scrab.mId;
            hunt.mEdge = static_cast<s32>(n);
            // (not with the power to go unseen in hand: the chant would spend it)
            std::vector<Goap::Fact> pre{regionFact(static_cast<s32>(n)), {Key::kInvisNow, 0}};
            pre.insert(pre.end(), gates.begin(), gates.end());
            add("Have Scrab#" + std::to_string(scrab.mId) + " kill the Fleeches of its floor from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY), pre, dead, nav.CameraOf(spot.mX, spot.mY - 5) == cam || nav.CameraOf(spot.mX, spot.mY - 5) == nav.CameraOf(scrab.CentreX(), scrab.mY2 - 5) ? 900 : 3000, hunt);
            any = true;
        }
        if (any)
        {
            huntable.insert(group.begin(), group.end());
            huntGates.insert(ways.begin(), ways.end());
        }
    }

    for (const Entity& entity : world.mEntities)
    {
        DomainStep step;
        step.mEntity = entity.mId;
        const std::string name = std::string(EntityKindName(entity.mKind)) + "#" + std::to_string(entity.mId);
        switch (entity.mKind)
        {
            case EntityKind::eLever:
            case EntityKind::ePullRing:
            case EntityKind::eWorkWheel:
            case EntityKind::eHandStone:
            case EntityKind::eSlapLock:
            {
                // (a lock that has been slapped is done: its switch is as good as on)
                if (entity.mKind == EntityKind::eSlapLock && world.mLocksSlapped.count(entity.mId) != 0)
                {
                    if (entity.mSwitch > 1)
                    {
                        d.mStart.Set(Key::Switch(entity.mSwitch), 1);
                    }
                    break;
                }
                const s32 node = UseNode(nav, entity, nullptr);
                // A Fleech hanging over a lock bites whoever slaps it: it has to be dead first (the walkthrough has
                // a Scrab do it)
                std::vector<Goap::Fact> guards;
                if (entity.mKind == EntityKind::eSlapLock)
                {
                    for (const Entity& f : world.mEntities)
                    {
                        if (f.mKind == EntityKind::eFleech && f.mHalf == entity.mHalf && std::abs(f.CentreX() - entity.CentreX()) <= 40 && std::abs(f.mY2 - entity.mY2) <= 120)
                        {
                            // (nor one asleep on the floor by a lock that gives the power to go unseen: the slap
                            // wakes it, and he is off and unseen before it strikes. The walkthrough's way with
                            // the fifth of the Mudanchee vaults: "sneak up to the Ghost Lock and slap it. Run
                            // back to the right and climb up. Turn invisible")
                            // (unless a Scrab can be had to kill it first: then that is the way, and the power is
                            // kept for something else)
                            if (f.mInputs[0] != 0 && f.mInputs[1] == 0 && entity.mInputs[0] > 0 && huntable.count(f.mId) == 0)
                            {
                                continue;
                            }
                            // (nor one that is under the lock's floor altogether: on the floor below, or hung
                            // from the underside of this one. The Mudomo vault's path 12)
                            if (node >= 0 && f.mY1 >= nav.Nodes()[node].mY - 5)
                            {
                                continue;
                            }
                            // (not one with a floor between it and the lock: it hangs over that floor)
                            bool floorBetween = false;
                            for (const NavNode& n : nav.Nodes())
                            {
                                floorBetween = floorBetween || (n.mHalf == f.mHalf && std::abs(n.mX - f.CentreX()) <= 13 && n.mY >= f.mY2 && n.mY < entity.mY2 - 20);
                            }
                            if (!floorBetween)
                            {
                                guards.push_back({Key::Mud(f.mId), 2});
                            }
                        }
                    }
                }
                // Paramites on the floor it is used from, with no wall between. Two or more hunt him along it: only
                // unseen then (the Mudomo vaults' path 10: "turn invisible and run past the Paramites ... quickly
                // go down and slap the Ghost Lock, then get back up before the invisibility wears off"). One alone
                // keeps off him until it is cornered, and near it he corners it: that one is drawn off with meat
                // ("throw some meat behind you to get rid of the Paramite, then slap the Ghost Lock").
                u32 shy = 0;
                Goap::Fact shyFact{0, 0};
                if ((entity.mKind == EntityKind::eSlapLock || entity.mKind == EntityKind::eLever) && node >= 0)
                {
                    const NavNode& at = nav.Nodes()[node];
                    std::vector<u32> pack;
                    s32 nearest = 100000;
                    for (const Entity& para : world.mEntities)
                    {
                        if (para.mKind != EntityKind::eParamite || para.mGone || para.mHalf != at.mHalf || para.mInputs[0] != 0)
                        {
                            continue;
                        }
                        s32 px = para.CentreX();
                        s32 py = para.mY2;
                        for (const ParamiteAt& live : world.mParamites)
                        {
                            if (live.mId == para.mId)
                            {
                                px = live.mX;
                                py = live.mY;
                            }
                        }
                        const s32 on = nav.NodeAt(px, py, para.mHalf, 30);
                        if (on >= 0 && nav.Nodes()[on].mLine == at.mLine && !WallBetween(nav, px, at.mX, at.mY, at.mHalf) && !GateBetween(nav, world, px, at.mX, at.mY, at.mHalf))
                        {
                            pack.push_back(para.mId);
                            nearest = std::min(nearest, std::abs(px - at.mX));
                        }
                    }
                    if (pack.size() >= 2 || (pack.size() == 1 && nearest <= 250))
                    {
                        for (u32 id : pack)
                        {
                            guards.push_back({Key::Mud(id), 2});
                        }
                        if (pack.size() == 1)
                        {
                            shy = pack[0];
                            shyFact = {Key::Mud(shy), 2};
                        }
                    }
                }
                // (what it takes with the meat thrown: everything but that Paramite out of the way)
                const auto withMeat = [&shyFact](std::vector<Goap::Fact> pre)
                {
                    pre.erase(std::remove(pre.begin(), pre.end(), shyFact), pre.end());
                    return pre;
                };
                DomainStep meatStep;
                meatStep.mKind = StepKind::eMeatUse;
                meatStep.mEntity = entity.mId;
                meatStep.mOther = shy;
                // Or led off: got to follow him away from it and come back to by another way (the walkthrough's
                // way with the first of the Mudanchee vaults)
                const FleechLure lure = lures.count(entity.mId) ? lures[entity.mId] : FleechLure();
                if (entity.mKind == EntityKind::eSlapLock && entity.mInputs[0] > 0 && node >= 0 && !world.mInvisEver && entity.mSwitch <= 1)
                {
                    step.mKind = StepKind::eUse;
                    std::vector<Goap::Fact> pre{regionFact(node), {Key::kInvis, 0}};
                    pre.insert(pre.end(), guards.begin(), guards.end());
                    add("Get invisibility from " + name, pre, {{Key::kInvis, 1}, {Key::kInvisNow, 1}}, 150, step);
                    if (shy)
                    {
                        add("Throw meat for Paramite#" + std::to_string(shy) + " and get invisibility from " + name, withMeat(pre), {{Key::kInvis, 1}, {Key::kInvisNow, 1}}, 300, meatStep);
                        d.mActions.back().mSpend = {Key::kMeat, 1};
                    }
                }
                if (node < 0 || entity.mHoldOnly || entity.mSwitch <= 1)
                {
                    break;
                }
                const u32 key = Key::Switch(entity.mSwitch);
                if (entity.mKind == EntityKind::eWorkWheel)
                {
                    // A blind Mudokon on the wheel's floor can be called to it from anywhere on its screen that
                    // is on the wheel's side of it: it walks until the wheel stops it
                    for (const Entity& mud : world.mEntities)
                    {
                        if (mud.mKind != EntityKind::eMudokon || !mud.mBlind || mud.mGone || mud.mHalf != entity.mHalf)
                        {
                            continue;
                        }
                        const s32 mudNode = UseNode(nav, mud, nullptr);
                        bool gets = mudNode >= 0 && nav.Nodes()[mudNode].mLine == nav.Nodes()[node].mLine;
                        for (s32 cur = mudNode; gets && cur != node;)
                        {
                            const NavNode& n = nav.Nodes()[cur];
                            s32 next = -1;
                            for (s32 i = n.mFirstEdge; i < n.mFirstEdge + n.mEdgeCount && next < 0; i++)
                            {
                                const NavEdge& e = nav.Edges()[i];
                                const NavNode& to = nav.Nodes()[e.mTo];
                                if (e.mKind == EdgeKind::eWalk && to.mLine == n.mLine && (to.mX - n.mX) * (nav.Nodes()[node].mX - n.mX) > 0 && !to.mHazard && to.mSweep < 0 && to.mSweepHigh < 0 && !nav.Condition(e, nullptr, nullptr))
                                {
                                    next = e.mTo;
                                }
                            }
                            gets = next >= 0;
                            cur = next;
                        }
                        if (!gets || mudNode == node)
                        {
                            continue;
                        }
                        std::set<s32> from;
                        for (std::size_t n = 0; n < nav.Nodes().size(); n++)
                        {
                            if (CallSpot(nav, mud, entity, static_cast<s32>(n)) && from.insert(d.mRegions[n]).second)
                            {
                                DomainStep send;
                                send.mKind = StepKind::eSend;
                                send.mEntity = mud.mId;
                                send.mOther = entity.mId;
                                add("Send Mudokon#" + std::to_string(mud.mId) + " to " + name + " from " + std::to_string(nav.Nodes()[n].mX) + "," + std::to_string(nav.Nodes()[n].mY), {regionFact(static_cast<s32>(n)), {Key::Mud(mud.mId), 0}, {key, 0}}, {{key, 1}}, 250, send);
                            }
                        }
                    }
                }
                step.mKind = StepKind::eUse;
                // A gate with a Scrab behind it is only opened with the power to go invisible in hand (the
                // walkthrough's way past the vault's second screen): it is there to keep the Scrab in. Shutting
                // one is fine.
                bool freesOn = false;
                bool freesOff = false;
                for (const NavFeature& f : nav.Input().mFeatures)
                {
                    // (not a gate a Scrab is to be walked through to Fleeches: that one is opened for it)
                    if (f.mKind != FeatureKind::eBarrier || !f.mSolid || f.mSwitch != entity.mSwitch || huntGates.count(f.mSwitch) != 0)
                    {
                        continue;
                    }
                    for (const Entity& scrab : world.mEntities)
                    {
                        if (scrab.mKind == EntityKind::eScrab && !scrab.mGone && scrab.mHalf == f.mHalf && std::abs(scrab.mY2 - f.mY2) <= 30 && std::abs(scrab.CentreX() - (f.mX1 + f.mX2) / 2) <= 300)
                        {
                            freesOn = freesOn || f.mStartClosed;
                            freesOff = freesOff || !f.mStartClosed;
                        }
                    }
                }
                if (entity.mOp == 0 || entity.mOp == 2)
                {
                    std::vector<Goap::Fact> pre{regionFact(node), {key, 0}};
                    pre.insert(pre.end(), guards.begin(), guards.end());
                    // Nor is a gate opened onto a Fleech that sleeps just behind it: that one is killed first
                    // (one that is awake there is led off, where that is what the gate is opened for: see the locks.
                    // And one asleep on the floor sleeps on: a gate makes no noise, and he sneaks by it.)
                    for (const NavFeature& f : nav.Input().mFeatures)
                    {
                        if (f.mKind != FeatureKind::eBarrier || !f.mSolid || f.mSwitch != entity.mSwitch || !f.mStartClosed)
                        {
                            continue;
                        }
                        // (pulled from the other layer than its gate is in, what the gate lets out does not
                        // get at him: the second Mudomo vault, whose levers are in the background)
                        if (node >= 0 && nav.Nodes()[node].mHalf != f.mHalf)
                        {
                            continue;
                        }
                        for (const Entity& fl : world.mEntities)
                        {
                            const Goap::Fact dead{Key::Mud(fl.mId), 2};
                            if (fl.mKind == EntityKind::eFleech && (fl.mInputs[1] != 0 || (fl.mInputs[0] == 0 && letOut.count(entity.mSwitch) == 0)) && fl.mHalf == f.mHalf && std::abs(fl.CentreX() - (f.mX1 + f.mX2) / 2) <= 80 && fl.mY2 <= f.mY2 + 10 && fl.mY2 >= f.mY2 - 120 && std::find(pre.begin(), pre.end(), dead) == pre.end())
                            {
                                // (unless this lever is the one the Scrab is let through by)
                                bool scrabGate = false;
                                for (const Entity& scrab : world.mEntities)
                                {
                                    scrabGate = scrabGate || (scrab.mKind == EntityKind::eScrab && std::abs(scrab.mY2 - f.mY2) <= 30 && std::abs(scrab.CentreX() - (f.mX1 + f.mX2) / 2) <= 300);
                                }
                                if (!scrabGate)
                                {
                                    pre.push_back(dead);
                                }
                            }
                        }
                    }
                    if (freesOn)
                    {
                        pre.push_back({Key::kInvisNow, 1});
                    }
                    // On the floor of a Scrab that no gate can be shut on (there is none along that floor), in reach
                    // of it, and got to from cells it does not get to with no crossing between that would ask for
                    // it (a ledge over its floor):
                    // unseen (the walkthrough's sixth Mudanchee vault: "use your invisibility power to slap that
                    // other Ghost Lock, then jump back into the well")
                    if (!freesOn && node >= 0)
                    {
                        const NavNode& at = nav.Nodes()[node];
                        bool canShut = true;
                        std::vector<Goap::Fact> shut;
                        // (not on a floor with a gate anywhere along it: there the Scrabs are shut in behind it
                        // first, or lured off, and the steps for that say so)
                        bool gated = false;
                        for (const NavFeature& f : nav.Input().mFeatures)
                        {
                            const s32 by = f.mKind == FeatureKind::eBarrier && f.mSolid && f.mSwitch > 1 && f.mHalf == at.mHalf ? nav.NodeAt((f.mX1 + f.mX2) / 2, f.mY2, f.mHalf, 30) : -1;
                            gated = gated || (by >= 0 && nav.Nodes()[by].mLine == at.mLine);
                        }
                        if (!gated && ScrabReaches(nav, world, at.mX, at.mY, at.mHalf, false, nullptr, &shut, &canShut) && !canShut)
                        {
                            bool mixed = false;
                            for (std::size_t n = 0; n < nav.Nodes().size() && !mixed; n++)
                            {
                                const NavNode& other = nav.Nodes()[n];
                                mixed = d.mRegions[n] == d.mRegions[node] && !ScrabReaches(nav, world, other.mX, other.mY, other.mHalf, false);
                            }
                            if (mixed)
                            {
                                pre.push_back({Key::kInvisNow, 1});
                            }
                        }
                    }
                    bool watchedOrb = false;
                    // At a place a Slig watches, a security orb on its screen (no chant there, so it is not taken
                    // over): unseen (the Mudanchee trial: "turn invisible and climb up. Go right and slap the 2
                    // Ghost Locks")
                    if (node >= 0 && std::find(pre.begin(), pre.end(), Goap::Fact{Key::kInvisNow, 1}) == pre.end() && !(entity.mKind == EntityKind::eSlapLock && entity.mInputs[0] > 0))
                    {
                        const NavNode& at = nav.Nodes()[node];
                        bool watched = false;
                        for (const SligAt& slig : world.Threats())
                        {
                            if (slig.mAsleep || slig.mPossessed || slig.mHalf != at.mHalf || std::abs(slig.mY - at.mY) > 45 || std::abs(slig.mX - at.mX) >= 450)
                            {
                                continue;
                            }
                            for (const Entity& orb : world.mEntities)
                            {
                                // (its screen or the thing's: its beat may cross both)
                                watched = watched || (orb.mKind == EntityKind::eSecurityOrb && !orb.mGone &&
                                                      (nav.CameraOf(orb.mX1, orb.mY1) == nav.CameraOf(slig.mX, slig.mY - 5) || nav.CameraOf(orb.mX1, orb.mY1) == nav.CameraOf(at.mX, at.mY - 5)));
                            }
                        }
                        // (taking it over from the next screen, which has no orb, was tried: its beat has it
                        // there some 50 frames at a time, and a chant needs more)
                        // (only where the power to go unseen is to be had: in a path with no lock that gives it
                        // there is nothing to wait for, and the thing is used as it always was. The Mines' path
                        // 10: the Slig by the well at 1404,339 wakes as he lands, and with this asked for the
                        // levers there were never pulled.)
                        bool power = world.mInvisCharge || world.mInvisible || world.mInvisComing;
                        for (const Entity& lock : world.mEntities)
                        {
                            power = power || (lock.mKind == EntityKind::eSlapLock && lock.mInputs[0] > 0);
                        }
                        if (watched && power)
                        {
                            pre.push_back({Key::kInvisNow, 1});
                            watchedOrb = true;
                        }
                    }
                    // (one that opens such a gate by way of something else, a timer in the Mudanchee vault's path 4,
                    // is not held back: the crossings onto that Scrab's floor want its gate shut or him unseen)
                    for (s32 first : PulledFirst(nav, world, entity))
                    {
                        pre.push_back({Key::Switch(first), 1});
                    }
                    std::vector<Goap::Fact> gives{{key, 1}};
                    if (entity.mKind == EntityKind::eSlapLock && entity.mInputs[0] > 0)
                    {
                        gives.push_back({Key::kInvis, 1});
                        gives.push_back({Key::kInvisNow, 1});
                    }
                    add("Use " + name + " sw" + std::to_string(entity.mSwitch) + " on", pre, gives, 150, step);
                    if (shy)
                    {
                        add("Throw meat for Paramite#" + std::to_string(shy) + " and use " + name + " sw" + std::to_string(entity.mSwitch) + " on", withMeat(pre), gives, 300, meatStep);
                        d.mActions.back().mSpend = {Key::kMeat, 1};
                    }
                    // A lever with a trap door for the cell on its other side: from there too, where that is another
                    // part of the path (he comes down on it, and what the lever does is open it under him. The
                    // Mudomo hub at 3187,1140: "immediately pull the lever beside you or you're lunch".)
                    if (entity.mKind == EntityKind::eLever && node >= 0)
                    {
                        s32 penGate = -1;
                        s32 penOpen = -1;
                        s32 penShut = -1;
                        std::vector<u32> penGroup;
                        if (FleechPen(nav, world, entity, &penGate, &penOpen, &penShut, &penGroup))
                        {
                            std::vector<Goap::Fact> penPre;
                            std::vector<Goap::Fact> penGives = gives;
                            for (const Goap::Fact& f : pre)
                            {
                                bool ofGroup = false;
                                for (u32 id : penGroup)
                                {
                                    ofGroup = ofGroup || (f.mKey == Key::Mud(id) && f.mValue == 2);
                                }
                                if (!ofGroup)
                                {
                                    penPre.push_back(f);
                                }
                            }
                            for (u32 id : penGroup)
                            {
                                penGives.push_back({Key::Mud(id), 2});
                            }
                            penGives.push_back(regionFact(penShut));
                            DomainStep pen;
                            pen.mKind = StepKind::eFleechPen;
                            pen.mEntity = entity.mId;
                            pen.mOther = static_cast<u32>(penGate);
                            const NavFeature& pg = nav.Input().mFeatures[penGate];
                            add("Use " + name + " sw" + std::to_string(entity.mSwitch) + " on and shut its Fleeches in behind the gate at " + std::to_string((pg.mX1 + pg.mX2) / 2) + "," + std::to_string(pg.mY2), penPre, penGives, 700, pen);
                        }
                    }
                    if (entity.mKind == EntityKind::eLever && node >= 0)
                    {
                        const s32 alt = UseNode(nav, entity, nullptr, 1);
                        if (alt >= 0 && alt != node && nav.Nodes()[alt].mTrapDoor >= 0 && d.mRegions[alt] != d.mRegions[node])
                        {
                            std::vector<Goap::Fact> altPre = pre;
                            altPre[0] = regionFact(alt);
                            DomainStep other = step;
                            other.mOther = 1;
                            add("Use " + name + " sw" + std::to_string(entity.mSwitch) + " on from the trap door", altPre, gives, 100, other);
                        }
                    }
                    // A lock Fleeches guard: unseen they do not find him at it. The Mudanchee trial's last: its
                    // door takes a while to open after the slap, the Fleeches of its room wake as he comes and
                    // walk their ledges, and take no notice of him unseen, or seen but quiet on the floor below.
                    // (with enough of the power's time left for the way there, the slap and the way down: some
                    // 170 frames, measured there)
                    if (!guards.empty() && !watchedOrb && !(entity.mKind == EntityKind::eSlapLock && entity.mInputs[0] > 0) &&
                        (world.mInvisCharge || world.mInvisComing || !world.mInvisible || world.mInvisLeft >= kUnseenUse))
                    {
                        std::vector<Goap::Fact> unseenPre;
                        for (const Goap::Fact& f : pre)
                        {
                            if (std::find(guards.begin(), guards.end(), f) == guards.end())
                            {
                                unseenPre.push_back(f);
                            }
                        }
                        unseenPre.push_back({Key::kInvisNow, 1});
                        add("Use " + name + " sw" + std::to_string(entity.mSwitch) + " on unseen", unseenPre, gives, 450, step);
                    }
                    if (lure.mFound)
                    {
                        DomainStep lead;
                        lead.mKind = StepKind::eFleechLure;
                        lead.mEntity = entity.mId;
                        std::vector<Goap::Fact> leadPre{regionFact(lure.mBait), {key, 0}};
                        if (lure.mSwitch > 1)
                        {
                            leadPre.push_back({Key::Switch(lure.mSwitch), lure.mValue});
                        }
                        for (s32 first : PulledFirst(nav, world, entity))
                        {
                            leadPre.push_back({Key::Switch(first), 1});
                        }
                        add("Lead the Fleeches off and use " + name, leadPre, gives, 600, lead);
                    }
                    // Or what its gate keeps in is led to a Scrab and killed with it
                    if (entity.mKind == EntityKind::eLever)
                    {
                        const FleechLead led = LeadFleeches(nav, world, node, entity.mSwitch);
                        if (led.mFound)
                        {
                            DomainStep lead;
                            lead.mKind = StepKind::eFleechLead;
                            lead.mEntity = entity.mId;
                            // (not with the power to go unseen in hand: the chant for the Scrab would spend it)
                            std::vector<Goap::Fact> leadPre{regionFact(node), {key, 0}, {Key::kInvisNow, 0}};
                            for (s32 first : PulledFirst(nav, world, entity))
                            {
                                leadPre.push_back({Key::Switch(first), 1});
                            }
                            std::vector<Goap::Fact> leadGives{{key, 1}};
                            for (u32 f : led.mGroup)
                            {
                                leadGives.push_back({Key::Mud(f), 2});
                            }
                            add("Let the Fleeches out with " + name + " and lead them to Scrab#" + std::to_string(led.mScrab), leadPre, leadGives, 1500, lead);
                        }
                    }
                    // Or a Paramite of the floor under a ring pulls it: taken over from a place on its screen that is
                    // not on its level (alone it keeps off him, but cornered it strikes), where no orb stops the
                    // chant. The Mudomo vaults' way with every ring that hangs over a pit.
                    if (entity.mKind == EntityKind::ePullRing)
                    {
                        const NavNode& under = nav.Nodes()[node];
                        const s32 scale = under.mHalf ? 2 : 1;
                        for (const Entity& para : world.mEntities)
                        {
                            // (one that walks its floor, or one that comes down a web when a switch is set, which
                            // his coming on its screen does: one that is spawned is not there to be taken over)
                            if (para.mKind != EntityKind::eParamite || para.mGone || para.mHalf != entity.mHalf || (para.mInputs[0] != 0 && para.mInputs[1] == 0))
                            {
                                continue;
                            }
                            s32 px = para.CentreX();
                            s32 py = para.mY2;
                            bool down = para.mInputs[0] == 0;
                            for (const ParamiteAt& live : world.mParamites)
                            {
                                if (live.mId == para.mId)
                                {
                                    px = live.mX;
                                    py = live.mY;
                                    down = !live.Hidden();
                                }
                            }
                            s32 at = nav.NodeAt(px, py, para.mHalf, 30);
                            if (!down)
                            {
                                // (up its web still: the floor it comes down on)
                                at = -1;
                                for (std::size_t n = 0; n < nav.Nodes().size(); n++)
                                {
                                    const NavNode& below = nav.Nodes()[n];
                                    if (below.mHalf == para.mHalf && below.mY > py - 10 && std::abs(below.mX - px) <= NavGraph::GridStep(para.mHalf) && (at < 0 || below.mY < nav.Nodes()[at].mY))
                                    {
                                        at = static_cast<s32>(n);
                                    }
                                }
                            }
                            if (at < 0)
                            {
                                continue;
                            }
                            // (the screen it is on is the one he chants on: the ring may be on another, along its
                            // floor. The Mudomo vaults' path 7: the ring a screen on from the two below the ledge.)
                            const s32 cam = nav.CameraOf(px, nav.Nodes()[at].mY - 5);
                            bool watched = false;
                            for (const Entity& o : world.mEntities)
                            {
                                watched = watched || (o.mKind == EntityKind::eSecurityOrb && !o.mGone && nav.CameraOf(o.mX1, o.mY1) == cam);
                            }
                            if (watched)
                            {
                                continue;
                            }
                            // (on the ring's floor, or on one with a web up to it)
                            const bool level = nav.Nodes()[at].mLine == under.mLine && !WallBetween(nav, px, under.mX, under.mY, under.mHalf) && !GateBetween(nav, world, px, under.mX, under.mY, under.mHalf);
                            // (or one it can be walked to by a longer way: webs down, hops, other screens. Those
                            // are for the round of all its rings, below.)
                            const bool routed = !level && !WebUp(nav, at, node, nullptr);
                            if (routed && !ParamiteRoute(nav, world, at, node, nullptr))
                            {
                                continue;
                            }
                            // (the chant takes the nearest on his screen, Abe::FindObjectToPossess: from a place
                            // that this one is the nearest to. The Mudomo vault's path 9, two of them on ledges
                            // either side of the door: from under the left one the right one's ring was never got)
                            const auto nearest = [&](const NavNode& spot)
                            {
                                const auto far = [&](s32 x, s32 y)
                                {
                                    return (x - spot.mX) * (x - spot.mX) + (y - spot.mY) * (y - spot.mY);
                                };
                                const s32 own = far(px, py);
                                for (const Entity& o : world.mEntities)
                                {
                                    if (o.mKind != EntityKind::eParamite || o.mGone || o.mId == para.mId || o.mHalf != para.mHalf)
                                    {
                                        continue;
                                    }
                                    s32 ox = o.CentreX();
                                    s32 oy = o.mY2;
                                    for (const ParamiteAt& live : world.mParamites)
                                    {
                                        if (live.mId == o.mId)
                                        {
                                            ox = live.mX;
                                            oy = live.mY;
                                        }
                                    }
                                    if (nav.CameraOf(ox, oy - 5) == cam && far(ox, oy) <= own)
                                    {
                                        return false;
                                    }
                                }
                                return true;
                            };
                            std::set<s32> from;
                            for (std::size_t n = 0; n < nav.Nodes().size(); n++)
                            {
                                const NavNode& spot = nav.Nodes()[n];
                                if (spot.mLow || spot.mHazard || spot.mHalf || nav.CameraOf(spot.mX, spot.mY - 5) != cam || (!under.mHalf && std::abs(spot.mY - nav.Nodes()[at].mY) * scale <= 45) || !nearest(spot) || !from.insert(d.mRegions[n]).second)
                                {
                                    continue;
                                }
                                DomainStep pull;
                                pull.mKind = StepKind::eParamitePull;
                                pull.mEntity = entity.mId;
                                pull.mOther = para.mId;
                                pull.mEdge = static_cast<s32>(n);
                                // (not with the power to go unseen in hand: the chant would spend it)
                                std::vector<Goap::Fact> pullPre{regionFact(static_cast<s32>(n)), {key, 0}, {Key::kInvisNow, 0}};
                                // (more than one that it can be walked to, or one that is not on its own floor's
                                // way: all of them in one go, and it is left under the last. The walkthrough of the
                                // first Mudomo vault: "continue left, climb up to the Ghost Lock and pull the ring
                                // to lower the lift ... climb down the web at the end. Pull the ring above the
                                // platform on the right")
                                const std::vector<u32> tour = ParamiteTour(nav, world, at);
                                if ((tour.size() >= 2 || routed) && !tour.empty() && tour[0] == entity.mId)
                                {
                                    std::vector<Goap::Fact> tourPre{regionFact(static_cast<s32>(n)), {Key::kInvisNow, 0}};
                                    std::vector<Goap::Fact> tourGives;
                                    std::string names;
                                    for (u32 id : tour)
                                    {
                                        const Entity* r = world.Find(id);
                                        tourPre.push_back({Key::Switch(r->mSwitch), 0});
                                        tourGives.push_back({Key::Switch(r->mSwitch), 1});
                                        names += (names.empty() ? "PullRing#" : ", PullRing#") + std::to_string(id);
                                    }
                                    DomainStep all = pull;
                                    all.mKind = StepKind::eParamiteTour;
                                    add("Have Paramite#" + std::to_string(para.mId) + " pull " + names + " from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY), tourPre, tourGives, 500 + 100 * static_cast<s32>(tour.size()), all);
                                    continue;
                                }
                                if (routed)
                                {
                                    continue;
                                }
                                add("Have Paramite#" + std::to_string(para.mId) + " pull " + name + " from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY), pullPre, gives, 700, pull);
                            }
                        }
                    }
                }
                if (entity.mOp == 1 || entity.mOp == 2)
                {
                    std::vector<Goap::Fact> offPre{regionFact(node), {key, 1}};
                    if (freesOff)
                    {
                        offPre.push_back({Key::kInvisNow, 1});
                    }
                    add("Use " + name + " sw" + std::to_string(entity.mSwitch) + " off", offPre, {{key, 0}}, 150, step);
                }
                break;
            }

            case EntityKind::eScrab:
            {
                if (entity.mGone || world.mScrabsKilled.count(entity.mId) != 0)
                {
                    d.mStart.Set(Key::Mud(entity.mId), 2);
                    break;
                }
                if (world.mScrabParked.count(entity.mId) != 0)
                {
                    break;
                }
                // A floor of Scrabs with gates that foot switches open, and one that a foot switch shuts again:
                // from a ledge over it one Scrab is taken over, made to kill the rest and the Fleeches down
                // there, and walked in behind the gate that shuts
                const s32 under = nav.NodeAt(entity.CentreX(), entity.mY2, entity.mHalf, 30);
                if (under < 0)
                {
                    break;
                }
                const s32 line = nav.Nodes()[under].mLine;
                bool trap = false;
                for (const Entity& f : world.mEntities)
                {
                    const s32 at = f.mKind == EntityKind::eFootSwitch && f.mOp == 1 ? nav.NodeAt(f.CentreX(), f.mY2, f.mHalf, 30) : -1;
                    trap = trap || (at >= 0 && nav.Nodes()[at].mLine == line);
                }
                if (!trap)
                {
                    break;
                }
                std::vector<Goap::Fact> gives;
                for (const Entity& other : world.mEntities)
                {
                    const s32 at = other.mKind == EntityKind::eScrab || other.mKind == EntityKind::eFleech || other.mKind == EntityKind::eFootSwitch ? nav.NodeAt(other.CentreX(), other.mY2, other.mHalf, 30) : -1;
                    if (at < 0 || nav.Nodes()[at].mLine != line)
                    {
                        continue;
                    }
                    if (other.mKind == EntityKind::eFootSwitch)
                    {
                        // (the gates that are only ever opened; the trap's ends shut)
                        bool shutAgain = false;
                        for (const Entity& off : world.mEntities)
                        {
                            shutAgain = shutAgain || (off.mKind == EntityKind::eFootSwitch && off.mSwitch == other.mSwitch && off.mOp == 1);
                        }
                        if (other.mOp == 0 && !shutAgain)
                        {
                            gives.push_back({Key::Switch(other.mSwitch), 1});
                        }
                    }
                    else
                    {
                        gives.push_back({Key::Mud(other.mId), 2});
                    }
                }
                // It has to be one that can get out to the rest: every shut gate between it and another of them
                // with the foot switch that opens it on this Scrab's side
                bool getsOut = true;
                for (const Entity& other : world.mEntities)
                {
                    const s32 at = (other.mKind == EntityKind::eScrab || other.mKind == EntityKind::eFleech) && other.mId != entity.mId ? nav.NodeAt(other.CentreX(), other.mY2, other.mHalf, 30) : -1;
                    if (at < 0 || nav.Nodes()[at].mLine != line)
                    {
                        continue;
                    }
                    for (const NavFeature& gate : nav.Input().mFeatures)
                    {
                        const s32 gx = (gate.mX1 + gate.mX2) / 2;
                        if (gate.mKind != FeatureKind::eBarrier || !gate.mSolid || std::abs(gate.mY2 - entity.mY2) > 30 || gx <= std::min(entity.CentreX(), other.CentreX()) || gx >= std::max(entity.CentreX(), other.CentreX()) || world.Switch(gate.mSwitch) == (gate.mStartClosed ? 1 : 0))
                        {
                            continue;
                        }
                        bool opens = false;
                        for (const Entity& sw : world.mEntities)
                        {
                            opens = opens || (sw.mKind == EntityKind::eFootSwitch && sw.mSwitch == gate.mSwitch && sw.mOp == (gate.mStartClosed ? 0 : 1) && (sw.CentreX() < gx) == (entity.CentreX() < gx));
                        }
                        getsOut = getsOut && opens;
                    }
                }
                if (!getsOut)
                {
                    break;
                }
                std::set<s32> from;
                for (std::size_t n = 0; n < nav.Nodes().size(); n++)
                {
                    const NavNode& spot = nav.Nodes()[n];
                    if (spot.mHalf == entity.mHalf && spot.mY < entity.mY2 - 60 && spot.mY > entity.mY2 - 200 && nav.CameraOf(spot.mX, spot.mY - 5) == nav.CameraOf(entity.CentreX(), entity.mY2 - 5) && from.insert(d.mRegions[n]).second)
                    {
                        step.mKind = StepKind::eScrabRampage;
                        step.mEdge = static_cast<s32>(n);
                        add("Clear the floor with " + name + " from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY), {regionFact(static_cast<s32>(n))}, gives, 2000, step);
                    }
                }
                break;
            }

            case EntityKind::eFleech:
            {
                if (entity.mGone || world.mFleechKilled.count(entity.mId) != 0)
                {
                    d.mStart.Set(Key::Mud(entity.mId), 2);
                    break;
                }
                // A Scrab on the floor under it, a gate between the two, and that gate's lever on a ledge the
                // Scrab can't get to: from there the Scrab is taken over and made to kill it
                s32 floorY = 0;
                // (it hangs from the underside of a ledge: the floor is the one under that)
                if (nav.FloorBelow(entity.CentreX(), entity.mY1 + 20, entity.mHalf, &floorY) < 0)
                {
                    break;
                }
                for (const Entity& scrab : world.mEntities)
                {
                    // (the one on that floor a screen or so off, not one that happens to be at the same height elsewhere)
                    if (scrab.mKind != EntityKind::eScrab || scrab.mGone || scrab.mHalf != entity.mHalf || std::abs(scrab.mY2 - floorY) > 30 || std::abs(scrab.CentreX() - entity.CentreX()) > 500)
                    {
                        continue;
                    }
                    for (const NavFeature& gate : nav.Input().mFeatures)
                    {
                        const s32 gateX = (gate.mX1 + gate.mX2) / 2;
                        if (gate.mKind != FeatureKind::eBarrier || !gate.mSolid || gate.mHalf != entity.mHalf || std::abs(gate.mY2 - floorY) > 30 || gateX < std::min(scrab.CentreX(), entity.CentreX()) || gateX > std::max(scrab.CentreX(), entity.CentreX()))
                        {
                            continue;
                        }
                        for (const Entity& lever : world.mEntities)
                        {
                            const s32 at = lever.mKind == EntityKind::eLever && lever.mSwitch == gate.mSwitch ? UseNode(nav, lever, nullptr) : -1;
                            if (at >= 0 && nav.Nodes()[at].mY < floorY - 60)
                            {
                                step.mKind = StepKind::eScrabKill;
                                step.mOther = scrab.mId;
                                step.mEdge = static_cast<s32>(lever.mId);
                                add("Have Scrab#" + std::to_string(scrab.mId) + " kill " + name + " from Lever#" + std::to_string(lever.mId), {regionFact(at)}, {{Key::Mud(entity.mId), 2}}, 900, step);
                            }
                        }
                    }
                }
                break;
            }

            case EntityKind::eSwitchLogic:
            {
                // Done already: nothing to plan, and the Mudokons it took are free again
                if (world.Switch(entity.mSwitch) == (entity.mOp == 1 ? 0 : 1))
                {
                    // (a tasks door's "all on" is no switch of the game's: it is not among the ones read)
                    if (entity.mSwitch >= 256)
                    {
                        d.mStart.Set(Key::Switch(entity.mSwitch), 1);
                    }
                    break;
                }
                // (one that only follows another switch is set with it: see add)
                if (entity.mHoldOnly)
                {
                    break;
                }
                Team team;
                std::vector<Goap::Fact> teamPre;
                if (PlanTogether(nav, world, entity, team, teamPre))
                {
                    const Entity* wheel = world.Find(team.mAbeWheel);
                    teamPre.insert(teamPre.begin(), regionFact(UseNode(nav, *wheel, nullptr)));
                    step.mKind = StepKind::eTogether;
                    add("Work together " + name + " sw" + std::to_string(entity.mSwitch), teamPre, {{Key::Switch(entity.mSwitch), entity.mOp == 1 ? 0 : 1}}, 600, step);
                    break;
                }
                // Several wheels that only count while they turn have to be turned at once: without Mudokons for
                // them there's nothing to do. (Ones that stay on are turned one after the other.)
                s32 wheelInputs = 0;
                for (s32 input : entity.mInputs)
                {
                    for (const Entity& w : world.mEntities)
                    {
                        wheelInputs += input > 1 && w.mKind == EntityKind::eWorkWheel && w.mSwitch == input && w.mHoldOnly ? 1 : 0;
                    }
                }
                if (wheelInputs >= 2)
                {
                    break;
                }
                std::vector<Goap::Fact> pre;
                for (s32 input : entity.mInputs)
                {
                    // 0 is no switch and 1 is the one that is always on
                    if (input > 1)
                    {
                        const Goap::Fact fact{Key::Switch(input), 1};
                        bool have = false;
                        for (const Goap::Fact& p : pre)
                        {
                            have = have || p == fact;
                        }
                        if (!have)
                        {
                            pre.push_back(fact);
                        }
                    }
                }
                step.mKind = StepKind::eLogic;
                add(name + " sw" + std::to_string(entity.mSwitch), pre, {{Key::Switch(entity.mSwitch), entity.mOp == 1 ? 0 : 1}}, 1, step);
                break;
            }

            case EntityKind::eMineCar:
            {
                // From where it stands to either end of each of its floors that has a floor to get out on: which
                // of them it can get to is found by driving
                const s32 carX = entity.mLive ? entity.mLiveX : entity.CentreX();
                const s32 carY = entity.mLive ? entity.mLiveY : entity.mY2;
                const s32 node = nav.NodeAt(carX, carY, entity.mHalf, 30);
                if (node < 0)
                {
                    break;
                }
                const s32 cell = NavGraph::GridStep(entity.mHalf);
                // The slam doors and the like across its floors have to be open
                std::vector<Goap::Fact> open = {regionFact(node)};
                for (const NavFeature& f : nav.Input().mFeatures)
                {
                    if (f.mKind != FeatureKind::eBarrier || f.mHalf != entity.mHalf || f.mSwitch <= 1)
                    {
                        continue;
                    }
                    for (const NavLine& line : nav.Input().mLines)
                    {
                        if (line.mType == kMineCarFloor && line.mY1 >= f.mY1 && line.mY1 <= f.mY2 && f.mX2 >= std::min(line.mX1, line.mX2) && f.mX1 <= std::max(line.mX1, line.mX2))
                        {
                            open.push_back({Key::Switch(f.mSwitch), f.mStartClosed ? 1 : 0});
                            break;
                        }
                    }
                }
                std::set<s32> to;
                for (const NavLine& line : nav.Input().mLines)
                {
                    if (line.mType != kMineCarFloor)
                    {
                        continue;
                    }
                    for (s32 end = 0; end < 2; end++)
                    {
                        const s32 x = end == 0 ? std::min(line.mX1, line.mX2) + cell / 2 : std::max(line.mX1, line.mX2) - cell / 2;
                        const s32 dest = nav.NodeAt(x, line.mY1 + (entity.mHalf ? 10 : 20), entity.mHalf, 12);
                        if (dest < 0 || nav.Nodes()[dest].mHazard || d.mRegions[dest] == d.mRegions[node] || !to.insert(d.mRegions[dest]).second)
                        {
                            continue;
                        }
                        step.mKind = StepKind::eRide;
                        step.mEdge = dest;
                        add("Ride " + name + " to " + std::to_string(nav.Nodes()[dest].mX) + "," + std::to_string(nav.Nodes()[dest].mY), open, {regionFact(dest)}, 600 + (std::abs(nav.Nodes()[dest].mX - carX) + std::abs(nav.Nodes()[dest].mY - carY)) / 6, step);
                    }
                }
                break;
            }

            case EntityKind::eRockSack:
            {
                const s32 node = UseNode(nav, entity, nullptr);
                if (node >= 0)
                {
                    step.mKind = StepKind::eRocks;
                    // (a sack gives nothing to Abe while he has some)
                    const u32 what = entity.mInputs[1] == 1 ? Key::kMeat : Key::kRocks;
                    add(std::string(entity.mInputs[1] == 1 ? "Get meat from " : entity.mInputs[1] == 2 ? "Get a grenade from " : "Get rocks from ") + name, {regionFact(node), {what, 0}}, {{what, entity.mInputs[0] > 0 ? entity.mInputs[0] : 5}}, 300, step);
                }
                break;
            }

            case EntityKind::ePantsLocker:
                // (the floor by it is clear as things are)
                if (world.Switch(entity.mSwitch))
                {
                    d.mStart.Set(Key::Switch(entity.mSwitch), 1);
                }
                break;

            case EntityKind::eUxb:
            {
                if (entity.mGone)
                {
                    d.mStart.Set(Key::Switch(entity.mSwitch), 1);
                    break;
                }
                // From the cell on either side of it, on its floor. (One with a UXB of its own is stood in once
                // that is off.)
                const s32 cell = NavGraph::GridStep(entity.mHalf);
                const s32 at = nav.NodeAt(entity.CentreX(), entity.mY2, entity.mHalf, 12);
                for (s32 side = -1; at >= 0 && side <= 1; side += 2)
                {
                    const s32 stand = nav.NodeAt(nav.Nodes()[at].mX + side * cell, nav.Nodes()[at].mY, entity.mHalf, 12);
                    if (stand < 0 || stand == at)
                    {
                        continue;
                    }
                    const NavNode& on = nav.Nodes()[stand];
                    if (on.mLow || (on.mHazard && on.mClear < 0) || on.mLine != nav.Nodes()[at].mLine)
                    {
                        continue;
                    }
                    std::vector<Goap::Fact> pre{regionFact(stand)};
                    if (on.mClear >= 0)
                    {
                        pre.push_back({Key::Switch(nav.Input().mFeatures[on.mClear].mSwitch), 1});
                    }
                    step.mKind = StepKind::eDisarm;
                    step.mEdge = stand;
                    add("Turn off " + name + " from " + std::to_string(on.mX) + "," + std::to_string(on.mY), pre, {{Key::Switch(entity.mSwitch), 1}}, 260, step);
                }
                break;
            }

            case EntityKind::eMine:
            {
                if (entity.mGone)
                {
                    d.mStart.Set(Key::Switch(entity.mSwitch), 1);
                    break;
                }
                // From any region there is a place to throw from
                // A drill that goes where the rock would fly takes most throws: with it stopped first if it can
                // be, and past it as it runs at a price
                std::set<std::pair<s32, u32>> from;
                const std::vector<ThrowSpot> spots = ThrowSpots(nav, entity.CentreX(), entity.mY2, entity.mHalf, &world);
                for (const ThrowSpot& spot : spots)
                {
                    const Entity* drill = spot.mDrill ? world.Find(spot.mDrill) : nullptr;
                    if (drill && !(drill->mInputs[0] == 1 && drill->mInputs[2] == 0 && drill->mSwitch > 1))
                    {
                        drill = nullptr;
                    }
                    if (from.insert({d.mRegions[spot.mNode], drill ? drill->mId : 0}).second)
                    {
                        step.mKind = StepKind::eBlow;
                        step.mEdge = spot.mNode;
                        std::vector<Goap::Fact> pre{regionFact(spot.mNode)};
                        for (s32 need : spot.mNeeds)
                        {
                            pre.push_back({Key::Switch(need), 1});
                        }
                        for (const std::pair<s32, s32>& open : spot.mOpen)
                        {
                            pre.push_back({Key::Switch(open.first), open.second});
                        }
                        // (a throw that may miss is taken back and tried again: dearer)
                        const s32 cost = spot.mSure ? 300 : 500;
                        const std::string blow = "Blow up " + name + " from " + std::to_string(nav.Nodes()[spot.mNode].mX) + "," + std::to_string(nav.Nodes()[spot.mNode].mY);
                        if (drill)
                        {
                            add(blow + " past Drill#" + std::to_string(drill->mId), pre, {{Key::Switch(entity.mSwitch), 1}}, cost + kPastDrillCost, step);
                            d.mActions.back().mSpend = {Key::kRocks, 1};
                            pre.push_back({Key::Switch(drill->mSwitch), drill->mInputs[1] ? 1 : 0});
                        }
                        add(blow, pre, {{Key::Switch(entity.mSwitch), 1}}, cost, step);
                        d.mActions.back().mSpend = {Key::kRocks, 1};
                    }
                }
                break;
            }

            case EntityKind::eMudokon:
            {
                if (entity.mGone)
                {
                    d.mStart.Set(Key::Mud(entity.mId), 1);
                    break;
                }
                const s32 node = UseNode(nav, entity, nullptr);
                if (node < 0)
                {
                    break;
                }
                if (kills && entity.mMood != MudMood::eSick)
                {
                    // Slaps do it, wherever Abe can stand next to it (a sick one doesn't feel them)
                    DomainStep kill;
                    kill.mKind = StepKind::eKill;
                    kill.mEntity = entity.mId;
                    add("Kill " + name, {regionFact(node), {Key::Mud(entity.mId), 0}}, {{Key::Mud(entity.mId), 2}}, 500, kill);
                }
                // The nearest portal it can walk to with Abe: it walks, drops off ends and rides lifts, and climbs nothing
                std::vector<bool> walks(nav.Nodes().size(), false);
                walks[node] = true;
                for (bool more = true; more;)
                {
                    more = false;
                    for (const NavEdge& edge : nav.Edges())
                    {
                        // (a hop along one floor is over something lying on it, which can be dealt with: not a gap)
                        const bool sameFloor = nav.Nodes()[edge.mFrom].mLine == nav.Nodes()[edge.mTo].mLine;
                        const bool can = edge.mKind == EdgeKind::eWalk || edge.mKind == EdgeKind::eFall || edge.mKind == EdgeKind::eLift || edge.mKind == EdgeKind::eRoll || ((edge.mKind == EdgeKind::eHop || edge.mKind == EdgeKind::eRunJump) && sameFloor);
                        if (can && walks[edge.mFrom] && !walks[edge.mTo])
                        {
                            walks[edge.mTo] = true;
                            more = true;
                        }
                    }
                }
                const Entity* best = nullptr;
                s32 bestDist = 0;
                std::vector<s32> blindWay;
                for (const Entity& portal : world.mEntities)
                {
                    if (portal.mKind != EntityKind::eBirdPortal || !portal.mRescuePortal || portal.mHalf != entity.mHalf)
                    {
                        continue;
                    }
                    if (entity.mBlind)
                    {
                        // It is walked there a stretch at a time, see RescueTask::LeadBlind
                        std::vector<s32> way;
                        if (!(portal.mInputs[1] > 1 && world.Switch(portal.mInputs[1])) && BlindWay(nav, world, node, portal, false, way) && (!best || way.size() < blindWay.size()))
                        {
                            best = &portal;
                            blindWay = way;
                        }
                        continue;
                    }
                    // Gone for good (the switch that does it is often the rescue it was put there for)
                    if (portal.mInputs[1] > 1 && world.Switch(portal.mInputs[1]))
                    {
                        continue;
                    }
                    const s32 pnode = UseNode(nav, portal, nullptr);
                    if (pnode < 0 || d.mRegions[pnode] != d.mRegions[node] || !walks[pnode])
                    {
                        continue;
                    }
                    const s32 dist = std::abs(portal.CentreX() - entity.CentreX()) + std::abs(portal.mY2 - entity.mY2) * 4;
                    if (!best || dist < bestDist)
                    {
                        best = &portal;
                        bestDist = dist;
                    }
                }
                if (!best)
                {
                    break;
                }
                step.mKind = StepKind::eRescue;
                step.mOther = best->mId;
                std::vector<Goap::Fact> effects{{Key::Mud(entity.mId), 1}};
                if (entity.mSwitch > 1)
                {
                    effects.push_back({Key::Switch(entity.mSwitch), 1});
                }
                std::vector<Goap::Fact> pre{regionFact(node), {Key::Mud(entity.mId), 0}};
                if (entity.mBlind && !blindWay.empty())
                {
                    // From where it ends up, with its way open and the drills on it stopped
                    pre[0] = regionFact(nav.Edges()[blindWay.back()].mTo);
                    const auto need = [&pre](const Goap::Fact& fact)
                    {
                        bool have = false;
                        for (const Goap::Fact& p : pre)
                        {
                            have = have || p == fact;
                        }
                        if (!have)
                        {
                            pre.push_back(fact);
                        }
                    };
                    for (s32 idx : blindWay)
                    {
                        const NavEdge& e = nav.Edges()[idx];
                        s32 condSwitch = 0;
                        s32 condValue = 0;
                        const bool drop = e.mKind == EdgeKind::eFall && nav.Nodes()[e.mFrom].mTrapDoor >= 0 && e.mFeature == nav.Nodes()[e.mFrom].mTrapDoor;
                        if (!drop && nav.Condition(e, &condSwitch, &condValue))
                        {
                            need({Key::Switch(condSwitch), condValue});
                        }
                        // (a lift that isn't at the stop it is walked onto: the switch that brings it)
                        const NavNode& onto = nav.Nodes()[e.mTo];
                        if (onto.mLiftStop >= 0 && nav.Nodes()[e.mFrom].mLiftStop < 0)
                        {
                            const NavFeature& stop = nav.Input().mFeatures[onto.mLiftStop];
                            if (!world.LiftAtStop(stop.mX1, stop.mX2, onto.mY, stop.mA != 0) && stop.mSwitch > 1)
                            {
                                need({Key::Switch(stop.mSwitch), 1});
                            }
                        }
                        for (s32 high = 0; high < 2; high++)
                        {
                            if (const Entity* drill = DrillAt(nav, world, e.mTo, high != 0))
                            {
                                need({Key::Switch(drill->mSwitch), drill->mInputs[1] ? 1 : 0});
                            }
                        }
                    }
                }
                if (best->mInputs[0] > 1)
                {
                    // The portal isn't there until its switch is on
                    pre.push_back({Key::Switch(best->mInputs[0]), 1});
                }
                add("Rescue " + name + " via BirdPortal#" + std::to_string(best->mId), pre, effects, 400 + bestDist / 4, step);
                d.mRescuable.push_back(entity.mId);
                break;
            }

            default:
                break;
        }
    }

    // Paramites that team up over the way out, in a path that has meat and no lock that makes him invisible: meat
    // is thrown for them first (the walkthrough's way with the fifth Mudomo vault: "At the top, toss the meat up
    // to the right, then climb up left and run to the exit"). The way out is barred when no way to it keeps off
    // their level, from where he is or from where its part of the path is come into.
    std::vector<char> packAt;
    {
        bool invisLock = false;
        s32 paramites = 0;
        for (const Entity& e : world.mEntities)
        {
            invisLock = invisLock || (e.mKind == EntityKind::eSlapLock && e.mInputs[0] > 0);
            paramites += e.mKind == EntityKind::eParamite && !e.mGone ? 1 : 0;
        }
        if (world.CarriesMeat() && !invisLock && paramites >= 2 && !world.mInvisible && !world.mInvisCharge)
        {
            packAt.resize(nav.Nodes().size(), 0);
            for (std::size_t n = 0; n < nav.Nodes().size(); n++)
            {
                packAt[n] = PackLevel(nav, world, static_cast<s32>(n)) ? 1 : 0;
            }
        }
        bool meatOut = false;
        if (abeNode >= 0)
        {
            const s32 cam = nav.CameraOf(nav.Nodes()[abeNode].mX, nav.Nodes()[abeNode].mY - 5);
            for (const RockAt& r : world.mRocksDown)
            {
                meatOut = meatOut || nav.CameraOf(r.mX, r.mY - 5) == cam;
            }
            for (const RockAt& r : world.mRocksFlying)
            {
                meatOut = meatOut || nav.CameraOf(r.mX, r.mY - 5) == cam;
            }
        }
        d.mStart.Set(Key::kPackFed, meatOut ? 1 : 0);
    }
    const auto packBars = [&](s32 target) -> bool
    {
        if (packAt.empty())
        {
            return false;
        }
        const s32 region = d.mRegions[target];
        std::vector<char> ok(nav.Nodes().size(), 0);
        ok[target] = 1;
        for (bool changed = true; changed;)
        {
            changed = false;
            for (const NavEdge& e : nav.Edges())
            {
                if (ok[e.mTo] && !ok[e.mFrom] && !packAt[e.mFrom] && d.mRegions[e.mFrom] == region)
                {
                    ok[e.mFrom] = 1;
                    changed = true;
                }
            }
        }
        if (abeNode >= 0 && d.mRegions[abeNode] == region)
        {
            return !ok[abeNode];
        }
        bool entries = false;
        bool open = false;
        for (const NavEdge& e : nav.Edges())
        {
            if (d.mRegions[e.mTo] == region && d.mRegions[e.mFrom] != region)
            {
                entries = true;
                open = open || ok[e.mTo];
            }
        }
        return entries && !open;
    };

    for (std::size_t i = 0; i < nav.Exits().size(); i++)
    {
        DomainStep step;
        step.mKind = StepKind::eExit;
        step.mEdge = static_cast<s32>(i);
        const NavExit& x = nav.Exits()[i];
        const NavFeature& f = nav.Input().mFeatures[x.mFeature];
        if (x.mNode < 0)
        {
            add("Exit by switch " + std::to_string(f.mSwitch) + " to path " + std::to_string(f.mDestPath), {{Key::Switch(f.mSwitch), 1}}, {{Key::kExited, 1}}, 50, step);
            continue;
        }
        std::vector<Goap::Fact> pre{regionFact(x.mNode)};
        // A lock in the path that makes him invisible, and Fleeches in it: that is how they are meant to be got
        // past, so not out without it
        bool lock = false;
        bool fleeches = false;
        for (const Entity& e : world.mEntities)
        {
            // (a lock that gives the power and nothing else: one with a ghost is on the way to a door anyhow)
            lock = lock || (e.mKind == EntityKind::eSlapLock && e.mInputs[0] > 0 && e.mSwitch <= 1);
            fleeches = fleeches || e.mKind == EntityKind::eFleech;
        }
        if (lock && fleeches)
        {
            pre.push_back({Key::kInvis, 1});
        }
        if (f.mKind == FeatureKind::eDoor && f.mStartClosed && f.mSwitch > 0)
        {
            pre.push_back({Key::Switch(f.mSwitch), 1});
        }
        // (a well that leads out only with its switch on)
        if (f.mKind == FeatureKind::eWellExpress && f.mSwitch > 1 && f.mDestPath < 0 && f.mOnDestPath >= 0)
        {
            pre.push_back({Key::Switch(f.mSwitch), 1});
        }
        // Fleeches asleep by the way out that a Scrab can be had to kill: dead first (they wake as he comes up)
        for (const Entity& fl : world.mEntities)
        {
            const NavNode& at = nav.Nodes()[x.mNode];
            if (fl.mKind == EntityKind::eFleech && huntable.count(fl.mId) != 0 && fl.mHalf == at.mHalf && std::abs(fl.mY2 - at.mY) * (at.mHalf ? 2 : 1) <= 30 && std::abs(fl.CentreX() - at.mX) * (at.mHalf ? 2 : 1) <= 100)
            {
                pre.push_back({Key::Mud(fl.mId), 2});
            }
        }
        if (packBars(x.mNode))
        {
            pre.push_back({Key::kPackFed, 1});
            DomainStep feed;
            feed.mKind = StepKind::eMeatFeed;
            feed.mEdge = x.mNode;
            add("Throw meat for the Paramites on the way to the exit at " + std::to_string(nav.Nodes()[x.mNode].mX) + "," + std::to_string(nav.Nodes()[x.mNode].mY), {regionFact(x.mNode), {Key::kMeat, 1}}, {{Key::kPackFed, 1}}, 300, feed);
            d.mActions.back().mSpend = {Key::kMeat, 1};
        }
        add("Exit " + std::to_string(nav.Nodes()[x.mNode].mX) + "," + std::to_string(nav.Nodes()[x.mNode].mY) + " to path " + std::to_string(f.mDestPath), pre, {{Key::kExited, 1}}, 200, step);
    }
    // A Slig that walks over a trap door a lever opens: the lever is pulled as it is on the door, and until it
    // has gone down nothing is crossed onto its floor near there (AO's Rupture Farms, path 16: the Slig at
    // 3600,313 that walks over the trap door at 3539, and the lever on the ledge over it)
    for (const Entity& lever : world.mEntities)
    {
        if (lever.mKind != EntityKind::eLever || lever.mGone || lever.mSwitch <= 1)
        {
            continue;
        }
        const s32 at = UseNode(nav, lever, nullptr);
        for (std::size_t fi = 0; at >= 0 && fi < nav.Input().mFeatures.size(); fi++)
        {
            const NavFeature& door = nav.Input().mFeatures[fi];
            // (or under a meat saw the lever brings down: path 18's second screen)
            // (the lever's own switch, or one a timer sets off it: the Barracks' first path, where Lever#7005's 157
            // opens the trap door on 156 the guard by the well stands on)
            bool sets = door.mSwitch == lever.mSwitch;
            for (const Entity& logic : world.mEntities)
            {
                sets = sets || (door.mKind == FeatureKind::eTrapDoor && logic.mKind == EntityKind::eSwitchLogic && logic.mHoldOnly && logic.mSwitch == door.mSwitch && logic.mInputs[0] == lever.mSwitch);
            }
            if ((door.mKind != FeatureKind::eTrapDoor && door.mKind != FeatureKind::eSaw) || !sets)
            {
                continue;
            }
            const s32 dx = (door.mX1 + door.mX2) / 2;
            for (const Entity& slig : world.mEntities)
            {
                if (slig.mKind != EntityKind::eSlig || slig.mGone || world.mFleechKilled.count(slig.mId) != 0 || slig.mState == 1 || slig.mHalf != door.mHalf || std::abs(slig.mY2 - door.mY1) > 40 || std::abs(slig.CentreX() - dx) > 250 || std::abs(nav.Nodes()[at].mY - door.mY1) < 40)
                {
                    continue;
                }
                DomainStep drop;
                drop.mKind = StepKind::eSligDrop;
                drop.mEntity = lever.mId;
                drop.mOther = slig.mId;
                drop.mEdge = static_cast<s32>(fi);
                const Goap::Fact down{Key::Mud(slig.mId), 2};
                // (through a door a timer opens it is not let down at all: see below)
                if (door.mSwitch == lever.mSwitch)
                add(std::string(door.mKind == FeatureKind::eSaw ? "Saw Slig#" : "Drop Slig#") + std::to_string(slig.mId) + (door.mKind == FeatureKind::eSaw ? " with the meat saw at " : " through the trap door at ") + std::to_string(dx) + "," + std::to_string(door.mY1) + " with Lever#" + std::to_string(lever.mId), {regionFact(at)}, {down}, 400, drop);
                for (Goap::Action& a : d.mActions)
                {
                    const DomainStep& s = d.mSteps[a.mTag];
                    // (where a timer opens the door, the lever is not pulled while the Slig is there to come
                    // down: it lands alive on the floor under the door, which is the lever's, and walks it from
                    // then on. The Barracks' first path: pulled as the guard at 1962,1400 turned its back, with a
                    // cell sneaked to the hoist at 2187 and up it at once, he got away; but the Slig then
                    // walks the floor the way on goes over, and every way past it measured there killed him.
                    // The walkthrough does it; the way past the guard into the well behind it needs no lever.)
                    if (door.mSwitch != lever.mSwitch && s.mKind == StepKind::eUse && s.mEntity == lever.mId)
                    {
                        a.mPre.push_back(down);
                        continue;
                    }
                    // (and no way out is taken that goes from the lever along its floor near there: where a
                    // hoist joins the lever's ledge and that floor they are one region, with nothing to cross)
                    // (a saw's only so far: a trap door's floor has always been another region)
                    if (door.mKind == FeatureKind::eSaw && s.mKind == StepKind::eExit && s.mEdge >= 0)
                    {
                        std::vector<s32> way;
                        if (nav.FindPath(at, nav.Exits()[s.mEdge].mNode, [](const NavEdge& e) -> s32
                                         { return e.mCost; },
                                         way))
                        {
                            for (const s32 e : way)
                            {
                                const NavNode& to = nav.Nodes()[nav.Edges()[e].mTo];
                                if (to.mHalf == door.mHalf && std::abs(to.mY - door.mY1) <= 30 && std::abs(to.mX - dx) <= 450)
                                {
                                    a.mPre.push_back(down);
                                    break;
                                }
                            }
                        }
                        continue;
                    }
                    if (s.mKind != StepKind::eCross || s.mEdge < 0)
                    {
                        continue;
                    }
                    const NavNode& from = nav.Nodes()[nav.Edges()[s.mEdge].mFrom];
                    const NavNode& to = nav.Nodes()[nav.Edges()[s.mEdge].mTo];
                    if (to.mHalf == door.mHalf && std::abs(to.mY - door.mY1) <= 30 && std::abs(to.mX - dx) <= 450 && std::abs(from.mY - door.mY1) > 30)
                    {
                        a.mPre.push_back(down);
                    }
                }
            }
        }
    }
    // A Brew can also be drunk and taken along: the fart it gives is then made where a flying Slig can be got at
    // from (the cells he comes into a region at, out of that Slig's own stretch and with no orb on the screen),
    // and until the Slig is dead nothing is crossed into the stretch its track goes along.
    d.mStart.Set(Key::kBrew, world.mBrew ? 1 : 0);
    {
        bool machines = false;
        for (const Entity& brew : world.mEntities)
        {
            const s32 at = brew.mKind == EntityKind::eBrew && !brew.mGone ? UseNode(nav, brew, nullptr) : -1;
            if (at < 0)
            {
                continue;
            }
            machines = true;
            DomainStep drink;
            drink.mKind = StepKind::eDrink;
            drink.mEntity = brew.mId;
            add("Drink at Brew#" + std::to_string(brew.mId), {regionFact(at), {Key::kBrew, 0}}, {{Key::kBrew, 1}}, 150, drink);
        }
        const std::size_t crossings = d.mActions.size();
        for (const Entity& flier : world.mEntities)
        {
            s32 x1 = 0;
            s32 y1 = 0;
            s32 x2 = 0;
            s32 y2 = 0;
            std::vector<std::pair<s32, s32>> ends;
            if (flier.mKind == EntityKind::eFlier && world.Switch(flier.mSwitch))
            {
                d.mStart.Set(Key::Switch(flier.mSwitch), 1);
            }
            if (!machines || flier.mKind != EntityKind::eFlier || flier.mGone || world.mFleechKilled.count(flier.mId) != 0 || !FlierTrack(nav.Input(), flier.CentreX(), (flier.mY1 + flier.mY2) / 2, x1, y1, x2, y2, &ends))
            {
                continue;
            }
            const auto inZone = [&](s32 node)
            {
                const NavNode& n = nav.Nodes()[node];
                return n.mHalf == flier.mHalf && n.mX >= x1 - 50 && n.mX <= x2 + 50 && n.mY >= y1 - 60 && n.mY <= y2 + 140;
            };
            const Goap::Fact dead{Key::Switch(flier.mSwitch), 1};
            const std::vector<NavLine> gates = FartFlight::Gates(nav.Input(), flier.mHalf, nullptr);
            std::set<s32> regions;
            for (std::size_t ai = 0; ai < crossings; ai++)
            {
                const DomainStep s = d.mSteps[d.mActions[ai].mTag];
                if (s.mKind != StepKind::eCross || s.mEdge < 0)
                {
                    continue;
                }
                const s32 node = nav.Edges()[s.mEdge].mTo;
                const NavNode& n = nav.Nodes()[node];
                if (n.mHalf != flier.mHalf || inZone(node) || !regions.insert(d.mRegions[node]).second)
                {
                    continue;
                }
                bool orb = false;
                for (const Entity& e : world.mEntities)
                {
                    orb = orb || (e.mKind == EntityKind::eSecurityOrb && !e.mGone && nav.CameraOf(e.CentreX(), (e.mY1 + e.mY2) / 2) == nav.CameraOf(n.mX, n.mY - 20));
                }
                std::vector<FartFlight::Point> corners;
                s32 length = 0;
                s32 bestLen = -1;
                for (const std::pair<s32, s32>& end : ends)
                {
                    if (!orb && FartFlight::Route(nav.Input(), n.mHalf, {n.mX, n.mY - 28}, {end.first, end.second - 20}, kFartReach / 2, corners, &length, &gates) && (bestLen < 0 || length < bestLen))
                    {
                        bestLen = length;
                    }
                }
                if (bestLen < 0)
                {
                    regions.erase(d.mRegions[node]);
                    continue;
                }
                DomainStep bomb;
                bomb.mKind = StepKind::eFartBomb;
                bomb.mEntity = flier.mId;
                bomb.mEdge = node;
                add("Fly a fart from " + std::to_string(n.mX) + "," + std::to_string(n.mY) + " to FlyingSlig#" + std::to_string(flier.mId), {regionFact(node), {Key::kBrew, 1}}, {dead, {Key::kBrew, 0}}, 500 + bestLen / 3, bomb);
            }
        }
    }

    // The fart a Brew gives can also be left by a mine: made two cells from it with his back to it, it hangs there
    // and goes off 220 frames later, by which time he is well away. For where an orb watches the screen (no
    // chanting, so no flying it): Feeco's lobby.
    for (const Entity& mine : world.mEntities)
    {
        if (mine.mKind != EntityKind::eMine || mine.mGone)
        {
            continue;
        }
        bool machines = false;
        for (const Entity& brew : world.mEntities)
        {
            machines = machines || (brew.mKind == EntityKind::eBrew && !brew.mGone);
        }
        const s32 cell = NavGraph::GridStep(mine.mHalf);
        for (s32 side = -1; machines && side <= 1; side += 2)
        {
            const s32 stand = nav.NodeAt(mine.CentreX() + side * 2 * cell, mine.mY2, mine.mHalf, 12);
            const s32 between = nav.NodeAt(mine.CentreX() + side * cell, mine.mY2, mine.mHalf, 12);
            if (stand < 0 || between < 0 || nav.Nodes()[stand].mHazard || nav.Nodes()[stand].mLine != nav.Nodes()[between].mLine)
            {
                continue;
            }
            DomainStep drop;
            drop.mKind = StepKind::eFartBomb;
            drop.mEntity = mine.mId;
            drop.mEdge = stand;
            add("Leave a fart by Mine#" + std::to_string(mine.mId) + " from " + std::to_string(nav.Nodes()[stand].mX) + "," + std::to_string(nav.Nodes()[stand].mY), {regionFact(stand), {Key::kBrew, 1}}, {{Key::Switch(mine.mSwitch), 1}, {Key::kBrew, 0}}, 450, drop);
        }
    }

    // And under a Slig that stands on a floor a little over his (Feeco's lobby: the one on the platform by the
    // door at 2162,900, from the floor 80 under it): the blast reaches 60 up from 28 over his feet. Until it is
    // dead nothing is crossed onto its floor near it, as with one a fart is flown at.
    for (const Entity& slig : world.mEntities)
    {
        bool machines = false;
        for (const Entity& brew : world.mEntities)
        {
            machines = machines || (brew.mKind == EntityKind::eBrew && !brew.mGone);
        }
        if (!machines || slig.mKind != EntityKind::eSlig || slig.mGone || slig.mSwitch > 1 || world.mFleechKilled.count(slig.mId) != 0)
        {
            continue;
        }
        s32 fy = 0;
        const s32 feet = slig.mY2 - 4;
        if (nav.FloorBelow(slig.CentreX(), feet + 20, slig.mHalf, &fy) < 0 || fy - feet < 30 || fy - feet > 84)
        {
            continue;
        }
        const s32 stand = nav.NodeAt(slig.CentreX(), fy, slig.mHalf, 14);
        if (stand < 0 || nav.Nodes()[stand].mHazard)
        {
            continue;
        }
        const Goap::Fact dead{Key::Mud(slig.mId), 2};
        for (Goap::Action& a : d.mActions)
        {
            const DomainStep& s = d.mSteps[a.mTag];
            s32 node = -1;
            if (s.mKind == StepKind::eExit && s.mEdge >= 0)
            {
                node = nav.Exits()[s.mEdge].mNode;
            }
            else if (s.mKind == StepKind::eCross && s.mEdge >= 0)
            {
                node = nav.Edges()[s.mEdge].mTo;
            }
            if (node >= 0 && nav.Nodes()[node].mHalf == slig.mHalf && std::abs(nav.Nodes()[node].mY - feet) <= 30 && std::abs(nav.Nodes()[node].mX - slig.CentreX()) <= kFartSligZone)
            {
                a.mPre.push_back(dead);
            }
        }
        DomainStep drop;
        drop.mKind = StepKind::eFartBomb;
        drop.mEntity = slig.mId;
        drop.mEdge = stand;
        add("Leave a fart under Slig#" + std::to_string(slig.mId) + " from " + std::to_string(nav.Nodes()[stand].mX) + "," + std::to_string(nav.Nodes()[stand].mY), {regionFact(stand), {Key::kBrew, 1}}, {dead, {Key::kBrew, 0}}, 450, drop);
    }

    // Sligs he can't take over (a security orb on their screen) that a flying Slig can get at: the one a spawner
    // makes is taken over from a cell of its screen where he isn't seen, flown to a place off each Slig's level
    // and its grenades thrown at them. Until one is dead nothing is crossed onto its floor near it
    // (FeatureKind::eDanger). (The Slig Barracks' path 1: "possess the flying Slig. Fly him upwards and blow up
    // the Sligs there".)
    for (const Entity& slig : world.mEntities)
    {
        if (slig.mKind == EntityKind::eSlig && world.Switch(World::HazardSwitch(slig.mId)))
        {
            d.mStart.Set(Key::Switch(World::HazardSwitch(slig.mId)), 1);
        }
    }
    // Sligs he can't take over on a floor with no shadow, in a path with a boom machine: a grenade from it is
    // thrown onto the floor of each, from each stretch of floor there is a place to throw from on. Until one is
    // dead nothing is crossed onto its floor (FeatureKind::eDanger). (The Barracks' path 1: "From the shadows on
    // the far left, toss the grenade to the middle ledge... Stand on the middle ledge and toss it to the Slig at
    // the top".)
    for (const GrenadeTarget& target : GrenadeTargets(nav.Input(), world))
    {
        const Entity* slig = world.Find(target.mSlig);
        std::set<s32> from;
        for (const GrenadeShot& shot : target.mShots)
        {
            const s32 node = slig ? nav.NodeAt(shot.mX, shot.mY, slig->mHalf, 12) : -1;
            if (node < 0 || !from.insert(d.mRegions[node]).second)
            {
                continue;
            }
            DomainStep toss;
            toss.mKind = StepKind::eGrenade;
            toss.mEntity = target.mSlig;
            toss.mEdge = node;
            // (from where he isn't seen before anywhere else)
            add("Throw a grenade at Slig#" + std::to_string(target.mSlig) + " from " + std::to_string(shot.mX) + "," + std::to_string(shot.mY), {regionFact(node)}, {{Key::Switch(World::HazardSwitch(target.mSlig)), 1}}, shot.mHidden ? 400 : 600, toss);
            d.mActions.back().mSpend = {Key::kRocks, 1};
        }
    }
    for (const Entity& spawner : world.mEntities)
    {
        if (spawner.mKind != EntityKind::eFlierSpawner)
        {
            continue;
        }
        // The cells he can get to without going onto the floor of any of those Sligs
        const std::vector<FlierTarget> targets = FlierTargets(nav.Input(), world, spawner);
        // (where he stands on the floor of one of them there is no bombing it from, but a lever or a lock the
        // flying Slig is wanted for is another matter: then nothing is kept off)
        bool onTheirs = false;
        const auto guarded = [&nav, &targets, &onTheirs](s32 node)
        {
            if (onTheirs)
            {
                return false;
            }
            for (const FlierTarget& target : targets)
            {
                if (OnFlierTargetsFloor(nav, target, node))
                {
                    return true;
                }
            }
            return false;
        };
        std::vector<u8> reach(nav.Nodes().size(), 0);
        if (abeNode < 0)
        {
            continue;
        }
        onTheirs = guarded(abeNode);
        std::vector<s32> open{abeNode};
        reach[abeNode] = 1;
        while (!open.empty())
        {
            const s32 at = open.back();
            open.pop_back();
            // (the cells of a region are walked between without an edge)
            for (std::size_t n = 0; n < nav.Nodes().size(); n++)
            {
                if (!reach[n] && d.mRegions[n] == d.mRegions[at] && !guarded(static_cast<s32>(n)))
                {
                    reach[n] = 1;
                    open.push_back(static_cast<s32>(n));
                }
            }
            for (const NavEdge& edge : nav.Edges())
            {
                // (not through what is shut as things are: the place to take it over from is one he can get to now)
                s32 sw = 0;
                s32 value = 0;
                if (edge.mFrom == at && !reach[edge.mTo] && nav.Condition(edge, &sw, &value) && world.Switch(sw) != value)
                {
                    continue;
                }
                if (edge.mFrom == at && !reach[edge.mTo] && !guarded(edge.mTo))
                {
                    reach[edge.mTo] = 1;
                    open.push_back(edge.mTo);
                }
            }
        }
        const s32 spot = FlierSpot(nav, world, spawner, reach);
        if (spot < 0)
        {
            continue;
        }
        // One step for those that can be got at as things are, and one for each that something has to be opened
        // for first
        std::vector<Goap::Fact> gives;
        std::string names;
        DomainStep bomb;
        bomb.mKind = StepKind::eFlierBomb;
        bomb.mEntity = spawner.mId;
        bomb.mEdge = spot;
        const std::string from = " with the flying Slig of FlierSpawner#" + std::to_string(spawner.mId) + " from " + std::to_string(nav.Nodes()[spot].mX) + "," + std::to_string(nav.Nodes()[spot].mY);
        for (const FlierTarget& target : targets)
        {
            if (onTheirs)
            {
                break;
            }
            // (its floor is a stretch nothing is crossed into until it is dead: GameView::BuildNavInput)
            const Goap::Fact dead{Key::Switch(World::HazardSwitch(target.mSlig)), 1};
            if (!target.mShots.empty())
            {
                gives.push_back(dead);
                names += " #" + std::to_string(target.mSlig);
                continue;
            }
            std::vector<Goap::Fact> pre{regionFact(spot)};
            for (const std::pair<s32, s32>& need : target.mNeeds)
            {
                pre.push_back({Key::Switch(need.first), need.second});
            }
            add("Bomb Slig #" + std::to_string(target.mSlig) + from + " once the way to it is open", pre, {dead}, 800, bomb);
        }
        if (!gives.empty())
        {
            add("Bomb Sligs" + names + from, {regionFact(spot)}, gives, 800, bomb);
        }
        // A lever it can fly to it pulls for him: what his own pull of it needs and gives, but for where he
        // stands. (The Barracks' path 1: "the right lever deactivates the electric walls and opens the door",
        // on a floor he can't get to.)
        std::vector<s32> which;
        const std::vector<NavLine> shut = FartFlight::Gates(nav.Input(), spawner.mHalf, &world, &which);
        const s32 fromX = spawner.CentreX();
        const s32 fromY = (spawner.mY1 + spawner.mY2) / 2;
        for (const Entity& lever : world.mEntities)
        {
            if (lever.mKind != EntityKind::eLever || lever.mGone || lever.mSwitch <= 1 || lever.mHalf != spawner.mHalf || std::abs(lever.CentreX() - fromX) + std::abs(lever.mY2 - fromY) > kFlierReach)
            {
                continue;
            }
            bool way = false;
            for (s32 side = 0; side < 2 && !way; side++)
            {
                const FlierFlight::Shot pull = FlierFlight::LeverPull(lever.CentreX(), lever.mY2 - 4, side == 1);
                std::vector<std::pair<s32, s32>> corners;
                way = FlierWay(nav.Input(), spawner.mHalf, fromX, fromY, pull.mX, pull.mY, &shut, corners);
            }
            const std::string own = "Use " + std::string(EntityKindName(lever.mKind)) + "#" + std::to_string(lever.mId) + " sw" + std::to_string(lever.mSwitch) + " o";
            const std::size_t count = d.mActions.size();
            for (std::size_t ai = 0; way && ai < count; ai++)
            {
                const DomainStep own_step = d.mSteps[d.mActions[ai].mTag];
                const std::string name = d.mActions[ai].mName;
                if (own_step.mKind != StepKind::eUse || own_step.mEntity != lever.mId || (name != own + "n" && name != own + "ff"))
                {
                    continue;
                }
                std::vector<Goap::Fact> pre{regionFact(spot)};
                for (const Goap::Fact& fact : d.mActions[ai].mPre)
                {
                    if (fact.mKey != Key::kRegion)
                    {
                        pre.push_back(fact);
                    }
                }
                DomainStep pull;
                pull.mKind = StepKind::eFlierPull;
                pull.mEntity = spawner.mId;
                pull.mOther = lever.mId;
                pull.mEdge = spot;
                const std::vector<Goap::Fact> effects = d.mActions[ai].mEffects;
                add("Have the flying Slig of FlierSpawner#" + std::to_string(spawner.mId) + " pull " + name.substr(4), pre, effects, 700, pull);
            }
        }
        // A Slig lock it can fly into the box of: it says the code back as one that walks does. (The Barracks'
        // Slig bunks: "possess the flying Slig... Talk to the Slig Lock".)
        for (const Entity& lock : world.mEntities)
        {
            if (lock.mKind != EntityKind::eSligLock || lock.mSwitch <= 1 || world.Switch(lock.mSwitch) != 0 || world.mLockCodes.count(lock.mId) == 0 || lock.mHalf != spawner.mHalf ||
                std::abs(lock.CentreX() - fromX) + std::abs((lock.mY1 + lock.mY2) / 2 - fromY) > kFlierReach)
            {
                continue;
            }
            std::vector<std::pair<s32, s32>> corners;
            if (!FlierWay(nav.Input(), spawner.mHalf, fromX, fromY, lock.CentreX(), (lock.mY1 + lock.mY2) / 2, &shut, corners))
            {
                continue;
            }
            DomainStep speak;
            speak.mKind = StepKind::eFlierLock;
            speak.mEntity = spawner.mId;
            speak.mOther = lock.mId;
            speak.mEdge = spot;
            add("Open SligLock#" + std::to_string(lock.mId) + " sw" + std::to_string(lock.mSwitch) + " with the flying Slig of FlierSpawner#" + std::to_string(spawner.mId), {regionFact(spot)}, {{Key::Switch(lock.mSwitch), 1}}, 900, speak);
        }
    }

    // A Slig lock: a Slig that Abe has taken over says its code back to it. The Slig is one a spawner makes when
    // its lever is pulled, taken over from by the lever (on its screen, with no orb there), and walked to the
    // lock (SligWay): what is shut on its way has to be open first, and the Sligs it walks into it shoots.
    // (Feeco's path 3: "pull the lever to summon a Slig, then possess him. Walk him right. Say Hi to the Slig
    // Lock, then repeat after it".)
    for (const Entity& lock : world.mEntities)
    {
        if (lock.mKind != EntityKind::eSligLock || lock.mSwitch <= 1 || world.Switch(lock.mSwitch) != 0 || world.mLockCodes.count(lock.mId) == 0)
        {
            continue;
        }
        for (const Entity& slig : world.mEntities)
        {
            if (slig.mKind != EntityKind::eSlig || slig.mGone || slig.mSwitch <= 1 || slig.mHalf != lock.mHalf)
            {
                continue;
            }
            const s32 start = SligNode(nav, slig);
            if (start < 0)
            {
                continue;
            }
            for (const Entity& lever : world.mEntities)
            {
                if (lever.mKind != EntityKind::eLever || lever.mSwitch != slig.mSwitch)
                {
                    continue;
                }
                const s32 stand = UseNode(nav, lever, nullptr);
                if (stand < 0)
                {
                    continue;
                }
                const s32 cam = nav.CameraOf(nav.Nodes()[stand].mX, nav.Nodes()[stand].mY - 5);
                bool orb = false;
                for (const Entity& e : world.mEntities)
                {
                    orb = orb || (e.mKind == EntityKind::eSecurityOrb && !e.mGone && nav.CameraOf(e.mX1, e.mY1) == cam);
                }
                if (orb || nav.CameraOf(nav.Nodes()[start].mX, nav.Nodes()[start].mY - 5) != cam)
                {
                    continue;
                }
                std::vector<Goap::Fact> pre{regionFact(stand)};
                const std::vector<s32> way = SligWay(nav, start, [&](s32 n)
                                                     { return AtSligLock(nav, lock, n); }, &pre, nullptr);
                if (way.empty() && !AtSligLock(nav, lock, start))
                {
                    continue;
                }
                // (what it opens itself is no gate on its way)
                pre.erase(std::remove(pre.begin(), pre.end(), Goap::Fact{Key::Switch(lock.mSwitch), 1}), pre.end());
                std::vector<Goap::Fact> gives{{Key::Switch(lock.mSwitch), 1}};
                // (the naked Sligs and the armed ones on the floor it walks it shoots: the floor by a locker
                // for guns that its way goes along is clear after)
                for (const Entity& locker : world.mEntities)
                {
                    bool along = false;
                    for (s32 edge : way)
                    {
                        const NavNode& n = nav.Nodes()[nav.Edges()[edge].mTo];
                        along = along || (n.mHalf == locker.mHalf && std::abs(n.mY - locker.mY2) <= 30 && std::abs(n.mX - locker.CentreX()) <= World::kLockerReach);
                    }
                    if (locker.mKind == EntityKind::ePantsLocker && along)
                    {
                        gives.push_back({Key::Switch(locker.mSwitch), 1});
                        // (and that floor is no gate on its way: it is what clears it)
                        pre.erase(std::remove(pre.begin(), pre.end(), Goap::Fact{Key::Switch(locker.mSwitch), 1}), pre.end());
                    }
                }
                for (u32 met : SligsMet(nav, world, start, way, slig.mId))
                {
                    gives.push_back({Key::Mud(met), 2});
                }
                DomainStep step;
                step.mKind = StepKind::eSligLock;
                step.mEntity = lock.mId;
                step.mOther = slig.mId;
                step.mEdge = static_cast<s32>(lever.mId);
                add("Open SligLock#" + std::to_string(lock.mId) + " sw" + std::to_string(lock.mSwitch) + " with Slig#" + std::to_string(slig.mId) + " called by Lever#" + std::to_string(lever.mId), pre, gives, 600 + 20 * static_cast<s32>(way.size()), step);
            }
        }
    }

    // A Brew machine: the fart he has after a Brew can be flown through the air to a mine or a Slig and set off
    // there (FartFlight), from where he drank it. Not with an orb on the machine's screen: it is flown by
    // chanting. And until a Slig the fart can get to is dead, nothing is crossed onto its floor near it.
    for (const Entity& brew : world.mEntities)
    {
        if (brew.mKind != EntityKind::eBrew || brew.mGone)
        {
            continue;
        }
        const s32 at = UseNode(nav, brew, nullptr);
        bool orb = false;
        for (const Entity& e : world.mEntities)
        {
            orb = orb || (e.mKind == EntityKind::eSecurityOrb && !e.mGone && e.mCamera == brew.mCamera);
        }
        if (at < 0 || orb)
        {
            continue;
        }
        const NavNode& stand = nav.Nodes()[at];
        const FartFlight::Point from{stand.mX, stand.mY - 28};
        for (const Entity& target : world.mEntities)
        {
            const bool mine = target.mKind == EntityKind::eMine;
            const bool slig = target.mKind == EntityKind::eSlig && world.mFleechKilled.count(target.mId) == 0;
            if ((!mine && !slig) || target.mGone || target.mHalf != stand.mHalf)
            {
                continue;
            }
            const FartFlight::Point to{target.CentreX(), target.mY2 - 25};
            // (not where the blast, or the mine's own, would get him)
            if (std::abs(to.mX - stand.mX) < 150 && std::abs(to.mY - stand.mY) < 150)
            {
                continue;
            }
            // Round every gate and trap door if it can be, else through the ones that can be opened, with them
            // open first
            std::vector<FartFlight::Point> corners;
            s32 length = 0;
            std::vector<Goap::Fact> pre{regionFact(at), {Key::kBrew, 0}};
            std::vector<s32> which;
            const std::vector<NavLine> gates = FartFlight::Gates(nav.Input(), stand.mHalf, nullptr, &which);
            if (!FartFlight::Route(nav.Input(), stand.mHalf, from, to, kFartReach, corners, &length, &gates))
            {
                if (!FartFlight::Route(nav.Input(), stand.mHalf, from, to, kFartReach, corners, &length))
                {
                    continue;
                }
                bool can = true;
                for (std::size_t g = 0; g < gates.size(); g++)
                {
                    if (FartFlight::Through(from, corners, gates[g]))
                    {
                        const NavFeature& f = nav.Input().mFeatures[which[g]];
                        can = can && f.mSwitch > 1;
                        pre.push_back({Key::Switch(f.mSwitch), f.mStartClosed ? 1 : 0});
                    }
                }
                if (!can)
                {
                    continue;
                }
            }
            DomainStep bomb;
            bomb.mKind = StepKind::eFartBomb;
            bomb.mEntity = target.mId;
            bomb.mOther = brew.mId;
            const Goap::Fact done = mine ? Goap::Fact{Key::Switch(target.mSwitch), 1} : Goap::Fact{Key::Mud(target.mId), 2};
            if (slig)
            {
                for (Goap::Action& a : d.mActions)
                {
                    const DomainStep& s = d.mSteps[a.mTag];
                    s32 node = -1;
                    if (s.mKind == StepKind::eExit && s.mEdge >= 0)
                    {
                        node = nav.Exits()[s.mEdge].mNode;
                    }
                    else if (s.mKind == StepKind::eCross && s.mEdge >= 0)
                    {
                        node = nav.Edges()[s.mEdge].mTo;
                    }
                    if (node >= 0 && nav.Nodes()[node].mHalf == target.mHalf && std::abs(nav.Nodes()[node].mY - target.mY2) <= 30 && std::abs(nav.Nodes()[node].mX - target.CentreX()) <= kFartSligZone)
                    {
                        a.mPre.push_back(done);
                    }
                }
            }
            add("Fly a fart from " + std::string(EntityKindName(brew.mKind)) + "#" + std::to_string(brew.mId) + " to " + EntityKindName(target.mKind) + "#" + std::to_string(target.mId), pre, {done}, 400 + length / 3, bomb);
        }
    }
    for (const Entity& slig : world.mEntities)
    {
        // (gone, or dropped through a trap door: World::mFleechKilled holds whatever was got rid of that way)
        // (or seen on a floor under the one it was put on: down a trap door whoever opened it)
        if (slig.mKind == EntityKind::eSlig && (slig.mGone || world.mFleechKilled.count(slig.mId) != 0 || (slig.mLive && (slig.mLiveY - slig.mY2) * (slig.mHalf ? 2 : 1) > 60)))
        {
            d.mStart.Set(Key::Mud(slig.mId), 2);
        }
    }

    // A foot switch that turns a gate over each time it is stepped on (the second Mudomo vault's locks, each
    // between two of them and two gates: in over one, which shuts him in with the lock, and out over the other):
    // stepping on it is a thing to do like pulling a lever
    for (const Entity& plate : world.mEntities)
    {
        if (plate.mKind != EntityKind::eFootSwitch || plate.mGone || plate.mOp != 2 || plate.mSwitch <= 1)
        {
            continue;
        }
        bool gate = false;
        for (const NavFeature& f : nav.Input().mFeatures)
        {
            gate = gate || (f.mKind == FeatureKind::eBarrier && f.mSolid && f.mSwitch == plate.mSwitch);
        }
        const s32 on = nav.NodeAt(plate.CentreX(), plate.mY2, plate.mHalf, 30);
        if (!gate || on < 0)
        {
            continue;
        }
        const s32 now = world.Switch(plate.mSwitch);
        DomainStep stepOn;
        stepOn.mKind = StepKind::eStepOn;
        stepOn.mEntity = plate.mId;
        stepOn.mEdge = on;
        add("Step on FootSwitch#" + std::to_string(plate.mId) + " sw" + std::to_string(plate.mSwitch), {regionFact(on), {Key::Switch(plate.mSwitch), now}}, {{Key::Switch(plate.mSwitch), now ? 0 : 1}, {Key::kPlate, static_cast<s32>(plate.mId)}}, 60, stepOn);
    }
    // (and a gate that is shut is opened from the one of them nearest to it: on the way from another he would
    // step on that one too, and shut it again. Measured there: in and out of the cell at 1287 four times.)
    for (Goap::Action& a : d.mActions)
    {
        const DomainStep& s = d.mSteps[a.mTag];
        if (s.mKind != StepKind::eCross || s.mEdge < 0 || nav.Edges()[s.mEdge].mFeature < 0)
        {
            continue;
        }
        const NavFeature& f = nav.Input().mFeatures[nav.Edges()[s.mEdge].mFeature];
        if (f.mKind != FeatureKind::eBarrier || !f.mSolid || f.mSwitch <= 1 || (world.Switch(f.mSwitch) == 0) != f.mStartClosed)
        {
            continue;
        }
        const Entity* nearest = nullptr;
        for (const Entity& plate : world.mEntities)
        {
            if (plate.mKind == EntityKind::eFootSwitch && !plate.mGone && plate.mOp == 2 && plate.mSwitch == f.mSwitch && plate.mHalf == f.mHalf && std::abs(plate.mY2 - f.mY2) <= 30 &&
                (!nearest || std::abs(plate.CentreX() - (f.mX1 + f.mX2) / 2) < std::abs(nearest->CentreX() - (f.mX1 + f.mX2) / 2)))
            {
                nearest = &plate;
            }
        }
        if (nearest)
        {
            a.mPre.push_back({Key::kPlate, static_cast<s32>(nearest->mId)});
        }
    }

    // Fleeches loose on a floor a Paramite walks, with no ring for it to pull: it is taken over and made to eat
    // them (the walkthrough of the second Mudomo vault: "Possess him, then go right to get the other Paramite.
    // Have him follow and attack all of the Fleeches, then bring him all the way back to the right"). Until then
    // nothing is done on those floors and no way is taken onto them.
    std::vector<Goap::Fact> eatenAll;
    std::set<s32> linesAll;
    for (const ParamiteAt& live : world.mParamites)
    {
        const s32 at = live.mPossessed || live.Hidden() || live.mId == 0 ? -1 : nav.NodeAt(live.mX, live.mY, live.mHalf, 30);
        if (at < 0 || !ParamiteTour(nav, world, at).empty())
        {
            continue;
        }
        std::vector<Goap::Fact> eaten;
        std::set<s32> lines;
        for (const Entity& f : world.mEntities)
        {
            if (f.mKind != EntityKind::eFleech || f.mGone || f.mInputs[1] != 0 || f.mHalf != live.mHalf || world.mFleechKilled.count(f.mId) != 0)
            {
                continue;
            }
            const s32 on = nav.NodeAt(f.CentreX(), f.mY2, f.mHalf, 30);
            if (on >= 0 && ParamiteRoute(nav, world, at, on, nullptr))
            {
                eaten.push_back({Key::Mud(f.mId), 2});
                lines.insert(nav.Nodes()[on].mLine);
            }
        }
        if (eaten.empty())
        {
            continue;
        }
        for (const Goap::Fact& f : eaten)
        {
            if (std::find(eatenAll.begin(), eatenAll.end(), f) == eatenAll.end())
            {
                eatenAll.push_back(f);
            }
        }
        linesAll.insert(lines.begin(), lines.end());
        const s32 cam = nav.CameraOf(live.mX, live.mY - 5);
        bool watched = false;
        for (const Entity& o : world.mEntities)
        {
            watched = watched || (o.mKind == EntityKind::eSecurityOrb && !o.mGone && nav.CameraOf(o.mX1, o.mY1) == cam);
        }
        std::set<s32> from;
        for (std::size_t n = 0; !watched && n < nav.Nodes().size(); n++)
        {
            const NavNode& spot = nav.Nodes()[n];
            // (from a place on its screen that is not on its floors, and that it is the nearest Paramite to)
            if (spot.mLow || spot.mHazard || nav.CameraOf(spot.mX, spot.mY - 5) != cam || lines.count(spot.mLine) != 0 || (spot.mHalf == live.mHalf && std::abs(spot.mY - live.mY) * (spot.mHalf ? 2 : 1) <= 45))
            {
                continue;
            }
            bool nearest = true;
            for (const ParamiteAt& other : world.mParamites)
            {
                const auto far = [&](const ParamiteAt& p)
                {
                    return (p.mX - spot.mX) * (p.mX - spot.mX) + (p.mY - spot.mY) * (p.mY - spot.mY);
                };
                nearest = nearest && (&other == &live || other.Hidden() || nav.CameraOf(other.mX, other.mY - 5) != cam || far(other) > far(live));
            }
            if (!nearest || !from.insert(d.mRegions[n]).second)
            {
                continue;
            }
            DomainStep clear;
            clear.mKind = StepKind::eParamiteClear;
            clear.mOther = live.mId;
            clear.mEdge = static_cast<s32>(n);
            add("Have Paramite#" + std::to_string(live.mId) + " eat the Fleeches of its floor from " + std::to_string(spot.mX) + "," + std::to_string(spot.mY), {regionFact(static_cast<s32>(n)), {Key::kInvisNow, 0}}, eaten, 900, clear);
        }
    }
    // What is done on those floors, and the ways onto them, wait for that
    if (!eatenAll.empty())
    {
        for (Goap::Action& a : d.mActions)
        {
            const DomainStep& s = d.mSteps[a.mTag];
            s32 where = -1;
            if (s.mKind == StepKind::eUse)
            {
                const Entity* e = world.Find(s.mEntity);
                where = e ? UseNode(nav, *e, nullptr) : -1;
            }
            else if (s.mKind == StepKind::eCross && s.mEdge >= 0 && linesAll.count(nav.Nodes()[nav.Edges()[s.mEdge].mFrom].mLine) == 0)
            {
                where = nav.Edges()[s.mEdge].mTo;
            }
            if (where >= 0 && linesAll.count(nav.Nodes()[where].mLine) != 0)
            {
                a.mPre.insert(a.mPre.end(), eatenAll.begin(), eatenAll.end());
            }
        }
    }

    // Scrabs got out of the way without a Scrab's death: lured off the end of their floor (the Mudanchee vault's
    // path 5: "hang again until the Scrab runs over to you, pull back up as he falls over the edge"), and shut in
    // behind a gate by its lever ("pull the lever when the Scrab moves to the right to trap him by the door").
    // What either makes safe is a stretch of floor (ScrabZone); whatever has to be done from a cell a Scrab can
    // get at him on now wants its stretch clear first, where there is a way to clear it.
    {
        const std::vector<LooseScrab> loose = LooseScrabs(world);
        const auto reached = [&](s32 node)
        {
            const NavNode& n = nav.Nodes()[node];
            return ScrabReaches(nav, world, n.mX, n.mY, n.mHalf);
        };
        const auto scrabsIn = [&](s32 zone)
        {
            s32 count = 0;
            for (const LooseScrab& scrab : loose)
            {
                const s32 at = nav.NodeAt(scrab.mX, scrab.mY, scrab.mHalf, 30);
                count += at >= 0 && ScrabZone(nav, at) == zone ? 1 : 0;
            }
            return count;
        };
        std::set<s32> clearable;
        std::set<s32> landings;
        std::set<s32> tops;
        for (const NavEdge& e : nav.Edges())
        {
            if (e.mKind != EdgeKind::eHoistUp || !tops.insert(e.mTo).second)
            {
                continue;
            }
            const s32 landing = LureLanding(nav, e.mTo);
            const s32 zone = ScrabZone(nav, e.mTo);
            if (landing < 0 || scrabsIn(zone) == 0 || reached(e.mFrom))
            {
                continue;
            }
            const NavNode& t = nav.Nodes()[e.mTo];
            DomainStep step;
            step.mKind = StepKind::eScrabLure;
            step.mEdge = e.mTo;
            add("Lure the Scrabs off the ledge at " + std::to_string(t.mX) + "," + std::to_string(t.mY), {regionFact(e.mFrom)}, {{Key::ScrabClear(zone), 1}, regionFact(e.mTo)}, 400, step);
            clearable.insert(zone);
            landings.insert(ScrabZone(nav, landing));
        }
        for (std::size_t fi = 0; fi < nav.Input().mFeatures.size(); fi++)
        {
            const NavFeature& f = nav.Input().mFeatures[fi];
            if (f.mKind != FeatureKind::eBarrier || !f.mSolid || f.mSwitch <= 1)
            {
                continue;
            }
            const s32 step2 = NavGraph::GridStep(f.mHalf);
            const s32 nl = nav.NodeAt(f.mX1 - step2 / 2, f.mY2, f.mHalf, 30);
            const s32 nr = nav.NodeAt(f.mX2 + step2 / 2, f.mY2, f.mHalf, 30);
            if (nl < 0 || nr < 0)
            {
                continue;
            }
            const s32 zl = ScrabZone(nav, nl);
            const s32 zr = ScrabZone(nav, nr);
            if (zl == zr || (scrabsIn(zl) + scrabsIn(zr) == 0 && !landings.count(zl) && !landings.count(zr)))
            {
                continue;
            }
            for (const Entity& lever : world.mEntities)
            {
                if (lever.mKind != EntityKind::eLever || lever.mSwitch != f.mSwitch || lever.mOp != 2)
                {
                    continue;
                }
                const s32 at = UseNode(nav, lever, nullptr);
                if (at < 0 || ScrabZone(nav, at) == zl || ScrabZone(nav, at) == zr)
                {
                    continue;
                }
                for (s32 side = 0; side < 2; side++)
                {
                    // side 1: the Scrabs left of the gate, the stretch right of it clear
                    DomainStep step;
                    step.mKind = StepKind::eScrabPen;
                    step.mEntity = lever.mId;
                    step.mOther = static_cast<u32>(fi);
                    step.mEdge = side;
                    add("Shut the Scrabs in " + std::string(side ? "left" : "right") + " of the gate at " + std::to_string((f.mX1 + f.mX2) / 2) + "," + std::to_string(f.mY2) + " with Lever#" + std::to_string(lever.mId),
                        {regionFact(at)}, {{Key::ScrabClear(side ? zr : zl), 1}, {Key::ScrabClear(side ? zl : zr), 0}, {Key::Switch(f.mSwitch), f.mStartClosed ? 0 : 1}}, 600, step);
                    clearable.insert(side ? zr : zl);
                }
            }
        }
        // As things stand: a stretch is clear when a Scrab can get at none of its cells
        for (s32 zone : clearable)
        {
            bool clear = true;
            for (std::size_t i = 0; i < nav.Nodes().size() && clear; i++)
            {
                clear = ScrabZone(nav, static_cast<s32>(i)) != zone || !reached(static_cast<s32>(i));
            }
            d.mStart.Set(Key::ScrabClear(zone), clear ? 1 : 0);
        }
        // What is done from a cell: there with no Scrab that can get at him
        for (std::size_t i = 0; i < d.mActions.size() && !clearable.empty(); i++)
        {
            const DomainStep& st = d.mSteps[d.mActions[i].mTag];
            s32 node = -1;
            switch (st.mKind)
            {
                case StepKind::eCross:
                    node = nav.Edges()[st.mEdge].mFrom;
                    break;
                case StepKind::eUse:
                case StepKind::eScrabPen:
                {
                    const Entity* entity = world.Find(st.mEntity);
                    node = entity ? UseNode(nav, *entity, nullptr) : -1;
                    break;
                }
                case StepKind::eExit:
                    node = nav.Exits()[st.mEdge].mNode;
                    break;
                default:
                    break;
            }
            if (node >= 0 && reached(node) && clearable.count(ScrabZone(nav, node)))
            {
                d.mActions[i].mPre.push_back({Key::ScrabClear(ScrabZone(nav, node)), 1});
            }
        }
    }

    return d;
}

// Everything that can be made true from the start, never mind the order or what it undoes
struct Reach final
{
    std::vector<bool> mReached;
    std::set<s32> mRegions;
    std::set<std::pair<u32, s32>> mFacts;
    const Goap::State* mStart = nullptr;

    bool Holds(const Goap::Fact& f) const
    {
        if (f.mKey == Key::kRegion)
        {
            return mRegions.count(f.mValue) != 0;
        }
        return mFacts.count({f.mKey, f.mValue}) != 0 || (f.mValue == 0 && mStart->Get(f.mKey) == 0);
    }
};

static Reach MakeReach(const Domain& d)
{
    Reach r;
    r.mStart = &d.mStart;
    r.mReached.assign(d.mActions.size(), false);
    r.mRegions.insert(d.mStart.Get(Key::kRegion));
    for (const Goap::Fact& f : d.mStart.Facts())
    {
        r.mFacts.insert({f.mKey, f.mValue});
    }
    bool grew = true;
    while (grew)
    {
        grew = false;
        for (std::size_t i = 0; i < d.mActions.size(); i++)
        {
            if (r.mReached[i])
            {
                continue;
            }
            bool ok = true;
            for (const Goap::Fact& p : d.mActions[i].mPre)
            {
                ok = ok && r.Holds(p);
            }
            if (d.mActions[i].mSpend.mKey)
            {
                bool some = false;
                for (const auto& f : r.mFacts)
                {
                    some = some || (f.first == d.mActions[i].mSpend.mKey && f.second > 0);
                }
                ok = ok && some;
            }
            if (!ok)
            {
                continue;
            }
            r.mReached[i] = true;
            grew = true;
            for (const Goap::Fact& e : d.mActions[i].mEffects)
            {
                if (e.mKey == Key::kRegion)
                {
                    r.mRegions.insert(e.mValue);
                }
                else
                {
                    r.mFacts.insert({e.mKey, e.mValue});
                }
            }
        }
    }
    return r;
}

std::vector<s32> Domain::ReachableRegions() const
{
    const Reach r = MakeReach(*this);
    return std::vector<s32>(r.mRegions.begin(), r.mRegions.end());
}

std::vector<Goap::Fact> Domain::Reachable() const
{
    const Reach r = MakeReach(*this);
    // Only what some action is waiting for: a switch nothing needs is best left alone (it may well set off a trap)
    std::vector<Goap::Fact> out;
    for (const auto& f : r.mFacts)
    {
        if (mStart.Get(f.first) == f.second)
        {
            continue;
        }
        bool wanted = false;
        for (const Goap::Action& a : mActions)
        {
            // (not by the action that only undoes it: a lever's other way)
            bool undoes = false;
            for (const Goap::Fact& e : a.mEffects)
            {
                undoes = undoes || e.mKey == f.first;
            }
            for (const Goap::Fact& p : a.mPre)
            {
                wanted = wanted || (!undoes && p.mKey == f.first && p.mValue == f.second);
            }
        }
        if (wanted)
        {
            out.push_back({f.first, f.second});
        }
    }
    return out;
}

std::string Domain::Frontier() const
{
    const Reach r = MakeReach(*this);
    s32 cells = 0;
    for (s32 region : mRegions)
    {
        cells += r.mRegions.count(region + 1) ? 1 : 0;
    }
    std::string out = "in reach: " + std::to_string(r.mRegions.size()) + " regions, " + std::to_string(cells) + " of " + std::to_string(mRegions.size()) + " cells.";
    // What stands at the edge of that
    s32 listed = 0;
    for (std::size_t i = 0; i < mActions.size() && listed < 8; i++)
    {
        if (r.mReached[i] || mActions[i].mPre.empty() || !r.Holds(mActions[i].mPre[0]))
        {
            continue;
        }
        for (const Goap::Fact& p : mActions[i].mPre)
        {
            if (!r.Holds(p))
            {
                out += " Blocked: " + mActions[i].mName + " needs " + (p.mKey >= 1000000 ? "Mudokon#" + std::to_string(p.mKey - 1000000) : p.mKey >= 1000 ? "switch " + std::to_string(p.mKey - 1000) : "fact " + std::to_string(p.mKey)) + "=" + std::to_string(p.mValue) + ".";
                listed++;
            }
        }
    }
    return out;
}

SideTrip Domain::WeighSideTrip(const NavGraph& nav, const World& world, s32 entryNode, const std::function<bool(const NavFeature&)>& leadsOn)
{
    SideTrip trip;
    // Everything that can be got to, shut doors and all: what opens them isn't looked into from here
    const std::vector<s32> costs = nav.CostsFrom(entryNode, [](const NavEdge& e)
                                                 { return e.mCost; });
    const auto inReach = [&costs](s32 node)
    {
        return node >= 0 && costs[node] >= 0;
    };
    bool portal[2] = {false, false};
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind == EntityKind::eBirdPortal && e.mRescuePortal && inReach(UseNode(nav, e, nullptr)))
        {
            portal[e.mHalf ? 1 : 0] = true;
        }
    }
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eMudokon || e.mGone)
        {
            continue;
        }
        trip.mThere++;
        if (inReach(UseNode(nav, e, nullptr)))
        {
            trip.mMudokons++;
            trip.mRescuable += portal[e.mHalf ? 1 : 0] ? 1 : 0;
        }
    }
    for (const NavExit& x : nav.Exits())
    {
        if ((x.mNode < 0 || inReach(x.mNode)) && (!leadsOn || leadsOn(nav.Input().mFeatures[x.mFeature])))
        {
            trip.mWayBack = true;
        }
    }
    return trip;
}

std::string Domain::Text() const
{
    std::string out;
    for (const Goap::Action& a : mActions)
    {
        out += a.mName + " cost " + std::to_string(a.mCost) + " needs";
        for (const Goap::Fact& f : a.mPre)
        {
            out += " " + std::to_string(f.mKey) + "=" + std::to_string(f.mValue);
        }
        out += " gives";
        for (const Goap::Fact& f : a.mEffects)
        {
            out += " " + std::to_string(f.mKey) + "=" + std::to_string(f.mValue);
        }
        out += "\n";
    }
    return out;
}

s32 Domain::FlierSpot(const NavGraph& nav, const World& world, const Entity& spawner, const std::vector<u8>& reach)
{
    const s32 cam = nav.CameraOf(spawner.CentreX(), (spawner.mY1 + spawner.mY2) / 2);
    // Where he isn't seen, if there is such a cell; else the cell furthest from the track it flies along (the
    // Barracks' path 1: the ledge at 2187,860, which is where the walkthrough takes it over from)
    s32 x1 = 0;
    s32 y1 = 0;
    s32 x2 = 0;
    s32 y2 = 0;
    std::vector<NavLine> track;
    FlierTrack(nav.Input(), spawner.CentreX(), (spawner.mY1 + spawner.mY2) / 2, x1, y1, x2, y2, nullptr, &track);
    // A flying Slig that is there already flies a track of its own, and is taken over from any screen that
    // goes through: the one where he is furthest from it. (The Barracks' path 7: the one at 1424,598 comes down
    // to 838 over the floor at 980, and the walkthrough has him "go right all the way and possess" it there;
    // from the ledge at 620 by its lock, 40 under its track, it had him every time.)
    std::set<s32> cams{cam};
    // (and the one its spawner makes, or one that has come by its wings on that track, where that track goes)
    for (const NavLine& line : track)
    {
        cams.insert(nav.CameraOf(line.mX1, line.mY1));
        cams.insert(nav.CameraOf(line.mX2, line.mY2));
    }
    for (const Entity& flier : world.mEntities)
    {
        if (flier.mKind != EntityKind::eFlier || flier.mGone || flier.mHalf != spawner.mHalf)
        {
            continue;
        }
        std::vector<NavLine> own;
        FlierTrack(nav.Input(), flier.CentreX(), (flier.mY1 + flier.mY2) / 2, x1, y1, x2, y2, nullptr, &own);
        for (const NavLine& line : own)
        {
            cams.insert(nav.CameraOf(line.mX1, line.mY1));
            cams.insert(nav.CameraOf(line.mX2, line.mY2));
            track.push_back(line);
        }
    }
    // (and one that is about, wherever it has come from: a naked Slig that came by wings at a locker. Where it
    // is counts as its track. The Barracks' path 7: the one that hangs at 1417,828 over the floor at 980.)
    for (const FlierAt& f : world.mFliers)
    {
        if (f.mHalf == spawner.mHalf)
        {
            cams.insert(nav.CameraOf(f.mX, f.mY - 20));
            track.push_back(NavLine{f.mX, f.mY, f.mX, f.mY, 8});
        }
    }
    // (or is still to come by them: the screen of the locker for wings is one a flying Slig comes to be on. He
    // waits there for it, as the walkthrough has it: "he'll be back and you'll get him then".)
    for (const RockAt& locker : world.mWingLockers)
    {
        cams.insert(nav.CameraOf(locker.mX, locker.mY - 20));
        track.push_back(NavLine{locker.mX, locker.mY - 40, locker.mX, locker.mY - 40, 8});
    }
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind == EntityKind::eSecurityOrb && !e.mGone)
        {
            cams.erase(nav.CameraOf(e.mX1, e.mY1));
        }
    }
    if (cams.empty())
    {
        return -1;
    }
    s32 best = -1;
    s64 bestFar = -1;
    for (std::size_t n = 0; n < nav.Nodes().size(); n++)
    {
        const NavNode& node = nav.Nodes()[n];
        if (!reach[n] || node.mHazard || node.mLow || node.mHalf != spawner.mHalf || node.mTrapDoor >= 0 || node.mLiftStop >= 0 || cams.count(nav.CameraOf(node.mX, node.mY - 5)) == 0)
        {
            continue;
        }
        if (node.mHidden)
        {
            return static_cast<s32>(n);
        }
        s64 far = 1000000;
        for (const NavLine& line : track)
        {
            const s64 lx = line.mX2 - line.mX1;
            const s64 ly = line.mY2 - line.mY1;
            const s64 sq = lx * lx + ly * ly;
            s64 t1000 = sq == 0 ? 0 : ((node.mX - line.mX1) * lx + (node.mY - 40 - line.mY1) * ly) * 1000 / sq;
            t1000 = std::max<s64>(0, std::min<s64>(1000, t1000));
            const s64 dx = line.mX1 + lx * t1000 / 1000 - node.mX;
            const s64 dy = line.mY1 + ly * t1000 / 1000 - (node.mY - 40);
            far = std::min(far, dx * dx + dy * dy);
        }
        if (far > bestFar)
        {
            bestFar = far;
            best = static_cast<s32>(n);
        }
    }
    return best;
}

bool Domain::OnFlierTargetsFloor(const NavGraph& nav, const FlierTarget& target, s32 node)
{
    const NavNode& n = nav.Nodes()[node];
    return std::abs(n.mY - target.mFloorY) <= kFlierTargetY && std::abs(n.mX - target.mX) <= kFlierTargetX;
}

bool Domain::FlierWay(const NavInput& in, bool half, s32 fromX, s32 fromY, s32 toX, s32 toY, const std::vector<NavLine>* shut, std::vector<std::pair<s32, s32>>& corners)
{
    corners.clear();
    // It is 17 to each side of its point: every floor and ceiling as much longer at each end, every wall there
    // again as far to each side, so that the way keeps its sides clear of their ends (measured in the Barracks'
    // path 1: flown up past the end of a ledge with its point two pixels clear of it, it hung under the ledge)
    std::vector<NavLine> more;
    if (shut)
    {
        more = *shut;
    }
    for (const NavLine& line : in.mLines)
    {
        const bool flat = std::abs(line.mX2 - line.mX1) >= std::abs(line.mY2 - line.mY1);
        const s32 floorTypes[] = {half ? 4 : 0, half ? 7 : 3, half ? 18 : 17, half ? 36 : 32};
        const s32 wallTypes[] = {half ? 5 : 1, half ? 6 : 2, half ? 18 : 17};
        if (flat && std::find(std::begin(floorTypes), std::end(floorTypes), line.mType) != std::end(floorTypes))
        {
            NavLine wide = line;
            (wide.mX1 < wide.mX2 ? wide.mX1 : wide.mX2) -= kFlierSide;
            (wide.mX1 < wide.mX2 ? wide.mX2 : wide.mX1) += kFlierSide;
            wide.mType = 0;
            more.push_back(wide);
            // (and it is 23 over its point and 8 under: not in sideways past the end of a floor at the floor's
            // own height. Between the ends of two floors 26 apart, one 20 higher than the other, the way went
            // on the grid and the flying Slig did not.)
            // (a little nearer the floor's end than the floor is made longer by: where a grenade is thrown
            // from under the end of a floor it is right at that)
            const s32 lo = std::min(wide.mX1, wide.mX2);
            const s32 hi = std::max(wide.mX1, wide.mX2);
            more.push_back(NavLine{lo + 3, (lo == wide.mX1 ? wide.mY1 : wide.mY2) - kFlierUnder, lo + 3, (lo == wide.mX1 ? wide.mY1 : wide.mY2) + kFlierOver, 1});
            more.push_back(NavLine{hi - 3, (hi == wide.mX1 ? wide.mY1 : wide.mY2) - kFlierUnder, hi - 3, (hi == wide.mX1 ? wide.mY1 : wide.mY2) + kFlierOver, 1});
        }
        else if (!flat && std::find(std::begin(wallTypes), std::end(wallTypes), line.mType) != std::end(wallTypes))
        {
            for (const s32 side : {-kFlierSide, kFlierSide})
            {
                NavLine beside = line;
                beside.mX1 += side;
                beside.mX2 += side;
                beside.mType = 1;
                more.push_back(beside);
            }
        }
    }
    // Where it starts may be nearer a wall than it flies by one (where its spawner puts it, by the wall of its
    // shaft: the Barracks' path 7, at 1436 with the wall at 1453): from there it comes away from the wall first
    for (const NavLine& line : in.mLines)
    {
        const bool flat = std::abs(line.mX2 - line.mX1) >= std::abs(line.mY2 - line.mY1);
        const s32 wallTypes[] = {half ? 5 : 1, half ? 6 : 2, half ? 18 : 17};
        if (!flat && std::find(std::begin(wallTypes), std::end(wallTypes), line.mType) != std::end(wallTypes) && fromY >= std::min(line.mY1, line.mY2) && fromY <= std::max(line.mY1, line.mY2) &&
            std::abs(fromX - line.mX1) <= kFlierSide)
        {
            fromX = line.mX1 + (fromX < line.mX1 ? -(kFlierSide + 1) : kFlierSide + 1);
        }
    }
    std::vector<FartFlight::Point> way;
    if (!FartFlight::Route(in, half, FartFlight::Point{fromX, fromY}, FartFlight::Point{toX, toY}, 2000, way, nullptr, &more))
    {
        // The way is looked for on a grid 25 wide, and a gap it fits through with little to spare may lie
        // between two of the grid's columns: looked for again with everything a little to one side. (The
        // Barracks' path 7: down the shaft between the end of the floor at 1400 and the wall at 1450, which
        // leaves its point ten pixels, 1420 to 1430: the walkthrough's "fly your Slig all the way to the bottom".)
        bool found = false;
        for (const s32 shift : {12, 6, 18})
        {
            NavInput moved = in;
            for (NavLine& line : moved.mLines)
            {
                line.mX1 += shift;
                line.mX2 += shift;
            }
            std::vector<NavLine> movedMore = more;
            for (NavLine& line : movedMore)
            {
                line.mX1 += shift;
                line.mX2 += shift;
            }
            if (FartFlight::Route(moved, half, FartFlight::Point{fromX + shift, fromY}, FartFlight::Point{toX + shift, toY}, 2000, way, nullptr, &movedMore))
            {
                for (FartFlight::Point& p : way)
                {
                    p.mX -= shift;
                }
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    std::set<std::pair<s32, s32>> screens;
    for (const NavLine& line : in.mLines)
    {
        // (its ends, each a pixel in along it: a floor that ends on the side of a screen is not in the next one)
        const s32 dx = line.mX2 > line.mX1 ? 1 : line.mX2 < line.mX1 ? -1 : 0;
        const s32 dy = line.mY2 > line.mY1 ? 1 : line.mY2 < line.mY1 ? -1 : 0;
        screens.insert({(line.mX1 + dx) / in.mCamW, (line.mY1 + dy) / in.mCamH});
        screens.insert({(line.mX2 - dx) / in.mCamW, (line.mY2 - dy) / in.mCamH});
    }
    for (const FartFlight::Point& p : way)
    {
        if (screens.count({p.mX / in.mCamW, p.mY / in.mCamH}) == 0)
        {
            return false;
        }
        corners.push_back({p.mX, p.mY});
    }
    return true;
}

std::vector<Domain::GrenadeTarget> Domain::GrenadeTargets(const NavInput& in, const World& world)
{
    std::vector<GrenadeTarget> targets;
    if (!in.mIsAe)
    {
        return targets;
    }
    const auto screen = [&in](s32 x, s32 y)
    {
        return std::make_pair(x / in.mCamW, y / in.mCamH);
    };
    // (as NavGraph has it: all of him in the zone)
    const auto hidden = [&in](s32 x, s32 y, bool half)
    {
        for (const NavFeature& f : in.mFeatures)
        {
            if (f.mKind == FeatureKind::eInvisibleZone && x - (half ? 13 : 25) >= f.mX1 && x + (half ? 13 : 25) <= f.mX2 && y >= f.mY1 + 40 && y <= f.mY2 + 4)
            {
                return true;
            }
        }
        return false;
    };
    for (const Entity& slig : world.mEntities)
    {
        if (slig.mKind != EntityKind::eSlig || slig.mGone || slig.mSwitch > 1 || world.mFleechKilled.count(slig.mId) != 0)
        {
            continue;
        }
        bool machine = false;
        bool orb = false;
        for (const Entity& e : world.mEntities)
        {
            machine = machine || (e.mKind == EntityKind::eRockSack && e.mInputs[1] == 2 && !e.mGone && e.mHalf == slig.mHalf);
            orb = orb || (e.mKind == EntityKind::eSecurityOrb && !e.mGone && screen(e.mX1, e.mY1) == screen(slig.CentreX(), slig.mY1));
        }
        if (!machine || !orb)
        {
            continue;
        }
        // The floor it stands on: the first under the top of where the map puts it
        const s32 floorType = slig.mHalf ? 4 : 0;
        const NavLine* floor = nullptr;
        for (const NavLine& line : in.mLines)
        {
            if (line.mType == floorType && line.mY1 == line.mY2 && std::min(line.mX1, line.mX2) <= slig.CentreX() && std::max(line.mX1, line.mX2) >= slig.CentreX() && line.mY1 >= slig.mY1 + 5 && line.mY1 - slig.mY1 <= 60 && (!floor || line.mY1 < floor->mY1))
            {
                floor = &line;
            }
        }
        if (!floor)
        {
            continue;
        }
        GrenadeTarget target;
        target.mSlig = slig.mId;
        target.mFloorY = floor->mY1;
        const s32 camX = (slig.CentreX() / in.mCamW) * in.mCamW;
        target.mX1 = std::max(std::min(floor->mX1, floor->mX2), camX);
        target.mX2 = std::min(std::max(floor->mX1, floor->mX2), camX + in.mCamW);
        // A shadow on it: gone by unseen, as such floors always were
        bool shadow = false;
        const s32 cell = NavGraph::GridStep(slig.mHalf);
        for (s32 x = target.mX1 + cell / 2; x < target.mX2 && !shadow; x += cell)
        {
            shadow = hidden(x, target.mFloorY, slig.mHalf);
        }
        if (shadow)
        {
            continue;
        }
        for (const NavLine& line : in.mLines)
        {
            if (line.mType != floorType || line.mY1 != line.mY2 || &line == floor || std::abs(line.mY1 - target.mFloorY) > 200)
            {
                continue;
            }
            const s32 lo = std::max(std::min(line.mX1, line.mX2), target.mX1 - 300);
            const s32 hi = std::min(std::max(line.mX1, line.mX2), target.mX2 + 300);
            for (s32 x = (lo / cell) * cell + cell / 2; x + cell / 2 <= hi; x += cell)
            {
                if (x - cell / 2 < lo)
                {
                    continue;
                }
                for (s32 way = 0; way < 6; way++)
                {
                    GrenadeShot shot;
                    shot.mX = x;
                    shot.mY = line.mY1;
                    shot.mFaceLeft = way % 2 == 1;
                    shot.mKind = way / 2 == 0 ? ThrowSpot::eLob : way / 2 == 1 ? ThrowSpot::eUp : ThrowSpot::eForward;
                    // Where it goes off, and from a little to either side of the cell's middle much the same:
                    // he does not stand to the pixel
                    bool good = true;
                    s32 ends[3] = {};
                    for (s32 k = 0; k < 3 && good; k++)
                    {
                        const ThrowArc::Flight flight = ThrowArc::Grenade(in, x + (k - 1) * 2, line.mY1, slig.mHalf, shot.mFaceLeft, shot.mKind);
                        const ThrowArc::Point& end = flight.mPoints.back();
                        good = !flight.mUnder && std::abs(end.mY - target.mFloorY) <= 3 && end.mX >= target.mX1 && end.mX <= target.mX2;
                        ends[k] = end.mX;
                    }
                    good = good && std::abs(ends[0] - ends[1]) <= 15 && std::abs(ends[2] - ends[1]) <= 15;
                    shot.mLandX = ends[1];
                    if (!good || std::abs(shot.mLandX - x) * (slig.mHalf ? 2 : 1) < ThrowArc::kGrenadeKillX)
                    {
                        continue;
                    }
                    shot.mHidden = hidden(x, line.mY1, slig.mHalf);
                    target.mShots.push_back(shot);
                }
            }
        }
        std::stable_sort(target.mShots.begin(), target.mShots.end(), [](const GrenadeShot& a, const GrenadeShot& b)
                         { return a.mHidden && !b.mHidden; });
        if (!target.mShots.empty())
        {
            targets.push_back(target);
        }
    }
    return targets;
}

std::vector<Domain::FlierTarget> Domain::FlierTargets(const NavInput& in, const World& world, const Entity& spawner)
{
    std::vector<FlierTarget> targets;
    const FartFlight::Point from{spawner.CentreX(), (spawner.mY1 + spawner.mY2) / 2};
    // What is shut as things are (gates, electric walls, trap doors), and which feature each is
    std::vector<s32> which;
    const std::vector<NavLine> shut = FartFlight::Gates(in, spawner.mHalf, &world, &which);
    const auto screen = [&in](s32 x, s32 y)
    {
        return std::make_pair(x / in.mCamW, y / in.mCamH);
    };
    for (const Entity& slig : world.mEntities)
    {
        if (slig.mKind != EntityKind::eSlig || slig.mGone || slig.mSwitch > 1 || slig.mHalf != spawner.mHalf || world.mFleechKilled.count(slig.mId) != 0)
        {
            continue;
        }
        bool orb = false;
        for (const Entity& e : world.mEntities)
        {
            orb = orb || (e.mKind == EntityKind::eSecurityOrb && !e.mGone && screen(e.mX1, e.mY1) == screen(slig.CentreX(), slig.mY1));
        }
        // The floor it stands on: the first under the top of where the map puts it
        s32 floorY = -1;
        s32 floorX1 = 0;
        s32 floorX2 = 0;
        for (const NavLine& line : in.mLines)
        {
            if (line.mType == (slig.mHalf ? 4 : 0) && std::min(line.mX1, line.mX2) <= slig.CentreX() && std::max(line.mX1, line.mX2) >= slig.CentreX() && line.mX1 != line.mX2)
            {
                const s32 y = line.mY1 + (line.mY2 - line.mY1) * (slig.CentreX() - line.mX1) / (line.mX2 - line.mX1);
                if (y >= slig.mY1 + 5 && (floorY < 0 || y < floorY))
                {
                    floorY = y;
                    floorX1 = std::min(line.mX1, line.mX2);
                    floorX2 = std::max(line.mX1, line.mX2);
                }
            }
        }
        // (and within a flight of the spawner: a way through the air is not looked for to the other end of the path)
        if (!orb || floorY < 0 || floorY - slig.mY1 > 60 || std::abs(slig.CentreX() - from.mX) + std::abs(floorY - from.mY) > kFlierReach)
        {
            continue;
        }
        FlierTarget target;
        target.mSlig = slig.mId;
        target.mX = slig.CentreX();
        target.mFloorY = floorY;
        bool needsKnown = false;
        for (const FlierFlight::Shot& shot : FlierFlight::Shots(target.mX, floorY, floorX1, floorX2))
        {
            // Nothing of the path in the grenade's way until it comes down, nor in the flying Slig's way there.
            // What is shut and a switch opens (a trap door the grenade would be thrown up under, an electric
            // wall on the way or right by the place: one that is on also gets a flying Slig that only comes
            // near it) is to be open first.
            bool can = world.mNoBomb.count(slig.mId) == 0;
            std::vector<u8> gateNeeded(shut.size(), 0);
            s32 px = shot.mX;
            s32 py = shot.mY - FlierFlight::kGrenadeUp;
            for (s32 frame = 1; can && frame < shot.mFrames; frame++)
            {
                s32 gx = 0;
                s32 gy = 0;
                FlierFlight::GrenadeAt(shot.mX, shot.mY, shot.mFaceLeft, frame, gx, gy);
                can = FartFlight::Open(in, slig.mHalf, FartFlight::Point{px, py}, FartFlight::Point{gx, gy});
                for (std::size_t g = 0; can && g < shut.size(); g++)
                {
                    const std::vector<NavLine> one{shut[g]};
                    gateNeeded[g] = gateNeeded[g] || !FartFlight::Open(in, slig.mHalf, FartFlight::Point{px, py}, FartFlight::Point{gx, gy}, &one);
                }
                px = gx;
                py = gy;
            }
            // (and on the Slig's screen: the game has only the screen the flying Slig is on, and a grenade from
            // the next one finds no Slig. Its box is drawn from 23 over its point.)
            can = can && screen(shot.mX, shot.mY - 25) == screen(slig.CentreX(), slig.mY1) && screen(shot.mWaitX, shot.mWaitY - 25) == screen(slig.CentreX(), slig.mY1);
            // (and from where it waits to where it throws nothing in its way, with its sides: under a floor that
            // goes on over the place it would wait at, it does not get up to throw)
            for (const s32 side : {-kFlierSide, 0, kFlierSide})
            {
                can = can && FartFlight::Open(in, slig.mHalf, FartFlight::Point{shot.mWaitX + side, shot.mWaitY}, FartFlight::Point{shot.mX + side, shot.mY - kFlierSide});
            }
            std::vector<std::pair<s32, s32>> corners;
            if (can && !FlierWay(in, slig.mHalf, from.mX, from.mY, shot.mWaitX, shot.mWaitY, &shut, corners))
            {
                can = FlierWay(in, slig.mHalf, from.mX, from.mY, shot.mWaitX, shot.mWaitY, nullptr, corners);
                std::vector<FartFlight::Point> way;
                for (const std::pair<s32, s32>& corner : corners)
                {
                    way.push_back({corner.first, corner.second});
                }
                for (std::size_t g = 0; can && g < shut.size(); g++)
                {
                    gateNeeded[g] = gateNeeded[g] || FartFlight::Through(from, way, shut[g]);
                }
            }
            std::vector<std::pair<s32, s32>> needs;
            for (std::size_t g = 0; can && g < shut.size(); g++)
            {
                const NavFeature& f = in.mFeatures[which[g]];
                const bool near = f.mKind == FeatureKind::eBarrier && std::abs(shut[g].mX1 - shot.mX) <= 45 && std::min(shot.mY, shot.mWaitY) <= shut[g].mY2 + 40 && std::max(shot.mY, shot.mWaitY) >= shut[g].mY1 - 20;
                if (near || gateNeeded[g])
                {
                    can = f.mSwitch > 1;
                    needs.push_back({f.mSwitch, f.mStartClosed ? 1 : 0});
                }
            }
            if (!can)
            {
                continue;
            }
            if (needs.empty())
            {
                target.mShots.push_back(shot);
            }
            else if (!needsKnown)
            {
                needsKnown = true;
                target.mNeeds = needs;
            }
        }
        if (!target.mShots.empty())
        {
            target.mNeeds.clear();
        }
        if (!target.mShots.empty() || needsKnown)
        {
            targets.push_back(target);
        }
    }
    return targets;
}

} // namespace Bot
