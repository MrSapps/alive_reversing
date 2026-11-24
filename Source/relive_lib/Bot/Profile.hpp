#pragma once

#include "../Types.hpp"
#include <string>
#include <vector>

namespace Bot {

// Which of a game's endings a run plays for
enum class Ending : u8
{
    eAny,     // get to the end, whatever happens to the Mudokons on the way (saves only what the way needs)
    eSaveAll, // the good ending: every Mudokon rescued
    eKillAll, // the worst one: every Mudokon dead
    eAverage, // the one in between: about half rescued, the rest left where they are
};

// How a run is to be played: "save_all", "kill_all", "average" or "any", then options after commas:
// seed=<n> for the bot's random choices, human to make it dawdle and vary like a person (by the seed),
// look to try what it is about to do against the game and take back what gets him hurt,
// been=<level> (any number of times) for a level played before a run that starts further on: a way back there is
// not a way on.
struct Profile final
{
    Ending mEnding = Ending::eAny;
    u64 mSeed = 1;
    bool mHuman = false;
    // Tries are made from each save, and what fails taken back, until one gets on unhurt (see Lookahead)
    bool mLook = false;
    // Looking ahead, only dying fails a try (hurt_ok): being hurt is no failure, so a try that is bitten once on
    // its way through is the one kept (the Scrab vault's Fleeches bite as they are led)
    bool mHurtOk = false;
    // The levels, or paths of them, he looks ahead in though mLook is off (look=<level> or look=<level>/<path>): the Slig Barracks were built with it, and
    // the levels before them without
    std::vector<std::string> mLookIn;
    std::vector<std::string> mBeen;

    static bool Parse(const std::string& text, Profile& profile, std::string& error);
    std::string Text() const;
    // Whether a Mudokon is one this ending rescues: index is its place among the path's Mudokons
    bool Rescues(u32 index) const;
    // Whether it is one this ending kills, if it can be done without getting stuck
    bool Kills(u32 index) const;
};

} // namespace Bot
