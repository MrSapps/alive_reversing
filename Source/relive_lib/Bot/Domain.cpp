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
    }
    return "?";
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
                }
            }
            break;
        }
        case EntityKind::ePullRing:
        case EntityKind::eWorkWheel:
        case EntityKind::eHandStone:
            node = nav.NodeAt(cx, entity.mY2, entity.mHalf, 40);
            break;
        case EntityKind::eBirdPortal:
            // A few cells off, either side
            for (s32 cells = 3; cells >= 1 && node < 0; cells--)
            {
                for (s32 side = -1; side <= 1 && node < 0; side += 2)
                {
                    node = nav.NodeAt(cx + side * cells * step, entity.mY2, entity.mHalf, 30);
                    left = side > 0;
                }
            }
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
                step.mKind = StepKind::eUse;
                const u32 key = Key::Switch(entity.mSwitch);
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
                // Several wheels only count turned at once: without Mudokons for them there's nothing to do
                s32 wheelInputs = 0;
                for (s32 input : entity.mInputs)
                {
                    for (const Entity& w : world.mEntities)
                    {
                        wheelInputs += input > 1 && w.mKind == EntityKind::eWorkWheel && w.mSwitch == input ? 1 : 0;
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
                // The nearest portal it can walk to with Abe
                const Entity* best = nullptr;
                s32 bestDist = 0;
                for (const Entity& portal : world.mEntities)
                {
                    if (portal.mKind != EntityKind::eBirdPortal || !portal.mRescuePortal || portal.mHalf != entity.mHalf)
                    {
                        continue;
                    }
                    const s32 pnode = UseNode(nav, portal, nullptr);
                    if (pnode < 0 || d.mRegions[pnode] != d.mRegions[node])
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
                add("Rescue " + name + " via BirdPortal#" + std::to_string(best->mId), {regionFact(node), {Key::Mud(entity.mId), 0}}, effects, 400 + bestDist / 4, step);
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
