#pragma once

#include "NavGraph.hpp"
#include <vector>

namespace Bot {

struct World;

// Flying a fart Abe has taken over (AE's EvilFart.cpp). After a Brew his next fart hangs where he made it and goes
// off 220 frames later; chanted at, it is his to fly for 900 frames. Each frame a held direction adds a fifth of a
// pixel a frame to its speed that way, up to 4, and a direction let go takes a fifth off. Walls stop it sideways,
// floors and ceilings up and down (it loses that part of its speed, nothing else). Chanting while it stands still
// sets it off 15 frames later, in a box 60 to each side, 60 up and 30 down from a point 50 over its own.
//
// All points here are of that point 50 over its own (the middle of the blast), which is also about where the
// game tests it against the walls (54 over).
class FartFlight final
{
public:
    struct Point final
    {
        s32 mX = 0;
        s32 mY = 0;
    };

    // The blast: how far to each side, up and down from its middle something's box is still hit
    static const s32 kBlastX = 60;
    static const s32 kBlastUp = 60;
    static const s32 kBlastDown = 30;
    // What the middle of the blast is over the fart's own point
    static const s32 kBody = 50;
    // The frames it can be flown, and the frames from the chant to the blast
    static const s32 kFrames = 900;
    static const s32 kFuse = 15;

    // The corners of a way through the air from one point to another, the last being the end itself. False when
    // there is none no longer than maxLen.
    // more: what else is in the way (shut gates, trap doors), as lines of type 1 (stops it sideways) or 0 (up and
    // down)
    static bool Route(const NavInput& in, bool half, Point from, Point to, s32 maxLen, std::vector<Point>& corners, s32* length = nullptr, const std::vector<NavLine>* more = nullptr);

    // The gates and trap doors of the path a fart can't get through as lines for Route: all of them, or with a
    // world only those that are shut in it. features: which feature of the path each is.
    static std::vector<NavLine> Gates(const NavInput& in, bool half, const World* world, std::vector<s32>* features = nullptr);
    // Whether a way (from, then its corners) goes through a line
    static bool Through(Point from, const std::vector<Point>& corners, const NavLine& line);

    // What to hold (Btn bits) to get from where it is to (tx, ty) and stand still there. Places and speeds are in
    // hundredths of a pixel.
    static u32 Steer(s32 x100, s32 y100, s32 velX100, s32 velY100, s32 tx, s32 ty);

    // How far it still goes with nothing held, in hundredths of a pixel
    static s32 Coast(s32 vel100);

    // Whether a blast with its middle at (x, y) hits a box
    static bool Hits(Point at, s32 x1, s32 y1, s32 x2, s32 y2);
};

} // namespace Bot
