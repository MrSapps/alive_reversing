#include "Goap.hpp"
#include <algorithm>
#include <map>
#include <queue>
#include <set>

namespace Bot::Goap {

s32 State::Get(u32 key) const
{
    const auto it = std::lower_bound(mFacts.begin(), mFacts.end(), key, [](const Fact& f, u32 k)
                                     { return f.mKey < k; });
    return it != mFacts.end() && it->mKey == key ? it->mValue : 0;
}

void State::Set(u32 key, s32 value)
{
    const auto it = std::lower_bound(mFacts.begin(), mFacts.end(), key, [](const Fact& f, u32 k)
                                     { return f.mKey < k; });
    if (it != mFacts.end() && it->mKey == key)
    {
        if (value == 0)
        {
            mFacts.erase(it);
        }
        else
        {
            it->mValue = value;
        }
    }
    else if (value != 0)
    {
        mFacts.insert(it, Fact{key, value});
    }
}

bool State::HasAll(const std::vector<Fact>& facts) const
{
    for (const Fact& f : facts)
    {
        if (!Has(f))
        {
            return false;
        }
    }
    return true;
}

bool State::operator<(const State& rhs) const
{
    return std::lexicographical_compare(mFacts.begin(), mFacts.end(), rhs.mFacts.begin(), rhs.mFacts.end(), [](const Fact& a, const Fact& b)
                                        { return a.mKey != b.mKey ? a.mKey < b.mKey : a.mValue < b.mValue; });
}

std::vector<bool> Planner::Relevant(const std::vector<Fact>& goal, const std::vector<Action>& actions)
{
    std::set<u32> keys;
    for (const Fact& f : goal)
    {
        keys.insert(f.mKey);
    }
    std::vector<bool> relevant(actions.size(), false);
    bool grew = true;
    while (grew)
    {
        grew = false;
        for (std::size_t i = 0; i < actions.size(); i++)
        {
            if (relevant[i])
            {
                continue;
            }
            // (using something up changes how much there is of it)
            std::vector<Fact> changes = actions[i].mEffects;
            if (actions[i].mSpend.mKey)
            {
                changes.push_back(actions[i].mSpend);
            }
            for (const Fact& e : changes)
            {
                if (keys.count(e.mKey))
                {
                    relevant[i] = true;
                    grew = true;
                    for (const Fact& p : actions[i].mPre)
                    {
                        keys.insert(p.mKey);
                    }
                    if (actions[i].mSpend.mKey)
                    {
                        keys.insert(actions[i].mSpend.mKey);
                    }
                    break;
                }
            }
        }
    }
    return relevant;
}

bool Planner::Can(const State& state, const Action& action)
{
    return state.HasAll(action.mPre) && (action.mSpend.mKey == 0 || state.Get(action.mSpend.mKey) >= action.mSpend.mValue);
}

void Planner::Do(State& state, const Action& action)
{
    if (action.mSpend.mKey)
    {
        state.Set(action.mSpend.mKey, state.Get(action.mSpend.mKey) - action.mSpend.mValue);
    }
    for (const Fact& effect : action.mEffects)
    {
        state.Set(effect.mKey, effect.mValue);
    }
}

State Planner::Apply(const State& start, const Plan& plan, const std::vector<Action>& actions)
{
    State state = start;
    for (s32 step : plan.mSteps)
    {
        Do(state, actions[step]);
    }
    return state;
}

// How many states the search for the cheapest plan gets before any plan will do
static const s32 kCheapestStates = 400;
// And the search for a cheaper way to what one dear step gives
static const s32 kImproveStates = 120;

// What a state is still short of, worked out as if nothing done were ever undone: every fact at the price of the
// cheapest action that gives it, on top of what that action needs. Counting only the dearest thing an action needs
// gives a number that is never more than the real cost, which a search for the cheapest plan can go by. Counting
// all it needs (sum) is nearer the truth on a long way and can be over it: that one is for finding any plan fast.
// Either way the states the goal can't be had from at all are told apart (-1). Something an action uses up counts
// as there when there is any of it.
class Estimate final
{
public:
    // more: the goal is also to have more of something counted than that (key 0: no)
    Estimate(const std::vector<Fact>& goal, const std::vector<Action>& actions, const std::vector<bool>& relevant, bool sum, const Fact& more = {})
        : mSum(sum)
        , mMore(more)
    {
        for (const Fact& g : goal)
        {
            mGoal.push_back(Id(g));
        }
        if (more.mKey)
        {
            mGoal.push_back(Id({more.mKey, kMore}));
        }
        std::set<u32> spent;
        for (std::size_t a = 0; a < actions.size(); a++)
        {
            if (relevant[a] && actions[a].mSpend.mKey)
            {
                spent.insert(actions[a].mSpend.mKey);
            }
        }
        for (std::size_t a = 0; a < actions.size(); a++)
        {
            if (!relevant[a])
            {
                continue;
            }
            Act act;
            if (actions[a].mSpend.mKey)
            {
                act.mPre.push_back(Id({actions[a].mSpend.mKey, kSome}));
                // (and using it up is the way to have none)
                act.mEffects.push_back(Id({actions[a].mSpend.mKey, 0}));
            }
            act.mCost = std::max(1, actions[a].mCost);
            for (const Fact& p : actions[a].mPre)
            {
                const s32 id = Id(p);
                if (std::find(act.mPre.begin(), act.mPre.end(), id) == act.mPre.end())
                {
                    act.mPre.push_back(id);
                }
            }
            for (const Fact& e : actions[a].mEffects)
            {
                act.mEffects.push_back(Id(e));
                if (e.mValue > 0 && spent.count(e.mKey))
                {
                    act.mEffects.push_back(Id({e.mKey, kSome}));
                }
                if (more.mKey == e.mKey && e.mValue > more.mValue)
                {
                    act.mEffects.push_back(Id({e.mKey, kMore}));
                }
            }
            mActs.push_back(std::move(act));
        }
        mNeededBy.resize(mFacts.size());
        for (std::size_t a = 0; a < mActs.size(); a++)
        {
            for (s32 id : mActs[a].mPre)
            {
                mNeededBy[id].push_back(static_cast<s32>(a));
            }
        }
        mPrice.resize(mFacts.size());
        mLeft.resize(mActs.size());
        mNeeds.resize(mActs.size());
    }

    // -1: the goal can't be had from this state
    s32 Of(const State& state)
    {
        const s32 kNone = 0x7fffffff;
        // (sums of sums grow fast: they stop here)
        const s32 kMost = 1000000000;
        std::fill(mPrice.begin(), mPrice.end(), kNone);
        using Item = std::pair<s32, s32>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
        for (std::size_t id = 0; id < mFacts.size(); id++)
        {
            const Fact& f = mFacts[id];
            if (f.mValue == kSome ? state.Get(f.mKey) > 0 : (f.mValue == kMore ? state.Get(f.mKey) > mMore.mValue : state.Has(f)))
            {
                mPrice[id] = 0;
                open.push({0, static_cast<s32>(id)});
            }
        }
        const auto give = [&](const Act& act, s32 price)
        {
            for (s32 e : act.mEffects)
            {
                if (price < mPrice[e])
                {
                    mPrice[e] = price;
                    open.push({price, e});
                }
            }
        };
        for (std::size_t a = 0; a < mActs.size(); a++)
        {
            mLeft[a] = static_cast<s32>(mActs[a].mPre.size());
            mNeeds[a] = 0;
            if (mLeft[a] == 0)
            {
                give(mActs[a], mActs[a].mCost);
            }
        }
        std::size_t goalsLeft = 0;
        for (s32 g : mGoal)
        {
            goalsLeft += mPrice[g] != 0 ? 1 : 0;
        }
        s32 most = 0;
        s32 all = 0;
        while (goalsLeft > 0 && !open.empty())
        {
            const Item item = open.top();
            open.pop();
            if (mPrice[item.second] != item.first)
            {
                continue;
            }
            if (item.first > 0 && std::find(mGoal.begin(), mGoal.end(), item.second) != mGoal.end())
            {
                most = item.first;
                all = std::min(kMost, all + item.first);
                goalsLeft--;
            }
            // Facts come out cheapest first: the one that completes what an action needs is the dearest of them
            for (s32 a : mNeededBy[item.second])
            {
                mNeeds[a] = std::min(kMost, mNeeds[a] + item.first);
                if (--mLeft[a] == 0)
                {
                    give(mActs[a], std::min(kMost, (mSum ? mNeeds[a] : item.first) + mActs[a].mCost));
                }
            }
        }
        return goalsLeft > 0 ? -1 : (mSum ? all : most);
    }

private:
    struct Act final
    {
        std::vector<s32> mPre;
        std::vector<s32> mEffects;
        s32 mCost = 1;
    };
    // The value that stands for "any of it" in a fact about something counted
    static const s32 kSome = -0x7fffffff;
    // And for "more of it than mMore says"
    static const s32 kMore = -0x7ffffffe;

    s32 Id(const Fact& fact)
    {
        const auto it = mIds.find({fact.mKey, fact.mValue});
        if (it != mIds.end())
        {
            return it->second;
        }
        mFacts.push_back(fact);
        mIds[{fact.mKey, fact.mValue}] = static_cast<s32>(mFacts.size()) - 1;
        return static_cast<s32>(mFacts.size()) - 1;
    }

    std::map<std::pair<u32, s32>, s32> mIds;
    std::vector<Fact> mFacts;
    std::vector<s32> mGoal;
    std::vector<Act> mActs;
    std::vector<std::vector<s32>> mNeededBy;
    std::vector<s32> mPrice;
    std::vector<s32> mLeft;
    std::vector<s32> mNeeds;
    bool mSum = false;
    Fact mMore;
};

// greedy: the state that looks nearest the goal first, with little weight on what it cost to get to. Finds a plan of a hundred steps
// in a few hundred states where the cheapest one can't be found in any number, and it isn't far off the cheapest.
// counted: what an action uses up is taken away. Without, having any is enough for as many actions as the plan
// likes, which keeps the states apart only by what matters to the way: see Planner::Find for what is made of it.
// more: the goal is also to have more of something counted than that (key 0: no).
static void Search(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, const std::vector<bool>& relevant, s32 maxExpanded, bool greedy, bool counted, const Fact& more, Plan& plan)
{
    const auto isGoal = [&](const State& state)
    {
        return state.HasAll(goal) && (more.mKey == 0 || state.Get(more.mKey) > more.mValue);
    };
    struct Node final
    {
        State mState;
        s32 mCost = 0;
        s32 mParent = -1;
        s32 mAction = -1;
    };
    std::vector<Node> nodes;
    // Per state: the cheapest way to it so far, and what it is still short of (-1: the goal can't be had from it)
    std::map<State, std::pair<s32, s32>> best;
    // The one that looks cheapest in all first, then the one found first
    using Item = std::pair<s32, s32>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    Estimate estimate(goal, actions, relevant, greedy, more);
    plan.mFound = false;
    plan.mSteps.clear();

    plan.mMiss.clear();
    plan.mExpanded = 0;
    const s32 startShort = estimate.Of(start);
    if (startShort < 0)
    {
        return;
    }
    nodes.push_back({start, 0, -1, -1});
    best[start] = {0, startShort};
    open.push({startShort, 0});

    while (!open.empty())
    {
        const Item item = open.top();
        open.pop();
        const s32 idx = item.second;
        if (best[nodes[idx].mState].first != nodes[idx].mCost)
        {
            continue;
        }
        if (isGoal(nodes[idx].mState))
        {
            plan.mFound = true;
            plan.mCost = nodes[idx].mCost;
            for (s32 at = idx; nodes[at].mParent >= 0; at = nodes[at].mParent)
            {
                plan.mSteps.push_back(nodes[at].mAction);
            }
            std::reverse(plan.mSteps.begin(), plan.mSteps.end());
            return;
        }
        if (++plan.mExpanded > maxExpanded)
        {
            plan.mMiss = "search gave up after " + std::to_string(maxExpanded) + " states";
            return;
        }

        for (std::size_t a = 0; a < actions.size(); a++)
        {
            if (!relevant[a] || !Planner::Can(nodes[idx].mState, actions[a]))
            {
                continue;
            }
            State next = nodes[idx].mState;
            if (counted)
            {
                Planner::Do(next, actions[a]);
            }
            else
            {
                for (const Fact& e : actions[a].mEffects)
                {
                    next.Set(e.mKey, e.mValue);
                }
            }
            if (next == nodes[idx].mState)
            {
                continue;
            }
            const s32 cost = nodes[idx].mCost + std::max(1, actions[a].mCost);
            auto found = best.find(next);
            if (found == best.end())
            {
                found = best.insert({next, {-1, estimate.Of(next)}}).first;
            }
            else if (found->second.first >= 0 && found->second.first <= cost)
            {
                continue;
            }
            if (found->second.second < 0)
            {
                continue;
            }
            found->second.first = cost;
            nodes.push_back({std::move(next), cost, idx, static_cast<s32>(a)});
            // (greedy still counts what it cost, a third as much as what is left: of two ways on, the dear one
            // isn't taken just for being a step further)
            open.push({greedy ? cost + 3 * std::min(found->second.second, 500000000) : cost + found->second.second, static_cast<s32>(nodes.size()) - 1});
        }
    }
}

static bool Works(State state, const std::vector<s32>& steps, const std::vector<Action>& actions, const std::vector<Fact>& goal)
{
    for (s32 step : steps)
    {
        if (!Planner::Can(state, actions[step]))
        {
            return false;
        }
        Planner::Do(state, actions[step]);
    }
    return state.HasAll(goal);
}

// A plan found by going for what looks nearest takes a dear action where a cheaper one does the same after a
// little more work (a throw past a running drill, when its lever is at hand). Each step that has a cheaper twin
// is looked at again: the cheapest way to what it gives that leaves Abe where the step needs him, put in its
// place if that costs less and the rest of the plan still works after it.
static void Improve(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, Plan& plan)
{
    for (std::size_t i = 0; i < plan.mSteps.size(); i++)
    {
        const Action& dear = actions[plan.mSteps[i]];
        bool twin = false;
        for (const Action& other : actions)
        {
            twin = twin || (other.mEffects == dear.mEffects && std::max(1, other.mCost) < std::max(1, dear.mCost));
        }
        if (!twin)
        {
            continue;
        }
        State before = start;
        for (std::size_t k = 0; k < i; k++)
        {
            Planner::Do(before, actions[plan.mSteps[k]]);
        }
        std::vector<Fact> want = dear.mEffects;
        for (const Fact& p : dear.mPre)
        {
            bool changed = false;
            for (const Fact& e : dear.mEffects)
            {
                changed = changed || e.mKey == p.mKey;
            }
            if (!changed)
            {
                want.push_back(p);
            }
        }
        Plan other;
        Search(before, want, actions, Planner::Relevant(want, actions), kImproveStates, false, true, {}, other);
        plan.mExpanded += other.mExpanded;
        if (!other.mFound || other.mCost >= std::max(1, dear.mCost))
        {
            continue;
        }
        std::vector<s32> steps(plan.mSteps.begin(), plan.mSteps.begin() + static_cast<std::ptrdiff_t>(i));
        steps.insert(steps.end(), other.mSteps.begin(), other.mSteps.end());
        steps.insert(steps.end(), plan.mSteps.begin() + static_cast<std::ptrdiff_t>(i) + 1, plan.mSteps.end());
        if (Works(start, steps, actions, goal))
        {
            plan.mCost += other.mCost - std::max(1, dear.mCost);
            plan.mSteps = steps;
            i += other.mSteps.size() - 1;
        }
    }
}

// Any plan, fast. Something that gets used up (rocks) is first planned with as if any of it were enough, and the
// plan then gone through with the count: where it runs out, the way to more of it is put in from the last place
// there is one, and the rest planned again from there.
static void FindAny(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, const std::vector<bool>& relevant, s32 maxExpanded, Plan& plan)
{
    Plan whole;
    State state = start;
    s32 refillStates = 0;
    for (s32 round = 0; round < 24; round++)
    {
        Plan part;
        Search(state, goal, actions, relevant, maxExpanded, true, false, {}, part);
        whole.mExpanded += part.mExpanded;
        if (!part.mFound)
        {
            whole.mMiss = part.mMiss.empty() ? "the goal can't be reached from here with the actions known" : part.mMiss;
            break;
        }
        // Where the count says no
        std::vector<State> before;
        State at = state;
        std::size_t stop = part.mSteps.size();
        for (std::size_t i = 0; i < part.mSteps.size(); i++)
        {
            before.push_back(at);
            if (!Planner::Can(at, actions[part.mSteps[i]]))
            {
                stop = i;
                break;
            }
            Planner::Do(at, actions[part.mSteps[i]]);
        }
        const auto take = [&](const Plan& from, std::size_t count)
        {
            for (std::size_t i = 0; i < count; i++)
            {
                whole.mSteps.push_back(from.mSteps[i]);
                whole.mCost += std::max(1, actions[from.mSteps[i]].mCost);
            }
        };
        if (stop == part.mSteps.size())
        {
            take(part, stop);
            whole.mFound = true;
            break;
        }
        const u32 key = actions[part.mSteps[stop]].mSpend.mKey;
        bool mended = false;
        const std::vector<bool> gives = Planner::Relevant({{key, 1}}, actions);
        for (std::size_t j = stop + 1; key && j-- > 0 && !mended && refillStates < maxExpanded;)
        {
            // (from there, or from further back: with some left, or before a way of no return, there may be a
            // way that there isn't later)
            const Fact more{key, before[j].Get(key)};
            Plan refill;
            Search(before[j], {}, actions, gives, maxExpanded / 4, true, true, more, refill);
            whole.mExpanded += refill.mExpanded;
            refillStates += refill.mExpanded;
            if (refill.mFound)
            {
                take(part, j);
                take(refill, refill.mSteps.size());
                state = before[j];
                for (s32 step : refill.mSteps)
                {
                    Planner::Do(state, actions[step]);
                }
                mended = true;
            }
        }
        if (!mended)
        {
            whole.mMiss = "what the plan uses up runs out, and there is no way to more of it";
            break;
        }
    }
    if (!whole.mFound && whole.mMiss.empty())
    {
        whole.mMiss = "no end to what the plan uses up";
    }
    if (!whole.mFound)
    {
        whole.mSteps.clear();
    }
    else
    {
        Improve(start, goal, actions, whole);
    }
    plan = whole;
}

s32 Planner::Short(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions)
{
    return Estimate(goal, actions, Relevant(goal, actions), true).Of(start);
}

Plan Planner::Find(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, s32 maxExpanded)
{
    Plan plan;
    if (start.HasAll(goal))
    {
        plan.mFound = true;
        return plan;
    }

    const std::vector<bool> relevant = Relevant(goal, actions);
    // The cheapest plan if it is soon found, else any: with many things that can each be done or not (a floor of
    // mines) the states to search for the cheapest are without number
    Search(start, goal, actions, relevant, std::min(maxExpanded, kCheapestStates), false, true, {}, plan);
    if (!plan.mFound && !plan.mMiss.empty())
    {
        FindAny(start, goal, actions, relevant, maxExpanded, plan);
    }
    if (plan.mFound || !plan.mMiss.empty())
    {
        return plan;
    }

    // Nothing left to try: say which goal facts nothing gives
    for (const Fact& g : goal)
    {
        if (start.Has(g))
        {
            continue;
        }
        bool provided = false;
        for (const Action& action : actions)
        {
            for (const Fact& e : action.mEffects)
            {
                provided = provided || e == g;
            }
        }
        if (!provided)
        {
            plan.mMiss += (plan.mMiss.empty() ? "" : ", ") + std::string("nothing gives fact ") + std::to_string(g.mKey) + "=" + std::to_string(g.mValue);
        }
    }
    if (plan.mMiss.empty())
    {
        plan.mMiss = "the goal can't be reached from here with the actions known";
    }
    return plan;
}

} // namespace Bot::Goap
