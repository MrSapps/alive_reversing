#pragma once

#include "Domain.hpp"
#include "NavGraph.hpp"
#include <vector>

namespace Bot {

// How a thrown rock flies. Not found by trying: where it leaves the hand and how fast, for each kind of throw,
// was measured in the game (the table in ThrowArc.cpp), and from there on a rock only falls and bounces, so
// its place on every frame is known before it is thrown. A mine goes off when the rock's box touches its own,
// which is while the rock is up to a dozen pixels over the mine's floor: higher than that it flies over.
class ThrowArc final
{
public:
    struct Point final
    {
        s32 mX = 0;
        s32 mY = 0;
    };

    struct Flight final
    {
        // Where the rock is at the end of each frame after it leaves the hand
        std::vector<Point> mPoints;
        // The frame the mine aimed at goes off, or -1
        s32 mHit = -1;
        // Whether it goes off whatever frame the mine's light is at (its box is a little wider with the light on)
        bool mSure = false;
    };

    // A throw from a cell (its middle, its floor). With a mine to aim at (its cell's middle, its floor), the
    // rock bounces off it as it sets it off.
    static Flight Throw(const NavInput& in, s32 x, s32 y, bool half, bool left, ThrowSpot::Kind kind, const Point* mine = nullptr);

    // Whether a rock there sets off a mine at (x, floor y): sure is on any frame of the mine's light, not sure is
    // on some
    static bool SetsOff(const Point& rock, s32 mineX, s32 mineY, bool half, bool sure);

    // The frame of a flight on which another mine would go off, or -1
    static s32 HitFrame(const Flight& flight, s32 mineX, s32 mineY, bool half, bool sure);

    // Whether the rock's box is on the mine's: it bounces off as the mine goes off
    static bool Touches(const Point& rock, s32 mineX, s32 mineY, bool half, bool sure);

    // Whether the mine's own look-out finds the rock: within its sides and not far over or under its floor
    static bool Sees(const Point& rock, s32 mineX, s32 mineY, bool half, bool sure);

    // How near a mine's blast kills: along its floor, and up and down from it (full scale). The game's box is
    // 113 by 50 about the mine, and it is where Abe stands that has to be in it.
    static const s32 kBlastX = 118;
    static const s32 kBlastY = 55;
    // AO's grenade: the frames it flies and lies before it goes off (its fuse is 90 from when the throw is
    // begun), and how near a mine its blast sets it off and kills (the game's box grows to 60 either side, 60 up
    // and 30 down, and it is the boxes that have to touch)
    static const s32 kFuse = 76;
    static const s32 kGrenadeReach = 45;
    static const s32 kGrenadeKillX = 85;
    static const s32 kGrenadeKillUp = 70;
    static const s32 kGrenadeKillDown = 95;

private:
    static Flight Fly(const NavInput& in, s32 x, s32 y, bool half, bool left, ThrowSpot::Kind kind, const Point* mine, bool sure);
    // AO's grenade: thrown like the rock, but it comes to rest and goes off when its fuse has run down, and its
    // blast sets off the mines in it. mHit is then the last frame.
    static Flight FlyGrenade(const NavInput& in, s32 x, s32 y, bool half, bool left, ThrowSpot::Kind kind, const Point* mine);
};

} // namespace Bot
