#include "LevelGraph.hpp"
#include <algorithm>

namespace Bot {

void LevelGraph::Build(s32 level, const std::vector<PathSummary>& paths, const std::set<s32>& been)
{
    mLevel = level;
    mPaths = paths;
    for (PathSummary& p : mPaths)
    {
        p.mExits.erase(std::remove_if(p.mExits.begin(), p.mExits.end(), [&](const std::pair<s32, s32>& exit)
                                      { return exit.first != level && been.count(exit.first) != 0; }),
                       p.mExits.end());
        p.mWants.erase(std::remove_if(p.mWants.begin(), p.mWants.end(), [&](const PathSummary::Want& want)
                                      { return want.mLevel != level && been.count(want.mLevel) != 0; }),
                       p.mWants.end());
    }
    mShut.clear();
    Measure();
}

bool LevelGraph::Shut(s32 path)
{
    const s32 here = Distance(path);
    if (here < 0)
    {
        return false;
    }
    // Its ways towards the end: out of the level, or into a path that is nearer to the end than it is
    const std::vector<PathSummary> was = mPaths;
    const std::set<s32> wasShut = mShut;
    bool any = false;
    for (PathSummary& p : mPaths)
    {
        if (p.mPath != path)
        {
            continue;
        }
        if (here == 0 && mShut.insert(path).second)
        {
            any = true;
        }
        const std::size_t before = p.mExits.size();
        p.mExits.erase(std::remove_if(p.mExits.begin(), p.mExits.end(), [&](const std::pair<s32, s32>& exit)
                                      { return exit.first == mLevel && here > 0 && Distance(exit.second) >= 0 && Distance(exit.second) < here; }),
                       p.mExits.end());
        any = any || p.mExits.size() != before;
    }
    if (any)
    {
        Measure();
    }
    if (any && Distance(path) > 0)
    {
        return true;
    }
    // (no other way from it: it stays as it was)
    mPaths = was;
    mShut = wasShut;
    Measure();
    return false;
}

void LevelGraph::Measure()
{
    const s32 level = mLevel;
    mDistance.clear();
    for (const PathSummary& p : mPaths)
    {
        for (const auto& exit : p.mExits)
        {
            if (exit.first != level && mShut.count(p.mPath) == 0)
            {
                mDistance[p.mPath] = 0;
            }
        }
    }
    // Outwards from the paths that leave the level, a step at a time
    bool grew = true;
    while (grew)
    {
        grew = false;
        for (const PathSummary& p : mPaths)
        {
            for (const auto& exit : p.mExits)
            {
                const auto to = mDistance.find(exit.second);
                if (exit.first != level || to == mDistance.end())
                {
                    continue;
                }
                const auto from = mDistance.find(p.mPath);
                if (from == mDistance.end() || from->second > to->second + 1)
                {
                    mDistance[p.mPath] = to->second + 1;
                    grew = true;
                }
            }
        }
    }
}

bool LevelGraph::LeadsToNew(s32 from, s32 avoid, const std::function<bool(s32 path)>& seen) const
{
    std::set<s32> done{from, avoid};
    std::vector<s32> open{from};
    while (!open.empty())
    {
        const s32 at = open.back();
        open.pop_back();
        for (const PathSummary& p : mPaths)
        {
            if (p.mPath != at)
            {
                continue;
            }
            for (const auto& exit : p.mExits)
            {
                if (exit.first != mLevel || done.count(exit.second) != 0)
                {
                    continue;
                }
                if (!seen(exit.second))
                {
                    return true;
                }
                done.insert(exit.second);
                open.push_back(exit.second);
            }
        }
    }
    return false;
}

s32 LevelGraph::Distance(s32 path) const
{
    const auto it = mDistance.find(path);
    return it == mDistance.end() ? -1 : it->second;
}

s32 LevelGraph::Mudokons(s32 path) const
{
    for (const PathSummary& p : mPaths)
    {
        if (p.mPath == path)
        {
            return p.mMudokons;
        }
    }
    return 0;
}

std::set<s32> LevelGraph::Wanters(s32 sw) const
{
    std::set<s32> out;
    for (const PathSummary& p : mPaths)
    {
        const s32 here = Distance(p.mPath);
        for (const PathSummary::Want& want : p.mWants)
        {
            // (the way on: out of the level, or into a path nearer its end)
            const bool on = want.mLevel != mLevel || (here >= 0 && Distance(want.mPath) >= 0 && Distance(want.mPath) < here);
            if (want.mSwitch == sw && on)
            {
                out.insert(p.mPath);
            }
        }
    }
    return out;
}

std::vector<s32> LevelGraph::Wanted(s32 path) const
{
    std::set<s32> all;
    const s32 here = Distance(path);
    for (const PathSummary& p : mPaths)
    {
        if (p.mPath == path)
        {
            continue;
        }
        // (a door he is past is none to set things for: the Mines' path 6, the last, has a lever for the switch
        // that the door from path 3 to 4 wants too)
        if (here < 0 || Distance(p.mPath) < 0 || Distance(p.mPath) > here)
        {
            continue;
        }
        for (const PathSummary::Want& want : p.mWants)
        {
            if (Wanters(want.mSwitch).count(p.mPath) != 0)
            {
                all.insert(want.mSwitch);
            }
        }
    }
    return std::vector<s32>(all.begin(), all.end());
}

std::vector<s32> LevelGraph::Errands(s32 path) const
{
    std::vector<s32> out;
    const std::vector<s32> wanted = Wanted(path);
    for (const PathSummary& p : mPaths)
    {
        for (const s32 sw : p.mSets)
        {
            if (p.mPath == path && std::find(wanted.begin(), wanted.end(), sw) != wanted.end() && std::find(out.begin(), out.end(), sw) == out.end())
            {
                out.push_back(sw);
            }
        }
    }
    return out;
}

std::set<s32> LevelGraph::ErrandExits(s32 from, const std::function<bool(s32 path, s32 sw)>& open) const
{
    std::set<s32> out;
    const auto find = [&](s32 path) -> const PathSummary*
    {
        for (const PathSummary& p : mPaths)
        {
            if (p.mPath == path)
            {
                return &p;
            }
        }
        return nullptr;
    };
    const PathSummary* start = find(from);
    if (!start)
    {
        return out;
    }
    for (const auto& exit : start->mExits)
    {
        if (exit.first != mLevel || exit.second == from || out.count(exit.second) != 0)
        {
            continue;
        }
        // Outwards from that path, for each switch it or a path behind it sets
        std::set<s32> seen{from, exit.second};
        std::vector<s32> todo{exit.second};
        bool found = false;
        while (!todo.empty() && !found)
        {
            const s32 at = todo.back();
            todo.pop_back();
            const PathSummary* p = find(at);
            if (!p)
            {
                continue;
            }
            bool wants = false;
            for (const s32 sw : Errands(at))
            {
                found = found || open(at, sw);
            }
            // (not on through a path whose own door wants something: what is behind it is for when he is there)
            for (const PathSummary::Want& want : p->mWants)
            {
                wants = wants || Wanters(want.mSwitch).count(at) != 0;
            }
            for (const auto& next : p->mExits)
            {
                if (!wants && next.first == mLevel && seen.insert(next.second).second)
                {
                    todo.push_back(next.second);
                }
            }
        }
        if (found)
        {
            out.insert(exit.second);
        }
    }
    return out;
}

std::string LevelGraph::Text() const
{
    std::string out;
    for (const PathSummary& p : mPaths)
    {
        out += "path " + std::to_string(p.mPath) + " mudokons " + std::to_string(p.mMudokons) + " to the end " + std::to_string(Distance(p.mPath)) + " exits";
        for (const auto& exit : p.mExits)
        {
            out += " " + std::to_string(exit.first) + "/" + std::to_string(exit.second);
        }
        if (!p.mSets.empty())
        {
            out += " sets";
            for (const s32 sw : p.mSets)
            {
                out += " " + std::to_string(sw);
            }
        }
        if (!p.mWants.empty())
        {
            out += " wants";
            for (const PathSummary::Want& want : p.mWants)
            {
                out += " " + std::to_string(want.mSwitch) + ">" + std::to_string(want.mPath);
            }
        }
        out += "\n";
    }
    return out;
}

} // namespace Bot
