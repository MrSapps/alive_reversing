#pragma once

#include "NavGraph.hpp"
#include "World.hpp"
#include <utility>
#include <vector>

namespace Bot {

// Mudokons that follow Abe on a floor under his. Wired by laughing gas they go after his x wherever he is, a floor
// down or not, and press the foot switches of their floor as they go over them: in Zulag 13 the gates of his floor
// are worked from theirs ("Lead the wired Mudokons right past the various barriers ... using the buttons up top to
// let the Mudokons through on the bottom when they get stuck").
// Where they stop can't be told well enough to say how a plate is left (measured there: they spread over the cells
// from two off his x on the one side to two on the other, some of them in the same cell, and those press a plate
// once between them). So a plate of theirs is worked by watching: from over it he steps a cell at a time, each one
// taking about one of them over it or back, until its switch is as it has to be.
class FollowBelow final
{
public:
    struct Plate final
    {
        u32 mId = 0;
        s32 mX = 0;
        s32 mSwitch = 0;
        s32 mOp = 0;
        // The cells of his floor he works it from: over it, two cells either way
        std::vector<s32> mFrom;
        // The barriers of their floor between them and it, open: switch and value
        std::vector<std::pair<s32, s32>> mOpen;
    };
    struct Result final
    {
        bool mFound = false;
        std::vector<u32> mMuds;
        std::vector<Plate> mPlates;
    };
    static Result Find(const NavGraph& nav, const World& world, s32 abeNode);
    // How far either way of his x they stop (the game's two cells)
    static constexpr s32 kSpread = 50;
};

} // namespace Bot
