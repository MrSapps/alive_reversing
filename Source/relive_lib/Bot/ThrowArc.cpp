#include "ThrowArc.hpp"
#include <algorithm>
#include <cmath>

namespace Bot {

// Measured in the Mines (path 4, at 962,460, facing right, `build-bot/arc2.txt`), full scale. At half scale the
// offsets and speeds are half and the pull down is the same.
struct ThrowStart final
{
    // From the cell's middle and the floor
    double mX;
    double mY;
    // A frame
    double mVelX;
    double mVelY;
};

static const ThrowStart kThrows[] = {
    {11, -55, 10, -10}, // eUp
    {11, -55, 3, -14},  // eLob
    {11, -55, 15, -8},  // eForward
    {11, -55, 10, 3},   // eDown
    {22, -15, 10, -4},  // eCrouched
};

static const double kPullAe = 1.01;
static const double kPullAo = 1.0;

enum LineType : s32
{
    eFloor = 0,
    eWallLeft = 1,
    eWallRight = 2,
    eCeiling = 3,
    eBgFloor = 4,
    eBgWallLeft = 5,
    eBgWallRight = 6,
    eBgCeiling = 7,
};

// Where the way from a to b first crosses one of the lines wanted: the line, or nullptr
static const NavLine* FirstCrossed(const NavInput& in, double ax, double ay, double bx, double by, s32 typeA, s32 typeB, double* hitX, double* hitY)
{
    const NavLine* best = nullptr;
    double bestT = 2.0;
    const double dx = bx - ax;
    const double dy = by - ay;
    for (const NavLine& line : in.mLines)
    {
        if (line.mType != typeA && line.mType != typeB)
        {
            continue;
        }
        if (std::min(line.mX1, line.mX2) > std::max(ax, bx) || std::max(line.mX1, line.mX2) < std::min(ax, bx) || std::min(line.mY1, line.mY2) > std::max(ay, by) || std::max(line.mY1, line.mY2) < std::min(ay, by))
        {
            continue;
        }
        const double lx = line.mX2 - line.mX1;
        const double ly = line.mY2 - line.mY1;
        const double det = lx * dy - dx * ly;
        if (std::fabs(det) < 0.004)
        {
            continue;
        }
        // a + t * d = line1 + u * l
        const double t = (lx * (line.mY1 - ay) - ly * (line.mX1 - ax)) / det;
        const double u = (dx * (line.mY1 - ay) - dy * (line.mX1 - ax)) / det;
        if (t < 0.0 || t > 1.0 || u < 0.0 || u > 1.0)
        {
            continue;
        }
        if (t < bestT)
        {
            bestT = t;
            best = &line;
            *hitX = ax + dx * t;
            *hitY = ay + dy * t;
        }
    }
    return best;
}

static ThrowArc::Point Whole(double x, double y)
{
    return {static_cast<s32>(std::floor(x)), static_cast<s32>(std::floor(y))};
}

ThrowArc::Flight ThrowArc::Throw(const NavInput& in, s32 x, s32 y, bool half, bool left, ThrowSpot::Kind kind, const Point* mine)
{
    // A frame on which the rock is sure to set the mine off, whatever came of the frames it only may have
    if (!in.mIsAe)
    {
        return FlyGrenade(in, x, y, half, left, kind, mine);
    }
    Flight flight = Fly(in, x, y, half, left, kind, mine, true);
    if (mine && flight.mHit < 0)
    {
        flight = Fly(in, x, y, half, left, kind, mine, false);
    }
    return flight;
}

ThrowArc::Flight ThrowArc::Fly(const NavInput& in, s32 x, s32 y, bool half, bool left, ThrowSpot::Kind kind, const Point* mine, bool sure)
{
    Flight flight;
    const ThrowStart& start = kThrows[static_cast<s32>(kind)];
    const double scale = half ? 0.5 : 1.0;
    const double pull = in.mIsAe ? kPullAe : kPullAo;
    const s32 floorType = half ? eBgFloor : eFloor;
    const s32 ceilingType = half ? eBgCeiling : eCeiling;
    const s32 wallLeft = half ? eBgWallLeft : eWallLeft;
    const s32 wallRight = half ? eBgWallRight : eWallRight;
    // The screen it is thrown on: off it a rock goes through floors
    const s32 camX = (x / in.mCamW) * in.mCamW;
    const s32 camY = ((y - 1) / in.mCamH) * in.mCamH;

    double velX = (left ? -start.mVelX : start.mVelX) * scale;
    double velY = start.mVelY * scale;
    double px = x + (left ? -start.mX : start.mX) * scale;
    double py = y + start.mY * scale;
    double hitX = 0.0;
    double hitY = 0.0;
    // A wall between hand and rock: it starts at the wall and goes the other way
    if (FirstCrossed(in, x, py, px, py, wallLeft, wallRight, &hitX, &hitY))
    {
        px = hitX;
        velX = -velX;
    }
    flight.mPoints.push_back(Whole(px, py));

    // Through a floor it was too slow to bounce off: from then on it only falls, through everything, and can
    // still come down on a mine on a floor below
    s32 falling = 0;
    for (s32 frame = 0; frame < 120 && velY <= 30.0 && falling < 40 && py < camY + 260; frame++)
    {
        const double fromX = px;
        const double fromY = py;
        velY += pull;
        px += velX;
        py += velY;
        if (falling > 0)
        {
            falling++;
        }
        else
        {
            bool through = false;
            // (the game takes the first floor or ceiling crossed, then asks which it is)
            const NavLine* line = FirstCrossed(in, fromX, fromY, px, py, floorType, ceilingType, &hitX, &hitY);
            if (line && line->mType == floorType && velY > 0.0)
            {
                const bool onScreen = px >= camX && px <= camX + 368 && py >= camY && py <= camY + 240;
                if (!onScreen)
                {
                    through = true;
                }
                else if (velY < 5.0)
                {
                    falling = 1;
                    through = true;
                }
                else
                {
                    py = hitY;
                    velX /= 2.0;
                    velY = -velY / 2.0;
                }
            }
            else if (line && line->mType == ceilingType && velY < 0.0)
            {
                py = hitY;
                velY = -velY / 2.0;
            }
            if (!through)
            {
                line = FirstCrossed(in, fromX, fromY, px, py, wallLeft, wallRight, &hitX, &hitY);
                if (line && ((line->mType == wallLeft && velX < 0.0) || (line->mType == wallRight && velX > 0.0)))
                {
                    velX = -velX / 2.0;
                    px = hitX;
                    py = hitY;
                }
            }
        }
        // The mine: it goes off, and the rock comes back off it at half the speed (off its side if the rock
        // came from beside it, else off its top)
        // (one falling through the floors no longer looks for what it touches: the mine has to see it)
        if (mine && flight.mHit < 0 && (falling > 0 ? Sees(Whole(px, py), mine->mX, mine->mY, half, sure) : SetsOff(Whole(px, py), mine->mX, mine->mY, half, sure)))
        {
            flight.mHit = static_cast<s32>(flight.mPoints.size());
            flight.mSure = sure;
            if (falling == 0 && Touches(Whole(px, py), mine->mX, mine->mY, half, sure))
            {
                const double halfWidth = half ? 2.5 : 5.0;
                if (fromX < mine->mX - halfWidth || fromX > mine->mX + halfWidth)
                {
                    px -= velX;
                    velX = -velX / 2.0;
                }
                else
                {
                    py -= velY;
                    velY = -velY / 2.0;
                }
            }
        }
        flight.mPoints.push_back(Whole(px, py));
    }
    return flight;
}

// Grenade.cpp: a pixel a frame faster down each frame with no limit; off a floor it comes down on at half the
// speed both ways, and to rest on it when it is slower than one down; off a wall at half the speed back; through
// a floor it comes up under. At rest it slides on, half as fast each frame. A mine it touches goes off at once
// (Mine::VOnThrowableHit); else it goes off itself at the end of its fuse, and the mines its blast reaches with it.
ThrowArc::Flight ThrowArc::FlyGrenade(const NavInput& in, s32 x, s32 y, bool half, bool left, ThrowSpot::Kind kind, const Point* mine)
{
    Flight flight;
    const ThrowStart& start = kThrows[static_cast<s32>(kind)];
    const double scale = half ? 0.5 : 1.0;
    const s32 floorType = half ? eBgFloor : eFloor;
    const s32 wallLeft = half ? eBgWallLeft : eWallLeft;
    const s32 wallRight = half ? eBgWallRight : eWallRight;
    double velX = (left ? -start.mVelX : start.mVelX) * scale;
    double velY = start.mVelY * scale;
    double px = x + (left ? -start.mX : start.mX) * scale;
    double py = y + start.mY * scale;
    double hitX = 0.0;
    double hitY = 0.0;
    if (FirstCrossed(in, x, py, px, py, wallLeft, wallRight, &hitX, &hitY))
    {
        px = hitX;
        velX = -velX;
    }
    flight.mPoints.push_back(Whole(px, py));
    bool resting = false;
    for (s32 frame = 0; frame < kFuse; frame++)
    {
        if (resting)
        {
            velX /= 2.0;
            px += velX;
        }
        else
        {
            const double fromX = px;
            const double fromY = py;
            velY += 1.0;
            px += velX;
            py += velY;
            // (AE's comes back down off a floor it flies up under, as fast along; AO's goes through it)
            // (going up from a floor it has just come back off, it is not under that floor)
            const NavLine* line = velY > 0.0 ? FirstCrossed(in, fromX, fromY, px, py, floorType, floorType, &hitX, &hitY) : in.mIsAe ? FirstCrossed(in, fromX, fromY - 0.01, px, py, floorType, floorType, &hitX, &hitY) : nullptr;
            if (line && velY <= 0.0)
            {
                px = hitX;
                py = hitY + 0.01;
                velY = -velY / 2.0;
                flight.mUnder = true;
            }
            else if (line)
            {
                py = hitY;
                if (velY < 1.0 + 1e-9)
                {
                    resting = true;
                    velY = 0.0;
                }
                else
                {
                    velY = -velY / 2.0;
                    velX /= 2.0;
                }
            }
            else
            {
                line = FirstCrossed(in, fromX, fromY, px, py, wallLeft, wallRight, &hitX, &hitY);
                if (line && ((line->mType == wallLeft && velX < 0.0) || (line->mType == wallRight && velX > 0.0)))
                {
                    velX = -velX / 2.0;
                    px = hitX;
                    py = hitY;
                }
            }
            if (mine && Touches(Whole(px, py), mine->mX, mine->mY, half, true))
            {
                flight.mPoints.push_back(Whole(px, py));
                flight.mHit = static_cast<s32>(flight.mPoints.size()) - 1;
                flight.mSure = true;
                return flight;
            }
        }
        flight.mPoints.push_back(Whole(px, py));
    }
    if (mine)
    {
        const s32 dx = std::abs(static_cast<s32>(px) - mine->mX) * (half ? 2 : 1);
        const s32 dy = std::abs(static_cast<s32>(py) - mine->mY) * (half ? 2 : 1);
        if (dx <= kGrenadeReach && dy <= 15)
        {
            flight.mHit = static_cast<s32>(flight.mPoints.size()) - 1;
            flight.mSure = resting && dx <= kGrenadeReach - 8;
        }
    }
    return flight;
}

ThrowArc::Flight ThrowArc::Grenade(const NavInput& in, s32 x, s32 y, bool half, bool left, ThrowSpot::Kind kind)
{
    return FlyGrenade(in, x, y, half, left, kind, nullptr);
}

// The rock's box, five lower, on the mine's (measured: the rock's is 4 or 5 either side of it and 9 or 10 to 2
// to 4 over it, the mine's 4 or 5 left to 5 or 7 right and 9 to 4 over its floor)
bool ThrowArc::Touches(const Point& rock, s32 mineX, s32 mineY, bool half, bool sure)
{
    const s32 dx = rock.mX - mineX;
    const s32 dy = rock.mY - mineY;
    if (half)
    {
        // (not measured: half of the full scale boxes)
        return sure ? (dx >= -3 && dx <= 4 && dy >= -5 && dy <= -1) : (dx >= -5 && dx <= 6 && dy >= -6 && dy <= 0);
    }
    return sure ? (dx >= -7 && dx <= 9 && dy >= -11 && dy <= -1) : (dx >= -9 && dx <= 12 && dy >= -12 && dy <= 0);
}

bool ThrowArc::SetsOff(const Point& rock, s32 mineX, s32 mineY, bool half, bool sure)
{
    return Touches(rock, mineX, mineY, half, sure) || Sees(rock, mineX, mineY, half, sure);
}

// The mine looks for itself as well: something within its sides and not far over or under its floor
bool ThrowArc::Sees(const Point& rock, s32 mineX, s32 mineY, bool half, bool sure)
{
    const s32 dx = rock.mX - mineX;
    const s32 dy = rock.mY - mineY;
    if (half)
    {
        return sure ? (dx >= -1 && dx <= 2 && dy >= -2 && dy <= 2) : (dx >= -2 && dx <= 3 && dy >= -4 && dy <= 3);
    }
    return sure ? (dx >= -3 && dx <= 4 && dy >= -5 && dy <= 5) : (dx >= -4 && dx <= 6 && dy >= -7 && dy <= 6);
}

s32 ThrowArc::HitFrame(const Flight& flight, s32 mineX, s32 mineY, bool half, bool sure)
{
    for (std::size_t i = 0; i < flight.mPoints.size(); i++)
    {
        if (SetsOff(flight.mPoints[i], mineX, mineY, half, sure))
        {
            return static_cast<s32>(i);
        }
    }
    return -1;
}

} // namespace Bot
