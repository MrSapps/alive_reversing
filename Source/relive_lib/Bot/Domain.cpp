#include "Domain.hpp"
#include "ThrowArc.hpp"
#include <algorithm>
#include <cstdlib>
#include <set>
#include <tuple>

namespace Bot {

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
        case EntityKind::eMine: return "Mine";
        case EntityKind::eRockSack: return "RockSack";
        case EntityKind::eDrill: return "Drill";
        case EntityKind::eMineCar: return "MineCar";
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
        return world.LiftAtStop(stop->mX, stop->mY, f.mA != 0) || (!now && (f.mSwitch > 1 || e.mKind == EdgeKind::eLift));
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

bool Domain::WallBetween(const NavGraph& nav, s32 x1, s32 x2, s32 y, bool half)
{
    const s32 scale = half ? 2 : 1;
    for (const NavLine& line : nav.Input().mLines)
    {
        const bool wall = half ? (line.mType == 5 || line.mType == 6) : (line.mType == 1 || line.mType == 2);
        if (wall && line.mX1 == line.mX2 && line.mX1 > std::min(x1, x2) && line.mX1 < std::max(x1, x2) && std::min(line.mY1, line.mY2) <= y - 20 / scale && std::max(line.mY1, line.mY2) >= y - 5)
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

    // Mudokons walk and drop off ledges, nothing more
    const NavGraph::EdgeCost mudCost = [](const NavEdge& e)
    {
        return e.mKind == EdgeKind::eWalk || e.mKind == EdgeKind::eFall ? e.mCost : -1;
    };
    struct Candidate final
    {
        const Entity* mMud;
        s32 mCost;
        std::vector<s32> mPath;
    };
    std::vector<Candidate> candidates;
    const s32 firstWheel = UseNode(nav, *wheels[0], nullptr);
    for (const Entity& e : world.mEntities)
    {
        if (e.mKind != EntityKind::eMudokon || e.mGone)
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
    if (candidates.size() + 1 < wheels.size())
    {
        return false;
    }
    candidates.resize(wheels.size() - 1);

    // Abe's wheel: the one furthest from where the Mudokons come from, so they pass the others first
    const s32 fromX = candidates[0].mMud->CentreX();
    const Entity* abeWheel = wheels[0];
    for (const Entity* w : wheels)
    {
        if (std::abs(w->CentreX() - fromX) > std::abs(abeWheel->CentreX() - fromX))
        {
            abeWheel = w;
        }
        team.mWheels.push_back(w->mId);
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

// What a throw through a running drill is taken to cost over one with nothing in the way: more than going round
// to its lever
static const s32 kPastDrillCost = 2500;
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
        if (world.mSwitches[i])
        {
            d.mStart.Set(Key::Switch(i), 1);
        }
    }
    if (abeNode >= 0)
    {
        d.mStart.Set(Key::kRegion, d.mRegions[abeNode] + 1);
    }
    d.mStart.Set(Key::kRocks, world.mRocks);
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
        if (both)
        {
            add("Cross " + std::string(EdgeKindName(e.mKind)) + " " + std::to_string(nav.Nodes()[e.mFrom].mX) + "," + std::to_string(nav.Nodes()[e.mFrom].mY) + "->" + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY),
                other, {regionFact(e.mTo)}, 100 + e.mCost + unseen(other), step);
        }
        add("Cross " + std::string(EdgeKindName(e.mKind)) + " " + std::to_string(nav.Nodes()[e.mFrom].mX) + "," + std::to_string(nav.Nodes()[e.mFrom].mY) + "->" + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY),
            pre, {regionFact(e.mTo)}, 100 + e.mCost + (both ? unseen(pre) : 0), step);
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
                // Or led off: got to follow him away from it and come back to by another way (the walkthrough's
                // way with the first of the Mudanchee vaults)
                const FleechLure lure = lures.count(entity.mId) ? lures[entity.mId] : FleechLure();
                if (entity.mKind == EntityKind::eSlapLock && entity.mInputs[0] > 0 && node >= 0 && !world.mInvisEver && entity.mSwitch <= 1)
                {
                    step.mKind = StepKind::eUse;
                    std::vector<Goap::Fact> pre{regionFact(node), {Key::kInvis, 0}};
                    pre.insert(pre.end(), guards.begin(), guards.end());
                    add("Get invisibility from " + name, pre, {{Key::kInvis, 1}, {Key::kInvisNow, 1}}, 150, step);
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
                    add("Get rocks from " + name, {regionFact(node), {Key::kRocks, 0}}, {{Key::kRocks, entity.mInputs[0] > 0 ? entity.mInputs[0] : 5}}, 300, step);
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
                            if (!world.LiftAtStop(onto.mX, onto.mY, stop.mA != 0) && stop.mSwitch > 1)
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
        // Fleeches asleep by the way out that a Scrab can be had to kill: dead first (they wake as he comes up)
        for (const Entity& fl : world.mEntities)
        {
            const NavNode& at = nav.Nodes()[x.mNode];
            if (fl.mKind == EntityKind::eFleech && huntable.count(fl.mId) != 0 && fl.mHalf == at.mHalf && std::abs(fl.mY2 - at.mY) * (at.mHalf ? 2 : 1) <= 30 && std::abs(fl.CentreX() - at.mX) * (at.mHalf ? 2 : 1) <= 100)
            {
                pre.push_back({Key::Mud(fl.mId), 2});
            }
        }
        add("Exit " + std::to_string(nav.Nodes()[x.mNode].mX) + "," + std::to_string(nav.Nodes()[x.mNode].mY) + " to path " + std::to_string(f.mDestPath), pre, {{Key::kExited, 1}}, 200, step);
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

} // namespace Bot
