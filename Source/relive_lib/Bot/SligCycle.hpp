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
// frames after the last. Once it has seen him it shoots, and being in cover by then does not help: only the shot
// that would kill him is stopped by a cover (and that one gives him all his health back).
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
    // A way he can't stop on (a hop, a drop, a hoist) from one cover to the next: where he is on each frame of it.
    // One that sees him on it shoots, and what it shoots finds him wherever he has got to by then (Abe.cpp's
    // BulletDamage: a cover only stops the bullet that would kill him), and it shoots again if he is still in the
    // open when that burst is over: mAfter frames before it fires, kShotFrame to the first bullet, kShotGap between
    // those of a burst and before it looks again. The frames to wait before the way, no more than `within`, so
    // that the fewest bursts are shot at him, and of those the soonest. mSpotters is that number of bursts (the
    // last bullet of one finds him, and the one before it can: they are aimed 16 short of him for each still to
    // come); mTo is not used.
    struct Shooter final
    {
        s32 mAfter = 0;
        s32 mTimes = 1;
    };
    static const s32 kShotFrame = 14;
    static const s32 kShotGap = 12;
    // (a frame of the track is taken as seen if he would be within that many frames either side of it)
    static const s32 kTrackSlack = 3;
    // (or that far either side of where it has him: a hop carries him a few past where it lands)
    static const s32 kTrackAside = 8;
    Move Through(const std::vector<Cell>& track, const std::vector<Shooter>& shooters, s32 within) const;
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
