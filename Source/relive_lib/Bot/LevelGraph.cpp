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
    }
    mDistance.clear();
    for (const PathSummary& p : mPaths)
    {
        for (const auto& exit : p.mExits)
        {
            if (exit.first != level)
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
        out += "\n";
    }
    return out;
}

} // namespace Bot
