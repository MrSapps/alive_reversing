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
    static Plan Find(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, s32 maxExpanded = 20000);
    // The state a plan leaves behind
    static State Apply(const State& start, const Plan& plan, const std::vector<Action>& actions);
    // The actions that change a fact some goal fact needs, directly or through another action's needs
    static std::vector<bool> Relevant(const std::vector<Fact>& goal, const std::vector<Action>& actions);
};

} // namespace Bot::Goap
