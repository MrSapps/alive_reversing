#pragma once

#include "../Types.hpp"
#include <vector>

namespace Bot {

// Flying a flying Slig Abe has taken over (AE's FlyingSlig.cpp). A held direction adds 0.7 of a pixel a frame to
// its speed that way (each way of a diagonal 0.707 of that); every frame it loses a quarter of a pixel a frame,
// along the way it goes, for each whole pixel a frame of its speed (the game divides by the speed rounded down),
// and under one pixel a frame it loses nothing while a direction is held and stops dead when none is. Its speed
// is cut to 5.5 once it is 6 or more. It bounces off what it flies into. Throw sends a grenade the way it faces
// that goes off where it first comes down; action pulls a lever one to two cells ahead of it.
//
// Places and speeds are in hundredths of a pixel, of its own point: the game shows it kBody under that.
class FlierFlight final
{
public:
    static constexpr s32 kSpeedUp = 70;
    static constexpr s32 kSlowDown = 25;
    static constexpr s32 kTopSpeed = 550;
    static constexpr s32 kBody = 20;

    // Its speed a frame on with these buttons (Btn bits) held
    static void Step(s32& velX, s32& velY, u32 held);
    // How far it still goes with nothing held
    static void Rest(s32 velX, s32 velY, s32& dx, s32& dy);
    // What to hold to come to rest at (tx, ty), in pixels: the buttons that leave it nearest to there if they
    // are let go a frame on
    static u32 Steer(s32 x, s32 y, s32 velX, s32 velY, s32 tx, s32 ty);
    // How near to the place asked for it is left to stop (hundredths of a pixel)
    static constexpr s32 kNear = 200;

    // Its grenade leaves kGrenadeUp over its point at 5 to 6.4 pixels a frame along and 6 up, and falls a pixel a
    // frame faster each frame. The frames until it is `drop` pixels under the Slig's point, and how far along
    // it is then at the middle speed.
    static constexpr s32 kGrenadeUp = 5;
    static constexpr s32 kGrenadeSpeed = 570;
    static s32 GrenadeFrames(s32 drop);
    // Where it is a number of frames after it is thrown from (x, y) (the Slig's point), at the middle speed
    static void GrenadeAt(s32 x, s32 y, bool left, s32 frames, s32& gx, s32& gy);
    static s32 GrenadeReach(s32 drop);
    // The blast of one (a small one: half an explosion's box) about where it went off, against a thing's box
    static constexpr s32 kBlastX = 30;
    static constexpr s32 kBlastUp = 30;
    static constexpr s32 kBlastDown = 15;

    // A Slig shoots what is on its level and in front of it (see kLevelOver). This far over a Slig's floor the
    // flying Slig is off its level.
    static constexpr s32 kOver = 60;
    // What is on a Slig's level: the flying Slig's point from this far over the Slig's floor to this far under
    // it. Its box is from 23 over its point to 8 under, a Slig's from 38 over its floor to 2 over (read from
    // the game in the Barracks' path 1), and a Slig shoots what has its middle no lower than the Slig's feet
    // and its bottom no higher than the Slig's head: from 46 over the floor to 5 under. With a little to spare.
    static constexpr s32 kLevelOver = 52;
    static constexpr s32 kLevelUnder = 10;

    // Where to throw from at a Slig that stands at x on a floor that goes from x1 to x2, off its level, and where
    // and when the grenade then comes down: the likeliest first. From just under an end of the floor the Slig
    // is near, a little way out from it: the grenade goes up over the edge and comes down on the floor (nothing
    // on that floor sees the flying Slig there, nor does a Slig of the floor above). Then from over its head,
    // to either side.
    struct Shot final
    {
        s32 mX = 0;
        s32 mY = 0;
        bool mFaceLeft = false;
        s32 mLandX = 0;
        s32 mFrames = 0;
        // Where it waits for the moment to throw, off the Slig's level: the place itself, or under it
        s32 mWaitX = 0;
        s32 mWaitY = 0;
        // How much nearer the Slig it may throw from, for the grenade to come down that much further on (from
        // under the end of a floor: up to where its own side is still clear of the floor's end, and the
        // grenade, which is level with the floor a pixel or two out from it, still gets over the edge)
        s32 mNearer = 0;
    };
    static std::vector<Shot> Shots(s32 sligX, s32 floorY, s32 floorX1, s32 floorX2);
    // Where it pulls a lever from: action, at rest and facing it, with the lever one to two cells ahead and not
    // much over or under it, takes it to the lever and pulls. The lever stands at x on the floor floorY (its
    // box is from 38 over the floor to it). From its left, or from its right.
    static Shot LeverPull(s32 leverX, s32 floorY, bool fromRight);
    static constexpr s32 kLeverAlong = 46;
    static constexpr s32 kLeverOver = 23;
    // How far under the floor and out from its end the throw from under it is made, and how near that end the
    // Slig has to be for it
    static constexpr s32 kUnder = 12;
    static constexpr s32 kWaitUnder = 36;
    static constexpr s32 kOut = 40;
    static constexpr s32 kOutLeast = 20;
    static constexpr s32 kNearEnd = 90;
};

} // namespace Bot
