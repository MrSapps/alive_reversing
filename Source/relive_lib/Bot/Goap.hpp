#pragma once

#include "../Types.hpp"
#include <string>
#include <vector>

// Goal oriented action planning: a world of facts, actions with what they need and what they change, and a search
// for the cheapest list of actions that makes the goal true. Knows nothing about the games.
namespace Bot::Goap {

struct Fact final
{
    u32 mKey = 0;
    s32 mValue = 0;

    bool operator==(const Fact& rhs) const
    {
        return mKey == rhs.mKey && mValue == rhs.mValue;
    }
};

// Fact values by key, kept in key order so two equal states are equal memory. A key that isn't there is 0.
class State final
{
public:
    s32 Get(u32 key) const;
    void Set(u32 key, s32 value);
    bool Has(const Fact& fact) const
    {
        return Get(fact.mKey) == fact.mValue;
    }
    bool HasAll(const std::vector<Fact>& facts) const;
    bool operator==(const State& rhs) const
    {
        return mFacts == rhs.mFacts;
    }
    bool operator<(const State& rhs) const;
    const std::vector<Fact>& Facts() const
    {
        return mFacts;
    }

private:
    std::vector<Fact> mFacts;
};

struct Action final
{
    std::string mName;
    std::vector<Fact> mPre;
    std::vector<Fact> mEffects;
    s32 mCost = 1;
    // What it uses up of something that is counted (rocks): the fact's key and how many. It needs at least that
    // many and leaves that many less. Key 0: nothing.
    Fact mSpend;
    // Whatever the maker of the action wants to find it by
    s32 mTag = 0;
};

struct Plan final
{
    bool mFound = false;
    // Indices into the actions, in the order to do them
    std::vector<s32> mSteps;
    s32 mCost = 0;
    // When there's none: the goal facts nothing can make true, or why the search gave up
    std::string mMiss;
    s32 mExpanded = 0;
};

class Planner final
{
public:
    // The cheapest plan from start to a state with every goal fact. Actions that can't help the goal are left out
    // of the search, and equal-cost plans come out the same way every time.
    // That holds for plans found within a few hundred states. A longer one is found by going for what looks nearest
    // the goal, and can cost somewhat more than the cheapest.
    static Plan Find(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, s32 maxExpanded = 5000);
    // Roughly what the goal costs from a state, without searching (it can be over or under): to tell which of
    // several goals is nearest. -1 if it can't be had.
    static s32 Short(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions);
    // Whether an action can be done in a state, and the state it leaves
    static bool Can(const State& state, const Action& action);
    static void Do(State& state, const Action& action);
    // The state a plan leaves behind
    static State Apply(const State& start, const Plan& plan, const std::vector<Action>& actions);
    // The actions that change a fact some goal fact needs, directly or through another action's needs
    static std::vector<bool> Relevant(const std::vector<Fact>& goal, const std::vector<Action>& actions);
};

} // namespace Bot::Goap
