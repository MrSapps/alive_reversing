#include "FlierFlight.hpp"
#include "GameView.hpp"
#include <cstdlib>

namespace Bot {

// The whole pixels a frame of a speed, as the game takes them: the root of the whole part of its square
static s32 WholeSpeed(s32 velX, s32 velY)
{
    const s64 square = (static_cast<s64>(velX) * velX + static_cast<s64>(velY) * velY) / 10000;
    s32 root = 0;
    while (static_cast<s64>(root + 1) * (root + 1) <= square)
    {
        root++;
    }
    return root;
}

void FlierFlight::Step(s32& velX, s32& velY, u32 held)
{
    // (up before down and left before right, as the game asks for them; a diagonal only with up or down)
    const s32 diagonal = kSpeedUp * 707 / 1000;
    s32 addX = 0;
    s32 addY = 0;
    const bool left = (held & Btn::eLeft) != 0;
    const bool right = (held & Btn::eRight) != 0;
    if ((held & (Btn::eUp | Btn::eDown)) != 0)
    {
        const s32 sign = (held & Btn::eUp) != 0 ? -1 : 1;
        addY = sign * (left || right ? diagonal : kSpeedUp);
        addX = left ? -diagonal : right ? diagonal : 0;
    }
    else
    {
        addX = left ? -kSpeedUp : right ? kSpeedUp : 0;
    }
    const s32 whole = WholeSpeed(velX, velY);
    if (whole >= 1)
    {
        velX += addX - velX * kSlowDown / (100 * whole);
        velY += addY - velY * kSlowDown / (100 * whole);
    }
    else if (addX != 0 || addY != 0)
    {
        velX += addX;
        velY += addY;
    }
    else
    {
        velX = 0;
        velY = 0;
    }
    const s32 after = WholeSpeed(velX, velY);
    if (after * 100 > kTopSpeed)
    {
        velX = velX * kTopSpeed / (after * 100);
        velY = velY * kTopSpeed / (after * 100);
    }
}

void FlierFlight::Rest(s32 velX, s32 velY, s32& dx, s32& dy)
{
    dx = 0;
    dy = 0;
    for (s32 guard = 0; guard < 200 && (velX != 0 || velY != 0); guard++)
    {
        Step(velX, velY, 0);
        dx += velX;
        dy += velY;
    }
}

u32 FlierFlight::Steer(s32 x, s32 y, s32 velX, s32 velY, s32 tx, s32 ty)
{
    static const u32 kHeld[] = {
        0,
        Btn::eLeft,
        Btn::eRight,
        Btn::eUp,
        Btn::eDown,
        Btn::eUp | Btn::eLeft,
        Btn::eUp | Btn::eRight,
        Btn::eDown | Btn::eLeft,
        Btn::eDown | Btn::eRight,
    };
    // (within two pixels of it and slower than a pixel a frame: let go, it stops dead there. Nearer is not worth
    // having: in the game every press for the last fraction of a pixel overshot and it never came to rest)
    if (WholeSpeed(velX, velY) == 0 && std::abs(tx * 100 - x) <= kNear && std::abs(ty * 100 - y) <= kNear)
    {
        return 0;
    }
    u32 best = 0;
    s64 bestFar = -1;
    for (const u32 held : kHeld)
    {
        s32 vx = velX;
        s32 vy = velY;
        Step(vx, vy, held);
        s32 dx = 0;
        s32 dy = 0;
        Rest(vx, vy, dx, dy);
        const s64 ex = static_cast<s64>(tx) * 100 - (x + vx + dx);
        const s64 ey = static_cast<s64>(ty) * 100 - (y + vy + dy);
        const s64 far = ex * ex + ey * ey;
        if (bestFar < 0 || far < bestFar)
        {
            bestFar = far;
            best = held;
        }
    }
    return best;
}

s32 FlierFlight::GrenadeFrames(s32 drop)
{
    // (on its way down: it is over a floor that is over the Slig's point on its way up too)
    s32 frames = 0;
    s32 y = -kGrenadeUp;
    s32 velY = -6;
    while ((y < drop || velY <= 0) && frames < 400)
    {
        velY++;
        y += velY;
        frames++;
    }
    return frames;
}

void FlierFlight::GrenadeAt(s32 x, s32 y, bool left, s32 frames, s32& gx, s32& gy)
{
    gx = x + (left ? -1 : 1) * frames * kGrenadeSpeed / 100;
    gy = y - kGrenadeUp;
    s32 velY = -6;
    for (s32 i = 0; i < frames; i++)
    {
        velY++;
        gy += velY;
    }
}

s32 FlierFlight::GrenadeReach(s32 drop)
{
    return GrenadeFrames(drop) * kGrenadeSpeed / 100;
}

FlierFlight::Shot FlierFlight::LeverPull(s32 leverX, s32 floorY, bool fromRight)
{
    Shot pull;
    pull.mX = leverX + (fromRight ? kLeverAlong : -kLeverAlong);
    pull.mY = floorY - kLeverOver;
    pull.mFaceLeft = fromRight;
    pull.mLandX = leverX;
    pull.mWaitX = pull.mX;
    pull.mWaitY = pull.mY;
    return pull;
}

std::vector<FlierFlight::Shot> FlierFlight::Shots(s32 sligX, s32 floorY, s32 floorX1, s32 floorX2)
{
    std::vector<Shot> shots;
    // From under an end of its floor: thrown kUnder under the floor the grenade is over it from the fourth frame
    // to the frame it comes down on it
    const s32 frames = GrenadeFrames(-kUnder);
    const s32 reach = frames * kGrenadeSpeed / 100;
    if (floorX2 - sligX <= kNearEnd && floorX2 - reach + kOut >= floorX1)
    {
        shots.push_back({floorX2 + kOut, floorY + kUnder, true, floorX2 + kOut - reach, frames, floorX2 + kOut, floorY + kWaitUnder, kOut - kOutLeast});
    }
    if (sligX - floorX1 <= kNearEnd && floorX1 + reach - kOut <= floorX2)
    {
        shots.push_back({floorX1 - kOut, floorY + kUnder, false, floorX1 - kOut + reach, frames, floorX1 - kOut, floorY + kWaitUnder, kOut - kOutLeast});
    }
    static const s32 kOvers[] = {kOver, kOver + 20, kOver + 40};
    for (const s32 over : kOvers)
    {
        const s32 along = GrenadeReach(over);
        shots.push_back({sligX + along, floorY - over, true, sligX, GrenadeFrames(over), sligX + along, floorY - over, 0});
        shots.push_back({sligX - along, floorY - over, false, sligX, GrenadeFrames(over), sligX - along, floorY - over, 0});
    }
    return shots;
}

} // namespace Bot
