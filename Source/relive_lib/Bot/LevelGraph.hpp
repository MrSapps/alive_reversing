#pragma once

#include "../Types.hpp"
#include <functional>
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
    // The switches its things can set (levers, wheels, a tasks door's own switch)
    std::vector<s32> mSets;
    // The switches its shut doors need on to open, with where each door leads (level and path, as mExits)
    struct Want final
    {
        s32 mLevel = 0;
        s32 mPath = 0;
        s32 mSwitch = 0;
    };
    std::vector<Want> mWants;
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
    // The ways towards the level's end this path has can't be taken as things are (Feeco's lobby: its door to the
    // executive offices, and the one to the path that leads to the Brewery, are behind gates the level's other
    // ends open): they are no ways on any more, and the distances are measured again without them. False, and
    // nothing changed, if it had none or there is no other way from it.
    bool Shut(s32 path);
    s32 Mudokons(s32 path) const;
    // The switches a door of another path wants, where that door is the way on from its path towards the level's
    // end (a hub's door that opens once the paths around it are done): what Abe is in a side path for. Only doors
    // of paths no farther from the end than this one: the others he is past.
    std::vector<s32> Wanted(s32 path) const;
    // The paths `from` leads straight into through which such a switch can be got at, without going through the
    // path whose door wants it. open: whether a switch is still to be set in a path (off, and not given up there)
    std::set<s32> ErrandExits(s32 from, const std::function<bool(s32 path, s32 sw)>& open) const;
    // The switches of `path` that Wanted is about for some other path
    std::vector<s32> Errands(s32 path) const;
    std::string Text() const;

private:
    void Measure();
    // The paths with a door on the way on that wants the switch
    std::set<s32> Wanters(s32 sw) const;
    std::set<s32> mShut;
    s32 mLevel = -1;
    std::vector<PathSummary> mPaths;
    std::map<s32, s32> mDistance;
};

} // namespace Bot
