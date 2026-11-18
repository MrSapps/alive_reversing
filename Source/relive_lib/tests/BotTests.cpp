#include <gtest/gtest.h>
#include <cmath>
#include "Bot/BotRng.hpp"
#include "Bot/Domain.hpp"
#include "Bot/FartFlight.hpp"
#include "Bot/GameView.hpp"
#include "Bot/Goap.hpp"
#include "Bot/LevelGraph.hpp"
#include "Bot/Lookahead.hpp"
#include "Bot/FlierFlight.hpp"
#include "Bot/NavGraph.hpp"
#include "Bot/Pilot.hpp"
#include "Bot/Profile.hpp"
#include "Bot/BombCycle.hpp"
#include "Bot/SligCycle.hpp"
#include "Bot/Task.hpp"
#include "Bot/ThrowArc.hpp"
#include "Bot/World.hpp"

using namespace Bot;

// ---- BotRng ----

TEST(BotRng, SameSeedSameNumbers)
{
    BotRng a(7);
    BotRng b(7);
    for (s32 i = 0; i < 100; i++)
    {
        EXPECT_EQ(a.Next(), b.Next());
    }
}

TEST(BotRng, SeedsDiffer)
{
    BotRng a(1);
    BotRng b(2);
    s32 same = 0;
    for (s32 i = 0; i < 100; i++)
    {
        same += a.Next() == b.Next() ? 1 : 0;
    }
    EXPECT_EQ(same, 0);
}

TEST(BotRng, RangeStaysInBounds)
{
    BotRng rng(3);
    bool sawLo = false;
    bool sawHi = false;
    for (s32 i = 0; i < 2000; i++)
    {
        const s32 v = rng.Range(-2, 3);
        ASSERT_GE(v, -2);
        ASSERT_LE(v, 3);
        sawLo = sawLo || v == -2;
        sawHi = sawHi || v == 3;
    }
    EXPECT_TRUE(sawLo);
    EXPECT_TRUE(sawHi);
    EXPECT_EQ(rng.Below(0), 0u);
    EXPECT_EQ(rng.Range(5, 5), 5);
}

TEST(BotRng, StateCanBePutBack)
{
    BotRng rng(9);
    rng.Next();
    const u64 state = rng.State();
    const u64 next = rng.Next();
    rng.SetState(state);
    EXPECT_EQ(rng.Next(), next);
}

// ---- Profile ----

TEST(BotProfile, ParsesEndingsAndOptions)
{
    Profile p;
    std::string error;
    ASSERT_TRUE(Profile::Parse("save_all", p, error)) << error;
    EXPECT_EQ(p.mEnding, Ending::eSaveAll);
    EXPECT_EQ(p.mSeed, 1u);
    EXPECT_FALSE(p.mHuman);

    ASSERT_TRUE(Profile::Parse("kill_all,seed=42,human", p, error)) << error;
    EXPECT_EQ(p.mEnding, Ending::eKillAll);
    EXPECT_EQ(p.mSeed, 42u);
    EXPECT_TRUE(p.mHuman);
    EXPECT_EQ(p.Text(), "kill_all,seed=42,human");
    EXPECT_FALSE(p.mLook);

    Profile looking;
    ASSERT_TRUE(Profile::Parse("any,look", looking, error)) << error;
    EXPECT_TRUE(looking.mLook);
    EXPECT_EQ(looking.Text(), "any,seed=1,look");

    ASSERT_TRUE(Profile::Parse("average", p, error));
    EXPECT_EQ(p.mEnding, Ending::eAverage);
    ASSERT_TRUE(Profile::Parse("any", p, error));
    EXPECT_EQ(p.mEnding, Ending::eAny);
}

TEST(BotProfile, RejectsWhatItDoesNotKnow)
{
    Profile p;
    std::string error;
    EXPECT_FALSE(Profile::Parse("win", p, error));
    EXPECT_NE(error.find("win"), std::string::npos);
    EXPECT_FALSE(Profile::Parse("save_all,seed=x", p, error));
    EXPECT_FALSE(Profile::Parse("save_all,fast", p, error));
    EXPECT_FALSE(Profile::Parse("", p, error));
}

TEST(BotProfile, WhichMudokonsAnEndingRescues)
{
    Profile p;
    p.mEnding = Ending::eSaveAll;
    EXPECT_TRUE(p.Rescues(0));
    EXPECT_TRUE(p.Rescues(1));
    p.mEnding = Ending::eAverage;
    EXPECT_TRUE(p.Rescues(0));
    EXPECT_FALSE(p.Rescues(1));
    p.mEnding = Ending::eKillAll;
    EXPECT_FALSE(p.Rescues(0));
    p.mEnding = Ending::eAny;
    EXPECT_FALSE(p.Rescues(0));
}

// ---- Goap ----

static Goap::Action Act(const char* name, std::vector<Goap::Fact> pre, std::vector<Goap::Fact> effects, s32 cost = 1)
{
    Goap::Action a;
    a.mName = name;
    a.mPre = std::move(pre);
    a.mEffects = std::move(effects);
    a.mCost = cost;
    return a;
}

TEST(BotGoap, StateKeepsFactsInOrderAndDropsZeroes)
{
    Goap::State a;
    a.Set(5, 1);
    a.Set(2, 3);
    Goap::State b;
    b.Set(2, 3);
    b.Set(5, 1);
    EXPECT_TRUE(a == b);
    EXPECT_EQ(a.Get(2), 3);
    EXPECT_EQ(a.Get(9), 0);
    a.Set(5, 0);
    EXPECT_EQ(a.Facts().size(), 1u);
    EXPECT_FALSE(a == b);
}

TEST(BotGoap, NothingToDoWhenTheGoalHolds)
{
    Goap::State start;
    start.Set(1, 1);
    const Goap::Plan plan = Goap::Planner::Find(start, {{1, 1}}, {});
    EXPECT_TRUE(plan.mFound);
    EXPECT_TRUE(plan.mSteps.empty());
}

TEST(BotGoap, ChainsActionsThroughTheirNeeds)
{
    // Door needs the switch, the switch needs Abe at the lever
    const u32 kAt = 1, kSwitch = 2, kThrough = 3;
    const std::vector<Goap::Action> actions = {
        Act("go through door", {{kAt, 2}, {kSwitch, 1}}, {{kThrough, 1}}),
        Act("pull lever", {{kAt, 1}}, {{kSwitch, 1}}),
        Act("walk to lever", {{kAt, 0}}, {{kAt, 1}}),
        Act("walk to door", {{kAt, 1}}, {{kAt, 2}}),
    };
    const Goap::Plan plan = Goap::Planner::Find({}, {{kThrough, 1}}, actions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    ASSERT_EQ(plan.mSteps.size(), 4u);
    EXPECT_EQ(actions[plan.mSteps[0]].mName, "walk to lever");
    EXPECT_EQ(actions[plan.mSteps[1]].mName, "pull lever");
    EXPECT_EQ(actions[plan.mSteps[2]].mName, "walk to door");
    EXPECT_EQ(actions[plan.mSteps[3]].mName, "go through door");
    EXPECT_EQ(plan.mCost, 4);
}

TEST(BotGoap, TakesTheCheapestPlan)
{
    const std::vector<Goap::Action> actions = {
        Act("long way", {}, {{1, 1}}, 10),
        Act("first half", {}, {{2, 1}}, 2),
        Act("second half", {{2, 1}}, {{1, 1}}, 3),
    };
    const Goap::Plan plan = Goap::Planner::Find({}, {{1, 1}}, actions);
    ASSERT_TRUE(plan.mFound);
    EXPECT_EQ(plan.mCost, 5);
    EXPECT_EQ(plan.mSteps.size(), 2u);
}

TEST(BotGoap, EqualPlansComeOutTheSameEveryTime)
{
    const std::vector<Goap::Action> actions = {
        Act("a", {}, {{1, 1}}, 4),
        Act("b", {}, {{1, 1}}, 4),
    };
    const Goap::Plan first = Goap::Planner::Find({}, {{1, 1}}, actions);
    for (s32 i = 0; i < 10; i++)
    {
        const Goap::Plan again = Goap::Planner::Find({}, {{1, 1}}, actions);
        EXPECT_EQ(again.mSteps, first.mSteps);
    }
    EXPECT_EQ(first.mSteps, std::vector<s32>{0});
}

TEST(BotGoap, SaysWhatNothingGives)
{
    const std::vector<Goap::Action> actions = {Act("pull lever", {}, {{2, 1}})};
    const Goap::Plan plan = Goap::Planner::Find({}, {{7, 1}}, actions);
    EXPECT_FALSE(plan.mFound);
    EXPECT_NE(plan.mMiss.find("7=1"), std::string::npos) << plan.mMiss;
}

TEST(BotGoap, ActionsThatCannotHelpAreLeftOut)
{
    // Ten switches that have nothing to do with the goal would be 1024 states to wade through
    std::vector<Goap::Action> actions;
    for (u32 i = 0; i < 10; i++)
    {
        actions.push_back(Act("noise on", {{100 + i, 0}}, {{100 + i, 1}}));
        actions.push_back(Act("noise off", {{100 + i, 1}}, {{100 + i, 0}}));
    }
    actions.push_back(Act("goal", {}, {{1, 1}}, 50));
    const std::vector<bool> relevant = Goap::Planner::Relevant({{1, 1}}, actions);
    EXPECT_FALSE(relevant[0]);
    EXPECT_TRUE(relevant.back());
    const Goap::Plan plan = Goap::Planner::Find({}, {{1, 1}}, actions);
    ASSERT_TRUE(plan.mFound);
    EXPECT_LT(plan.mExpanded, 5);
}

// Whether a plan can be done step by step from a state, and ends with the goal
static bool PlanWorks(Goap::State state, const Goap::Plan& plan, const std::vector<Goap::Action>& actions, const std::vector<Goap::Fact>& goal)
{
    for (s32 step : plan.mSteps)
    {
        if (!Goap::Planner::Can(state, actions[step]))
        {
            return false;
        }
        Goap::Planner::Do(state, actions[step]);
    }
    return state.HasAll(goal);
}

TEST(BotGoap, ALongPlanIsFoundWithoutTryingEveryOrder)
{
    // Thirty things to do in any order, all needed: the cheapest order is one of 2^30 sets to look through
    std::vector<Goap::Action> actions;
    std::vector<Goap::Fact> goal;
    for (u32 i = 0; i < 30; i++)
    {
        actions.push_back(Act(("mine " + std::to_string(i)).c_str(), {{100 + i, 0}}, {{100 + i, 1}}, 5 + static_cast<s32>(i % 3)));
        goal.push_back({100 + i, 1});
    }
    const Goap::Plan plan = Goap::Planner::Find({}, goal, actions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    EXPECT_EQ(plan.mSteps.size(), 30u);
    EXPECT_LT(plan.mExpanded, 200);
    EXPECT_TRUE(PlanWorks({}, plan, actions, goal));
}

TEST(BotGoap, WhatIsUsedUpIsFetchedAgain)
{
    // A sack gives two rocks, and only to empty hands. Five mines take three trips to it.
    const u32 kRocks = 3;
    std::vector<Goap::Action> actions = {Act("get rocks", {{kRocks, 0}}, {{kRocks, 2}})};
    std::vector<Goap::Fact> goal;
    for (u32 i = 0; i < 5; i++)
    {
        actions.push_back(Act(("blow up " + std::to_string(i)).c_str(), {}, {{100 + i, 1}}));
        actions.back().mSpend = {kRocks, 1};
        goal.push_back({100 + i, 1});
    }
    const Goap::Plan plan = Goap::Planner::Find({}, goal, actions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    EXPECT_EQ(plan.mSteps.size(), 8u);
    EXPECT_TRUE(PlanWorks({}, plan, actions, goal));
}

TEST(BotGoap, MoreIsFetchedBeforeTheWayOfNoReturn)
{
    // One rock in hand and the sack here; three mines, two of them only within reach from over there, and no way
    // back. The rock has to go on the first mine from here, so the sack gives more before crossing. (The thirty
    // other things make it a plan too long to find the cheapest of.)
    const u32 kAt = 1, kRocks = 3;
    Goap::State start;
    start.Set(kAt, 1);
    start.Set(kRocks, 1);
    std::vector<Goap::Action> actions = {
        Act("get rocks", {{kAt, 1}, {kRocks, 0}}, {{kRocks, 3}}),
        Act("cross", {{kAt, 1}}, {{kAt, 2}}),
        Act("blow up 0 from here", {{kAt, 1}}, {{100, 1}}),
        Act("blow up 0 from there", {{kAt, 2}}, {{100, 1}}),
        Act("blow up 1", {{kAt, 2}}, {{101, 1}}),
        Act("blow up 2", {{kAt, 2}}, {{102, 1}}),
    };
    for (std::size_t i = 2; i < actions.size(); i++)
    {
        actions[i].mSpend = {kRocks, 1};
    }
    std::vector<Goap::Fact> goal = {{100, 1}, {101, 1}, {102, 1}};
    for (u32 i = 0; i < 30; i++)
    {
        actions.push_back(Act(("other " + std::to_string(i)).c_str(), {{kAt, 2}, {200 + i, 0}}, {{200 + i, 1}}, 5));
        goal.push_back({200 + i, 1});
    }
    const Goap::Plan plan = Goap::Planner::Find(start, goal, actions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    EXPECT_TRUE(PlanWorks(start, plan, actions, goal));
    EXPECT_LT(plan.mExpanded, 2000);
}

TEST(BotGoap, ADearStepGivesWayToACheaperWayToTheSame)
{
    // A mine can be blown up past a running drill, at a price, or cheaply once the lever next to it has stopped
    // the drill. Thirty more things to do make it a plan too long to find the cheapest of.
    std::vector<Goap::Action> actions = {
        Act("blow up past the drill", {}, {{100, 1}}, 2800),
        Act("blow up", {{50, 1}}, {{100, 1}}, 300),
        Act("stop the drill", {{50, 0}}, {{50, 1}}, 150),
    };
    std::vector<Goap::Fact> goal = {{100, 1}};
    for (u32 i = 0; i < 30; i++)
    {
        actions.push_back(Act(("other " + std::to_string(i)).c_str(), {{100, 1}, {200 + i, 0}}, {{200 + i, 1}}, 5));
        goal.push_back({200 + i, 1});
    }
    const Goap::Plan plan = Goap::Planner::Find({}, goal, actions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    EXPECT_TRUE(PlanWorks({}, plan, actions, goal));
    for (s32 step : plan.mSteps)
    {
        EXPECT_NE(actions[step].mName, "blow up past the drill");
    }
    EXPECT_EQ(plan.mCost, 150 + 300 + 30 * 5);
}

TEST(BotGoap, RunningOutWithNoMoreToBeHadIsNoPlan)
{
    const u32 kRocks = 3;
    Goap::State start;
    start.Set(kRocks, 1);
    std::vector<Goap::Action> actions = {
        Act("blow up 0", {}, {{100, 1}}),
        Act("blow up 1", {}, {{101, 1}}),
    };
    actions[0].mSpend = {kRocks, 1};
    actions[1].mSpend = {kRocks, 1};
    const Goap::Plan plan = Goap::Planner::Find(start, {{100, 1}, {101, 1}}, actions);
    EXPECT_FALSE(plan.mFound);
    EXPECT_FALSE(plan.mMiss.empty());
}

TEST(BotGoap, AToggleCanBeUndone)
{
    // The door is open only with the switch off, and it starts on
    Goap::State start;
    start.Set(2, 1);
    const std::vector<Goap::Action> actions = {
        Act("lever on", {{2, 0}}, {{2, 1}}),
        Act("lever off", {{2, 1}}, {{2, 0}}),
        Act("door", {{2, 0}}, {{1, 1}}),
    };
    const Goap::Plan plan = Goap::Planner::Find(start, {{1, 1}}, actions);
    ASSERT_TRUE(plan.mFound);
    ASSERT_EQ(plan.mSteps.size(), 2u);
    EXPECT_EQ(actions[plan.mSteps[0]].mName, "lever off");
}

// ---- NavGraph ----

static NavLine Line(s32 x1, s32 y1, s32 x2, s32 y2, s32 type)
{
    NavLine l;
    l.mX1 = x1;
    l.mY1 = y1;
    l.mX2 = x2;
    l.mY2 = y2;
    l.mType = type;
    return l;
}

static NavFeature Feature(FeatureKind kind, s32 x1, s32 y1, s32 x2, s32 y2)
{
    NavFeature f;
    f.mKind = kind;
    f.mX1 = x1;
    f.mY1 = y1;
    f.mX2 = x2;
    f.mY2 = y2;
    return f;
}

static bool HasEdge(const NavGraph& g, s32 from, s32 to, EdgeKind kind)
{
    for (const NavEdge& e : g.Edges())
    {
        if (e.mFrom == from && e.mTo == to && e.mKind == kind)
        {
            return true;
        }
    }
    return false;
}

TEST(BotNav, CellsSitOnTheGamesGrid)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    NavGraph ae;
    ae.Build(in);
    ASSERT_EQ(ae.Nodes().size(), 4u);
    EXPECT_EQ(ae.Nodes()[0].mX, 12);
    EXPECT_EQ(ae.Nodes()[3].mX, 87);

    in.mIsAe = false;
    in.mLines[0] = Line(240, 300, 340, 300, 0);
    NavGraph ao;
    ao.Build(in);
    ASSERT_FALSE(ao.Nodes().empty());
    EXPECT_EQ(ao.Nodes()[0].mX, 240);
    EXPECT_EQ(ao.Nodes()[1].mX, 265);
}

TEST(BotNav, WalksAlongAFloorAndFindsTheWay)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    NavGraph g;
    g.Build(in);
    const s32 a = g.NodeAt(12, 100, false);
    const s32 b = g.NodeAt(287, 100, false);
    ASSERT_GE(a, 0);
    ASSERT_GE(b, 0);
    std::vector<s32> path;
    s32 cost = 0;
    ASSERT_TRUE(g.FindPath(a, b, nullptr, path, &cost));
    EXPECT_EQ(path.size(), 11u);
    for (s32 e : path)
    {
        EXPECT_EQ(g.Edges()[e].mKind, EdgeKind::eWalk);
        EXPECT_FALSE(g.Edges()[e].mLeft);
    }
    EXPECT_GT(cost, 0);
    EXPECT_EQ(g.NodeAt(12, 300, false), -1);
}

TEST(BotNav, AWallStopsAWalk)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mLines.push_back(Line(150, 0, 150, 100, 1));
    NavGraph g;
    g.Build(in);
    std::vector<s32> path;
    EXPECT_FALSE(g.FindPath(g.NodeAt(12, 100, false), g.NodeAt(287, 100, false), nullptr, path));
    // A wall that ends above Abe's head doesn't
    in.mLines[1] = Line(150, 0, 150, 40, 1);
    g.Build(in);
    EXPECT_TRUE(g.FindPath(g.NodeAt(12, 100, false), g.NodeAt(287, 100, false), nullptr, path));
}

TEST(BotNav, HopsOverAMineAndNeverStandsOnIt)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eHazard, 151, 80, 174, 104));
    NavGraph g;
    g.Build(in);
    const s32 mine = g.NodeAt(162, 100, false);
    ASSERT_GE(mine, 0);
    EXPECT_TRUE(g.Nodes()[mine].mHazard);

    std::vector<s32> path;
    ASSERT_TRUE(g.FindPath(g.NodeAt(12, 100, false), g.NodeAt(287, 100, false), nullptr, path));
    s32 hops = 0;
    for (s32 e : path)
    {
        EXPECT_NE(g.Edges()[e].mTo, mine);
        hops += g.Edges()[e].mKind == EdgeKind::eHop ? 1 : 0;
    }
    EXPECT_EQ(hops, 1);
}

TEST(BotNav, FallsOnlyWhereItIsSurvivable)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));   // ledge
    in.mLines.push_back(Line(100, 200, 300, 200, 0)); // 100 below: fine
    NavGraph g;
    g.Build(in);
    EXPECT_TRUE(HasEdge(g, g.NodeAt(87, 100, false), g.NodeAt(112, 200, false), EdgeKind::eFall));

    in.mLines[1] = Line(100, 400, 300, 400, 0); // 300 below: deadly
    g.Build(in);
    for (const NavEdge& e : g.Edges())
    {
        EXPECT_NE(e.mKind, EdgeKind::eFall);
    }

    // Unless it is a soft landing
    in.mFeatures.push_back(Feature(FeatureKind::eSoftLanding, 100, 380, 140, 404));
    in.mFeatures.back().mAnyScale = true;
    g.Build(in);
    EXPECT_TRUE(HasEdge(g, g.NodeAt(87, 100, false), g.NodeAt(112, 400, false), EdgeKind::eFall));

    // And never into a death drop
    in.mFeatures.clear();
    in.mLines[1] = Line(100, 200, 300, 200, 0);
    in.mFeatures.push_back(Feature(FeatureKind::eDeathDrop, 100, 150, 140, 180));
    g.Build(in);
    for (const NavEdge& e : g.Edges())
    {
        EXPECT_NE(e.mKind, EdgeKind::eFall);
    }
}

TEST(BotNav, HoistsGoUpAndDown)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 200, 200, 0));
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    // From the ledge at 100 to just above the floor at 200
    in.mFeatures.push_back(Feature(FeatureKind::eHoist, 100, 100, 124, 195));
    NavGraph g;
    g.Build(in);
    const s32 low = g.NodeAt(112, 200, false);
    const s32 high = g.NodeAt(112, 100, false);
    EXPECT_TRUE(HasEdge(g, low, high, EdgeKind::eHoistUp));
    EXPECT_TRUE(HasEdge(g, high, low, EdgeKind::eHoistDown));
    EXPECT_FALSE(HasEdge(g, g.NodeAt(62, 200, false), g.NodeAt(62, 100, false), EdgeKind::eHoistUp));

    std::vector<s32> path;
    ASSERT_TRUE(g.FindPath(g.NodeAt(12, 200, false), g.NodeAt(12, 100, false), nullptr, path));
}

TEST(BotNav, DoorsLeadToTheirPartnerOrOutOfThePath)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(1000, 100, 1200, 100, 0));
    NavFeature a = Feature(FeatureKind::eDoor, 50, 80, 74, 104);
    a.mA = 1;
    a.mB = 2;
    a.mCamera = 1;
    a.mDestCamera = 2;
    NavFeature b = Feature(FeatureKind::eDoor, 1100, 80, 1124, 104);
    b.mA = 2;
    b.mB = 1;
    b.mCamera = 2;
    b.mDestCamera = 1;
    NavFeature out = Feature(FeatureKind::eDoor, 150, 80, 174, 104);
    out.mA = 3;
    out.mCamera = 1;
    out.mDestPath = 7;
    out.mDestCamera = 4;
    in.mFeatures = {a, b, out};
    NavGraph g;
    g.Build(in);
    EXPECT_TRUE(HasEdge(g, g.NodeAt(62, 100, false), g.NodeAt(1112, 100, false), EdgeKind::eDoor));
    EXPECT_TRUE(HasEdge(g, g.NodeAt(1112, 100, false), g.NodeAt(62, 100, false), EdgeKind::eDoor));
    ASSERT_EQ(g.Exits().size(), 1u);
    EXPECT_EQ(g.Exits()[0].mNode, g.NodeAt(162, 100, false));
    EXPECT_EQ(g.Input().mFeatures[g.Exits()[0].mFeature].mDestPath, 7);
}

TEST(BotNav, AoWalksAcrossTheGapBetweenCameras)
{
    NavInput in;
    in.mIsAe = false;
    in.mCamW = 1024;
    in.mCamH = 480;
    in.mLines.push_back(Line(108, 317, 774, 317, 0));
    in.mLines.push_back(Line(1114, 317, 1794, 317, 0));
    NavGraph g;
    g.Build(in);
    const s32 lastOfFirst = g.NodeAt(615, 317, false);
    const s32 firstOfSecond = g.NodeAt(1024 + 240, 317, false);
    ASSERT_GE(lastOfFirst, 0);
    ASSERT_GE(firstOfSecond, 0);
    EXPECT_EQ(g.NodeAt(640, 317, false), -1);
    EXPECT_TRUE(HasEdge(g, lastOfFirst, firstOfSecond, EdgeKind::eWalk));
    EXPECT_TRUE(HasEdge(g, firstOfSecond, lastOfFirst, EdgeKind::eWalk));
    EXPECT_EQ(g.StepX(615, 1, false), 1264);
    EXPECT_EQ(g.StepX(1264, -1, false), 615);
}

TEST(BotNav, AoFallsSkipTheGapUnderACamera)
{
    NavInput in;
    in.mIsAe = false;
    in.mCamW = 1024;
    in.mCamH = 480;
    in.mLines.push_back(Line(240, 300, 400, 300, 0));
    // In the camera below, 50 under its top edge
    in.mLines.push_back(Line(240, 480 + 170, 600, 480 + 170, 0));
    NavGraph g;
    g.Build(in);
    s32 floorY = 0;
    s32 distance = 0;
    ASSERT_GE(g.FloorBelow(440, 301, false, &floorY, &distance), 0);
    EXPECT_EQ(floorY, 650);
    // 59 to the bottom of this camera's view, 50 into the next
    EXPECT_EQ(distance, 109);
}

TEST(BotNav, BannedEdgesAreRoutedAround)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    NavGraph g;
    g.Build(in);
    const s32 a = g.NodeAt(12, 100, false);
    const s32 b = g.NodeAt(287, 100, false);
    std::vector<s32> path;
    ASSERT_TRUE(g.FindPath(a, b, nullptr, path));
    const s32 banned = path[3];
    EXPECT_FALSE(g.FindPath(a, b, [&](const NavEdge& e)
                            { return &e == &g.Edges()[banned] ? -1 : e.mCost; }, path));
}

TEST(BotNav, LearnsWhereAnEdgeReallyLeads)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(100, 200, 300, 200, 0));
    NavGraph g;
    g.Build(in);
    const s32 from = g.NodeAt(87, 100, false);
    const s32 guessed = g.NodeAt(112, 200, false);
    const s32 actual = g.NodeAt(137, 200, false);
    for (std::size_t i = 0; i < g.Edges().size(); i++)
    {
        if (g.Edges()[i].mFrom == from && g.Edges()[i].mKind == EdgeKind::eFall)
        {
            EXPECT_EQ(g.Edges()[i].mTo, guessed);
            g.LearnEdge(static_cast<s32>(i), actual);
            EXPECT_EQ(g.Edges()[i].mTo, actual);
        }
    }
}

static const NavEdge* FindEdge(const NavGraph& g, s32 from, s32 to, EdgeKind kind)
{
    for (const NavEdge& e : g.Edges())
    {
        if (e.mFrom == from && e.mTo == to && e.mKind == kind)
        {
            return &e;
        }
    }
    return nullptr;
}

TEST(BotNav, AMineCellOpensWhenTheMineIsGone)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    NavFeature mineFeature = Feature(FeatureKind::eHazard, 151, 80, 174, 104);
    mineFeature.mSwitch = World::HazardSwitch(7);
    in.mFeatures.push_back(mineFeature);
    NavGraph g;
    g.Build(in);
    const s32 mine = g.NodeAt(162, 100, false);
    ASSERT_GE(mine, 0);
    EXPECT_TRUE(g.Nodes()[mine].mHazard);

    // Into its cell and out of it there are edges now, each waiting for the mine's switch
    const NavEdge* in1 = FindEdge(g, g.NodeAt(137, 100, false), mine, EdgeKind::eWalk);
    const NavEdge* out1 = FindEdge(g, mine, g.NodeAt(187, 100, false), EdgeKind::eWalk);
    ASSERT_NE(in1, nullptr);
    ASSERT_NE(out1, nullptr);
    s32 sw = 0;
    s32 value = 0;
    ASSERT_TRUE(g.Condition(*in1, &sw, &value));
    EXPECT_EQ(sw, World::HazardSwitch(7));
    EXPECT_EQ(value, 1);
    EXPECT_TRUE(g.Condition(*out1, nullptr, nullptr));
    // The cell is a region of its own
    const std::vector<s32> regions = g.Regions();
    EXPECT_NE(regions[mine], regions[g.NodeAt(137, 100, false)]);
    EXPECT_NE(regions[mine], regions[g.NodeAt(187, 100, false)]);

    // While it is there the way is still the hop over it; gone, it is walked through
    const auto cost = [&g](bool gone)
    {
        return [&g, gone](const NavEdge& e) -> s32
        {
            return g.Condition(e, nullptr, nullptr) && !gone ? -1 : e.mCost;
        };
    };
    std::vector<s32> path;
    ASSERT_TRUE(g.FindPath(g.NodeAt(112, 100, false), g.NodeAt(212, 100, false), cost(false), path));
    s32 hops = 0;
    for (s32 e : path)
    {
        EXPECT_NE(g.Edges()[e].mTo, mine);
        hops += g.Edges()[e].mKind == EdgeKind::eHop ? 1 : 0;
    }
    EXPECT_EQ(hops, 1);
    ASSERT_TRUE(g.FindPath(g.NodeAt(137, 100, false), g.NodeAt(187, 100, false), cost(true), path));
    ASSERT_EQ(path.size(), 2u);
    EXPECT_EQ(g.Edges()[path[0]].mTo, mine);
}

TEST(BotNav, AHazardThatStaysIsStillNoCell)
{
    // A UXB has no switch of its own: nothing leads into its cell
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eHazard, 151, 80, 174, 104));
    NavGraph g;
    g.Build(in);
    const s32 uxb = g.NodeAt(162, 100, false);
    for (const NavEdge& e : g.Edges())
    {
        EXPECT_NE(e.mTo, uxb);
        EXPECT_NE(e.mFrom, uxb);
        EXPECT_FALSE(g.Condition(e, nullptr, nullptr));
    }
}

TEST(BotNav, ALowCeilingIsRolledUnder)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    // A ceiling 40 over the floor, with walls down to it on both sides: eLineTypes 3, 1 and 2
    in.mLines.push_back(Line(100, 60, 200, 60, 3));
    in.mLines.push_back(Line(100, 0, 100, 60, 2));
    in.mLines.push_back(Line(200, 0, 200, 60, 1));
    NavGraph g;
    g.Build(in);
    const s32 before = g.NodeAt(87, 100, false);
    const s32 under = g.NodeAt(137, 100, false);
    const s32 after = g.NodeAt(212, 100, false);
    ASSERT_GE(before, 0);
    ASSERT_GE(under, 0);
    EXPECT_FALSE(g.Nodes()[before].mLow);
    EXPECT_TRUE(g.Nodes()[under].mLow);
    EXPECT_TRUE(HasEdge(g, before, g.NodeAt(112, 100, false), EdgeKind::eRoll));
    EXPECT_TRUE(HasEdge(g, under, g.NodeAt(162, 100, false), EdgeKind::eRoll));
    EXPECT_FALSE(HasEdge(g, under, g.NodeAt(162, 100, false), EdgeKind::eWalk));
    EXPECT_TRUE(HasEdge(g, after, g.NodeAt(237, 100, false), EdgeKind::eWalk));
    std::vector<s32> path;
    EXPECT_TRUE(g.FindPath(g.NodeAt(12, 100, false), g.NodeAt(287, 100, false), nullptr, path));
}

TEST(BotNav, ARunningJumpClearsFourCells)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(175, 100, 300, 100, 0));
    NavGraph g;
    g.Build(in);
    // Three cells of run-up from 12, off the floor at 87, down on 187
    const NavEdge* jump = FindEdge(g, g.NodeAt(12, 100, false), g.NodeAt(187, 100, false), EdgeKind::eRunJump);
    ASSERT_NE(jump, nullptr) << g.Text();
    EXPECT_EQ(jump->mX, 87);
    EXPECT_FALSE(jump->mLeft);
    // No room to run from 62
    EXPECT_EQ(FindEdge(g, g.NodeAt(62, 100, false), g.NodeAt(187, 100, false), EdgeKind::eRunJump), nullptr);
}

TEST(BotNav, ARunningJumpCatchesALedgeFiveCellsOff)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(200, 100, 300, 100, 0));
    NavFeature ledge = Feature(FeatureKind::eEdge, 200, 100, 224, 124);
    ledge.mA = 1;
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    EXPECT_NE(FindEdge(g, g.NodeAt(12, 100, false), g.NodeAt(212, 100, false), EdgeKind::eRunJump), nullptr) << g.Text();

    // A ledge that is only grabbed facing the other way is no use
    in.mFeatures[0].mDir = 0;
    NavGraph wrongWay;
    wrongWay.Build(in);
    EXPECT_EQ(FindEdge(wrongWay, wrongWay.NodeAt(12, 100, false), wrongWay.NodeAt(212, 100, false), EdgeKind::eRunJump), nullptr);
}

TEST(BotNav, AStandingHopCatchesALedgeFourCellsOff)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 25, 100, 0));
    in.mLines.push_back(Line(100, 100, 200, 100, 0));
    NavFeature ledge = Feature(FeatureKind::eEdge, 100, 100, 124, 124);
    ledge.mA = 1;
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    EXPECT_TRUE(HasEdge(g, g.NodeAt(12, 100, false), g.NodeAt(112, 100, false), EdgeKind::eHop)) << g.Text();
}

TEST(BotNav, ALiftJoinsItsStops)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(0, 300, 100, 300, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 100, 176, 124));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 300, 176, 324));
    NavGraph g;
    g.Build(in);
    const s32 top = g.NodeAt(137, 100, false);
    const s32 bottom = g.NodeAt(137, 300, false);
    ASSERT_GE(top, 0);
    ASSERT_GE(bottom, 0);
    EXPECT_GE(g.Nodes()[top].mLiftStop, 0);
    EXPECT_LT(g.Nodes()[g.NodeAt(87, 100, false)].mLiftStop, 0);
    // Ridden from the middle of the platform, walked onto from the floor beside it
    EXPECT_TRUE(HasEdge(g, top, bottom, EdgeKind::eLift));
    EXPECT_TRUE(HasEdge(g, bottom, top, EdgeKind::eLift));
    EXPECT_FALSE(HasEdge(g, g.NodeAt(112, 100, false), g.NodeAt(112, 300, false), EdgeKind::eLift));
    EXPECT_TRUE(HasEdge(g, g.NodeAt(87, 100, false), g.NodeAt(112, 100, false), EdgeKind::eWalk));
    std::vector<s32> path;
    EXPECT_TRUE(g.FindPath(g.NodeAt(12, 100, false), g.NodeAt(12, 300, false), nullptr, path));
}

TEST(BotNav, ADoorThatStartsShutWaitsForItsSwitch)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(0, 300, 100, 300, 0));
    NavFeature a = Feature(FeatureKind::eDoor, 50, 80, 74, 104);
    a.mA = 0;
    a.mB = 1;
    a.mDestCamera = 0;
    a.mSwitch = 7;
    a.mStartClosed = true;
    NavFeature b = Feature(FeatureKind::eDoor, 50, 280, 74, 304);
    b.mA = 1;
    b.mB = 0;
    b.mDestCamera = 0;
    in.mFeatures = {a, b};
    NavGraph g;
    g.Build(in);
    const s32 up = g.NodeAt(62, 100, false);
    const s32 down = g.NodeAt(62, 300, false);
    const NavEdge* through = FindEdge(g, up, down, EdgeKind::eDoor);
    ASSERT_NE(through, nullptr) << g.Text();
    s32 switchId = 0;
    s32 openValue = 0;
    EXPECT_TRUE(g.Condition(*through, &switchId, &openValue));
    EXPECT_EQ(switchId, 7);
    EXPECT_EQ(openValue, 1);
    // The other door of the pair is open, but leads to the shut one's floor: the floors aren't one region
    const std::vector<s32> regions = g.Regions();
    EXPECT_NE(regions[up], regions[down]);
    const NavEdge* back = FindEdge(g, down, up, EdgeKind::eDoor);
    ASSERT_NE(back, nullptr);
    EXPECT_FALSE(g.Condition(*back, nullptr, nullptr));
}

TEST(BotNav, InvisibleZonesHideWhoIsWhollyInside)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eInvisibleZone, 105, 0, 200, 100));
    NavGraph g;
    g.Build(in);
    EXPECT_FALSE(g.Nodes()[g.NodeAt(87, 100, false)].mHidden);
    // At 112 Abe's left side sticks out
    EXPECT_FALSE(g.Nodes()[g.NodeAt(112, 100, false)].mHidden);
    EXPECT_TRUE(g.Nodes()[g.NodeAt(137, 100, false)].mHidden);
    EXPECT_TRUE(g.Nodes()[g.NodeAt(162, 100, false)].mHidden);
    EXPECT_FALSE(g.Nodes()[g.NodeAt(212, 100, false)].mHidden);
}

// ---- Regions and the planning domain ----

// A corridor cut in two by a slam door on switch 5, a lever on the near side, the way out on the far side
static NavInput CorridorWithDoor()
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature door = Feature(FeatureKind::eBarrier, 200, 80, 224, 104);
    door.mSwitch = 5;
    door.mStartClosed = true;
    in.mFeatures.push_back(door);
    NavFeature exit = Feature(FeatureKind::eDoor, 350, 80, 374, 104);
    exit.mDestPath = 2;
    exit.mDestCamera = 1;
    in.mFeatures.push_back(exit);
    return in;
}

static Entity MakeEntity(EntityKind kind, u32 id, s32 x1, s32 y1, s32 x2, s32 y2)
{
    Entity e;
    e.mKind = kind;
    e.mId = id;
    e.mX1 = x1;
    e.mY1 = y1;
    e.mX2 = x2;
    e.mY2 = y2;
    return e;
}

TEST(BotDomain, ABarrierSplitsTheFloorIntoRegions)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    s32 count = 0;
    const std::vector<s32> regions = g.Regions(&count);
    const s32 nearSide = g.NodeAt(12, 100, false);
    const s32 farSide = g.NodeAt(387, 100, false);
    EXPECT_NE(regions[nearSide], regions[farSide]);
    EXPECT_EQ(regions[nearSide], regions[g.NodeAt(162, 100, false)]);
    EXPECT_GE(count, 2);
}

TEST(BotDomain, PlansLeverThenDoorThenExit)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world;
    Entity lever = MakeEntity(EntityKind::eLever, 1001, 50, 80, 124, 104);
    lever.mSwitch = 5;
    lever.mOp = 2;
    world.mEntities.push_back(lever);

    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss << "\n" << domain.Text();
    ASSERT_GE(plan.mSteps.size(), 3u);
    EXPECT_EQ(domain.mSteps[domain.mActions[plan.mSteps.front()].mTag].mKind, StepKind::eUse);
    EXPECT_EQ(domain.mSteps[domain.mActions[plan.mSteps.back()].mTag].mKind, StepKind::eExit);
    for (std::size_t i = 1; i + 1 < plan.mSteps.size(); i++)
    {
        EXPECT_EQ(domain.mSteps[domain.mActions[plan.mSteps[i]].mTag].mKind, StepKind::eCross);
    }
}

TEST(BotDomain, NoLeverNoPlanAndItSaysWhy)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world;
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    EXPECT_FALSE(plan.mFound);
    EXPECT_FALSE(plan.mMiss.empty());
}

TEST(BotDomain, ARescueOpensTheDoorItsSwitchIsOn)
{
    // The tutorial's trick: the door opens when the Mudokon is saved
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world;
    Entity mud = MakeEntity(EntityKind::eMudokon, 1002, 100, 80, 124, 100);
    mud.mSwitch = 5;
    world.mEntities.push_back(mud);
    Entity portal = MakeEntity(EntityKind::eBirdPortal, 1003, 150, 80, 174, 104);
    portal.mRescuePortal = true;
    world.mEntities.push_back(portal);

    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    ASSERT_EQ(domain.mRescuable.size(), 1u);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    const DomainStep& first = domain.mSteps[domain.mActions[plan.mSteps.front()].mTag];
    EXPECT_EQ(first.mKind, StepKind::eRescue);
    EXPECT_EQ(first.mEntity, 1002u);
    EXPECT_EQ(first.mOther, 1003u);
}

TEST(BotDomain, AMudokonAlreadyGoneIsNotAGoalAgain)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world;
    Entity mud = MakeEntity(EntityKind::eMudokon, 1002, 100, 80, 124, 100);
    mud.mGone = true;
    world.mEntities.push_back(mud);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    EXPECT_TRUE(domain.mRescuable.empty());
    EXPECT_EQ(domain.mStart.Get(Key::Mud(1002)), 1);
}

TEST(BotDomain, SwitchLogicNeedsAllItsInputs)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world;
    Entity leverA = MakeEntity(EntityKind::eLever, 1, 25, 80, 99, 104);
    leverA.mSwitch = 10;
    Entity leverB = MakeEntity(EntityKind::eLever, 2, 100, 80, 174, 104);
    leverB.mSwitch = 11;
    Entity logic = MakeEntity(EntityKind::eSwitchLogic, 3, 0, 0, 10, 10);
    logic.mSwitch = 5;
    logic.mInputs[0] = 10;
    logic.mInputs[1] = 11;
    // The switch that is always on doesn't count as an input
    logic.mInputs[2] = 1;
    world.mEntities = {leverA, leverB, logic};
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    s32 uses = 0;
    for (s32 step : plan.mSteps)
    {
        uses += domain.mSteps[domain.mActions[step].mTag].mKind == StepKind::eUse ? 1 : 0;
    }
    EXPECT_EQ(uses, 2);
}

// Two wheels that open the corridor's door only turned at the same time
static World TwoWheels()
{
    World world;
    // (wheels whose switch is only on while they turn)
    Entity wheelA = MakeEntity(EntityKind::eWorkWheel, 1, 100, 80, 124, 104);
    wheelA.mSwitch = 10;
    wheelA.mHoldOnly = true;
    Entity wheelB = MakeEntity(EntityKind::eWorkWheel, 2, 150, 80, 174, 104);
    wheelB.mSwitch = 11;
    wheelB.mHoldOnly = true;
    Entity logic = MakeEntity(EntityKind::eSwitchLogic, 3, 0, 0, 10, 10);
    logic.mSwitch = 5;
    logic.mInputs[0] = 10;
    logic.mInputs[1] = 11;
    world.mEntities = {wheelA, wheelB, logic};
    return world;
}

TEST(BotDomain, WheelsTurnedTogetherNeedAMudokonEach)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world = TwoWheels();
    // Abe has one pair of hands
    Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    EXPECT_FALSE(Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions).mFound);

    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 4, 25, 80, 49, 100));
    domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss << "\n" << domain.Text();
    EXPECT_EQ(domain.mSteps[domain.mActions[plan.mSteps.front()].mTag].mKind, StepKind::eTogether);

    Team team;
    std::vector<Goap::Fact> pre;
    ASSERT_TRUE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
    ASSERT_EQ(team.mMuds.size(), 1u);
    EXPECT_EQ(team.mMuds[0], 4u);
    EXPECT_EQ(team.mWheels.size(), 2u);
    // Abe's is the one the Mudokon gets to last
    EXPECT_EQ(team.mAbeWheel, 2u);
}

TEST(BotDomain, AMudokonAtAWheelAlreadyHasIt)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world = TwoWheels();
    // One stands at the first wheel: the other wheel is Abe's, and nobody has to be brought
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 4, 100, 80, 124, 100));
    Team team;
    std::vector<Goap::Fact> pre;
    ASSERT_TRUE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
    ASSERT_EQ(team.mMuds.size(), 1u);
    EXPECT_EQ(team.mMuds[0], 4u);
    ASSERT_EQ(team.mFree.size(), 1u);
    EXPECT_EQ(team.mFree[0], 2u);
    EXPECT_EQ(team.mAbeWheel, 2u);
    ASSERT_EQ(pre.size(), 1u);
    EXPECT_EQ(pre[0], (Goap::Fact{Key::Mud(4), 0}));

    // One at that place on a floor far below is at no wheel, and has no way up
    world.mEntities.back().mY1 = 280;
    world.mEntities.back().mY2 = 300;
    EXPECT_FALSE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
    // And one that is gone turns nothing
    world.mEntities.back().mY1 = 80;
    world.mEntities.back().mY2 = 100;
    world.mEntities.back().mGone = true;
    EXPECT_FALSE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
}

// Three wheels on a floor a lift's shaft goes through, a Mudokon at the middle one, and another on the floor below
// behind a wall a switch opens (the Slig Barracks' wheel room)
static NavInput WheelsOverALift()
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mLines.push_back(Line(375, 100, 700, 100, 0));
    in.mLines.push_back(Line(0, 300, 300, 300, 0));
    in.mLines.push_back(Line(375, 300, 700, 300, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 300, 100, 375, 124));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 300, 300, 375, 324));
    NavFeature wall = Feature(FeatureKind::eBarrier, 100, 280, 124, 304);
    wall.mSwitch = 7;
    wall.mStartClosed = true;
    in.mFeatures.push_back(wall);
    return in;
}

TEST(BotDomain, MudokonsForTheWheelsComeUpByLift)
{
    NavGraph g;
    g.Build(WheelsOverALift());
    World world;
    Entity wheelA = MakeEntity(EntityKind::eWorkWheel, 1, 100, 80, 124, 104);
    wheelA.mSwitch = 10;
    Entity wheelB = MakeEntity(EntityKind::eWorkWheel, 2, 500, 80, 524, 104);
    wheelB.mSwitch = 11;
    Entity wheelC = MakeEntity(EntityKind::eWorkWheel, 5, 150, 80, 174, 104);
    wheelC.mSwitch = 12;
    Entity logic = MakeEntity(EntityKind::eSwitchLogic, 3, 0, 0, 10, 10);
    logic.mSwitch = 5;
    logic.mInputs[0] = 10;
    logic.mInputs[1] = 11;
    logic.mInputs[2] = 12;
    world.mEntities = {wheelA, wheelB, logic, wheelC};
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 6, 150, 80, 174, 100));
    Team team;
    std::vector<Goap::Fact> pre;
    // Two wheels without a Mudokon and one pair of hands
    EXPECT_FALSE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));

    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 4, 25, 280, 49, 300));
    ASSERT_TRUE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
    // The one at its wheel, then the one brought
    ASSERT_EQ(team.mMuds.size(), 2u);
    EXPECT_EQ(team.mMuds[0], 6u);
    EXPECT_EQ(team.mMuds[1], 4u);
    EXPECT_EQ(team.mWheels.size(), 3u);
    ASSERT_EQ(team.mFree.size(), 2u);
    EXPECT_EQ(team.mFree[0], 1u);
    EXPECT_EQ(team.mFree[1], 2u);
    // It comes onto the wheels' floor off the lift, and Abe's wheel is the one further from there
    EXPECT_GT(team.mFromX, 300);
    EXPECT_LT(team.mFromX, 375);
    EXPECT_EQ(team.mAbeWheel, 1u);
    // The wall between it and the lift has to be open
    EXPECT_NE(std::find(pre.begin(), pre.end(), Goap::Fact{Key::Switch(7), 1}), pre.end());
    EXPECT_NE(std::find(pre.begin(), pre.end(), Goap::Fact{Key::Mud(4), 0}), pre.end());
    EXPECT_NE(std::find(pre.begin(), pre.end(), Goap::Fact{Key::Mud(6), 0}), pre.end());

    // Without the one at its wheel: three wheels, and only two pairs of hands
    world.mEntities[4].mGone = true;
    EXPECT_FALSE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
}

TEST(BotDomain, AMudokonBehindTheDoorCannotHelpOpenIt)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world = TwoWheels();
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 4, 300, 80, 324, 100));
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    // It would have to come through the door the wheels open
    EXPECT_FALSE(Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions).mFound);
}

TEST(BotDomain, KillsOnlyWhenAskedAndNotWhatTheWayOutNeeds)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world;
    Entity needed = MakeEntity(EntityKind::eMudokon, 1002, 100, 80, 124, 100);
    needed.mSwitch = 5;
    world.mEntities.push_back(needed);
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 1004, 25, 80, 49, 100));
    Entity portal = MakeEntity(EntityKind::eBirdPortal, 1003, 150, 80, 174, 104);
    portal.mRescuePortal = true;
    world.mEntities.push_back(portal);

    const Domain peaceful = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    EXPECT_FALSE(Goap::Planner::Find(peaceful.mStart, {{Key::Mud(1004), 2}}, peaceful.mActions).mFound);

    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr, true);
    // The one that is only in the way: killed, and the way out is still there
    Goap::Plan kill = Goap::Planner::Find(domain.mStart, {{Key::Mud(1004), 2}}, domain.mActions);
    ASSERT_TRUE(kill.mFound) << kill.mMiss;
    EXPECT_EQ(domain.mSteps[domain.mActions[kill.mSteps.back()].mTag].mKind, StepKind::eKill);
    EXPECT_TRUE(Goap::Planner::Find(Goap::Planner::Apply(domain.mStart, kill, domain.mActions), {{Key::kExited, 1}}, domain.mActions).mFound);
    // The one whose rescue opens the door: it can be killed, and then there's no way out
    kill = Goap::Planner::Find(domain.mStart, {{Key::Mud(1002), 2}}, domain.mActions);
    ASSERT_TRUE(kill.mFound) << kill.mMiss;
    EXPECT_FALSE(Goap::Planner::Find(Goap::Planner::Apply(domain.mStart, kill, domain.mActions), {{Key::kExited, 1}}, domain.mActions).mFound);
}

TEST(BotDomain, OnlySwitchesSomethingWaitsForAreWorthTrying)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world;
    Entity opens = MakeEntity(EntityKind::eLever, 1, 25, 80, 99, 104);
    opens.mSwitch = 5;
    opens.mOp = 2;
    // Nothing in the path is on switch 66: a trap, for all the bot knows
    Entity trap = MakeEntity(EntityKind::eLever, 2, 100, 80, 174, 104);
    trap.mSwitch = 66;
    trap.mOp = 2;
    world.mEntities = {opens, trap};
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const std::vector<Goap::Fact> worth = domain.Reachable();
    ASSERT_EQ(worth.size(), 1u);
    EXPECT_EQ(worth[0].mKey, Key::Switch(5));
    EXPECT_EQ(worth[0].mValue, 1);
}

TEST(BotDomain, ALevelLoaderIsAnExitThatNeedsItsSwitch)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature loader = Feature(FeatureKind::eLevelLoader, 300, 0, 324, 24);
    loader.mSwitch = 9;
    loader.mDestPath = 2;
    in.mFeatures.push_back(loader);
    NavGraph g;
    g.Build(in);
    ASSERT_EQ(g.Exits().size(), 1u);
    EXPECT_LT(g.Exits()[0].mNode, 0);

    World world;
    Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    EXPECT_FALSE(Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions).mFound);
    Entity lever = MakeEntity(EntityKind::eLever, 1, 25, 80, 99, 104);
    lever.mSwitch = 9;
    world.mEntities.push_back(lever);
    domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss;
    EXPECT_EQ(plan.mSteps.size(), 2u);
}

TEST(BotNavGraph, AFallIntoAWellComesOutWhereTheWellThrows)
{
    // A floor that ends over a deep shaft, a well at the bottom under its end, and a ledge the well throws to:
    // it aims at a point in the air just short of the ledge
    NavInput in;
    in.mIsAe = true;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(100, 900, 300, 900, 0));
    in.mLines.push_back(Line(450, 840, 600, 840, 0));
    NavFeature well = Feature(FeatureKind::eWellLocal, 250, 880, 274, 900);
    well.mC = 425;
    well.mD = 760;
    in.mFeatures.push_back(well);
    NavGraph g;
    g.Build(in);

    const s32 top = g.NodeAt(187, 100, false);
    const s32 ledge = g.NodeAt(462, 840, false);
    const s32 bottom = g.NodeAt(262, 900, false);
    ASSERT_GE(top, 0);
    ASSERT_GE(ledge, 0);
    ASSERT_GE(bottom, 0);
    // The fall would kill, but for the well
    EXPECT_TRUE(HasEdge(g, top, ledge, EdgeKind::eFall));
    // And used from beside it, the well goes to the same place
    EXPECT_TRUE(HasEdge(g, bottom, ledge, EdgeKind::eWell));
}

TEST(BotTasks, ASlogTrapNeedsWallsAndTheirLeverOnTheFloor)
{
    NavInput in;
    in.mLines.push_back(Line(0, 500, 1000, 500, 0));
    NavFeature wall = Feature(FeatureKind::eBarrier, 425, 500, 449, 524);
    wall.mSwitch = 35;
    in.mFeatures.push_back(wall);
    wall.mX1 = 475;
    wall.mX2 = 499;
    in.mFeatures.push_back(wall);
    NavGraph g;
    g.Build(in);
    World world;
    Entity lever = MakeEntity(EntityKind::eLever, 5, 275, 480, 351, 504);
    lever.mSwitch = 35;
    world.mEntities.push_back(lever);

    SlogTrapTask::Setup setup;
    ASSERT_TRUE(SlogTrapTask::Find(g, world, 500, false, setup));
    EXPECT_EQ(setup.mLever, 5u);
    EXPECT_EQ(setup.mX1, 425);
    EXPECT_EQ(setup.mX2, 499);
    EXPECT_EQ(setup.mLeverSide, -1);
    // Not from another floor, and not with a lever for something else
    EXPECT_FALSE(SlogTrapTask::Find(g, world, 380, false, setup));
    world.mEntities[0].mSwitch = 36;
    EXPECT_FALSE(SlogTrapTask::Find(g, world, 500, false, setup));

    // A Slog on the floor above isn't coming; one dropping from it is, and one asleep never
    SlogAt slog;
    slog.mX = 600;
    slog.mY = 380;
    world.mSlogs.push_back(slog);
    EXPECT_TRUE(SlogTrapTask::AnyAwake(world));
    EXPECT_FALSE(SlogTrapTask::Coming(world, 500));
    world.mSlogs[0].mY = 440;
    EXPECT_TRUE(SlogTrapTask::Coming(world, 500));
    world.mSlogs[0].mAsleep = true;
    EXPECT_FALSE(SlogTrapTask::Coming(world, 500));
    EXPECT_FALSE(SlogTrapTask::AnyAwake(world));
}

TEST(BotTasks, QuietLastsUntilTheSligTurnsAtTheEndOfItsBeat)
{
    std::vector<SligAt> sligs(1);
    sligs[0].mX = 600;
    sligs[0].mY = 399;
    sligs[0].mFacingLeft = true;
    std::vector<SligBeat> beats = {{400, false, 300, 900}};

    // The Mudokon is at 800, Abe's screen is 750 to 1125: the Slig is off it, walking away to 300
    EXPECT_EQ(World::QuietFrames(sligs, beats, 800, 400, false, 750, 1125), 150);
    // Still on the screen it would turn to a sound
    EXPECT_EQ(World::QuietFrames(sligs, beats, 800, 400, false, 375, 1125), 0);
    // Facing that way it shoots at what the Mudokon says
    sligs[0].mFacingLeft = false;
    EXPECT_EQ(World::QuietFrames(sligs, beats, 800, 400, false, 750, 1125), 0);
    // A beat not seen yet can't be counted on
    sligs[0].mFacingLeft = true;
    beats[0].mMinX = 590;
    beats[0].mMaxX = 610;
    EXPECT_EQ(World::QuietFrames(sligs, beats, 800, 400, false, 750, 1125), 0);
    // Another floor, a sleeping Slig, no Slig: all the quiet in the world
    EXPECT_GT(World::QuietFrames(sligs, beats, 800, 520, false, 750, 1125), 10000);
    sligs[0].mAsleep = true;
    EXPECT_GT(World::QuietFrames(sligs, beats, 800, 400, false, 750, 1125), 10000);
    EXPECT_GT(World::QuietFrames({}, beats, 800, 400, false, 750, 1125), 10000);
}

TEST(BotTasks, AUxbUnderAHopIsFoundWhileItIsOn)
{
    World world;
    UxbAt uxb;
    uxb.mX = 1713;
    uxb.mY = 1400;
    world.mUxbs.push_back(uxb);

    // Either way over it
    EXPECT_NE(DisarmTask::Under(world, 1762, 1687, 1400, false), nullptr);
    EXPECT_NE(DisarmTask::Under(world, 1687, 1762, 1400, false), nullptr);
    // A hop beside it, one on another floor, and one over it once it is off
    EXPECT_EQ(DisarmTask::Under(world, 1687, 1612, 1400, false), nullptr);
    EXPECT_EQ(DisarmTask::Under(world, 1762, 1687, 1500, false), nullptr);
    world.mUxbs[0].mOff = true;
    EXPECT_EQ(DisarmTask::Under(world, 1762, 1687, 1400, false), nullptr);
}

TEST(BotTasks, ASligWatchesItsOwnFloorWhenAwake)
{
    World world;
    SligAt slig;
    slig.mX = 500;
    slig.mY = 399;
    world.mSligs.push_back(slig);

    // On its floor, a screen away or less
    EXPECT_NE(ClearSligTask::Watcher(world, 300, 400, false), nullptr);
    // The floor below, the far end of the floor, the other scale
    EXPECT_EQ(ClearSligTask::Watcher(world, 300, 500, false), nullptr);
    EXPECT_EQ(ClearSligTask::Watcher(world, 1200, 400, false), nullptr);
    EXPECT_EQ(ClearSligTask::Watcher(world, 300, 400, true), nullptr);

    world.mSligs[0].mAsleep = true;
    EXPECT_EQ(ClearSligTask::Watcher(world, 300, 400, false), nullptr);
}

TEST(BotTasks, AsASligWalksUpTurnsAndShoots)
{
    World world;
    SligAt me;
    me.mX = 500;
    me.mY = 400;
    me.mPossessed = true;
    world.mSligs.push_back(me);
    SligAt other;
    other.mX = 1100;
    other.mY = 399;
    world.mSligs.push_back(other);
    Entity mud = MakeEntity(EntityKind::eMudokon, 7, 300, 380, 324, 400);
    mud.mLive = true;
    mud.mLiveX = 312;
    mud.mLiveY = 400;

    // The other Slig first: too far to hit, so towards it
    s32 x = 0;
    ASSERT_TRUE(ClearSligTask::Target(world, me, &mud, x));
    EXPECT_EQ(x, 1100);
    EXPECT_EQ(ClearSligTask::AsSlig(me, &x), static_cast<u32>(Btn::eRight));
    // In range and facing it: shoot
    me.mX = 950;
    EXPECT_EQ(ClearSligTask::AsSlig(me, &x), static_cast<u32>(Btn::eThrow));
    // In range with its back to it: turn
    me.mFacingLeft = true;
    EXPECT_EQ(ClearSligTask::AsSlig(me, &x), static_cast<u32>(Btn::eRight));

    // With that one dead, the Mudokon that is to die; and no Mudokon when it isn't
    world.mSligs.pop_back();
    ASSERT_TRUE(ClearSligTask::Target(world, me, &mud, x));
    EXPECT_EQ(x, 312);
    EXPECT_FALSE(ClearSligTask::Target(world, me, nullptr, x));
    // One on another floor is out of its line of fire
    mud.mLiveY = 500;
    EXPECT_FALSE(ClearSligTask::Target(world, me, &mud, x));
    EXPECT_EQ(ClearSligTask::AsSlig(me, nullptr), 0u);
}

TEST(BotTasks, ASligShootsOverAMudokonThatHasDucked)
{
    World world;
    SligAt me;
    me.mX = 500;
    me.mY = 400;
    me.mPossessed = true;
    Entity mud = MakeEntity(EntityKind::eMudokon, 7, 600, 380, 624, 400);
    mud.mLive = true;
    mud.mLiveX = 612;
    mud.mLiveY = 400;
    mud.mLiveTop = 345;
    world.mEntities.push_back(mud);

    // Standing between the Slig and what it shoots at, it would take the bullet
    EXPECT_FALSE(ClearSligTask::LevelShotClear(world, me, 800, 0));
    // Not when it is the one to shoot, behind the target, or on the other side
    EXPECT_TRUE(ClearSligTask::LevelShotClear(world, me, 800, 7));
    EXPECT_TRUE(ClearSligTask::LevelShotClear(world, me, 560, 0));
    EXPECT_TRUE(ClearSligTask::LevelShotClear(world, me, 200, 0));
    // Ducked, it is under the gun
    world.mEntities[0].mLiveTop = 398;
    EXPECT_TRUE(ClearSligTask::LevelShotClear(world, me, 800, 0));
}

TEST(BotTasks, AShotFromBehindTakesWhatIsNearest)
{
    World world;
    SligAt me;
    me.mX = 3137;
    me.mY = 1361;
    me.mHalf = true;
    me.mPossessed = true;
    Entity mud = MakeEntity(EntityKind::eMudokon, 7, 3150, 1380, 3174, 1400);
    mud.mLive = true;
    mud.mLiveX = 3162;
    mud.mLiveY = 1400;
    world.mEntities.push_back(mud);

    // The Slig in front is nearer than the Mudokon and stays so
    EXPECT_TRUE(ClearSligTask::BehindShotClear(world, me, 3137, 3137));
    // Walking off towards the Mudokon, it won't be by the time the shot goes off
    EXPECT_FALSE(ClearSligTask::BehindShotClear(world, me, 3140, 3160));
    // Too far to the side to be hit at all
    EXPECT_FALSE(ClearSligTask::BehindShotClear(world, me, 3037, 3037));
    // Right behind the Mudokon it is the Mudokon that gets it
    me.mX = 3162;
    EXPECT_FALSE(ClearSligTask::BehindShotClear(world, me, 3170, 3170));
}

TEST(BotTasks, ATeleporterLeadsToAFloorNotSeenYet)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mLines.push_back(Line(400, 200, 700, 200, 0));
    NavGraph g;
    g.Build(in);
    World world;
    Entity a = MakeEntity(EntityKind::eTeleporter, 1, 100, 80, 124, 100);
    a.mCamera = 1;
    a.mInputs[0] = 2;
    a.mInputs[1] = 10;
    a.mInputs[2] = 11;
    world.mEntities.push_back(a);
    Entity b = MakeEntity(EntityKind::eTeleporter, 2, 500, 180, 524, 200);
    b.mCamera = 2;
    b.mInputs[0] = 1;
    b.mInputs[1] = 11;
    b.mInputs[2] = 10;
    world.mEntities.push_back(b);
    const s32 first = g.Nodes()[g.NodeAt(112, 100, false)].mLine;
    const s32 second = g.Nodes()[g.NodeAt(512, 200, false)].mLine;

    const Entity* dest = nullptr;
    const Entity* tele = ClearSligTask::NextTeleporter(g, world, 0, 300, 100, false, {first}, &dest);
    ASSERT_NE(tele, nullptr);
    EXPECT_EQ(tele->mId, 1u);
    ASSERT_NE(dest, nullptr);
    EXPECT_EQ(dest->mId, 2u);
    // Not back to where it has been, and not one that is off its floor
    EXPECT_EQ(ClearSligTask::NextTeleporter(g, world, 400, 700, 200, false, {first, second}, nullptr), nullptr);
    EXPECT_EQ(ClearSligTask::NextTeleporter(g, world, 150, 300, 100, false, {first}, nullptr), nullptr);
}

TEST(BotDomain, AMudokonIsSentThroughAPortalItCanWalkTo)
{
    // Two floors and a hoist between them: Abe gets to both portals, the Mudokon only to the one on its floor,
    // though the other is nearer
    NavInput in;
    in.mLines.push_back(Line(0, 200, 1600, 200, 0));
    in.mLines.push_back(Line(300, 100, 500, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eHoist, 350, 100, 374, 195));
    NavGraph g;
    g.Build(in);
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 1002, 400, 180, 424, 200));
    Entity up = MakeEntity(EntityKind::eBirdPortal, 1003, 400, 80, 424, 100);
    up.mRescuePortal = true;
    world.mEntities.push_back(up);
    Entity along = MakeEntity(EntityKind::eBirdPortal, 1004, 1500, 180, 1524, 200);
    along.mRescuePortal = true;
    world.mEntities.push_back(along);

    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    ASSERT_EQ(domain.mRescuable.size(), 1u);
    bool found = false;
    for (const DomainStep& step : domain.mSteps)
    {
        if (step.mKind == StepKind::eRescue)
        {
            found = true;
            EXPECT_EQ(step.mOther, 1004u);
        }
    }
    EXPECT_TRUE(found);
}

static NavInput CorridorWithMines()
{
    // Six mines in a row: too many to hop or to clear with a running jump
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    for (s32 i = 0; i < 6; i++)
    {
        NavFeature mine = Feature(FeatureKind::eHazard, 400 + i * 25, 80, 424 + i * 25, 104);
        mine.mSwitch = World::HazardSwitch(2000 + i);
        in.mFeatures.push_back(mine);
    }
    NavFeature exit = Feature(FeatureKind::eDoor, 700, 80, 724, 104);
    exit.mDestPath = 2;
    exit.mDestCamera = 1;
    in.mFeatures.push_back(exit);
    return in;
}

static World WorldWithMines(bool sack)
{
    World world;
    for (u32 i = 0; i < 6; i++)
    {
        Entity mine = MakeEntity(EntityKind::eMine, 2000 + i, 400 + i * 25, 80, 424 + i * 25, 104);
        mine.mSwitch = World::HazardSwitch(mine.mId);
        world.mEntities.push_back(mine);
    }
    if (sack)
    {
        world.mEntities.push_back(MakeEntity(EntityKind::eRockSack, 3000, 100, 10, 124, 34));
    }
    return world;
}

TEST(BotDomain, MinesInTheWayAreBlownUpWithRocksFromTheSack)
{
    NavGraph g;
    g.Build(CorridorWithMines());
    World world = WorldWithMines(true);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss << "\n" << domain.Text();
    EXPECT_EQ(domain.mSteps[domain.mActions[plan.mSteps.front()].mTag].mKind, StepKind::eRocks);
    EXPECT_EQ(domain.mSteps[domain.mActions[plan.mSteps.back()].mTag].mKind, StepKind::eExit);
    s32 blown = 0;
    for (s32 step : plan.mSteps)
    {
        blown += domain.mSteps[domain.mActions[step].mTag].mKind == StepKind::eBlow ? 1 : 0;
    }
    // (the last two can be hopped, so it need not be all six)
    EXPECT_GE(blown, 4);
    EXPECT_LE(blown, 6);

    // With rocks in hand the sack is left alone
    world.mRocks = 4;
    const Domain armed = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan armedPlan = Goap::Planner::Find(armed.mStart, {{Key::kExited, 1}}, armed.mActions);
    ASSERT_TRUE(armedPlan.mFound);
    EXPECT_EQ(armed.mSteps[armed.mActions[armedPlan.mSteps.front()].mTag].mKind, StepKind::eBlow);

    // And mines that are gone are floor
    world.mRocks = 0;
    for (Entity& e : world.mEntities)
    {
        e.mGone = e.mKind == EntityKind::eMine;
    }
    EXPECT_EQ(world.Switch(World::HazardSwitch(2001)), 1);
    const Domain clear = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan clearPlan = Goap::Planner::Find(clear.mStart, {{Key::kExited, 1}}, clear.mActions);
    ASSERT_TRUE(clearPlan.mFound);
    for (s32 step : clearPlan.mSteps)
    {
        const StepKind kind = clear.mSteps[clear.mActions[step].mTag].mKind;
        EXPECT_TRUE(kind == StepKind::eCross || kind == StepKind::eExit);
    }
}

TEST(BotDomain, NoSackNoWayThroughTheMines)
{
    NavGraph g;
    g.Build(CorridorWithMines());
    const World world = WorldWithMines(false);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    EXPECT_FALSE(Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions).mFound);
}

TEST(BotDomain, RocksAreThrownFromOutOfTheBlast)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 800, 200, 0));
    // A low tunnel over the right of it
    in.mLines.push_back(Line(500, 160, 800, 160, 3));
    in.mLines.push_back(Line(300, 100, 400, 100, 0));
    NavGraph g;
    g.Build(in);

    // On the floor: from out of the blast, which reaches four and a half cells
    const std::vector<ThrowSpot> level = Domain::ThrowSpots(g, 312, 200, false);
    ASSERT_FALSE(level.empty());
    EXPECT_TRUE(level.front().mSure);
    for (const ThrowSpot& spot : level)
    {
        // (the ledge over it is a place to throw down from, too)
        if (spot.mLevel == 0)
        {
            EXPECT_GE(spot.mCells, 5);
            EXPECT_EQ(g.Nodes()[spot.mNode].mY, 200);
        }
    }
    // On the ledge above: from under it, up
    const std::vector<ThrowSpot> above = Domain::ThrowSpots(g, 362, 100, false);
    ASSERT_FALSE(above.empty());
    EXPECT_EQ(above.front().mKind, ThrowSpot::eUp);
    EXPECT_EQ(above.front().mLevel, -1);
    EXPECT_EQ(g.Nodes()[above.front().mNode].mY, 200);
    // In the tunnel there is no standing up: crouched throws only from in there
    const std::vector<ThrowSpot> tunnel = Domain::ThrowSpots(g, 762, 200, false);
    ASSERT_FALSE(tunnel.empty());
    for (const ThrowSpot& spot : tunnel)
    {
        if (g.Nodes()[spot.mNode].mLow)
        {
            EXPECT_EQ(spot.mKind, ThrowSpot::eCrouched);
        }
    }
    EXPECT_EQ(tunnel.front().mKind, ThrowSpot::eCrouched);
}

// The flight of a rock as it was measured in the Mines (path 4, thrown from 962,460 to the right, a wall at 1100)
TEST(BotThrowArc, ARockFliesAsItWasMeasured)
{
    NavInput in;
    in.mLines.push_back(Line(750, 460, 1125, 460, 0));
    // (the wall stops short of the floor: the crouched throw went under it)
    in.mLines.push_back(Line(1100, 300, 1100, 400, 2));
    const auto at = [&](ThrowSpot::Kind kind, s32 frame)
    {
        const std::vector<ThrowArc::Point> flight = ThrowArc::Throw(in, 962, 460, false, false, kind).mPoints;
        return frame < static_cast<s32>(flight.size()) ? std::make_pair(flight[frame].mX, flight[frame].mY) : std::make_pair(-1, -1);
    };
    // Up: off the wall, down to the floor, a bounce
    EXPECT_EQ(at(ThrowSpot::eUp, 0), std::make_pair(973, 405));
    EXPECT_EQ(at(ThrowSpot::eUp, 10), std::make_pair(1073, 360));
    EXPECT_EQ(at(ThrowSpot::eUp, 13).first, 1100);
    EXPECT_EQ(at(ThrowSpot::eUp, 14), std::make_pair(1095, 370));
    EXPECT_EQ(at(ThrowSpot::eUp, 23), std::make_pair(1050, 452));
    EXPECT_EQ(at(ThrowSpot::eUp, 24), std::make_pair(1045, 460));
    EXPECT_EQ(at(ThrowSpot::eUp, 25), std::make_pair(1042, 453));
    EXPECT_EQ(at(ThrowSpot::eUp, 38), std::make_pair(1010, 460));
    // Forward
    EXPECT_EQ(at(ThrowSpot::eForward, 8), std::make_pair(1093, 377));
    EXPECT_EQ(at(ThrowSpot::eForward, 9), std::make_pair(1100, 377));
    EXPECT_EQ(at(ThrowSpot::eForward, 21), std::make_pair(1010, 460));
    // Down: one bounce, and too slow for a second: through the floor
    EXPECT_EQ(at(ThrowSpot::eDown, 8), std::make_pair(1053, 460));
    EXPECT_EQ(at(ThrowSpot::eDown, 9), std::make_pair(1058, 455));
    EXPECT_EQ(at(ThrowSpot::eDown, 18), std::make_pair(1103, 460));
    EXPECT_EQ(at(ThrowSpot::eDown, 19), std::make_pair(1108, 465));
    // Crouched: from lower and further forward
    EXPECT_EQ(at(ThrowSpot::eCrouched, 0), std::make_pair(984, 445));
    EXPECT_EQ(at(ThrowSpot::eCrouched, 10), std::make_pair(1084, 460));
    EXPECT_EQ(at(ThrowSpot::eCrouched, 15), std::make_pair(1109, 459));
    // Lob
    EXPECT_EQ(at(ThrowSpot::eLob, 31), std::make_pair(1066, 460));
}

// A mine goes off when the rock's box touches it: up to a dozen pixels over the mine's floor, not higher
TEST(BotThrowArc, ARockSetsOffTheMineItComesDownOnAndFliesOverTheNearOnes)
{
    NavInput in;
    in.mLines.push_back(Line(0, 460, 1500, 460, 0));
    const auto hits = [&](ThrowSpot::Kind kind, s32 mineX, bool left)
    {
        const ThrowArc::Point mine{mineX, 460};
        const ThrowArc::Flight flight = ThrowArc::Throw(in, 512, 460, false, left, kind, &mine);
        return flight.mHit >= 0 && flight.mSure;
    };
    // Crouched: over the mines up to three cells off, onto the ones four to six off
    EXPECT_FALSE(hits(ThrowSpot::eCrouched, 562, false));
    EXPECT_FALSE(hits(ThrowSpot::eCrouched, 587, false));
    EXPECT_TRUE(hits(ThrowSpot::eCrouched, 612, false));
    EXPECT_TRUE(hits(ThrowSpot::eCrouched, 637, false));
    EXPECT_TRUE(hits(ThrowSpot::eCrouched, 662, false));
    // To the left it is the same the other way round
    EXPECT_TRUE(hits(ThrowSpot::eCrouched, 387, true));
    // As measured (Mines path 4): thrown crouched from 3587, the rock was at 3679 and fifteen over the floor, then
    // on the mine at 3687, and came back off its side at half the speed
    NavInput mines;
    mines.mLines.push_back(Line(3400, 2280, 3750, 2280, 0));
    const ThrowArc::Point mine{3687, 2280};
    const ThrowArc::Flight flight = ThrowArc::Throw(mines, 3587, 2280, false, false, ThrowSpot::eCrouched, &mine);
    ASSERT_EQ(flight.mHit, 8);
    EXPECT_EQ(flight.mPoints[7].mX, 3679);
    EXPECT_EQ(flight.mPoints[7].mY, 2265);
    EXPECT_EQ(flight.mPoints[8].mX, 3679);
    EXPECT_EQ(flight.mPoints[8].mY, 2269);
    EXPECT_EQ(flight.mPoints[9].mX, 3674);
    // A lob comes down on a mine two cells off
    EXPECT_TRUE(hits(ThrowSpot::eLob, 612, false) || hits(ThrowSpot::eLob, 587, false) || hits(ThrowSpot::eLob, 637, false));
}

TEST(BotTasks, AThrowIsAimedWithTheDirectionHeld)
{
    EXPECT_EQ(BlowMineTask::AimBits(ThrowSpot::eUp, false), static_cast<u32>(Btn::eUp));
    EXPECT_EQ(BlowMineTask::AimBits(ThrowSpot::eDown, true), static_cast<u32>(Btn::eDown));
    // Forward is the way he faces, the lob the other way
    EXPECT_EQ(BlowMineTask::AimBits(ThrowSpot::eForward, false), static_cast<u32>(Btn::eRight));
    EXPECT_EQ(BlowMineTask::AimBits(ThrowSpot::eForward, true), static_cast<u32>(Btn::eLeft));
    EXPECT_EQ(BlowMineTask::AimBits(ThrowSpot::eLob, false), static_cast<u32>(Btn::eLeft));
    EXPECT_EQ(BlowMineTask::AimBits(ThrowSpot::eCrouched, true), static_cast<u32>(Btn::eLeft));
}

TEST(BotDomain, ASideTripIsWeighedByWhatIsInReachThere)
{
    // A secret area: in by a door at the left, two Mudokons, one of them behind a wall, a portal, and the door
    // back out
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(300, 100, 400, 100, 0));
    NavFeature back = Feature(FeatureKind::eDoor, 0, 80, 24, 104);
    back.mDestPath = 1;
    back.mDestLevel = 5;
    in.mFeatures.push_back(back);
    NavGraph g;
    g.Build(in);
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 1, 100, 80, 124, 100));
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 2, 350, 80, 374, 100));
    Entity portal = MakeEntity(EntityKind::eBirdPortal, 3, 150, 80, 174, 104);
    portal.mRescuePortal = true;
    world.mEntities.push_back(portal);
    const auto leadsOn = [](const NavFeature& f)
    {
        return f.mDestPath == 1;
    };

    SideTrip trip = Domain::WeighSideTrip(g, world, g.NodeAt(12, 100, false), leadsOn);
    EXPECT_EQ(trip.mThere, 2);
    EXPECT_EQ(trip.mMudokons, 1);
    EXPECT_EQ(trip.mRescuable, 1);
    EXPECT_TRUE(trip.mWayBack);

    // No portal: something to kill, nothing to save
    world.mEntities.pop_back();
    trip = Domain::WeighSideTrip(g, world, g.NodeAt(12, 100, false), leadsOn);
    EXPECT_EQ(trip.mMudokons, 1);
    EXPECT_EQ(trip.mRescuable, 0);

    // A Mudokon already dealt with isn't a reason to go
    world.mEntities[0].mGone = true;
    EXPECT_EQ(Domain::WeighSideTrip(g, world, g.NodeAt(12, 100, false), leadsOn).mMudokons, 0);

    // The only door leads somewhere that is no way on: a one way trip
    EXPECT_FALSE(Domain::WeighSideTrip(g, world, g.NodeAt(12, 100, false), [](const NavFeature&)
                                       { return false; }).mWayBack);
}

// ---- The paths of a level ----

TEST(BotLevel, CountsThePathsToTheWayOut)
{
    // 1 -> 2 -> 3 -> another level, with 7 a dead end off 1 that only leads back
    std::vector<PathSummary> paths(4);
    paths[0].mPath = 1;
    paths[0].mExits = {{5, 2}, {5, 7}};
    paths[1].mPath = 2;
    paths[1].mExits = {{5, 3}};
    paths[2].mPath = 3;
    paths[2].mExits = {{6, 1}};
    paths[3].mPath = 7;
    paths[3].mMudokons = 12;
    paths[3].mExits = {{5, 1}};
    LevelGraph level;
    level.Build(5, paths);
    EXPECT_EQ(level.Distance(3), 0);
    EXPECT_EQ(level.Distance(2), 1);
    EXPECT_EQ(level.Distance(1), 2);
    EXPECT_EQ(level.Distance(7), 3);
    EXPECT_EQ(level.Distance(9), -1);
    EXPECT_EQ(level.Mudokons(7), 12);
    EXPECT_EQ(level.Mudokons(1), 0);
}

// ---- Pilot ----

// A stand-in for Abe: walks 5 pixels a frame while a direction is held, turns in 3 frames
struct FakeAbe final
{
    AbeState mState;
    s32 mTurning = 0;

    FakeAbe()
    {
        mState.mValid = true;
        mState.mMove = Move::eIdle;
        mState.mOnGround = true;
        mState.mX = 12;
        mState.mY = 100;
    }

    void Step(u32 bits)
    {
        mState.mFrame++;
        if (mTurning > 0)
        {
            if (--mTurning == 0)
            {
                mState.mFacingLeft = !mState.mFacingLeft;
                mState.mMove = Move::eIdle;
                mState.mMotion = 0;
            }
            return;
        }
        const bool left = (bits & Btn::eLeft) != 0;
        const bool right = (bits & Btn::eRight) != 0;
        if (left || right)
        {
            if (left != mState.mFacingLeft)
            {
                mTurning = 3;
                mState.mMove = Move::eTurn;
                mState.mMotion = 2;
                return;
            }
            mState.mMove = Move::eWalk;
            mState.mMotion = 1;
            mState.mX += left ? -5 : 5;
        }
        else if (mState.mMove == Move::eWalk)
        {
            // Stops on the grid, as the game does
            const s32 off = ((mState.mX - 12) % 25 + 25) % 25;
            if (off == 0)
            {
                mState.mMove = Move::eIdle;
                mState.mMotion = 0;
            }
            else
            {
                mState.mX += mState.mFacingLeft ? -5 : 5;
            }
        }
    }
};

static PilotStatus RunPrim(FakeAbe& abe, const Prim& prim, s32 maxFrames = 2000)
{
    Pilot pilot;
    pilot.Start(prim, abe.mState);
    for (s32 i = 0; i < maxFrames; i++)
    {
        u32 bits = 0;
        const PilotStatus status = pilot.Step(abe.mState, bits);
        if (status != PilotStatus::eRunning)
        {
            return status;
        }
        abe.Step(bits);
    }
    return PilotStatus::eRunning;
}

TEST(BotPilot, MoveToEndsInTheCell)
{
    FakeAbe abe;
    EXPECT_EQ(RunPrim(abe, Prim::MoveTo(212)), PilotStatus::eDone);
    EXPECT_EQ(abe.mState.mX, 212);
    EXPECT_EQ(abe.mState.mMove, Move::eIdle);
    // And back, which needs a turn
    EXPECT_EQ(RunPrim(abe, Prim::MoveTo(62)), PilotStatus::eDone);
    EXPECT_EQ(abe.mState.mX, 62);
    EXPECT_TRUE(abe.mState.mFacingLeft);
}

TEST(BotPilot, MoveToAlreadyThereIsDoneAtOnce)
{
    FakeAbe abe;
    EXPECT_EQ(RunPrim(abe, Prim::MoveTo(15), 5), PilotStatus::eDone);
    EXPECT_EQ(abe.mState.mX, 12);
}

TEST(BotPilot, FaceTurnsAndStops)
{
    FakeAbe abe;
    EXPECT_EQ(RunPrim(abe, Prim::Face(true)), PilotStatus::eDone);
    EXPECT_TRUE(abe.mState.mFacingLeft);
    EXPECT_EQ(abe.mState.mX, 12);
    EXPECT_EQ(RunPrim(abe, Prim::Face(true), 3), PilotStatus::eDone);
}

TEST(BotPilot, WaitHoldsItsButtonsForItsFrames)
{
    Pilot pilot;
    FakeAbe abe;
    pilot.Start(Prim::Wait(3, Btn::eChant), abe.mState);
    u32 bits = 0;
    for (s32 i = 0; i < 3; i++)
    {
        EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
        EXPECT_EQ(bits, static_cast<u32>(Btn::eChant));
    }
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eDone);
    EXPECT_EQ(bits, 0u);
}

TEST(BotPilot, PressFailsWhenNothingHappens)
{
    FakeAbe abe;
    EXPECT_EQ(RunPrim(abe, Prim::Press(Btn::eAction, 5)), PilotStatus::eFailed);
}

TEST(BotPilot, ADeadAbeFailsWhateverIsRunning)
{
    FakeAbe abe;
    Pilot pilot;
    pilot.Start(Prim::MoveTo(500), abe.mState);
    u32 bits = 0;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    abe.mState.mMove = Move::eDead;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eFailed);
    EXPECT_EQ(pilot.Why(), "dead");
    EXPECT_EQ(bits, 0u);
}

// Out of a well with the next move's buttons to hold: nothing while he goes in and flies, the buttons from when
// his feet touch, and done as soon as he is off (the game goes from the landing straight into the run then)
TEST(BotPilot, AWellLandsStraightIntoTheRunThatIsNext)
{
    FakeAbe abe;
    Pilot pilot;
    Prim press = Prim::Press(Btn::eUp);
    press.mAfter = Btn::eRight | Btn::eRun;
    pilot.Start(press, abe.mState);
    u32 bits = 0;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    EXPECT_EQ(bits, static_cast<u32>(Btn::eUp));
    // Into the well, still on the floor
    abe.mState.mMotion = 81;
    abe.mState.mMove = Move::eBusy;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    EXPECT_EQ(bits, 0u);
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    EXPECT_EQ(bits, 0u);
    // Shot out
    abe.mState.mOnGround = false;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    EXPECT_EQ(bits, 0u);
    // Down
    abe.mState.mOnGround = true;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    EXPECT_EQ(bits, static_cast<u32>(Btn::eRight | Btn::eRun));
    abe.mState.mMove = Move::eRun;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eDone);
    EXPECT_EQ(bits, static_cast<u32>(Btn::eRight | Btn::eRun));
}

// With nothing to hold he lands, stands up, and only then is it done
TEST(BotPilot, AWellWithNothingNextEndsStanding)
{
    FakeAbe abe;
    Pilot pilot;
    pilot.Start(Prim::Press(Btn::eUp), abe.mState);
    u32 bits = 0;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    abe.mState.mMotion = 81;
    abe.mState.mMove = Move::eBusy;
    abe.mState.mOnGround = false;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    abe.mState.mOnGround = true;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    EXPECT_EQ(bits, 0u);
    abe.mState.mMove = Move::eIdle;
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eRunning);
    EXPECT_EQ(pilot.Step(abe.mState, bits), PilotStatus::eDone);
    EXPECT_EQ(bits, 0u);
}

// A drill over a floor from x 300 to 500, parked at the left end; it goes out to the right and comes back
static SweepAt DrillAt(s32 offsetFromRight, s32 phase)
{
    SweepAt sweep;
    sweep.mId = 9;
    sweep.mSign = -1;
    sweep.mTravel = 200 * 256;
    sweep.mOffset = offsetFromRight * 256;
    sweep.mSpeed = 3 * 256;
    sweep.mPhase = phase;
    const s32 x = 500 - offsetFromRight;
    sweep.mX1 = x - 13;
    sweep.mX2 = x + 13;
    sweep.mY1 = 70;
    sweep.mY2 = 90;
    return sweep;
}

TEST(BotSweep, ADrillIsFollowedOutAndBack)
{
    // Parked at the left end with nothing to wait for: it sets off at once, to the right
    const SweepAt sweep = DrillAt(200, 0);
    s32 x1 = 0;
    s32 y1 = 0;
    s32 x2 = 0;
    s32 y2 = 0;
    EXPECT_FALSE(sweep.At(0, x1, y1, x2, y2));
    ASSERT_TRUE(sweep.At(11, x1, y1, x2, y2));
    EXPECT_EQ(x1, 287 + 30);
    // At the far end after 200 / 3 frames, and coming back after that
    ASSERT_TRUE(sweep.At(1 + 67, x1, y1, x2, y2));
    EXPECT_GE(x1, 487);
    ASSERT_TRUE(sweep.At(1 + 67 + 30, x1, y1, x2, y2));
    EXPECT_LT(x1, 487 - 80);
    // A place near the right end is safe only until it gets there
    EXPECT_FALSE(sweep.Hits(0, 40, 440, 40, 460, 100));
    EXPECT_TRUE(sweep.Hits(0, 70, 440, 40, 460, 100));
    // Higher up it is never reached
    EXPECT_FALSE(sweep.Hits(0, 300, 440, 0, 460, 60));

    // One that won't start again hurts nothing
    SweepAt stopped = DrillAt(200, 0);
    stopped.mWillStart = false;
    EXPECT_FALSE(stopped.Hits(0, 400, 0, 0, 1000, 1000));
    // One that waits is safe for as long as it does
    SweepAt waiting = DrillAt(200, 0);
    waiting.mPause = 50;
    EXPECT_FALSE(waiting.Hits(0, 50, 290, 40, 310, 100));
    EXPECT_TRUE(waiting.Hits(0, 60, 290, 40, 310, 100));
}

TEST(BotSweep, TheWayPastADrillIsTakenBehindIt)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 300, 70, 500, 94));
    NavGraph g;
    g.Build(in);
    EXPECT_GE(g.Nodes()[g.NodeAt(312, 100, false)].mSweep, 0);
    EXPECT_GE(g.Nodes()[g.NodeAt(512, 100, false)].mSweep, 0);
    EXPECT_LT(g.Nodes()[g.NodeAt(262, 100, false)].mSweep, 0);
    EXPECT_LT(g.Nodes()[g.NodeAt(537, 100, false)].mSweep, 0);

    World world;
    Entity drill = MakeEntity(EntityKind::eDrill, 9, 300, 70, 500, 94);
    drill.mSwitch = 5;
    drill.mInputs[0] = 1;
    drill.mInputs[1] = 1;
    world.mEntities.push_back(drill);
    EXPECT_EQ(GoToTask::SweepOver(g, world, g.NodeAt(262, 100, false)), nullptr);
    ASSERT_NE(GoToTask::SweepOver(g, world, g.NodeAt(312, 100, false)), nullptr);

    // Three cells into its reach and out again, at a walk
    std::vector<s32> path;
    const s32 from = g.NodeAt(262, 100, false);
    ASSERT_TRUE(g.FindPath(from, g.NodeAt(337, 100, false), nullptr, path));
    ASSERT_EQ(path.size(), 3u);
    // With the drill over those cells but on its way out, faster than he walks: it is gone before he is there
    world.mSweeps.push_back(DrillAt(170, 1));
    EXPECT_TRUE(GoToTask::SweepsClear(g, world, path, path.size()));
    // Near the far end already: back before he is through
    world.mSweeps[0] = DrillAt(10, 1);
    world.mSweeps[0].mSpeed = 8 * 256;
    EXPECT_FALSE(GoToTask::SweepsClear(g, world, path, path.size()));
    // Well past them and still going away: yes
    world.mSweeps[0] = DrillAt(60, 1);
    EXPECT_TRUE(GoToTask::SweepsClear(g, world, path, path.size()));
    // Coming back and nearly there: no
    world.mSweeps[0] = DrillAt(120, 2);
    EXPECT_FALSE(GoToTask::SweepsClear(g, world, path, path.size()));
    // If Abe is taken to be a good deal later than worked out, the first is no good either way and the second
    // still is
    world.mSweeps[0] = DrillAt(60, 1);
    EXPECT_TRUE(GoToTask::SweepsClear(g, world, path, path.size(), 12));

    // Its switch on stops it (it started on): once parked the cells are floor like any other
    world.mSweeps[0] = DrillAt(200, 0);
    world.mSweeps[0].mWillStart = false;
    world.mSwitches[5] = 1;
    EXPECT_FALSE(world.SweepRuns(world.mEntities[0]));
    EXPECT_EQ(GoToTask::SweepOver(g, world, g.NodeAt(312, 100, false)), nullptr);
    // ...but not while it is still finishing its stroke
    world.mSweeps[0].mPhase = 2;
    EXPECT_NE(GoToTask::SweepOver(g, world, g.NodeAt(312, 100, false)), nullptr);
}

TEST(BotSweep, NoThrowingFromWhereADrillComesBy)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 300, 70, 500, 94));
    NavGraph g;
    g.Build(in);
    World world;
    Entity drill = MakeEntity(EntityKind::eDrill, 9, 300, 70, 500, 94);
    drill.mSwitch = 5;
    drill.mInputs[0] = 1;
    drill.mInputs[1] = 1;
    world.mEntities.push_back(drill);
    // A mine at 562: five cells to its left is under the drill
    const auto hasSpotAt = [&](s32 x)
    {
        for (const ThrowSpot& spot : Domain::ThrowSpots(g, 562, 100, false, &world))
        {
            if (g.Nodes()[spot.mNode].mX == x)
            {
                return true;
            }
        }
        return false;
    };
    EXPECT_FALSE(hasSpotAt(437));
    EXPECT_TRUE(hasSpotAt(687));
    world.mSwitches[5] = 1;
    EXPECT_TRUE(hasSpotAt(437));
}

// ---- Trap doors, drills overhead, blind Mudokons, lift shafts ----

TEST(BotNav, ATrapDoorIsFloorWhileItIsShut)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(250, 100, 400, 100, 0));
    in.mLines.push_back(Line(0, 200, 400, 200, 0));
    NavFeature trap = Feature(FeatureKind::eTrapDoor, 200, 100, 250, 120);
    trap.mSwitch = 7;
    trap.mStartClosed = true;
    in.mFeatures.push_back(trap);
    NavGraph g;
    g.Build(in);
    const s32 before = g.NodeAt(187, 100, false);
    const s32 on = g.NodeAt(212, 100, false);
    const s32 under = g.NodeAt(212, 200, false);
    ASSERT_GE(before, 0);
    ASSERT_GE(on, 0);
    ASSERT_GE(under, 0);
    EXPECT_EQ(g.Nodes()[on].mTrapDoor, 0);
    // Walked onto while its switch is off, fallen through once it is on
    bool walk = false;
    bool fall = false;
    for (const NavEdge& e : g.Edges())
    {
        s32 sw = 0;
        s32 value = -1;
        if (e.mFrom == before && e.mTo == on && e.mKind == EdgeKind::eWalk)
        {
            walk = g.Condition(e, &sw, &value) && sw == 7 && value == 0;
        }
        if (e.mFrom == on && e.mTo == under && e.mKind == EdgeKind::eFall)
        {
            fall = g.Condition(e, &sw, &value) && sw == 7 && value == 1;
        }
    }
    EXPECT_TRUE(walk);
    EXPECT_TRUE(fall);
}

TEST(BotNav, UnderADrillThatGoesOverheadIsRolled)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 300, 40, 500, 64));
    NavGraph g;
    g.Build(in);
    const s32 under = g.NodeAt(412, 100, false);
    ASSERT_GE(under, 0);
    EXPECT_TRUE(g.Nodes()[under].mLow);
    EXPECT_EQ(g.Nodes()[under].mSweepHigh, 0);
    EXPECT_EQ(g.Nodes()[under].mSweep, -1);
    EXPECT_FALSE(g.Nodes()[g.NodeAt(612, 100, false)].mLow);

    // One that comes lower gets him crouched too: its cells are crossed when it is away
    in.mFeatures[0] = Feature(FeatureKind::eSweep, 300, 70, 500, 94);
    g.Build(in);
    EXPECT_EQ(g.Nodes()[g.NodeAt(412, 100, false)].mSweep, 0);
    EXPECT_EQ(g.Nodes()[g.NodeAt(412, 100, false)].mSweepHigh, -1);
    EXPECT_FALSE(g.Nodes()[g.NodeAt(412, 100, false)].mLow);
}

TEST(BotDomain, ABlindMudokonsWayNeedsTheDrillOnItStopped)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    NavFeature sweep = Feature(FeatureKind::eSweep, 300, 40, 500, 64);
    sweep.mId = 9;
    in.mFeatures.push_back(sweep);
    NavGraph g;
    g.Build(in);
    World world;
    Entity drill = MakeEntity(EntityKind::eDrill, 9, 300, 40, 500, 64);
    drill.mSwitch = 5;
    drill.mInputs[0] = 1;
    drill.mInputs[1] = 1;
    world.mEntities.push_back(drill);
    Entity portal = MakeEntity(EntityKind::eBirdPortal, 20, 700, 80, 724, 104);
    portal.mRescuePortal = true;
    const s32 from = g.NodeAt(112, 100, false);
    std::vector<s32> way;
    // There is a way once the drill is stopped: not as things are
    EXPECT_TRUE(Domain::BlindWay(g, world, from, portal, false, way));
    EXPECT_FALSE(way.empty());
    EXPECT_FALSE(Domain::BlindWay(g, world, from, portal, true, way));
    world.mSwitches[5] = 1;
    ASSERT_TRUE(Domain::BlindWay(g, world, from, portal, true, way));
    ASSERT_FALSE(way.empty());
    // It ends where the Mudokon would see the portal open: within a screen of it
    EXPECT_LT(std::abs(g.Nodes()[g.Edges()[way.back()].mTo].mX - 712), 300);
    // From there it is there already
    EXPECT_TRUE(Domain::BlindWay(g, world, g.NodeAt(612, 100, false), portal, true, way));
    EXPECT_TRUE(way.empty());
}

TEST(BotDomain, APortalIsStoodByOnItsOwnSideOfAWall)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    in.mLines.push_back(Line(75, 0, 75, 100, 1));
    NavGraph g;
    g.Build(in);
    // Three cells to its left is behind the wall
    const Entity portal = MakeEntity(EntityKind::eBirdPortal, 20, 125, 80, 149, 104);
    const s32 node = Domain::UseNode(g, portal, nullptr);
    ASSERT_GE(node, 0);
    EXPECT_GT(g.Nodes()[node].mX, 137);
}

TEST(BotNav, ALiftsShaftIsHoppedWhileTheLiftIsAway)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mLines.push_back(Line(375, 100, 700, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 300, 100, 375, 124));
    NavFeature ledge = Feature(FeatureKind::eEdge, 375, 100, 399, 124);
    ledge.mA = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    const s32 from = g.NodeAt(287, 100, false);
    const s32 to = g.NodeAt(387, 100, false);
    ASSERT_GE(from, 0);
    ASSERT_GE(to, 0);
    bool hop = false;
    for (const NavEdge& e : g.Edges())
    {
        hop = hop || (e.mFrom == from && e.mTo == to && e.mKind == EdgeKind::eHop && e.mShaft == 0);
    }
    EXPECT_TRUE(hop);

    // The lift is where it starts until it is seen somewhere else
    World world;
    EXPECT_TRUE(world.LiftAtStop(300, 375, 100, true));
    EXPECT_FALSE(world.LiftAtStop(300, 375, 100, false));
    world.mLiftsSeen.push_back({304, 371, 300});
    EXPECT_FALSE(world.LiftAtStop(300, 375, 100, true));
    world.mLifts.push_back({304, 371, 100});
    EXPECT_TRUE(world.LiftAtStop(300, 375, 100, false));
    // (a lift of another shaft that only partly overlaps this one is not this one's)
    World two;
    two.mLifts.push_back({354, 421, 100});
    EXPECT_FALSE(two.LiftAtStop(300, 375, 100, false));
    EXPECT_TRUE(two.LiftAtStop(300, 375, 100, true));
    EXPECT_TRUE(two.LiftAtStop(350, 425, 100, false));
}

TEST(BotSweep, ARollFitsUnderADrillWhereAWalkNeverDoes)
{
    // A drill that comes down on the cell at 412, with a stroke so short it is never above a standing head
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 400, 20, 424, 104));
    NavGraph g;
    g.Build(in);
    World world;
    SweepAt sweep;
    sweep.mId = 9;
    sweep.mVertical = true;
    sweep.mSign = -1;
    sweep.mTravel = 70 * 256;
    sweep.mSpeed = 5 * 256;
    sweep.mPhase = 2;
    sweep.mOffset = 0;
    sweep.mX1 = 400;
    sweep.mX2 = 424;
    sweep.mY1 = 94;
    sweep.mY2 = 100;
    world.mSweeps.push_back(sweep);
    std::vector<s32> path;
    ASSERT_TRUE(g.FindPath(g.NodeAt(387, 100, false), g.NodeAt(437, 100, false), [](const NavEdge& e)
                           { return e.mCost; }, path));
    bool walks = false;
    bool rolls = false;
    for (s32 late = 0; late < 30; late++)
    {
        walks = walks || GoToTask::SweepsClear(g, world, path, path.size(), late, false);
        rolls = rolls || GoToTask::SweepsClear(g, world, path, path.size(), late, true);
    }
    EXPECT_FALSE(walks);
    EXPECT_TRUE(rolls);
}

TEST(BotDomain, WheelsThatStayOnAreTurnedOneAfterTheOther)
{
    // Mines path 6: three wheels and a syncer on a slam door, and nobody to help
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world = TwoWheels();
    world.mEntities[0].mHoldOnly = false;
    world.mEntities[1].mHoldOnly = false;
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss << "\n" << domain.Text();
    s32 uses = 0;
    for (s32 step : plan.mSteps)
    {
        uses += domain.mSteps[domain.mActions[step].mTag].mKind == StepKind::eUse ? 1 : 0;
    }
    EXPECT_EQ(uses, 2);
}

TEST(BotNav, ATeleporterIsAWayToTheOneItLeadsTo)
{
    // A half size floor with a teleporter that leads to one on the full size floor below (Mines path 10)
    NavInput in;
    in.mIsAe = true;
    in.mLines.push_back(Line(0, 100, 200, 100, 4));
    in.mLines.push_back(Line(0, 300, 400, 300, 0));
    NavFeature up = Feature(FeatureKind::eTeleporter, 87, 80, 100, 104);
    up.mHalf = true;
    up.mA = 0;
    up.mB = 1;
    up.mSwitch = 217;
    up.mCamera = 26;
    up.mDestCamera = 26;
    in.mFeatures.push_back(up);
    NavFeature down = Feature(FeatureKind::eTeleporter, 300, 280, 324, 304);
    down.mA = 1;
    down.mB = 0;
    down.mSwitch = 218;
    down.mCamera = 26;
    down.mDestCamera = 26;
    in.mFeatures.push_back(down);
    NavGraph g;
    g.Build(in);

    const s32 to = g.NodeAt(312, 300, false);
    ASSERT_GE(to, 0);
    s32 from = -1;
    for (const NavEdge& e : g.Edges())
    {
        if (e.mKind == EdgeKind::eTeleport && e.mTo == to)
        {
            from = e.mFrom;
        }
    }
    ASSERT_GE(from, 0);
    EXPECT_TRUE(g.Nodes()[from].mHalf);
    EXPECT_TRUE(up.Contains(g.Nodes()[from].mX, g.Nodes()[from].mY));
    // And the two floors are one way round joined: there's a route across
    std::vector<s32> path;
    EXPECT_TRUE(g.FindPath(g.NodeAt(12, 100, true), to, nullptr, path));
}

TEST(BotNav, TheWayOutCanBeACrawl)
{
    // The floor goes on under a 40 high ceiling, and the path ends in there (Mines path 6)
    NavInput in;
    in.mLines.push_back(Line(0, 100, 300, 100, 0));
    in.mLines.push_back(Line(200, 60, 300, 60, 3));
    in.mLines.push_back(Line(200, 0, 200, 60, 2));
    NavFeature out = Feature(FeatureKind::ePathExit, 225, 80, 400, 200);
    out.mAnyScale = true;
    out.mDestPath = 2;
    out.mDestLevel = 3;
    in.mFeatures.push_back(out);
    NavGraph g;
    g.Build(in);

    const s32 under = g.NodeAt(237, 100, false);
    ASSERT_GE(under, 0);
    EXPECT_TRUE(g.Nodes()[under].mLow);
    bool found = false;
    for (const NavExit& x : g.Exits())
    {
        found = found || (x.mNode >= 0 && g.Nodes()[x.mNode].mLow && g.Input().mFeatures[x.mFeature].mKind == FeatureKind::ePathExit);
    }
    EXPECT_TRUE(found);
}

TEST(BotNav, ATeleporterItsPlateSetsOffIsTakenFromThePlate)
{
    // The Mudanchee trial: a teleporter three cells wide with a foot switch in the middle of it that sets it
    // off. Stood beside the plate nothing happens.
    NavInput in;
    in.mIsAe = true;
    in.mLines.push_back(Line(0, 300, 400, 300, 0));
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature tele = Feature(FeatureKind::eTeleporter, 100, 220, 175, 304);
    tele.mA = 0;
    tele.mB = 1;
    tele.mSwitch = 195;
    tele.mCamera = 5;
    tele.mDestCamera = 5;
    in.mFeatures.push_back(tele);
    NavFeature out = Feature(FeatureKind::eTeleporter, 300, 20, 374, 104);
    out.mA = 1;
    out.mB = 0;
    out.mCamera = 5;
    out.mDestCamera = 5;
    in.mFeatures.push_back(out);
    NavFeature plate = Feature(FeatureKind::ePlate, 125, 280, 149, 304);
    plate.mSwitch = 195;
    in.mFeatures.push_back(plate);
    NavGraph g;
    g.Build(in);

    const s32 to = g.NodeAt(337, 100, false);
    ASSERT_GE(to, 0);
    EXPECT_TRUE(HasEdge(g, g.NodeAt(137, 300, false), to, EdgeKind::eTeleport));
    EXPECT_FALSE(HasEdge(g, g.NodeAt(112, 300, false), to, EdgeKind::eTeleport));
    EXPECT_FALSE(HasEdge(g, g.NodeAt(162, 300, false), to, EdgeKind::eTeleport));
}

TEST(BotNav, ATeleporterPutsHimOnTheFloorInItsBox)
{
    // The Mudanchee trial: one teleporter right under the floor of the one that leads to it. Where he comes out is
    // the floor in its box, not the floor its top touches.
    NavInput in;
    in.mIsAe = true;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    in.mLines.push_back(Line(0, 200, 400, 200, 0));
    NavFeature up = Feature(FeatureKind::eTeleporter, 100, 20, 175, 100);
    up.mA = 0;
    up.mB = 1;
    up.mCamera = 5;
    up.mDestCamera = 5;
    in.mFeatures.push_back(up);
    NavFeature down = Feature(FeatureKind::eTeleporter, 100, 116, 175, 206);
    down.mA = 1;
    down.mB = 2;
    down.mCamera = 5;
    down.mDestCamera = 5;
    in.mFeatures.push_back(down);
    NavGraph g;
    g.Build(in);

    const s32 from = g.NodeAt(137, 100, false);
    const s32 to = g.NodeAt(137, 200, false);
    ASSERT_GE(from, 0);
    ASSERT_GE(to, 0);
    EXPECT_TRUE(HasEdge(g, from, to, EdgeKind::eTeleport));
}

TEST(BotNav, RolledOffOverAGapHeComesDownOnTheFloorBeyond)
{
    // The Mudanchee trial: the upper floor ends under a low wall with a pit below its end, and the floor a level
    // down starts two cells further out. Rolled off he clears the pit (measured: off 587,360, down on 662,460).
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(150, 60, 200, 60, 3));
    in.mLines.push_back(Line(150, 0, 150, 60, 2));
    in.mLines.push_back(Line(250, 200, 500, 200, 0));
    NavGraph g;
    g.Build(in);

    const s32 under = g.NodeAt(187, 100, false);
    const s32 beyond = g.NodeAt(262, 200, false);
    ASSERT_GE(under, 0);
    ASSERT_GE(beyond, 0);
    EXPECT_TRUE(g.Nodes()[under].mLow);
    EXPECT_TRUE(HasEdge(g, under, beyond, EdgeKind::eFall));
    // (walked off the end of a floor with room to stand he comes straight down, into the pit: no way)
    NavInput open;
    open.mLines.push_back(Line(0, 100, 200, 100, 0));
    open.mLines.push_back(Line(250, 200, 500, 200, 0));
    NavGraph h;
    h.Build(open);
    EXPECT_FALSE(HasEdge(h, h.NodeAt(187, 100, false), h.NodeAt(262, 200, false), EdgeKind::eFall));
}

TEST(BotNav, TheWayOutCanBeADropOffTheEndOfAFloor)
{
    // The Mudanchee trial's first path ends in a drop: the next path begins under the end of the last floor
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    NavFeature out = Feature(FeatureKind::ePathExit, 150, 160, 330, 280);
    out.mAnyScale = true;
    out.mDestPath = 10;
    out.mDestLevel = 3;
    in.mFeatures.push_back(out);
    NavGraph g;
    g.Build(in);

    const s32 end = g.NodeAt(187, 100, false);
    ASSERT_GE(end, 0);
    s32 exits = 0;
    for (const NavExit& x : g.Exits())
    {
        if (x.mNode >= 0 && g.Input().mFeatures[x.mFeature].mKind == FeatureKind::ePathExit)
        {
            exits++;
            EXPECT_EQ(x.mNode, end);
        }
    }
    EXPECT_EQ(exits, 1);
}

TEST(BotNav, FromALedgeTheHoistOfTheLedgeAcrossIsCaughtAtARunningJump)
{
    // The Mudanchee trial's last room: two ledges a level over the floor, five cells apart, each with a hoist at
    // its inner end. The left one is three cells long, too short for the run-up a jump to a landing wants.
    NavInput in;
    in.mLines.push_back(Line(0, 200, 300, 200, 0));
    in.mLines.push_back(Line(0, 100, 75, 100, 0));
    in.mLines.push_back(Line(175, 100, 300, 100, 0));
    NavFeature left = Feature(FeatureKind::eHoist, 50, 100, 75, 200);
    left.mDir = 0;
    in.mFeatures.push_back(left);
    NavFeature right = Feature(FeatureKind::eHoist, 175, 100, 199, 200);
    right.mDir = 1;
    in.mFeatures.push_back(right);
    NavGraph g;
    g.Build(in);

    const s32 from = g.NodeAt(37, 100, false);
    const s32 to = g.NodeAt(187, 100, false);
    ASSERT_GE(from, 0);
    ASSERT_GE(to, 0);
    const NavEdge* over = FindEdge(g, from, to, EdgeKind::eRunJump);
    ASSERT_NE(over, nullptr);
    EXPECT_EQ(over->mX, 62);
    EXPECT_FALSE(over->mLeft);
    // And back: off at 187 to the hoist at 62
    const NavEdge* back = FindEdge(g, g.NodeAt(212, 100, false), g.NodeAt(62, 100, false), EdgeKind::eRunJump);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(back->mX, 187);
    EXPECT_TRUE(back->mLeft);
    // Not with a hoist that faces the other way at the far end: there is nothing to catch
    in.mFeatures[1].mDir = 0;
    NavGraph h;
    h.Build(in);
    EXPECT_EQ(FindEdge(h, h.NodeAt(37, 100, false), h.NodeAt(187, 100, false), EdgeKind::eRunJump), nullptr);
}

TEST(BotVault, ATeleporterMadeOfSwitchesWantsTheOthersOnFirst)
{
    // The Mudanchee trial: the teleporter to the last room has a switch that is on when three locks are slapped
    // and the plate in it is stood on. The plate is his to step on; the locks have to be done before.
    NavInput in;
    in.mIsAe = true;
    in.mLines.push_back(Line(0, 300, 400, 300, 0));
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature tele = Feature(FeatureKind::eTeleporter, 100, 220, 175, 304);
    tele.mA = 0;
    tele.mB = 1;
    tele.mSwitch = 197;
    tele.mCamera = 5;
    tele.mDestCamera = 5;
    in.mFeatures.push_back(tele);
    NavFeature out = Feature(FeatureKind::eTeleporter, 300, 20, 374, 104);
    out.mA = 1;
    // (one way: the room it leads to is another part of the path)
    out.mB = 2;
    out.mCamera = 5;
    out.mDestCamera = 5;
    in.mFeatures.push_back(out);
    NavGraph g;
    g.Build(in);
    World world;
    Entity logic = MakeEntity(EntityKind::eSwitchLogic, 1, 0, 0, 24, 24);
    logic.mSwitch = 197;
    logic.mInputs[0] = 198;
    logic.mInputs[1] = 154;
    logic.mInputs[2] = 159;
    world.mEntities.push_back(logic);
    Entity plate = MakeEntity(EntityKind::eFootSwitch, 2, 125, 280, 149, 304);
    plate.mSwitch = 159;
    world.mEntities.push_back(plate);

    s32 crossings = 0;
    for (const Goap::Action& a : Domain::Build(g, world, g.NodeAt(12, 300, false), nullptr).mActions)
    {
        if (a.mName.rfind("Cross teleport", 0) != 0)
        {
            continue;
        }
        crossings++;
        const auto wants = [&a](s32 sw)
        {
            return std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::Switch(sw), 1}) != a.mPre.end();
        };
        EXPECT_TRUE(wants(198)) << a.mName;
        EXPECT_TRUE(wants(154)) << a.mName;
        EXPECT_FALSE(wants(159)) << a.mName;
    }
    EXPECT_GT(crossings, 0);
}

TEST(BotTasks, ABeamThatWaitsAtItsEndLeavesTimeForAStep)
{
    // A motion detector's beam, 20 wide, that goes between 925 and 1075 at 8 a frame and waits 16 frames at
    // each end. It has just got to the right end.
    World world;
    BeamAt beam;
    beam.mX1 = 1065;
    beam.mX2 = 1085;
    beam.mY1 = 700;
    beam.mY2 = 720;
    beam.mMinX = 925;
    beam.mMaxX = 1075;
    beam.mVel256 = -8 * 256;
    beam.mPause = 16;
    beam.mEndPause = 16;
    world.mBeams.push_back(beam);

    // A sneaked cell at 1012 (13 frames) is over before it has come back that far
    EXPECT_TRUE(world.BeamClear(1012, 720, false, -1, 25, 13));
    // Without the wait it would be on him
    world.mBeams[0].mPause = 0;
    EXPECT_FALSE(world.BeamClear(1012, 720, false, -1, 25, 13));
    // Under it he keeps still whatever it does
    world.mBeams[0].mPause = 16;
    EXPECT_FALSE(world.BeamClear(1070, 720, false, -1, 25, 13));
    // Another floor, another size: not his beam
    EXPECT_TRUE(world.BeamClear(1070, 500, false, -1, 25, 13));
    EXPECT_TRUE(world.BeamClear(1070, 720, true, -1, 25, 13));
}

TEST(BotTasks, ABeamThatIsCarriedAboutMayBeNearerByWhatItsCarrierMoves)
{
    // A Greeter's beam, 20 wide, that waits at the end of its sweep 28 from where he stands. The Greeter goes 3
    // a frame, and which way is its own business.
    World world;
    BeamAt beam;
    beam.mX1 = 1040;
    beam.mX2 = 1060;
    beam.mY1 = 700;
    beam.mY2 = 720;
    beam.mMinX = 900;
    beam.mMaxX = 1050;
    beam.mVel256 = -2 * 256;
    beam.mPause = 16;
    beam.mEndPause = 16;
    world.mBeams.push_back(beam);

    // Fixed where it is, a turn on the spot at 1012 (12 frames) is clear of it
    EXPECT_TRUE(world.BeamClear(1012, 720, false, 0, 0, 12));
    // Carried, it may be 36 nearer by the end of the turn: not clear
    world.mBeams[0].mDrift256 = 3 * 256;
    EXPECT_FALSE(world.BeamClear(1012, 720, false, 0, 0, 12));
    // From further off the same turn is
    EXPECT_TRUE(world.BeamClear(962, 720, false, 0, 0, 12));
    // and something shorter where he was is: it has not had the time
    EXPECT_TRUE(world.BeamClear(1012, 720, false, 0, 0, 5));
    // nor has it at the start of a run away from it, which outpaces it
    EXPECT_TRUE(world.BeamClear(1012, 720, false, -1, 100, 12));
}

TEST(BotTasks, ASligCannotShootBeforeItHasWalkedItsBeat)
{
    // A half size Slig that walks 1275 to 1550 and waits 85 frames before it shoots at what it sees. The
    // lever is at 1742, on the screen that starts at 1500.
    SligAt slig;
    slig.mHalf = true;
    slig.mX = 1404;
    slig.mBeatMin = 1275;
    slig.mBeatMax = 1550;
    slig.mSightDelay = 85;
    // Walking away: to the end of its beat, back to that screen, and its wait (1.2 pixels a frame)
    slig.mFacingLeft = true;
    EXPECT_EQ(slig.FramesUntilSees(1742, 1500, 1875), (129 + 225) * 10 / 12 + 85);
    // Coming: only as far as the screen
    slig.mFacingLeft = false;
    EXPECT_EQ(slig.FramesUntilSees(1742, 1500, 1875), 96 * 10 / 12 + 85);
    // What is on its beat it can come upon at any time, and a beat not known can't be counted on
    EXPECT_EQ(slig.FramesUntilSees(1400, 1125, 1500), 0);
    slig.mBeatMin = 0;
    slig.mBeatMax = 0;
    EXPECT_EQ(slig.FramesUntilSees(1742, 1500, 1875), 0);
}

TEST(BotDomain, TheMineCarNeedsTheDoorsAcrossItsTrackOpen)
{
    // A floor with the car's own floor 20 over it, a slam door across both, the car on the right of it
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    in.mLines.push_back(Line(0, 80, 400, 80, 11));
    NavFeature door = Feature(FeatureKind::eBarrier, 200, 80, 224, 104);
    door.mSwitch = 219;
    door.mStartClosed = true;
    in.mFeatures.push_back(door);
    NavGraph g;
    g.Build(in);
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eMineCar, 7, 300, 80, 324, 100));
    const Domain domain = Domain::Build(g, world, g.NodeAt(312, 100, false), nullptr);

    bool ride = false;
    for (const Goap::Action& a : domain.mActions)
    {
        if (domain.mSteps[a.mTag].mKind != StepKind::eRide)
        {
            continue;
        }
        ride = true;
        bool needsDoor = false;
        for (const Goap::Fact& f : a.mPre)
        {
            needsDoor = needsDoor || f == Goap::Fact{Key::Switch(219), 1};
        }
        EXPECT_TRUE(needsDoor) << a.mName;
    }
    EXPECT_TRUE(ride) << domain.Text();
}

// ---- What Necrum took (measured on its path 2 and 3) ----

TEST(BotNecrum, AHopOffALedgeEndComesDownFourCellsOutOnAFloorBelow)
{
    // As measured: from 662,880 to the left onto the ledge at 960, he ends at 562
    NavInput in;
    in.mLines.push_back(Line(650, 880, 850, 880, 0));
    in.mLines.push_back(Line(475, 960, 575, 960, 0));
    NavGraph g;
    g.Build(in);
    const s32 top = g.NodeAt(662, 880, false);
    const s32 low = g.NodeAt(562, 960, false);
    ASSERT_GE(top, 0);
    ASSERT_GE(low, 0);
    EXPECT_TRUE(HasEdge(g, top, low, EdgeKind::eHop));
    // Not off the other end, where there is nothing to come down on
    for (const NavEdge& e : g.Edges())
    {
        EXPECT_FALSE(e.mFrom == g.NodeAt(837, 880, false) && e.mKind == EdgeKind::eHop);
    }
}

TEST(BotNecrum, AHopDownAShortDropComesDownACellNearer)
{
    // As measured in the Barracks' rifle range: from 1437,380 to the left onto the floor at 420 that ends at 1362, he
    // ends in that last cell (he meets the floor at 1350, between where the frames before and after have him)
    NavInput in;
    in.mLines.push_back(Line(1425, 380, 1475, 380, 0));
    in.mLines.push_back(Line(1250, 420, 1375, 420, 0));
    NavGraph g;
    g.Build(in);
    const s32 top = g.NodeAt(1437, 380, false);
    ASSERT_GE(top, 0);
    EXPECT_TRUE(HasEdge(g, top, g.NodeAt(1362, 420, false), EdgeKind::eHop)) << g.Text();
    EXPECT_FALSE(HasEdge(g, top, g.NodeAt(1337, 420, false), EdgeKind::eHop));
}

TEST(BotNecrum, NoHopDownThroughADrill)
{
    // The same ledges with a drill between them (the Mines' path 4, off 2537,1400)
    NavInput in;
    in.mLines.push_back(Line(650, 880, 850, 880, 0));
    in.mLines.push_back(Line(475, 960, 575, 960, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 600, 800, 624, 960));
    NavGraph g;
    g.Build(in);
    EXPECT_FALSE(HasEdge(g, g.NodeAt(662, 880, false), g.NodeAt(562, 960, false), EdgeKind::eHop));
}

TEST(BotNecrum, AHopToALedgeFourCellsOffNeedsAHoistThereToCatch)
{
    // As measured: Necrum's path 6, from 1187,360 to 1287,360, where a hoist at the far ledge's end catches him as
    // he drops short of it. The Mines' path 4, from 2062,1400 at 2162,1400, has none: he went on down.
    NavInput in;
    in.mLines.push_back(Line(1050, 360, 1200, 360, 0));
    in.mLines.push_back(Line(1275, 360, 1350, 360, 0));
    in.mLines.push_back(Line(900, 460, 1500, 460, 0));
    {
        NavGraph g;
        g.Build(in);
        EXPECT_FALSE(HasEdge(g, g.NodeAt(1187, 360, false), g.NodeAt(1287, 360, false), EdgeKind::eHop));
    }
    NavFeature hoist = Feature(FeatureKind::eHoist, 1275, 360, 1299, 462);
    hoist.mDir = 1;
    in.mFeatures.push_back(hoist);
    NavGraph g;
    g.Build(in);
    EXPECT_TRUE(HasEdge(g, g.NodeAt(1187, 360, false), g.NodeAt(1287, 360, false), EdgeKind::eHop));
}

TEST(BotNecrum, AHopCatchesALedgeTwoCellsOffAndHigher)
{
    // As measured: from 487,960 to the left he catches the ledge at 437,880
    NavInput in;
    in.mLines.push_back(Line(475, 960, 575, 960, 0));
    in.mLines.push_back(Line(300, 880, 450, 880, 0));
    NavFeature edge = Feature(FeatureKind::eEdge, 425, 880, 449, 904);
    edge.mA = 1;
    edge.mDir = 0;
    in.mFeatures.push_back(edge);
    NavGraph g;
    g.Build(in);
    const s32 from = g.NodeAt(487, 960, false);
    const s32 to = g.NodeAt(437, 880, false);
    ASSERT_GE(from, 0);
    ASSERT_GE(to, 0);
    EXPECT_TRUE(HasEdge(g, from, to, EdgeKind::eHop));
}

TEST(BotNecrum, AFallThroughAWellInTheAirGoesWhereTheWellThrows)
{
    // A shaft with no floor, a well's box in it, and the ledge the well throws to
    NavInput in;
    in.mLines.push_back(Line(50, 620, 250, 620, 0));
    in.mLines.push_back(Line(50, 340, 100, 340, 0));
    NavFeature hoist = Feature(FeatureKind::eHoist, 50, 620, 74, 644);
    in.mFeatures.push_back(hoist);
    NavFeature well = Feature(FeatureKind::eWellLocal, 50, 980, 74, 1004);
    well.mC = 87;
    well.mD = 325;
    in.mFeatures.push_back(well);
    NavGraph g;
    g.Build(in);
    const s32 top = g.NodeAt(62, 620, false);
    const s32 ledge = g.NodeAt(87, 340, false);
    ASSERT_GE(top, 0);
    ASSERT_GE(ledge, 0);
    EXPECT_TRUE(HasEdge(g, top, ledge, EdgeKind::eHoistDown));
}

TEST(BotNecrum, AWellGoesOneWayWithItsSwitchOffAndAnotherWithItOn)
{
    // An express well that comes back out of itself until switch 100 is on, and then leads to a well on another
    // screen, which has nowhere to throw him and lets him out two cells to its right
    NavInput in;
    in.mLines.push_back(Line(300, 1240, 575, 1240, 0));
    in.mLines.push_back(Line(650, 980, 1200, 980, 0));
    NavFeature well = Feature(FeatureKind::eWellExpress, 500, 1220, 524, 1244);
    well.mB = 1;
    well.mA = 1;
    well.mCamera = 6;
    well.mDestCamera = 6;
    well.mSwitch = 100;
    well.mC = 470;
    well.mD = 1237;
    well.mOnA = 0;
    well.mOnDestCamera = 4;
    in.mFeatures.push_back(well);
    NavFeature out = Feature(FeatureKind::eWellLocal, 925, 960, 949, 984);
    out.mCamera = 4;
    in.mFeatures.push_back(out);
    NavGraph g;
    g.Build(in);
    const s32 at = g.NodeAt(512, 1240, false);
    const s32 landing = g.NodeAt(987, 980, false);
    ASSERT_GE(at, 0);
    ASSERT_GE(landing, 0);
    const NavEdge* on = FindEdge(g, at, landing, EdgeKind::eWell);
    ASSERT_NE(on, nullptr);
    s32 sw = 0;
    s32 value = 0;
    EXPECT_TRUE(g.Condition(*on, &sw, &value));
    EXPECT_EQ(sw, 100);
    EXPECT_EQ(value, 1);
    // (off, it shoots him out at its own exit point, two cells from where he stood)
    const NavEdge* off = FindEdge(g, at, g.NodeAt(462, 1240, false), EdgeKind::eWell);
    ASSERT_NE(off, nullptr);
    EXPECT_TRUE(g.Condition(*off, &sw, &value));
    EXPECT_EQ(value, 0);
    // And it is no way out of the path
    EXPECT_TRUE(g.Exits().empty());
}

TEST(BotNecrum, ABirdPortalOfAbesOwnIsAWayOnceItsSwitchIsOn)
{
    // A gap too wide to jump, a portal at its near side made by switch 198, and the portal's exit at the far side
    NavInput in;
    in.mLines.push_back(Line(275, 460, 525, 460, 0));
    in.mLines.push_back(Line(650, 460, 1150, 460, 0));
    NavFeature portal = Feature(FeatureKind::ePortal, 500, 440, 524, 464);
    portal.mDir = 0;
    portal.mSwitch = 198;
    portal.mCamera = 11;
    portal.mDestCamera = 11;
    in.mFeatures.push_back(portal);
    NavFeature out = Feature(FeatureKind::ePortalExit, 650, 440, 674, 464);
    out.mCamera = 11;
    in.mFeatures.push_back(out);
    NavGraph g;
    g.Build(in);
    const s32 far = g.NodeAt(662, 460, false);
    ASSERT_GE(far, 0);
    // From two to five cells before it (it is chanted open from where its doves are not scared off), not nearer
    const NavEdge* through = FindEdge(g, g.NodeAt(412, 460, false), far, EdgeKind::ePortal);
    ASSERT_NE(through, nullptr);
    EXPECT_NE(FindEdge(g, g.NodeAt(462, 460, false), far, EdgeKind::ePortal), nullptr);
    EXPECT_EQ(FindEdge(g, g.NodeAt(487, 460, false), far, EdgeKind::ePortal), nullptr);
    s32 sw = 0;
    s32 value = 0;
    EXPECT_TRUE(g.Condition(*through, &sw, &value));
    EXPECT_EQ(sw, 198);
    EXPECT_EQ(value, 1);
}

TEST(BotNecrum, ATasksDoorIsOpenOnceAllItsHubsAreOn)
{
    World world;
    Entity door;
    door.mKind = EntityKind::eSwitchLogic;
    door.mId = 3004;
    door.mSwitch = World::DoorSwitch(3004);
    door.mInputs[0] = 100;
    door.mInputs[1] = 101;
    world.mEntities.push_back(door);
    EXPECT_EQ(world.Switch(World::DoorSwitch(3004)), 0);
    world.mSwitches[100] = 1;
    EXPECT_EQ(world.Switch(World::DoorSwitch(3004)), 0);
    world.mSwitches[101] = 1;
    EXPECT_EQ(world.Switch(World::DoorSwitch(3004)), 1);
}

TEST(BotNecrum, AWayBackToALevelPlayedIsNoWayOutOfThisOne)
{
    // Path 3 leads back to level 1 and on to path 1; only path 5 leaves for a new level
    std::vector<PathSummary> paths(3);
    paths[0].mPath = 3;
    paths[0].mExits = {{1, 1}, {2, 1}};
    paths[1].mPath = 1;
    paths[1].mExits = {{2, 5}};
    paths[2].mPath = 5;
    paths[2].mExits = {{4, 6}};
    LevelGraph naive;
    naive.Build(2, paths);
    EXPECT_EQ(naive.Distance(3), 0);
    LevelGraph level;
    level.Build(2, paths, {1});
    EXPECT_EQ(level.Distance(5), 0);
    EXPECT_EQ(level.Distance(1), 1);
    EXPECT_EQ(level.Distance(3), 2);
}

TEST(BotNecrum, AFootIsNotPutDownOnASlurg)
{
    World world;
    SlurgAt slurg;
    slurg.mX = 537;
    slurg.mY = 460;
    slurg.mX1 = 527;
    slurg.mX2 = 558;
    slurg.mDir = -1;
    world.mSlurgs.push_back(slurg);
    // A hop that comes down at 587 in 24 frames is clear of it (it crawls away to the left); one at 512 is
    // not (it will be there by then), nor one at 562 (its box reaches 558 and the foot comes down at 553)
    EXPECT_FALSE(world.SlurgWhereHeLands(587, 460, false, false, 24));
    EXPECT_TRUE(world.SlurgWhereHeLands(512, 460, false, false, 24));
    EXPECT_TRUE(world.SlurgWhereHeLands(562, 460, false, false, 24));
    // Its own cell and the ones a step's reach either side; not two cells off, not another floor
    EXPECT_TRUE(world.SlurgBy(537, 460, false));
    EXPECT_TRUE(world.SlurgBy(512, 460, false));
    EXPECT_TRUE(world.SlurgBy(562, 460, false));
    EXPECT_FALSE(world.SlurgBy(587, 460, false));
    EXPECT_FALSE(world.SlurgBy(487, 460, false));
    EXPECT_FALSE(world.SlurgBy(537, 360, false));
    EXPECT_FALSE(world.SlurgBy(537, 460, true));
}

TEST(BotNecrum, TheLeverNearestTheDoorIsPulledLastWhereLeversWakeFleeches)
{
    // A room: a door on switch 46 that levers 43 (far) and 44 (by the door) open between them, and a Fleech
    // that 44 wakes
    NavInput in;
    in.mLines.push_back(Line(800, 460, 1075, 460, 0));
    NavFeature door = Feature(FeatureKind::eDoor, 1025, 440, 1049, 464);
    door.mSwitch = 46;
    door.mStartClosed = true;
    in.mFeatures.push_back(door);
    NavGraph g;
    g.Build(in);
    World world;
    Entity far;
    far.mKind = EntityKind::eLever;
    far.mId = 1;
    far.mX1 = 825;
    far.mX2 = 902;
    far.mY1 = 440;
    far.mY2 = 464;
    far.mSwitch = 43;
    Entity near = far;
    near.mId = 2;
    near.mX1 = 925;
    near.mX2 = 1002;
    near.mSwitch = 44;
    Entity logic;
    logic.mKind = EntityKind::eSwitchLogic;
    logic.mId = 3;
    logic.mSwitch = 46;
    logic.mInputs[0] = 43;
    logic.mInputs[1] = 44;
    Entity fleech;
    fleech.mKind = EntityKind::eFleech;
    fleech.mId = 4;
    fleech.mSwitch = 44;
    world.mEntities = {far, near, logic, fleech};
    EXPECT_TRUE(Domain::PulledFirst(g, world, far).empty());
    EXPECT_EQ(Domain::PulledFirst(g, world, near), std::vector<s32>{43});
    // With no Fleech to wake the order does not matter
    world.mEntities.pop_back();
    EXPECT_TRUE(Domain::PulledFirst(g, world, near).empty());
}

// ---- What the Mudanchee vault took ----

TEST(BotVault, AbeKeepsOffAFloorALooseScrabCanGetAtHimOn)
{
    // A floor with a gate in the middle (shut until switch 228 is on) and a Scrab to the left of it
    NavInput in;
    in.mLines.push_back(Line(2600, 1240, 3275, 1240, 0));
    NavFeature gate = Feature(FeatureKind::eBarrier, 2926, 1220, 2950, 1244);
    gate.mSolid = true;
    gate.mStartClosed = true;
    gate.mSwitch = 228;
    in.mFeatures.push_back(gate);
    NavGraph g;
    g.Build(in);
    World world;
    Entity scrab;
    scrab.mKind = EntityKind::eScrab;
    scrab.mId = 1;
    scrab.mX1 = 2725;
    scrab.mX2 = 2749;
    scrab.mY1 = 1220;
    scrab.mY2 = 1244;
    world.mEntities.push_back(scrab);
    // On its side of the gate, not on the other, and not on another floor
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 2837, 1240, false));
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 3087, 1240, false));
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 2837, 1140, false));
    // With the gate open it gets everywhere
    world.mSwitches[228] = 1;
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 3087, 1240, false));
    // Invisible (or with the power to be) he is not minded; a dead Scrab minds nothing
    world.mInvisCharge = true;
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 3087, 1240, false));
    world.mInvisCharge = false;
    world.mScrabsKilled.insert(1);
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 2837, 1240, false));
}

// A ledge over a floor a Scrab is loose on: the way down is there for the planner, for an Abe who is unseen
static bool NeedsUnseen(const Goap::Action& a)
{
    bool needs = false;
    for (const Goap::Fact& f : a.mPre)
    {
        needs = needs || (f.mKey == Key::kInvisNow && f.mValue == 1);
    }
    return needs;
}

TEST(BotVault, AFloorALooseScrabIsOnIsCrossedOntoOnlyUnseen)
{
    NavInput in;
    in.mLines.push_back(Line(2600, 1140, 2700, 1140, 0));
    in.mLines.push_back(Line(2600, 1240, 3275, 1240, 0));
    NavGraph g;
    g.Build(in);
    World world;
    Entity scrab;
    scrab.mKind = EntityKind::eScrab;
    scrab.mId = 1;
    scrab.mX1 = 2925;
    scrab.mX2 = 2949;
    scrab.mY1 = 1220;
    scrab.mY2 = 1244;
    world.mEntities.push_back(scrab);
    const s32 ledge = g.NodeAt(2662, 1140, false, 6);
    ASSERT_GE(ledge, 0);
    s32 downs = 0;
    for (const Goap::Action& a : Domain::Build(g, world, ledge, nullptr).mActions)
    {
        if (a.mName.rfind("Cross fall", 0) == 0)
        {
            downs++;
            EXPECT_TRUE(NeedsUnseen(a)) << a.mName;
        }
    }
    EXPECT_GT(downs, 0);
    // The power not yet used does for finding a route, and so does its ring on the way; for the plan it is still
    // a floor to be unseen on, and its start says he can be
    world.mInvisComing = true;
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 2837, 1240, false));
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 2837, 1240, false, false));
    const Domain coming = Domain::Build(g, world, ledge, nullptr);
    EXPECT_EQ(coming.mStart.Get(Key::kInvisNow), 1);
    // Invisible, it is a floor like any other
    world.mInvisComing = false;
    world.mInvisible = true;
    for (const Goap::Action& a : Domain::Build(g, world, ledge, nullptr).mActions)
    {
        EXPECT_FALSE(NeedsUnseen(a)) << a.mName;
    }
}

// A Fleech that hangs over a lock has to be dead before the lock is slapped; one that hangs over a floor that is
// over the lock does not
TEST(BotVault, AFleechOverAnotherFloorDoesNotGuardALock)
{
    for (s32 pass = 0; pass < 2; pass++)
    {
        NavInput in;
        in.mLines.push_back(Line(700, 460, 1084, 460, 0));
        if (pass == 1)
        {
            in.mLines.push_back(Line(700, 380, 1024, 380, 0));
        }
        NavGraph g;
        g.Build(in);
        World world;
        Entity lock;
        lock.mKind = EntityKind::eSlapLock;
        lock.mId = 1;
        lock.mX1 = 876;
        lock.mX2 = 900;
        lock.mY1 = 440;
        lock.mY2 = 464;
        lock.mSwitch = 177;
        Entity fleech;
        fleech.mKind = EntityKind::eFleech;
        fleech.mId = 2;
        fleech.mX1 = 850;
        fleech.mX2 = 874;
        fleech.mY1 = 300;
        fleech.mY2 = 364;
        world.mEntities = {lock, fleech};
        s32 uses = 0;
        for (const Goap::Action& a : Domain::Build(g, world, g.NodeAt(762, 460, false, 6), nullptr).mActions)
        {
            // (the way past the guard unseen is another action, with its own test)
            if (a.mName.rfind("Use SlapLock#1", 0) != 0 || a.mName.find("unseen") != std::string::npos)
            {
                continue;
            }
            uses++;
            bool guarded = false;
            for (const Goap::Fact& f : a.mPre)
            {
                guarded = guarded || (f.mKey == Key::Mud(2) && f.mValue == 2);
            }
            EXPECT_EQ(guarded, pass == 0) << a.mName;
        }
        EXPECT_GT(uses, 0);
    }
}

TEST(BotVault, AScrabShutInBehindAGateIsWhereItWasLeft)
{
    NavInput in;
    in.mLines.push_back(Line(438, 1240, 2214, 1240, 0));
    NavFeature gate = Feature(FeatureKind::eBarrier, 600, 1216, 624, 1240);
    gate.mSolid = true;
    gate.mStartClosed = true;
    gate.mSwitch = 230;
    in.mFeatures.push_back(gate);
    NavGraph g;
    g.Build(in);
    World world;
    Entity scrab;
    scrab.mKind = EntityKind::eScrab;
    scrab.mId = 7;
    scrab.mX1 = 1249;
    scrab.mX2 = 1273;
    scrab.mY1 = 1220;
    scrab.mY2 = 1244;
    world.mEntities.push_back(scrab);
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 1012, 1240, false));
    // Walked in behind the gate at 509 and let go there
    world.mScrabParked[7] = 509;
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 1012, 1240, false));
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 562, 1240, false));
}

TEST(BotVault, APortalIsHoppedIntoFromTwoCellsOffWhereverItsMiddleIs)
{
    // The portal's middle is 438, a pixel short of two cells from 487 (the Mudanchee vault's path 7)
    NavInput in;
    in.mLines.push_back(Line(378, 360, 675, 360, 0));
    in.mLines.push_back(Line(1575, 460, 1776, 460, 0));
    NavFeature portal = Feature(FeatureKind::ePortal, 426, 340, 450, 364);
    portal.mDir = 1;
    portal.mSwitch = 1;
    portal.mCamera = 2;
    portal.mDestCamera = 4;
    in.mFeatures.push_back(portal);
    NavFeature out = Feature(FeatureKind::ePortalExit, 1601, 440, 1626, 460);
    out.mCamera = 4;
    in.mFeatures.push_back(out);
    NavGraph g;
    g.Build(in);
    const s32 far = g.NodeAt(1612, 460, false);
    ASSERT_GE(far, 0);
    EXPECT_NE(FindEdge(g, g.NodeAt(487, 360, false), far, EdgeKind::ePortal), nullptr);
    EXPECT_EQ(FindEdge(g, g.NodeAt(462, 360, false), far, EdgeKind::ePortal), nullptr);
}

TEST(BotVault, ARunningJumpThroughAHoistCatchesTheLedgeOverIt)
{
    // A long floor, a ledge 100 over its right end with a hoist up to it that is taken facing right
    NavInput in;
    in.mLines.push_back(Line(1000, 460, 1400, 460, 0));
    in.mLines.push_back(Line(1325, 360, 1550, 360, 0));
    NavFeature hoist = Feature(FeatureKind::eHoist, 1326, 361, 1350, 463);
    hoist.mDir = 1;
    in.mFeatures.push_back(hoist);
    NavGraph g;
    g.Build(in);
    const s32 top = g.NodeAt(1337, 360, false);
    ASSERT_GE(top, 0);
    // From where a run is under way, well back along the floor
    const NavEdge* jump = FindEdge(g, g.NodeAt(1062, 460, false), top, EdgeKind::eRunJump);
    ASSERT_NE(jump, nullptr);
    // (dear, so that it is only taken with something after him)
    EXPECT_GE(jump->mCost, 400);
    // Not facing the other way
    EXPECT_EQ(FindEdge(g, g.NodeAt(1387, 460, false), top, EdgeKind::eRunJump), nullptr);
}

TEST(BotVault, WellsOfAChainThatGoByTheSameSwitchGoTheSameWay)
{
    // As in the Mudanchee vault's path 3: a well that with switch 145 on throws to a point over a second well,
    // which by the same switch throws up to a ledge (and with it off only along the floor)
    NavInput in;
    in.mLines.push_back(Line(419, 720, 705, 720, 0));
    in.mLines.push_back(Line(576, 460, 1830, 460, 0));
    in.mLines.push_back(Line(500, 80, 648, 80, 0));
    in.mLines.push_back(Line(550, 640, 623, 640, 0));
    NavFeature first = Feature(FeatureKind::eWellLocal, 500, 700, 525, 724);
    first.mSwitch = 145;
    first.mC = 611;
    first.mD = 636;
    first.mOnC = 587;
    first.mOnD = 407;
    in.mFeatures.push_back(first);
    NavFeature second = Feature(FeatureKind::eWellLocal, 577, 440, 601, 464);
    second.mSwitch = 145;
    second.mC = 634;
    second.mD = 455;
    second.mOnC = 561;
    second.mOnD = 92;
    in.mFeatures.push_back(second);
    NavGraph g;
    g.Build(in);
    const s32 at = g.NodeAt(512, 720, false);
    const s32 ledge = g.NodeAt(562, 80, false);
    ASSERT_GE(at, 0);
    ASSERT_GE(ledge, 0);
    const NavEdge* on = FindEdge(g, at, ledge, EdgeKind::eWell);
    ASSERT_NE(on, nullptr);
    s32 sw = 0;
    s32 value = 0;
    EXPECT_TRUE(g.Condition(*on, &sw, &value));
    EXPECT_EQ(sw, 145);
    EXPECT_EQ(value, 1);
}

// ---- What the Mudanchee vault's path 5 took ----

// Its first screen: a floor over another, a hoist at each end of the upper one, a Scrab up there and one shut in
// behind a gate below with the lock, and the gate's lever on the upper floor
static NavInput ScrabPenScreen()
{
    NavInput in;
    in.mLines.push_back(Line(18, 200, 1083, 200, 0));
    in.mLines.push_back(Line(100, 100, 948, 100, 0));
    NavFeature right = Feature(FeatureKind::eHoist, 925, 100, 950, 200);
    right.mDir = 0;
    in.mFeatures.push_back(right);
    NavFeature left = Feature(FeatureKind::eHoist, 100, 100, 124, 202);
    left.mDir = 1;
    in.mFeatures.push_back(left);
    NavFeature gate = Feature(FeatureKind::eBarrier, 200, 180, 224, 204);
    gate.mSolid = true;
    gate.mStartClosed = true;
    gate.mSwitch = 207;
    in.mFeatures.push_back(gate);
    return in;
}

static World ScrabPenWorld()
{
    World world;
    Entity up;
    up.mKind = EntityKind::eScrab;
    up.mId = 1;
    up.mX1 = 501;
    up.mX2 = 525;
    up.mY1 = 80;
    up.mY2 = 104;
    world.mEntities.push_back(up);
    Entity pen = up;
    pen.mId = 2;
    pen.mX1 = 100;
    pen.mX2 = 124;
    pen.mY1 = 180;
    pen.mY2 = 204;
    world.mEntities.push_back(pen);
    Entity lever;
    lever.mKind = EntityKind::eLever;
    lever.mId = 3;
    lever.mX1 = 300;
    lever.mX2 = 375;
    lever.mY1 = 80;
    lever.mY2 = 100;
    lever.mSwitch = 207;
    lever.mOp = 2;
    world.mEntities.push_back(lever);
    Entity lock;
    lock.mKind = EntityKind::eSlapLock;
    lock.mId = 4;
    lock.mX1 = 151;
    lock.mX2 = 175;
    lock.mY1 = 179;
    lock.mY2 = 203;
    lock.mSwitch = 219;
    world.mEntities.push_back(lock);
    return world;
}

TEST(BotVault, AScrabChasedOffTheEndOfItsFloorComesDownOnTheOneBelow)
{
    NavInput in = ScrabPenScreen();
    NavGraph g;
    g.Build(in);
    // From the hoist at the right end it lands a cell on, on the floor below
    EXPECT_EQ(Domain::LureLanding(g, g.NodeAt(937, 100, false)), g.NodeAt(962, 200, false));
    // Not from the middle of the floor
    EXPECT_EQ(Domain::LureLanding(g, g.NodeAt(512, 100, false)), -1);
    // And with floor at its height within three cells it jumps the gap instead
    in.mLines.push_back(Line(990, 100, 1070, 100, 0));
    NavGraph gap;
    gap.Build(in);
    EXPECT_EQ(Domain::LureLanding(gap, gap.NodeAt(937, 100, false)), -1);
}

TEST(BotVault, AScrabIsOnTheFloorItIsOnNotTheOneTheMapPutItOn)
{
    NavGraph g;
    g.Build(ScrabPenScreen());
    World world = ScrabPenWorld();
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 512, 100, false));
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 512, 200, false));
    // Lured off its floor, it is on the one below
    ScrabAt live;
    live.mId = 1;
    live.mX = 962;
    live.mY = 200;
    world.mScrabs.push_back(live);
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 512, 100, false));
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 512, 200, false));
    // And not past the gate, which is shut: with the one behind it dead, nothing gets at him there
    world.mScrabsKilled.insert(2);
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 137, 200, false));
    EXPECT_TRUE(Domain::GateBetween(g, world, 137, 512, 200, false));
    world.mSwitches[207] = 1;
    EXPECT_FALSE(Domain::GateBetween(g, world, 137, 512, 200, false));
}

TEST(BotVault, ScrabsAreLuredOffALedgeAndShutInBehindAGateToGetAtALock)
{
    NavGraph g;
    g.Build(ScrabPenScreen());
    const World world = ScrabPenWorld();
    const s32 abe = g.NodeAt(512, 200, false);
    ASSERT_GE(abe, 0);
    const Domain d = Domain::Build(g, world, abe, [](s32)
                                   { return true; });
    const Goap::Plan plan = Goap::Planner::Find(d.mStart, {{Key::Switch(219), 1}}, d.mActions);
    ASSERT_TRUE(plan.mFound);
    std::vector<std::string> names;
    for (s32 step : plan.mSteps)
    {
        if (d.mSteps[d.mActions[step].mTag].mKind != StepKind::eCross)
        {
            names.push_back(d.mActions[step].mName);
        }
    }
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "Lure the Scrabs off the ledge at 937,100");
    EXPECT_EQ(names[1], "Shut the Scrabs in right of the gate at 212,204 with Lever#3");
    EXPECT_EQ(names[2], "Use SlapLock#4 sw219 on");
    EXPECT_EQ(d.mSteps[d.mActions[plan.mSteps[0]].mTag].mKind, StepKind::eScrabLure);
}

TEST(BotVault, AHopDownOntoAWellGoesWhereTheWellLeads)
{
    // As measured in the Mudanchee vault's path 5: off the platform at 312,620 he comes down in the well at 212,
    // which throws him out at 612
    NavInput in;
    in.mLines.push_back(Line(301, 620, 475, 620, 0));
    in.mLines.push_back(Line(0, 720, 700, 720, 0));
    NavFeature well = Feature(FeatureKind::eWellLocal, 201, 700, 225, 724);
    well.mC = 612;
    well.mD = 716;
    in.mFeatures.push_back(well);
    NavGraph g;
    g.Build(in);
    const s32 from = g.NodeAt(312, 620, false);
    const s32 to = g.NodeAt(612, 720, false);
    ASSERT_GE(from, 0);
    ASSERT_GE(to, 0);
    bool found = false;
    for (const NavEdge& e : g.Edges())
    {
        if (e.mFrom == from && e.mTo == to && e.mKind == EdgeKind::eHop)
        {
            found = true;
            // (and it says which well, for what walks the floor the well is in)
            EXPECT_GE(e.mWell, 0);
        }
    }
    EXPECT_TRUE(found);
}

// A lock in a room with two Fleeches, as in the Mudanchee vault's path 2: a floor that leads west from the room, a
// floor over that with a hoist up to it at each end, and a well by the lock that throws him out far away
static NavInput FleechRoom(bool eastHoist)
{
    NavInput in;
    in.mLines.push_back(Line(430, 700, 1075, 700, 0));
    in.mLines.push_back(Line(526, 620, 900, 620, 0));
    in.mLines.push_back(Line(1200, 300, 1500, 300, 0));
    NavFeature west = Feature(FeatureKind::eHoist, 526, 620, 550, 700);
    west.mDir = 1;
    in.mFeatures.push_back(west);
    if (eastHoist)
    {
        NavFeature east = Feature(FeatureKind::eHoist, 876, 620, 900, 700);
        east.mDir = 0;
        in.mFeatures.push_back(east);
    }
    NavFeature well = Feature(FeatureKind::eWellLocal, 951, 680, 975, 704);
    well.mC = 1312;
    well.mD = 296;
    in.mFeatures.push_back(well);
    return in;
}

static World FleechRoomWorld()
{
    World world;
    Entity fleech;
    fleech.mKind = EntityKind::eFleech;
    fleech.mId = 1;
    fleech.mX1 = 901;
    fleech.mX2 = 925;
    fleech.mY1 = 679;
    fleech.mY2 = 703;
    world.mEntities.push_back(fleech);
    fleech.mId = 2;
    fleech.mX1 = 1026;
    fleech.mX2 = 1050;
    world.mEntities.push_back(fleech);
    Entity lock;
    lock.mKind = EntityKind::eSlapLock;
    lock.mId = 3;
    lock.mX1 = 1002;
    lock.mX2 = 1026;
    lock.mY1 = 680;
    lock.mY2 = 704;
    lock.mSwitch = 124;
    world.mEntities.push_back(lock);
    return world;
}

TEST(BotVault, ALockFleechesGuardIsAlsoSlappedUnseenWhileThePowerLasts)
{
    // The Mudanchee trial's last lock: its Fleeches take no notice of him unseen. So besides the way that wants
    // them dead there is one that wants him unseen, and it is only offered while there is time left for it.
    NavGraph g;
    g.Build(FleechRoom(true));
    World world = FleechRoomWorld();
    const s32 from = g.NodeAt(587, 700, false);
    const auto unseen = [&](const Domain& d) -> const Goap::Action*
    {
        for (const Goap::Action& a : d.mActions)
        {
            if (a.mName.size() > 10 && a.mName.compare(a.mName.size() - 10, 10, " on unseen") == 0)
            {
                return &a;
            }
        }
        return nullptr;
    };
    {
        const Domain d = Domain::Build(g, world, from, nullptr);
        const Goap::Action* a = unseen(d);
        ASSERT_NE(a, nullptr);
        EXPECT_NE(std::find(a->mPre.begin(), a->mPre.end(), Goap::Fact{Key::kInvisNow, 1}), a->mPre.end());
        EXPECT_EQ(std::find(a->mPre.begin(), a->mPre.end(), Goap::Fact{Key::Mud(1), 2}), a->mPre.end());
        EXPECT_EQ(std::find(a->mPre.begin(), a->mPre.end(), Goap::Fact{Key::Mud(2), 2}), a->mPre.end());
    }
    // Unseen already and most of it gone: not enough for the way there, the slap and the way off
    world.mInvisible = true;
    world.mInvisLeft = 100;
    {
        const Domain d = Domain::Build(g, world, from, nullptr);
        EXPECT_EQ(unseen(d), nullptr);
    }
    world.mInvisLeft = 200;
    {
        const Domain d = Domain::Build(g, world, from, nullptr);
        EXPECT_NE(unseen(d), nullptr);
    }
}

TEST(BotVault, AFleechCrawlsClimbsAHoistAndFallsButTakesNoWell)
{
    NavGraph g;
    g.Build(FleechRoom(true));
    const World world = FleechRoomWorld();
    s32 walked = 0;
    for (const NavEdge& e : g.Edges())
    {
        const s32 frames = Domain::FleechFrames(g, world, e);
        switch (e.mKind)
        {
            case EdgeKind::eWalk:
                // (a cell every seven frames)
                EXPECT_EQ(frames, 7);
                walked++;
                break;
            case EdgeKind::eHoistUp:
                EXPECT_EQ(frames, 40);
                break;
            case EdgeKind::eFall:
            case EdgeKind::eHoistDown:
                EXPECT_EQ(frames, 21);
                break;
            default:
                EXPECT_EQ(frames, -1) << EdgeKindName(e.mKind);
                break;
        }
    }
    EXPECT_GT(walked, 0);
    // From the far end of the floor to the lock: as long as the floor is, and never to where the well leads
    const s32 lock = g.NodeAt(987, 700, false);
    const std::vector<s32> times = Domain::FleechTimes(g, world, {lock});
    EXPECT_EQ(times[g.NodeAt(537, 700, false)], 18 * 7);
    EXPECT_EQ(times[g.NodeAt(1312, 300, false)], 100000);
    // (and the other way round: from the lock to there)
    const std::vector<s32> from = Domain::FleechTimes(g, world, {lock}, -1, true);
    EXPECT_EQ(from[g.NodeAt(537, 700, false)], 18 * 7);
    EXPECT_EQ(from[g.NodeAt(1312, 300, false)], 100000);
}

TEST(BotVault, FleechesAreLedToTheFootOfAHoistAndTheLockGotAtTheOtherWay)
{
    NavGraph g;
    g.Build(FleechRoom(true));
    const World world = FleechRoomWorld();
    const NavGraph::EdgeCost any = [](const NavEdge& e) -> s32
    {
        return std::max(1, e.mCost);
    };
    const s32 lock = g.NodeAt(987, 700, false);
    const std::vector<s32> homes{g.NodeAt(912, 700, false), g.NodeAt(1037, 700, false)};
    const Domain::FleechLure lure = Domain::LureFleeches(g, world, homes, lock, any, any);
    ASSERT_TRUE(lure.mFound);
    // To the west hoist along the floor, back over the top, and whether to go on is settled at the top's east end
    EXPECT_EQ(lure.mBait, g.NodeAt(537, 700, false));
    EXPECT_EQ(lure.mCommit, g.NodeAt(887, 620, false));
    ASSERT_FALSE(lure.mBack.empty());
    EXPECT_EQ(g.Edges()[lure.mBack.front()].mKind, EdgeKind::eHoistUp);
    EXPECT_EQ(g.Edges()[lure.mBack.back()].mTo, lock);
    // (they lose three frames a cell on him)
    EXPECT_GE(lure.mLead, 40);
    // He shows himself along the floor, and leaves by the well
    ASSERT_FALSE(lure.mOut.empty());
    EXPECT_EQ(g.Nodes()[g.Edges()[lure.mOut.back()].mTo].mY, 700);
    ASSERT_FALSE(lure.mAway.empty());
    EXPECT_EQ(g.Edges()[lure.mAway.back()].mTo, g.NodeAt(1312, 300, false));
    // From where they would be at the hoist before him there is no such way
    const Domain::FleechLure late = Domain::LureFleeches(g, world, {g.NodeAt(587, 700, false)}, lock, any, any, -1, g.NodeAt(462, 700, false));
    EXPECT_FALSE(late.mFound);
}

TEST(BotVault, ALockFleechesGuardIsPlannedByLeadingThemOff)
{
    NavGraph g;
    g.Build(FleechRoom(true));
    const World world = FleechRoomWorld();
    const s32 abe = g.NodeAt(462, 700, false);
    ASSERT_GE(abe, 0);
    const Domain d = Domain::Build(g, world, abe, [](s32)
                                   { return true; });
    const Goap::Plan plan = Goap::Planner::Find(d.mStart, {{Key::Switch(124), 1}}, d.mActions);
    ASSERT_TRUE(plan.mFound);
    std::vector<std::string> names;
    for (s32 step : plan.mSteps)
    {
        if (d.mSteps[d.mActions[step].mTag].mKind != StepKind::eCross)
        {
            names.push_back(d.mActions[step].mName);
        }
    }
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], "Lead the Fleeches off and use SlapLock#3");
    EXPECT_EQ(d.mSteps[d.mActions[plan.mSteps.back()].mTag].mKind, StepKind::eFleechLure);
}

TEST(BotVault, FleechesShutInWithALockAreLetOutFirstToBeLedOff)
{
    // No hoist out of the room, and a gate across the floor that a lever at the far end opens
    NavInput in = FleechRoom(false);
    NavFeature gate = Feature(FeatureKind::eBarrier, 838, 680, 862, 704);
    gate.mSolid = true;
    gate.mStartClosed = true;
    gate.mSwitch = 121;
    in.mFeatures.push_back(gate);
    NavGraph g;
    g.Build(in);
    World world = FleechRoomWorld();
    Entity lever;
    lever.mKind = EntityKind::eLever;
    lever.mId = 4;
    lever.mX1 = 437;
    lever.mX2 = 512;
    lever.mY1 = 680;
    lever.mY2 = 700;
    lever.mSwitch = 121;
    lever.mOp = 2;
    world.mEntities.push_back(lever);
    const s32 abe = g.NodeAt(462, 700, false);
    ASSERT_GE(abe, 0);
    const Domain d = Domain::Build(g, world, abe, [](s32)
                                   { return true; });
    const Goap::Plan plan = Goap::Planner::Find(d.mStart, {{Key::Switch(124), 1}}, d.mActions);
    ASSERT_TRUE(plan.mFound);
    std::vector<std::string> names;
    for (s32 step : plan.mSteps)
    {
        if (d.mSteps[d.mActions[step].mTag].mKind != StepKind::eCross)
        {
            names.push_back(d.mActions[step].mName);
        }
    }
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "Use Lever#4 sw121 on");
    EXPECT_EQ(names[1], "Lead the Fleeches off and use SlapLock#3");
}

// The Fleech room with a gate across its floor, a Scrab on the near side of it, the Fleeches asleep on the far side
// by a lock that gives the power to go unseen, and the gate's lever up on the ledge
static World ScrabHuntWorld()
{
    World world = FleechRoomWorld();
    for (Entity& e : world.mEntities)
    {
        if (e.mKind == EntityKind::eFleech)
        {
            e.mInputs[0] = 1;
        }
        if (e.mKind == EntityKind::eSlapLock)
        {
            e.mInputs[0] = 120;
        }
    }
    Entity scrab;
    scrab.mKind = EntityKind::eScrab;
    scrab.mId = 5;
    scrab.mX1 = 588;
    scrab.mX2 = 612;
    scrab.mY1 = 676;
    scrab.mY2 = 700;
    world.mEntities.push_back(scrab);
    Entity lever;
    lever.mKind = EntityKind::eLever;
    lever.mId = 4;
    lever.mX1 = 637;
    lever.mX2 = 712;
    lever.mY1 = 600;
    lever.mY2 = 620;
    lever.mSwitch = 121;
    lever.mOp = 2;
    world.mEntities.push_back(lever);
    return world;
}

static NavInput ScrabHuntRoom()
{
    NavInput in = FleechRoom(true);
    NavFeature gate = Feature(FeatureKind::eBarrier, 838, 680, 862, 704);
    gate.mSolid = true;
    gate.mStartClosed = true;
    gate.mSwitch = 121;
    in.mFeatures.push_back(gate);
    return in;
}

TEST(BotVault, AGateBetweenAScrabAndAPlaceIsWhatKeepsItOffOrCouldIfShut)
{
    NavGraph g;
    g.Build(ScrabHuntRoom());
    World world = ScrabHuntWorld();
    std::vector<Goap::Fact> keep;
    std::vector<Goap::Fact> shut;
    bool canShut = false;
    // Shut: it does not get to the far side, and the gate staying shut is why
    EXPECT_FALSE(Domain::ScrabReaches(g, world, 950, 700, false, false, &keep, &shut, &canShut));
    ASSERT_EQ(keep.size(), 1u);
    EXPECT_TRUE(keep[0] == (Goap::Fact{Key::Switch(121), 0}));
    EXPECT_TRUE(shut.empty());
    // Its own side it has
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 700, 700, false, false));
    // Open: it does, and shutting the gate would stop it
    world.mSwitches[121] = 1;
    keep.clear();
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 950, 700, false, false, &keep, &shut, &canShut));
    EXPECT_TRUE(keep.empty());
    ASSERT_EQ(shut.size(), 1u);
    EXPECT_TRUE(shut[0] == (Goap::Fact{Key::Switch(121), 0}));
    EXPECT_TRUE(canShut);
    // No gate between it and its own side: nothing to shut
    EXPECT_TRUE(Domain::ScrabReaches(g, world, 700, 700, false, false, &keep, &shut, &canShut));
    EXPECT_FALSE(canShut);
}

TEST(BotVault, FleechesOnAScrabsFloorAreKilledWithItBeforeTheLockTheyLieBy)
{
    NavGraph g;
    g.Build(ScrabHuntRoom());
    const World world = ScrabHuntWorld();
    const s32 abe = g.NodeAt(662, 620, false);
    ASSERT_GE(abe, 0);
    const Domain d = Domain::Build(g, world, abe, [](s32)
                                   { return true; });
    // The hunt: with the gate open and no power in hand (the chant would spend it), both Fleeches dead by it
    const Goap::Action* hunt = nullptr;
    const Goap::Action* lock = nullptr;
    for (const Goap::Action& a : d.mActions)
    {
        const DomainStep& step = d.mSteps[a.mTag];
        hunt = step.mKind == StepKind::eScrabHunt && step.mEntity == 5 && !hunt ? &a : hunt;
        lock = step.mKind == StepKind::eUse && step.mEntity == 3 && !lock ? &a : lock;
    }
    ASSERT_NE(hunt, nullptr);
    const auto has = [](const std::vector<Goap::Fact>& facts, const Goap::Fact& f)
    {
        return std::find(facts.begin(), facts.end(), f) != facts.end();
    };
    EXPECT_TRUE(has(hunt->mPre, {Key::Switch(121), 1}));
    EXPECT_TRUE(has(hunt->mPre, {Key::kInvisNow, 0}));
    EXPECT_TRUE(has(hunt->mEffects, {Key::Mud(1), 2}));
    EXPECT_TRUE(has(hunt->mEffects, {Key::Mud(2), 2}));
    // The lock: the one asleep beside it is its guard now that there is a way to kill it
    ASSERT_NE(lock, nullptr);
    EXPECT_TRUE(has(lock->mPre, {Key::Mud(2), 2}));
    // And the lever that opens the Scrab's gate for the hunt is not held back for the power
    for (const Goap::Action& a : d.mActions)
    {
        const DomainStep& step = d.mSteps[a.mTag];
        if (step.mKind == StepKind::eUse && step.mEntity == 4)
        {
            EXPECT_FALSE(has(a.mPre, {Key::kInvisNow, 1})) << a.mName;
        }
    }
    // Without the Scrab the Fleech asleep on the floor is no guard: the lock is slapped among them
    World none = world;
    none.mEntities.erase(std::remove_if(none.mEntities.begin(), none.mEntities.end(), [](const Entity& e)
                                        { return e.mKind == EntityKind::eScrab; }),
                         none.mEntities.end());
    const Domain plain = Domain::Build(g, none, abe, [](s32)
                                       { return true; });
    for (const Goap::Action& a : plain.mActions)
    {
        const DomainStep& step = plain.mSteps[a.mTag];
        EXPECT_NE(step.mKind, StepKind::eScrabHunt);
        if (step.mKind == StepKind::eUse && step.mEntity == 3)
        {
            EXPECT_FALSE(has(a.mPre, {Key::Mud(2), 2}));
        }
    }
}

TEST(BotVault, ASwitchThatFollowsAnotherIsSetAndUnsetWithIt)
{
    // A timer trigger's output: the lever's switch 121 moves 122 with it, both ways, and there is no step for it
    NavGraph g;
    g.Build(ScrabHuntRoom());
    World world = ScrabHuntWorld();
    Entity timer;
    timer.mKind = EntityKind::eSwitchLogic;
    timer.mId = 9;
    timer.mInputs[0] = 121;
    timer.mSwitch = 122;
    timer.mHoldOnly = true;
    world.mEntities.push_back(timer);
    const s32 abe = g.NodeAt(662, 620, false);
    ASSERT_GE(abe, 0);
    const Domain d = Domain::Build(g, world, abe, [](s32)
                                   { return true; });
    s32 on = 0;
    s32 off = 0;
    for (const Goap::Action& a : d.mActions)
    {
        const DomainStep& step = d.mSteps[a.mTag];
        EXPECT_FALSE(step.mKind == StepKind::eLogic && step.mEntity == 9);
        if (step.mKind == StepKind::eUse && step.mEntity == 4)
        {
            const bool setsOn = std::find(a.mEffects.begin(), a.mEffects.end(), Goap::Fact{Key::Switch(121), 1}) != a.mEffects.end();
            const Goap::Fact with{Key::Switch(122), setsOn ? 1 : 0};
            EXPECT_TRUE(std::find(a.mEffects.begin(), a.mEffects.end(), with) != a.mEffects.end()) << a.mName;
            on += setsOn ? 1 : 0;
            off += setsOn ? 0 : 1;
        }
    }
    EXPECT_GE(on, 1);
    EXPECT_GE(off, 1);
}

// The first screen of the sixth Mudanchee vault in small: a Scrab in a pen under a gap in the floor above, Fleeches
// shut in further on behind a gate with its lever outside, and a hoist up to the floor above beyond that
static NavInput FleechLeadRoom()
{
    NavInput in;
    in.mLines.push_back(Line(0, 460, 1456, 460, 0));
    in.mLines.push_back(Line(101, 360, 475, 360, 0));
    in.mLines.push_back(Line(525, 360, 1325, 360, 0));
    in.mLines.push_back(Line(250, 362, 250, 480, 1));
    in.mLines.push_back(Line(700, 360, 700, 480, 1));
    in.mLines.push_back(Line(825, 360, 825, 480, 1));
    NavFeature up = Feature(FeatureKind::eHoist, 1300, 360, 1324, 466);
    up.mDir = 0;
    in.mFeatures.push_back(up);
    NavFeature start = Feature(FeatureKind::eHoist, 100, 360, 124, 466);
    start.mDir = 1;
    in.mFeatures.push_back(start);
    NavFeature gate = Feature(FeatureKind::eBarrier, 975, 440, 999, 464);
    gate.mSolid = true;
    gate.mStartClosed = true;
    gate.mSwitch = 103;
    in.mFeatures.push_back(gate);
    return in;
}

static World FleechLeadWorld()
{
    World world;
    Entity fleech;
    fleech.mKind = EntityKind::eFleech;
    fleech.mY1 = 440;
    fleech.mY2 = 464;
    for (u32 i = 0; i < 3; i++)
    {
        fleech.mId = 1 + i;
        fleech.mX1 = 901 + 24 * static_cast<s32>(i);
        fleech.mX2 = fleech.mX1 + 24;
        world.mEntities.push_back(fleech);
    }
    Entity lever;
    lever.mKind = EntityKind::eLever;
    lever.mId = 4;
    lever.mX1 = 1025;
    lever.mX2 = 1100;
    lever.mY1 = 440;
    lever.mY2 = 460;
    lever.mSwitch = 103;
    lever.mOp = 2;
    world.mEntities.push_back(lever);
    Entity scrab;
    scrab.mKind = EntityKind::eScrab;
    scrab.mId = 5;
    scrab.mX1 = 427;
    scrab.mX2 = 452;
    scrab.mY1 = 442;
    scrab.mY2 = 462;
    world.mEntities.push_back(scrab);
    Entity lock;
    lock.mKind = EntityKind::eSlapLock;
    lock.mId = 6;
    lock.mX1 = 876;
    lock.mX2 = 900;
    lock.mY1 = 439;
    lock.mY2 = 463;
    lock.mSwitch = 106;
    lock.mInputs[0] = 120;
    world.mEntities.push_back(lock);
    return world;
}

TEST(BotVault, FleechesLetOutAreLedOverAGapToDropToAScrab)
{
    NavGraph g;
    g.Build(FleechLeadRoom());
    const World world = FleechLeadWorld();
    const s32 from = Domain::UseNode(g, world.mEntities[3], nullptr);
    ASSERT_GE(from, 0);
    const Domain::FleechLead lead = Domain::LeadFleeches(g, world, from, 103);
    ASSERT_TRUE(lead.mFound);
    EXPECT_EQ(lead.mScrab, 5u);
    EXPECT_EQ(lead.mGroup.size(), 3u);
    // They come up the hoist after him and along the floor above to its end over the pen
    bool hoist = false;
    for (s32 edge : lead.mWay)
    {
        hoist = hoist || g.Edges()[edge].mKind == EdgeKind::eHoistUp;
        EXPECT_GE(Domain::FleechFrames(g, world, g.Edges()[edge], 103), 0);
    }
    EXPECT_TRUE(hoist);
    EXPECT_EQ(g.Nodes()[lead.mDrop].mY, 360);
    EXPECT_GT(g.Nodes()[lead.mDrop].mX, 525);
    // He goes on by a hop they cannot make, and they come down in the pen
    EXPECT_LT(Domain::FleechFrames(g, world, g.Edges()[lead.mOver], 103), 0);
    EXPECT_EQ(g.Nodes()[lead.mLand].mY, 460);
    EXPECT_GT(g.Nodes()[lead.mLand].mX, 250);
    EXPECT_LT(g.Nodes()[lead.mLand].mX, 700);
    // The Scrab is taken over from across the gap, out of its reach and not right over them
    EXPECT_EQ(g.Nodes()[lead.mSpot].mY, 360);
    EXPECT_LT(g.Nodes()[lead.mSpot].mX, 475);
    EXPECT_FALSE(Domain::ScrabReaches(g, world, g.Nodes()[lead.mSpot].mX, 360, false, false));
    // With the gate open already there is nothing to let out
    World open = world;
    open.mSwitches[103] = 1;
    EXPECT_FALSE(Domain::LeadFleeches(g, open, from, 103).mFound);
    // Nor is there anywhere to lead them without the Scrab
    World none = world;
    none.mEntities.erase(std::remove_if(none.mEntities.begin(), none.mEntities.end(), [](const Entity& e)
                                        { return e.mKind == EntityKind::eScrab; }),
                         none.mEntities.end());
    EXPECT_FALSE(Domain::LeadFleeches(g, none, from, 103).mFound);
}

TEST(BotVault, TheLeverThatLetsFleechesOutIsPulledToLeadThemToTheScrab)
{
    NavGraph g;
    g.Build(FleechLeadRoom());
    const World world = FleechLeadWorld();
    const s32 abe = g.NodeAt(137, 460, false);
    ASSERT_GE(abe, 0);
    const Domain d = Domain::Build(g, world, abe, [](s32)
                                   { return true; });
    const auto has = [](const std::vector<Goap::Fact>& facts, const Goap::Fact& f)
    {
        return std::find(facts.begin(), facts.end(), f) != facts.end();
    };
    const Goap::Action* lead = nullptr;
    for (const Goap::Action& a : d.mActions)
    {
        const DomainStep& step = d.mSteps[a.mTag];
        lead = step.mKind == StepKind::eFleechLead && step.mEntity == 4 ? &a : lead;
        // (pulled plainly it wants them dead first)
        if (step.mKind == StepKind::eUse && step.mEntity == 4 && has(a.mEffects, {Key::Switch(103), 1}))
        {
            EXPECT_TRUE(has(a.mPre, {Key::Mud(1), 2})) << a.mName;
        }
    }
    ASSERT_NE(lead, nullptr);
    // Not with the power in hand: the chant for the Scrab would spend it
    EXPECT_TRUE(has(lead->mPre, {Key::kInvisNow, 0}));
    EXPECT_TRUE(has(lead->mEffects, {Key::Switch(103), 1}));
    for (u32 f = 1; f <= 3; f++)
    {
        EXPECT_TRUE(has(lead->mEffects, {Key::Mud(f), 2}));
    }
    // And that is how the lock in their pen is got at
    const Goap::Plan plan = Goap::Planner::Find(d.mStart, {{Key::Switch(106), 1}}, d.mActions);
    ASSERT_TRUE(plan.mFound);
    std::vector<std::string> names;
    for (s32 step : plan.mSteps)
    {
        if (d.mSteps[d.mActions[step].mTag].mKind != StepKind::eCross)
        {
            names.push_back(d.mActions[step].mName);
        }
    }
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "Let the Fleeches out with Lever#4 and lead them to Scrab#5");
    EXPECT_EQ(names[1], "Use SlapLock#6 sw106 on");
}

TEST(BotVault, AThingOnAScrabsFloorUnderALedgeIsUsedUnseen)
{
    // A floor with a Scrab that strikes at once, a ledge over it with a hoist down, and a lock on each
    NavInput in;
    in.mLines.push_back(Line(0, 460, 700, 460, 0));
    in.mLines.push_back(Line(200, 360, 300, 360, 0));
    NavFeature hoist = Feature(FeatureKind::eHoist, 200, 360, 224, 466);
    hoist.mDir = 1;
    in.mFeatures.push_back(hoist);
    NavGraph g;
    g.Build(in);
    World world;
    Entity scrab;
    scrab.mKind = EntityKind::eScrab;
    scrab.mId = 1;
    scrab.mX1 = 500;
    scrab.mX2 = 524;
    scrab.mY1 = 440;
    scrab.mY2 = 460;
    world.mEntities.push_back(scrab);
    Entity lock;
    lock.mKind = EntityKind::eSlapLock;
    lock.mId = 2;
    lock.mX1 = 400;
    lock.mX2 = 424;
    lock.mY1 = 440;
    lock.mY2 = 464;
    lock.mSwitch = 108;
    world.mEntities.push_back(lock);
    lock.mId = 3;
    lock.mX1 = 250;
    lock.mX2 = 274;
    lock.mY1 = 340;
    lock.mY2 = 364;
    lock.mSwitch = 107;
    world.mEntities.push_back(lock);
    const s32 abe = g.NodeAt(237, 360, false);
    ASSERT_GE(abe, 0);
    const auto unseen = [&](const World& w, u32 id) -> s32
    {
        const Domain d = Domain::Build(g, w, abe, [](s32)
                                       { return true; });
        s32 found = -1;
        for (const Goap::Action& a : d.mActions)
        {
            const DomainStep& step = d.mSteps[a.mTag];
            if (step.mKind == StepKind::eUse && step.mEntity == id)
            {
                found = std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::kInvisNow, 1}) != a.mPre.end() ? 1 : 0;
            }
        }
        return found;
    };
    // The one on its floor only unseen; the one on the ledge as he is
    EXPECT_EQ(unseen(world, 2), 1);
    EXPECT_EQ(unseen(world, 3), 0);
    // One that waits before it strikes is run from instead (the crossing is timed against its beat)
    World waits = world;
    waits.mEntities[0].mInputs[0] = 40;
    EXPECT_EQ(unseen(waits, 2), 0);
    // And with no Scrab there is nothing to hide from
    World none = world;
    none.mEntities.erase(none.mEntities.begin());
    EXPECT_EQ(unseen(none, 2), 0);
}

// The Mudomo hub's third ring: a floor with a Paramite on it, a ledge over it with the ring, and a web from the one
// up to the other
static NavInput WebRoom(bool web)
{
    NavInput in;
    in.mLines.push_back(Line(3000, 460, 3375, 460, 0));
    in.mLines.push_back(Line(3125, 340, 3250, 340, 0));
    if (web)
    {
        in.mLines.push_back(Line(3250, 340, 3250, 440, 8));
    }
    return in;
}

static World WebRoomWorld()
{
    World world;
    Entity ring;
    ring.mKind = EntityKind::ePullRing;
    ring.mId = 1;
    ring.mX1 = 3175;
    ring.mX2 = 3200;
    ring.mY1 = 250;
    ring.mY2 = 274;
    ring.mInputs[0] = 44;
    ring.mSwitch = 156;
    world.mEntities.push_back(ring);
    Entity paramite;
    paramite.mKind = EntityKind::eParamite;
    paramite.mId = 2;
    paramite.mX1 = 3200;
    paramite.mX2 = 3224;
    paramite.mY1 = 440;
    paramite.mY2 = 464;
    world.mEntities.push_back(paramite);
    return world;
}

static s32 ParamitePulls(const Domain& d)
{
    s32 count = 0;
    for (const Goap::Action& a : d.mActions)
    {
        count += a.mName.compare(0, 14, "Have Paramite#") == 0 ? 1 : 0;
    }
    return count;
}

TEST(BotParamite, AWebLeadsFromTheFloorAtItsFootToTheOneByItsTop)
{
    NavGraph g;
    g.Build(WebRoom(true));
    const s32 below = g.NodeAt(3212, 460, false);
    const s32 above = g.NodeAt(3187, 340, false);
    ASSERT_GE(below, 0);
    ASSERT_GE(above, 0);
    s32 webX = 0;
    EXPECT_TRUE(Domain::WebUp(g, below, above, &webX));
    EXPECT_EQ(webX, 3250);
    // (up only: it is got on from a jump at its foot)
    EXPECT_FALSE(Domain::WebUp(g, above, below, nullptr));
    NavGraph none;
    none.Build(WebRoom(false));
    EXPECT_FALSE(Domain::WebUp(none, none.NodeAt(3212, 460, false), none.NodeAt(3187, 340, false), nullptr));
}

TEST(BotParamite, AParamiteThatCanGetUnderARingIsHadToPullIt)
{
    // Taken over from a place on its screen that is not on its level, where no orb stops the chant
    NavGraph g;
    g.Build(WebRoom(true));
    World world = WebRoomWorld();
    const s32 from = g.NodeAt(3137, 340, false);
    {
        const Domain d = Domain::Build(g, world, from, nullptr);
        EXPECT_GT(ParamitePulls(d), 0);
        for (const Goap::Action& a : d.mActions)
        {
            if (a.mName.compare(0, 14, "Have Paramite#") == 0)
            {
                // (never from its own floor: cornered it strikes)
                EXPECT_EQ(a.mName.find(",460"), std::string::npos) << a.mName;
                EXPECT_NE(std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::Switch(156), 0}), a.mPre.end());
            }
        }
    }
    // No web: it cannot get to the ring's floor
    {
        NavGraph none;
        none.Build(WebRoom(false));
        const Domain d = Domain::Build(none, world, none.NodeAt(3137, 340, false), nullptr);
        EXPECT_EQ(ParamitePulls(d), 0);
    }
    // An orb on the screen: no chant
    {
        World orbed = world;
        Entity orb;
        orb.mKind = EntityKind::eSecurityOrb;
        orb.mId = 3;
        orb.mX1 = 3300;
        orb.mX2 = 3324;
        orb.mY1 = 380;
        orb.mY2 = 404;
        orbed.mEntities.push_back(orb);
        const Domain d = Domain::Build(g, orbed, from, nullptr);
        EXPECT_EQ(ParamitePulls(d), 0);
    }
    // One that is spawned later is not there to be taken over
    {
        World spawned = world;
        spawned.mEntities[1].mInputs[0] = 2;
        const Domain d = Domain::Build(g, spawned, from, nullptr);
        EXPECT_EQ(ParamitePulls(d), 0);
    }
}

// A floor 400 wide with Paramites seen on it
static NavInput ParamiteFloor(bool wall)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    if (wall)
    {
        in.mLines.push_back(Line(125, 40, 125, 100, 1));
    }
    return in;
}

static ParamiteAt SeenParamite(s32 x, s32 brain)
{
    ParamiteAt p;
    p.mId = 1;
    p.mX = x;
    p.mY = 100;
    p.mBrain = brain;
    return p;
}

TEST(BotParamite, ALoneParamiteWithLessThanEightCellsBehindItHasHimCornered)
{
    NavGraph g;
    g.Build(ParamiteFloor(false));
    World world;
    world.mParamites.push_back(SeenParamite(100, 0));
    // Its side of the floor ends 162 back from him: under the eight cells (200) it needs
    EXPECT_TRUE(Domain::ParamiteCornered(g, world, 162, 100, false));
    // 237 back is room enough
    EXPECT_FALSE(Domain::ParamiteCornered(g, world, 237, 100, false));
    // (a Paramite to his right is backed off to the right end instead)
    World right;
    right.mParamites.push_back(SeenParamite(300, 0));
    EXPECT_TRUE(Domain::ParamiteCornered(g, right, 237, 100, false));
    EXPECT_FALSE(Domain::ParamiteCornered(g, right, 12, 100, false));
    // Up its web it is not there, and one that is possessed is not a threat
    World hidden;
    hidden.mParamites.push_back(SeenParamite(100, 3));
    EXPECT_FALSE(Domain::ParamiteCornered(g, hidden, 162, 100, false));
    World possessed = world;
    possessed.mParamites[0].mPossessed = true;
    EXPECT_FALSE(Domain::ParamiteCornered(g, possessed, 162, 100, false));
}

TEST(BotParamite, AWallBetweenAParamiteAndHimTakesItsCornerAway)
{
    NavGraph g;
    g.Build(ParamiteFloor(true));
    World world;
    world.mParamites.push_back(SeenParamite(50, 0));
    EXPECT_FALSE(Domain::ParamiteCornered(g, world, 162, 100, false));
    EXPECT_FALSE(Domain::ParamitePack(g, world, 237, 100, false));
    // Without the wall the same place is a corner
    NavGraph open;
    open.Build(ParamiteFloor(false));
    EXPECT_TRUE(Domain::ParamiteCornered(open, world, 162, 100, false));
}

TEST(BotParamite, TwoSeenParamitesOnHisFloorMakeAPackButOneDoesNot)
{
    NavGraph g;
    g.Build(ParamiteFloor(false));
    World world;
    world.mParamites.push_back(SeenParamite(50, 0));
    EXPECT_FALSE(Domain::ParamitePack(g, world, 237, 100, false));
    world.mParamites.push_back(SeenParamite(300, 0));
    EXPECT_TRUE(Domain::ParamitePack(g, world, 237, 100, false));
    // One of the two up its web is not seen
    World hidden = world;
    hidden.mParamites[1].mBrain = 9;
    EXPECT_FALSE(Domain::ParamitePack(g, hidden, 237, 100, false));
    // Nor are two that are on another floor
    NavInput in = ParamiteFloor(false);
    in.mLines.push_back(Line(0, 300, 400, 300, 0));
    NavGraph two;
    two.Build(in);
    World below = world;
    below.mParamites[0].mY = 300;
    below.mParamites[1].mY = 300;
    EXPECT_FALSE(Domain::ParamitePack(two, below, 237, 100, false));
}

TEST(BotVault, AFleechEntirelyUnderALocksFloorIsNoGuardOfIt)
{
    NavGraph g;
    g.Build(FleechRoom(true));
    World world = FleechRoomWorld();
    const s32 abe = g.NodeAt(462, 700, false);
    ASSERT_GE(abe, 0);
    const auto guarded = [&](const World& w, u32 fleech) -> bool
    {
        const Domain d = Domain::Build(g, w, abe, [](s32)
                                       { return true; });
        for (const Goap::Action& a : d.mActions)
        {
            const DomainStep& step = d.mSteps[a.mTag];
            if (step.mKind == StepKind::eUse && step.mEntity == 3 && std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::Mud(fleech), 2}) != a.mPre.end())
            {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(guarded(world, 2));
    // The same Fleech hung from the underside of the lock's floor
    world.mEntities[1].mY1 = 705;
    world.mEntities[1].mY2 = 729;
    EXPECT_FALSE(guarded(world, 2));
}

TEST(BotNav, ABirdPortalToAnotherPathIsAWayOutFromTwoCellsBeforeIt)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature portal = Feature(FeatureKind::ePortal, 160, 80, 184, 104);
    portal.mDir = 0;
    portal.mDestPath = 5;
    portal.mDestCamera = 2;
    in.mFeatures.push_back(portal);
    NavGraph g;
    g.Build(in);
    ASSERT_EQ(g.Exits().size(), 1u);
    // (hopped into from two cells to its left)
    EXPECT_EQ(g.Exits()[0].mNode, g.NodeAt(112, 100, false));
    // One that stays in the path is no exit
    in.mFeatures[0].mDestPath = -1;
    NavGraph local;
    local.Build(in);
    EXPECT_TRUE(local.Exits().empty());
}

static NavInput PackCorridor()
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 370, 100, 0));
    NavFeature exit = Feature(FeatureKind::eDoor, 346, 80, 370, 104);
    exit.mDestPath = 2;
    exit.mDestCamera = 1;
    in.mFeatures.push_back(exit);
    return in;
}

static World PackWorld(s32 paramites, bool meat, bool invisLock)
{
    World world;
    if (meat)
    {
        Entity sack = MakeEntity(EntityKind::eRockSack, 3000, 20, 80, 44, 104);
        sack.mInputs[1] = 1;
        world.mEntities.push_back(sack);
    }
    for (s32 i = 0; i < paramites; i++)
    {
        world.mEntities.push_back(MakeEntity(EntityKind::eParamite, 10 + i, 150 + i * 60, 80, 174 + i * 60, 104));
    }
    if (invisLock)
    {
        Entity lock = MakeEntity(EntityKind::eSlapLock, 4000, 60, 80, 84, 104);
        lock.mInputs[0] = 1;
        world.mEntities.push_back(lock);
    }
    return world;
}

// (the exit's preconditions, or none if it is not offered; and whether meat is thrown for the pack)
static bool PackExitNeedsFeeding(const World& world, bool* throws)
{
    NavGraph g;
    g.Build(PackCorridor());
    const Domain d = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    bool fed = false;
    *throws = false;
    for (const Goap::Action& a : d.mActions)
    {
        if (a.mName.compare(0, 5, "Exit ") == 0)
        {
            fed = std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::kPackFed, 1}) != a.mPre.end();
        }
        *throws = *throws || a.mName.compare(0, 24, "Throw meat for the Param") == 0;
    }
    return fed;
}

TEST(BotParamite, AWayOutPastTwoParamitesOnItsLevelWantsMeatThrownFirst)
{
    bool throws = false;
    EXPECT_TRUE(PackExitNeedsFeeding(PackWorld(2, true, false), &throws));
    EXPECT_TRUE(throws);
}

TEST(BotParamite, OneParamiteOrNoMeatOrAGhostLockLeavesTheWayOutAlone)
{
    bool throws = true;
    // (one alone is cornered and left, not fed)
    EXPECT_FALSE(PackExitNeedsFeeding(PackWorld(1, true, false), &throws));
    EXPECT_FALSE(throws);
    EXPECT_FALSE(PackExitNeedsFeeding(PackWorld(2, false, false), &throws));
    EXPECT_FALSE(throws);
    // (he runs past them unseen instead)
    EXPECT_FALSE(PackExitNeedsFeeding(PackWorld(2, true, true), &throws));
    EXPECT_FALSE(throws);
}

TEST(BotParamite, ParamitesOnAnotherFloorOfTheScreenAreNotOnTheWaysLevel)
{
    NavInput in = PackCorridor();
    in.mLines.push_back(Line(0, 220, 370, 220, 0));
    NavGraph g;
    g.Build(in);
    World world = PackWorld(2, true, false);
    const s32 up = g.NodeAt(162, 100, false);
    const s32 down = g.NodeAt(162, 220, false);
    ASSERT_GE(up, 0);
    ASSERT_GE(down, 0);
    EXPECT_TRUE(Domain::PackLevel(g, world, up));
    EXPECT_FALSE(Domain::PackLevel(g, world, down));
    world.mEntities.pop_back();
    EXPECT_FALSE(Domain::PackLevel(g, world, up));
}

// A floor cut by a gate that starts shut: a lever on the left whose switch wakes a Fleech, and a foot switch at
// each end of the floor that works the gate (the one beyond it sets, the other clears, unless they are swapped)
static NavInput FleechPenRoom()
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature gate = Feature(FeatureKind::eBarrier, 200, 80, 224, 104);
    gate.mSwitch = 60;
    gate.mStartClosed = true;
    gate.mSolid = true;
    in.mFeatures.push_back(gate);
    return in;
}

static World FleechPenWorld(bool sameSide)
{
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eLever, 1, 25, 80, 99, 104));
    world.mEntities.back().mSwitch = 50;
    world.mEntities.push_back(MakeEntity(EntityKind::eFleech, 2, 300, 80, 324, 104));
    world.mEntities.back().mSwitch = 50;
    Entity on = MakeEntity(EntityKind::eFootSwitch, 3, 340, 90, 364, 104);
    on.mSwitch = 60;
    on.mOp = 0;
    world.mEntities.push_back(on);
    Entity off = MakeEntity(EntityKind::eFootSwitch, 4, 100, 90, 124, 104);
    off.mSwitch = 60;
    off.mOp = 1;
    if (sameSide)
    {
        off.mX1 = 240;
        off.mX2 = 264;
    }
    world.mEntities.push_back(off);
    return world;
}

TEST(BotVault, AFleechPenNeedsOneFootSwitchEachSideOfTheGate)
{
    NavGraph g;
    g.Build(FleechPenRoom());
    const World world = FleechPenWorld(false);
    s32 gate = -1;
    s32 open = -1;
    s32 shut = -1;
    std::vector<u32> group;
    ASSERT_TRUE(Domain::FleechPen(g, world, world.mEntities[0], &gate, &open, &shut, &group));
    EXPECT_EQ(g.Input().mFeatures[gate].mSwitch, 60);
    EXPECT_EQ(open, g.NodeAt(352, 100, false));
    EXPECT_EQ(shut, g.NodeAt(112, 100, false));
    ASSERT_EQ(group.size(), 1u);
    EXPECT_EQ(group[0], 2u);
    // Both on the lever's side: nothing beyond the gate to open it from
    const World same = FleechPenWorld(true);
    EXPECT_FALSE(Domain::FleechPen(g, same, same.mEntities[0], &gate, &open, &shut, &group));
    // A lever that wakes no Fleech has no pen
    World calm = world;
    calm.mEntities[1].mSwitch = 51;
    EXPECT_FALSE(Domain::FleechPen(g, calm, calm.mEntities[0], &gate, &open, &shut, &group));
}

TEST(BotVault, ALeverThatWakesFleechesIsOfferedWithTheGateShutBehindThem)
{
    NavGraph g;
    g.Build(FleechPenRoom());
    const auto offered = [&](const World& w) -> bool
    {
        const Domain d = Domain::Build(g, w, g.NodeAt(12, 100, false), nullptr);
        for (const Goap::Action& a : d.mActions)
        {
            if (a.mName.find("and shut its Fleeches in behind the gate") != std::string::npos)
            {
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(offered(FleechPenWorld(false)));
    EXPECT_FALSE(offered(FleechPenWorld(true)));
}

static const NavEdge* FindWellEdge(const NavGraph& g, s32 from, s32 to)
{
    for (const NavEdge& e : g.Edges())
    {
        if (e.mFrom == from && e.mTo == to && e.mWell >= 0)
        {
            return &e;
        }
    }
    return nullptr;
}

// A well that names no place to throw him to, on a floor with a ledge up and to its right whose hoist ends above
// the floor but reaches into the rise
static NavInput RiseRoom(bool hoist)
{
    NavInput in;
    in.mLines.push_back(Line(0, 724, 400, 724, 0));
    in.mLines.push_back(Line(250, 600, 400, 600, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eWellLocal, 201, 700, 225, 724));
    if (hoist)
    {
        in.mFeatures.push_back(Feature(FeatureKind::eHoist, 250, 600, 275, 690));
    }
    return in;
}

TEST(BotNav, AWellThatNamesNoPlaceLeavesHimHungFromAHoistInItsRise)
{
    NavGraph g;
    g.Build(RiseRoom(true));
    const s32 well = g.NodeAt(212, 724, false);
    const s32 ledge = g.NodeAt(262, 600, false);
    ASSERT_GE(well, 0);
    ASSERT_GE(ledge, 0);
    EXPECT_NE(FindWellEdge(g, well, ledge), nullptr);
    // Without the hoist he comes down on the floor, two cells to the right
    NavGraph none;
    none.Build(RiseRoom(false));
    EXPECT_EQ(FindWellEdge(none, well, none.NodeAt(262, 600, false)), nullptr);
    EXPECT_NE(FindWellEdge(none, well, none.NodeAt(262, 724, false)), nullptr);
}

TEST(BotNav, LettingGoDownAHoistOntoAWellOnTheFloorBelowGoesWhereTheWellLeads)
{
    // The well stands on the floor under the hoist, its box reaching well up from it
    NavInput in;
    in.mLines.push_back(Line(150, 100, 400, 100, 0));
    in.mLines.push_back(Line(0, 400, 400, 400, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eHoist, 200, 100, 224, 400));
    NavFeature well = Feature(FeatureKind::eWellLocal, 200, 340, 224, 404);
    well.mC = 337;
    well.mD = 396;
    in.mFeatures.push_back(well);
    NavGraph g;
    g.Build(in);
    const s32 top = g.NodeAt(212, 100, false);
    ASSERT_GE(top, 0);
    const NavEdge* out = FindEdge(g, top, g.NodeAt(337, 400, false), EdgeKind::eHoistDown);
    ASSERT_NE(out, nullptr);
    EXPECT_GE(out->mWell, 0);
    // (not straight down to the floor under him)
    EXPECT_EQ(FindEdge(g, top, g.NodeAt(212, 400, false), EdgeKind::eHoistDown), nullptr);
    // A well beside the shaft is not one he lands in
    in.mFeatures[1].mX1 = 300;
    in.mFeatures[1].mX2 = 324;
    NavGraph apart;
    apart.Build(in);
    EXPECT_NE(FindEdge(apart, top, apart.NodeAt(212, 400, false), EdgeKind::eHoistDown), nullptr);
}

TEST(BotNav, AnOpenTrapDoorOverAWellItsSwitchTurnsDropsHimWhereTheWellThrowsHimWhileItIsOn)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(250, 100, 400, 100, 0));
    in.mLines.push_back(Line(250, 300, 400, 300, 0));
    in.mLines.push_back(Line(0, 400, 400, 400, 0));
    NavFeature trap = Feature(FeatureKind::eTrapDoor, 200, 100, 250, 120);
    trap.mSwitch = 7;
    trap.mStartClosed = true;
    in.mFeatures.push_back(trap);
    // (it names no place with its switch off; with it on it throws him onto the floor between)
    NavFeature well = Feature(FeatureKind::eWellLocal, 205, 200, 230, 224);
    well.mSwitch = 7;
    well.mOnC = 337;
    well.mOnD = 296;
    in.mFeatures.push_back(well);
    NavGraph g;
    g.Build(in);
    const s32 on = g.NodeAt(212, 100, false);
    const s32 thrown = g.NodeAt(337, 300, false);
    const s32 under = g.NodeAt(212, 400, false);
    ASSERT_GE(on, 0);
    ASSERT_GE(thrown, 0);
    ASSERT_GE(under, 0);
    const NavEdge* fall = FindEdge(g, on, thrown, EdgeKind::eFall);
    ASSERT_NE(fall, nullptr);
    EXPECT_GE(fall->mWell, 0);
    EXPECT_EQ(FindEdge(g, on, under, EdgeKind::eFall), nullptr);
    // (and it is only open with the switch on)
    s32 sw = 0;
    s32 value = -1;
    EXPECT_TRUE(g.Condition(*fall, &sw, &value));
    EXPECT_EQ(sw, 7);
    EXPECT_EQ(value, 1);
}

TEST(BotNav, RolledOffALowEndOverAGapHeClearsItToTheFloorFortyDown)
{
    // A drill that comes over the end of the floor keeps him rolling, and the next floor starts two cells out
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    in.mLines.push_back(Line(450, 140, 700, 140, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 300, 40, 420, 64));
    NavGraph g;
    g.Build(in);
    const s32 end = g.NodeAt(387, 100, false);
    const s32 over = g.NodeAt(462, 140, false);
    ASSERT_GE(end, 0);
    ASSERT_GE(over, 0);
    EXPECT_TRUE(g.Nodes()[end].mLow);
    EXPECT_NE(FindEdge(g, end, over, EdgeKind::eFall), nullptr);
    // Standing upright he walks off the end into the gap instead
    NavInput tall = in;
    tall.mFeatures.clear();
    NavGraph upright;
    upright.Build(tall);
    EXPECT_EQ(FindEdge(upright, upright.NodeAt(387, 100, false), upright.NodeAt(462, 140, false), EdgeKind::eFall), nullptr);
    // A drop of 20 is a step down, not one that carries him that far
    NavInput shallow = in;
    shallow.mLines[1] = Line(450, 120, 700, 120, 0);
    NavGraph step;
    step.Build(shallow);
    EXPECT_EQ(FindEdge(step, step.NodeAt(387, 100, false), step.NodeAt(462, 120, false), EdgeKind::eFall), nullptr);
}

// ---- A Paramite that is played: its way, its floors, its rings ----

static bool HasLeg(const std::vector<Domain::ParaLeg>& legs, Domain::ParaLeg::Kind kind)
{
    for (const Domain::ParaLeg& leg : legs)
    {
        if (leg.mKind == kind)
        {
            return true;
        }
    }
    return false;
}

TEST(BotParamite, AWalkAlongOneFloorIsOneLeg)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavGraph g;
    g.Build(in);
    const World world;
    std::vector<Domain::ParaLeg> legs;
    ASSERT_TRUE(Domain::ParamiteRoute(g, world, g.NodeAt(12, 100, false), g.NodeAt(312, 100, false), &legs));
    ASSERT_EQ(legs.size(), 1u);
    EXPECT_EQ(legs[0].mKind, Domain::ParaLeg::eWalk);
    EXPECT_EQ(legs[0].mTo, g.NodeAt(312, 100, false));
}

TEST(BotParamite, AWebIsClimbedDownFromTheLedgeAndUpFromTheFloor)
{
    // A ledge too high to fall from (210), so the web is the way down
    NavInput in;
    in.mLines.push_back(Line(0, 460, 400, 460, 0));
    in.mLines.push_back(Line(100, 250, 250, 250, 0));
    in.mLines.push_back(Line(250, 250, 250, 440, 8));
    NavGraph g;
    g.Build(in);
    const World world;
    const s32 ledge = g.NodeAt(212, 250, false);
    const s32 floor = g.NodeAt(212, 460, false);
    ASSERT_GE(ledge, 0);
    ASSERT_GE(floor, 0);
    std::vector<Domain::ParaLeg> down;
    ASSERT_TRUE(Domain::ParamiteRoute(g, world, ledge, floor, &down));
    EXPECT_TRUE(HasLeg(down, Domain::ParaLeg::eWebDown));
    EXPECT_FALSE(HasLeg(down, Domain::ParaLeg::eWebUp));
    for (const Domain::ParaLeg& leg : down)
    {
        if (leg.mKind == Domain::ParaLeg::eWebDown)
        {
            EXPECT_EQ(leg.mX, 250);
        }
    }
    std::vector<Domain::ParaLeg> up;
    ASSERT_TRUE(Domain::ParamiteRoute(g, world, floor, ledge, &up));
    EXPECT_TRUE(HasLeg(up, Domain::ParaLeg::eWebUp));
    // Without the web there is no way up at all
    in.mLines.pop_back();
    NavGraph none;
    none.Build(in);
    EXPECT_FALSE(Domain::ParamiteRoute(none, world, none.NodeAt(212, 460, false), none.NodeAt(212, 250, false), nullptr));
}

TEST(BotParamite, AGapOfThreeCellsIsHoppedToTheFourthAndOneOfFourIsNot)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(175, 100, 300, 100, 0));
    NavGraph g;
    g.Build(in);
    const World world;
    std::vector<Domain::ParaLeg> legs;
    ASSERT_TRUE(Domain::ParamiteRoute(g, world, g.NodeAt(12, 100, false), g.NodeAt(212, 100, false), &legs));
    ASSERT_TRUE(HasLeg(legs, Domain::ParaLeg::eHop));
    for (const Domain::ParaLeg& leg : legs)
    {
        if (leg.mKind == Domain::ParaLeg::eHop)
        {
            EXPECT_EQ(leg.mTo, g.NodeAt(187, 100, false));
        }
    }
    // One cell wider and the fourth cell is no floor
    in.mLines[1] = Line(200, 100, 325, 100, 0);
    NavGraph wide;
    wide.Build(in);
    EXPECT_FALSE(Domain::ParamiteRoute(wide, world, wide.NodeAt(12, 100, false), wide.NodeAt(212, 100, false), nullptr));
}

TEST(BotParamite, ADropOffTheEndOfAFloorIsAFallOnlyUnderOneHundredAndSeventy)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(0, 250, 400, 250, 0));
    NavGraph g;
    g.Build(in);
    const World world;
    std::vector<Domain::ParaLeg> legs;
    ASSERT_TRUE(Domain::ParamiteRoute(g, world, g.NodeAt(12, 100, false), g.NodeAt(312, 250, false), &legs));
    EXPECT_TRUE(HasLeg(legs, Domain::ParaLeg::eFall));
    // 200 down is too far for it
    in.mLines[1] = Line(0, 300, 400, 300, 0);
    NavGraph deep;
    deep.Build(in);
    EXPECT_FALSE(Domain::ParamiteRoute(deep, world, deep.NodeAt(12, 100, false), deep.NodeAt(312, 300, false), nullptr));
}

TEST(BotParamite, ABlockThatStopsShortOfItsWallHeightIsWalkedUnderAndAWallToTheFloorIsNot)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    // Down to 21 over the floor: under the 20 it looks at
    in.mLines.push_back(Line(200, 0, 200, 79, 1));
    NavGraph g;
    g.Build(in);
    const World world;
    EXPECT_TRUE(Domain::ParamiteRoute(g, world, g.NodeAt(12, 100, false), g.NodeAt(387, 100, false), nullptr));
    EXPECT_FALSE(Domain::ParamiteWall(g, 100, 300, 100, false));
    in.mLines[1] = Line(200, 0, 200, 100, 1);
    NavGraph wall;
    wall.Build(in);
    EXPECT_FALSE(Domain::ParamiteRoute(wall, world, wall.NodeAt(12, 100, false), wall.NodeAt(387, 100, false), nullptr));
    EXPECT_TRUE(Domain::ParamiteWall(wall, 100, 300, 100, false));
}

TEST(BotParamite, ABlockHangingFortyOverTheFloorStopsAScrabButNotAParamite)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    in.mLines.push_back(Line(200, 0, 200, 60, 1));
    NavGraph g;
    g.Build(in);
    // (a Scrab looks 45 over its feet, a Paramite 20)
    EXPECT_TRUE(Domain::WallBetween(g, 100, 300, 100, false));
    EXPECT_FALSE(Domain::ParamiteWall(g, 100, 300, 100, false));
    // Not a wall that is not between the two places
    EXPECT_FALSE(Domain::WallBetween(g, 250, 300, 100, false));
}

TEST(BotParamite, ALiftsStopIsFloorForAParamiteOnlyWhileTheLiftIsAtIt)
{
    NavInput in;
    in.mLines.push_back(Line(0, 300, 400, 300, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 100, 200, 110));
    NavGraph g;
    g.Build(in);
    const s32 stop = g.NodeAt(137, 100, false);
    ASSERT_GE(stop, 0);
    ASSERT_GE(g.Nodes()[stop].mLiftStop, 0);
    World world;
    EXPECT_EQ(Domain::ParamiteFloor(g, world, 137, 100, false), -1);
    LiftAt away;
    away.mX1 = 100;
    away.mX2 = 200;
    away.mY = 300;
    world.mLifts.push_back(away);
    EXPECT_EQ(Domain::ParamiteFloor(g, world, 137, 100, false), -1);
    world.mLifts[0].mY = 100;
    EXPECT_EQ(Domain::ParamiteFloor(g, world, 137, 100, false), stop);
    // (a plain floor needs no lift)
    EXPECT_GE(Domain::ParamiteFloor(g, world, 337, 300, false), 0);
}

TEST(BotParamite, ACellInsideAShutGateIsNoFloorForAParamite)
{
    NavGraph g;
    g.Build(FleechPenRoom());
    World world;
    EXPECT_EQ(Domain::ParamiteFloor(g, world, 212, 100, false), -1);
    EXPECT_GE(Domain::ParamiteFloor(g, world, 162, 100, false), 0);
    // Open, the cell is floor
    world.mSwitches[60] = 1;
    EXPECT_GE(Domain::ParamiteFloor(g, world, 212, 100, false), 0);
}

static Entity TourRing(u32 id, s32 centreX, s32 sw)
{
    Entity ring = MakeEntity(EntityKind::ePullRing, id, centreX - 12, 40, centreX + 12, 64);
    ring.mSwitch = sw;
    return ring;
}

TEST(BotParamite, RingsComeNearestFirstEachFromTheOneBefore)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavGraph g;
    g.Build(in);
    World world;
    world.mEntities.push_back(TourRing(1, 312, 5));
    world.mEntities.push_back(TourRing(2, 112, 6));
    world.mEntities.push_back(TourRing(3, 212, 7));
    const std::vector<u32> tour = Domain::ParamiteTour(g, world, g.NodeAt(12, 100, false));
    ASSERT_EQ(tour.size(), 3u);
    EXPECT_EQ(tour[0], 2u);
    EXPECT_EQ(tour[1], 3u);
    EXPECT_EQ(tour[2], 1u);
    // From the far end the order is the other way round
    const std::vector<u32> back = Domain::ParamiteTour(g, world, g.NodeAt(387, 100, false));
    ASSERT_EQ(back.size(), 3u);
    EXPECT_EQ(back[0], 1u);
}

TEST(BotParamite, ARingWhoseSwitchIsOnOrBehindAShutGateIsLeftOutOfTheTour)
{
    NavGraph g;
    g.Build(FleechPenRoom());
    World world;
    world.mEntities.push_back(TourRing(1, 62, 5));
    world.mEntities.push_back(TourRing(2, 112, 6));
    world.mEntities.push_back(TourRing(3, 312, 7));
    world.mSwitches[5] = 1;
    // (ring 1 is pulled already, ring 3 is past the gate that is shut)
    std::vector<u32> tour = Domain::ParamiteTour(g, world, g.NodeAt(12, 100, false));
    ASSERT_EQ(tour.size(), 1u);
    EXPECT_EQ(tour[0], 2u);
    // With the gate open it is gone to as well
    world.mSwitches[60] = 1;
    tour = Domain::ParamiteTour(g, world, g.NodeAt(12, 100, false));
    ASSERT_EQ(tour.size(), 2u);
    EXPECT_EQ(tour[1], 3u);
}

// ---- Rolling off and jumping to ledges ----

TEST(BotNav, RolledOffALowEndHeCatchesALedgeOutAndBelowThatFacesTheWayHeGoes)
{
    // A drill over the end of the floor keeps him low there; no floor under what is past it
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    in.mLines.push_back(Line(480, 200, 700, 200, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 300, 40, 420, 64));
    NavFeature ledge = Feature(FeatureKind::eEdge, 470, 200, 495, 224);
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    const s32 end = g.NodeAt(387, 100, false);
    const s32 hung = g.NodeAt(487, 200, false);
    ASSERT_GE(end, 0);
    ASSERT_GE(hung, 0);
    ASSERT_TRUE(g.Nodes()[end].mLow);
    EXPECT_NE(FindEdge(g, end, hung, EdgeKind::eFall), nullptr);
    // A ledge that is caught the other way round is not
    in.mFeatures[1].mDir = 0;
    NavGraph other;
    other.Build(in);
    EXPECT_EQ(FindEdge(other, other.NodeAt(387, 100, false), other.NodeAt(487, 200, false), EdgeKind::eFall), nullptr);
    // And a hoist that faces him is caught the same
    in.mFeatures[1].mKind = FeatureKind::eHoist;
    in.mFeatures[1].mDir = 1;
    NavGraph hoist;
    hoist.Build(in);
    EXPECT_NE(FindEdge(hoist, hoist.NodeAt(387, 100, false), hoist.NodeAt(487, 200, false), EdgeKind::eFall), nullptr);
}

TEST(BotNav, OffTheLastCellOfAFloorARunningJumpCatchesAnEdgeUpToAHundredHigher)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(140, 40, 300, 40, 0));
    NavFeature ledge = Feature(FeatureKind::eEdge, 147, 40, 171, 64);
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    const s32 from = g.NodeAt(62, 100, false);
    const s32 top = g.NodeAt(162, 40, false);
    ASSERT_GE(from, 0);
    ASSERT_GE(top, 0);
    const NavEdge* jump = FindEdge(g, from, top, EdgeKind::eRunJump);
    ASSERT_NE(jump, nullptr) << g.Text();
    // (off the last cell, 87)
    EXPECT_EQ(jump->mX, 87);
    // 150 up is out of its reach
    NavInput high;
    high.mLines.push_back(Line(0, 100, 100, 100, 0));
    high.mLines.push_back(Line(140, -50, 300, -50, 0));
    NavFeature far = Feature(FeatureKind::eEdge, 147, -50, 171, -26);
    far.mDir = 1;
    high.mFeatures.push_back(far);
    NavGraph none;
    none.Build(high);
    EXPECT_EQ(FindEdge(none, none.NodeAt(62, 100, false), none.NodeAt(162, -50, false), EdgeKind::eRunJump), nullptr);
}

// ---- Wells ----

// A floor with a well on it that throws to (x, y), and what is under that place
static NavInput ThrowRoom(s32 x, s32 y)
{
    NavInput in;
    in.mLines.push_back(Line(0, 300, 200, 300, 0));
    NavFeature well = Feature(FeatureKind::eWellLocal, 100, 276, 124, 300);
    well.mC = x;
    well.mD = y;
    in.mFeatures.push_back(well);
    return in;
}

TEST(BotNav, ThrownToAPointWellOverAFloorHeComesDownACellAndAHalfOn)
{
    NavInput in = ThrowRoom(330, 400);
    in.mLines.push_back(Line(250, 500, 700, 500, 0));
    NavGraph g;
    g.Build(in);
    const s32 well = g.NodeAt(112, 300, false);
    ASSERT_GE(well, 0);
    // 100 over the floor he flies on to the right, from 337 to 362
    EXPECT_NE(FindWellEdge(g, well, g.NodeAt(362, 500, false)), nullptr);
    EXPECT_EQ(FindWellEdge(g, well, g.NodeAt(337, 500, false)), nullptr);
    // Only 40 over it he comes straight down
    NavInput low = ThrowRoom(330, 460);
    low.mLines.push_back(Line(250, 500, 700, 500, 0));
    NavGraph near;
    near.Build(low);
    EXPECT_NE(FindWellEdge(near, near.NodeAt(112, 300, false), near.NodeAt(337, 500, false)), nullptr);
    EXPECT_EQ(FindWellEdge(near, near.NodeAt(112, 300, false), near.NodeAt(362, 500, false)), nullptr);
}

TEST(BotNav, ThrownToAPointWithNoFloorUnderItHeDriftsOnlyAsFarAsTheFallLetsHim)
{
    // Far below a cell on; a ledge three cells on and 20 lower than the point
    NavInput in = ThrowRoom(330, 400);
    in.mLines.push_back(Line(345, 900, 700, 900, 0));
    in.mLines.push_back(Line(395, 420, 500, 420, 0));
    NavGraph g;
    g.Build(in);
    const s32 well = g.NodeAt(112, 300, false);
    ASSERT_GE(well, 0);
    EXPECT_NE(FindWellEdge(g, well, g.NodeAt(362, 900, false)), nullptr);
    EXPECT_EQ(FindWellEdge(g, well, g.NodeAt(412, 420, false)), nullptr);
    // With only the ledge, 20 of fall is not time to get three cells on
    in.mLines.erase(in.mLines.begin() + 1);
    NavGraph shallow;
    shallow.Build(in);
    EXPECT_EQ(FindWellEdge(shallow, shallow.NodeAt(112, 300, false), shallow.NodeAt(412, 420, false)), nullptr);
}

// A way out through an express well that is open only with its switch on
static NavInput OnlyWithSwitchWellRoom(s32 onDestPath)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature well = Feature(FeatureKind::eWellExpress, 100, 76, 124, 100);
    well.mSwitch = 9;
    well.mOnDestPath = onDestPath;
    well.mOnDestCamera = 1;
    in.mFeatures.push_back(well);
    return in;
}

TEST(BotNav, AWellThatLeadsOutOfThePathOnlyWithItsSwitchOnIsAnExit)
{
    NavGraph g;
    g.Build(OnlyWithSwitchWellRoom(4));
    ASSERT_FALSE(g.Exits().empty());
    EXPECT_EQ(g.Input().mFeatures[g.Exits()[0].mFeature].mKind, FeatureKind::eWellExpress);
    NavGraph stays;
    stays.Build(OnlyWithSwitchWellRoom(-1));
    EXPECT_TRUE(stays.Exits().empty());
}

TEST(BotNav, TheExitThroughAWellThatIsOpenOnlyWithItsSwitchOnWaitsForTheSwitch)
{
    NavGraph g;
    g.Build(OnlyWithSwitchWellRoom(4));
    const Domain d = Domain::Build(g, World(), g.NodeAt(12, 100, false), nullptr);
    bool found = false;
    for (const Goap::Action& a : d.mActions)
    {
        if (a.mName.compare(0, 5, "Exit ") == 0)
        {
            found = true;
            EXPECT_NE(std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::Switch(9), 1}), a.mPre.end());
        }
    }
    EXPECT_TRUE(found);
}

TEST(BotNav, AWellWhoseSwitchIsSetByTheZoneHeStandsInGoesWhereThatSetsItTo)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(250, 300, 400, 300, 0));
    NavFeature well = Feature(FeatureKind::eWellLocal, 100, 76, 124, 100);
    well.mSwitch = 7;
    well.mC = 287;
    well.mD = 300;
    well.mOnC = 362;
    well.mOnD = 300;
    in.mFeatures.push_back(well);
    NavGraph both;
    both.Build(in);
    const s32 from = both.NodeAt(112, 100, false);
    const s32 off = both.NodeAt(287, 300, false);
    const s32 on = both.NodeAt(362, 300, false);
    ASSERT_GE(from, 0);
    // Without a zone it goes by the switch: one edge for each value
    const NavEdge* byOff = FindWellEdge(both, from, off);
    const NavEdge* byOn = FindWellEdge(both, from, on);
    ASSERT_NE(byOff, nullptr);
    ASSERT_NE(byOn, nullptr);
    EXPECT_EQ(byOff->mWhen, 0);
    EXPECT_EQ(byOn->mWhen, 1);
    // A zone that turns it on, where he stands: the one edge, to where on leads, whatever the switch is
    NavFeature zone = Feature(FeatureKind::eSwitchZone, 100, 80, 124, 95);
    zone.mSwitch = 7;
    zone.mA = 0;
    in.mFeatures.push_back(zone);
    NavGraph set;
    set.Build(in);
    const NavEdge* only = FindWellEdge(set, from, on);
    ASSERT_NE(only, nullptr);
    EXPECT_EQ(only->mWhen, -1);
    EXPECT_EQ(FindWellEdge(set, from, off), nullptr);
    // And one that turns it off sends him to the other place
    in.mFeatures.back().mA = 1;
    NavGraph unset;
    unset.Build(in);
    EXPECT_NE(FindWellEdge(unset, from, off), nullptr);
    EXPECT_EQ(FindWellEdge(unset, from, on), nullptr);
}

// ---- Foot switches that turn a gate over ----

static World PlateWorld()
{
    World world;
    // The nearer to the gate (centre 212) is id 3, the one beyond it id 4
    Entity near = MakeEntity(EntityKind::eFootSwitch, 3, 150, 90, 174, 104);
    near.mSwitch = 60;
    near.mOp = 2;
    world.mEntities.push_back(near);
    Entity far = MakeEntity(EntityKind::eFootSwitch, 4, 340, 90, 364, 104);
    far.mSwitch = 60;
    far.mOp = 2;
    world.mEntities.push_back(far);
    return world;
}

TEST(BotFlierSight, AFloorCeilingOrWallBetweenTwoPointsBlocksTheSightOfAFlyingSlig)
{
    // A wall from 100,0 to 100,50: between points either side of it at 25, not at 80 (under its end), and not
    // between two on one side of it
    NavInput in;
    in.mLines.push_back(Line(100, 0, 100, 50, 1));
    EXPECT_TRUE(Domain::SightBlocked(in, false, 50, 25, 150, 25));
    EXPECT_FALSE(Domain::SightBlocked(in, false, 50, 80, 150, 80));
    EXPECT_FALSE(Domain::SightBlocked(in, false, 20, 25, 80, 25));
    // (a line of the other scale is none)
    EXPECT_FALSE(Domain::SightBlocked(in, true, 50, 25, 150, 25));
    // A line that only stops what flies (17) counts for a grenade's flight, not for sight
    NavInput fence;
    fence.mLines.push_back(Line(100, 0, 100, 50, 17));
    EXPECT_FALSE(Domain::SightBlocked(fence, false, 50, 25, 150, 25));
    EXPECT_TRUE(Domain::SightBlocked(fence, false, 50, 25, 150, 25, true));
    // (a floor too: from over it to under it)
    NavInput floor;
    floor.mLines.push_back(Line(0, 100, 200, 100, 0));
    EXPECT_TRUE(Domain::SightBlocked(floor, false, 50, 60, 150, 140));
    EXPECT_FALSE(Domain::SightBlocked(floor, false, 50, 60, 150, 90));
}

TEST(BotFlierSight, ATracksStretchesAreJoinedWhenTheirEndsAreAsNearAsAskedFor)
{
    // The Barracks' path 14: two stretches of one track with ends 3 apart (808,1442 and 811,1439)
    NavInput in;
    in.mLines.push_back(Line(786, 1350, 808, 1442, 8));
    in.mLines.push_back(Line(811, 1439, 977, 1439, 8));
    s32 x1 = 0;
    s32 y1 = 0;
    s32 x2 = 0;
    s32 y2 = 0;
    ASSERT_TRUE(Domain::FlierTrack(in, 790, 1360, x1, y1, x2, y2));
    EXPECT_EQ(x2, 808);
    ASSERT_TRUE(Domain::FlierTrack(in, 790, 1360, x1, y1, x2, y2, nullptr, nullptr, 3));
    EXPECT_EQ(x1, 786);
    EXPECT_EQ(x2, 977);
    EXPECT_EQ(y1, 1350);
    EXPECT_EQ(y2, 1442);
}

TEST(BotFlierSight, ALeverUnderTheTrackOfAFlyingSligThatIsAboutIsDearForHimButNotOneItsScreenOnlyJustHas)
{
    // A floor at 1500 from 400 to 1900 (screens are 375 wide), a flying Slig about on a track from 1186 to 1551
    // at 1440, and three levers: at 1300 under the middle of it, at 1564 on the next screen (which has 51 of the
    // track), and at 600 with no track near
    NavInput in;
    in.mIsAe = true;
    in.mLines.push_back(Line(400, 1500, 1900, 1500, 0));
    in.mLines.push_back(Line(1186, 1440, 1551, 1440, 8));
    World world;
    Entity under = MakeEntity(EntityKind::eLever, 11, 1263, 1480, 1337, 1504);
    under.mSwitch = 30;
    under.mOp = 2;
    Entity edge = MakeEntity(EntityKind::eLever, 12, 1527, 1480, 1601, 1504);
    edge.mSwitch = 31;
    edge.mOp = 2;
    Entity off = MakeEntity(EntityKind::eLever, 13, 551, 1480, 627, 1504);
    off.mSwitch = 32;
    off.mOp = 2;
    world.mEntities.push_back(under);
    world.mEntities.push_back(edge);
    world.mEntities.push_back(off);
    FlierAt flier;
    flier.mX = 1300;
    flier.mY = 1455;
    world.mFliers.push_back(flier);
    NavGraph g;
    g.Build(in);
    const auto cost = [&g](const World& w, u32 lever)
    {
        const Domain d = Domain::Build(g, w, g.NodeAt(412, 1500, false), nullptr);
        s32 least = -1;
        for (const Goap::Action& a : d.mActions)
        {
            const DomainStep& step = d.mSteps[a.mTag];
            if (step.mKind == StepKind::eUse && step.mEntity == lever && (least < 0 || a.mCost < least))
            {
                least = a.mCost;
            }
        }
        return least;
    };
    const s32 plain = cost(world, 13);
    ASSERT_GT(plain, 0);
    EXPECT_EQ(cost(world, 11), plain + 1500);
    EXPECT_EQ(cost(world, 12), plain);
    // With no flying Slig about none is dear
    World quiet = world;
    quiet.mFliers.clear();
    EXPECT_EQ(cost(quiet, 11), plain);
}

TEST(BotFlierSight, ATasksDoorWithLeversForHubsSetsItsOwnSwitchWhenTheyAreAllOn)
{
    // The Barracks' path 14: a tasks door (its "all hubs on" a switch of the bot's own, 256 and up) whose own
    // switch is 218 and whose hubs are 208 and 210, each a lever's
    NavInput in;
    in.mIsAe = true;
    in.mLines.push_back(Line(0, 100, 600, 100, 0));
    World world;
    Entity a = MakeEntity(EntityKind::eLever, 1, 88, 80, 112, 104);
    a.mSwitch = 208;
    Entity b = MakeEntity(EntityKind::eLever, 2, 288, 80, 312, 104);
    b.mSwitch = 210;
    Entity door = MakeEntity(EntityKind::eSwitchLogic, 3, 488, 80, 512, 104);
    door.mSwitch = World::DoorSwitch(3);
    door.mState = 218;
    door.mInputs[0] = 208;
    door.mInputs[1] = 210;
    world.mEntities.push_back(a);
    world.mEntities.push_back(b);
    world.mEntities.push_back(door);
    NavGraph g;
    g.Build(in);
    const auto gives = [&g](const World& w)
    {
        const Domain d = Domain::Build(g, w, g.NodeAt(12, 100, false), nullptr);
        for (const Goap::Action& act : d.mActions)
        {
            if (d.mSteps[act.mTag].mKind == StepKind::eLogic && std::find(act.mEffects.begin(), act.mEffects.end(), Goap::Fact{Key::Switch(218), 1}) != act.mEffects.end())
            {
                EXPECT_NE(std::find(act.mPre.begin(), act.mPre.end(), Goap::Fact{Key::Switch(208), 1}), act.mPre.end());
                EXPECT_NE(std::find(act.mPre.begin(), act.mPre.end(), Goap::Fact{Key::Switch(210), 1}), act.mPre.end());
                return true;
            }
        }
        return false;
    };
    EXPECT_TRUE(gives(world));
    // Not with a hub that no lever sets (a wheel's, a lock's): those blocks are got through one switch at a time
    World wheel = world;
    wheel.mEntities[1].mKind = EntityKind::eWorkWheel;
    EXPECT_FALSE(gives(wheel));
    // Nor a logic that is no tasks door (its switch one of the game's)
    World plain = world;
    plain.mEntities[2].mSwitch = 99;
    EXPECT_FALSE(gives(plain));
}

TEST(BotZone, AZoneThatSetsASwitchWhenHeIsInItIsAThingToDoWhereACellIsInIt)
{
    // A floor from 0 to 400 at 100, and over 200 to 250 of it a zone that sets switch 77 (the Barracks' path 14:
    // the trap door at 1700,1420 is opened by the zone he drops into)
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    NavFeature zone = Feature(FeatureKind::eSwitchZone, 200, 80, 250, 104);
    zone.mSwitch = 77;
    zone.mA = 0;
    in.mFeatures.push_back(zone);
    NavGraph g;
    g.Build(in);
    const World world;
    const Domain d = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    s32 found = 0;
    for (const Goap::Action& a : d.mActions)
    {
        const DomainStep& step = d.mSteps[a.mTag];
        if (step.mKind != StepKind::eZone)
        {
            continue;
        }
        found++;
        EXPECT_EQ(step.mOther, 77u);
        // (a cell of the floor that is in it)
        EXPECT_GE(g.Nodes()[step.mEdge].mX, 200);
        EXPECT_LE(g.Nodes()[step.mEdge].mX, 250);
        EXPECT_NE(std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::Switch(77), 0}), a.mPre.end());
        EXPECT_NE(std::find(a.mEffects.begin(), a.mEffects.end(), Goap::Fact{Key::Switch(77), 1}), a.mEffects.end());
    }
    EXPECT_EQ(found, 1);
    // Not one that is over no cell (up off the floor), one that sets its switch off, or one whose switch is on
    const auto count = [&world](const NavInput& input, const World& w)
    {
        NavGraph graph;
        graph.Build(input);
        const Domain domain = Domain::Build(graph, w, graph.NodeAt(12, 100, false), nullptr);
        s32 n = 0;
        for (const Goap::Action& a : domain.mActions)
        {
            n += domain.mSteps[a.mTag].mKind == StepKind::eZone ? 1 : 0;
        }
        return n;
    };
    NavInput high = in;
    high.mFeatures[0].mY1 = 20;
    high.mFeatures[0].mY2 = 60;
    EXPECT_EQ(count(high, world), 0);
    NavInput off = in;
    off.mFeatures[0].mA = 1;
    EXPECT_EQ(count(off, world), 0);
    World on;
    on.mSwitches[77] = 1;
    EXPECT_EQ(count(in, on), 0);
    EXPECT_EQ(count(in, world), 1);
}

TEST(BotVault, AToggleFootSwitchOfAGatesSwitchIsSteppedOnAndTheShutGateWantsTheNearestOne)
{
    NavGraph g;
    g.Build(FleechPenRoom());
    const World world = PlateWorld();
    const Domain d = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    s32 steps = 0;
    s32 gateCrossings = 0;
    for (const Goap::Action& a : d.mActions)
    {
        if (a.mName.compare(0, 18, "Step on FootSwitch") == 0)
        {
            steps++;
        }
        const DomainStep& step = d.mSteps[a.mTag];
        if (step.mKind != StepKind::eCross || g.Edges()[step.mEdge].mFeature < 0 || g.Input().mFeatures[g.Edges()[step.mEdge].mFeature].mKind != FeatureKind::eBarrier)
        {
            continue;
        }
        gateCrossings++;
        EXPECT_NE(std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::kPlate, 3}), a.mPre.end()) << a.mName;
        EXPECT_EQ(std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{Key::kPlate, 4}), a.mPre.end()) << a.mName;
    }
    EXPECT_EQ(steps, 2);
    EXPECT_GT(gateCrossings, 0);
    // A plate that sets rather than turns over (mOp 0) is no such step, and the crossing wants none
    World sets = world;
    sets.mEntities[0].mOp = 0;
    sets.mEntities[1].mOp = 0;
    const Domain e = Domain::Build(g, sets, g.NodeAt(12, 100, false), nullptr);
    for (const Goap::Action& a : e.mActions)
    {
        EXPECT_NE(a.mName.compare(0, 18, "Step on FootSwitch"), 0) << a.mName;
        for (const Goap::Fact& f : a.mPre)
        {
            EXPECT_NE(f.mKey, Key::kPlate) << a.mName;
        }
    }
}

// ---- Switches of gates that are gone ----

TEST(BotWorld, ASwitchOfAGateThatIsGoneIsAsItWasSeenExceptWhereTheRealOneIsAsked)
{
    World world;
    world.mSwitches[5] = 0;
    EXPECT_EQ(world.Switch(5), 0);
    world.mGatesGone[5] = 1;
    EXPECT_EQ(world.Switch(5), 1);
    EXPECT_EQ(world.Switch(5, true), 0);
    // (others are as they are)
    world.mSwitches[6] = 1;
    EXPECT_EQ(world.Switch(6), 1);
}

// ---- AO (Rupture Farms) ----

static NavInput AoInput()
{
    NavInput in;
    in.mIsAe = false;
    in.mCamW = 1024;
    in.mCamH = 480;
    return in;
}

static const ThrowSpot::Kind kAllThrows[] = {ThrowSpot::eUp, ThrowSpot::eLob, ThrowSpot::eForward, ThrowSpot::eDown, ThrowSpot::eCrouched};

TEST(BotThrowArc, AnAoGrenadeFliesAndLiesOutItsFuseOnTheFloor)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 460, 1500, 460, 0));
    for (ThrowSpot::Kind kind : kAllThrows)
    {
        const ThrowArc::Flight flight = ThrowArc::Throw(in, 512, 460, false, false, kind);
        // The hand and the 76 frames of the fuse, and nothing hit with no mine to aim at
        ASSERT_EQ(flight.mPoints.size(), 77u);
        EXPECT_EQ(flight.mHit, -1);
        // It comes to rest on the floor, sliding on less and less
        EXPECT_EQ(flight.mPoints.back().mY, 460);
        EXPECT_LE(std::abs(flight.mPoints.back().mX - flight.mPoints[flight.mPoints.size() - 2].mX), 1);
    }
    // As measured on the floor of the test: a crouched throw lies at 666
    EXPECT_EQ(ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched).mPoints.back().mX, 666);
}

TEST(BotThrowArc, AThrowIsAGrenadeInAoAndARockInAe)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 460, 1500, 460, 0));
    const ThrowArc::Point rest = ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched).mPoints.back();
    const ThrowArc::Point mine{rest.mX + 10, 460};
    // The grenade lies there till its fuse is out, and then it goes off with the mine in reach
    const ThrowArc::Flight ao = ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched, &mine);
    EXPECT_EQ(ao.mHit, 76);
    EXPECT_EQ(ao.mPoints.size(), 77u);
    // A rock does not: it flies over a mine that far off and is not a fuse's worth of frames in the air
    in.mIsAe = true;
    const ThrowArc::Flight ae = ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched, &mine);
    EXPECT_NE(ae.mHit, 76);
    EXPECT_NE(ae.mPoints.size(), 77u);
}

TEST(BotThrowArc, AGrenadeThatLiesWithinFortyFivePixelsOfAMineSetsItOff)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 460, 1500, 460, 0));
    const s32 rest = ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched).mPoints.back().mX;
    const auto hit = [&](s32 mineX)
    {
        const ThrowArc::Point mine{mineX, 460};
        return ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched, &mine).mHit;
    };
    EXPECT_EQ(hit(rest + 45), 76);
    EXPECT_EQ(hit(rest + 46), -1);
    // Behind it, too: only that the rock never touched it on its way
    EXPECT_EQ(hit(rest + 1), 76);
}

TEST(BotThrowArc, AGrenadeIsSureOfAMineOnlyTillThirtySevenPixelsOff)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 460, 1500, 460, 0));
    const s32 rest = ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched).mPoints.back().mX;
    const auto flight = [&](s32 mineX)
    {
        const ThrowArc::Point mine{mineX, 460};
        return ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched, &mine);
    };
    EXPECT_GE(flight(rest + 37).mHit, 0);
    EXPECT_TRUE(flight(rest + 37).mSure);
    EXPECT_GE(flight(rest + 38).mHit, 0);
    EXPECT_FALSE(flight(rest + 38).mSure);
}

TEST(BotThrowArc, AGrenadeReachesAMineOnlyFifteenPixelsAboveOrBelowItsFloor)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 460, 1500, 460, 0));
    const s32 rest = ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched).mPoints.back().mX;
    const auto hit = [&](s32 mineY)
    {
        const ThrowArc::Point mine{rest + 10, mineY};
        return ThrowArc::Throw(in, 512, 460, false, false, ThrowSpot::eCrouched, &mine).mHit;
    };
    EXPECT_EQ(hit(460), 76);
    EXPECT_EQ(hit(445), 76);
    EXPECT_EQ(hit(444), -1);
    EXPECT_EQ(hit(475), 76);
    EXPECT_EQ(hit(476), -1);
}

TEST(BotThrowArc, AGrenadeThatTouchesAMineOnItsWayDownSetsItOffAtOnce)
{
    // The place of the Mines' rock: it comes down on the mine at 3687 on the ninth frame, and does not lie to the end
    NavInput in = AoInput();
    in.mLines.push_back(Line(3400, 2280, 3750, 2280, 0));
    const ThrowArc::Point mine{3687, 2280};
    const ThrowArc::Flight flight = ThrowArc::Throw(in, 3587, 2280, false, false, ThrowSpot::eCrouched, &mine);
    EXPECT_EQ(flight.mHit, 8);
    EXPECT_EQ(flight.mPoints.size(), 9u);
    EXPECT_TRUE(flight.mSure);
}

// Which spots to throw a grenade at a mine from the rule says there are, counted the long way round: every cell and
// throw whose grenade comes to the mine, bar those in the mine's own blast and those the grenade's blast kills
// (85 either side of where it lies, 70 under to 95 over his feet)
struct BlastCount final
{
    s32 mNearAcross = 0;
    s32 mKilled = 0;
    s32 mKept = 0;
    s32 mWrong = 0;
};

static BlastCount CountBlastSpots(const NavInput& in, s32 mineX, s32 mineY)
{
    NavGraph g;
    g.Build(in);
    const std::vector<ThrowSpot> spots = Domain::ThrowSpots(g, mineX, mineY, false);
    const s32 mineNode = g.NodeAt(mineX, mineY, false, 12);
    BlastCount count;
    if (mineNode < 0)
    {
        count.mWrong = 1000000;
        return count;
    }
    const s32 mx = g.Nodes()[mineNode].mX;
    const s32 my = g.Nodes()[mineNode].mY;
    const ThrowArc::Point target{mx, my};
    for (std::size_t i = 0; i < g.Nodes().size(); i++)
    {
        const NavNode& n = g.Nodes()[i];
        const s32 dx = std::abs(n.mX - mx);
        const s32 dy = n.mY - my;
        if (dx == 0 || dx > 450 || std::abs(dy) > 200 || (dx <= 118 && std::abs(dy) <= 55))
        {
            continue;
        }
        for (ThrowSpot::Kind kind : kAllThrows)
        {
            const ThrowArc::Flight flight = ThrowArc::Throw(in, n.mX, n.mY, false, mx < n.mX, kind, &target);
            if (flight.mHit < 0)
            {
                continue;
            }
            const ThrowArc::Point& end = flight.mPoints.back();
            const bool across = std::abs(end.mX - n.mX) <= 85;
            const bool killed = across && n.mY - end.mY >= -70 && n.mY - end.mY <= 95;
            bool present = false;
            for (const ThrowSpot& spot : spots)
            {
                present = present || (spot.mNode == static_cast<s32>(i) && spot.mKind == kind);
            }
            count.mNearAcross += across ? 1 : 0;
            count.mKilled += killed ? 1 : 0;
            count.mKept += killed ? 0 : 1;
            count.mWrong += present == killed ? 1 : 0;
        }
    }
    return count;
}

// (a wall in the way throws the grenade back, so that it lies near him: the only way it comes to a mine more than
// the mine's own blast away and lies within the grenade's)
TEST(BotDomain, NoGrenadeIsThrownFromWhereItsOwnBlastReachesAlongTheFloor)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 300, 640, 300, 0));
    in.mLines.push_back(Line(490, 100, 490, 300, 2));
    const BlastCount count = CountBlastSpots(in, 515, 300);
    EXPECT_GT(count.mKilled, 0);
    EXPECT_GT(count.mKept, 0);
    EXPECT_EQ(count.mWrong, 0);
}

TEST(BotDomain, ALedgeSixtyOverWhereAGrenadeLiesIsInItsBlastAndOneEightyOverIsNot)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 300, 640, 300, 0));
    in.mLines.push_back(Line(300, 240, 420, 240, 0));
    in.mLines.push_back(Line(510, 100, 510, 300, 2));
    const BlastCount near = CountBlastSpots(in, 515, 300);
    EXPECT_GT(near.mKilled, 0);
    EXPECT_EQ(near.mWrong, 0);

    in.mLines[1] = Line(300, 220, 420, 220, 0);
    const BlastCount far = CountBlastSpots(in, 515, 300);
    EXPECT_GT(far.mNearAcross, 0);
    EXPECT_EQ(far.mKilled, 0);
    EXPECT_EQ(far.mWrong, 0);
}

TEST(BotDomain, AFloorEightyUnderWhereAGrenadeLiesIsInItsBlastAndOneAHundredAndTwentyUnderIsNot)
{
    NavInput in = AoInput();
    in.mLines.push_back(Line(0, 200, 640, 200, 0));
    in.mLines.push_back(Line(300, 280, 420, 280, 0));
    in.mLines.push_back(Line(514, 0, 514, 280, 2));
    const BlastCount near = CountBlastSpots(in, 515, 200);
    EXPECT_GT(near.mKilled, 0);
    EXPECT_EQ(near.mWrong, 0);

    in.mLines[1] = Line(300, 320, 420, 320, 0);
    in.mLines[2] = Line(514, 0, 514, 320, 2);
    const BlastCount far = CountBlastSpots(in, 515, 200);
    EXPECT_GT(far.mNearAcross, 0);
    EXPECT_EQ(far.mKilled, 0);
    EXPECT_EQ(far.mWrong, 0);
}

// ---- AO nav ----

static NavInput TwoLifts(s32 secondX1)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(0, 300, 100, 300, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 100, 176, 124));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, secondX1, 300, secondX1 + 76, 324));
    return in;
}

TEST(BotNav, LiftStopsUpToTwoPixelsOffEachOtherAreOneLift)
{
    for (s32 x1 : {100, 101, 102})
    {
        NavGraph g;
        g.Build(TwoLifts(x1));
        const s32 top = g.NodeAt(137, 100, false);
        const s32 bottom = g.NodeAt(137, 300, false);
        ASSERT_GE(top, 0);
        ASSERT_GE(bottom, 0);
        EXPECT_TRUE(HasEdge(g, top, bottom, EdgeKind::eLift)) << x1;
        EXPECT_TRUE(HasEdge(g, bottom, top, EdgeKind::eLift)) << x1;
    }
}

TEST(BotNav, LiftStopsThreePixelsOrMoreOffEachOtherAreNotOneLift)
{
    for (s32 x1 : {103, 130})
    {
        NavGraph g;
        g.Build(TwoLifts(x1));
        const s32 top = g.NodeAt(137, 100, false);
        const s32 bottom = g.NodeAt(137, 300, false);
        ASSERT_GE(top, 0);
        ASSERT_GE(bottom, 0);
        EXPECT_FALSE(HasEdge(g, top, bottom, EdgeKind::eLift)) << x1;
        EXPECT_FALSE(HasEdge(g, bottom, top, EdgeKind::eLift)) << x1;
    }
}

// The cell a fall off the right end of the floor at upY (at 287) comes down in, or -1 for no fall
static s32 FallsTo(bool ae, s32 upY, s32 lowY, s32 lowX2, bool soft = false)
{
    NavInput in = AoInput();
    in.mIsAe = ae;
    in.mLines.push_back(Line(0, upY, 300, upY, 0));
    in.mLines.push_back(Line(0, lowY, lowX2, lowY, 0));
    if (soft)
    {
        in.mFeatures.push_back(Feature(FeatureKind::eSoftLanding, 0, lowY - 30, 2000, lowY + 20));
        in.mFeatures.back().mAnyScale = true;
    }
    NavGraph g;
    g.Build(in);
    const s32 from = g.NodeAt(287, upY, false);
    for (const NavEdge& e : g.Edges())
    {
        if (e.mFrom == from && e.mKind == EdgeKind::eFall)
        {
            return g.Nodes()[e.mTo].mX;
        }
    }
    return -1;
}

TEST(BotNav, AoWalkedOffALedgeAHundredAndFiftyHighLandsOneCellFurtherOut)
{
    // Not to the cell by the ledge's end as AE's does: he keeps the walk's speed down (150 is not high enough)
    EXPECT_EQ(FallsTo(true, 100, 251, 2000), 312);
    EXPECT_EQ(FallsTo(false, 100, 250, 2000), 315);
    EXPECT_EQ(FallsTo(false, 100, 251, 2000), 340);
    EXPECT_EQ(FallsTo(false, 100, 279, 2000), 340);
}

TEST(BotNav, AoWalkedOffAHighLedgeLandsFurtherOutTheHigherItIs)
{
    // 300 down onto a soft landing: five frames and the rest at 20 pixels a frame, 2.77 pixels each
    EXPECT_EQ(FallsTo(false, 10, 310, 2000, true), 365);
    // AE comes straight down
    EXPECT_EQ(FallsTo(true, 10, 310, 2000, true), 312);
}

TEST(BotNav, AoWalkedOffALedgeAScreenUpLandsFurtherOutForTheChangeOfScreen)
{
    // The same 400 down but over a change of screen (five frames more) and the fall is a little longer
    EXPECT_EQ(FallsTo(false, 400, 800, 2000, true), 365);
}

TEST(BotNav, AoWalkedOffALedgeThereIsNoWayDownWhereThereIsNoFloorOutThere)
{
    // The floor below ends at 345, which a fall of 160 reaches (340) and at 330 it does not
    EXPECT_EQ(FallsTo(false, 100, 260, 345), 340);
    EXPECT_EQ(FallsTo(false, 100, 260, 330), -1);
    // AE comes down by the ledge's end, where there is floor
    EXPECT_EQ(FallsTo(true, 100, 260, 330), 312);
}

// ---- AO saw ----

static NavInput SawRoom(s32 exitX)
{
    NavInput in;
    // The lever's ledge, joined to the floor under it by a hoist, and the saw over the floor
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(0, 200, 600, 200, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eHoist, 100, 100, 124, 195));
    NavFeature saw = Feature(FeatureKind::eSaw, 300, 170, 350, 200);
    saw.mSwitch = 9;
    in.mFeatures.push_back(saw);
    NavFeature exit = Feature(FeatureKind::eDoor, exitX, 80, exitX + 24, 104);
    exit.mDestPath = 2;
    exit.mDestCamera = 1;
    in.mFeatures.push_back(exit);
    return in;
}

static World SawWorld()
{
    World world;
    Entity lever = MakeEntity(EntityKind::eLever, 1001, 20, 80, 94, 104);
    lever.mSwitch = 9;
    lever.mOp = 2;
    world.mEntities.push_back(lever);
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 2001, 300, 170, 324, 200));
    return world;
}

static bool ExitNeedsSligDead(const Domain& domain, u32 sligId)
{
    bool any = false;
    for (const Goap::Action& a : domain.mActions)
    {
        if (domain.mSteps[a.mTag].mKind != StepKind::eExit)
        {
            continue;
        }
        any = true;
        bool needs = false;
        for (const Goap::Fact& f : a.mPre)
        {
            needs = needs || (f.mKey == Key::Mud(sligId) && f.mValue == 2);
        }
        if (!needs)
        {
            return false;
        }
    }
    return any;
}

TEST(BotDomain, ASligIsSawnWithTheLeverThatBringsTheSawDown)
{
    NavInput in = SawRoom(550);
    // (exit on the floor under the saw)
    in.mFeatures.back().mY1 = 180;
    in.mFeatures.back().mY2 = 204;
    NavGraph g;
    g.Build(in);
    const Domain domain = Domain::Build(g, SawWorld(), g.NodeAt(12, 100, false), nullptr);
    bool drop = false;
    for (const Goap::Action& a : domain.mActions)
    {
        const DomainStep& s = domain.mSteps[a.mTag];
        if (s.mKind == StepKind::eSligDrop)
        {
            drop = true;
            EXPECT_EQ(s.mEntity, 1001u);
            EXPECT_EQ(s.mOther, 2001u);
            ASSERT_EQ(a.mEffects.size(), 1u);
            EXPECT_EQ(a.mEffects[0].mKey, Key::Mud(2001));
        }
    }
    EXPECT_TRUE(drop) << domain.Text();
}

TEST(BotDomain, TheWayOutOnTheFloorOfTheSawNeedsTheSligDeadWhenTheWayFromTheLeverGoesAlongIt)
{
    NavInput in = SawRoom(550);
    in.mFeatures.back().mY1 = 180;
    in.mFeatures.back().mY2 = 204;
    NavGraph g;
    g.Build(in);
    const Domain domain = Domain::Build(g, SawWorld(), g.NodeAt(12, 100, false), nullptr);
    EXPECT_TRUE(ExitNeedsSligDead(domain, 2001)) << domain.Text();
}

TEST(BotDomain, TheWayOutOnTheLeversLedgeNeedsNoSligDeadForTheSaw)
{
    NavGraph g;
    g.Build(SawRoom(150));
    const Domain domain = Domain::Build(g, SawWorld(), g.NodeAt(12, 100, false), nullptr);
    bool exit = false;
    for (const Goap::Action& a : domain.mActions)
    {
        exit = exit || domain.mSteps[a.mTag].mKind == StepKind::eExit;
    }
    ASSERT_TRUE(exit) << domain.Text();
    EXPECT_FALSE(ExitNeedsSligDead(domain, 2001));
}

// ---- A fart that is flown (Feeco Depot) ----

// A room 600 wide and 200 high with a wall down its middle that stops 60 short of the ceiling
static NavInput FartRoom(bool wallToCeiling)
{
    NavInput in;
    in.mLines.push_back(Line(0, 300, 600, 300, 0));
    in.mLines.push_back(Line(0, 100, 600, 100, 3));
    in.mLines.push_back(Line(0, 100, 0, 300, 1));
    in.mLines.push_back(Line(600, 100, 600, 300, 2));
    in.mLines.push_back(Line(300, wallToCeiling ? 100 : 160, 300, 300, 1));
    return in;
}

static bool WayCrosses(FartFlight::Point from, const std::vector<FartFlight::Point>& corners, const NavLine& line)
{
    return FartFlight::Through(from, corners, line);
}

TEST(BotFart, WithNothingBetweenTheWayIsOneLegStraightThere)
{
    std::vector<FartFlight::Point> corners;
    s32 length = 0;
    ASSERT_TRUE(FartFlight::Route(FartRoom(false), false, {50, 250}, {250, 200}, 2000, corners, &length));
    ASSERT_EQ(corners.size(), 1u);
    EXPECT_EQ(corners[0].mX, 250);
    EXPECT_EQ(corners[0].mY, 200);
    EXPECT_EQ(length, 250);
}

TEST(BotFart, AWallInTheWayIsFlownOverWhereItStopsShortOfTheCeiling)
{
    const NavInput in = FartRoom(false);
    std::vector<FartFlight::Point> corners;
    s32 length = 0;
    ASSERT_TRUE(FartFlight::Route(in, false, {50, 250}, {550, 250}, 2000, corners, &length));
    EXPECT_GE(corners.size(), 3u);
    EXPECT_EQ(corners.back().mX, 550);
    EXPECT_FALSE(WayCrosses({50, 250}, corners, in.mLines[4]));
    // (up over the wall's top at 160 and down again: more than straight across)
    EXPECT_GT(length, 500 + 2 * 80);
    bool over = false;
    for (const FartFlight::Point& p : corners)
    {
        over = over || p.mY < 160;
    }
    EXPECT_TRUE(over);
}

TEST(BotFart, AWallFromFloorToCeilingLeavesNoWayAndNeitherDoesAWayTooLong)
{
    std::vector<FartFlight::Point> corners;
    EXPECT_FALSE(FartFlight::Route(FartRoom(true), false, {50, 250}, {550, 250}, 2000, corners));
    EXPECT_FALSE(FartFlight::Route(FartRoom(false), false, {50, 250}, {550, 250}, 600, corners));
    // (a floor stops it up and down but not along: under the room's floor there is no getting in)
    EXPECT_FALSE(FartFlight::Route(FartRoom(false), false, {50, 250}, {50, 350}, 2000, corners));
}

TEST(BotFart, AWallOfTheOtherScaleIsNothingToIt)
{
    NavInput in = FartRoom(true);
    // (the middle wall as a half scale one: types 5 and 6)
    in.mLines[4].mType = 5;
    std::vector<FartFlight::Point> corners;
    EXPECT_TRUE(FartFlight::Route(in, false, {50, 250}, {550, 250}, 2000, corners));
    EXPECT_EQ(corners.size(), 1u);
}

TEST(BotFart, AShutGateIsInTheWayAndAnOpenOneIsNot)
{
    NavInput in = FartRoom(false);
    // A gate in the gap over the wall, on switch 40, shut until it is on
    NavFeature gate = Feature(FeatureKind::eBarrier, 288, 140, 312, 160);
    gate.mSwitch = 40;
    gate.mStartClosed = true;
    in.mFeatures.push_back(gate);
    World world;
    std::vector<s32> which;
    const std::vector<NavLine> all = FartFlight::Gates(in, false, nullptr, &which);
    ASSERT_EQ(all.size(), 1u);
    EXPECT_EQ(which[0], 0);
    std::vector<FartFlight::Point> corners;
    EXPECT_FALSE(FartFlight::Route(in, false, {50, 250}, {550, 250}, 2000, corners, nullptr, &all));
    // Shut in the world as it is: the same. Open: no line for it.
    EXPECT_EQ(FartFlight::Gates(in, false, &world).size(), 1u);
    world.mSwitches[40] = 1;
    const std::vector<NavLine> open = FartFlight::Gates(in, false, &world);
    EXPECT_TRUE(open.empty());
    EXPECT_TRUE(FartFlight::Route(in, false, {50, 250}, {550, 250}, 2000, corners, nullptr, &open));
    // (and the way through says which gate it goes through)
    EXPECT_TRUE(FartFlight::Through({50, 250}, corners, all[0]));
}

// The fart as EvilFart.cpp moves it: a fifth of a pixel a frame more each frame a way is held, up to 4, and a
// fifth less each frame it is not
static void FlyFart(s32& x100, s32& y100, s32& vx, s32& vy, u32 held)
{
    const auto axis = [](s32& v, bool more, bool less)
    {
        if (more && v < 400)
        {
            v += 20;
        }
        if (less && v > -400)
        {
            v -= 20;
        }
        if (!more && !less)
        {
            v += v > 0 ? -20 : (v < 0 ? 20 : 0);
        }
    };
    axis(vx, (held & Btn::eRight) != 0, (held & Btn::eLeft) != 0);
    axis(vy, (held & Btn::eDown) != 0, (held & Btn::eUp) != 0);
    x100 += vx;
    y100 += vy;
}

TEST(BotFart, SteeredItStopsWhereItIsToWithoutGoingPast)
{
    s32 x = 100 * 100;
    s32 y = 500 * 100;
    s32 vx = 0;
    s32 vy = 0;
    s32 furthest = x;
    s32 frames = 0;
    for (; frames < 400; frames++)
    {
        const u32 held = FartFlight::Steer(x, y, vx, vy, 600, 380);
        if (held == 0 && vx == 0 && vy == 0)
        {
            break;
        }
        FlyFart(x, y, vx, vy, held);
        furthest = std::max(furthest, x);
    }
    // 500 across at up to 4 a frame, with the speeding up and the slowing down: some 150 frames
    EXPECT_LT(frames, 170);
    EXPECT_NEAR(x / 100, 600, 1);
    EXPECT_NEAR(y / 100, 380, 1);
    EXPECT_LE(furthest, 601 * 100);
}

TEST(BotFart, AtFullSpeedItCoastsThirtyEightPixels)
{
    EXPECT_EQ(FartFlight::Coast(400), 3800);
    EXPECT_EQ(FartFlight::Coast(-400), 3800);
    EXPECT_EQ(FartFlight::Coast(20), 0);
    s32 x = 0;
    s32 y = 0;
    s32 vx = 400;
    s32 vy = 0;
    while (vx != 0)
    {
        FlyFart(x, y, vx, vy, 0);
    }
    EXPECT_EQ(x, 3800);
}

TEST(BotFart, TheBlastReachesSixtyToEachSideSixtyUpAndThirtyDown)
{
    const FartFlight::Point at{500, 400};
    EXPECT_TRUE(FartFlight::Hits(at, 550, 390, 560, 410));
    EXPECT_FALSE(FartFlight::Hits(at, 561, 390, 580, 410));
    EXPECT_TRUE(FartFlight::Hits(at, 490, 330, 510, 340));
    EXPECT_FALSE(FartFlight::Hits(at, 490, 320, 510, 339));
    EXPECT_TRUE(FartFlight::Hits(at, 490, 430, 510, 450));
    EXPECT_FALSE(FartFlight::Hits(at, 490, 431, 510, 450));
}

// A floor with a Brew machine at its left, a mine in the way and the way out beyond it; a wall between them that
// leaves a gap over it, and a gate in that gap if asked for
static NavInput BrewCorridor(bool gate)
{
    NavInput in;
    in.mLines.push_back(Line(0, 300, 800, 300, 0));
    in.mLines.push_back(Line(0, 100, 800, 100, 3));
    in.mLines.push_back(Line(0, 100, 0, 300, 1));
    in.mLines.push_back(Line(800, 100, 800, 300, 2));
    // (four in a row: too many to hop)
    for (s32 i = 0; i < 4; i++)
    {
        NavFeature mine = Feature(FeatureKind::eHazard, 500 + i * 25, 280, 524 + i * 25, 304);
        mine.mSwitch = World::HazardSwitch(2000 + i);
        in.mFeatures.push_back(mine);
    }
    NavFeature exit = Feature(FeatureKind::eDoor, 700, 280, 724, 304);
    exit.mDestPath = 2;
    exit.mDestCamera = 1;
    in.mFeatures.push_back(exit);
    if (gate)
    {
        NavFeature g = Feature(FeatureKind::eBarrier, 388, 200, 412, 300);
        g.mSwitch = 40;
        g.mStartClosed = true;
        in.mFeatures.push_back(g);
        // (and nothing gets over it)
        in.mLines.push_back(Line(400, 100, 400, 220, 1));
    }
    return in;
}

static World BrewWorld(bool orb)
{
    World world;
    for (u32 i = 0; i < 4; i++)
    {
        Entity mine = MakeEntity(EntityKind::eMine, 2000 + i, 500 + i * 25, 280, 524 + i * 25, 304);
        mine.mSwitch = World::HazardSwitch(mine.mId);
        world.mEntities.push_back(mine);
    }
    Entity brew = MakeEntity(EntityKind::eBrew, 3000, 100, 266, 124, 302);
    brew.mCamera = 1;
    world.mEntities.push_back(brew);
    if (orb)
    {
        Entity o = MakeEntity(EntityKind::eSecurityOrb, 3001, 200, 150, 224, 174);
        o.mCamera = 1;
        world.mEntities.push_back(o);
    }
    return world;
}

static const Goap::Action* FartAction(const Domain& d, u32 target)
{
    for (const Goap::Action& a : d.mActions)
    {
        const DomainStep& s = d.mSteps[a.mTag];
        // (flown from a machine: one left by the mine names no machine)
        if (s.mKind == StepKind::eFartBomb && s.mEntity == target && s.mOther != 0)
        {
            return &a;
        }
    }
    return nullptr;
}

TEST(BotFart, AMineInTheWayIsBlownUpWithAFartFlownFromTheBrewMachine)
{
    NavGraph g;
    g.Build(BrewCorridor(false));
    World world = BrewWorld(false);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 300, false), nullptr);
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss << "\n" << domain.Text();
    bool flown = false;
    for (s32 step : plan.mSteps)
    {
        const DomainStep& s = domain.mSteps[domain.mActions[step].mTag];
        flown = flown || (s.mKind == StepKind::eFartBomb && s.mEntity >= 2000 && s.mEntity <= 2003 && s.mOther == 3000);
    }
    EXPECT_TRUE(flown) << domain.Text();
}

TEST(BotFart, WithAnOrbOnTheMachinesScreenNoFartIsFlownFromIt)
{
    NavGraph g;
    g.Build(BrewCorridor(false));
    World world = BrewWorld(true);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 300, false), nullptr);
    EXPECT_EQ(FartAction(domain, 2000), nullptr);
}

TEST(BotFart, AFartThatHasToGoThroughAGateWaitsForTheGatesSwitch)
{
    NavGraph g;
    g.Build(BrewCorridor(true));
    World world = BrewWorld(false);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 300, false), nullptr);
    const Goap::Action* fart = FartAction(domain, 2000);
    ASSERT_NE(fart, nullptr) << domain.Text();
    const Goap::Fact open{Key::Switch(40), 1};
    EXPECT_NE(std::find(fart->mPre.begin(), fart->mPre.end(), open), fart->mPre.end());
    // (with no gate there it waits for nothing but being at the machine with no Brew in him)
    NavGraph plain;
    plain.Build(BrewCorridor(false));
    const Domain free = Domain::Build(plain, world, plain.NodeAt(12, 300, false), nullptr);
    const Goap::Action* direct = FartAction(free, 2000);
    ASSERT_NE(direct, nullptr);
    EXPECT_EQ(std::find(direct->mPre.begin(), direct->mPre.end(), open), direct->mPre.end());
}

TEST(BotFart, AFlyingSligsTrackIsTheLinesJoinedEndToEndThatGoByIt)
{
    NavInput in;
    // Two tracks: a low one of three lines and a high one of one
    in.mLines.push_back(Line(100, 500, 300, 520, 8));
    in.mLines.push_back(Line(300, 520, 600, 510, 8));
    in.mLines.push_back(Line(600, 510, 900, 515, 8));
    in.mLines.push_back(Line(400, 420, 800, 430, 8));
    s32 x1 = 0;
    s32 y1 = 0;
    s32 x2 = 0;
    s32 y2 = 0;
    std::vector<std::pair<s32, s32>> ends;
    std::vector<NavLine> lines;
    ASSERT_TRUE(Domain::FlierTrack(in, 450, 510, x1, y1, x2, y2, &ends, &lines));
    EXPECT_EQ(x1, 100);
    EXPECT_EQ(x2, 900);
    EXPECT_EQ(y1, 500);
    EXPECT_EQ(y2, 520);
    EXPECT_EQ(lines.size(), 3u);
    ASSERT_EQ(ends.size(), 2u);
    EXPECT_EQ(std::min(ends[0].first, ends[1].first), 100);
    EXPECT_EQ(std::max(ends[0].first, ends[1].first), 900);
    ASSERT_TRUE(Domain::FlierTrack(in, 600, 425, x1, y1, x2, y2, &ends));
    EXPECT_EQ(x1, 400);
    EXPECT_EQ(x2, 800);
    // (no track at all: none)
    NavInput bare;
    bare.mLines.push_back(Line(0, 300, 800, 300, 0));
    EXPECT_FALSE(Domain::FlierTrack(bare, 100, 100, x1, y1, x2, y2));
}

TEST(BotFart, NothingIsCrossedIntoAFlyingSligsStretchUntilItIsGone)
{
    NavInput in;
    in.mLines.push_back(Line(0, 300, 800, 300, 0));
    NavFeature zone = Feature(FeatureKind::eDanger, 400, 200, 800, 340);
    zone.mSwitch = World::HazardSwitch(5000);
    zone.mA = 1;
    in.mFeatures.push_back(zone);
    NavGraph g;
    g.Build(in);
    const s32 before = g.NodeAt(387, 300, false);
    const s32 inside = g.NodeAt(412, 300, false);
    const s32 further = g.NodeAt(437, 300, false);
    ASSERT_GE(before, 0);
    ASSERT_GE(inside, 0);
    const NavEdge* in1 = FindEdge(g, before, inside, EdgeKind::eWalk);
    ASSERT_NE(in1, nullptr);
    s32 sw = 0;
    s32 value = 0;
    EXPECT_TRUE(g.Condition(*in1, &sw, &value));
    EXPECT_EQ(sw, World::HazardSwitch(5000));
    EXPECT_EQ(value, 1);
    // (out of it, and along inside it, wait for nothing)
    EXPECT_FALSE(g.Condition(*FindEdge(g, inside, before, EdgeKind::eWalk), &sw, &value));
    EXPECT_FALSE(g.Condition(*FindEdge(g, inside, further, EdgeKind::eWalk), &sw, &value));
    // The world gives the switch as on once the Slig is dead, and all along for one that is not to be got rid of
    World world;
    Entity flier = MakeEntity(EntityKind::eFlier, 5000, 600, 240, 624, 264);
    flier.mSwitch = World::HazardSwitch(5000);
    flier.mInputs[0] = 1;
    world.mEntities.push_back(flier);
    EXPECT_EQ(world.Switch(flier.mSwitch), 0);
    world.mFleechKilled.insert(5000);
    EXPECT_EQ(world.Switch(flier.mSwitch), 1);
    world.mFleechKilled.clear();
    world.mEntities[0].mInputs[0] = 0;
    EXPECT_EQ(world.Switch(flier.mSwitch), 1);
}

TEST(BotFart, TheWayIntoTheStretchOfAFlyingSligThatIsLeftAliveIsADearOne)
{
    NavInput in;
    in.mLines.push_back(Line(0, 300, 800, 300, 0));
    NavGraph plain;
    plain.Build(in);
    NavFeature zone = Feature(FeatureKind::eDanger, 400, 200, 800, 340);
    zone.mSwitch = World::HazardSwitch(5000);
    zone.mA = 0;
    in.mFeatures.push_back(zone);
    NavGraph g;
    g.Build(in);
    const NavEdge* dear = FindEdge(g, g.NodeAt(387, 300, false), g.NodeAt(412, 300, false), EdgeKind::eWalk);
    const NavEdge* was = FindEdge(plain, plain.NodeAt(387, 300, false), plain.NodeAt(412, 300, false), EdgeKind::eWalk);
    ASSERT_NE(dear, nullptr);
    ASSERT_NE(was, nullptr);
    EXPECT_EQ(dear->mCost, was->mCost + 1500);
}

TEST(BotFart, RunningJumpsThatComeDownInTheNextOnesRunUpAreAlsoOneEdge)
{
    // Three mines, each a running jump apart
    NavInput in;
    in.mLines.push_back(Line(0, 300, 1200, 300, 0));
    for (s32 x : {300, 325, 475, 650})
    {
        in.mFeatures.push_back(Feature(FeatureKind::eHazard, x, 280, x + 24, 304));
    }
    NavGraph g;
    g.Build(in);
    s32 singles = 0;
    s32 doubles = 0;
    s32 triples = 0;
    for (const NavEdge& e : g.Edges())
    {
        if (e.mKind != EdgeKind::eRunJump || e.mLeft)
        {
            continue;
        }
        singles += e.mX2 == 0 ? 1 : 0;
        doubles += e.mX2 != 0 && e.mX3 == 0 ? 1 : 0;
        triples += e.mX3 != 0 ? 1 : 0;
        if (e.mX2 != 0)
        {
            // (each take-off further on than the one before, and the edge ends where its last jump does)
            EXPECT_GT(e.mX2, e.mX);
            EXPECT_GT(g.Nodes()[e.mTo].mX, e.mX2);
        }
        if (e.mX3 != 0)
        {
            EXPECT_GT(e.mX3, e.mX2);
            EXPECT_GT(g.Nodes()[e.mTo].mX, e.mX3);
        }
    }
    EXPECT_GE(singles, 3);
    EXPECT_GE(doubles, 1);
    EXPECT_GE(triples, 1);
}

TEST(BotFart, AFartIsLeftByAMineFromTwoCellsOffItWithTheBrewHeHasInHim)
{
    NavGraph g;
    g.Build(BrewCorridor(false));
    // (an orb on the machine's screen: nothing is flown, so a fart left by the mines is what there is)
    World world = BrewWorld(true);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 300, false), nullptr);
    const Goap::Action* left = nullptr;
    for (const Goap::Action& a : domain.mActions)
    {
        const DomainStep& s = domain.mSteps[a.mTag];
        if (s.mKind == StepKind::eFartBomb && s.mEntity == 2000 && s.mOther == 0)
        {
            left = &a;
            // (from the cell two off the first mine, on the side that is floor: the other side is the next mine)
            EXPECT_EQ(g.Nodes()[s.mEdge].mX, 462);
        }
    }
    ASSERT_NE(left, nullptr) << domain.Text();
    const Goap::Fact brew{Key::kBrew, 1};
    EXPECT_NE(std::find(left->mPre.begin(), left->mPre.end(), brew), left->mPre.end());
    const Goap::Fact spent{Key::kBrew, 0};
    EXPECT_NE(std::find(left->mEffects.begin(), left->mEffects.end(), spent), left->mEffects.end());
    // The way out is then: drink, leave farts, go
    const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
    ASSERT_TRUE(plan.mFound) << plan.mMiss << "\n" << domain.Text();
    EXPECT_EQ(domain.mSteps[domain.mActions[plan.mSteps.front()].mTag].mKind, StepKind::eDrink);
}

TEST(BotFart, AHopDownOntoAMinesCellIsThereOnlyWhereThereIsABrewMachineAndWaitsForTheMine)
{
    // A ledge forty over a floor that begins a cell out from it, with a mine where the hop comes down: three cells
    // out (as measured in Feeco's Terminal 2, off 1187,420: he ends at 1262,460)
    NavInput in;
    in.mLines.push_back(Line(1175, 420, 1200, 420, 0));
    in.mLines.push_back(Line(1225, 460, 1400, 460, 0));
    NavFeature mine = Feature(FeatureKind::eHazard, 1250, 440, 1274, 464);
    mine.mSwitch = World::HazardSwitch(7005);
    in.mFeatures.push_back(mine);
    NavGraph plain;
    plain.Build(in);
    EXPECT_EQ(FindEdge(plain, plain.NodeAt(1187, 420, false), plain.NodeAt(1262, 460, false), EdgeKind::eHop), nullptr);
    in.mBrew = true;
    NavGraph g;
    g.Build(in);
    const NavEdge* hop = FindEdge(g, g.NodeAt(1187, 420, false), g.NodeAt(1262, 460, false), EdgeKind::eHop);
    ASSERT_NE(hop, nullptr);
    s32 sw = 0;
    s32 value = 0;
    EXPECT_TRUE(g.Condition(*hop, &sw, &value));
    EXPECT_EQ(sw, World::HazardSwitch(7005));
}

// A lobby (2) with its own door out of the level and one to a path (5) that leaves it too, both of which can be
// shut; and a junction (9) that leads to the paths 3 and 4, which leave the level
static LevelGraph LobbyLevel()
{
    std::vector<PathSummary> paths(5);
    paths[0].mPath = 2;
    paths[0].mExits = {{12, 11}, {5, 5}, {5, 9}};
    paths[1].mPath = 5;
    paths[1].mExits = {{9, 16}, {5, 2}};
    paths[2].mPath = 9;
    paths[2].mExits = {{5, 3}, {5, 4}, {5, 2}};
    paths[3].mPath = 3;
    paths[3].mExits = {{6, 1}, {5, 9}};
    paths[4].mPath = 4;
    paths[4].mExits = {{8, 1}, {5, 9}};
    LevelGraph level;
    level.Build(5, paths);
    return level;
}

TEST(BotLevel, APathWhoseWayOutOfTheLevelIsShutIsMeasuredByItsOtherWays)
{
    LevelGraph level = LobbyLevel();
    EXPECT_EQ(level.Distance(2), 0);
    EXPECT_EQ(level.Distance(9), 1);
    // Its own door out is not to be had: one path away, through 5
    EXPECT_TRUE(level.Shut(2));
    EXPECT_EQ(level.Distance(2), 1);
    // Nor is the door to 5: two away, through the junction
    EXPECT_TRUE(level.Shut(2));
    EXPECT_EQ(level.Distance(2), 2);
    EXPECT_EQ(level.Distance(9), 1);
    EXPECT_EQ(level.Distance(5), 0);
}

// A hub (2) whose door out of the level wants a switch from each of the paths around it: 10 sets its own, 9 leads on
// to 14 which sets it, 7 sets a switch nothing wants, 1 is where he came from, and 12 is a side room of 10
static LevelGraph HubLevel()
{
    std::vector<PathSummary> paths(6);
    paths[5].mPath = 12;
    paths[5].mExits = {{6, 10}};
    // (its door back wants what 7 sets: 7 is nearer the level's end, he is past that door when he is there)
    paths[5].mWants = {{6, 10, 77}};
    paths[0].mPath = 2;
    paths[0].mExits = {{6, 1}, {6, 10}, {6, 9}, {6, 7}, {13, 11}};
    paths[0].mWants = {{13, 11, 116}, {13, 11, 218}};
    paths[1].mPath = 10;
    paths[1].mExits = {{6, 2}, {6, 12}};
    paths[1].mSets = {116, 30};
    // (its own door back wants the same switch)
    paths[1].mWants = {{6, 2, 116}};
    paths[2].mPath = 9;
    paths[2].mExits = {{6, 14}, {6, 2}};
    paths[2].mSets = {208};
    // (a door further in, away from the level's end, wants one of the hub's too: that is not a way on)
    paths[2].mWants = {{6, 14, 116}};
    paths[3].mPath = 14;
    paths[3].mExits = {{6, 2}};
    paths[3].mSets = {218};
    paths[3].mWants = {{6, 2, 208}};
    paths[4].mPath = 7;
    paths[4].mExits = {{6, 2}};
    paths[4].mSets = {77};
    LevelGraph level;
    level.Build(6, paths);
    return level;
}

TEST(BotLevel, ASwitchADoorOfAnotherPathWantsIsWhatASidePathIsFor)
{
    const LevelGraph level = HubLevel();
    // In 10: the hub wants 116 and 218, 14's door wants 208 (its own door's want is no one else's)
    EXPECT_EQ(level.Wanted(10), (std::vector<s32>{116, 208, 218}));
    // (in the hub nothing is: its own door's are not "elsewhere", and the doors of the paths around it are
    // farther from the level's end than it is)
    EXPECT_TRUE(level.Wanted(2).empty());
    EXPECT_EQ(level.Wanted(12), (std::vector<s32>{116, 208, 218}));
    EXPECT_EQ(level.Errands(10), (std::vector<s32>{116}));
    EXPECT_EQ(level.Errands(9), (std::vector<s32>{208}));
    EXPECT_EQ(level.Errands(14), (std::vector<s32>{218}));
    EXPECT_TRUE(level.Errands(7).empty());
    EXPECT_TRUE(level.Errands(2).empty());
}

TEST(BotLevel, FromAHubTheWaysToWhatItsDoorWantsAreTheWaysOut)
{
    const LevelGraph level = HubLevel();
    std::set<std::pair<s32, s32>> done;
    const auto open = [&done](s32 path, s32 sw)
    {
        return done.count({path, sw}) == 0;
    };
    // 10 sets one, 9 sets one for 14 and leads to 14; not 7 (nothing wanted there) and not back to 1
    EXPECT_EQ(level.ErrandExits(2, open), (std::set<s32>{9, 10}));
    done.insert({10, 116});
    EXPECT_EQ(level.ErrandExits(2, open), (std::set<s32>{9}));
    // In 9 with its own done: on to 14, not back through the hub whose door it is for
    done.insert({9, 208});
    EXPECT_EQ(level.ErrandExits(2, open), (std::set<s32>{9}));
    EXPECT_EQ(level.ErrandExits(9, open), (std::set<s32>{14}));
    done.insert({14, 218});
    EXPECT_TRUE(level.ErrandExits(2, open).empty());
    EXPECT_TRUE(level.ErrandExits(9, open).empty());
    // From the path before the hub nothing is seen through it
    done.clear();
    EXPECT_TRUE(level.ErrandExits(1, open).empty());
    EXPECT_TRUE(level.ErrandExits(7, open).empty());
    // What is to be set where he is is not a reason to go out and come back in
    EXPECT_TRUE(level.ErrandExits(10, open).empty());
}

TEST(BotLevel, ADoorBackToALevelHeHasBeenInWantsNothing)
{
    std::vector<PathSummary> paths(2);
    paths[0].mPath = 2;
    paths[0].mExits = {{13, 11}, {6, 10}, {4, 3}};
    paths[0].mWants = {{4, 3, 50}};
    paths[1].mPath = 10;
    paths[1].mExits = {{6, 2}};
    paths[1].mSets = {50};
    LevelGraph level;
    level.Build(6, paths);
    EXPECT_EQ(level.Errands(10), (std::vector<s32>{50}));
    level.Build(6, paths, {4});
    EXPECT_TRUE(level.Errands(10).empty());
}

TEST(BotLevel, APathWithNoOtherWayStaysAsItWas)
{
    LevelGraph level = LobbyLevel();
    EXPECT_TRUE(level.Shut(2));
    EXPECT_TRUE(level.Shut(2));
    // From the junction the ways on are to 3 and 4: with them shut there is only the way back to the lobby, which
    // leads nowhere but here. Nothing changes.
    EXPECT_FALSE(level.Shut(9));
    EXPECT_EQ(level.Distance(9), 1);
    EXPECT_EQ(level.Distance(2), 2);
    // (3's own way out shut, it is two from the end: back through the junction to 4)
    EXPECT_TRUE(level.Shut(3));
    EXPECT_EQ(level.Distance(3), 2);
    // And one the level does not know is not shut
    EXPECT_FALSE(level.Shut(77));
}

// A lever ledge with a Slig spawner's floor under it on the same screen; that floor goes on through a gate (switch
// 20) and off its end down to a lower floor with a Slig on it. One Slig lock before the gate (it opens the gate),
// one on the lower floor.
static NavInput SligLockRoom()
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(150, 200, 700, 200, 0));
    in.mLines.push_back(Line(600, 300, 1000, 300, 0));
    NavFeature gate = Feature(FeatureKind::eBarrier, 400, 180, 424, 204);
    gate.mSwitch = 20;
    gate.mStartClosed = true;
    in.mFeatures.push_back(gate);
    return in;
}

static World SligLockWorld()
{
    World world;
    Entity lever = MakeEntity(EntityKind::eLever, 1, 25, 80, 99, 104);
    lever.mSwitch = 7;
    lever.mOp = 2;
    world.mEntities.push_back(lever);
    Entity spawned = MakeEntity(EntityKind::eSlig, 2, 212, 180, 236, 204);
    spawned.mSwitch = 7;
    world.mEntities.push_back(spawned);
    Entity first = MakeEntity(EntityKind::eSligLock, 3, 300, 150, 350, 204);
    first.mSwitch = 20;
    world.mEntities.push_back(first);
    Entity second = MakeEntity(EntityKind::eSligLock, 4, 900, 250, 975, 304);
    second.mSwitch = 21;
    world.mEntities.push_back(second);
    Entity guard = MakeEntity(EntityKind::eSlig, 5, 800, 280, 824, 304);
    world.mEntities.push_back(guard);
    world.mLockCodes[3] = {5, 6, 8};
    world.mLockCodes[4] = {6, 5, 8};
    return world;
}

static const Goap::Action* SligLockAction(const Domain& d, u32 lock)
{
    for (const Goap::Action& a : d.mActions)
    {
        if (a.mName.find("Open SligLock#" + std::to_string(lock) + " ") == 0)
        {
            return &a;
        }
    }
    return nullptr;
}

static bool HasFact(const std::vector<Goap::Fact>& facts, u32 key, s32 value)
{
    return std::find(facts.begin(), facts.end(), Goap::Fact{key, value}) != facts.end();
}

TEST(BotSligLock, ASligsWayGoesAlongFloorsAndOffTheirEndsAndSaysWhichGatesItNeeds)
{
    NavGraph g;
    g.Build(SligLockRoom());
    const World world = SligLockWorld();
    const s32 start = Domain::SligNode(g, world.mEntities[1]);
    ASSERT_EQ(start, g.NodeAt(212, 200, false));
    const auto at = [&](std::size_t lock)
    {
        return [&g, &world, lock](s32 n)
        {
            return Domain::AtSligLock(g, world.mEntities[lock], n);
        };
    };
    // (a Slig the map puts in the air over a floor is not on that floor; one a spawner makes comes down to it)
    Entity boxed = MakeEntity(EntityKind::eSlig, 9, 212, 120, 236, 144);
    EXPECT_LT(Domain::SligNode(g, boxed), 0);
    boxed.mSwitch = 7;
    EXPECT_EQ(Domain::SligNode(g, boxed), start);
    // To the first lock: along its floor, nothing shut on the way
    std::vector<Goap::Fact> gates;
    const std::vector<s32> near = Domain::SligWay(g, start, at(2), &gates, nullptr);
    ASSERT_FALSE(near.empty());
    EXPECT_TRUE(gates.empty());
    EXPECT_TRUE(Domain::AtSligLock(g, world.mEntities[2], g.Edges()[near.back()].mTo));
    // To the second: through the gate and off the end of the floor
    const std::vector<s32> far = Domain::SligWay(g, start, at(3), &gates, nullptr);
    ASSERT_FALSE(far.empty());
    ASSERT_EQ(gates.size(), 1u);
    EXPECT_TRUE(HasFact(gates, Key::Switch(20), 1));
    bool drops = false;
    for (s32 idx : far)
    {
        drops = drops || g.Edges()[idx].mKind == EdgeKind::eFall;
    }
    EXPECT_TRUE(drops);
    // And none back up: it can't climb
    const s32 below = g.NodeAt(812, 300, false);
    ASSERT_GE(below, 0);
    EXPECT_TRUE(Domain::SligWay(g, below, at(2), &gates, nullptr).empty());
    // (where it is already there, there is no way to go)
    EXPECT_TRUE(Domain::SligWay(g, g.NodeAt(312, 200, false), at(2), nullptr, nullptr).empty());
}

TEST(BotSligLock, AsThingsStandOnlyAnOpenGateIsGoneThrough)
{
    NavGraph g;
    g.Build(SligLockRoom());
    World world = SligLockWorld();
    const s32 start = Domain::SligNode(g, world.mEntities[1]);
    const auto second = [&](s32 n)
    {
        return Domain::AtSligLock(g, world.mEntities[3], n);
    };
    EXPECT_TRUE(Domain::SligWay(g, start, second, nullptr, &world).empty());
    world.mSwitches[20] = 1;
    EXPECT_FALSE(Domain::SligWay(g, start, second, nullptr, &world).empty());
}

TEST(BotSligLock, ALockIsOpenedWithTheSligItsLeverCalls)
{
    NavGraph g;
    g.Build(SligLockRoom());
    World world = SligLockWorld();
    const s32 abe = g.NodeAt(12, 100, false);
    const Domain d = Domain::Build(g, world, abe, nullptr);
    const Goap::Action* first = SligLockAction(d, 3);
    const Goap::Action* second = SligLockAction(d, 4);
    ASSERT_NE(first, nullptr) << d.Text();
    ASSERT_NE(second, nullptr) << d.Text();
    EXPECT_EQ(d.mSteps[first->mTag].mKind, StepKind::eSligLock);
    EXPECT_EQ(d.mSteps[first->mTag].mEntity, 3u);
    EXPECT_EQ(d.mSteps[first->mTag].mOther, 2u);
    EXPECT_EQ(d.mSteps[first->mTag].mEdge, 1);
    // The first gives its switch and wants nothing but Abe by the lever
    EXPECT_TRUE(HasFact(first->mEffects, Key::Switch(20), 1));
    EXPECT_EQ(first->mPre.size(), 1u);
    EXPECT_FALSE(HasFact(first->mEffects, Key::Mud(5), 2));
    // The second wants the gate on the Slig's way open, and the Slig it walks into is shot
    EXPECT_TRUE(HasFact(second->mPre, Key::Switch(20), 1));
    EXPECT_TRUE(HasFact(second->mEffects, Key::Switch(21), 1));
    EXPECT_TRUE(HasFact(second->mEffects, Key::Mud(5), 2));

    // One that is open already is not opened again
    world.mSwitches[20] = 1;
    EXPECT_EQ(SligLockAction(Domain::Build(g, world, abe, nullptr), 3), nullptr);
    world.mSwitches[20] = 0;
    // With an orb on the lever's screen there is no chant, so no Slig to take over
    World watched = world;
    watched.mEntities.push_back(MakeEntity(EntityKind::eSecurityOrb, 6, 300, 20, 324, 44));
    EXPECT_EQ(SligLockAction(Domain::Build(g, watched, abe, nullptr), 3), nullptr);
    // (an orb on another screen is nothing to it)
    watched.mEntities.back() = MakeEntity(EntityKind::eSecurityOrb, 6, 800, 20, 824, 44);
    EXPECT_NE(SligLockAction(Domain::Build(g, watched, abe, nullptr), 3), nullptr);
    // And a lock whose code is not known is left alone
    World unknown = world;
    unknown.mLockCodes.erase(3);
    EXPECT_EQ(SligLockAction(Domain::Build(g, unknown, abe, nullptr), 3), nullptr);
}

TEST(BotSligLock, TheSligSaysHiOnceAndThenTheCodeAPartAtATime)
{
    const std::vector<s32> code{5, 6, 8};
    bool hi = false;
    s32 said = 0;
    // Idle, and while the lock says its own Hi: nothing
    EXPECT_EQ(SligLockTask::Say(0, code, true, hi, said), 0u);
    // It waits for Hi: once, when the Slig can speak
    EXPECT_EQ(SligLockTask::Say(1, code, false, hi, said), 0u);
    EXPECT_EQ(SligLockTask::Say(1, code, true, hi, said), static_cast<u32>(Btn::eSpeak1));
    EXPECT_EQ(SligLockTask::Say(1, code, true, hi, said), 0u);
    // It says its code: nothing
    EXPECT_EQ(SligLockTask::Say(2, code, true, hi, said), 0u);
    // It listens: the code, a part each time the Slig stands again
    EXPECT_EQ(SligLockTask::Say(3, code, true, hi, said), static_cast<u32>(Btn::eSpeak5));
    EXPECT_EQ(SligLockTask::Say(3, code, false, hi, said), 0u);
    EXPECT_EQ(SligLockTask::Say(3, code, true, hi, said), static_cast<u32>(Btn::eSpeak6));
    EXPECT_EQ(SligLockTask::Say(3, code, true, hi, said), static_cast<u32>(Btn::eSpeak8));
    EXPECT_EQ(SligLockTask::Say(3, code, true, hi, said), 0u);
    // It said no: the next time round starts with Hi again and the whole code
    EXPECT_EQ(SligLockTask::Say(5, code, true, hi, said), 0u);
    EXPECT_EQ(SligLockTask::Say(1, code, true, hi, said), static_cast<u32>(Btn::eSpeak1));
    EXPECT_EQ(SligLockTask::Say(3, code, true, hi, said), static_cast<u32>(Btn::eSpeak5));
}

TEST(BotSligLock, ItDropsOntoAFloorOnlyBehindSligsThatWalkAway)
{
    NavGraph g;
    g.Build(SligLockRoom());
    World world;
    // Down to 712,300 going right, with nobody there
    EXPECT_TRUE(SligLockTask::DropClear(g, world, 712, 300, false, false));
    SligAt slig;
    slig.mX = 900;
    slig.mY = 300;
    slig.mMotion = SligLockTask::kSligWalking;
    world.mSligs.push_back(slig);
    // One ahead that walks away: yes
    EXPECT_TRUE(SligLockTask::DropClear(g, world, 800, 300, false, false));
    // One that faces the place: no
    world.mSligs[0].mFacingLeft = true;
    EXPECT_FALSE(SligLockTask::DropClear(g, world, 800, 300, false, false));
    // One that faces away but stands (the end of its beat: it turns next): no
    world.mSligs[0].mFacingLeft = false;
    world.mSligs[0].mMotion = 0;
    EXPECT_FALSE(SligLockTask::DropClear(g, world, 800, 300, false, false));
    // One behind where it lands, or right next to it: no
    world.mSligs[0].mMotion = SligLockTask::kSligWalking;
    world.mSligs[0].mX = 770;
    EXPECT_FALSE(SligLockTask::DropClear(g, world, 800, 300, false, false));
    world.mSligs[0].mX = 840;
    EXPECT_FALSE(SligLockTask::DropClear(g, world, 800, 300, false, false));
    // One on the next screen sees nothing of it, whichever way it faces
    world.mSligs[0].mX = 900;
    world.mSligs[0].mFacingLeft = true;
    EXPECT_TRUE(SligLockTask::DropClear(g, world, 712, 300, false, false));
    // Nor does one on another floor, or one that is asleep
    world.mSligs[0].mX = 730;
    world.mSligs[0].mY = 200;
    EXPECT_TRUE(SligLockTask::DropClear(g, world, 712, 300, false, false));
    world.mSligs[0].mY = 300;
    world.mSligs[0].mAsleep = true;
    EXPECT_TRUE(SligLockTask::DropClear(g, world, 712, 300, false, false));
}

// A ledge over the end of a floor a Slig walks, all on one screen. shadow: with an invisible zone on that floor.
static NavInput WatchedFloor(bool shadow)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(0, 200, 370, 200, 0));
    if (shadow)
    {
        in.mFeatures.push_back(Feature(FeatureKind::eInvisibleZone, 150, 60, 250, 204));
    }
    return in;
}

static World WatchedWorld(bool lock)
{
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 1, 250, 180, 274, 204));
    if (lock)
    {
        Entity ghost = MakeEntity(EntityKind::eSlapLock, 2, 25, 80, 49, 104);
        ghost.mInputs[0] = 95;
        world.mEntities.push_back(ghost);
    }
    return world;
}

TEST(BotSligWatch, AFloorASligWatchesWithNoShadowOnItIsAPlaceToBeOnlyUnseen)
{
    NavGraph g;
    g.Build(WatchedFloor(false));
    World world = WatchedWorld(true);
    std::vector<u32> watchers;
    EXPECT_TRUE(Domain::SligWatched(g, world, 112, 200, false, &watchers));
    ASSERT_EQ(watchers.size(), 1u);
    EXPECT_EQ(watchers[0], 1u);
    // Not the ledge over it: another floor
    EXPECT_FALSE(Domain::SligWatched(g, world, 62, 100, false));
    // Not in a path where the power to go unseen is not to be had: there it is crossed behind the Slig's back
    EXPECT_FALSE(Domain::SligWatched(g, WatchedWorld(false), 112, 200, false));
    // (in hand counts as to be had)
    World charged = WatchedWorld(false);
    charged.mInvisCharge = true;
    EXPECT_TRUE(Domain::SligWatched(g, charged, 112, 200, false));
    // Not with the Slig dead, asleep, or one a spawner has not made
    world.mEntities[0].mGone = true;
    EXPECT_FALSE(Domain::SligWatched(g, world, 112, 200, false));
    world.mEntities[0].mGone = false;
    world.mEntities[0].mState = 1;
    EXPECT_FALSE(Domain::SligWatched(g, world, 112, 200, false));
    world.mEntities[0].mState = 0;
    world.mEntities[0].mSwitch = 9;
    EXPECT_FALSE(Domain::SligWatched(g, world, 112, 200, false));
    // (nor with its switch on: one a spawner makes counts when it is seen on the floor, and then as one
    // that is not the map's)
    world.mSwitches[9] = 1;
    EXPECT_FALSE(Domain::SligWatched(g, world, 112, 200, false));
    SligAt made;
    made.mX = 262;
    made.mY = 200;
    world.mSligs.push_back(made);
    bool anon = false;
    watchers.clear();
    EXPECT_TRUE(Domain::SligWatched(g, world, 112, 200, false, &watchers, &anon));
    EXPECT_TRUE(anon);
    EXPECT_TRUE(watchers.empty());
    world.mSligs[0].mAsleep = true;
    EXPECT_FALSE(Domain::SligWatched(g, world, 112, 200, false));
    // Not with a shadow on that floor on that screen: the way there is from shadow to shadow
    NavGraph shaded;
    shaded.Build(WatchedFloor(true));
    EXPECT_FALSE(Domain::SligWatched(shaded, WatchedWorld(true), 112, 200, false));
    // And a Slig on the next screen does not see him
    World next = WatchedWorld(true);
    next.mEntities[0] = MakeEntity(EntityKind::eSlig, 1, 400, 180, 424, 204);
    NavInput wide = WatchedFloor(false);
    wide.mLines[1] = Line(0, 200, 700, 200, 0);
    NavGraph two;
    two.Build(wide);
    EXPECT_FALSE(Domain::SligWatched(two, next, 112, 200, false));
    EXPECT_TRUE(Domain::SligWatched(two, next, 387, 200, false));
}

TEST(BotSligWatch, TheWayOntoSuchAFloorIsPlannedUnseenOrWithItsSligsDead)
{
    NavGraph g;
    g.Build(WatchedFloor(false));
    const s32 abe = g.NodeAt(12, 100, false);
    const auto crossings = [&](const World& world, s32& unseen, s32& dead, s32& plain)
    {
        unseen = 0;
        dead = 0;
        plain = 0;
        const Domain d = Domain::Build(g, world, abe, nullptr);
        for (const Goap::Action& a : d.mActions)
        {
            if (d.mSteps[a.mTag].mKind != StepKind::eCross || g.Nodes()[g.Edges()[d.mSteps[a.mTag].mEdge].mTo].mY != 200)
            {
                continue;
            }
            if (NeedsUnseen(a))
            {
                unseen++;
            }
            else if (HasFact(a.mPre, Key::Mud(1), 2))
            {
                dead++;
            }
            else
            {
                plain++;
            }
        }
    };
    s32 unseen = 0;
    s32 dead = 0;
    s32 plain = 0;
    crossings(WatchedWorld(true), unseen, dead, plain);
    EXPECT_GE(unseen, 1);
    EXPECT_EQ(dead, unseen);
    EXPECT_EQ(plain, 0);
    // With no power to be had it is a crossing like any other
    crossings(WatchedWorld(false), unseen, dead, plain);
    EXPECT_EQ(unseen, 0);
    EXPECT_EQ(dead, 0);
    EXPECT_GE(plain, 1);
}

TEST(BotNav, AHopCatchesALedgeHighOverHisFloorWithFloorStillUnderHim)
{
    // Feeco's path 3: from 787,1500 up to the ledge at 837,1420, two cells off. From the last cell of the floor,
    // one cell off, he goes by under it.
    NavInput in;
    in.mLines.push_back(Line(0, 100, 75, 100, 0));
    in.mLines.push_back(Line(75, 20, 150, 20, 0));
    NavFeature ledge = Feature(FeatureKind::eEdge, 75, 20, 99, 44);
    ledge.mA = 1;
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    const s32 top = g.NodeAt(87, 20, false);
    ASSERT_GE(top, 0);
    EXPECT_TRUE(HasEdge(g, g.NodeAt(37, 100, false), top, EdgeKind::eHop)) << g.Text();
    EXPECT_TRUE(HasEdge(g, g.NodeAt(12, 100, false), top, EdgeKind::eHop)) << g.Text();
    EXPECT_FALSE(HasEdge(g, g.NodeAt(62, 100, false), top, EdgeKind::eHop)) << g.Text();

    // A ledge only a little over his floor is not caught that way: he lands on his own floor first
    in.mLines[1] = Line(75, 60, 150, 60, 0);
    in.mFeatures[0] = Feature(FeatureKind::eEdge, 75, 60, 99, 84);
    in.mFeatures[0].mA = 1;
    in.mFeatures[0].mDir = 1;
    NavGraph low;
    low.Build(in);
    const s32 lowTop = low.NodeAt(87, 60, false);
    ASSERT_GE(lowTop, 0);
    EXPECT_FALSE(HasEdge(low, low.NodeAt(37, 100, false), lowTop, EdgeKind::eHop)) << low.Text();
}

// A Slig that walks a beat: stands `pauseLeft` frames at its left end facing left, turns for 12 (blind), walks
// right a pixel a frame, stands `pauseRight` at the right end, turns, walks back
static SligCycle::Seen BeatAt(s32 frame, s32 minX, s32 maxX, s32 pauseLeft, s32 pauseRight, s32 phase)
{
    const s32 walk = maxX - minX;
    const s32 period = pauseLeft + 12 + walk + pauseRight + 12 + walk;
    s32 t = (frame + phase) % period;
    SligCycle::Seen s;
    if (t < pauseLeft)
    {
        s.mX = minX;
        s.mFacingLeft = true;
        return s;
    }
    t -= pauseLeft;
    if (t < 12)
    {
        s.mX = minX;
        s.mFacingLeft = t < 6;
        s.mBlind = true;
        return s;
    }
    t -= 12;
    if (t < walk)
    {
        s.mX = minX + t;
        return s;
    }
    t -= walk;
    if (t < pauseRight)
    {
        s.mX = maxX;
        return s;
    }
    t -= pauseRight;
    if (t < 12)
    {
        s.mX = maxX;
        s.mFacingLeft = t >= 6;
        s.mBlind = true;
        return s;
    }
    t -= 12;
    s.mX = maxX - t;
    s.mFacingLeft = true;
    return s;
}

TEST(BotSligCycle, ASligOnABeatIsKnownOnceItHasComeRoundAndThenToldAhead)
{
    SligCycle cycle;
    // Two out of step, the same period: 30 + 12 + 80 + 20 + 12 + 80 = 234
    const auto at = [](s32 frame)
    {
        return std::vector<SligCycle::Seen>{BeatAt(frame, 500, 580, 30, 20, 0), BeatAt(frame, 520, 600, 20, 30, 70)};
    };
    s32 knownAt = -1;
    for (s32 f = 1; f <= 500; f++)
    {
        cycle.Observe(static_cast<u32>(f), 7, at(f));
        if (cycle.Known() && knownAt < 0)
        {
            knownAt = f;
        }
    }
    // Once each has been seen to go round twice
    EXPECT_EQ(knownAt, 2 * 234);
    EXPECT_EQ(cycle.Period(0), 234);
    EXPECT_EQ(cycle.Period(1), 234);
    for (s32 ahead = 0; ahead < 600; ahead += 7)
    {
        EXPECT_TRUE(cycle.Ahead(0, ahead) == at(500 + ahead)[0]) << ahead;
        EXPECT_TRUE(cycle.Ahead(1, ahead) == at(500 + ahead)[1]) << ahead;
    }
    // One that stands still the whole time tells nothing
    SligCycle still;
    for (s32 f = 1; f <= 500; f++)
    {
        still.Observe(static_cast<u32>(f), 7, {SligCycle::Seen{}});
    }
    EXPECT_FALSE(still.Known());
    // Put off its beat, it has to be watched round again; a frame missed or another screen and all of it is
    SligCycle::Seen off = at(501)[0];
    off.mX += 3;
    cycle.Observe(501, 7, {off, at(501)[1]});
    EXPECT_FALSE(cycle.Known());
    EXPECT_EQ(cycle.Period(1), 234);
    // (and not by the first stretch that looks the same: the whole period has to)
    for (s32 f = 502; f < 501 + 2 * 234; f++)
    {
        cycle.Observe(static_cast<u32>(f), 7, at(f));
        EXPECT_FALSE(cycle.Known()) << f;
    }
    cycle.Observe(501 + 2 * 234, 7, at(501 + 2 * 234));
    EXPECT_TRUE(cycle.Known());
    cycle.Observe(503 + 2 * 234, 7, at(503));
    EXPECT_EQ(cycle.Watched(), 1);
    cycle.Observe(504 + 2 * 234, 8, at(504));
    EXPECT_EQ(cycle.Watched(), 1);
}

// Cells 25 apart from 625 down to 475: cover at both ends, the open between
static std::vector<SligCycle::Cell> OpenBetweenCovers()
{
    std::vector<SligCycle::Cell> cells;
    for (s32 x = 625; x >= 475; x -= 25)
    {
        cells.push_back({x, x == 625 || x == 475});
    }
    return cells;
}

// Plays a crossing out a move at a time against what the Sligs really do (`at`), asking again after each roll as
// the task does. Gives how many of them saw him, or -1 if he never got there.
static s32 PlayCrossing(const std::function<std::vector<SligCycle::Seen>(s32)>& at, s32 from, std::vector<s32>* stops = nullptr)
{
    SligCycle cycle;
    for (s32 f = 1; f <= from; f++)
    {
        cycle.Observe(static_cast<u32>(f), 1, at(f));
    }
    std::vector<SligCycle::Cell> cells = OpenBetweenCovers();
    const auto covered = [](s32 x, bool)
    {
        return x >= 625 || x <= 475;
    };
    u32 seen = 0;
    s32 frame = from;
    const auto step = [&](s32 x, bool cover)
    {
        frame++;
        const std::vector<SligCycle::Seen> now = at(frame);
        cycle.Observe(static_cast<u32>(frame), 1, now);
        for (std::size_t i = 0; i < now.size() && !cover; i++)
        {
            if (!now[i].mBlind && (now[i].mFacingLeft ? x <= now[i].mX : x >= now[i].mX))
            {
                seen |= 1u << i;
            }
        }
    };
    for (s32 tries = 0; tries < 20 && cells.size() > 1; tries++)
    {
        const SligCycle::Move move = cycle.Cross(cells, covered);
        for (s32 f = 0; f < move.mDelay; f++)
        {
            step(cells[0].mX, cells[0].mCover);
        }
        step(cells[0].mX, cells[0].mCover);
        for (s32 x = cells[0].mX - 6; x > cells[move.mTo].mX; x -= 6)
        {
            step(x, covered(x, true));
        }
        for (s32 f = 0; f <= SligCycle::kRollEnd; f++)
        {
            step(cells[move.mTo].mX, cells[move.mTo].mCover);
        }
        if (stops)
        {
            stops->push_back(cells[move.mTo].mX);
        }
        cells.erase(cells.begin(), cells.begin() + move.mTo);
    }
    return cells.size() == 1 ? static_cast<s32>(seen == 0 ? 0 : (seen & 1) + ((seen >> 1) & 1) + ((seen >> 2) & 1)) : -1;
}

TEST(BotSligCycle, TheOpenIsCrossedWhenNoneLooks)
{
    // One Slig on a beat over the open, 500 to 580. He has to pass under it, so it sees him unless that is while
    // it turns at its left end: right of it while it still faces left, left of it once it faces right
    const auto one = [](s32 frame)
    {
        return std::vector<SligCycle::Seen>{BeatAt(frame, 500, 580, 30, 20, 0)};
    };
    SligCycle cycle;
    for (s32 f = 1; f <= 500; f++)
    {
        cycle.Observe(static_cast<u32>(f), 1, one(f));
    }
    ASSERT_TRUE(cycle.Known());
    const SligCycle::Move move = cycle.Cross(OpenBetweenCovers(), [](s32 x, bool)
                                             { return x >= 625 || x <= 475; });
    EXPECT_EQ(move.mSpotters, 0);
    EXPECT_GT(move.mDelay, 0);
    // (and it works out, from whenever he starts to look)
    for (s32 from = 500; from < 740; from += 37)
    {
        EXPECT_EQ(PlayCrossing(one, from), 0) << from;
    }
    // In cover all the way there is nothing to wait for
    const SligCycle::Move hidden = cycle.Cross(OpenBetweenCovers(), [](s32, bool)
                                               { return true; });
    EXPECT_EQ(hidden.mSpotters, 0);
    EXPECT_EQ(hidden.mDelay, 0);
    EXPECT_EQ(hidden.mTo, 6);
}

TEST(BotSligCycle, ASligThatWalksHisWayIsNotRolledPast)
{
    // One that walks left from 600 to 470 (past the far cover) a pixel a frame and stands there a long time. He
    // rolls six a frame: setting out behind it he would pass it and be seen. He waits until it is far enough on.
    const auto slow = [](s32 frame)
    {
        return std::vector<SligCycle::Seen>{BeatAt(frame, 470, 600, 100, 100, 100 + 12 + 130 + 100 + 12)};
    };
    // (1028: it is on its way left, in the middle of the open)
    ASSERT_TRUE(slow(1028)[0].mFacingLeft);
    ASSERT_GT(slow(1028)[0].mX, 500);
    std::vector<s32> stops;
    EXPECT_EQ(PlayCrossing(slow, 1028, &stops), 0);
    ASSERT_FALSE(stops.empty());
    EXPECT_EQ(stops.back(), 475);
    // With a second one that always looks his way there is no way unseen: the best is with only that one
    const auto two = [&](s32 frame)
    {
        SligCycle::Seen stare;
        stare.mX = 300;
        // (it has to be seen to do something to be known: it shifts a pixel to and fro)
        stare.mX += (frame / 50) % 2;
        return std::vector<SligCycle::Seen>{slow(frame)[0], stare};
    };
    EXPECT_EQ(PlayCrossing(two, 1028), 1);
}

TEST(BotNav, ACellIsCoverFromTheBackgroundOnlyWithAllOfHimInside)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 375, 200, 0));
    // (a rock 44 wide: one cell of it holds him standing, 26 wide and 52 high)
    NavFeature rock = Feature(FeatureKind::eZCover, 143, 140, 187, 202);
    rock.mAnyScale = true;
    in.mFeatures.push_back(rock);
    NavGraph g;
    g.Build(in);
    EXPECT_TRUE(g.Nodes()[g.NodeAt(162, 200, false)].mZCover);
    EXPECT_FALSE(g.Nodes()[g.NodeAt(137, 200, false)].mZCover);
    EXPECT_FALSE(g.Nodes()[g.NodeAt(187, 200, false)].mZCover);
    // Rolling his box is 37 wide, further ahead of him than behind: in it for less of the way
    EXPECT_TRUE(g.ZCovered(170, 200, -24, 13, 34));
    EXPECT_FALSE(g.ZCovered(162, 200, -24, 13, 34));
    // One that begins just 52 over the floor still holds him; a unit lower it does not
    NavInput just = in;
    just.mFeatures[0].mY1 = 148;
    NavGraph j;
    j.Build(just);
    EXPECT_TRUE(j.Nodes()[j.NodeAt(162, 200, false)].mZCover);
    just.mFeatures[0].mY1 = 149;
    NavGraph k;
    k.Build(just);
    EXPECT_FALSE(k.Nodes()[k.NodeAt(162, 200, false)].mZCover);
    // Too low a cover is none to one who stands
    NavInput low = in;
    low.mFeatures[0].mY1 = 160;
    NavGraph h;
    h.Build(low);
    EXPECT_FALSE(h.Nodes()[h.NodeAt(162, 200, false)].mZCover);
    EXPECT_TRUE(h.ZCovered(162, 200, -13, 13, 34));
    // but one to him crouched, as the higher one is too
    EXPECT_TRUE(h.Nodes()[h.NodeAt(162, 200, false)].mZLow);
    EXPECT_FALSE(h.Nodes()[h.NodeAt(137, 200, false)].mZLow);
    EXPECT_TRUE(g.Nodes()[g.NodeAt(162, 200, false)].mZLow);
}

TEST(BotNav, ACoverOfTheNextScreenThatReachesOverIsNone)
{
    // (the game looks for the cover among what belongs to the screen he is on)
    NavInput in;
    in.mLines.push_back(Line(0, 200, 375, 200, 0));
    NavFeature rock = Feature(FeatureKind::eZCover, 143, 140, 187, 202);
    rock.mAnyScale = true;
    rock.mCell = 1;
    in.mFeatures.push_back(rock);
    NavGraph next;
    next.Build(in);
    EXPECT_FALSE(next.Nodes()[next.NodeAt(162, 200, false)].mZCover);
    EXPECT_FALSE(next.ZCovered(162, 200, -13, 13, 34));
    in.mFeatures[0].mCell = 0;
    NavGraph own;
    own.Build(in);
    EXPECT_TRUE(own.Nodes()[own.NodeAt(162, 200, false)].mZCover);
}

TEST(BotSligCycle, ALeapIsMadeWhenTheFewestSeeHimOnIt)
{
    // One Slig on a beat from 500 to 580, and a way that has him at 450 in the open for a while: it sees him
    // there whenever it faces left
    const auto one = [](s32 frame)
    {
        return std::vector<SligCycle::Seen>{BeatAt(frame, 500, 580, 30, 20, 0)};
    };
    SligCycle cycle;
    for (s32 f = 1; f <= 700; f++)
    {
        cycle.Observe(static_cast<u32>(f), 1, one(f));
    }
    ASSERT_TRUE(cycle.Known());
    const auto way = [](s32 open)
    {
        std::vector<SligCycle::Cell> track(5, SligCycle::Cell{475, true});
        track.insert(track.end(), static_cast<std::size_t>(open), SligCycle::Cell{450, false});
        track.insert(track.end(), 5, SligCycle::Cell{425, true});
        return track;
    };
    const std::vector<SligCycle::Shooter> shooters{{10, 1}};
    // Short enough to be made while it faces right: then, and it is not seen
    const std::vector<SligCycle::Cell> quick = way(30);
    const SligCycle::Move move = cycle.Through(quick, shooters, 600);
    EXPECT_EQ(move.mSpotters, 0);
    s32 seen = 0;
    s32 seenAtOnce = 0;
    for (s32 t = 0; t < static_cast<s32>(quick.size()); t++)
    {
        const SligCycle::Seen then = one(700 + move.mDelay + t + 1)[0];
        const SligCycle::Seen now = one(700 + t + 1)[0];
        seen += !quick[t].mCover && !then.mBlind && then.mFacingLeft ? 1 : 0;
        seenAtOnce += !quick[t].mCover && !now.mBlind && now.mFacingLeft ? 1 : 0;
    }
    EXPECT_EQ(seen, 0);
    // (frame 700 is the end of its walk left: made at once it would have been)
    EXPECT_GT(seenAtOnce, 0);
    EXPECT_GT(move.mDelay, 0);
    // Longer than it ever faces right for: it shoots, and again while he is still out there
    const SligCycle::Move slow = cycle.Through(way(300), shooters, 600);
    EXPECT_GE(slow.mSpotters, 2);
    // One that only just can't be made unseen: one burst
    EXPECT_EQ(cycle.Through(way(150), shooters, 600).mSpotters, 1);
    // In cover all the way there is nothing to wait for
    const SligCycle::Move hidden = cycle.Through(way(0), shooters, 600);
    EXPECT_EQ(hidden.mSpotters, 0);
    EXPECT_EQ(hidden.mDelay, 0);
    // With something else that has to let him go too (flying mines over the same stretch): the first wait both
    // allow, which is a later one
    const std::function<bool(s32)> later = [&move](s32 delay)
    {
        return delay > move.mDelay + 20 && delay % 2 == 0;
    };
    const SligCycle::Move both = cycle.Through(quick, shooters, 600, &later);
    EXPECT_EQ(both.mSpotters, 0);
    EXPECT_GT(both.mDelay, move.mDelay + 20);
    EXPECT_EQ(both.mDelay % 2, 0);
    // (in cover all the way as well: not before it is let)
    const std::function<bool(s32)> from50 = [](s32 delay)
    {
        return delay >= 50;
    };
    EXPECT_EQ(cycle.Through(way(0), shooters, 600, &from50).mDelay, 50);
    // And with no wait let at all there is none: 99
    const std::function<bool(s32)> never = [](s32)
    {
        return false;
    };
    EXPECT_EQ(cycle.Through(quick, shooters, 600, &never).mSpotters, 99);
}

// A floor, a ledge over it from 100 on with a hoist up to its end, and something that shuts in the ledge's
// second cell
static NavInput LedgeWithFence(s32 hoistDir, bool solid)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 370, 200, 0));
    in.mLines.push_back(Line(100, 100, 300, 100, 0));
    NavFeature hoist = Feature(FeatureKind::eHoist, 100, 100, 124, 195);
    hoist.mDir = hoistDir;
    in.mFeatures.push_back(hoist);
    NavFeature fence = Feature(FeatureKind::eBarrier, 125, 60, 149, 104);
    fence.mSwitch = 7;
    fence.mStartClosed = true;
    fence.mSolid = solid;
    in.mFeatures.push_back(fence);
    return in;
}

TEST(BotNav, APullUpOntoALedgeBesideAnElectricWallWaitsForItToBeOff)
{
    // Facing the wall as he comes up he reaches into its cell: only with it off
    NavGraph g;
    g.Build(LedgeWithFence(1, false));
    const NavEdge* up = FindEdge(g, g.NodeAt(112, 200, false), g.NodeAt(112, 100, false), EdgeKind::eHoistUp);
    ASSERT_NE(up, nullptr);
    s32 gate = 0;
    s32 open = 0;
    ASSERT_TRUE(g.Condition(*up, &gate, &open));
    EXPECT_EQ(gate, 7);
    EXPECT_EQ(open, 1);
    // (so the cell up there is not of a piece with the floor below)
    World world;
    const Domain d = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    EXPECT_NE(d.mRegions[g.NodeAt(112, 200, false)], d.mRegions[g.NodeAt(112, 100, false)]);
    // Facing away from it there is nothing to it
    NavGraph away;
    away.Build(LedgeWithFence(0, false));
    up = FindEdge(away, away.NodeAt(112, 200, false), away.NodeAt(112, 100, false), EdgeKind::eHoistUp);
    ASSERT_NE(up, nullptr);
    EXPECT_FALSE(away.Condition(*up, nullptr, nullptr));
    // Nor to a door that is only shut: that hurts nobody
    NavGraph door;
    door.Build(LedgeWithFence(1, true));
    up = FindEdge(door, door.NodeAt(112, 200, false), door.NodeAt(112, 100, false), EdgeKind::eHoistUp);
    ASSERT_NE(up, nullptr);
    EXPECT_FALSE(door.Condition(*up, nullptr, nullptr));
}

TEST(BotSligWatch, AWayOntoAFloorASligWalksIsDearerThanOneThatKeepsOffIt)
{
    // A ledge (0..100 at 100) over a floor a Slig walks (at 200)
    NavGraph g;
    g.Build(WatchedFloor(false));
    World world = WatchedWorld(false);
    const s32 ledge = g.NodeAt(87, 100, false);
    const s32 floor = g.NodeAt(112, 200, false);
    ASSERT_GE(ledge, 0);
    ASSERT_GE(floor, 0);
    EXPECT_LT(Domain::SligBeat(g, world, ledge), 0);
    EXPECT_EQ(Domain::SligBeat(g, world, floor), g.Nodes()[floor].mLine);
    const auto downCost = [&](const World& w)
    {
        const Domain d = Domain::Build(g, w, ledge, nullptr);
        s32 cost = -1;
        for (const Goap::Action& a : d.mActions)
        {
            if (d.mSteps[a.mTag].mKind == StepKind::eCross && g.Nodes()[g.Edges()[d.mSteps[a.mTag].mEdge].mTo].mY == 200)
            {
                cost = cost < 0 ? a.mCost : std::min(cost, a.mCost);
            }
        }
        return cost;
    };
    const s32 watched = downCost(world);
    // Not with the Slig gone, nor with one asleep
    world.mEntities[0].mGone = true;
    const s32 gone = downCost(world);
    world.mEntities[0].mGone = false;
    world.mEntities[0].mState = 1;
    const s32 asleep = downCost(world);
    ASSERT_GT(gone, 0);
    EXPECT_EQ(watched, gone + Domain::kSligBeatCost);
    EXPECT_EQ(asleep, gone);
}

TEST(BotSligWatch, ASligOffTheScreenAndWalkingAwayIsNothingToASortie)
{
    NavGraph g;
    g.Build(WatchedFloor(true));
    SligAt slig;
    slig.mX = 300;
    slig.mY = 200;
    // On his screen
    EXPECT_EQ(ClearSligTask::SligAbout(g, slig, 112, 200, false), 1);
    // Off it: walking away, and standing or coming
    slig.mX = 400;
    slig.mMotion = 2;
    EXPECT_EQ(ClearSligTask::SligAbout(g, slig, 112, 200, false), 3);
    slig.mMotion = 0;
    EXPECT_EQ(ClearSligTask::SligAbout(g, slig, 112, 200, false), 2);
    slig.mMotion = 2;
    slig.mFacingLeft = true;
    EXPECT_EQ(ClearSligTask::SligAbout(g, slig, 112, 200, false), 2);
    // Another floor, asleep or taken over: nothing to him
    slig.mY = 100;
    EXPECT_EQ(ClearSligTask::SligAbout(g, slig, 112, 200, false), 0);
    slig.mY = 200;
    slig.mAsleep = true;
    EXPECT_EQ(ClearSligTask::SligAbout(g, slig, 112, 200, false), 0);
    // The place to make a sortie from: the nearest cell by the thing that its floor's Slig doesn't see. Here the
    // shadow (150..250: the cells 187 and 212 are wholly in it) for a thing at 262, and none for one too far
    // from it
    const auto plain = [](const NavEdge& e) -> s32
    {
        return e.mCost + 1;
    };
    const s32 haven = ClearSligTask::Haven(g, plain, g.NodeAt(262, 200, false));
    ASSERT_GE(haven, 0);
    EXPECT_TRUE(g.Nodes()[haven].mHidden);
    EXPECT_EQ(g.Nodes()[haven].mX, 212);
    EXPECT_LT(ClearSligTask::Haven(g, plain, g.NodeAt(337, 200, false)), 0);
    // With no shadow there is nowhere on that floor
    NavGraph bare;
    bare.Build(WatchedFloor(false));
    EXPECT_LT(ClearSligTask::Haven(bare, plain, bare.NodeAt(262, 200, false)), 0);
}

// ---- A Slig that stands guard (the Slig Barracks' first path) ----

// A floor (0..370 at 200) with a ledge over its right part (175..300 at 100) and a hoist up to the ledge's end
static NavInput GuardedLedge(s32 hoistDir)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 370, 200, 0));
    in.mLines.push_back(Line(175, 100, 300, 100, 0));
    NavFeature hoist = Feature(FeatureKind::eHoist, 175, 100, 199, 195);
    hoist.mDir = hoistDir;
    in.mFeatures.push_back(hoist);
    return in;
}

static SligAt GuardAt(s32 x, s32 y, bool facingLeft, s32 motion)
{
    SligAt slig;
    slig.mX = x;
    slig.mY = y;
    slig.mFacingLeft = facingLeft;
    slig.mMotion = motion;
    return slig;
}

TEST(BotNav, AFallALedgeCatchesEndsHungHoweverHeWentOff)
{
    // (the room of RolledOffALowEndHeCatchesALedgeOutAndBelowThatFacesTheWayHeGoes)
    NavInput in;
    in.mLines.push_back(Line(0, 100, 400, 100, 0));
    in.mLines.push_back(Line(480, 200, 700, 200, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eSweep, 300, 40, 420, 64));
    NavFeature ledge = Feature(FeatureKind::eEdge, 470, 200, 495, 224);
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    const s32 end = g.NodeAt(387, 100, false);
    const s32 hung = g.NodeAt(487, 200, false);
    ASSERT_GE(end, 0);
    ASSERT_GE(hung, 0);
    s32 falls = 0;
    for (const NavEdge& e : g.Edges())
    {
        if (e.mKind == EdgeKind::eFall && e.mFrom == end && e.mTo == hung)
        {
            falls++;
            EXPECT_TRUE(e.mHung);
        }
    }
    EXPECT_GE(falls, 1);
    // A walk off a ledge onto the floor under it is a landing
    NavInput plain;
    plain.mLines.push_back(Line(0, 100, 100, 100, 0));
    plain.mLines.push_back(Line(0, 200, 400, 200, 0));
    NavGraph p;
    p.Build(plain);
    s32 landings = 0;
    for (const NavEdge& e : p.Edges())
    {
        if (e.mKind == EdgeKind::eFall)
        {
            landings++;
            EXPECT_FALSE(e.mHung);
        }
    }
    EXPECT_GE(landings, 1);
}

TEST(BotSligGuard, TheGuardIsTheNearestSligThatStandsAwakeOnThatFloor)
{
    World world;
    EXPECT_EQ(GoToTask::Sentry(world, 100, 200, false), nullptr);
    // One that walks its beat is none, nor one a step lower, asleep, taken over or far off
    world.mSligs.push_back(GuardAt(150, 200, false, 2));
    world.mSligs.push_back(GuardAt(150, 200, false, 4));
    world.mSligs.push_back(GuardAt(150, 221, false, 0));
    world.mSligs.push_back(GuardAt(150, 200, false, 0));
    world.mSligs.back().mAsleep = true;
    world.mSligs.push_back(GuardAt(150, 200, false, 0));
    world.mSligs.back().mPossessed = true;
    world.mSligs.push_back(GuardAt(360, 200, false, 0));
    EXPECT_EQ(GoToTask::Sentry(world, 100, 200, false), nullptr);
    // At half size the cells are half as long
    world.mSligs.push_back(GuardAt(200, 200, false, 0));
    world.mSligs.back().mHalf = true;
    EXPECT_EQ(GoToTask::Sentry(world, 100, 200, false), nullptr);
    ASSERT_NE(GoToTask::Sentry(world, 100, 200, true), nullptr);
    EXPECT_EQ(GoToTask::Sentry(world, 60, 200, true), nullptr);
    // One that stands, or turns where it stands: the nearer of two
    world.mSligs.push_back(GuardAt(340, 200, true, 0));
    world.mSligs.push_back(GuardAt(150, 206, true, 5));
    const SligAt* guard = GoToTask::Sentry(world, 100, 200, false);
    ASSERT_NE(guard, nullptr);
    EXPECT_EQ(guard->mX, 150);
    guard = GoToTask::Sentry(world, 300, 200, false);
    ASSERT_NE(guard, nullptr);
    EXPECT_EQ(guard->mX, 340);
}

TEST(BotSligGuard, ItHasBegunToTurnItsBackOnlyWhileItTurnsFromFacingHim)
{
    // He is at 100, it at 150: turned his way it faces left
    EXPECT_TRUE(GoToTask::TurnsFrom(GuardAt(150, 200, true, 5), 100));
    EXPECT_FALSE(GoToTask::TurnsFrom(GuardAt(150, 200, true, 0), 100));
    // (turning from its back to him it comes round to face him)
    EXPECT_FALSE(GoToTask::TurnsFrom(GuardAt(150, 200, false, 5), 100));
    EXPECT_TRUE(GoToTask::TurnsFrom(GuardAt(150, 200, false, 5), 200));
    EXPECT_FALSE(GoToTask::TurnsFrom(GuardAt(150, 200, true, 5), 200));
}

TEST(BotSligGuard, HungUnderAGuardHeComesUpAsItTurnsAwayWithNoOtherSligAboutToSee)
{
    NavGraph g;
    g.Build(GuardedLedge(1));
    const s32 ledge = g.NodeAt(262, 200, false);
    ASSERT_GE(ledge, 0);
    World world;
    const SligAt* why = nullptr;
    // No guard there: up at once
    EXPECT_TRUE(GoToTask::UpBehind(g, world, ledge, &why));
    EXPECT_EQ(why, nullptr);
    // One that faces him, or has its back to him for who knows how much longer: not yet
    world.mSligs.push_back(GuardAt(312, 200, true, 0));
    EXPECT_FALSE(GoToTask::UpBehind(g, world, ledge, &why));
    ASSERT_NE(why, nullptr);
    EXPECT_EQ(why->mX, 312);
    world.mSligs[0].mFacingLeft = false;
    EXPECT_FALSE(GoToTask::UpBehind(g, world, ledge, &why));
    // As it begins to turn away
    world.mSligs[0] = GuardAt(312, 200, true, 5);
    EXPECT_TRUE(GoToTask::UpBehind(g, world, ledge, &why));
    // Another, on a floor a step lower, whose beat ends short of him: not while it is nearly where it sees him
    SligAt walker = GuardAt(20, 221, false, 2);
    walker.mBeatMin = 0;
    walker.mBeatMax = 40;
    world.mSligs.push_back(walker);
    EXPECT_FALSE(GoToTask::UpBehind(g, world, ledge, &why));
    ASSERT_NE(why, nullptr);
    EXPECT_EQ(why->mX, 20);
    // (it has to walk to the far end of its beat and back first: 170 pixels is 70 frames, 130 is 54)
    world.mSligs[1].mFacingLeft = true;
    world.mSligs[1].mBeatMax = 170;
    world.mSligs[1].mX = 170;
    EXPECT_TRUE(GoToTask::UpBehind(g, world, ledge, &why));
    world.mSligs[1].mX = 130;
    EXPECT_FALSE(GoToTask::UpBehind(g, world, ledge, &why));
    // One asleep, or on another floor altogether, is no matter
    world.mSligs[1].mAsleep = true;
    EXPECT_TRUE(GoToTask::UpBehind(g, world, ledge, &why));
    world.mSligs[1].mAsleep = false;
    world.mSligs[1].mY = 300;
    EXPECT_TRUE(GoToTask::UpBehind(g, world, ledge, &why));
}

TEST(BotSligGuard, PastAGuardTheHoistThreeCellsOnIsCaughtAtARunningJumpFromAStand)
{
    NavGraph g;
    g.Build(GuardedLedge(1));
    const NavEdge* up = FindEdge(g, g.NodeAt(187, 200, false), g.NodeAt(187, 100, false), EdgeKind::eHoistUp);
    ASSERT_NE(up, nullptr);
    const s32 from = g.NodeAt(112, 200, false);
    World world;
    // With no guard there it is walked to and jumped up at
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, from, *up), 0);
    // One between him and the hoist, or past it: off two cells on
    world.mSligs.push_back(GuardAt(162, 200, false, 0));
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, from, *up), 162);
    world.mSligs[0].mX = 212;
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, from, *up), 162);
    // Behind him it is not on the way, and one that walks is not a guard
    world.mSligs[0].mX = 62;
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, from, *up), 0);
    world.mSligs[0] = GuardAt(162, 200, false, 2);
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, from, *up), 0);
    world.mSligs[0] = GuardAt(162, 200, false, 0);
    // Only from three cells off: the run has to be two cells, and the hoist in the next
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, g.NodeAt(137, 200, false), *up), 0);
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, g.NodeAt(87, 200, false), *up), 0);
    // Not to a hoist that is caught the other way round, nor by any other kind of step
    NavGraph other;
    other.Build(GuardedLedge(0));
    const NavEdge* wrong = FindEdge(other, other.NodeAt(187, 200, false), other.NodeAt(187, 100, false), EdgeKind::eHoistUp);
    ASSERT_NE(wrong, nullptr);
    EXPECT_EQ(GoToTask::JumpUpPastGuard(other, world, other.NodeAt(112, 200, false), *wrong), 0);
    const NavEdge* walk = FindEdge(g, g.NodeAt(162, 200, false), g.NodeAt(187, 200, false), EdgeKind::eWalk);
    ASSERT_NE(walk, nullptr);
    EXPECT_EQ(GoToTask::JumpUpPastGuard(g, world, from, *walk), 0);
}

// A lever on a floor (0..400 at 200), its box from 150 to 226: it is pulled from 162 or from 212
static Entity LeverBetween()
{
    Entity lever = MakeEntity(EntityKind::eLever, 1001, 150, 180, 226, 204);
    lever.mSwitch = 9;
    return lever;
}

TEST(BotDomain, ALeverIsPulledFromTheSideThatIsNotTheCellADoorStandsIn)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 400, 200, 0));
    NavGraph open;
    open.Build(in);
    bool left = true;
    EXPECT_EQ(Domain::UseNode(open, LeverBetween(), &left), open.NodeAt(162, 200, false));
    EXPECT_FALSE(left);
    // With a door shut in that cell
    NavFeature door = Feature(FeatureKind::eBarrier, 150, 150, 174, 204);
    door.mSwitch = 8;
    door.mStartClosed = true;
    door.mSolid = true;
    in.mFeatures.push_back(door);
    NavGraph shut;
    shut.Build(in);
    EXPECT_EQ(Domain::UseNode(shut, LeverBetween(), &left), shut.NodeAt(212, 200, false));
    EXPECT_TRUE(left);
    // (a lever with nowhere else to stand is pulled from there all the same)
    Entity atEnd = LeverBetween();
    atEnd.mX2 = 200;
    EXPECT_EQ(Domain::UseNode(shut, atEnd, &left), shut.NodeAt(162, 200, false));
}

// A lever on the floor (0..600 at 200) and a trap door in the floor over it (250..400 at 100), with a Slig by the
// door. The lever's switch 9 opens the door itself, or sets off a timer that does (the door's switch 7).
static NavInput TrapOverLever(bool timer)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 600, 200, 0));
    in.mLines.push_back(Line(250, 100, 300, 100, 0));
    in.mLines.push_back(Line(350, 100, 400, 100, 0));
    NavFeature trap = Feature(FeatureKind::eTrapDoor, 300, 100, 350, 120);
    trap.mSwitch = timer ? 7 : 9;
    trap.mStartClosed = true;
    in.mFeatures.push_back(trap);
    return in;
}

static World TrapWorld(bool timer)
{
    World world;
    Entity lever = MakeEntity(EntityKind::eLever, 1001, 20, 180, 94, 204);
    lever.mSwitch = 9;
    lever.mOp = 2;
    world.mEntities.push_back(lever);
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 2001, 300, 76, 324, 100));
    if (timer)
    {
        Entity logic = MakeEntity(EntityKind::eSwitchLogic, 3001, 500, 20, 524, 44);
        logic.mInputs[0] = 9;
        logic.mSwitch = 7;
        logic.mHoldOnly = true;
        world.mEntities.push_back(logic);
    }
    return world;
}

static bool UsesNeedSligDown(const Domain& domain, u32 lever, u32 slig, bool* any)
{
    bool all = true;
    *any = false;
    for (const Goap::Action& a : domain.mActions)
    {
        const DomainStep& s = domain.mSteps[a.mTag];
        if (s.mKind != StepKind::eUse || s.mEntity != lever)
        {
            continue;
        }
        *any = true;
        bool needs = false;
        for (const Goap::Fact& f : a.mPre)
        {
            needs = needs || (f.mKey == Key::Mud(slig) && f.mValue == 2);
        }
        all = all && needs;
    }
    return all;
}

static bool HasDrop(const Domain& domain)
{
    for (const Goap::Action& a : domain.mActions)
    {
        if (domain.mSteps[a.mTag].mKind == StepKind::eSligDrop)
        {
            return true;
        }
    }
    return false;
}

TEST(BotDomain, ALeverWhoseTimerOpensATrapDoorUnderASligIsLeftAloneWhileTheSligIsThere)
{
    // The lever's own switch on the door: the Slig is let down by it, and the lever is free to pull
    NavGraph direct;
    direct.Build(TrapOverLever(false));
    const Domain plain = Domain::Build(direct, TrapWorld(false), direct.NodeAt(12, 200, false), nullptr);
    bool any = false;
    EXPECT_TRUE(HasDrop(plain)) << plain.Text();
    EXPECT_FALSE(UsesNeedSligDown(plain, 1001, 2001, &any));
    EXPECT_TRUE(any);
    // Through a timer: no letting it down, and no pulling the lever until it is gone
    NavGraph g;
    g.Build(TrapOverLever(true));
    World world = TrapWorld(true);
    const Domain timed = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    EXPECT_FALSE(HasDrop(timed)) << timed.Text();
    EXPECT_TRUE(UsesNeedSligDown(timed, 1001, 2001, &any));
    EXPECT_TRUE(any);
    EXPECT_NE(timed.mStart.Get(Key::Mud(2001)), 2);
    // A timer that works some other door is nothing to it
    World elsewhere = TrapWorld(true);
    elsewhere.mEntities[2].mSwitch = 6;
    const Domain other = Domain::Build(g, elsewhere, g.NodeAt(12, 200, false), nullptr);
    EXPECT_FALSE(UsesNeedSligDown(other, 1001, 2001, &any));
    EXPECT_TRUE(any);
    // Seen on the floor under the one it was put on, it has come down
    world.mEntities[1].mLive = true;
    world.mEntities[1].mLiveX = 312;
    world.mEntities[1].mLiveY = 199;
    const Domain down = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    EXPECT_EQ(down.mStart.Get(Key::Mud(2001)), 2);
    // (not while it is seen where it was put)
    world.mEntities[1].mLiveY = 100;
    const Domain up = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    EXPECT_NE(up.mStart.Get(Key::Mud(2001)), 2);
}

TEST(BotLookahead, TheDelaysGoFromCoarseToFineAndNoneComesTwice)
{
    std::set<s32> seen;
    for (s32 i = 0; i < Bot::Lookahead::Count(); i++)
    {
        const s32 delay = Bot::Lookahead::Delay(i);
        ASSERT_GE(delay, 0) << i;
        ASSERT_LT(delay, Bot::Lookahead::kWindow) << i;
        ASSERT_EQ(delay % Bot::Lookahead::kFinest, 0) << i;
        ASSERT_TRUE(seen.insert(delay).second) << "delay " << delay << " twice";
    }
    // (every 8th frame of the window, so all of them)
    ASSERT_EQ(static_cast<s32>(seen.size()), Bot::Lookahead::kWindow / Bot::Lookahead::kFinest);
    ASSERT_EQ(Bot::Lookahead::Delay(Bot::Lookahead::Count()), -1);
    ASSERT_EQ(Bot::Lookahead::Delay(-1), -1);
    // The first pass is the coarse one, straight on first; the next halves it
    ASSERT_EQ(Bot::Lookahead::Delay(0), 0);
    ASSERT_EQ(Bot::Lookahead::Delay(1), 32);
    ASSERT_EQ(Bot::Lookahead::Delay(9), 288);
    ASSERT_EQ(Bot::Lookahead::Delay(10), 16);
    ASSERT_EQ(Bot::Lookahead::Delay(11), 48);
    ASSERT_EQ(Bot::Lookahead::Delay(20), 8);
    ASSERT_EQ(Bot::Lookahead::Delay(39), 312);
}

// (a try's failure for these: so long after the save, each at a place of its own unless said otherwise)
static s32 FailedSomewhereElse(Bot::Lookahead::Stretch& stretch, s32 frames)
{
    static s32 place = 0;
    return Bot::Lookahead::Failed(stretch, frames, ++place);
}

TEST(BotLookahead, ATryThatFailedOnceItHadBegunIsFollowedByTheNextDelay)
{
    Bot::Lookahead::Stretch stretch;
    ASSERT_EQ(stretch.mDelay, 0);
    // (straight on, hurt 100 frames in; then held back 32 and hurt 33 in: a frame after it began)
    ASSERT_EQ(FailedSomewhereElse(stretch, 100), 32);
    ASSERT_EQ(stretch.mDelay, 32);
    ASSERT_EQ(FailedSomewhereElse(stretch, 33), 64);
    ASSERT_EQ(stretch.mCeiling, Bot::Lookahead::kWindow + 1);
    // All of them fail: each is given once, then there is none
    s32 given = 3;
    while (FailedSomewhereElse(stretch, 1000) >= 0)
    {
        given++;
    }
    // (of the same buttons again: every coarsest step of the window)
    ASSERT_EQ(given, Bot::Lookahead::kWindow / Bot::Lookahead::kCoarsest);
    ASSERT_EQ(FailedSomewhereElse(stretch, 1000), -1);
    ASSERT_EQ(stretch.mWay, Bot::Lookahead::Way::eSame);
}

TEST(BotLookahead, HurtWhileItStillHeldBackRulesOutEveryDelayThatLong)
{
    Bot::Lookahead::Stretch stretch;
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), 32);
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), 64);
    // Holding back 64, hurt 50 frames in: he had done nothing yet, so 50 and over is no use. The rest of the
    // coarse pass is passed over
    ASSERT_EQ(FailedSomewhereElse(stretch, 50), -1);
    ASSERT_EQ(stretch.mCeiling, 50);
    // and so is it of whatever else is tried
    std::vector<std::pair<Bot::Lookahead::Way, s32>> rest;
    for (s32 delay = Bot::Lookahead::NextWay(stretch); delay >= 0; delay = Bot::Lookahead::NextWay(stretch))
    {
        for (; delay >= 0; delay = FailedSomewhereElse(stretch, 60))
        {
            rest.push_back({stretch.mWay, delay});
        }
    }
    using Way = Bot::Lookahead::Way;
    ASSERT_EQ(rest, (std::vector<std::pair<Way, s32>>{{Way::eAgain, 32}, {Way::eQuiet, 0}, {Way::eQuiet, 32}, {Way::eFast, 0}, {Way::eFast, 32}, {Way::eFine, 16}, {Way::eFine, 48}, {Way::eFine, 8}, {Way::eFine, 24}, {Way::eFine, 40}}));
    // (hurt on the very frame it was to begin counts as while it held back)
    Bot::Lookahead::Stretch other;
    ASSERT_EQ(FailedSomewhereElse(other, 100), 32);
    ASSERT_EQ(FailedSomewhereElse(other, 32), -1);
    ASSERT_EQ(other.mCeiling, 32);
    ASSERT_EQ(Bot::Lookahead::NextWay(other), 0);
    ASSERT_EQ(other.mWay, Way::eQuiet);
}

TEST(BotLookahead, BegunAgainEveryDelayIsTriedOnceMoreButForTheOnesKnownTooLong)
{
    Bot::Lookahead::Stretch stretch;
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), 32);
    ASSERT_EQ(FailedSomewhereElse(stretch, 24), -1);
    ASSERT_EQ(Bot::Lookahead::NextWay(stretch), 0);
    ASSERT_EQ(stretch.mWay, Bot::Lookahead::Way::eQuiet);
    ASSERT_EQ(Bot::Lookahead::Again(stretch), 0);
    ASSERT_EQ(stretch.mRounds, 1);
    ASSERT_EQ(stretch.mDelay, 0);
    ASSERT_EQ(stretch.mWay, Bot::Lookahead::Way::eSame);
    // (24 itself is too long: he was hurt 24 frames in while he held back. No coarse step is under that)
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), -1);
    ASSERT_EQ(Bot::Lookahead::NextWay(stretch), 0);
    ASSERT_EQ(stretch.mWay, Bot::Lookahead::Way::eQuiet);
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), -1);
    ASSERT_EQ(Bot::Lookahead::NextWay(stretch), 0);
    ASSERT_EQ(stretch.mWay, Bot::Lookahead::Way::eFast);
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), -1);
    ASSERT_EQ(Bot::Lookahead::NextWay(stretch), 16);
    ASSERT_EQ(stretch.mWay, Bot::Lookahead::Way::eFine);
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), 8);
    ASSERT_EQ(FailedSomewhereElse(stretch, 200), -1);
    ASSERT_EQ(Bot::Lookahead::NextWay(stretch), -1);
    ASSERT_EQ(Bot::Lookahead::NextWay(stretch), -1);
}

TEST(BotLookahead, TriesThatFailAlikeHoweverLateTheyBeganAreNotAMatterOfWhen)
{
    // Each fails 40 frames after it began (a frame's difference is nothing), at the same place: after the
    // sixth there is no more to try
    Bot::Lookahead::Stretch stretch;
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 40, 7), 32);
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 32 + 41, 7), 64);
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 64 + 39, 7), 96);
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 96 + 40, 7), 128);
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 128 + 40, 7), 160);
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 160 + 40, 7), -1);
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 160 + 40, 7), -1);
    // (and that is the end of the same thing begun later, however the next one fails)
    ASSERT_EQ(Bot::Lookahead::Failed(stretch, 500, 99), -1);

    // One that failed elsewhere, or two frames sooner after it began: it does depend on when, and the count
    // is from the next one (the first try, which the bot played itself, often fails unlike the rest)
    Bot::Lookahead::Stretch elsewhere;
    Bot::Lookahead::Stretch sooner;
    for (s32 i = 0; i < 5; i++)
    {
        ASSERT_EQ(Bot::Lookahead::Failed(elsewhere, elsewhere.mDelay + 40, i == 2 ? 8 : 7), 32 * (i + 1));
        ASSERT_EQ(Bot::Lookahead::Failed(sooner, sooner.mDelay + (i == 2 ? 38 : 40), 7), 32 * (i + 1));
    }
    // (the sixth try is only the third alike in a row: the ninth is the sixth)
    for (s32 i = 5; i < 8; i++)
    {
        ASSERT_EQ(Bot::Lookahead::Failed(elsewhere, elsewhere.mDelay + 40, 7), 32 * (i + 1));
        ASSERT_EQ(Bot::Lookahead::Failed(sooner, sooner.mDelay + 40, 7), 32 * (i + 1));
    }
    ASSERT_EQ(Bot::Lookahead::Failed(elsewhere, elsewhere.mDelay + 40, 7), -1);
    ASSERT_EQ(Bot::Lookahead::Failed(sooner, sooner.mDelay + 40, 7), -1);

    // Hurt while it still held back says nothing about what it would have done
    Bot::Lookahead::Stretch held;
    for (s32 i = 0; i < 5; i++)
    {
        ASSERT_GE(Bot::Lookahead::Failed(held, held.mDelay + 40, 7), 0);
    }
    ASSERT_EQ(held.mDelay, 160);
    ASSERT_EQ(Bot::Lookahead::Failed(held, 150, 7), -1);
    ASSERT_EQ(held.mAlike, 5);
    ASSERT_EQ(held.mCeiling, 150);

    // Nor do tries that did something else count with these
    Bot::Lookahead::Stretch other;
    for (s32 i = 0; i < 5; i++)
    {
        ASSERT_GE(Bot::Lookahead::Failed(other, other.mDelay + 40, 7), 0);
    }
    ASSERT_EQ(Bot::Lookahead::NextWay(other), 32);
    ASSERT_EQ(Bot::Lookahead::Failed(other, 32 + 40, 7), 64);

    // Begun again with something ruled out, it is counted anew
    Bot::Lookahead::Stretch again;
    for (s32 i = 0; i < 5; i++)
    {
        ASSERT_GE(Bot::Lookahead::Failed(again, again.mDelay + 40, 7), 0);
    }
    ASSERT_EQ(Bot::Lookahead::Again(again), 0);
    ASSERT_EQ(Bot::Lookahead::Failed(again, 40, 7), 32);
}

TEST(BotLookahead, WhenNoMomentGetsItThroughSomethingElseIsTriedAtEachMoment)
{
    using Way = Bot::Lookahead::Way;
    // Every try fails, each somewhere else: what is tried, in order
    Bot::Lookahead::Stretch stretch;
    std::vector<std::pair<Way, s32>> tried;
    s32 delay = FailedSomewhereElse(stretch, 1000);
    for (;;)
    {
        if (delay < 0)
        {
            delay = Bot::Lookahead::NextWay(stretch);
            if (delay < 0)
            {
                break;
            }
        }
        ASSERT_EQ(stretch.mDelay, delay);
        tried.push_back({stretch.mWay, delay});
        delay = FailedSomewhereElse(stretch, 1000);
    }
    const s32 coarse = Bot::Lookahead::kWindow / Bot::Lookahead::kCoarsest;
    // The same buttons and planned anew, at the coarse moments (not straight on again for either: the first try
    // was that); sneaked and run, straight on too; last the same buttons at the moments in between
    ASSERT_EQ(static_cast<s32>(tried.size()), (coarse - 1) * 2 + coarse * 2 + Bot::Lookahead::Count() - coarse);
    ASSERT_EQ(tried[0], (std::pair<Way, s32>{Way::eSame, 32}));
    ASSERT_EQ(tried[coarse - 2], (std::pair<Way, s32>{Way::eSame, 288}));
    ASSERT_EQ(tried[coarse - 1], (std::pair<Way, s32>{Way::eAgain, 32}));
    ASSERT_EQ(tried[2 * coarse - 2], (std::pair<Way, s32>{Way::eQuiet, 0}));
    ASSERT_EQ(tried[3 * coarse - 3], (std::pair<Way, s32>{Way::eQuiet, 288}));
    ASSERT_EQ(tried[3 * coarse - 2], (std::pair<Way, s32>{Way::eFast, 0}));
    ASSERT_EQ(tried[4 * coarse - 2], (std::pair<Way, s32>{Way::eFine, 16}));
    ASSERT_EQ(tried.back(), (std::pair<Way, s32>{Way::eFine, 312}));
    ASSERT_EQ(Bot::Lookahead::NextWay(stretch), -1);
    // Only the tries with the same buttons press what the first try pressed
    ASSERT_TRUE(Bot::Lookahead::Repeats(Way::eSame));
    ASSERT_TRUE(Bot::Lookahead::Repeats(Way::eFine));
    ASSERT_FALSE(Bot::Lookahead::Repeats(Way::eAgain));
    ASSERT_FALSE(Bot::Lookahead::Repeats(Way::eQuiet));
    ASSERT_FALSE(Bot::Lookahead::Repeats(Way::eFast));
}

// One frame of a flying Slig that is flown, as FlierFlight has it: its speed changed by what is held, then moved
static void FlyAFrame(s32& x, s32& y, s32& velX, s32& velY, u32 held)
{
    Bot::FlierFlight::Step(velX, velY, held);
    x += velX;
    y += velY;
}

TEST(BotFlierFlight, AHeldDirectionSpeedsItUpAndTheFasterItGoesTheMoreItLoses)
{
    s32 velX = 0;
    s32 velY = 0;
    // From rest: 0.7 of a pixel a frame more each frame, and nothing lost under one pixel a frame
    Bot::FlierFlight::Step(velX, velY, Bot::Btn::eRight);
    ASSERT_EQ(velX, 70);
    Bot::FlierFlight::Step(velX, velY, Bot::Btn::eRight);
    ASSERT_EQ(velX, 140);
    // At one whole pixel a frame it loses a quarter of its speed, at two an eighth
    Bot::FlierFlight::Step(velX, velY, Bot::Btn::eRight);
    ASSERT_EQ(velX, 140 + 70 - 35);
    Bot::FlierFlight::Step(velX, velY, Bot::Btn::eRight);
    ASSERT_EQ(velX, 175 + 70 - 43);
    Bot::FlierFlight::Step(velX, velY, Bot::Btn::eRight);
    ASSERT_EQ(velX, 202 + 70 - 25);
    ASSERT_EQ(velY, 0);
    // Held on, it never gets to six pixels a frame: there it is cut to 5.5
    s32 top = 0;
    for (s32 i = 0; i < 100; i++)
    {
        Bot::FlierFlight::Step(velX, velY, Bot::Btn::eRight);
        top = std::max(top, velX);
    }
    ASSERT_GE(top, 550);
    ASSERT_LT(top, 600);
    // The other way it slows down faster than it sped up, and then goes that way
    const s32 before = velX;
    Bot::FlierFlight::Step(velX, velY, Bot::Btn::eLeft);
    ASSERT_LT(velX, before - 70);
    for (s32 i = 0; i < 100; i++)
    {
        Bot::FlierFlight::Step(velX, velY, Bot::Btn::eLeft);
    }
    ASSERT_LE(velX, -500);
    ASSERT_GT(velX, -600);
}

TEST(BotFlierFlight, UpAndDownGoWithLeftAndRightAtSevenTenthsEachAndUpAndLeftWin)
{
    const s32 diagonal = Bot::FlierFlight::kSpeedUp * 707 / 1000;
    struct Case final
    {
        u32 mHeld;
        s32 mX;
        s32 mY;
    };
    const Case cases[] = {
        {Bot::Btn::eUp, 0, -70},
        {Bot::Btn::eDown, 0, 70},
        {Bot::Btn::eLeft, -70, 0},
        {Bot::Btn::eRight, 70, 0},
        {Bot::Btn::eUp | Bot::Btn::eLeft, -diagonal, -diagonal},
        {Bot::Btn::eUp | Bot::Btn::eRight, diagonal, -diagonal},
        {Bot::Btn::eDown | Bot::Btn::eLeft, -diagonal, diagonal},
        {Bot::Btn::eDown | Bot::Btn::eRight, diagonal, diagonal},
        // (both of a pair held: the game asks for up before down and left before right)
        {Bot::Btn::eUp | Bot::Btn::eDown, 0, -70},
        {Bot::Btn::eLeft | Bot::Btn::eRight, -70, 0},
        {0, 0, 0},
    };
    for (const Case& c : cases)
    {
        s32 velX = 0;
        s32 velY = 0;
        Bot::FlierFlight::Step(velX, velY, c.mHeld);
        ASSERT_EQ(velX, c.mX) << c.mHeld;
        ASSERT_EQ(velY, c.mY) << c.mHeld;
    }
}

TEST(BotFlierFlight, LetGoItStopsDeadUnderAPixelAFrameAndCoastsFromOver)
{
    s32 velX = 90;
    s32 velY = -30;
    Bot::FlierFlight::Step(velX, velY, 0);
    ASSERT_EQ(velX, 0);
    ASSERT_EQ(velY, 0);
    s32 dx = 1;
    s32 dy = 1;
    Bot::FlierFlight::Rest(90, -30, dx, dy);
    ASSERT_EQ(dx, 0);
    ASSERT_EQ(dy, 0);

    // From its top speed: what Rest says is what a flight with nothing held comes to
    velX = 550;
    velY = 0;
    Bot::FlierFlight::Rest(velX, velY, dx, dy);
    s32 x = 0;
    s32 y = 0;
    s32 frames = 0;
    while (velX != 0 || velY != 0)
    {
        FlyAFrame(x, y, velX, velY, 0);
        frames++;
    }
    ASSERT_EQ(dx, x);
    ASSERT_EQ(dy, 0);
    // (about twenty frames and fifty pixels)
    ASSERT_GT(frames, 12);
    ASSERT_LT(frames, 30);
    ASSERT_GT(x, 3000);
    ASSERT_LT(x, 7000);
    // And up and to the left as far the other way
    Bot::FlierFlight::Rest(-389, -389, dx, dy);
    ASSERT_LT(dx, -2000);
    ASSERT_EQ(dx, dy);
}

TEST(BotFlierFlight, SteeredItComesToRestAtThePlaceAskedFor)
{
    struct Case final
    {
        s32 mX;
        s32 mY;
        s32 mFrames;
    };
    // (pixels from where it starts, and the frames it may take)
    const Case cases[] = {{200, -150, 90}, {-300, 0, 90}, {0, 260, 80}, {3, 0, 12}, {-40, 35, 40}, {0, 0, 2}};
    for (const Case& c : cases)
    {
        s32 x = 100000;
        s32 y = 50000;
        s32 velX = 0;
        s32 velY = 0;
        const s32 tx = 1000 + c.mX;
        const s32 ty = 500 + c.mY;
        s32 there = -1;
        for (s32 frame = 0; frame < 200; frame++)
        {
            FlyAFrame(x, y, velX, velY, Bot::FlierFlight::Steer(x, y, velX, velY, tx, ty));
            const bool at = velX == 0 && velY == 0 && std::abs(x - tx * 100) <= Bot::FlierFlight::kNear && std::abs(y - ty * 100) <= Bot::FlierFlight::kNear;
            if (at && there < 0)
            {
                there = frame;
            }
            // (and there it stays)
            ASSERT_TRUE(at || there < 0) << c.mX << "," << c.mY << " left at " << frame;
            // It lets go in time: never more than three pixels past the place
            ASSERT_LE((x - tx * 100) * (c.mX >= 0 ? 1 : -1), 300) << c.mX << "," << c.mY << " at " << frame;
            ASSERT_LE((y - ty * 100) * (c.mY >= 0 ? 1 : -1), 300) << c.mX << "," << c.mY << " at " << frame;
        }
        ASSERT_GE(there, 0) << c.mX << "," << c.mY;
        ASSERT_LE(there, c.mFrames) << c.mX << "," << c.mY;
        ASSERT_EQ(velX, 0);
        ASSERT_EQ(velY, 0);
        ASSERT_LE(std::abs(x - tx * 100), Bot::FlierFlight::kNear);
        ASSERT_LE(std::abs(y - ty * 100), Bot::FlierFlight::kNear);
    }
}

TEST(BotFlierFlight, ItsGrenadeRisesALittleAndComesDownFurtherOffTheFurtherItFalls)
{
    // Thrown 5 over the Slig's point at 6 up, a pixel a frame slower each frame: level with the Slig again on
    // the twelfth frame, 90 under it on the twenty-first
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(0), 12);
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(1), 12);
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(2), 13);
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(90), 21);
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(100), 21);
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(101), 22);
    // (5.7 pixels a frame along)
    ASSERT_EQ(Bot::FlierFlight::GrenadeReach(90), 21 * 570 / 100);
    ASSERT_EQ(Bot::FlierFlight::GrenadeReach(0), 12 * 570 / 100);
}

TEST(BotFlierFlight, ItThrowsFromOffTheSligsLevelToEitherSideSoThatTheGrenadeComesDownOnIt)
{
    // A Slig in the middle of a wide floor: from over its head only
    const std::vector<Bot::FlierFlight::Shot> shots = Bot::FlierFlight::Shots(1000, 700, 800, 1200);
    ASSERT_EQ(shots.size(), 6u);
    for (std::size_t i = 0; i < shots.size(); i++)
    {
        const Bot::FlierFlight::Shot& shot = shots[i];
        // Over the Slig's head, facing it, as far to the side as the grenade goes while it falls to the floor
        ASSERT_LE(shot.mY, 700 - Bot::FlierFlight::kOver);
        ASSERT_EQ(shot.mFaceLeft, shot.mX > 1000);
        const s32 reach = Bot::FlierFlight::GrenadeReach(700 - shot.mY);
        ASSERT_EQ(shot.mX + (shot.mFaceLeft ? -reach : reach), 1000);
        ASSERT_EQ(shot.mLandX, 1000);
        ASSERT_EQ(shot.mFrames, Bot::FlierFlight::GrenadeFrames(700 - shot.mY));
        // (from the right, then from the left, at each height: the lowest first)
        ASSERT_EQ(shot.mFaceLeft, i % 2 == 0);
        ASSERT_EQ(shot.mY, 700 - Bot::FlierFlight::kOver - 20 * static_cast<s32>(i / 2));
    }
}

TEST(BotFlierFlight, NearAnEndOfItsFloorTheGrenadeIsFirstThrownUpOverThatEndFromUnderIt)
{
    // The grenade is over a floor 12 over the Slig's point from its fourth frame, and comes down on it on the
    // tenth
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(-Bot::FlierFlight::kUnder), 10);
    // (20 over the Slig's point is as high as it gets, on its fifth and sixth frames: down from there on the
    // seventh)
    ASSERT_EQ(Bot::FlierFlight::GrenadeFrames(-20), 7);
    // A Slig on a ledge from 950 to 1040, near both ends: from under the right end, then the left, then from
    // over its head
    const std::vector<Bot::FlierFlight::Shot> shots = Bot::FlierFlight::Shots(1000, 700, 950, 1040);
    ASSERT_EQ(shots.size(), 8u);
    ASSERT_EQ(shots[0].mX, 1040 + Bot::FlierFlight::kOut);
    ASSERT_EQ(shots[0].mY, 700 + Bot::FlierFlight::kUnder);
    ASSERT_TRUE(shots[0].mFaceLeft);
    ASSERT_EQ(shots[0].mFrames, 10);
    // (57 pixels along in ten frames: 17 in from the end)
    ASSERT_EQ(shots[0].mLandX, 1040 + Bot::FlierFlight::kOut - 57);
    ASSERT_EQ(shots[1].mX, 950 - Bot::FlierFlight::kOut);
    ASSERT_EQ(shots[1].mY, 700 + Bot::FlierFlight::kUnder);
    ASSERT_FALSE(shots[1].mFaceLeft);
    ASSERT_EQ(shots[1].mLandX, 950 - Bot::FlierFlight::kOut + 57);
    ASSERT_EQ(shots[2].mY, 700 - Bot::FlierFlight::kOver);
    // From under the floor it may throw from nearer, up to 20 out from the end: its side is 17 from its point.
    // From over the Slig's head there is one place.
    ASSERT_EQ(shots[0].mNearer, Bot::FlierFlight::kOut - Bot::FlierFlight::kOutLeast);
    ASSERT_EQ(shots[1].mNearer, 20);
    ASSERT_EQ(shots[2].mNearer, 0);
    ASSERT_EQ(shots[0].mWaitX, shots[0].mX);
    ASSERT_EQ(shots[0].mWaitY, 700 + Bot::FlierFlight::kWaitUnder);
    ASSERT_EQ(shots[2].mWaitX, shots[2].mX);
    ASSERT_EQ(shots[2].mWaitY, shots[2].mY);

    // Only the end it is near (90 at most)
    ASSERT_EQ(Bot::FlierFlight::Shots(1000, 700, 900, 1090).size(), 7u);
    ASSERT_TRUE(Bot::FlierFlight::Shots(1000, 700, 900, 1090)[0].mFaceLeft);
    ASSERT_EQ(Bot::FlierFlight::Shots(1000, 700, 910, 1091).size(), 7u);
    ASSERT_FALSE(Bot::FlierFlight::Shots(1000, 700, 910, 1091)[0].mFaceLeft);
    ASSERT_EQ(Bot::FlierFlight::Shots(1000, 700, 909, 1091).size(), 6u);
    // And not onto a floor too short for the grenade to come down on
    ASSERT_EQ(Bot::FlierFlight::Shots(1000, 700, 990, 1006).size(), 6u);
    ASSERT_EQ(Bot::FlierFlight::Shots(1000, 700, 990, 1007).size(), 8u);
}

// A column of three screens (375 by 260 each): a floor along the bottom one, where a flying Slig spawner is, and
// on the middle one a ledge from 0 to 240 at 440 with a Slig at 200 and a security orb
static NavInput FlierColumn()
{
    NavInput in;
    in.mLines.push_back(Line(0, 700, 375, 700, 0));
    in.mLines.push_back(Line(0, 440, 240, 440, 0));
    in.mLines.push_back(Line(300, 100, 375, 100, 0));
    return in;
}

static World FlierColumnWorld()
{
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eFlierSpawner, 2001, 188, 600, 212, 624));
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 1001, 188, 420, 212, 444));
    world.mEntities.push_back(MakeEntity(EntityKind::eSecurityOrb, 1002, 300, 300, 324, 324));
    return world;
}

TEST(BotFlierTargets, NothingIsPlannedWithAFlyingSligOfASpawnerThatGaveNoneWhenWaitedFor)
{
    NavGraph g;
    g.Build(FlierColumn());
    World world = FlierColumnWorld();
    const auto fliers = [&g](const World& w)
    {
        const Domain domain = Domain::Build(g, w, g.NodeAt(12, 700, false), nullptr);
        s32 count = 0;
        for (const DomainStep& step : domain.mSteps)
        {
            count += step.mKind == StepKind::eFlierBomb || step.mKind == StepKind::eFlierPull || step.mKind == StepKind::eFlierLock ? 1 : 0;
        }
        return count;
    };
    EXPECT_GT(fliers(world), 0);
    world.mNoFlier.insert(2001);
    EXPECT_EQ(fliers(world), 0);
    // (another spawner's is none of that one's)
    world.mNoFlier.clear();
    world.mNoFlier.insert(2002);
    EXPECT_GT(fliers(world), 0);
}

TEST(BotFlierTargets, ASligUnderASecurityOrbIsOneForTheFlyingSligWithThePlacesToThrowFrom)
{
    const NavInput in = FlierColumn();
    World world = FlierColumnWorld();
    const std::vector<Domain::FlierTarget> targets = Domain::FlierTargets(in, world, world.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].mSlig, 1001u);
    EXPECT_EQ(targets[0].mX, 200);
    EXPECT_EQ(targets[0].mFloorY, 440);
    EXPECT_TRUE(targets[0].mNeeds.empty());
    // From under the right end of its ledge (it is 40 from it; the left end is too far), then from over its head
    // to either side at three heights
    ASSERT_EQ(targets[0].mShots.size(), 7u);
    EXPECT_EQ(targets[0].mShots[0].mX, 240 + FlierFlight::kOut);
    EXPECT_EQ(targets[0].mShots[0].mY, 440 + FlierFlight::kUnder);
    EXPECT_TRUE(targets[0].mShots[0].mFaceLeft);
    EXPECT_EQ(targets[0].mShots[1].mY, 440 - FlierFlight::kOver);

    // Not one he can take over himself (no orb on its screen), one that is dead, one the grenades did not get,
    // or one further from the spawner than a flight
    World noOrb = FlierColumnWorld();
    noOrb.mEntities[2].mY1 = 30;
    noOrb.mEntities[2].mY2 = 54;
    EXPECT_TRUE(Domain::FlierTargets(in, noOrb, noOrb.mEntities[0]).empty());
    World gone = FlierColumnWorld();
    gone.mEntities[1].mGone = true;
    EXPECT_TRUE(Domain::FlierTargets(in, gone, gone.mEntities[0]).empty());
    World killed = FlierColumnWorld();
    killed.mFleechKilled.insert(1001);
    EXPECT_TRUE(Domain::FlierTargets(in, killed, killed.mEntities[0]).empty());
    World given = FlierColumnWorld();
    given.mNoBomb.insert(1001);
    EXPECT_TRUE(Domain::FlierTargets(in, given, given.mEntities[0]).empty());
    World far = FlierColumnWorld();
    far.mEntities[0].mX1 += 1100;
    far.mEntities[0].mX2 += 1100;
    EXPECT_TRUE(Domain::FlierTargets(in, far, far.mEntities[0]).empty());
    // (a Slig a spawner makes is not there to be thrown at)
    World spawned = FlierColumnWorld();
    spawned.mEntities[1].mSwitch = 40;
    EXPECT_TRUE(Domain::FlierTargets(in, spawned, spawned.mEntities[0]).empty());
}

TEST(BotFlierTargets, AGrenadeIsNotThrownUpUnderATrapDoorThatIsShut)
{
    // A trap door from the end of the ledge on: the grenade thrown from under the end would come up under it
    NavInput in = FlierColumn();
    NavFeature door;
    door.mKind = FeatureKind::eTrapDoor;
    door.mX1 = 240;
    door.mY1 = 440;
    door.mX2 = 300;
    door.mY2 = 460;
    door.mSwitch = 50;
    door.mStartClosed = true;
    in.mFeatures.push_back(door);
    World world = FlierColumnWorld();
    std::vector<Domain::FlierTarget> targets = Domain::FlierTargets(in, world, world.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    for (const FlierFlight::Shot& shot : targets[0].mShots)
    {
        EXPECT_LE(shot.mY, 440 - FlierFlight::kOver);
    }
    EXPECT_EQ(targets[0].mShots.size(), 6u);
    // Open, it is
    world.mSwitches[50] = 1;
    targets = Domain::FlierTargets(in, world, world.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    ASSERT_EQ(targets[0].mShots.size(), 7u);
    EXPECT_EQ(targets[0].mShots[0].mY, 440 + FlierFlight::kUnder);
}

TEST(BotFlierTargets, WithAnElectricWallByTheOnlyPlaceToThrowFromTheWallIsToBeOffFirst)
{
    // A ceiling over the ledge's screen keeps the flying Slig from over the Slig's head: the place under the end
    // of the ledge is the only one. An electric wall stands 20 from it.
    NavInput in = FlierColumn();
    in.mLines.push_back(Line(0, 400, 375, 400, 3));
    NavFeature wall;
    wall.mKind = FeatureKind::eBarrier;
    wall.mX1 = 288;
    wall.mY1 = 440;
    wall.mX2 = 312;
    wall.mY2 = 520;
    wall.mSwitch = 50;
    wall.mStartClosed = true;
    in.mFeatures.push_back(wall);
    World world = FlierColumnWorld();
    std::vector<Domain::FlierTarget> targets = Domain::FlierTargets(in, world, world.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_TRUE(targets[0].mShots.empty());
    ASSERT_EQ(targets[0].mNeeds.size(), 1u);
    EXPECT_EQ(targets[0].mNeeds[0], (std::pair<s32, s32>{50, 1}));
    // Off: thrown from there as things are
    world.mSwitches[50] = 1;
    targets = Domain::FlierTargets(in, world, world.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].mShots.size(), 1u);
    EXPECT_TRUE(targets[0].mNeeds.empty());
    // A wall no switch turns off: no place at all, and the Slig is not one for the flying Slig
    in.mFeatures[0].mSwitch = 0;
    world.mSwitches[50] = 0;
    EXPECT_TRUE(Domain::FlierTargets(in, world, world.mEntities[0]).empty());
    // And one far from the place is nothing to it
    in.mFeatures[0].mSwitch = 50;
    in.mFeatures[0].mX1 = 340;
    in.mFeatures[0].mX2 = 364;
    targets = Domain::FlierTargets(in, world, world.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].mShots.size(), 1u);
}

TEST(BotFlierTargets, AWayThroughTheAirKeepsToTheScreensThePathHas)
{
    const NavInput in = FlierColumn();
    std::vector<std::pair<s32, s32>> corners;
    // Up the column, round the end of the ledge
    ASSERT_TRUE(Domain::FlierWay(in, false, 200, 600, 100, 300, nullptr, corners));
    ASSERT_FALSE(corners.empty());
    EXPECT_EQ(corners.back(), (std::pair<s32, s32>{100, 300}));
    // To the screen beside the bottom one, where the path has nothing: no
    EXPECT_FALSE(Domain::FlierWay(in, false, 200, 600, 500, 600, nullptr, corners));
    EXPECT_TRUE(corners.empty());
    // With something of the path there, it does
    NavInput wider = in;
    wider.mLines.push_back(Line(375, 700, 750, 700, 0));
    EXPECT_TRUE(Domain::FlierWay(wider, false, 200, 600, 500, 600, nullptr, corners));
    // Not through what is shut
    const std::vector<NavLine> shut{Line(300, 500, 300, 700, 1)};
    EXPECT_FALSE(Domain::FlierWay(wider, false, 200, 600, 500, 600, &shut, corners));
}

TEST(BotFlierFlight, ALeverIsPulledFromACellAndABitToEitherSideOfItFacingIt)
{
    // The lever stands at 2137 on the floor at 460. From its left the flying Slig faces right, 46 from it and 23
    // over the floor; from its right it faces left. It waits where it pulls from.
    const Bot::FlierFlight::Shot left = Bot::FlierFlight::LeverPull(2137, 460, false);
    EXPECT_EQ(left.mX, 2137 - Bot::FlierFlight::kLeverAlong);
    EXPECT_EQ(left.mY, 460 - Bot::FlierFlight::kLeverOver);
    EXPECT_FALSE(left.mFaceLeft);
    EXPECT_EQ(left.mWaitX, left.mX);
    EXPECT_EQ(left.mWaitY, left.mY);
    EXPECT_EQ(left.mNearer, 0);
    const Bot::FlierFlight::Shot right = Bot::FlierFlight::LeverPull(2137, 460, true);
    EXPECT_EQ(right.mX, 2137 + 46);
    EXPECT_EQ(right.mY, 437);
    EXPECT_TRUE(right.mFaceLeft);
    EXPECT_EQ(right.mWaitX, right.mX);
    EXPECT_EQ(right.mWaitY, right.mY);
}

TEST(BotFlierTargets, AWayThroughTheAirKeepsTheFlyingSligsSidesClearOfTheEndsOfFloors)
{
    // A room from 400 to 775 with a ledge from 400 to 600 at 300
    NavInput in;
    in.mLines.push_back(Line(400, 500, 775, 500, 0));
    in.mLines.push_back(Line(400, 20, 775, 20, 3));
    in.mLines.push_back(Line(400, 20, 400, 500, 1));
    in.mLines.push_back(Line(775, 20, 775, 500, 2));
    in.mLines.push_back(Line(400, 300, 600, 300, 0));
    std::vector<std::pair<s32, s32>> corners;
    // Straight up five pixels past the end of the ledge its point is clear of it and its side is not: round, then
    ASSERT_TRUE(Domain::FlierWay(in, false, 605, 400, 605, 200, nullptr, corners));
    EXPECT_GT(corners.size(), 1u);
    s32 out = 0;
    for (const std::pair<s32, s32>& corner : corners)
    {
        out = std::max(out, corner.first);
    }
    EXPECT_GE(out, 620);
    // (25 past it, it goes straight up)
    ASSERT_TRUE(Domain::FlierWay(in, false, 625, 400, 625, 200, nullptr, corners));
    EXPECT_EQ(corners.size(), 1u);

    // With a floor 20 under the ledge there is no flying in under the ledge from beside it: it is 31 high
    in.mLines.push_back(Line(400, 320, 775, 320, 0));
    EXPECT_FALSE(Domain::FlierWay(in, false, 700, 310, 560, 310, nullptr, corners));
    // (along beside the ledge's end at that height it does fly)
    EXPECT_TRUE(Domain::FlierWay(in, false, 700, 310, 640, 310, nullptr, corners));
}

TEST(BotFlierTargets, APlaceToThrowFromIsOneItCanGetUpToAndOnTheSligsOwnScreen)
{
    // A floor from 250 to 300 at 460, between where it would wait under the end of the Slig's ledge (476) and
    // where it would throw from (452): from over the Slig's head only
    NavInput in = FlierColumn();
    in.mLines.push_back(Line(250, 460, 300, 460, 0));
    World world = FlierColumnWorld();
    std::vector<Domain::FlierTarget> targets = Domain::FlierTargets(in, world, world.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].mShots.size(), 6u);
    for (const FlierFlight::Shot& shot : targets[0].mShots)
    {
        EXPECT_LE(shot.mY, 440 - FlierFlight::kOver);
    }

    // A Slig on a ledge at 300, 40 under the top of its screen: over its head is the next screen, where a
    // grenade thrown finds no Slig. From under the end of the ledge only.
    NavInput high;
    high.mLines.push_back(Line(0, 700, 375, 700, 0));
    high.mLines.push_back(Line(0, 300, 240, 300, 0));
    high.mLines.push_back(Line(300, 100, 375, 100, 0));
    World top = FlierColumnWorld();
    top.mEntities[1].mY1 = 280;
    top.mEntities[1].mY2 = 304;
    targets = Domain::FlierTargets(high, top, top.mEntities[0]);
    ASSERT_EQ(targets.size(), 1u);
    ASSERT_EQ(targets[0].mShots.size(), 1u);
    EXPECT_EQ(targets[0].mShots[0].mY, 300 + FlierFlight::kUnder);
}

// The Barracks' boom machine rooms (path 1, screen 12): the ground with its shadows, the middle ledge and the
// top floor with a Slig each, the short platform by the door, and the machine a screen to the left
static NavInput BoomRooms()
{
    NavInput in;
    in.mLines.push_back(Line(25, 1240, 706, 1240, 0));
    in.mLines.push_back(Line(450, 1180, 525, 1180, 0));
    in.mLines.push_back(Line(499, 1120, 725, 1120, 0));
    in.mLines.push_back(Line(601, 1179, 651, 1179, 0));
    in.mLines.push_back(Line(700, 1040, 700, 1260, 2));
    NavFeature shadow;
    shadow.mKind = FeatureKind::eInvisibleZone;
    shadow.mX1 = 349;
    shadow.mY1 = 1178;
    shadow.mX2 = 467;
    shadow.mY2 = 1248;
    in.mFeatures.push_back(shadow);
    return in;
}

static World BoomRoomsWorld()
{
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eRockSack, 13002, 75, 1208, 99, 1244));
    world.mEntities.back().mInputs[1] = 2;
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 12007, 550, 1100, 575, 1120));
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 12006, 476, 1160, 500, 1184));
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 12005, 525, 1220, 550, 1240));
    world.mEntities.push_back(MakeEntity(EntityKind::eSecurityOrb, 12020, 451, 1099, 475, 1123));
    return world;
}

TEST(BotGrenadeTargets, ASligOnAFloorWithNoShadowUnderAnOrbIsOneForAGrenadeFromWhereItComesToLieOnItsFloor)
{
    const NavInput in = BoomRooms();
    const World world = BoomRoomsWorld();
    const std::vector<Domain::GrenadeTarget> targets = Domain::GrenadeTargets(in, world);
    // Not the one on the ground: there are shadows to go by it in
    ASSERT_EQ(targets.size(), 2u);
    EXPECT_EQ(targets[0].mSlig, 12007u);
    EXPECT_EQ(targets[0].mFloorY, 1120);
    EXPECT_EQ(targets[0].mX1, 499);
    EXPECT_EQ(targets[0].mX2, 725);
    // The top floor: from the left end of the middle ledge only (from under the floor it would come back down),
    // lobbed, thrown up or thrown far (it lies where it first comes down: AE's does not bounce on)
    ASSERT_EQ(targets[0].mShots.size(), 3u);
    for (const Domain::GrenadeShot& shot : targets[0].mShots)
    {
        EXPECT_EQ(shot.mX, 462);
        EXPECT_EQ(shot.mY, 1180);
        EXPECT_FALSE(shot.mFaceLeft);
        EXPECT_GE(shot.mLandX, 499);
        EXPECT_GE(shot.mLandX - shot.mX, static_cast<s32>(ThrowArc::kGrenadeKillX));
    }
    EXPECT_EQ(targets[0].mShots[0].mKind, ThrowSpot::eLob);
    EXPECT_EQ(targets[0].mShots[1].mKind, ThrowSpot::eUp);
    EXPECT_EQ(targets[0].mShots[2].mKind, ThrowSpot::eForward);
    // The middle ledge: from the shadow on the ground first
    EXPECT_EQ(targets[1].mSlig, 12006u);
    ASSERT_FALSE(targets[1].mShots.empty());
    EXPECT_TRUE(targets[1].mShots[0].mHidden);
    EXPECT_EQ(targets[1].mShots[0].mX, 387);
    EXPECT_EQ(targets[1].mShots[0].mY, 1240);
    EXPECT_FALSE(targets[1].mShots.back().mHidden);
    for (std::size_t i = 1; i < targets[1].mShots.size(); i++)
    {
        // (the ones from a shadow first, and all of those from there)
        EXPECT_TRUE(targets[1].mShots[i - 1].mHidden || !targets[1].mShots[i].mHidden);
        EXPECT_EQ(targets[1].mShots[i].mHidden, targets[1].mShots[i].mY == 1240 && targets[1].mShots[i].mX - 25 >= 349 && targets[1].mShots[i].mX + 25 <= 467);
        EXPECT_GE(targets[1].mShots[i].mLandX, 450);
        EXPECT_LE(targets[1].mShots[i].mLandX, 525);
    }

    // None with no machine in the path, with no orb on the Slig's screen (he takes it over), in AO, or of a
    // Slig that is dead
    World noMachine = BoomRoomsWorld();
    noMachine.mEntities[0].mInputs[1] = 0;
    EXPECT_TRUE(Domain::GrenadeTargets(in, noMachine).empty());
    World noOrb = BoomRoomsWorld();
    noOrb.mEntities[4].mY1 -= 260;
    EXPECT_TRUE(Domain::GrenadeTargets(in, noOrb).empty());
    NavInput ao = BoomRooms();
    ao.mIsAe = false;
    EXPECT_TRUE(Domain::GrenadeTargets(ao, world).empty());
    World dead = BoomRoomsWorld();
    dead.mEntities[1].mGone = true;
    const std::vector<Domain::GrenadeTarget> left = Domain::GrenadeTargets(in, dead);
    ASSERT_EQ(left.size(), 1u);
    EXPECT_EQ(left[0].mSlig, 12006u);
    // With a shadow on the middle ledge its Slig is gone by unseen
    NavInput dark = BoomRooms();
    dark.mFeatures[0].mY1 = 1100;
    dark.mFeatures[0].mX2 = 500;
    const std::vector<Domain::GrenadeTarget> lit = Domain::GrenadeTargets(dark, world);
    ASSERT_EQ(lit.size(), 1u);
    EXPECT_EQ(lit[0].mSlig, 12007u);
}

TEST(BotGrenadeTargets, AnOrbIsGotByAGrenadeHeldUntilItGoesOffInTheAirByIt)
{
    // The Barracks' path 7, its top left: a floor at 100 with an orb at 450,40 over it. A grenade that lay on
    // that floor under it went off and the orb was still there.
    EXPECT_FALSE(Domain::BlastGetsOrb(435, 100, 450, 40, false));
    EXPECT_TRUE(Domain::BlastGetsOrb(435, 70, 450, 40, false));
    EXPECT_TRUE(Domain::BlastGetsOrb(450, 30, 450, 40, false));
    EXPECT_TRUE(Domain::BlastGetsOrb(450, -10, 450, 40, false));
    EXPECT_FALSE(Domain::BlastGetsOrb(450, -11, 450, 40, false));
    EXPECT_TRUE(Domain::BlastGetsOrb(435, 90, 450, 40, false));
    EXPECT_FALSE(Domain::BlastGetsOrb(435, 91, 450, 40, false));
    EXPECT_FALSE(Domain::BlastGetsOrb(495, 60, 450, 40, false));
    // (at half scale all of it is half as far)
    EXPECT_FALSE(Domain::BlastGetsOrb(475, 60, 450, 40, true));
    EXPECT_TRUE(Domain::BlastGetsOrb(465, 50, 450, 40, true));

    NavInput in;
    in.mLines.push_back(Line(300, 100, 700, 100, 0));
    NavFeature shadow;
    shadow.mKind = FeatureKind::eInvisibleZone;
    shadow.mX1 = 525;
    shadow.mY1 = 40;
    shadow.mX2 = 600;
    shadow.mY2 = 104;
    in.mFeatures.push_back(shadow);
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eRockSack, 13002, 675, 68, 699, 104));
    world.mEntities.back().mInputs[1] = 2;
    world.mEntities.push_back(MakeEntity(EntityKind::eSecurityOrb, 12020, 450, 40, 474, 64));
    const std::vector<Domain::OrbTarget> targets = Domain::OrbTargets(in, world);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].mOrb, 12020u);
    ASSERT_FALSE(targets[0].mShots.empty());
    // From the shadow first
    EXPECT_TRUE(targets[0].mShots[0].mHidden);
    EXPECT_EQ(targets[0].mShots[0].mX, 562);
    for (const Domain::OrbShot& shot : targets[0].mShots)
    {
        EXPECT_EQ(shot.mY, 100);
        // Held, and it goes off where the flight has it that many frames sooner: in the air by the orb, and not
        // by him
        EXPECT_GE(shot.mHold, 4);
        // (it comes back off the orb itself)
        const std::vector<ThrowArc::Box> off{ThrowArc::BoxOf(450, 40, false, world.mOrbBox)};
        const ThrowArc::Flight flight = ThrowArc::Grenade(in, shot.mX, shot.mY, false, shot.mFaceLeft, shot.mKind, ThrowArc::kFuse - shot.mHold, &off);
        EXPECT_EQ(flight.mPoints.size(), static_cast<std::size_t>(ThrowArc::kFuse - shot.mHold) + 1);
        EXPECT_EQ(flight.mPoints.back().mX, shot.mAtX);
        EXPECT_EQ(flight.mPoints.back().mY, shot.mAtY);
        EXPECT_LT(shot.mAtY, 100);
        EXPECT_TRUE(Domain::BlastGetsOrb(shot.mAtX, shot.mAtY, 450, 40, false));
        EXPECT_GE(std::abs(shot.mAtX - shot.mX), static_cast<s32>(ThrowArc::kGrenadeKillX));
        EXPECT_EQ(shot.mFaceLeft, shot.mX > 450);
        EXPECT_EQ(shot.mHidden, shot.mX == 562);
    }
    // Not from where he would be in the blast: an orb a hundred over a floor at 500, and a grenade lobbed from 83
    // to the side of it is by it 24 frames on, 91 over the floor
    NavInput low;
    low.mLines.push_back(Line(300, 500, 1000, 500, 0));
    World over = world;
    over.mEntities[1] = MakeEntity(EntityKind::eSecurityOrb, 12020, 600, 400, 624, 424);
    const ThrowArc::Flight lob = ThrowArc::Grenade(low, 512, 500, false, false, ThrowSpot::eLob, 24);
    EXPECT_TRUE(Domain::BlastGetsOrb(lob.mPoints.back().mX, lob.mPoints.back().mY, 600, 400, false));
    const std::vector<Domain::OrbTarget> far = Domain::OrbTargets(low, over);
    ASSERT_EQ(far.size(), 1u);
    ASSERT_FALSE(far[0].mShots.empty());
    // (nor from the floor of the screen under the orb's: the same, an orb at 200 and the floor at 300)
    World above = world;
    above.mEntities[1] = MakeEntity(EntityKind::eSecurityOrb, 12020, 600, 200, 624, 224);
    NavInput lower;
    lower.mLines.push_back(Line(300, 300, 1000, 300, 0));
    for (const Domain::OrbTarget& target : Domain::OrbTargets(lower, above))
    {
        EXPECT_TRUE(target.mShots.empty());
    }
    // (and from the orb's own screen, 375 to 750: off it the orb is not there to be blown up)
    for (const Domain::OrbShot& shot : far[0].mShots)
    {
        EXPECT_EQ(shot.mX / 375, 1);
    }
    for (const Domain::OrbShot& shot : far[0].mShots)
    {
        EXPECT_TRUE(std::abs(shot.mAtX - shot.mX) >= static_cast<s32>(ThrowArc::kGrenadeKillX) || std::abs(shot.mAtY - shot.mY) >= static_cast<s32>(ThrowArc::kGrenadeKillDown) + 30);
    }
    // None with no machine in the path, of an orb that is gone, or in AO
    World noMachine = world;
    noMachine.mEntities[0].mInputs[1] = 0;
    EXPECT_TRUE(Domain::OrbTargets(in, noMachine).empty());
    World gone = world;
    gone.mEntities[1].mGone = true;
    EXPECT_TRUE(Domain::OrbTargets(in, gone).empty());
    NavInput ao = in;
    ao.mIsAe = false;
    EXPECT_TRUE(Domain::OrbTargets(ao, world).empty());
}

TEST(BotGrenadeTargets, ASligWhoseFloorGoesOnUnderAnOrbsScreenAndTheOnesFromALockerAreOnesForAGrenade)
{
    // The Barracks' path 7, its top right: a ledge from one screen on into the next, which has the orb, and a
    // Slig that starts on the first of them: chanted at it runs off under the orb
    NavInput in;
    in.mLines.push_back(Line(987, 100, 1387, 100, 0));
    in.mLines.push_back(Line(537, 100, 887, 100, 0));
    in.mLines.push_back(Line(862, 200, 1462, 200, 0));
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eRockSack, 1000, 601, 68, 625, 104));
    world.mEntities.back().mInputs[1] = 2;
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 2006, 1012, 80, 1036, 104));
    world.mEntities.push_back(MakeEntity(EntityKind::eSecurityOrb, 3007, 1175, 40, 1199, 64));
    std::vector<Domain::GrenadeTarget> targets = Domain::GrenadeTargets(in, world);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].mSlig, 2006u);
    EXPECT_EQ(targets[0].mFloorY, 100);
    // (thrown onto the stretch on its own screen; all of its floor is told too)
    EXPECT_EQ(targets[0].mX1, 987);
    EXPECT_EQ(targets[0].mX2, 1125);
    EXPECT_EQ(targets[0].mLineX1, 987);
    EXPECT_EQ(targets[0].mLineX2, 1387);
    // Not with the orb a screen further on than its floor goes, on the screen under, or gone
    World further = world;
    further.mEntities[2] = MakeEntity(EntityKind::eSecurityOrb, 3007, 1550, 40, 1574, 64);
    EXPECT_TRUE(Domain::GrenadeTargets(in, further).empty());
    World under = world;
    under.mEntities[2] = MakeEntity(EntityKind::eSecurityOrb, 3007, 1175, 300, 1199, 324);
    EXPECT_TRUE(Domain::GrenadeTargets(in, under).empty());
    World gone = world;
    gone.mEntities[2].mGone = true;
    EXPECT_TRUE(Domain::GrenadeTargets(in, gone).empty());

    // A locker for guns on the floor under, with a Slig about by it that came by its gun there: the locker stands
    // for it, anywhere along that floor, with no orb
    World armed = gone;
    Entity locker = MakeEntity(EntityKind::ePantsLocker, 3004, 1150, 180, 1174, 204);
    locker.mSwitch = World::HazardSwitch(locker.mId);
    armed.mEntities.push_back(locker);
    EXPECT_TRUE(Domain::GrenadeTargets(in, armed).empty());
    SligAt slig;
    slig.mX = 1062;
    slig.mY = 200;
    armed.mSligs.push_back(slig);
    targets = Domain::GrenadeTargets(in, armed);
    ASSERT_EQ(targets.size(), 1u);
    EXPECT_EQ(targets[0].mSlig, 3004u);
    EXPECT_EQ(targets[0].mFloorY, 200);
    EXPECT_EQ(targets[0].mX1, 862);
    EXPECT_EQ(targets[0].mX2, 1462);
    // Not one that is his, asleep, on another floor or far off along it
    armed.mSligs[0].mPossessed = true;
    EXPECT_TRUE(Domain::GrenadeTargets(in, armed).empty());
    armed.mSligs[0].mPossessed = false;
    armed.mSligs[0].mAsleep = true;
    EXPECT_TRUE(Domain::GrenadeTargets(in, armed).empty());
    armed.mSligs[0].mAsleep = false;
    armed.mSligs[0].mY = 100;
    EXPECT_TRUE(Domain::GrenadeTargets(in, armed).empty());
    armed.mSligs[0].mY = 200;
    armed.mSligs[0].mX = 1162 + 601;
    EXPECT_TRUE(Domain::GrenadeTargets(in, armed).empty());
}

TEST(BotDomain, ANakedSligCanGetToAButtonOnItsFloorWithNothingShutBetween)
{
    NavInput in;
    in.mLines.push_back(Line(1100, 200, 1462, 200, 0));
    NavFeature door = Feature(FeatureKind::eBarrier, 1212, 160, 1224, 204);
    door.mSolid = true;
    door.mSwitch = 187;
    door.mStartClosed = true;
    in.mFeatures.push_back(door);
    World world;
    Entity button = MakeEntity(EntityKind::eSligButton, 3000, 1301, 180, 1325, 204);
    button.mSwitch = 187;
    world.mEntities.push_back(button);
    // The door shut: not through it, on either side of it yes
    EXPECT_FALSE(Domain::FloorOpen(in, world, 1162, 1312, 200, false));
    EXPECT_TRUE(Domain::FloorOpen(in, world, 1237, 1312, 200, false));
    EXPECT_TRUE(Domain::FloorOpen(in, world, 1112, 1187, 200, false));
    // (nor is one of the other scale, or on another level, in the way)
    EXPECT_TRUE(Domain::FloorOpen(in, world, 1162, 1312, 200, true));
    EXPECT_TRUE(Domain::FloorOpen(in, world, 1162, 1312, 100, false));
    // No naked Slig: none for the button
    EXPECT_FALSE(Domain::CrawlerFor(in, world, world.mEntities[0]));
    // One about beyond the door: none; one on the button's side: that one
    RockAt out;
    out.mX = 1162;
    out.mY = 200;
    world.mCrawlers.push_back(out);
    EXPECT_FALSE(Domain::CrawlerFor(in, world, world.mEntities[0]));
    RockAt pen;
    pen.mX = 1262;
    pen.mY = 200;
    world.mCrawlers.push_back(pen);
    s32 x = 0;
    EXPECT_TRUE(Domain::CrawlerFor(in, world, world.mEntities[0], &x));
    EXPECT_EQ(x, 1262);
    // (not one on another level or of the other scale)
    world.mCrawlers[1].mY = 100;
    EXPECT_FALSE(Domain::CrawlerFor(in, world, world.mEntities[0]));
    world.mCrawlers[1].mY = 200;
    world.mCrawlers[1].mHalf = true;
    EXPECT_FALSE(Domain::CrawlerFor(in, world, world.mEntities[0]));
    world.mCrawlers[1].mHalf = false;
    // The door open: the one beyond it too (it is the first found)
    world.mSwitches[187] = 1;
    EXPECT_TRUE(Domain::FloorOpen(in, world, 1162, 1312, 200, false));
    EXPECT_TRUE(Domain::CrawlerFor(in, world, world.mEntities[0], &x));
    EXPECT_EQ(x, 1162);
    world.mSwitches[187] = 0;
    // None about (their screen is not there): the ones of the map, where it has them; but not while he has one
    world.mCrawlers.clear();
    world.mEntities.push_back(MakeEntity(EntityKind::eCrawler, 3005, 1225, 180, 1249, 204));
    EXPECT_TRUE(Domain::CrawlerFor(in, world, world.mEntities[0], &x));
    EXPECT_EQ(x, 1237);
    world.mCrawlerMine = true;
    EXPECT_FALSE(Domain::CrawlerFor(in, world, world.mEntities[0]));
    world.mCrawlerMine = false;
    world.mEntities[1].mGone = true;
    EXPECT_FALSE(Domain::CrawlerFor(in, world, world.mEntities[0]));
}

TEST(BotGrenadeTargets, AGrenadeOfAesLiesWhereItComesDownIfItIsPutOnTheFloorToTheLastBit)
{
    // The Barracks' path 7: thrown up and along from 662,100 over a gap onto the floor at 200. Traced in the
    // game: at 968,200 thirty frames on, and it slid to 970 and lay there.
    NavInput in;
    in.mLines.push_back(Line(537, 100, 887, 100, 0));
    in.mLines.push_back(Line(862, 200, 1462, 200, 0));
    const ThrowArc::Flight flight = ThrowArc::Grenade(in, 662, 100, false, false, ThrowSpot::eUp);
    EXPECT_EQ(flight.mPoints[29].mX, 963);
    EXPECT_EQ(flight.mPoints[29].mY, 190);
    EXPECT_EQ(flight.mPoints[30].mX, 968);
    EXPECT_EQ(flight.mPoints[30].mY, 200);
    EXPECT_EQ(flight.mPoints.back().mX, 970);
    EXPECT_EQ(flight.mPoints.back().mY, 200);
    EXPECT_FALSE(flight.mUnder);
    // AO's bounces on
    NavInput ao = in;
    ao.mIsAe = false;
    EXPECT_GT(ThrowArc::Grenade(ao, 662, 100, false, false, ThrowSpot::eUp).mPoints.back().mX, 1050);
    // And AE's too when it is not put on the floor to the last bit (the game works that out in 256ths, cut off):
    // thrown far from 687,100 onto the ledge across at 100, 13 down in the frame it meets it with 5 to go. Traced:
    // it was still on its way along that ledge at 1119.
    in.mLines.push_back(Line(987, 100, 1387, 100, 0));
    const ThrowArc::Flight far = ThrowArc::Grenade(in, 687, 100, false, false, ThrowSpot::eForward);
    EXPECT_EQ(far.mPoints[20].mY, 95);
    // (six 256ths over it)
    EXPECT_EQ(far.mPoints[21].mY, 99);
    EXPECT_GT(far.mPoints.back().mX, 1119);
    EXPECT_EQ(far.mPoints.back().mY, 100);
}

TEST(BotGrenadeTargets, AGrenadeComesBackOffASecurityOrbAndOneThrownFlatGoesUnderIt)
{
    // The Barracks' path 7, its top left: the ledge he throws from, the one across with the Slig at 462, the wall
    // behind it, and the orb at 450,40 between (its box read from the game: 14 left to 15 right, 36 to 10 over
    // where it is). Traced: a grenade lobbed from 537,100 came down past the orb, stopped going left at 457 and
    // lay at 462, not at the 435 it would have got to.
    NavInput in;
    in.mLines.push_back(Line(525, 100, 899, 100, 0));
    in.mLines.push_back(Line(400, 100, 500, 100, 0));
    in.mLines.push_back(Line(425, 20, 425, 217, 1));
    World world;
    const std::vector<ThrowArc::Box> off{ThrowArc::BoxOf(450, 40, false, world.mOrbBox)};
    EXPECT_EQ(off[0].mX1, 436);
    EXPECT_EQ(off[0].mY1, 4);
    EXPECT_EQ(off[0].mX2, 465);
    EXPECT_EQ(off[0].mY2, 30);
    const ThrowArc::Flight free = ThrowArc::Grenade(in, 537, 100, false, true, ThrowSpot::eLob);
    EXPECT_LT(free.mPoints.back().mX, 445);
    const ThrowArc::Flight lob = ThrowArc::Grenade(in, 537, 100, false, true, ThrowSpot::eLob, ThrowArc::kFuse, &off);
    EXPECT_GE(lob.mPoints.back().mX, 455);
    EXPECT_LE(lob.mPoints.back().mX, 465);
    EXPECT_EQ(lob.mPoints.back().mY, 100);
    s32 least = 9999;
    for (const ThrowArc::Point& at : lob.mPoints)
    {
        least = std::min(least, at.mX);
    }
    EXPECT_GE(least, 455);
    // Thrown down it goes under the orb and lies further off, by the Slig
    const ThrowArc::Flight flat = ThrowArc::Grenade(in, 537, 100, false, true, ThrowSpot::eDown, ThrowArc::kFuse, &off);
    EXPECT_GE(flat.mPoints.back().mX, 425);
    EXPECT_LE(flat.mPoints.back().mX, 452);
    EXPECT_EQ(flat.mPoints.back().mY, 100);
    // And that is the throw for the Slig there: from 537, down
    world.mEntities.push_back(MakeEntity(EntityKind::eRockSack, 1000, 601, 68, 625, 104));
    world.mEntities.back().mInputs[1] = 2;
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 1017, 462, 80, 486, 104));
    world.mEntities.push_back(MakeEntity(EntityKind::eSecurityOrb, 1002, 450, 40, 474, 64));
    const std::vector<Domain::GrenadeTarget> targets = Domain::GrenadeTargets(in, world);
    ASSERT_EQ(targets.size(), 1u);
    bool down = false;
    for (const Domain::GrenadeShot& shot : targets[0].mShots)
    {
        down = down || (shot.mX == 537 && shot.mKind == ThrowSpot::eDown && shot.mFaceLeft);
        // (none lobbed from there: that one is the one that comes back)
        EXPECT_FALSE(shot.mX == 537 && shot.mKind == ThrowSpot::eLob);
    }
    EXPECT_TRUE(down);
    // Up under an orb it comes down again off the orb's underside: thrown from 600,300 it is at 641,221 and
    // then 651,215 on its way up to 200, and an orb at 642,224 has its box down to 214 there
    NavInput flatFloor;
    flatFloor.mLines.push_back(Line(300, 300, 900, 300, 0));
    const std::vector<ThrowArc::Box> over{ThrowArc::BoxOf(642, 224, false, world.mOrbBox)};
    s32 top = 9999;
    s32 topFree = 9999;
    for (const ThrowArc::Point& at : ThrowArc::Grenade(flatFloor, 600, 300, false, false, ThrowSpot::eUp, ThrowArc::kFuse, &over).mPoints)
    {
        top = std::min(top, at.mY);
    }
    for (const ThrowArc::Point& at : ThrowArc::Grenade(flatFloor, 600, 300, false, false, ThrowSpot::eUp).mPoints)
    {
        topFree = std::min(topFree, at.mY);
    }
    EXPECT_GE(top, over[0].mY2);
    // (on its way down from the frame it is put back under it)
    const ThrowArc::Flight hit = ThrowArc::Grenade(flatFloor, 600, 300, false, false, ThrowSpot::eUp, ThrowArc::kFuse, &over);
    const ThrowArc::Flight miss = ThrowArc::Grenade(flatFloor, 600, 300, false, false, ThrowSpot::eUp);
    std::size_t first = 0;
    while (first + 1 < hit.mPoints.size() && first < miss.mPoints.size() && hit.mPoints[first].mY == miss.mPoints[first].mY && hit.mPoints[first].mX == miss.mPoints[first].mX)
    {
        first++;
    }
    ASSERT_LT(first + 1, hit.mPoints.size());
    EXPECT_GT(hit.mPoints[first + 1].mY, hit.mPoints[first].mY);
    // (and down from there to the floor)
    EXPECT_EQ(ThrowArc::Grenade(flatFloor, 600, 300, false, false, ThrowSpot::eUp, ThrowArc::kFuse, &over).mPoints.back().mY, 300);
    EXPECT_LT(topFree, over[0].mY2);
    // (at half scale the box is half as big about the orb)
    const ThrowArc::Box small = ThrowArc::BoxOf(450, 40, true, world.mOrbBox);
    EXPECT_EQ(small.mX1, 443);
    EXPECT_EQ(small.mY2, 35);
}

TEST(BotGrenadeTargets, AGrenadeOfAesComesBackDownOffAFloorItFliesUpUnder)
{
    // Lobbed from under the top floor: up under it and down again, not onto it
    const NavInput in = BoomRooms();
    const ThrowArc::Flight under = ThrowArc::Grenade(in, 512, 1180, false, false, ThrowSpot::eLob);
    EXPECT_TRUE(under.mUnder);
    EXPECT_GT(under.mPoints.back().mY, 1120);
    // From clear of it: onto it, to lie there (it bounces, and is not under the floor for that)
    const ThrowArc::Flight clear = ThrowArc::Grenade(in, 462, 1180, false, false, ThrowSpot::eLob);
    EXPECT_FALSE(clear.mUnder);
    EXPECT_EQ(clear.mPoints.back().mY, 1120);
    EXPECT_EQ(clear.mPoints.size(), static_cast<std::size_t>(ThrowArc::kFuse) + 1);
    // AO's goes through
    NavInput ao = BoomRooms();
    ao.mIsAe = false;
    const ThrowArc::Flight through = ThrowArc::Grenade(ao, 512, 1180, false, false, ThrowSpot::eLob);
    EXPECT_FALSE(through.mUnder);
    EXPECT_EQ(through.mPoints.back().mY, 1120);
}

TEST(BotGrenadeTargets, WhereASligThatWalksItsBeatIsSomeFramesOn)
{
    // 2.2 a frame: on along its beat, back from the end, and round again
    EXPECT_EQ(GrenadeSligTask::SligIn(500, false, 500, 700, 50), 610);
    EXPECT_EQ(GrenadeSligTask::SligIn(650, false, 500, 700, 50), 640);
    EXPECT_EQ(GrenadeSligTask::SligIn(650, true, 500, 700, 50), 540);
    EXPECT_EQ(GrenadeSligTask::SligIn(520, true, 500, 700, 50), 590);
    EXPECT_EQ(GrenadeSligTask::SligIn(600, true, 0, 0, 50), 600);
}

TEST(BotGrenadeTargets, NotThrownFromWhereItComesUpUnderAFloorNorFromWhereItsBlastReachesHim)
{
    // A bit of floor over the way the lob from the shadow takes to the middle ledge: it comes up under that and
    // still comes down on the ledge, but where is not known well, so it is not thrown from there
    NavInput in = BoomRooms();
    const World world = BoomRoomsWorld();
    in.mLines.push_back(Line(415, 1100, 440, 1100, 0));
    const ThrowArc::Flight under = ThrowArc::Grenade(in, 387, 1240, false, false, ThrowSpot::eLob);
    ASSERT_TRUE(under.mUnder);
    ASSERT_EQ(under.mPoints.back().mY, 1180);
    for (const Domain::GrenadeTarget& target : Domain::GrenadeTargets(in, world))
    {
        for (const Domain::GrenadeShot& shot : target.mShots)
        {
            EXPECT_FALSE(target.mSlig == 12006 && shot.mX == 387 && shot.mKind == ThrowSpot::eLob && !shot.mFaceLeft);
        }
    }

    // A Slig on a floor high over the ground: lobbed from the ground it comes to lie on that floor less than a
    // blast from over his head, so not from there
    NavInput high;
    high.mLines.push_back(Line(25, 1240, 706, 1240, 0));
    high.mLines.push_back(Line(440, 1100, 600, 1100, 0));
    World over;
    over.mEntities.push_back(MakeEntity(EntityKind::eRockSack, 13002, 75, 1208, 99, 1244));
    over.mEntities.back().mInputs[1] = 2;
    over.mEntities.push_back(MakeEntity(EntityKind::eSlig, 12007, 500, 1080, 524, 1100));
    over.mEntities.push_back(MakeEntity(EntityKind::eSecurityOrb, 12020, 451, 1099, 475, 1123));
    const ThrowArc::Flight nearBy = ThrowArc::Grenade(high, 387, 1240, false, false, ThrowSpot::eLob);
    ASSERT_FALSE(nearBy.mUnder);
    ASSERT_EQ(nearBy.mPoints.back().mY, 1100);
    ASSERT_LT(nearBy.mPoints.back().mX - 387, static_cast<s32>(ThrowArc::kGrenadeKillX));
    // (and with nowhere else to throw from it is not one for a grenade at all)
    EXPECT_TRUE(Domain::GrenadeTargets(high, over).empty());
}

// ---- Flying mines ----

// A mine as MovingBomb.cpp has it go: round a closed track, from a stand half a pixel a frame faster each frame
// up to its pace, on from one line to the next with what is left of the step
struct TestMine final
{
    std::vector<s32> mXs;
    std::vector<s32> mYs;
    double mPace = 6.734;
    double mSpeed = 0.0;
    double mAlong = 0.0;
    // Its box changes with its animation, which comes round every 8 frames
    bool mPulses = false;
    s32 mTick = 0;

    double Length() const
    {
        double length = 0.0;
        for (std::size_t i = 0; i + 1 < mXs.size(); i++)
        {
            length += std::hypot(static_cast<double>(mXs[i + 1] - mXs[i]), static_cast<double>(mYs[i + 1] - mYs[i]));
        }
        return length;
    }
    void Step()
    {
        mSpeed = std::min(mPace, mSpeed + 0.5);
        mAlong = std::fmod(mAlong + mSpeed, Length());
        mTick++;
    }
    BombCycle::Seen Seen(u32 id) const
    {
        double left = mAlong;
        double x = static_cast<double>(mXs[0]);
        double y = static_cast<double>(mYs[0]);
        for (std::size_t i = 0; i + 1 < mXs.size(); i++)
        {
            const double dx = static_cast<double>(mXs[i + 1] - mXs[i]);
            const double dy = static_cast<double>(mYs[i + 1] - mYs[i]);
            const double side = std::hypot(dx, dy);
            if (left <= side)
            {
                x = static_cast<double>(mXs[i]) + dx * left / side;
                y = static_cast<double>(mYs[i]) + dy * left / side;
                break;
            }
            left -= side;
        }
        BombCycle::Seen seen;
        seen.mId = id;
        seen.mX = static_cast<s32>(x * 256.0);
        seen.mY = static_cast<s32>(y * 256.0);
        seen.mLeft = mPulses ? -9 - mTick % 8 : -11;
        seen.mTop = mPulses ? -24 - (mTick % 8) / 2 : -26;
        seen.mRight = mPulses ? 14 - (mTick % 8) / 2 : 13;
        seen.mBottom = -4;
        return seen;
    }
};

// The second mine of the Barracks' path 5: round a box 125 wide and 120 high
static TestMine BoxMine()
{
    TestMine mine;
    mine.mXs = {3875, 4000, 4000, 3875, 3875};
    mine.mYs = {340, 340, 460, 460, 340};
    return mine;
}

static bool SameBox(const BombCycle::Box& box, const BombCycle::Seen& seen, s32 within)
{
    return std::abs(box.mX1 - (seen.mX / 256 + seen.mLeft)) <= within && std::abs(box.mY1 - (seen.mY / 256 + seen.mTop)) <= within && std::abs(box.mX2 - (seen.mX / 256 + seen.mRight)) <= within && std::abs(box.mY2 - (seen.mY / 256 + seen.mBottom)) <= within;
}

TEST(BotMines, AMineIsToldAheadOnceItHasBeenWatchedRoundItsTrack)
{
    BombCycle cycle;
    TestMine mine = BoxMine();
    s32 knownAt = -1;
    u32 frame = 0;
    for (frame = 1; frame <= 200; frame++)
    {
        mine.Step();
        cycle.Observe(frame, 5, {mine.Seen(7)});
        if (cycle.Known(7) && knownAt < 0)
        {
            knownAt = static_cast<s32>(frame);
        }
    }
    frame--;
    // A lap is 490 pixels at 6.734 a frame: not known before it is up to its pace and has been round once since
    const s32 lap = 490 * 1000 / 6734;
    EXPECT_GT(knownAt, lap);
    EXPECT_LT(knownAt, lap + 40);
    EXPECT_NEAR(cycle.Lap256(7), 490.0 / 6.734 * 256.0, 100.0);
    EXPECT_EQ(cycle.Count(), 1);
    // Where it will be, for laps to come (a lap is no whole number of frames, so this is more than what it did a
    // lap ago)
    for (s32 ahead = 0; ahead <= 400; ahead += 7)
    {
        TestMine then = mine;
        for (s32 i = 0; i < ahead; i++)
        {
            then.Step();
        }
        BombCycle::Box box;
        ASSERT_TRUE(cycle.At(7, ahead, box));
        EXPECT_TRUE(SameBox(box, then.Seen(7), 3)) << ahead << ": " << box.mX1 << "," << box.mY1 << " for " << then.Seen(7).mX / 256 - 11 << "," << then.Seen(7).mY / 256 - 26;
    }
    // A place on its way is met when it is there and not before; one at the other scale never
    const BombCycle::Box onTop{3930, 300, 3950, 345};
    s32 wrong = 0;
    s32 met = 0;
    TestMine then = mine;
    for (s32 ahead = 0; ahead <= 150; ahead++)
    {
        const BombCycle::Seen seen = then.Seen(7);
        const BombCycle::Box real{seen.mX / 256 + seen.mLeft, seen.mY / 256 + seen.mTop, seen.mX / 256 + seen.mRight, seen.mY / 256 + seen.mBottom};
        wrong += cycle.Hits(false, ahead, onTop) != real.Meets(onTop) ? 1 : 0;
        met += real.Meets(onTop) ? 1 : 0;
        EXPECT_FALSE(cycle.Hits(true, ahead, onTop));
        then.Step();
    }
    EXPECT_GT(met, 8);
    EXPECT_LT(met, 40);
    // (a frame either side of when it comes and goes, at most)
    EXPECT_LE(wrong, 4);
    // And how long until then
    const s32 next = cycle.Next(false, onTop, 0, 200);
    ASSERT_GE(next, 0);
    EXPECT_TRUE(cycle.Hits(false, next, onTop));
    EXPECT_FALSE(next > 0 && cycle.Hits(false, next - 1, onTop));
    EXPECT_EQ(cycle.Next(false, BombCycle::Box{3930, 380, 3950, 420}, 0, 200), -1);
    // There is no telling of one that isn't there
    BombCycle::Box none;
    EXPECT_FALSE(cycle.At(8, 0, none));
    EXPECT_FALSE(cycle.Known(8));
}

TEST(BotMines, ItsBoxChangesWithItsAnimationAndThatIsToldAheadToo)
{
    BombCycle cycle;
    TestMine mine = BoxMine();
    mine.mPulses = true;
    u32 frame = 0;
    // Seen for a few frames only: as wide as it has been so far, whenever
    for (frame = 1; frame <= 6; frame++)
    {
        mine.Step();
        cycle.Observe(frame, 5, {mine.Seen(7)});
    }
    {
        BombCycle::Box box;
        ASSERT_TRUE(cycle.At(7, 0, box));
        const s32 x = mine.Seen(7).mX / 256;
        const s32 y = mine.Seen(7).mY / 256;
        EXPECT_EQ(box.mX1, x - 15);
        EXPECT_EQ(box.mX2, x + 14);
        EXPECT_EQ(box.mY1, y - 27);
        EXPECT_EQ(box.mY2, y - 4);
    }
    for (; frame <= 200; frame++)
    {
        mine.Step();
        cycle.Observe(frame, 5, {mine.Seen(7)});
    }
    ASSERT_TRUE(cycle.Known(7));
    // Round its track and round its animation: the box it will have then, not the one it has now
    s32 differs = 0;
    for (s32 ahead = 0; ahead <= 300; ahead++)
    {
        TestMine then = mine;
        for (s32 i = 0; i < ahead; i++)
        {
            then.Step();
        }
        BombCycle::Box box;
        ASSERT_TRUE(cycle.At(7, ahead, box));
        const BombCycle::Seen real = then.Seen(7);
        EXPECT_TRUE(SameBox(box, real, 3)) << ahead;
        // (how wide it is, to the pixel)
        EXPECT_EQ(box.mX2 - box.mX1, real.mRight - real.mLeft) << ahead;
        EXPECT_EQ(box.mY2 - box.mY1, real.mBottom - real.mTop) << ahead;
        differs += real.mLeft != mine.Seen(7).mLeft ? 1 : 0;
    }
    EXPECT_GT(differs, 200);
    // Not seen for a while, its animation is anywhere: as wide as it gets again, until that has come round twice
    for (s32 i = 0; i < 5; i++)
    {
        mine.Step();
    }
    frame += 5;
    cycle.Observe(frame, 5, {mine.Seen(7)});
    for (s32 i = 0; i < 10; i++)
    {
        mine.Step();
        cycle.Observe(++frame, 5, {mine.Seen(7)});
    }
    {
        BombCycle::Box box;
        ASSERT_TRUE(cycle.At(7, 3, box));
        EXPECT_EQ(box.mX2 - box.mX1, 14 + 16);
    }
    for (s32 i = 0; i < 20; i++)
    {
        mine.Step();
        cycle.Observe(++frame, 5, {mine.Seen(7)});
    }
    {
        TestMine then = mine;
        for (s32 i = 0; i < 3; i++)
        {
            then.Step();
        }
        BombCycle::Box box;
        ASSERT_TRUE(cycle.At(7, 3, box));
        EXPECT_EQ(box.mX2 - box.mX1, then.Seen(7).mRight - then.Seen(7).mLeft);
    }
}

TEST(BotMines, ALapOnceWatchedIsKeptWhileTheMineIsOutOfSight)
{
    BombCycle cycle;
    TestMine mine = BoxMine();
    u32 frame = 0;
    for (frame = 1; frame <= 200; frame++)
    {
        mine.Step();
        cycle.Observe(frame, 5, {mine.Seen(7)});
    }
    ASSERT_TRUE(cycle.Known(7));
    const s32 lap = cycle.Lap256(7);
    // Frames missed (a rewind): where it is on its lap is found again, and how it moves a frame later
    for (s32 i = 0; i < 30; i++)
    {
        mine.Step();
    }
    frame += 30;
    cycle.Observe(frame, 5, {mine.Seen(7)});
    EXPECT_FALSE(cycle.Known(7));
    EXPECT_EQ(cycle.Lap256(7), lap);
    mine.Step();
    cycle.Observe(++frame, 5, {mine.Seen(7)});
    EXPECT_TRUE(cycle.Known(7));
    {
        TestMine then = mine;
        for (s32 i = 0; i < 100; i++)
        {
            then.Step();
        }
        BombCycle::Box box;
        ASSERT_TRUE(cycle.At(7, 100, box));
        EXPECT_TRUE(SameBox(box, then.Seen(7), 3));
    }
    // Its screen gone, it is gone; back, it starts from a stand where it was put: on the lap that is known, but
    // not at its pace yet
    for (s32 i = 0; i < 50; i++)
    {
        cycle.Observe(++frame, 5, {});
    }
    EXPECT_EQ(cycle.Count(), 0);
    EXPECT_FALSE(cycle.Known(7));
    EXPECT_FALSE(cycle.Hits(false, 0, BombCycle::Box{3800, 300, 4100, 500}));
    TestMine back = BoxMine();
    back.mAlong = 50.0;
    cycle.Observe(++frame, 5, {back.Seen(7)});
    EXPECT_FALSE(cycle.Known(7));
    s32 knownAfter = -1;
    for (s32 i = 1; i <= 40; i++)
    {
        back.Step();
        cycle.Observe(++frame, 5, {back.Seen(7)});
        if (cycle.Known(7) && knownAfter < 0)
        {
            knownAfter = i;
        }
    }
    // (half a pixel a frame faster each frame up to 6.734: 14 frames)
    EXPECT_GE(knownAfter, 9);
    EXPECT_LE(knownAfter, 15);
    EXPECT_EQ(cycle.Lap256(7), lap);
    {
        TestMine then = back;
        for (s32 i = 0; i < 100; i++)
        {
            then.Step();
        }
        BombCycle::Box box;
        ASSERT_TRUE(cycle.At(7, 100, box));
        EXPECT_TRUE(SameBox(box, then.Seen(7), 3));
    }
    // One that keeps still is known as soon as it has for a while: it is where it is
    TestMine parked = BoxMine();
    parked.mPace = 0.0;
    BombCycle still;
    for (u32 f = 1; f <= static_cast<u32>(BombCycle::kStill); f++)
    {
        still.Observe(f, 5, {parked.Seen(9)});
        EXPECT_FALSE(still.Known(9));
    }
    still.Observe(static_cast<u32>(BombCycle::kStill) + 1, 5, {parked.Seen(9)});
    EXPECT_TRUE(still.Known(9));
    EXPECT_TRUE(still.Hits(false, 500, BombCycle::Box{3870, 320, 3880, 330}));
    EXPECT_FALSE(still.Hits(false, 500, BombCycle::Box{3900, 320, 3910, 330}));
    // Another path: all of it forgotten
    cycle.Observe(++frame, 6, {back.Seen(7)});
    EXPECT_EQ(cycle.Lap256(7), 0);
}

// The track of BoxMine as the map has it (OGData::eLineTypes: 8 is a track), and a mine put on its top side
static NavInput BoxTrack()
{
    NavInput in;
    in.mLines.push_back(Line(3875, 340, 4000, 340, 8));
    in.mLines.push_back(Line(4000, 340, 4000, 460, 8));
    in.mLines.push_back(Line(4000, 460, 3875, 460, 8));
    in.mLines.push_back(Line(3875, 460, 3875, 340, 8));
    in.mFeatures.push_back(Feature(FeatureKind::eFlyingMine, 3925, 330, 3950, 350));
    return in;
}

TEST(BotMines, WhereAMineCanEverBeIsItsTrackOnTheMap)
{
    NavInput in = BoxTrack();
    // (a floor line through it is no track; a mine by no track stays where it is put; one on a track that does
    // not come round has all of it both ways)
    in.mLines.push_back(Line(3800, 335, 4100, 335, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eFlyingMine, 3000, 100, 3025, 120));
    in.mLines.push_back(Line(2000, 100, 2100, 100, 8));
    in.mLines.push_back(Line(2100, 100, 2100, 200, 8));
    in.mLines.push_back(Line(2100, 200, 2200, 200, 8));
    in.mFeatures.push_back(Feature(FeatureKind::eFlyingMine, 2090, 140, 2110, 160));
    NavFeature small = Feature(FeatureKind::eFlyingMine, 3925, 330, 3950, 350);
    small.mHalf = true;
    in.mFeatures.push_back(small);
    const std::vector<BombCycle::Track> tracks = BombCycle::Tracks(in);
    ASSERT_EQ(tracks.size(), 4u);
    EXPECT_EQ(tracks[0].mXs, (std::vector<s32>{3875, 4000, 4000, 3875, 3875}));
    EXPECT_EQ(tracks[0].mYs, (std::vector<s32>{340, 340, 460, 460, 340}));
    EXPECT_FALSE(tracks[0].mHalf);
    EXPECT_EQ(tracks[1].mXs, (std::vector<s32>{3000}));
    EXPECT_EQ(tracks[1].mYs, (std::vector<s32>{100}));
    EXPECT_EQ(tracks[2].mXs, (std::vector<s32>{2000, 2100, 2100, 2200}));
    EXPECT_EQ(tracks[2].mYs, (std::vector<s32>{100, 100, 200, 200}));
    EXPECT_TRUE(tracks[3].mHalf);

    BombCycle cycle;
    EXPECT_FALSE(cycle.Any());
    cycle.SetTracks(5, tracks);
    EXPECT_TRUE(cycle.HasTracks(5));
    cycle.Observe(1, 5, {});
    EXPECT_TRUE(cycle.Any());
    // Its box is from 11 left of where it is to 13 right, and from 26 to 4 above: a place just clear of that on
    // each side is never met, one a pixel nearer is
    const auto ever = [&cycle](s32 x1, s32 y1, s32 x2, s32 y2)
    {
        return cycle.Ever(false, BombCycle::Box{x1, y1, x2, y2});
    };
    EXPECT_TRUE(ever(3930, 300, 3950, 320));
    EXPECT_FALSE(ever(3930, 337, 3950, 420));
    EXPECT_TRUE(ever(3930, 336, 3950, 420));
    EXPECT_FALSE(ever(3930, 300, 3950, 313));
    EXPECT_TRUE(ever(3930, 300, 3950, 314));
    EXPECT_FALSE(ever(4014, 380, 4030, 400));
    EXPECT_TRUE(ever(4013, 380, 4030, 400));
    EXPECT_FALSE(ever(3840, 380, 3863, 400));
    EXPECT_TRUE(ever(3840, 380, 3864, 400));
    // (the one that stays put, and the track that does not come round)
    EXPECT_TRUE(ever(2990, 80, 3010, 90));
    EXPECT_TRUE(ever(2150, 180, 2160, 190));
    EXPECT_FALSE(ever(2150, 80, 2160, 90));
    // Half the size at the other scale
    EXPECT_TRUE(cycle.Ever(true, BombCycle::Box{4005, 380, 4030, 400}));
    EXPECT_FALSE(cycle.Ever(true, BombCycle::Box{4007, 380, 4030, 400}));
    // No mine on it yet: its screen has not loaded
    const BombCycle::Box onTop{3930, 300, 3950, 345};
    EXPECT_TRUE(cycle.Missing(false, onTop));
    EXPECT_FALSE(cycle.Watching(false, onTop));
    EXPECT_FALSE(cycle.Missing(false, BombCycle::Box{3930, 380, 3950, 420}));
    // One there, and watched: not known until it has been round
    TestMine mine = BoxMine();
    u32 frame = 1;
    for (s32 i = 0; i < 30; i++)
    {
        mine.Step();
        cycle.Observe(++frame, 5, {mine.Seen(7)});
    }
    EXPECT_FALSE(cycle.Missing(false, onTop));
    EXPECT_TRUE(cycle.Watching(false, onTop));
    EXPECT_FALSE(cycle.Watching(false, BombCycle::Box{3930, 380, 3950, 420}));
    EXPECT_FALSE(cycle.Watching(true, onTop));
    for (s32 i = 0; i < 100; i++)
    {
        mine.Step();
        cycle.Observe(++frame, 5, {mine.Seen(7)});
    }
    EXPECT_FALSE(cycle.Missing(false, onTop));
    EXPECT_FALSE(cycle.Watching(false, onTop));
    EXPECT_TRUE(ever(3930, 300, 3950, 320));
    // Out of sight again (its lap is kept): none on the track
    cycle.Observe(++frame, 5, {});
    EXPECT_TRUE(cycle.Missing(false, onTop));
    EXPECT_FALSE(cycle.Watching(false, onTop));
    EXPECT_TRUE(ever(3930, 300, 3950, 320));
    // The tracks are another path's: nothing of them here
    cycle.Observe(++frame, 6, {});
    EXPECT_FALSE(cycle.Any());
    EXPECT_FALSE(cycle.Ever(false, onTop));
    EXPECT_FALSE(cycle.Missing(false, onTop));
}

static std::vector<GoToTask::Body> BodiesOf(const NavGraph& g, const NavEdge* edge, bool rest)
{
    std::vector<s32> path{static_cast<s32>(edge - g.Edges().data())};
    std::vector<GoToTask::Body> out;
    GoToTask::Bodies(g, path, 0, 1, false, rest, out);
    return out;
}

TEST(BotMines, WhereHeIsFrameByFrameOfAHopAndAFallIsAsMeasured)
{
    // A hop over a gap: 10 frames where he stands (leaning back, then far forward), 5 through the air, 9 landing
    {
        NavInput in;
        in.mLines.push_back(Line(0, 100, 200, 100, 0));
        in.mLines.push_back(Line(250, 100, 400, 100, 0));
        NavGraph g;
        g.Build(in);
        const NavEdge* hop = FindEdge(g, g.NodeAt(187, 100, false), g.NodeAt(262, 100, false), EdgeKind::eHop);
        ASSERT_NE(hop, nullptr);
        const std::vector<GoToTask::Body> at = BodiesOf(g, hop, false);
        ASSERT_EQ(at.size(), 24u);
        for (std::size_t i = 0; i < 24; i++)
        {
            EXPECT_EQ(at[i].mX, i < 10 ? 187 : i < 15 ? 187 + 15 * (static_cast<s32>(i) - 9) : 262) << i;
            EXPECT_EQ(at[i].mY, 100) << i;
        }
        // (his box: 23 behind and 19 ahead at first, 45 ahead as he goes off, trailing in the air, 26 landing)
        const BombCycle::Box lean = GoToTask::BombBody(at[0]);
        EXPECT_EQ(lean.mX1, 187 - 23);
        EXPECT_EQ(lean.mX2, 187 + 19);
        EXPECT_EQ(lean.mY1, 100 - 55 - 1);
        EXPECT_EQ(lean.mY2, 99);
        const BombCycle::Box off = GoToTask::BombBody(at[9]);
        EXPECT_EQ(off.mX1, 187 - 14);
        EXPECT_EQ(off.mX2, 187 + 45);
        const BombCycle::Box air = GoToTask::BombBody(at[14]);
        EXPECT_EQ(air.mX1, 262 - 44);
        EXPECT_EQ(air.mX2, 262 + 9);
        const BombCycle::Box land = GoToTask::BombBody(at[20]);
        EXPECT_EQ(land.mX1, 262 - 26);
        EXPECT_EQ(land.mX2, 262 + 26);
        // And a moment standing where it ends: 14 to either side and 53 high
        const std::vector<GoToTask::Body> rested = BodiesOf(g, hop, true);
        ASSERT_EQ(rested.size(), 36u);
        const BombCycle::Box stood = GoToTask::BombBody(rested[30]);
        EXPECT_EQ(stood.mX1, 262 - 14);
        EXPECT_EQ(stood.mX2, 262 + 14);
        EXPECT_EQ(stood.mY1, 100 - 53 - 1);
        // The other way his box is the other way round
        const NavEdge* back = FindEdge(g, g.NodeAt(262, 100, false), g.NodeAt(187, 100, false), EdgeKind::eHop);
        ASSERT_NE(back, nullptr);
        const BombCycle::Box backOff = GoToTask::BombBody(BodiesOf(g, back, false)[9]);
        EXPECT_EQ(backOff.mX1, 262 - 45);
        EXPECT_EQ(backOff.mX2, 262 + 14);
    }
    // A hop down 100 (the Barracks' path 5, off 4337,340 to the left): level for those 5, then down faster and
    // faster for 6, a cell further out
    {
        NavInput in;
        in.mLines.push_back(Line(4325, 340, 4525, 340, 0));
        in.mLines.push_back(Line(4025, 440, 4250, 440, 0));
        NavGraph g;
        g.Build(in);
        const NavEdge* hop = FindEdge(g, g.NodeAt(4337, 340, false), g.NodeAt(4237, 440, false), EdgeKind::eHop);
        ASSERT_NE(hop, nullptr) << g.Text();
        const std::vector<GoToTask::Body> at = BodiesOf(g, hop, false);
        ASSERT_EQ(at.size(), 30u);
        EXPECT_EQ(at[9].mX, 4337);
        EXPECT_EQ(at[14].mX, 4262);
        EXPECT_EQ(at[14].mY, 340);
        EXPECT_EQ(at[15].mY, 345);
        EXPECT_EQ(at[17].mY, 340 + (90 + 207) / 10);
        EXPECT_LT(at[17].mX, 4262);
        EXPECT_GT(at[17].mX, 4237);
        EXPECT_EQ(at[20].mX, 4237);
        EXPECT_EQ(at[20].mY, 440);
        EXPECT_EQ(at[29].mY, 440);
    }
    // Walked off an edge: 6 frames on the floor, down 0.9 t squared, 10 on his feet again
    {
        NavInput in;
        in.mLines.push_back(Line(0, 100, 200, 100, 0));
        in.mLines.push_back(Line(0, 200, 400, 200, 0));
        NavGraph g;
        g.Build(in);
        const NavEdge* fall = FindEdge(g, g.NodeAt(187, 100, false), g.NodeAt(212, 200, false), EdgeKind::eFall);
        ASSERT_NE(fall, nullptr) << g.Text();
        const std::vector<GoToTask::Body> at = BodiesOf(g, fall, false);
        ASSERT_EQ(at.size(), 27u);
        EXPECT_EQ(at[5].mY, 100);
        EXPECT_EQ(at[5].mX, 199);
        EXPECT_EQ(at[10].mX, 212);
        EXPECT_EQ(at[11].mY, 100 + 9 * 25 / 10);
        EXPECT_EQ(at[16].mY, 190);
        EXPECT_EQ(at[17].mY, 200);
        EXPECT_EQ(at[26].mY, 200);
        // (wider and higher in the air)
        EXPECT_EQ(GoToTask::BombBody(at[11]).mX1, at[11].mX - 35);
        EXPECT_EQ(GoToTask::BombBody(at[11]).mY1, at[11].mY - 64 - 1);
        EXPECT_EQ(GoToTask::BombBody(at[20]).mX1, 212 - 26);
    }
    // A hop up to a ledge: off as any hop, 3 level and 6 up to it, 3 hung under it on the side he came from, then
    // 23 pulling himself up, his box over the ledge long before he stands on it
    {
        NavInput in;
        in.mLines.push_back(Line(475, 960, 575, 960, 0));
        in.mLines.push_back(Line(300, 880, 450, 880, 0));
        NavFeature edge = Feature(FeatureKind::eEdge, 425, 880, 449, 904);
        edge.mA = 1;
        edge.mDir = 0;
        in.mFeatures.push_back(edge);
        NavGraph g;
        g.Build(in);
        const NavEdge* hop = FindEdge(g, g.NodeAt(487, 960, false), g.NodeAt(437, 880, false), EdgeKind::eHop);
        ASSERT_NE(hop, nullptr) << g.Text();
        const std::vector<GoToTask::Body> at = BodiesOf(g, hop, false);
        ASSERT_EQ(at.size(), 45u);
        EXPECT_EQ(at[9].mX, 487);
        EXPECT_EQ(at[12].mX, 487 - 19);
        EXPECT_EQ(at[12].mY, 960);
        EXPECT_EQ(at[18].mX, 448);
        EXPECT_EQ(at[18].mY, 889);
        const BombCycle::Box hung = GoToTask::BombBody(at[20]);
        EXPECT_EQ(hung.mX1, 437 - 6);
        EXPECT_EQ(hung.mX2, 437 + 27);
        EXPECT_EQ(hung.mY1, 880 + 4);
        EXPECT_EQ(hung.mY2, 880 + 64);
        const BombCycle::Box pulling = GoToTask::BombBody(at[30]);
        EXPECT_EQ(pulling.mY1, 880 - 37);
        EXPECT_EQ(pulling.mY2, 880 + 33);
        const BombCycle::Box up = GoToTask::BombBody(at[44]);
        EXPECT_EQ(up.mY1, 880 - 54);
        EXPECT_EQ(up.mY2, 880 + 10);
    }
}

// A floor with a mine that goes up and down through it at 400: down to `bottom`
static TestMine ShaftMine(s32 bottom)
{
    TestMine mine;
    mine.mXs = {400, 409, 409, 400, 400};
    mine.mYs = {-60, -60, bottom, bottom, -60};
    mine.mPace = 3.5;
    return mine;
}

static BombCycle Watched(TestMine& mine, u32& frame)
{
    BombCycle cycle;
    BombCycle::Track track;
    track.mXs = mine.mXs;
    track.mYs = mine.mYs;
    cycle.SetTracks(5, {track});
    for (frame = 1; frame <= 300; frame++)
    {
        mine.Step();
        cycle.Observe(frame, 5, {mine.Seen(7)});
    }
    frame--;
    return cycle;
}

TEST(BotMines, TheWayIsGoneFromOnePlaceNoMineReachesToTheNextWhenAllOfItFitsInBetween)
{
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    NavGraph g;
    g.Build(in);
    TestMine mine = ShaftMine(79);
    u32 frame = 0;
    BombCycle cycle = Watched(mine, frame);
    ASSERT_TRUE(cycle.Known(7));
    // The two cells it comes down through are no place to wait, standing or crouched; the ones either side are
    bool duck = true;
    EXPECT_TRUE(GoToTask::BombStop(g, cycle, g.NodeAt(362, 100, false), duck));
    EXPECT_FALSE(duck);
    EXPECT_FALSE(GoToTask::BombStop(g, cycle, g.NodeAt(387, 100, false), duck));
    EXPECT_FALSE(GoToTask::BombStop(g, cycle, g.NodeAt(412, 100, false), duck));
    EXPECT_TRUE(GoToTask::BombStop(g, cycle, g.NodeAt(437, 100, false), duck));
    EXPECT_FALSE(duck);
    // One that stops 15 higher goes over a crouched man
    {
        TestMine high = ShaftMine(64);
        u32 f = 0;
        const BombCycle over = Watched(high, f);
        EXPECT_TRUE(GoToTask::BombStop(g, over, g.NodeAt(387, 100, false), duck));
        EXPECT_TRUE(duck);
        EXPECT_TRUE(GoToTask::BombStop(g, over, g.NodeAt(362, 100, false), duck));
        EXPECT_FALSE(duck);
        // (and one rolled under: any time)
        std::vector<s32> under;
        ASSERT_TRUE(g.FindPath(g.NodeAt(362, 100, false), g.NodeAt(437, 100, false), nullptr, under));
        for (s32 later = 0; later < 90; later++)
        {
            EXPECT_TRUE(GoToTask::BombsClear(g, over, under, under.size(), true, 0, later)) << later;
        }
    }

    // Through: three cells at a walk and a moment at the end, 47 frames in which it has to be up out of the way
    std::vector<s32> path;
    ASSERT_TRUE(g.FindPath(g.NodeAt(362, 100, false), g.NodeAt(437, 100, false), nullptr, path));
    ASSERT_EQ(path.size(), 3u);
    std::vector<GoToTask::Body> bodies;
    GoToTask::Bodies(g, path, 0, path.size(), false, true, bodies);
    ASSERT_EQ(bodies.size(), 47u);
    // (and the two frames it takes him to start on it, standing where he is)
    {
        GoToTask::Body standing;
        standing.mX = 362;
        standing.mY = 100;
        bodies.insert(bodies.begin(), 2, standing);
    }
    s32 clear = 0;
    s32 firstClear = -1;
    for (s32 later = 0; later < 85; later++)
    {
        const bool now = GoToTask::BombsClear(g, cycle, path, path.size(), false, 0, later);
        clear += now ? 1 : 0;
        firstClear = now && firstClear < 0 ? later : firstClear;
        // What it says is so: the mine as it then goes meets him on no frame of the way, or does on one
        TestMine then = mine;
        for (s32 i = 0; i < later; i++)
        {
            then.Step();
        }
        // (or he a frame later than that)
        bool met = false;
        bool metLate = false;
        for (const GoToTask::Body& body : bodies)
        {
            const BombCycle::Seen seen = then.Seen(7);
            const BombCycle::Box real{seen.mX / 256 + seen.mLeft, seen.mY / 256 + seen.mTop, seen.mX / 256 + seen.mRight, seen.mY / 256 + seen.mBottom};
            met = met || real.Meets(GoToTask::BombBody(body));
            then.Step();
            const BombCycle::Seen next = then.Seen(7);
            const BombCycle::Box after{next.mX / 256 + next.mLeft, next.mY / 256 + next.mTop, next.mX / 256 + next.mRight, next.mY / 256 + next.mBottom};
            metLate = metLate || after.Meets(GoToTask::BombBody(body));
        }
        if (now)
        {
            EXPECT_FALSE(met) << later;
            EXPECT_FALSE(metLate) << later;
        }
        else
        {
            EXPECT_TRUE(met || metLate) << later;
        }
    }
    // (a lap is 85 frames; it is low enough to meet him for some 20 of them, and he is in its way for some 30
    // of his 47)
    EXPECT_GT(clear, 15);
    EXPECT_LT(clear, 45);
    ASSERT_GE(firstClear, 0);
    // Standing where he is first, it is as if it were that much later
    for (s32 later = 0; later < 60; later += 3)
    {
        EXPECT_EQ(GoToTask::BombsClear(g, cycle, path, path.size(), false, 20, later), GoToTask::BombsClear(g, cycle, path, path.size(), false, 0, later + 20)) << later;
    }
    // And what it says of later is what it says then
    for (s32 later = 1; later <= 40; later++)
    {
        const bool said = GoToTask::BombsClear(g, cycle, path, path.size(), false, 0, later);
        BombCycle then = cycle;
        TestMine moved = mine;
        for (s32 i = 0; i < later; i++)
        {
            moved.Step();
            then.Observe(frame + 1 + static_cast<u32>(i), 5, {moved.Seen(7)});
        }
        if (then.Known(7))
        {
            EXPECT_EQ(GoToTask::BombsClear(g, then, path, path.size(), false, 0, 0), said) << later;
        }
    }
    // Only the first cell of it: the mine is no matter there
    EXPECT_TRUE(GoToTask::BombsClear(g, cycle, std::vector<s32>{}, 0, false, 0, 0));
}

TEST(BotNav, AnOpenTrapDoorIsHoppedOverAndNotFallenThroughIntoADeathDrop)
{
    // The Barracks' path 5: floors with trap doors between them, open until a lever shuts them, a death drop
    // under all of it and a floor below that (which nobody comes down to alive)
    NavInput in;
    in.mLines.push_back(Line(0, 100, 200, 100, 0));
    in.mLines.push_back(Line(250, 100, 400, 100, 0));
    in.mLines.push_back(Line(0, 240, 400, 240, 0));
    NavFeature trap = Feature(FeatureKind::eTrapDoor, 200, 100, 250, 120);
    trap.mSwitch = 7;
    trap.mStartClosed = false;
    in.mFeatures.push_back(trap);
    {
        NavGraph g;
        g.Build(in);
        const s32 before = g.NodeAt(187, 100, false);
        const s32 after = g.NodeAt(262, 100, false);
        const s32 on = g.NodeAt(212, 100, false);
        ASSERT_GE(on, 0);
        ASSERT_EQ(g.Nodes()[on].mTrapDoor, 0);
        // Over it either way, while it is open (its switch off: it starts open); shut it is walked over
        const NavEdge* hop = FindEdge(g, before, after, EdgeKind::eHop);
        ASSERT_NE(hop, nullptr) << g.Text();
        s32 sw = 0;
        s32 value = -1;
        ASSERT_TRUE(g.Condition(*hop, &sw, &value));
        EXPECT_EQ(sw, 7);
        EXPECT_EQ(value, 0);
        const NavEdge* back = FindEdge(g, after, before, EdgeKind::eHop);
        ASSERT_NE(back, nullptr);
        ASSERT_TRUE(g.Condition(*back, &sw, &value));
        EXPECT_EQ(value, 0);
        const NavEdge* walk = FindEdge(g, before, on, EdgeKind::eWalk);
        ASSERT_NE(walk, nullptr);
        ASSERT_TRUE(g.Condition(*walk, &sw, &value));
        EXPECT_EQ(sw, 7);
        EXPECT_EQ(value, 1);
        // (not from on it, nor onto it)
        EXPECT_EQ(FindEdge(g, on, g.NodeAt(287, 100, false), EdgeKind::eHop), nullptr);
        EXPECT_EQ(FindEdge(g, g.NodeAt(162, 100, false), g.NodeAt(237, 100, false), EdgeKind::eHop), nullptr);
        // Through it there is the floor below
        EXPECT_NE(FindEdge(g, before, g.NodeAt(212, 240, false), EdgeKind::eFall), nullptr) << g.Text();
        EXPECT_NE(FindEdge(g, on, g.NodeAt(212, 240, false), EdgeKind::eFall), nullptr);
    }
    // (AO the same. A trap door two of its cells wide, wherever those are.)
    {
        NavInput flat = AoInput();
        flat.mLines.push_back(Line(0, 100, 2000, 100, 0));
        NavGraph cells;
        cells.Build(flat);
        std::vector<s32> xs;
        for (const NavNode& n : cells.Nodes())
        {
            xs.push_back(n.mX);
        }
        std::sort(xs.begin(), xs.end());
        ASSERT_GE(xs.size(), 12u);
        const s32 left = (xs[4] + xs[5]) / 2;
        const s32 right = (xs[6] + xs[7]) / 2;
        NavInput ao = AoInput();
        ao.mLines.push_back(Line(0, 100, left, 100, 0));
        ao.mLines.push_back(Line(right, 100, 2000, 100, 0));
        NavFeature door = Feature(FeatureKind::eTrapDoor, left, 100, right, 120);
        door.mSwitch = 7;
        door.mStartClosed = true;
        ao.mFeatures.push_back(door);
        NavGraph g;
        g.Build(ao);
        ASSERT_GE(g.NodeAt(xs[5], 100, false), 0);
        ASSERT_GE(g.Nodes()[g.NodeAt(xs[5], 100, false)].mTrapDoor, 0) << g.Text();
        ASSERT_GE(g.Nodes()[g.NodeAt(xs[6], 100, false)].mTrapDoor, 0);
        ASSERT_LT(g.Nodes()[g.NodeAt(xs[4], 100, false)].mTrapDoor, 0);
        ASSERT_LT(g.Nodes()[g.NodeAt(xs[7], 100, false)].mTrapDoor, 0);
        // (every hop there is is over it, and only while it is open: its switch on, as it starts shut)
        s32 hops = 0;
        for (const NavEdge& e : g.Edges())
        {
            if (e.mKind == EdgeKind::eHop)
            {
                hops++;
                s32 sw = 0;
                s32 value = -1;
                EXPECT_TRUE(g.Condition(e, &sw, &value)) << g.Nodes()[e.mFrom].mX << " to " << g.Nodes()[e.mTo].mX;
                EXPECT_EQ(sw, 7);
                EXPECT_EQ(value, 1);
            }
        }
        EXPECT_EQ(hops, 2);
    }
    // With a death drop between: no way down through it
    in.mFeatures.push_back(Feature(FeatureKind::eDeathDrop, 100, 180, 400, 204));
    {
        NavGraph g;
        g.Build(in);
        const s32 before = g.NodeAt(187, 100, false);
        const s32 on = g.NodeAt(212, 100, false);
        EXPECT_EQ(FindEdge(g, before, g.NodeAt(212, 240, false), EdgeKind::eFall), nullptr);
        EXPECT_EQ(FindEdge(g, on, g.NodeAt(212, 240, false), EdgeKind::eFall), nullptr);
        EXPECT_EQ(FindEdge(g, on, g.NodeAt(237, 240, false), EdgeKind::eFall), nullptr);
        EXPECT_NE(FindEdge(g, before, g.NodeAt(262, 100, false), EdgeKind::eHop), nullptr);
    }
    // However far down the floor below is, without one (Rupture Farms' path 16 is left so)
    {
        NavInput deep;
        deep.mLines.push_back(Line(0, 100, 200, 100, 0));
        deep.mLines.push_back(Line(250, 100, 400, 100, 0));
        deep.mLines.push_back(Line(0, 400, 400, 400, 0));
        deep.mFeatures.push_back(trap);
        NavGraph g;
        g.Build(deep);
        EXPECT_NE(FindEdge(g, g.NodeAt(212, 100, false), g.NodeAt(212, 400, false), EdgeKind::eFall), nullptr);
    }
}

TEST(BotNav, ALiftIsRiddenNoFurtherThanTheStopItsRideEndsAt)
{
    // The Barracks' path 5: a lift a lever brings up from the bottom of its shaft, past a stop that is the end
    // of the ride down ("bottom floor"), to the top. He rides it between the top and that stop and no lower.
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(0, 300, 100, 300, 0));
    in.mLines.push_back(Line(0, 500, 100, 500, 0));
    NavFeature top = Feature(FeatureKind::eLiftStop, 100, 100, 176, 124);
    top.mC = 1;
    NavFeature mid = Feature(FeatureKind::eLiftStop, 100, 300, 176, 324);
    mid.mC = 2;
    NavFeature low = Feature(FeatureKind::eLiftStop, 100, 500, 176, 524);
    low.mC = 2;
    in.mFeatures = {top, mid, low};
    {
        NavGraph g;
        g.Build(in);
        const s32 a = g.NodeAt(137, 100, false);
        const s32 b = g.NodeAt(137, 300, false);
        const s32 c = g.NodeAt(137, 500, false);
        EXPECT_TRUE(HasEdge(g, a, b, EdgeKind::eLift)) << g.Text();
        EXPECT_TRUE(HasEdge(g, b, a, EdgeKind::eLift));
        EXPECT_FALSE(HasEdge(g, a, c, EdgeKind::eLift));
        EXPECT_FALSE(HasEdge(g, b, c, EdgeKind::eLift));
        // (up from the bottom it goes: nothing ends the ride up before the top)
        EXPECT_TRUE(HasEdge(g, c, b, EdgeKind::eLift));
        EXPECT_TRUE(HasEdge(g, c, a, EdgeKind::eLift));
    }
    // A stop that ends the ride up, in the middle: not on up past it, nor up from it
    in.mFeatures[1].mC = 1;
    {
        NavGraph g;
        g.Build(in);
        const s32 a = g.NodeAt(137, 100, false);
        const s32 b = g.NodeAt(137, 300, false);
        const s32 c = g.NodeAt(137, 500, false);
        EXPECT_FALSE(HasEdge(g, c, a, EdgeKind::eLift)) << g.Text();
        EXPECT_FALSE(HasEdge(g, b, a, EdgeKind::eLift));
        EXPECT_TRUE(HasEdge(g, c, b, EdgeKind::eLift));
        EXPECT_TRUE(HasEdge(g, a, b, EdgeKind::eLift));
        EXPECT_TRUE(HasEdge(g, a, c, EdgeKind::eLift));
    }
    // With stops that end nothing it is ridden all the way either way
    in.mFeatures[0].mC = 0;
    in.mFeatures[1].mC = 0;
    in.mFeatures[2].mC = 0;
    {
        NavGraph g;
        g.Build(in);
        EXPECT_TRUE(HasEdge(g, g.NodeAt(137, 100, false), g.NodeAt(137, 500, false), EdgeKind::eLift));
        EXPECT_TRUE(HasEdge(g, g.NodeAt(137, 500, false), g.NodeAt(137, 100, false), EdgeKind::eLift));
    }
}

// A floor that ends at `end`, a gap, and a ledge `rise` over it whose edge (grabbed going left... right here)
// starts `out` from the last cell
static NavInput HopToLedge(s32 out, s32 rise)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 125, 200, 0));
    in.mLines.push_back(Line(112 + out, 200 - rise, 400, 200 - rise, 0));
    NavFeature ledge = Feature(FeatureKind::eEdge, 112 + out, 200 - rise, 112 + out + 24, 224 - rise);
    ledge.mA = 1;
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    return in;
}

TEST(BotNav, AHopCatchesNoLedgeItHasDroppedBelowTheReachOf)
{
    // The Barracks' path 5: from 3637,1500 at the ledge 80 up that ends 88 off he fell past it into the pit. A hop
    // is level for three cells and then drops: 51 by 88 out.
    const auto caught = [](s32 out, s32 rise)
    {
        NavGraph g;
        g.Build(HopToLedge(out, rise));
        const s32 from = g.NodeAt(112, 200, false);
        for (const NavEdge& e : g.Edges())
        {
            if (e.mFrom == from && e.mKind == EdgeKind::eHop && g.Nodes()[e.mTo].mY == 200 - rise)
            {
                return true;
            }
        }
        return false;
    };
    // Three cells out (the edge starts 63 off): caught 80 up
    EXPECT_TRUE(caught(63, 80));
    // Four cells out (88 off): not 80 up, nor 40; level with him and a little over, yes
    EXPECT_FALSE(caught(88, 80));
    EXPECT_FALSE(caught(88, 40));
    EXPECT_TRUE(caught(88, 30));
    EXPECT_TRUE(caught(88, 0));
}

TEST(BotNav, ARunningJumpEndsAtTheLedgeItCatchesOnItsWayUpNotTheOneBeyond)
{
    // The Barracks' path 5 (3300 and 1300 off): off 3637,1500 over the pit, a ledge 80 up whose edge ends 88 off
    // and, under and beyond it, a floor at his own height with an edge of its own. He ends hung from the upper one.
    NavInput in;
    in.mLines.push_back(Line(325, 200, 550, 200, 0));
    in.mLines.push_back(Line(200, 120, 250, 120, 0));
    in.mLines.push_back(Line(150, 200, 225, 200, 0));
    NavFeature upper = Feature(FeatureKind::eEdge, 225, 120, 249, 144);
    upper.mA = 1;
    upper.mDir = 0;
    NavFeature lower = Feature(FeatureKind::eEdge, 200, 200, 233, 228);
    lower.mA = 1;
    lower.mDir = 0;
    in.mFeatures = {upper, lower};
    const auto jumps = [](const NavGraph& g, s32 y)
    {
        s32 count = 0;
        for (const NavEdge& e : g.Edges())
        {
            count += e.mKind == EdgeKind::eRunJump && g.Nodes()[e.mFrom].mY == 200 && g.Nodes()[e.mFrom].mX >= 325 && g.Nodes()[e.mTo].mY == y && g.Nodes()[e.mTo].mX < 300 ? 1 : 0;
        }
        return count;
    };
    {
        NavGraph g;
        g.Build(in);
        ASSERT_GE(g.NodeAt(237, 120, false), 0);
        ASSERT_GE(g.NodeAt(212, 200, false), 0);
        EXPECT_GE(jumps(g, 120), 1) << g.Text();
        EXPECT_EQ(jumps(g, 200), 0) << g.Text();
        // (off the last cell of the floor, 337: the edge ends 88 from it)
        const NavEdge* up = FindEdge(g, g.NodeAt(362, 200, false), g.NodeAt(237, 120, false), EdgeKind::eRunJump);
        ASSERT_NE(up, nullptr) << g.Text();
        EXPECT_EQ(up->mX, 337);
    }
    // An edge that ends 93 off is out of its reach: then it is the lower one the jump ends at
    in.mFeatures[0].mX2 = 244;
    {
        NavGraph g;
        g.Build(in);
        EXPECT_EQ(jumps(g, 120), 0) << g.Text();
        EXPECT_GE(jumps(g, 200), 1) << g.Text();
    }
    // As without the upper ledge
    in.mFeatures = {lower};
    {
        NavGraph g;
        g.Build(in);
        EXPECT_GE(jumps(g, 200), 1) << g.Text();
    }
}

static NavInput CorridorWithUxb()
{
    // Six UXBs in a row: too many to hop or to clear with a running jump
    NavInput in;
    in.mLines.push_back(Line(0, 100, 800, 100, 0));
    for (s32 i = 0; i < 6; i++)
    {
        NavFeature uxb = Feature(FeatureKind::eHazard, 300 + i * 25, 80, 324 + i * 25, 104);
        uxb.mSwitch = World::HazardSwitch(4000 + i);
        in.mFeatures.push_back(uxb);
    }
    NavFeature exit = Feature(FeatureKind::eDoor, 700, 80, 724, 104);
    exit.mDestPath = 2;
    exit.mDestCamera = 1;
    in.mFeatures.push_back(exit);
    return in;
}

TEST(BotDomain, AUxbInTheWayIsTurnedOffFromTheCellBesideItAndIsFloorAfter)
{
    NavGraph g;
    g.Build(CorridorWithUxb());
    World world;
    for (u32 i = 0; i < 6; i++)
    {
        Entity uxb = MakeEntity(EntityKind::eUxb, 4000 + i, 300 + i * 25, 80, 324 + i * 25, 104);
        uxb.mSwitch = World::HazardSwitch(uxb.mId);
        world.mEntities.push_back(uxb);
    }
    EXPECT_EQ(world.Switch(World::HazardSwitch(4000)), 0);
    const s32 start = g.NodeAt(12, 100, false);
    {
        const Domain domain = Domain::Build(g, world, start, nullptr);
        const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
        ASSERT_TRUE(plan.mFound) << plan.mMiss << "\n" << domain.Text() << g.Text();
        // The first of them first, from the cell on his side of it
        const DomainStep* first = nullptr;
        for (s32 step : plan.mSteps)
        {
            const DomainStep& s = domain.mSteps[domain.mActions[step].mTag];
            if (s.mKind == StepKind::eDisarm && !first)
            {
                first = &s;
            }
        }
        ASSERT_NE(first, nullptr) << domain.Text();
        EXPECT_EQ(first->mEntity, 4000u);
        EXPECT_EQ(g.Nodes()[first->mEdge].mX, 287);
        EXPECT_EQ(g.Nodes()[first->mEdge].mY, 100);
        // From either side of each there is a way to do it; from a cell with a UXB of its own, once that is off
        s32 ways = 0;
        bool fromNext = false;
        for (const Goap::Action& a : domain.mActions)
        {
            const DomainStep& s = domain.mSteps[a.mTag];
            if (s.mKind != StepKind::eDisarm)
            {
                continue;
            }
            ways++;
            ASSERT_EQ(a.mEffects.size(), 1u);
            EXPECT_EQ(a.mEffects[0].mKey, Key::Switch(World::HazardSwitch(s.mEntity)));
            EXPECT_EQ(a.mEffects[0].mValue, 1);
            if (s.mEntity == 4000 && g.Nodes()[s.mEdge].mX == 337)
            {
                fromNext = true;
                ASSERT_EQ(a.mPre.size(), 2u);
                EXPECT_EQ(a.mPre[1].mKey, Key::Switch(World::HazardSwitch(4001)));
                EXPECT_EQ(a.mPre[1].mValue, 1);
            }
            if (s.mEntity == 4000 && g.Nodes()[s.mEdge].mX == 287)
            {
                EXPECT_EQ(a.mPre.size(), 1u);
            }
        }
        EXPECT_EQ(ways, 12);
        EXPECT_TRUE(fromNext);
    }
    // Off, they are floor: nothing to turn off, and walked over
    for (Entity& e : world.mEntities)
    {
        e.mGone = true;
    }
    EXPECT_EQ(world.Switch(World::HazardSwitch(4000)), 1);
    {
        const Domain domain = Domain::Build(g, world, start, nullptr);
        const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
        ASSERT_TRUE(plan.mFound) << plan.mMiss;
        for (const DomainStep& s : domain.mSteps)
        {
            EXPECT_NE(s.mKind, StepKind::eDisarm);
        }
    }
    // With no UXB the world knows of (AO, where the bot does not turn them off) there is no way through
    {
        World none;
        const Domain domain = Domain::Build(g, none, start, nullptr);
        const Goap::Plan plan = Goap::Planner::Find(domain.mStart, {{Key::kExited, 1}}, domain.mActions);
        EXPECT_FALSE(plan.mFound);
    }
}

TEST(BotMines, WhereHeIsFrameByFrameOfARunningJumpThatEndsHungFromALedgeIsAsMeasured)
{
    // The Barracks' path 5, off 2012,1660 to the ledge at 2137,1660 with a flying mine going round under it: run to
    // where he takes off, 16 frames through the air (up to 31 over where he left, 15 under it as he catches the
    // ledge), then hung under the ledge and up over it in 26
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(200, 100, 300, 100, 0));
    NavFeature ledge = Feature(FeatureKind::eEdge, 200, 100, 224, 124);
    ledge.mA = 1;
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    const NavEdge* jump = FindEdge(g, g.NodeAt(12, 100, false), g.NodeAt(212, 100, false), EdgeKind::eRunJump);
    ASSERT_NE(jump, nullptr) << g.Text();
    ASSERT_EQ(jump->mX, 87);
    const std::vector<GoToTask::Body> bodies = BodiesOf(g, jump, false);
    // (the run to the take-off, 75 at 6.25 a frame, then 16 and 26)
    const std::size_t run = 12;
    ASSERT_EQ(bodies.size(), run + 16 + 26);
    EXPECT_EQ(bodies[0].mX, 12);
    EXPECT_EQ(bodies[6].mX, 49);
    EXPECT_EQ(bodies[0].mY, 100);
    EXPECT_EQ(bodies[0].mDown, 0);
    // In the air: up to 31 over the floor in the middle of it, under it at the end, his box 69 high
    EXPECT_EQ(bodies[run].mY, 100);
    EXPECT_EQ(bodies[run + 8].mY, 69);
    EXPECT_EQ(bodies[run + 15].mY, 115);
    EXPECT_EQ(bodies[run + 8].mUp, 69);
    EXPECT_GT(bodies[run + 8].mX, 87);
    EXPECT_LT(bodies[run + 8].mX, 201);
    EXPECT_EQ(bodies[run + 15].mX, 201);
    // Hung: at the ledge, his box from 5 under it to 65 under it, then up
    EXPECT_EQ(bodies[run + 16].mX, 212);
    EXPECT_EQ(bodies[run + 16].mY, 100);
    EXPECT_EQ(bodies[run + 16].mUp, -5);
    EXPECT_EQ(bodies[run + 16].mDown, 65);
    EXPECT_EQ(bodies[run + 16 + 3].mDown, 70);
    EXPECT_EQ(bodies[run + 16 + 7].mDown, 34);
    EXPECT_EQ(bodies[run + 16 + 7].mUp, 36);
    EXPECT_EQ(bodies[run + 16 + 19].mDown, 11);
    EXPECT_EQ(bodies[run + 16 + 25].mUp, 53);
}

TEST(BotNav, FromALiftsPlatformALedgeIsHoppedTo)
{
    // The Barracks' path 5: off the lift at 1362,960 to the ledge at 1437,900, the walkthrough's "hop right and
    // hoist up". (He is only ever on the platform where the lift is.)
    NavInput in;
    in.mLines.push_back(Line(0, 200, 100, 200, 0));
    in.mLines.push_back(Line(225, 140, 325, 140, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 200, 176, 224));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 500, 176, 524));
    NavFeature ledge = Feature(FeatureKind::eEdge, 225, 140, 249, 164);
    ledge.mA = 1;
    ledge.mDir = 1;
    in.mFeatures.push_back(ledge);
    NavGraph g;
    g.Build(in);
    const s32 on = g.NodeAt(162, 200, false);
    const s32 up = g.NodeAt(237, 140, false);
    ASSERT_GE(on, 0);
    ASSERT_GE(up, 0);
    ASSERT_GE(g.Nodes()[on].mLiftStop, 0);
    EXPECT_NE(FindEdge(g, on, up, EdgeKind::eHop), nullptr) << g.Text();
}

TEST(BotMines, WhereHeIsFrameByFrameOfARideOnALiftIsAsMeasured)
{
    // The Barracks' path 5, the lift at 1362 from 1260 up: 7 frames until it moves, then 4 a frame
    NavInput in;
    in.mLines.push_back(Line(0, 100, 100, 100, 0));
    in.mLines.push_back(Line(0, 300, 100, 300, 0));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 100, 176, 124));
    in.mFeatures.push_back(Feature(FeatureKind::eLiftStop, 100, 300, 176, 324));
    NavGraph g;
    g.Build(in);
    const NavEdge* up = FindEdge(g, g.NodeAt(137, 300, false), g.NodeAt(137, 100, false), EdgeKind::eLift);
    ASSERT_NE(up, nullptr);
    const std::vector<GoToTask::Body> bodies = BodiesOf(g, up, false);
    ASSERT_EQ(bodies.size(), 7u + 50u);
    EXPECT_EQ(bodies[0].mY, 300);
    EXPECT_EQ(bodies[6].mY, 300);
    EXPECT_EQ(bodies[7].mY, 300);
    EXPECT_EQ(bodies[8].mY, 296);
    EXPECT_EQ(bodies[32].mY, 200);
    EXPECT_EQ(bodies[56].mY, 104);
    for (const GoToTask::Body& b : bodies)
    {
        EXPECT_EQ(b.mX, 137);
        EXPECT_EQ(b.mBack, 23);
        EXPECT_EQ(b.mFront, 23);
        EXPECT_EQ(b.mUp, 55);
    }
    // (and down)
    const NavEdge* down = FindEdge(g, g.NodeAt(137, 100, false), g.NodeAt(137, 300, false), EdgeKind::eLift);
    ASSERT_NE(down, nullptr);
    const std::vector<GoToTask::Body> lower = BodiesOf(g, down, false);
    ASSERT_EQ(lower.size(), 57u);
    EXPECT_EQ(lower[6].mY, 100);
    EXPECT_EQ(lower[32].mY, 200);
}

TEST(BotNav, OffAShortLedgeARunningJumpCatchesTheEdgeOfTheLedgeAcross)
{
    // The Barracks' path 7 (the ledges at 720 in its middle): a ledge three cells long, a gap five cells wide and
    // the edge of the ledge across, which faces him. With one cell of run he is hung from it.
    NavInput in;
    in.mLines.push_back(Line(300, 200, 375, 200, 0));
    in.mLines.push_back(Line(75, 200, 200, 200, 0));
    NavFeature edge = Feature(FeatureKind::eEdge, 175, 200, 205, 224);
    edge.mA = 1;
    edge.mDir = 0;
    in.mFeatures.push_back(edge);
    {
        NavGraph g;
        g.Build(in);
        const s32 from = g.NodeAt(337, 200, false);
        const s32 to = g.NodeAt(187, 200, false);
        ASSERT_GE(from, 0);
        ASSERT_GE(to, 0);
        const NavEdge* jump = FindEdge(g, from, to, EdgeKind::eRunJump);
        ASSERT_NE(jump, nullptr) << g.Text();
        EXPECT_EQ(jump->mX, 312);
        EXPECT_EQ(jump->mFeature, 0);
        EXPECT_TRUE(jump->mLeft);
    }
    // An edge that is only caught going the other way is none
    in.mFeatures[0].mDir = 1;
    {
        NavGraph g;
        g.Build(in);
        EXPECT_EQ(FindEdge(g, g.NodeAt(337, 200, false), g.NodeAt(187, 200, false), EdgeKind::eRunJump), nullptr) << g.Text();
    }
    // Nor one a cell further off
    in.mFeatures[0].mDir = 0;
    in.mFeatures[0].mX1 = 150;
    in.mFeatures[0].mX2 = 180;
    {
        NavGraph g;
        g.Build(in);
        for (const NavEdge& e : g.Edges())
        {
            EXPECT_FALSE(e.mKind == EdgeKind::eRunJump && g.Nodes()[e.mFrom].mX >= 300 && g.Nodes()[e.mTo].mX < 300) << g.Text();
        }
    }
}

TEST(BotDomain, TheFloorByALockerForGunsIsKeptOffWhileANakedSligThatIsAwakeCanGetToIt)
{
    // The Barracks' Slig bunks: naked Sligs with a panic switch, a locker along their floor, and something shut
    // between one of them and it
    World world;
    Entity locker = MakeEntity(EntityKind::ePantsLocker, 7000, 500, 80, 524, 104);
    locker.mSwitch = World::HazardSwitch(locker.mId);
    world.mEntities.push_back(locker);
    Entity asleep = MakeEntity(EntityKind::eCrawler, 7001, 100, 80, 124, 104);
    asleep.mInputs[0] = 38;
    asleep.mInputs[1] = 7000;
    world.mEntities.push_back(asleep);
    Entity penned = MakeEntity(EntityKind::eCrawler, 7002, 800, 80, 824, 104);
    penned.mInputs[0] = 1;
    penned.mInputs[1] = 7000;
    penned.mInputs[2] = 79;
    penned.mInputs[3] = 1;
    world.mEntities.push_back(penned);
    const s32 sw = World::HazardSwitch(7000);
    // Asleep, and the one that is always awake shut in: clear
    EXPECT_EQ(world.Switch(sw), 1);
    // The panic switch on: not clear, until that one is gone
    world.mSwitches[38] = 1;
    EXPECT_EQ(world.Switch(sw), 0);
    world.mEntities[1].mGone = true;
    EXPECT_EQ(world.Switch(sw), 1);
    // What shut the other in open: not clear, until that one is gone
    world.mSwitches[79] = 1;
    EXPECT_EQ(world.Switch(sw), 0);
    world.mEntities[2].mGone = true;
    EXPECT_EQ(world.Switch(sw), 1);
    // An armed Slig about by the locker, awake and not his: not clear
    SligAt slig;
    slig.mX = 700;
    slig.mY = 100;
    world.mSligs.push_back(slig);
    EXPECT_EQ(world.Switch(sw), 0);
    world.mSligs[0].mX = 512 + World::kLockerReach + 1;
    EXPECT_EQ(world.Switch(sw), 1);
    world.mSligs[0].mX = 700;
    world.mSligs[0].mPossessed = true;
    EXPECT_EQ(world.Switch(sw), 1);
    world.mSligs[0].mPossessed = false;
    world.mSligs[0].mY = 200;
    EXPECT_EQ(world.Switch(sw), 1);
    // (one of another locker's is nothing to this one)
    Entity other = MakeEntity(EntityKind::eCrawler, 7003, 300, 80, 324, 104);
    other.mInputs[0] = 1;
    other.mInputs[1] = 7999;
    world.mEntities.push_back(other);
    EXPECT_EQ(world.Switch(sw), 1);
}

// A pen: the stretch 0..374 of the floor at 100, with one Slog the map puts there awake
static World PenWorld()
{
    World world;
    Entity slog = MakeEntity(EntityKind::eSlog, 7, 100, 80, 124, 100);
    slog.mSwitch = World::HazardSwitch(7);
    slog.mInputs[0] = 60;
    slog.mInputs[2] = 0;
    slog.mInputs[3] = 374;
    slog.mInputs[4] = 100;
    world.mEntities.push_back(slog);
    return world;
}

TEST(BotSlogs, APenIsOpenWithABoneOnItOrWithItsSlogsOutAtABoneOnTheScreenShown)
{
    World world = PenWorld();
    const s32 sw = world.mEntities[0].mSwitch;
    EXPECT_EQ(world.Switch(sw), 0);
    RockAt bone;
    bone.mX = 200;
    bone.mY = 100;
    world.mBones.push_back(bone);
    EXPECT_EQ(world.Switch(sw), 1);
    // A bone off the stretch, on the screen shown: nothing to a Slog that waits where the map put it
    world.mBones[0].mX = 500;
    world.mBones[0].mShown = true;
    EXPECT_EQ(world.Switch(sw), 0);
    EXPECT_FALSE(world.SlogsOut(world.mEntities[0]));
    // The Slog is out after him: it goes for that bone, and the stretch is empty
    world.mEntities[0].mLive = true;
    world.mEntities[0].mState = 1;
    EXPECT_TRUE(world.SlogsOut(world.mEntities[0]));
    EXPECT_EQ(world.Switch(sw), 1);
    // (not for a bone on another screen)
    world.mBones[0].mShown = false;
    EXPECT_EQ(world.Switch(sw), 0);
    // (nor with a second Slog of the stretch still where it was put)
    world.mBones[0].mShown = true;
    Entity second = world.mEntities[0];
    second.mId = 8;
    second.mSwitch = World::HazardSwitch(8);
    second.mLive = true;
    second.mState = 0;
    world.mEntities.push_back(second);
    EXPECT_FALSE(world.SlogsOut(world.mEntities[0]));
    EXPECT_EQ(world.Switch(sw), 0);
}

TEST(BotSlogs, ASlogThatIsAfterHimIsFedByABoneOnTheScreenShownAndTheFloorBetweenPensIsShutWithoutOne)
{
    World world;
    SlogAt slog;
    slog.mX = 450;
    slog.mY = 100;
    world.mSlogs.push_back(slog);
    World::SlogFloor floor;
    floor.mSwitch = World::SlogFloorSwitch(100, 1);
    floor.mX1 = 375;
    floor.mX2 = 749;
    floor.mFloorY = 100;
    world.mSlogFloors.push_back(floor);
    // No Slog after him: the stretch is free to walk
    EXPECT_EQ(world.Switch(floor.mSwitch), 1);
    EXPECT_FALSE(world.SlogFed(world.mSlogs[0]));
    world.mSlogs[0].mChasing = true;
    EXPECT_EQ(world.Switch(floor.mSwitch), 0);
    RockAt bone;
    bone.mX = 600;
    bone.mY = 100;
    world.mBones.push_back(bone);
    // (a bone that is not on the screen shown is none)
    EXPECT_EQ(world.Switch(floor.mSwitch), 0);
    EXPECT_FALSE(world.SlogFed(world.mSlogs[0]));
    world.mBones[0].mShown = true;
    EXPECT_EQ(world.Switch(floor.mSwitch), 1);
    EXPECT_TRUE(world.SlogFed(world.mSlogs[0]));
    // (one that waits where the map put it takes no notice of that bone)
    world.mSlogs[0].mChasing = false;
    EXPECT_FALSE(world.SlogFed(world.mSlogs[0]));
    world.mSlogs[0].mChasing = true;
    // (one on another floor is none either)
    world.mBones[0].mY = 400;
    EXPECT_EQ(world.Switch(floor.mSwitch), 0);
}

// A lever (switch 51) on the corridor's near side; a timer trigger's two outputs (105, 106) that follow it; and a
// logic that toggles 107 when 105 and 106 change together within its delay
static World ToggleWorld()
{
    World world;
    Entity lever = MakeEntity(EntityKind::eLever, 1, 50, 80, 124, 104);
    lever.mSwitch = 51;
    Entity outA = MakeEntity(EntityKind::eSwitchLogic, 20, 0, 0, 10, 10);
    outA.mHoldOnly = true;
    outA.mInputs[0] = 51;
    outA.mSwitch = 105;
    Entity outB = outA;
    outB.mId = 21;
    outB.mSwitch = 106;
    Entity logic = MakeEntity(EntityKind::eSwitchLogic, 22, 0, 0, 10, 10);
    logic.mSwitch = 107;
    logic.mOp = 2;
    logic.mState = 30;
    logic.mInputs[0] = 105;
    logic.mInputs[1] = 106;
    world.mEntities = {lever, outA, outB, logic};
    return world;
}

static const Goap::Action* ActionNamed(const Domain& domain, const std::string& start)
{
    for (const Goap::Action& a : domain.mActions)
    {
        if (a.mName.rfind(start, 0) == 0)
        {
            return &a;
        }
    }
    return nullptr;
}

static bool Gives(const Goap::Action& a, u32 key, s32 value)
{
    return std::find(a.mEffects.begin(), a.mEffects.end(), Goap::Fact{key, value}) != a.mEffects.end();
}

static bool Wants(const Goap::Action& a, u32 key, s32 value)
{
    return std::find(a.mPre.begin(), a.mPre.end(), Goap::Fact{key, value}) != a.mPre.end();
}

TEST(BotDomain, ATimerTriggerTogglesItsOutputsAndALogicThatTogglesTurnsItsOutputBack)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world = ToggleWorld();
    // Its outputs off: they come on with it, and the logic's output is left alone (it is off)
    Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    const Goap::Action* on = ActionNamed(domain, "Use Lever#1 sw51 on");
    ASSERT_NE(on, nullptr) << domain.Text();
    EXPECT_TRUE(Gives(*on, Key::Switch(105), 1));
    EXPECT_TRUE(Gives(*on, Key::Switch(106), 1));
    EXPECT_FALSE(Gives(*on, Key::Switch(107), 0));
    // Its outputs on, and the logic's output with them: toggled they go off, both at once, and the output with them
    world.mSwitches[105] = 1;
    world.mSwitches[106] = 1;
    world.mSwitches[107] = 1;
    domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    on = ActionNamed(domain, "Use Lever#1 sw51 on");
    ASSERT_NE(on, nullptr) << domain.Text();
    EXPECT_TRUE(Gives(*on, Key::Switch(105), 0));
    EXPECT_TRUE(Gives(*on, Key::Switch(106), 0));
    EXPECT_TRUE(Gives(*on, Key::Switch(107), 0));
    EXPECT_FALSE(Gives(*on, Key::Switch(105), 1));
}

// Two levers (switches 10 and 11) and a logic that wants them within `delay` frames of each other
static World TwoLevers(s32 delay)
{
    World world;
    Entity leverA = MakeEntity(EntityKind::eLever, 1, 25, 80, 99, 104);
    leverA.mSwitch = 10;
    Entity leverB = MakeEntity(EntityKind::eLever, 2, 100, 80, 174, 104);
    leverB.mSwitch = 11;
    Entity logic = MakeEntity(EntityKind::eSwitchLogic, 3, 0, 0, 10, 10);
    logic.mSwitch = 5;
    logic.mOp = 2;
    logic.mState = delay;
    logic.mInputs[0] = 10;
    logic.mInputs[1] = 11;
    world.mEntities = {leverA, leverB, logic};
    return world;
}

TEST(BotDomain, LeversThatOnlyCountPulledWithinAShortWhileOfEachOtherAreForAMudokonAndHim)
{
    NavGraph g;
    g.Build(CorridorWithDoor());
    World world = TwoLevers(30);
    Team team;
    std::vector<Goap::Fact> pre;
    // No Mudokon: nobody for the second lever
    EXPECT_FALSE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
    world.mEntities.push_back(MakeEntity(EntityKind::eMudokon, 4, 300, 80, 324, 100));
    ASSERT_TRUE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
    EXPECT_EQ(team.mWheels.size(), 2u);
    ASSERT_EQ(team.mMuds.size(), 1u);
    EXPECT_EQ(team.mMuds[0], 4u);
    // (his own is the one furthest from where the Mudokon comes from)
    EXPECT_EQ(team.mAbeWheel, 1u);
    const Domain domain = Domain::Build(g, world, g.NodeAt(12, 100, false), nullptr);
    EXPECT_NE(ActionNamed(domain, "Work together"), nullptr) << domain.Text();
    // With time enough between them he pulls both himself
    world.mEntities[2].mState = 200;
    EXPECT_FALSE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
    // (and a logic with no delay at all is not one of these)
    world.mEntities[2].mState = 0;
    EXPECT_FALSE(Domain::PlanTogether(g, world, world.mEntities[2], team, pre));
}

// A floor at 200 he starts on, and one at 100 over its right half that a Slig walks, with a hoist up to its left
// end and an electric wall (switch 30, off) across it
static NavInput ZapRoom()
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 400, 200, 0));
    in.mLines.push_back(Line(150, 100, 400, 100, 0));
    NavFeature hoist = Feature(FeatureKind::eHoist, 150, 100, 174, 200);
    hoist.mDir = 2;
    in.mFeatures.push_back(hoist);
    NavFeature wall = Feature(FeatureKind::eBarrier, 250, 60, 274, 100);
    wall.mBeam = true;
    wall.mSwitch = 30;
    in.mFeatures.push_back(wall);
    return in;
}

TEST(BotDomain, ElectricWallsAcrossASligsFloorAreTheWayToBeRidOfItAndTheFloorIsDearUntilTheyHave)
{
    NavGraph g;
    g.Build(ZapRoom());
    World world;
    world.mEntities.push_back(MakeEntity(EntityKind::eSlig, 9, 325, 76, 349, 100));
    Domain domain = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    const Goap::Action* zap = ActionNamed(domain, "ElectricWall sw30 gets Slig#9");
    ASSERT_NE(zap, nullptr) << domain.Text();
    EXPECT_TRUE(Wants(*zap, Key::Switch(30), 1));
    EXPECT_TRUE(Gives(*zap, Key::Mud(9), 2));
    EXPECT_EQ(domain.mSteps[zap->mTag].mKind, StepKind::eZapWait);
    // Along that floor (the wall is where its regions meet): one way with the Slig gone, and one without that
    // costs more
    s32 with = -1;
    s32 without = -1;
    for (const Goap::Action& a : domain.mActions)
    {
        const DomainStep& step = domain.mSteps[a.mTag];
        if (step.mKind == StepKind::eCross && g.Nodes()[g.Edges()[step.mEdge].mTo].mY == 100)
        {
            (Wants(a, Key::Mud(9), 2) ? with : without) = a.mCost;
        }
    }
    ASSERT_GE(with, 0) << domain.Text();
    ASSERT_GE(without, 0) << domain.Text();
    EXPECT_GT(without, with + 1000);
    // The wall on already and the Slig's beat not seen to go through it: nothing to wait for
    world.mSwitches[30] = 1;
    domain = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    EXPECT_EQ(ActionNamed(domain, "ElectricWall sw30 gets Slig#9"), nullptr);
    // (seen to: waited for)
    world.mEntities[0].mLive = true;
    world.mEntities[0].mLiveX = 337;
    world.mEntities[0].mLiveY = 100;
    SligAt slig;
    slig.mX = 337;
    slig.mY = 100;
    slig.mBeatMin = 200;
    slig.mBeatMax = 380;
    world.mSligs.push_back(slig);
    domain = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    EXPECT_NE(ActionNamed(domain, "ElectricWall sw30 gets Slig#9"), nullptr) << domain.Text();
    // (one whose beat stops short of the wall is not)
    world.mSligs[0].mBeatMin = 300;
    domain = Domain::Build(g, world, g.NodeAt(12, 200, false), nullptr);
    EXPECT_EQ(ActionNamed(domain, "ElectricWall sw30 gets Slig#9"), nullptr);
}

// A floor at 200 with a ledge 80 over its right part and a hoist up to the ledge's left end at 162; `wall`: a
// wall at 75 that cuts the floor off behind the cell three short of the hoist
static NavInput StandJumpRoom(bool wall)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 400, 200, 0));
    in.mLines.push_back(Line(150, 120, 400, 120, 0));
    NavFeature hoist = Feature(FeatureKind::eHoist, 150, 120, 174, 200);
    hoist.mDir = 2;
    in.mFeatures.push_back(hoist);
    if (wall)
    {
        in.mLines.push_back(Line(75, 100, 75, 200, 1));
    }
    return in;
}

static bool HasStandJump(const NavGraph& g, s32 from, s32 to)
{
    for (const NavEdge& e : g.Edges())
    {
        if (e.mFrom == from && e.mTo == to && e.mKind == EdgeKind::eRunJump && e.mCost == NavGraph::kStandJumpCost)
        {
            return true;
        }
    }
    return false;
}

TEST(BotNav, ARunningJumpUpAHoistFromAStandIsThereOnlyWhereTheFloorBehindIsNoRunUp)
{
    NavGraph walled;
    walled.Build(StandJumpRoom(true));
    const s32 from = walled.NodeAt(87, 200, false);
    const s32 top = walled.NodeAt(162, 120, false);
    ASSERT_GE(from, 0);
    ASSERT_GE(top, 0);
    EXPECT_TRUE(HasStandJump(walled, from, top));
    // (from three cells short of the hoist, and nowhere else)
    EXPECT_FALSE(HasStandJump(walled, walled.NodeAt(112, 200, false), top));
    NavGraph open;
    open.Build(StandJumpRoom(false));
    EXPECT_FALSE(HasStandJump(open, open.NodeAt(87, 200, false), open.NodeAt(162, 120, false)));
}

// A floor two screens long at 200 (a screen is 375 wide), and a ledge at 120 from over the first screen's end to the
// middle of the second, with a hoist up to it. `pen`: the first screen is a Slog's stretch (the map's Slog 7), come onto along the floor.
static NavInput TwoScreenFloor(bool pen)
{
    NavInput in;
    in.mLines.push_back(Line(0, 200, 749, 200, 0));
    // (the ledge starts over the first screen's last cells, with a wall at its end down to a tunnel's height over
    // the floor: nothing comes down off it onto the first screen)
    in.mLines.push_back(Line(325, 120, 600, 120, 0));
    in.mLines.push_back(Line(325, 40, 325, 160, 1));
    in.mLines.push_back(Line(325, 40, 325, 160, 2));
    NavFeature hoist = Feature(FeatureKind::eHoist, 450, 120, 474, 200);
    hoist.mDir = 2;
    in.mFeatures.push_back(hoist);
    if (pen)
    {
        NavFeature stretch = Feature(FeatureKind::eDanger, 0, 140, 374, 206);
        stretch.mA = 1;
        stretch.mId = 7;
        stretch.mSwitch = World::HazardSwitch(7);
        in.mFeatures.push_back(stretch);
    }
    return in;
}

TEST(BotSlogs, ABoneIsNotThrownWhereItWouldRollOffTheScreen)
{
    NavGraph g;
    g.Build(TwoScreenFloor(false));
    // From the floor itself, along which a bone rolls half a screen (220): whatever throw is taken from any cell of
    // the first screen, the bone is still on that screen when it has rolled
    s32 thrown = 0;
    for (s32 x = 62; x <= 337; x += 25)
    {
        const s32 at = g.NodeAt(x, 200, false);
        ASSERT_GE(at, 0);
        u32 aim = 0;
        s32 frames = 0;
        s32 downX = 0;
        if (Domain::BoneThrow(g, at, 0, 374, 200, aim, frames, &downX))
        {
            thrown++;
            const s32 rest = downX + (aim % 2 == 1 ? -220 : 220);
            EXPECT_GE(rest, 0) << "from " << x << " down at " << downX;
            EXPECT_LE(rest, 374) << "from " << x << " down at " << downX;
        }
    }
    EXPECT_GT(thrown, 0);
    // (a stretch that ends at a wall short of the screen's edge stops it: more throws do)
    s32 walled = 0;
    for (s32 x = 62; x <= 337; x += 25)
    {
        u32 aim = 0;
        s32 frames = 0;
        walled += Domain::BoneThrow(g, g.NodeAt(x, 200, false), 26, 349, 200, aim, frames) ? 1 : 0;
    }
    EXPECT_GT(walled, thrown);
}

TEST(BotSlogs, AStretchComeOntoAlongTheFloorHasItsSlogsDrawnOutAndTheBoneThrownAwayFromIt)
{
    NavGraph g;
    g.Build(TwoScreenFloor(true));
    World world = PenWorld();
    world.mEntities[0].mInputs[4] = 200;
    world.mEntities[0].mY1 = 180;
    world.mEntities[0].mY2 = 200;
    world.mRocks = 2;
    const s32 abe = g.NodeAt(600, 200, false);
    ASSERT_GE(abe, 0);
    Domain domain = Domain::Build(g, world, abe, nullptr);
    const Goap::Action* draw = ActionNamed(domain, "Draw the Slogs out of");
    ASSERT_NE(draw, nullptr) << domain.Text();
    EXPECT_EQ(domain.mSteps[draw->mTag].mKind, StepKind::eDrawSlogs);
    EXPECT_TRUE(Wants(*draw, Key::SlogsOut(7), 0));
    EXPECT_TRUE(Gives(*draw, Key::SlogsOut(7), 1));
    const Goap::Action* away = ActionNamed(domain, "Throw a bone away from");
    ASSERT_NE(away, nullptr) << domain.Text();
    EXPECT_EQ(domain.mSteps[away->mTag].mKind, StepKind::eFeedAway);
    EXPECT_TRUE(Wants(*away, Key::SlogsOut(7), 1));
    EXPECT_TRUE(Gives(*away, Key::Switch(World::HazardSwitch(7)), 1));
    // (two bones: one buys too little)
    EXPECT_EQ(away->mSpend.mValue, 2);
    // (from over the floor, not from on it)
    EXPECT_LE(g.Nodes()[domain.mSteps[away->mTag].mEdge].mY, 150);
    // And no bone thrown onto the stretch from out there: he would have to get round to it
    EXPECT_EQ(ActionNamed(domain, "Throw a bone to the Slogs at 0..374"), nullptr) << domain.Text();
    // (already out: said so at the start)
    world.mEntities[0].mLive = true;
    world.mEntities[0].mState = 1;
    domain = Domain::Build(g, world, abe, nullptr);
    EXPECT_EQ(domain.mStart.Get(Key::SlogsOut(7)), 1);
}

TEST(BotSlogs, ASlogLooseOnAFloorWithNoStretchIsBaitedUnderAThingThatFallsFromClearOfIt)
{
    NavGraph g;
    g.Build(TwoScreenFloor(false));
    World world;
    world.mRocks = 1;
    const s32 abe = g.NodeAt(600, 120, false);
    ASSERT_GE(abe, 0);
    // Nothing after him: nothing loose
    Domain domain = Domain::Build(g, world, abe, nullptr);
    EXPECT_EQ(domain.mStart.Get(Key::kSlogLoose), 0);
    SlogAt slog;
    slog.mX = 500;
    slog.mY = 200;
    slog.mChasing = true;
    world.mSlogs.push_back(slog);
    domain = Domain::Build(g, world, abe, nullptr);
    EXPECT_EQ(domain.mStart.Get(Key::kSlogLoose), 1);
    // (one that is awake and not after him is not loose on him)
    world.mSlogs[0].mChasing = false;
    domain = Domain::Build(g, world, abe, nullptr);
    EXPECT_EQ(domain.mStart.Get(Key::kSlogLoose), 0);
    world.mSlogs[0].mChasing = true;
    domain = Domain::Build(g, world, abe, nullptr);
    // No thing that falls: nothing to do about it
    EXPECT_EQ(ActionNamed(domain, "Throw a bone under the falling thing"), nullptr);
    // One that falls onto its floor just past the ledge's end (the ledge's last cell is within its reach)
    RockAt faller;
    faller.mX = 637;
    faller.mY = 20;
    faller.mFallIn = 10;
    world.mFallers.push_back(faller);
    domain = Domain::Build(g, world, abe, nullptr);
    const Goap::Action* bait = ActionNamed(domain, "Throw a bone under the falling thing at 637");
    ASSERT_NE(bait, nullptr) << domain.Text();
    EXPECT_EQ(domain.mSteps[bait->mTag].mKind, StepKind::eBaitSlog);
    EXPECT_TRUE(Wants(*bait, Key::kSlogLoose, 1));
    EXPECT_TRUE(Gives(*bait, Key::kSlogLoose, 0));
    EXPECT_EQ(bait->mSpend.mValue, 1);
    // (thrown from the ledge, and from clear of where it comes down)
    const NavNode& stand = g.Nodes()[domain.mSteps[bait->mTag].mEdge];
    EXPECT_EQ(stand.mY, 120);
    EXPECT_GE(std::abs(stand.mX - 637), 60);
    // (one whose reach the ledge's last cell is in: not thrown under from that cell, though it is the one the
    // bone would lie nearest from)
    for (s32 fx = 560; fx <= 700; fx += 5)
    {
        world.mFallers[0].mX = fx;
        domain = Domain::Build(g, world, abe, nullptr);
        if (const Goap::Action* close = ActionNamed(domain, "Throw a bone under the falling thing at " + std::to_string(fx)))
        {
            EXPECT_GE(std::abs(g.Nodes()[domain.mSteps[close->mTag].mEdge].mX - fx), 60) << fx;
        }
    }
    world.mFallers[0].mX = 637;
    // One that falls once only (no interval known) is no such thing
    world.mFallers[0].mFallIn = -1;
    domain = Domain::Build(g, world, abe, nullptr);
    EXPECT_EQ(ActionNamed(domain, "Throw a bone under the falling thing"), nullptr);
    // And on a floor with a stretch of Slogs' own the bones are what opens and shuts it: not loose
    world.mFallers[0].mFallIn = 10;
    World penned = PenWorld();
    penned.mEntities[0].mInputs[4] = 200;
    penned.mSlogs = world.mSlogs;
    penned.mFallers = world.mFallers;
    domain = Domain::Build(g, penned, abe, nullptr);
    EXPECT_EQ(domain.mStart.Get(Key::kSlogLoose), 0);
}
