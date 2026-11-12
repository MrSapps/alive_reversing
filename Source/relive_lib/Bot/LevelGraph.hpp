#pragma once

#include "../Types.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Bot {

// What a path of a level has that matters for choosing where to go next
struct PathSummary final
{
    s32 mPath = 0;
    s32 mMudokons = 0;
    // Where its doors, wells and other ways out lead: level and path (a path of another level ends this one)
    std::vector<std::pair<s32, s32>> mExits;
};

// The paths of a level and how they lead to each other
class LevelGraph final
{
public:
    // been: the levels Abe has been in already. A way back to one of them is no way out of this one.
    void Build(s32 level, const std::vector<PathSummary>& paths, const std::set<s32>& been = {});
    s32 Level() const
    {
        return mLevel;
    }
    // How many paths away the way out of the level is: 0 for a path with an exit to another level, -1 if there's
    // no way from it
    s32 Distance(s32 path) const;
    s32 Mudokons(s32 path) const;
    std::string Text() const;

private:
    s32 mLevel = -1;
    std::vector<PathSummary> mPaths;
    std::map<s32, s32> mDistance;
};

} // namespace Bot
