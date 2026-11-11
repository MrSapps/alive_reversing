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
            for (const Fact& e : actions[i].mEffects)
            {
                if (keys.count(e.mKey))
                {
                    relevant[i] = true;
                    grew = true;
                    for (const Fact& p : actions[i].mPre)
                    {
                        keys.insert(p.mKey);
                    }
                    break;
                }
            }
        }
    }
    return relevant;
}

State Planner::Apply(const State& start, const Plan& plan, const std::vector<Action>& actions)
{
    State state = start;
    for (s32 step : plan.mSteps)
    {
        for (const Fact& effect : actions[step].mEffects)
        {
            state.Set(effect.mKey, effect.mValue);
        }
    }
    return state;
}

// The actions worth searching with. Costs are worked out as if nothing an action did were ever undone (each fact
// at the price of the cheapest action that gives it, on top of the dearest thing that action needs): the actions
// that way to the goal uses, and with them every action that only moves between the facts those touch (the ways
// back). reachable is false when even so the goal can't be had.
static std::vector<bool> Support(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, const std::vector<bool>& relevant, bool& reachable)
{
    using Key = std::pair<u32, s32>;
    std::map<Key, s32> price;
    std::map<Key, s32> giver;
    const auto priceOf = [&](const Fact& f) -> s32
    {
        if (start.Has(f))
        {
            return 0;
        }
        const auto it = price.find({f.mKey, f.mValue});
        return it == price.end() ? -1 : it->second;
    };
    for (bool more = true; more;)
    {
        more = false;
        for (std::size_t a = 0; a < actions.size(); a++)
        {
            if (!relevant[a])
            {
                continue;
            }
            s32 need = 0;
            for (const Fact& p : actions[a].mPre)
            {
                const s32 c = priceOf(p);
                need = c < 0 || need < 0 ? -1 : std::max(need, c);
            }
            if (need < 0)
            {
                continue;
            }
            const s32 total = need + std::max(1, actions[a].mCost);
            for (const Fact& e : actions[a].mEffects)
            {
                const s32 had = priceOf(e);
                if (had < 0 || total < had)
                {
                    price[{e.mKey, e.mValue}] = total;
                    giver[{e.mKey, e.mValue}] = static_cast<s32>(a);
                    more = true;
                }
            }
        }
    }

    std::vector<bool> use(actions.size(), false);
    reachable = true;
    std::vector<Fact> todo = goal;
    std::set<u32> touched;
    for (const Fact& f : start.Facts())
    {
        touched.insert(f.mKey);
    }
    while (!todo.empty())
    {
        const Fact f = todo.back();
        todo.pop_back();
        touched.insert(f.mKey);
        if (start.Has(f))
        {
            continue;
        }
        const auto it = giver.find({f.mKey, f.mValue});
        if (it == giver.end())
        {
            reachable = false;
            return use;
        }
        if (!use[it->second])
        {
            use[it->second] = true;
            for (const Fact& p : actions[it->second].mPre)
            {
                todo.push_back(p);
            }
            for (const Fact& e : actions[it->second].mEffects)
            {
                touched.insert(e.mKey);
            }
        }
    }
    // What those facts are about is all a plan can need to change: an action is of use if every fact it gives is
    // one of those values or about one of those keys
    std::set<Key> values;
    for (std::size_t a = 0; a < actions.size(); a++)
    {
        for (const Fact& f : use[a] ? actions[a].mPre : std::vector<Fact>{})
        {
            values.insert({f.mKey, f.mValue});
        }
        for (const Fact& f : use[a] ? actions[a].mEffects : std::vector<Fact>{})
        {
            values.insert({f.mKey, f.mValue});
        }
    }
    for (const Fact& f : start.Facts())
    {
        values.insert({f.mKey, f.mValue});
    }
    for (const Fact& f : goal)
    {
        values.insert({f.mKey, f.mValue});
    }
    for (std::size_t a = 0; a < actions.size(); a++)
    {
        bool all = relevant[a] && !use[a];
        for (const Fact& e : actions[a].mEffects)
        {
            all = all && (values.count({e.mKey, e.mValue}) || (e.mValue == 0 && touched.count(e.mKey)));
        }
        for (const Fact& p : actions[a].mPre)
        {
            all = all && (values.count({p.mKey, p.mValue}) || (p.mValue == 0 && touched.count(p.mKey)) || start.Has(p));
        }
        if (all)
        {
            use[a] = true;
        }
    }
    return use;
}

static void Search(const State& start, const std::vector<Fact>& goal, const std::vector<Action>& actions, const std::vector<bool>& relevant, s32 maxExpanded, Plan& plan)
{
    struct Node final
    {
        State mState;
        s32 mCost = 0;
        s32 mParent = -1;
        s32 mAction = -1;
    };
    std::vector<Node> nodes;
    std::map<State, s32> best;
    // Cheapest first, then the one found first
    using Item = std::pair<s32, s32>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;

    nodes.push_back({start, 0, -1, -1});
    best[start] = 0;
    open.push({0, 0});
    plan.mExpanded = 0;
    plan.mMiss.clear();

    while (!open.empty())
    {
        const Item item = open.top();
        open.pop();
        const s32 idx = item.second;
        if (nodes[idx].mCost != item.first || best[nodes[idx].mState] != item.first)
        {
            continue;
        }
        if (nodes[idx].mState.HasAll(goal))
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
            if (!relevant[a] || !nodes[idx].mState.HasAll(actions[a].mPre))
            {
                continue;
            }
            State next = nodes[idx].mState;
            for (const Fact& e : actions[a].mEffects)
            {
                next.Set(e.mKey, e.mValue);
            }
            if (next == nodes[idx].mState)
            {
                continue;
            }
            const s32 cost = nodes[idx].mCost + std::max(1, actions[a].mCost);
            const auto found = best.find(next);
            if (found != best.end() && found->second <= cost)
            {
                continue;
            }
            best[next] = cost;
            nodes.push_back({std::move(next), cost, idx, static_cast<s32>(a)});
            open.push({cost, static_cast<s32>(nodes.size()) - 1});
        }
    }
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
    // With many things that can each be done or not (a floor of mines) the states to search are without number:
    // first with only the actions a plan would use, and all of them only if that finds nothing
    bool reachable = false;
    const std::vector<bool> support = Support(start, goal, actions, relevant, reachable);
    if (reachable)
    {
        Search(start, goal, actions, support, maxExpanded, plan);
        if (!plan.mFound && support != relevant)
        {
            Search(start, goal, actions, relevant, maxExpanded / 4, plan);
        }
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
