#pragma once

#include "../Types.hpp"
#include <vector>

namespace Bot {

struct NavInput;

// Where the flying mines (AE's MovingBomb) were seen to go, a frame at a time, and from that where they will be:
// one on a closed track with nothing to stop it comes round to where it was, at the pace it had. Nothing of the
// game's mine is modelled here: the bot watches each for a lap as a player does, and goes by what it saw.
//
// Such a mine (MovingBomb.cpp) follows its track at a fixed pace once it is up to speed and blows up when its box
// meets the box of anything alive at its scale. A lap is seldom a whole number of frames, so what is kept is the
// lap as the places it was at, and where on it the mine is now: that is found again each frame it is seen.
// Its box changes with its animation (18 to 25 wide): that comes round too, and is told ahead the same way; until
// it is known the box is taken as wide as it was ever seen.
//
// Where a mine can ever be is known sooner, from the map: the track its TLV sits on. A mine is only there while
// its screen is, so the places to wait at have to be known before it has been seen.
class BombCycle final
{
public:
    struct Seen final
    {
        // Which it is (the same number each frame)
        u32 mId = 0;
        // Where it is, x256
        s32 mX = 0;
        s32 mY = 0;
        // Its box about that place, in pixels
        s32 mLeft = 0;
        s32 mTop = 0;
        s32 mRight = 0;
        s32 mBottom = 0;
        bool mHalf = false;
    };

    struct Box final
    {
        s32 mX1 = 0;
        s32 mY1 = 0;
        s32 mX2 = 0;
        s32 mY2 = 0;
        bool Meets(const Box& o) const
        {
            return mX1 <= o.mX2 && mX2 >= o.mX1 && mY1 <= o.mY2 && mY2 >= o.mY1;
        }
    };

    // A track of the map a mine goes along
    struct Track final
    {
        // Its corners, in pixels (a closed one ends where it starts)
        std::vector<s32> mXs;
        std::vector<s32> mYs;
        bool mHalf = false;
    };
    // The tracks of a path's flying mines, one for each mine
    static std::vector<Track> Tracks(const NavInput& in);
    // The tracks of the path `key`
    void SetTracks(s32 key, const std::vector<Track>& tracks);
    bool HasTracks(s32 key) const
    {
        return mTrackKey == key;
    }

    // The frames one has to keep still for to count as one that stays where it is
    static const s32 kStill = 40;
    // The most frames a lap can take
    static const s32 kMaxLap = 1500;

    // Each frame: the mines there are in the path `key`. In another path it starts over. A lap that was watched
    // is kept while its mine is out of sight and over a frame missed (a rewind): the track is the same.
    void Observe(u32 frame, s32 key, const std::vector<Seen>& bombs);

    // Whether there is anything to keep clear of in the path
    bool Any() const
    {
        return mSeen > 0 || (mTrackKey == mKey && !mTracks.empty());
    }
    // The mines there are now
    s32 Count() const
    {
        return mSeen;
    }
    // Where it will be is known: its lap has been watched and it is at its pace, or it keeps still
    bool Known(u32 id) const;
    // A mine whose track meets the box is being watched: where it will be isn't known yet
    bool Watching(bool half, const Box& box) const;
    // A track meets the box and no mine is on it now (its screen has yet to load, or it has blown up)
    bool Missing(bool half, const Box& box) const;
    // The frames a lap of it takes, x256 (0: not known, or it keeps still)
    s32 Lap256(u32 id) const;
    // Its box that many frames from the last one seen. False if there is no such mine.
    // (as it is now, while its lap isn't known)
    bool At(u32 id, s32 frames, Box& box) const;
    // One of them at that scale meets the box that many frames from now
    bool Hits(bool half, s32 frames, const Box& box) const;
    // A mine at that scale can meet the box at some time: its track goes there (one that is on no track of the
    // map: its lap does, or while that isn't known it is within 150 pixels now)
    bool Ever(bool half, const Box& box) const;
    // The frames from `from` on until the box is first met, up to `within` (-1: not within that)
    s32 Next(bool half, const Box& box, s32 from, s32 within) const;

private:
    struct Pt final
    {
        s32 mX = 0;
        s32 mY = 0;
    };
    struct Bomb final
    {
        Seen mNow;
        // Where it was each frame since it has been at its pace (while the lap is watched), then the lap
        std::vector<Pt> mTrack;
        // What it has moved each frame of late, x256
        std::vector<s32> mSteps;
        bool mLapped = false;
        // What it moves a frame at its pace, x256, and whether it does now
        s32 mPace = 0;
        bool mAtPace = false;
        // There now; and the track of the map it is on, or -1
        bool mSeen = false;
        bool mMissed = false;
        s32 mTrackAt = -1;
        // The frames from the last place of the lap back to the first, x256
        s32 mClose = 0;
        // Where on the lap it is now: frames past mTrack[0], x256
        s32 mPhase = 0;
        s32 mStill = 0;
        // Its box about where it is, each frame of late (it changes with its animation), and the frames after
        // which that comes round (0: not known)
        std::vector<Seen> mBoxes;
        s32 mBoxPeriod = 0;
    };

    static s32 Dist(const Pt& a, const Pt& b);
    void Watch(Bomb& bomb, const Seen& seen);
    static void Locate(Bomb& bomb);
    static Pt Place(const Bomb& bomb, s32 frames);
    static Box BoxAt(const Bomb& bomb, const Pt& at, s32 frames);
    static void WatchBox(Bomb& bomb, const Seen& seen, bool fresh);
    const Bomb* Find(u32 id) const;

    static bool Ready(const Bomb& bomb)
    {
        return bomb.mSeen && ((bomb.mLapped && bomb.mAtPace) || bomb.mStill >= kStill);
    }
    bool TrackMeets(std::size_t track, bool half, const Box& box) const;

    std::vector<Bomb> mBombs;
    s32 mSeen = 0;
    std::vector<Track> mTracks;
    // Places along each track, a few pixels apart
    std::vector<std::vector<Pt>> mAlong;
    s32 mTrackKey = -1;
    u32 mFrame = 0;
    s32 mKey = -1;
};

} // namespace Bot
