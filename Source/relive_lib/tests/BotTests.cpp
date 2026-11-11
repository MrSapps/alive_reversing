#include <gtest/gtest.h>
#include "Bot/BotRng.hpp"
#include "Bot/Domain.hpp"
#include "Bot/Goap.hpp"
#include "Bot/LevelGraph.hpp"
#include "Bot/NavGraph.hpp"
#include "Bot/Pilot.hpp"
#include "Bot/Profile.hpp"
#include "Bot/Task.hpp"

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
    Entity wheelA = MakeEntity(EntityKind::eWorkWheel, 1, 100, 80, 124, 104);
    wheelA.mSwitch = 10;
    Entity wheelB = MakeEntity(EntityKind::eWorkWheel, 2, 150, 80, 174, 104);
    wheelB.mSwitch = 11;
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
