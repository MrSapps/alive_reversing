#pragma once

#include "../Types.hpp"
#include <functional>
#include <vector>

namespace Bot {

// What the Sligs that shoot from the background were seen to do, a frame at a time, and from that what they will
// do: one on a beat with fixed waits comes round to the same again, so what it did a period ago it does now.
// Nothing of the game's Slig is modelled here: the bot watches them as a player does, and goes by what it saw.
//
// Such a Slig (AE's Slig.cpp, its idle, walking and paused brains) shoots at Abe when he is on its screen in front
// of it (his x at or past its own, the way it faces), nearer the player than it is, and not wholly inside a Z
// cover. It does not look while it turns. Each shot it lands takes a third of his health, which comes back 180
// frames after the last.
class SligCycle final
{
public:
    struct Seen final
    {
        s32 mX = 0;
        bool mFacingLeft = false;
        // Turning: it sees nothing
        bool mBlind = false;
        // What it is doing, as the game numbers it: standing to shoot is not standing at the end of its beat
        s32 mMotion = 0;
        bool operator==(const Seen& o) const
        {
            return mX == o.mX && mFacingLeft == o.mFacingLeft && mBlind == o.mBlind && mMotion == o.mMotion;
        }
    };

    // The least and the most a period can be
    static const s32 kMinPeriod = 40;
    static const s32 kMaxPeriod = 900;

    // Each frame: the Sligs of the screen `key`, always in the same order. Another screen, another number of
    // them or a frame missed (a rewind) and it starts over.
    void Observe(u32 frame, s32 key, const std::vector<Seen>& sligs);

    s32 Count() const
    {
        return static_cast<s32>(mPeriods.size());
    }
    // The frames it has watched them for without a break
    s32 Watched() const
    {
        return static_cast<s32>(mSeen.size());
    }
    // The period of one of them, 0 while it isn't known
    s32 Period(s32 slig) const
    {
        return mPeriods[slig];
    }
    // Every one's period is known
    bool Known() const;
    // One of them as it will be that many frames from the last one seen (as it is now, while its period isn't
    // known)
    Seen Ahead(s32 slig, s32 frames) const;

    // A cell of the way through the open: the first is where he is (crouched), the last the cover he is after
    struct Cell final
    {
        s32 mX = 0;
        bool mCover = false;
    };
    struct Move final
    {
        // The frames to wait where he is, then the cell to roll to (an index into the cells)
        s32 mDelay = 0;
        s32 mTo = 0;
        // How many of them see him on the whole way to the last cell, at best
        s32 mSpotters = 0;
    };
    // The frames a roll takes to begin, to go a cell (25 at 6.25 a frame), and to end crouched before the next
    // can be asked for
    static const s32 kRollBegin = 1;
    static const s32 kRollCell = 4;
    static const s32 kRollEnd = 3;
    // The way through the open with the fewest of them seeing him, and of those the soonest done: rolls from
    // cell to cell with waits between, crouched, wherever nothing sees him wait (a Slig that walks his way can be
    // followed behind its back, no nearer than it has got). `covered` says whether he is in cover at x, rolling
    // or crouched. Only its first move is given: it is asked again from where that ends.
    Move Cross(const std::vector<Cell>& cells, const std::function<bool(s32 x, bool rolling)>& covered) const;
    // Which of them (a bit each) see him at x that many frames from now
    u32 SeenBy(s32 x, s32 frames) const;

private:
    void FindPeriod(s32 slig);

    u32 mFrame = 0;
    s32 mKey = -1;
    // [frame][slig]
    std::vector<std::vector<Seen>> mSeen;
    std::vector<s32> mPeriods;
};

} // namespace Bot
