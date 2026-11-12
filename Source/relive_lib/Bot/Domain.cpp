#include "Domain.hpp"
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
                    if (node >= 0 && i == 0 && !into)
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
    struct Try final
    {
        ThrowSpot::Kind mKind;
        s32 mLevel;
        s32 mCells;
    };
    // In the order that has worked in the game: an up throw from four cells off under a ledge (measured on the
    // Mines' path 4, where five and six fell short), a throw at the floor from five cells along it (the blast
    // reaches four and a half), a crouched one from six, a forward one from ten
    static const Try kTries[] = {
        {ThrowSpot::eUp, -1, 4}, {ThrowSpot::eUp, -1, 5}, {ThrowSpot::eUp, -1, 3}, {ThrowSpot::eLob, -1, 3}, {ThrowSpot::eUp, -1, 6}, {ThrowSpot::eLob, -1, 2}, {ThrowSpot::eUp, -1, 7}, {ThrowSpot::eLob, -1, 4}, {ThrowSpot::eUp, -1, 2},
        {ThrowSpot::eDown, 0, 5}, {ThrowSpot::eCrouched, 0, 6}, {ThrowSpot::eForward, 0, 10}, {ThrowSpot::eDown, 0, 6}, {ThrowSpot::eCrouched, 0, 7}, {ThrowSpot::eDown, 0, 7}, {ThrowSpot::eCrouched, 0, 8}, {ThrowSpot::eDown, 0, 8}, {ThrowSpot::eCrouched, 0, 9}, {ThrowSpot::eForward, 0, 11}, {ThrowSpot::eForward, 0, 12}, {ThrowSpot::eCrouched, 0, 5}, {ThrowSpot::eDown, 0, 4},
        {ThrowSpot::eDown, 1, 5}, {ThrowSpot::eDown, 1, 6}, {ThrowSpot::eForward, 1, 8}, {ThrowSpot::eDown, 1, 4}, {ThrowSpot::eDown, 1, 7}, {ThrowSpot::eForward, 1, 7}, {ThrowSpot::eForward, 1, 9}, {ThrowSpot::eDown, 1, 3},
        // (from above the rock bounces off the ledge's edge and the floor below: what hits is found by trying, and
        // a miss is taken back, so the list is long)
        {ThrowSpot::eCrouched, 1, 4}, {ThrowSpot::eCrouched, 1, 5}, {ThrowSpot::eCrouched, 1, 3}, {ThrowSpot::eCrouched, 1, 6}, {ThrowSpot::eDown, 1, 2}, {ThrowSpot::eCrouched, 1, 2}, {ThrowSpot::eCrouched, 1, 7}, {ThrowSpot::eUp, 1, 6}, {ThrowSpot::eUp, 1, 7}, {ThrowSpot::eUp, 1, 5}, {ThrowSpot::eUp, 1, 8}, {ThrowSpot::eForward, 1, 6}, {ThrowSpot::eForward, 1, 10}, {ThrowSpot::eUp, 1, 4}, {ThrowSpot::eCrouched, 1, 8}, {ThrowSpot::eForward, 1, 5},
    };
    const s32 scale = half ? 2 : 1;
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
        // Crouching at the top of a hoist is climbing down it
        bool hoistTop = false;
        for (s32 e = n.mFirstEdge; e < n.mFirstEdge + n.mEdgeCount; e++)
        {
            hoistTop = hoistTop || nav.Edges()[e].mKind == EdgeKind::eHoistDown;
        }
        const s32 dy = (n.mY - y) * scale;
        const s32 level = std::abs(dy) <= 10 ? 0 : (dy >= 60 && dy <= 140 ? -1 : (dy <= -60 && dy >= -140 ? 1 : 2));
        const s32 cells = (std::abs(n.mX - x) * scale + 12) / 25;
        for (std::size_t k = 0; level != 2 && k < sizeof(kTries) / sizeof(kTries[0]); k++)
        {
            // (under a low ceiling there is only the crouched throw)
            if (kTries[k].mLevel == level && kTries[k].mCells == cells && (!n.mLow || kTries[k].mKind == ThrowSpot::eCrouched) && (!hoistTop || kTries[k].mKind != ThrowSpot::eCrouched))
            {
                ThrowSpot spot;
                spot.mNode = static_cast<s32>(i);
                spot.mKind = kTries[k].mKind;
                spot.mOrder = static_cast<s32>(k);
                spot.mCells = cells;
                spot.mLevel = level;
                const s32 step = NavGraph::GridStep(half);
                if (n.mClear >= 0)
                {
                    spot.mNeeds.push_back(nav.Input().mFeatures[n.mClear].mSwitch);
                }
                // One by where he stands goes off if the rock comes back down on it, and takes him with it
                for (const NavNode& other : nav.Nodes())
                {
                    if (other.mClear >= 0 && other.mHalf == half && std::abs(other.mY - n.mY) * scale <= 50 && std::abs(other.mX - n.mX) * scale < (level < 0 ? 125 : 90) && std::abs(other.mX - x) + std::abs(other.mY - y) > 12)
                    {
                        const s32 sw = nav.Input().mFeatures[other.mClear].mSwitch;
                        if (std::find(spot.mNeeds.begin(), spot.mNeeds.end(), sw) == spot.mNeeds.end())
                        {
                            spot.mNeeds.push_back(sw);
                        }
                    }
                }
                for (s32 c = 1; level == 0 && c < cells; c++)
                {
                    const s32 between = nav.NodeAt(n.mX + (x > n.mX ? c : -c) * step, n.mY, half, 8);
                    if (between >= 0 && nav.Nodes()[between].mClear >= 0)
                    {
                        spot.mNeeds.push_back(nav.Input().mFeatures[nav.Nodes()[between].mClear].mSwitch);
                    }
                }
                // (a crouched throw along the floor goes under a drill that is overhead)
                const s32 top = std::min(n.mY, y) - (level == 0 && spot.mKind == ThrowSpot::eCrouched ? 30 : 70) / scale;
                for (const Entity& drill : world ? world->mEntities : std::vector<Entity>{})
                {
                    if (drill.mKind == EntityKind::eDrill && drill.mHalf == half && drill.mX1 <= std::max(n.mX, x) && drill.mX2 >= std::min(n.mX, x) && drill.mY1 <= std::max(n.mY, y) && drill.mY2 >= top)
                    {
                        spot.mDrill = drill.mId;
                    }
                }
                spots.push_back(spot);
            }
        }
    }
    std::stable_sort(spots.begin(), spots.end(), [](const ThrowSpot& a, const ThrowSpot& b)
                     { return a.mOrder < b.mOrder; });
    return spots;
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

    const auto add = [&d](std::string name, std::vector<Goap::Fact> pre, std::vector<Goap::Fact> effects, s32 cost, const DomainStep& step)
    {
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
        DomainStep step;
        step.mKind = StepKind::eCross;
        step.mEdge = static_cast<s32>(i);
        add("Cross " + std::string(EdgeKindName(e.mKind)) + " " + std::to_string(nav.Nodes()[e.mFrom].mX) + "," + std::to_string(nav.Nodes()[e.mFrom].mY) + "->" + std::to_string(nav.Nodes()[e.mTo].mX) + "," + std::to_string(nav.Nodes()[e.mTo].mY),
            pre, {regionFact(e.mTo)}, 100 + e.mCost, step);
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
            {
                const s32 node = UseNode(nav, entity, nullptr);
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
                if (entity.mOp == 0 || entity.mOp == 2)
                {
                    add("Use " + name + " sw" + std::to_string(entity.mSwitch) + " on", {regionFact(node), {key, 0}}, {{key, 1}}, 150, step);
                }
                if (entity.mOp == 1 || entity.mOp == 2)
                {
                    add("Use " + name + " sw" + std::to_string(entity.mSwitch) + " off", {regionFact(node), {key, 1}}, {{key, 0}}, 150, step);
                }
                break;
            }

            case EntityKind::eSwitchLogic:
            {
                // Done already: nothing to plan, and the Mudokons it took are free again
                if (world.Switch(entity.mSwitch) == (entity.mOp == 1 ? 0 : 1))
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
                // be, and past it as it runs at a price (by trying, a miss is taken back)
                std::set<std::pair<s32, u32>> from;
                const std::vector<ThrowSpot> spots = ThrowSpots(nav, entity.CentreX(), entity.mY2, entity.mHalf, &world);
                // From a ledge above the rock bounces off the edge and the floor, and what hits is found by
                // trying: only for a mine there is no other way to
                bool proven = false;
                for (const ThrowSpot& spot : spots)
                {
                    proven = proven || spot.mLevel <= 0;
                }
                for (const ThrowSpot& spot : spots)
                {
                    if (proven && spot.mLevel > 0)
                    {
                        continue;
                    }
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
                        const s32 cost = 300;
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
        if (f.mKind == FeatureKind::eDoor && f.mStartClosed && f.mSwitch > 0)
        {
            pre.push_back({Key::Switch(f.mSwitch), 1});
        }
        add("Exit " + std::to_string(nav.Nodes()[x.mNode].mX) + "," + std::to_string(nav.Nodes()[x.mNode].mY) + " to path " + std::to_string(f.mDestPath), pre, {{Key::kExited, 1}}, 200, step);
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
