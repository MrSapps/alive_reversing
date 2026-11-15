#include "NavGraph.hpp"
#include <algorithm>
#include <cstdlib>
#include <queue>

namespace Bot {

// eLineTypes
static const s32 kFloor = 0;
static const s32 kWallLeft = 1;
static const s32 kWallRight = 2;
static const s32 kCeiling = 3;
static const s32 kBgFloor = 4;
static const s32 kBgCeiling = 7;
static const s32 kBgWallLeft = 5;
static const s32 kBgWallRight = 6;

// Rough frames
static const s32 kSweepReach = 20;
static const s32 kWalkCost = 6;
static const s32 kRollCost = 5;
static const s32 kHopCost = 26;
static const s32 kRunJumpCost = 60;
// A running jump: cells of run-up, cells in the air to a floor, and to a ledge it grabs
static const s32 kRunUpCells = 3;
static const s32 kRunJumpCells = 4;
static const s32 kRunJumpLedgeCells = 5;
static const s32 kHoistCost = 55;
static const s32 kDoorCost = 80;
static const s32 kWellCost = 100;
// A fall this far kills (at full scale)
static const s32 kDeadlyFall = 180;

const char* EdgeKindName(EdgeKind kind)
{
    switch (kind)
    {
        case EdgeKind::eWalk: return "walk";
        case EdgeKind::eFall: return "fall";
        case EdgeKind::eHop: return "hop";
        case EdgeKind::eRunJump: return "runjump";
        case EdgeKind::eRoll: return "roll";
        case EdgeKind::eLift: return "lift";
        case EdgeKind::eHoistUp: return "hoist_up";
        case EdgeKind::eHoistDown: return "hoist_down";
        case EdgeKind::eDoor: return "door";
        case EdgeKind::eWell: return "well";
        case EdgeKind::eTeleport: return "teleport";
        case EdgeKind::ePortal: return "portal";
    }
    return "?";
}

static s32 Mod(s32 a, s32 m)
{
    const s32 r = a % m;
    return r < 0 ? r + m : r;
}

s32 NavGraph::SnapUp(s32 x, bool half) const
{
    if (!half)
    {
        if (mInput.mIsAe)
        {
            const s32 r = Mod(x - 12, 25);
            return r == 0 ? x : x + 25 - r;
        }
        // AO's grid is counted from each camera's own left edge, and a camera isn't a whole number of cells wide
        const s32 local = Mod(x, 1024);
        const s32 r = Mod(local - 15, 25);
        return r == 0 ? x : x + 25 - r;
    }
    for (s32 i = 0; i < 14; i++)
    {
        const s32 r = mInput.mIsAe ? Mod(Mod(x + i, 375) - 6, 13) : Mod(Mod(x + i, 1024) - 11, 13);
        if (r == 0)
        {
            return x + i;
        }
    }
    return x;
}

bool NavGraph::IsFloor(const NavLine& line, bool half) const
{
    return line.mType == (half ? kBgFloor : kFloor) && line.mX1 != line.mX2;
}

static s32 YOnLine(const NavLine& line, s32 x)
{
    if (line.mX1 == line.mX2)
    {
        return line.mY1;
    }
    x = std::max(std::min(line.mX1, line.mX2), std::min(std::max(line.mX1, line.mX2), x));
    return line.mY1 + (line.mY2 - line.mY1) * (x - line.mX1) / (line.mX2 - line.mX1);
}

// A floor counts for a few pixels past its ends: the grid and the lines don't always meet (AO puts Abe on the first
// cell of a camera and then looks for the line near him)
static const s32 kLineEndSlack = 3;

static bool Covers(const NavLine& line, s32 x)
{
    return x >= std::min(line.mX1, line.mX2) - kLineEndSlack && x <= std::max(line.mX1, line.mX2) + kLineEndSlack;
}

// AO's cameras are 1024x480 apart in the world but only show 368x240 of it: the rest is a gap that is skipped
static const s32 kAoCamW = 1024;
static const s32 kAoCamH = 480;
static const s32 kAoFirstCellX = 240;
static const s32 kAoLastCellX = 615;
static const s32 kAoFirstCellXHalf = 245;
static const s32 kAoLastCellXHalf = 622;
static const s32 kAoTopY = 120;
static const s32 kAoBottomY = 360;

static s32 FloorDiv(s32 a, s32 m)
{
    return (a - Mod(a, m)) / m;
}

s32 NavGraph::StepX(s32 x, s32 dir, bool half) const
{
    if (half && mInput.mIsAe)
    {
        if (dir > 0)
        {
            return SnapUp(x + 1, true);
        }
        s32 nx = x - 13;
        while (SnapUp(nx, true) >= x)
        {
            nx--;
        }
        return SnapUp(nx, true);
    }
    const s32 step = half ? 13 : 25;
    const s32 nx = x + dir * step;
    if (mInput.mIsAe)
    {
        return nx;
    }
    const s32 first = half ? kAoFirstCellXHalf : kAoFirstCellX;
    const s32 last = half ? kAoLastCellXHalf : kAoLastCellX;
    const s32 camLeft = FloorDiv(x, kAoCamW) * kAoCamW;
    if (nx - camLeft > last)
    {
        return camLeft + kAoCamW + first;
    }
    if (nx - camLeft < first)
    {
        return camLeft - kAoCamW + last;
    }
    return nx;
}

s32 NavGraph::FloorBelow(s32 x, s32 y, bool half, s32* floorY, s32* fallDistance) const
{
    // The first floor at or under y, no lower than maxY
    const auto search = [&](s32 fromY, s32 maxY, s32& outY)
    {
        s32 best = -1;
        for (std::size_t i = 0; i < mInput.mLines.size(); i++)
        {
            const NavLine& line = mInput.mLines[i];
            if (!IsFloor(line, half) || !Covers(line, x))
            {
                continue;
            }
            const s32 ly = YOnLine(line, x);
            if (ly >= fromY && ly <= maxY && (best < 0 || ly < outY))
            {
                best = static_cast<s32>(i);
                outY = ly;
            }
        }
        return best;
    };

    s32 foundY = 0;
    s32 found = -1;
    s32 distance = 0;
    if (mInput.mIsAe)
    {
        found = search(y, 1 << 30, foundY);
        distance = foundY - y;
    }
    else
    {
        const s32 camTop = FloorDiv(y, kAoCamH) * kAoCamH;
        found = search(y, camTop + kAoBottomY + 20, foundY);
        distance = foundY - y;
        for (s32 k = 1; found < 0 && k <= 3; k++)
        {
            const s32 top = camTop + kAoCamH * k;
            found = search(top + kAoTopY - 20, top + kAoBottomY + 20, foundY);
            distance = (camTop + kAoBottomY - y) + (kAoBottomY - kAoTopY) * (k - 1) + (foundY - (top + kAoTopY));
        }
    }
    if (floorY)
    {
        *floorY = foundY;
    }
    if (fallDistance)
    {
        *fallDistance = distance;
    }
    return found;
}

s32 NavGraph::FloorAbove(s32 x, s32 y, bool half, s32 minRise, s32* floorY) const
{
    // The lowest floor above y - minRise, no higher than minY
    const auto search = [&](s32 belowY, s32 minY, s32& outY)
    {
        s32 best = -1;
        for (std::size_t i = 0; i < mInput.mLines.size(); i++)
        {
            const NavLine& line = mInput.mLines[i];
            if (!IsFloor(line, half) || !Covers(line, x))
            {
                continue;
            }
            const s32 ly = YOnLine(line, x);
            if (ly < belowY && ly >= minY && (best < 0 || ly > outY))
            {
                best = static_cast<s32>(i);
                outY = ly;
            }
        }
        return best;
    };

    s32 foundY = 0;
    s32 found = -1;
    if (mInput.mIsAe)
    {
        found = search(y - minRise, y - 400, foundY);
    }
    else
    {
        const s32 camTop = FloorDiv(y, kAoCamH) * kAoCamH;
        found = search(y - minRise, camTop + kAoTopY - 20, foundY);
        if (found < 0)
        {
            const s32 top = camTop - kAoCamH;
            found = search(top + kAoBottomY + 20, top + kAoTopY - 20, foundY);
        }
    }
    if (floorY)
    {
        *floorY = foundY;
    }
    return found;
}

s32 NavGraph::FloorNear(s32 x, s32 y, bool half, s32 tolerance, s32* floorY) const
{
    s32 best = -1;
    s32 bestY = 0;
    for (std::size_t i = 0; i < mInput.mLines.size(); i++)
    {
        const NavLine& line = mInput.mLines[i];
        if (!IsFloor(line, half) || !Covers(line, x))
        {
            continue;
        }
        const s32 ly = YOnLine(line, x);
        if (std::abs(ly - y) <= tolerance && (best < 0 || std::abs(ly - y) < std::abs(bestY - y)))
        {
            best = static_cast<s32>(i);
            bestY = ly;
        }
    }
    if (floorY)
    {
        *floorY = bestY;
    }
    return best;
}

bool NavGraph::CeilingOver(s32 x, s32 y, bool half) const
{
    // What the games look for before letting Abe stand up: a ceiling from 5 to 45 above his feet
    const s32 top = y - (half ? 23 : 45);
    for (const NavLine& line : mInput.mLines)
    {
        if (line.mType != (half ? kBgCeiling : kCeiling) || line.mX1 == line.mX2 || x < std::min(line.mX1, line.mX2) || x > std::max(line.mX1, line.mX2))
        {
            continue;
        }
        const s32 ly = line.mY1 + (line.mY2 - line.mY1) * (x - line.mX1) / (line.mX2 - line.mX1);
        if (ly >= top && ly <= y - 5)
        {
            return true;
        }
    }
    return false;
}

bool NavGraph::HoistTopAt(const NavNode& n) const
{
    for (const NavFeature& f : mInput.mFeatures)
    {
        if (f.mKind == FeatureKind::eHoist && (f.mAnyScale || f.mHalf == n.mHalf) && n.mX >= f.mX1 - 2 && n.mX <= f.mX2 + 2 && std::abs(f.mY1 - n.mY) <= 12)
        {
            return true;
        }
    }
    return false;
}

bool NavGraph::WallBetween(s32 xa, s32 xb, s32 y, bool half, s32 height) const
{
    const s32 lo = std::min(xa, xb);
    const s32 hi = std::max(xa, xb);
    const s32 heights[2] = {height ? height : (half ? 10 : 20), height ? height : (half ? 25 : 50)};
    for (const NavLine& line : mInput.mLines)
    {
        const bool wall = half ? (line.mType == kBgWallLeft || line.mType == kBgWallRight) : (line.mType == kWallLeft || line.mType == kWallRight);
        if (!wall)
        {
            continue;
        }
        const s32 top = std::min(line.mY1, line.mY2);
        const s32 bottom = std::max(line.mY1, line.mY2);
        for (s32 h : heights)
        {
            const s32 rayY = y - h;
            if (rayY < top || rayY > bottom)
            {
                continue;
            }
            // Where the wall is at that height
            s32 wx = line.mX1;
            if (line.mY1 != line.mY2)
            {
                wx = line.mX1 + (line.mX2 - line.mX1) * (rayY - line.mY1) / (line.mY2 - line.mY1);
            }
            if (wx >= lo && wx <= hi)
            {
                return true;
            }
        }
    }
    return false;
}

s32 NavGraph::BarrierBetween(s32 xa, s32 xb, s32 y, bool half) const
{
    const s32 lo = std::min(xa, xb);
    const s32 hi = std::max(xa, xb);
    for (std::size_t i = 0; i < mInput.mFeatures.size(); i++)
    {
        const NavFeature& f = mInput.mFeatures[i];
        if (f.mKind != FeatureKind::eBarrier || (!f.mAnyScale && f.mHalf != half))
        {
            continue;
        }
        // It stands on the floor it blocks, somewhere in its cell
        const s32 fx = (f.mX1 + f.mX2) / 2;
        if (fx >= lo && fx <= hi && y >= f.mY1 - 30 && y <= f.mY2 + 30)
        {
            return static_cast<s32>(i);
        }
    }
    return -1;
}

bool NavGraph::Condition(const NavEdge& edge, s32* switchId, s32* openValue) const
{
    if (edge.mWhen >= 0 && edge.mWell >= 0)
    {
        // A well that goes here only while its switch is one way
        if (switchId)
        {
            *switchId = mInput.mFeatures[edge.mWell].mSwitch;
        }
        if (openValue)
        {
            *openValue = edge.mWhen;
        }
        return true;
    }
    if (edge.mClear >= 0)
    {
        // Into a cell with a mine: open once it is gone, which the world gives as a switch of its own
        if (switchId)
        {
            *switchId = mInput.mFeatures[edge.mClear].mSwitch;
        }
        if (openValue)
        {
            *openValue = 1;
        }
        return true;
    }
    // Onto a lift's platform at a stop the lift doesn't start at and a lift mover brings it to: once that
    // mover's switch is on
    const s32 stop = mNodes[edge.mTo].mLiftStop;
    if (stop >= 0 && mNodes[edge.mFrom].mLiftStop < 0 && mInput.mFeatures[stop].mSwitch > 1 && !mInput.mFeatures[stop].mA && (edge.mFeature < 0 || mInput.mFeatures[edge.mFeature].mKind != FeatureKind::eBarrier))
    {
        if (switchId)
        {
            *switchId = mInput.mFeatures[stop].mSwitch;
        }
        if (openValue)
        {
            *openValue = 1;
        }
        return true;
    }
    if (edge.mFeature < 0)
    {
        return false;
    }
    const NavFeature& f = mInput.mFeatures[edge.mFeature];
    s32 value = 0;
    if (f.mKind == FeatureKind::eBarrier)
    {
        // Shut to start with: open once its switch is on. Else open until it is.
        value = f.mStartClosed ? 1 : 0;
        // A plate on the floor just before it, on the side he comes from, that opens it as he steps on it: no
        // condition from that side (the Mudanchee trial: "as you run over the pressure plates, you'll open then
        // close the gate behind you")
        const NavNode& from = mNodes[edge.mFrom];
        const NavNode& to = mNodes[edge.mTo];
        const s32 gx = (f.mX1 + f.mX2) / 2;
        const s32 cell = GridStep(from.mHalf);
        if (f.mSolid && from.mHalf == to.mHalf && edge.mKind == EdgeKind::eWalk)
        {
            for (const NavFeature& plate : mInput.mFeatures)
            {
                const s32 px = (plate.mX1 + plate.mX2) / 2;
                if (plate.mKind == FeatureKind::ePlate && plate.mSwitch == f.mSwitch && plate.mHalf == from.mHalf && plate.mA == (value == 1 ? 0 : 1) && std::abs(plate.mY2 - from.mY) <= 30 &&
                    (px < from.mX) == (to.mX > from.mX) && px != from.mX && std::abs(px - gx) <= 5 * cell && std::abs(px - gx) >= cell)
                {
                    return false;
                }
            }
        }
    }
    else if (f.mKind == FeatureKind::eDoor && edge.mKind == EdgeKind::eDoor && f.mStartClosed && f.mSwitch > 1)
    {
        value = 1;
    }
    else if (f.mKind == FeatureKind::ePortal && edge.mKind == EdgeKind::ePortal && f.mSwitch > 1)
    {
        // Not there until its switch is on (a hand stone's, say)
        value = 1;
    }
    else if (f.mKind == FeatureKind::eTrapDoor)
    {
        // Fallen through open, walked on shut
        const s32 open = f.mStartClosed ? 1 : 0;
        value = edge.mKind == EdgeKind::eFall ? open : 1 - open;
    }
    else
    {
        return false;
    }
    if (switchId)
    {
        *switchId = f.mSwitch;
    }
    if (openValue)
    {
        *openValue = value;
    }
    return true;
}

std::vector<s32> NavGraph::Regions(s32* count) const
{
    // Union the ends of every edge that has no barrier and has a way back
    std::vector<s32> parent(mNodes.size());
    for (std::size_t i = 0; i < parent.size(); i++)
    {
        parent[i] = static_cast<s32>(i);
    }
    const auto find = [&](s32 a)
    {
        while (parent[a] != a)
        {
            parent[a] = parent[parent[a]];
            a = parent[a];
        }
        return a;
    };
    const auto isBarrier = [&](const NavEdge& e)
    {
        return Condition(e, nullptr, nullptr);
    };
    for (const NavEdge& e : mEdges)
    {
        if (isBarrier(e))
        {
            continue;
        }
        const NavNode& to = mNodes[e.mTo];
        for (s32 b = to.mFirstEdge; b < to.mFirstEdge + to.mEdgeCount; b++)
        {
            if (mEdges[b].mTo == e.mFrom && !isBarrier(mEdges[b]))
            {
                parent[find(e.mFrom)] = find(e.mTo);
                break;
            }
        }
    }
    std::vector<s32> region(mNodes.size(), -1);
    std::vector<s32> number(mNodes.size(), -1);
    s32 next = 0;
    for (std::size_t i = 0; i < mNodes.size(); i++)
    {
        const s32 root = find(static_cast<s32>(i));
        if (number[root] < 0)
        {
            number[root] = next++;
        }
        region[i] = number[root];
    }
    if (count)
    {
        *count = next;
    }
    return region;
}

const NavFeature* NavGraph::FeatureAt(FeatureKind kind, s32 x, s32 y, bool half, s32* index) const
{
    for (std::size_t i = 0; i < mInput.mFeatures.size(); i++)
    {
        const NavFeature& f = mInput.mFeatures[i];
        if (f.mKind == kind && (f.mAnyScale || f.mHalf == half) && f.Contains(x, y))
        {
            if (index)
            {
                *index = static_cast<s32>(i);
            }
            return &f;
        }
    }
    return nullptr;
}

s32 NavGraph::NodeOnLine(s32 line, s32 x) const
{
    if (line < 0 || mLineCount[line] == 0)
    {
        return -1;
    }
    s32 best = mLineFirst[line];
    for (s32 i = mLineFirst[line]; i < mLineFirst[line] + mLineCount[line]; i++)
    {
        if (std::abs(mNodes[i].mX - x) < std::abs(mNodes[best].mX - x))
        {
            best = i;
        }
    }
    return best;
}

s32 NavGraph::NodeAt(s32 x, s32 y, bool half, s32 yTolerance) const
{
    s32 best = -1;
    const s32 reach = GridStep(half) / 2 + 1;
    for (std::size_t i = 0; i < mNodes.size(); i++)
    {
        const NavNode& n = mNodes[i];
        if (n.mHalf != half || std::abs(n.mX - x) > reach || std::abs(n.mY - y) > yTolerance)
        {
            continue;
        }
        if (best < 0 || std::abs(n.mY - y) * 100 + std::abs(n.mX - x) < std::abs(mNodes[best].mY - y) * 100 + std::abs(mNodes[best].mX - x))
        {
            best = static_cast<s32>(i);
        }
    }
    return best;
}

s32 NavGraph::CameraOf(s32 x, s32 y) const
{
    return (x / mInput.mCamW) + (y / mInput.mCamH) * 256;
}

s32 NavGraph::Thrown(s32 x, s32 y, s32 fromX, bool half, s32 depth, s32 knownSwitch, s32 knownValue) const
{
    // Which way a well further along goes: by the switch the chain was started with if it is the same one
    const auto whenOf = [&](const NavFeature& f)
    {
        return f.mSwitch == 1 ? 1 : (f.mSwitch > 1 && f.mSwitch == knownSwitch ? knownValue : 0);
    };
    // Into another well: the game puts him in the one whose box he is in as he falls
    for (std::size_t i = 0; depth < 8 && i < mInput.mFeatures.size(); i++)
    {
        const NavFeature& f = mInput.mFeatures[i];
        if ((f.mKind == FeatureKind::eWellLocal || f.mKind == FeatureKind::eWellExpress) && f.mHalf == half && x >= f.mX1 && x <= f.mX2 && y >= f.mY1 - 4 && y <= f.mY2 + 4 && (fromX < f.mX1 || fromX > f.mX2))
        {
            return WellOut(static_cast<s32>(i), whenOf(f), depth + 1, knownSwitch, knownValue);
        }
    }
    const s32 step = GridStep(half);
    if (x == 0 && y == 0)
    {
        return -1;
    }
    s32 fy = 0;
    const s32 under = FloorBelow(x, y - 40, half, &fy);
    // (thrown to a point in the air over a well, he comes down into it: the Mudanchee vault's path 3, from the
    // well at 512,720 to the one at 589,460)
    if (depth < 8)
    {
        // (he flies on a little the way he was thrown as he comes down: a well just past the point is fallen into
        // as well. Measured in the second Mudomo vault's background, thrown to 869,334 and down into the well at
        // 851..863, 360.)
        s32 below = -1;
        for (s32 k = 0; k <= 2 && below < 0; k++)
        {
            below = WellInFall(x + (x < fromX ? -k : k) * step / 2, y - 60, under >= 0 ? fy + 30 : y + 400, half);
        }
        if (below >= 0 && (fromX < mInput.mFeatures[below].mX1 || fromX > mInput.mFeatures[below].mX2))
        {
            return WellOut(below, whenOf(mInput.mFeatures[below]), depth + 1, knownSwitch, knownValue);
        }
    }
    if (under >= 0 && fy - y < 200)
    {
        // (a point well over the floor: he flies on the way he was thrown as he comes down, a cell and a half.
        // Measured in the Mudomo trial: thrown to 4150,880 from the right he came down at 4112,980, past the gate
        // that stands at 4150.)
        if ((fy - y) * (half ? 2 : 1) >= 60)
        {
            const s32 nx = x + (x < fromX ? -1 : 1) * 3 * step / 2;
            s32 ny = 0;
            const s32 on = FloorBelow(nx, y - 40, half, &ny);
            if (on >= 0 && std::abs(ny - fy) <= 12)
            {
                return NodeOnLine(on, nx);
            }
        }
        return NodeOnLine(under, x);
    }
    // Nothing under it: he flies on the way he was thrown and drops onto, or catches the edge of, what is next
    const s32 dir = x < fromX ? -1 : 1;
    for (s32 k = 1; k <= 4; k++)
    {
        const s32 nx = x + dir * k * step;
        const s32 line = FloorBelow(nx, y - 40, half, &fy);
        // (as far as the fall gives him time to drift: some 3 a frame. Measured in the Mudomo trial: thrown to
        // 2821,600 with no floor under it, he came down at 2787,880, not on the ledge three cells on at 620.)
        s32 frames = 0;
        for (s32 fallen = 0; fallen < std::max(0, fy - y) * (half ? 2 : 1); frames++)
        {
            fallen += std::min(20, 2 * (frames + 1));
        }
        if (line >= 0 && k * step * (half ? 2 : 1) <= 2 * frames + 25)
        {
            return NodeOnLine(line, nx);
        }
    }
    return under >= 0 ? NodeOnLine(under, x) : -1;
}

s32 NavGraph::WellOut(s32 featureIdx, s32 when, s32 depth, s32 knownSwitch, s32 knownValue) const
{
    const NavFeature& f = mInput.mFeatures[featureIdx];
    if (depth == 0 && f.mSwitch > 1)
    {
        knownSwitch = f.mSwitch;
        knownValue = when;
    }
    const s32 centre = (f.mX1 + f.mX2) / 2;
    // Shot out of a well that names no place he pops up two cells to its right, and a hoist that reaches down into
    // that rise catches him: he ends hung from the floor at its top (the Mudomo vault's path 12: the well at
    // 437,1500 is the only way up to the lock's ledge at 487,1380, whose hoist ends 40 over the floor below)
    const auto caught = [&](const NavFeature& well, s32 from) -> s32
    {
        const s32 px = from + 2 * GridStep(well.mHalf);
        const s32 scale = well.mHalf ? 2 : 1;
        for (const NavFeature& h : mInput.mFeatures)
        {
            if (h.mKind == FeatureKind::eHoist && h.mHalf == well.mHalf && px >= h.mX1 && px <= h.mX2 && h.mY1 < well.mY1 && h.mY2 >= well.mY1 - 100 / scale && h.mY2 < well.mY2 - 10 / scale)
            {
                s32 hy = 0;
                const s32 line = FloorNear(px, h.mY1, well.mHalf, 12, &hy);
                if (line >= 0)
                {
                    return NodeOnLine(line, px);
                }
            }
        }
        return -1;
    };
    if (f.mKind == FeatureKind::eWellLocal)
    {
        const s32 x = when == 1 ? f.mOnC : f.mC;
        const s32 y = when == 1 ? f.mOnD : f.mD;
        if (x == 0 && y == 0)
        {
            const s32 hung = caught(f, centre);
            if (hung >= 0)
            {
                return hung;
            }
            // Nowhere to throw him: he pops out and comes down two cells to its right (Necrum's path 3, the
            // well at 937,980)
            s32 fy = 0;
            const s32 line = FloorBelow(centre + 2 * GridStep(f.mHalf), f.mY1 - 20, f.mHalf, &fy);
            return line >= 0 ? NodeOnLine(line, centre + 2 * GridStep(f.mHalf)) : -1;
        }
        return Thrown(x, y, centre, f.mHalf, depth, knownSwitch, knownValue);
    }
    // An express well: he is put in the well it names in the camera it names, and shot out of that one
    if ((when == 1 ? f.mOnDestPath : f.mDestPath) >= 0)
    {
        return -1;
    }
    const s32 id = when == 1 ? f.mOnA : f.mA;
    const s32 camera = when == 1 ? f.mOnDestCamera : f.mDestCamera;
    for (std::size_t i = 0; i < mInput.mFeatures.size(); i++)
    {
        const NavFeature& other = mInput.mFeatures[i];
        if ((other.mKind != FeatureKind::eWellExpress && other.mKind != FeatureKind::eWellLocal) || other.mB != id || other.mCamera != camera)
        {
            continue;
        }
        const s32 from = (other.mX1 + other.mX2) / 2;
        if (other.mKind == FeatureKind::eWellLocal)
        {
            if (static_cast<s32>(i) == featureIdx || depth >= 8)
            {
                return -1;
            }
            return WellOut(static_cast<s32>(i), other.mSwitch == 1 ? 1 : (other.mSwitch > 1 && other.mSwitch == knownSwitch ? knownValue : 0), depth + 1, knownSwitch, knownValue);
        }
        if (other.mC == 0 && other.mD == 0)
        {
            const s32 hung = caught(other, from);
            if (hung >= 0)
            {
                return hung;
            }
            s32 fy = 0;
            const s32 line = FloorBelow(from, other.mY1 - 60, other.mHalf, &fy);
            return line >= 0 ? NodeOnLine(line, from) : -1;
        }
        return Thrown(other.mC, other.mD, from, other.mHalf, depth, knownSwitch, knownValue);
    }
    return -1;
}

void NavGraph::AddWellEdges(std::vector<NavEdge>& edges, s32 from, s32 wellIdx, EdgeKind kind, s32 cost, s32 feature, bool left) const
{
    const NavFeature& well = mInput.mFeatures[wellIdx];
    const bool express = well.mKind == FeatureKind::eWellExpress;
    const s32 off = well.mSwitch == 1 ? -1 : WellOut(wellIdx, 0);
    const s32 on = well.mSwitch <= 0 ? -1 : WellOut(wellIdx, 1);
    const bool offOut = express && well.mSwitch != 1 && well.mDestPath >= 0;
    const bool onOut = express && well.mSwitch > 0 && well.mOnDestPath >= 0;
    const auto add = [&](s32 to, s32 when)
    {
        if (to >= 0 && to != from)
        {
            AddEdge(edges, from, to, kind, cost, feature, left);
            edges.back().mWell = wellIdx;
            edges.back().mWhen = when;
        }
    };
    // (a well whose switch is set by standing where it is jumped into from: it leads where that sets it to. The
    // second Mudomo vault's background wells, each in a box that turns it.)
    s32 zone = -1;
    for (const NavFeature& z : mInput.mFeatures)
    {
        const NavNode& at = mNodes[from];
        if (z.mKind == FeatureKind::eSwitchZone && z.mSwitch == well.mSwitch && well.mSwitch > 1 && z.mA <= 1 && (z.mAnyScale || z.mHalf == at.mHalf) && at.mX >= z.mX1 && at.mX <= z.mX2 && at.mY - 10 >= z.mY1 && at.mY - 10 <= z.mY2)
        {
            zone = z.mA == 0 ? 1 : 0;
        }
    }
    if (zone >= 0 && kind == EdgeKind::eWell)
    {
        add(zone == 1 ? on : off, -1);
    }
    else if (well.mSwitch <= 1 || (off == on && !offOut && !onOut))
    {
        add(well.mSwitch == 1 ? on : off, -1);
    }
    else
    {
        add(off, 0);
        add(on, 1);
    }
}

s32 NavGraph::WellInFall(s32 x, s32 fromY, s32 toY, bool half) const
{
    // (the game asks whether the point he is at is in the well's box on each frame of the fall)
    s32 best = -1;
    for (std::size_t i = 0; i < mInput.mFeatures.size(); i++)
    {
        const NavFeature& f = mInput.mFeatures[i];
        // (one on the floor he lands on is the other rule's: there he may drift into it)
        if ((f.mKind == FeatureKind::eWellLocal || f.mKind == FeatureKind::eWellExpress) && f.mHalf == half && x >= f.mX1 && x <= f.mX2 && f.mY1 > fromY + 30 && f.mY2 < toY - 10 && (best < 0 || f.mY1 < mInput.mFeatures[best].mY1))
        {
            best = static_cast<s32>(i);
        }
    }
    return best;
}

s32 NavGraph::WellLanding(const NavFeature& well) const
{
    // (where it throws him, by the same rule as a well he jumps into: Thrown)
    return Thrown(well.mC, well.mD, (well.mX1 + well.mX2) / 2, well.mHalf, 0);
}

void NavGraph::AddEdge(std::vector<NavEdge>& edges, s32 from, s32 to, EdgeKind kind, s32 cost, s32 feature, bool left) const
{
    if (to < 0 || to == from)
    {
        return;
    }
    NavEdge e;
    e.mFrom = from;
    e.mTo = to;
    e.mKind = kind;
    e.mCost = cost;
    e.mFeature = feature;
    e.mLeft = left;
    edges.push_back(e);
}

void NavGraph::AddRunJumps(std::vector<NavEdge>& edges, s32 from, s32 dir, bool overLift) const
{
    const NavNode& n = mNodes[from];
    const s32 step = GridStep(n.mHalf);
    // Room to get up to speed: the cells to the take-off are plain floor
    s32 x = n.mX;
    for (s32 k = 0; k < kRunUpCells; k++)
    {
        const s32 nx = StepX(x, dir, n.mHalf);
        const s32 node = NodeOnLine(n.mLine, nx);
        if (node < 0 || std::abs(mNodes[node].mX - nx) > step / 2 || mNodes[node].mHazard || mNodes[node].mLow || mNodes[node].mLiftStop >= 0 || WallBetween(x, x + dir * step, n.mY, n.mHalf))
        {
            return;
        }
        x = nx;
    }
    const s32 takeoff = x;
    const s32 y = mNodes[NodeOnLine(n.mLine, takeoff)].mY;

    // Up a hoist without stopping: a running jump that passes through a hoist he faces catches the ledge over
    // it (Abe's run jump looks for a hoist where he is). Dear, so that it is only taken when the bot makes it
    // cheap: with something after him (see Bot::EdgeCostFn).
    for (s32 k = 3; k >= 2 && !overLift; k--)
    {
        const s32 hx = takeoff + dir * k * step;
        s32 hoistIdx = -1;
        const NavFeature* hoist = FeatureAt(FeatureKind::eHoist, hx, y - 10, n.mHalf, &hoistIdx);
        if (!hoist || (hoist->mDir != 2 && (hoist->mDir == 0) != (dir < 0)) || WallBetween(takeoff, hx, y, n.mHalf) || BarrierBetween(n.mX, hx, y, n.mHalf) >= 0)
        {
            continue;
        }
        const s32 top = NodeAt(hx, hoist->mY1, n.mHalf, 12);
        if (top >= 0 && (y - mNodes[top].mY) * (n.mHalf ? 2 : 1) <= 130)
        {
            AddEdge(edges, from, top, EdgeKind::eRunJump, 400, hoistIdx, dir < 0);
            edges.back().mX = takeoff;
            // (and from further back along the floor, so that a run that is under way need not stop for it)
            s32 bx = n.mX;
            for (s32 back = 1; back <= 14; back++)
            {
                const s32 px = StepX(bx, -dir, n.mHalf);
                const s32 behind = NodeOnLine(n.mLine, px);
                // (not from the far side of a gate)
                if (behind < 0 || std::abs(mNodes[behind].mX - px) > step / 2 || mNodes[behind].mHazard || mNodes[behind].mLow || WallBetween(bx, px, n.mY, n.mHalf) || BarrierBetween(bx, px, n.mY, n.mHalf) >= 0)
                {
                    break;
                }
                AddEdge(edges, behind, top, EdgeKind::eRunJump, 400 + back, hoistIdx, dir < 0);
                edges.back().mX = takeoff;
                bx = px;
            }
            break;
        }
    }

    // What is under the flight: a floor to walk on means no jump is needed that far
    bool gap = false;
    s32 shaft = -1;
    for (s32 k = 1; k <= kRunJumpLedgeCells; k++)
    {
        const s32 nx = StepX(x, dir, n.mHalf);
        if (WallBetween(x, x + dir * step, y, n.mHalf) || CeilingOver(nx, y, n.mHalf))
        {
            return;
        }
        x = nx;
        s32 fy = 0;
        const s32 line = FloorNear(x, y, n.mHalf, 12, &fy);
        const s32 node = line >= 0 ? NodeOnLine(line, x) : -1;
        bool floor = node >= 0 && std::abs(mNodes[node].mX - x) <= step / 2;
        if (overLift && floor && mNodes[node].mLiftStop >= 0)
        {
            shaft = mNodes[node].mLiftStop;
            floor = false;
        }
        if (k >= kRunJumpCells - 1 && gap && !overLift)
        {
            // A ledge to catch
            for (std::size_t i = 0; i < mInput.mFeatures.size(); i++)
            {
                const NavFeature& f = mInput.mFeatures[i];
                if (f.mKind != FeatureKind::eEdge || !f.mA || (!f.mAnyScale && f.mHalf != n.mHalf) || (f.mDir != 2 && (f.mDir == 0) != (dir < 0)))
                {
                    continue;
                }
                if (x >= f.mX1 - 2 && x <= f.mX2 + 2 && f.mY1 >= y - 25 && f.mY1 <= y + 40)
                {
                    const s32 to = NodeAt(x, f.mY1, n.mHalf, 12);
                    if (to >= 0 && k == kRunJumpLedgeCells)
                    {
                        AddEdge(edges, from, to, EdgeKind::eRunJump, kRunJumpCost, static_cast<s32>(i), dir < 0);
                        edges.back().mX = takeoff;
                        return;
                    }
                }
            }
        }
        if (k == kRunJumpCells && gap && floor && !mNodes[node].mHazard)
        {
            // Lands and needs a cell or two to stop in
            const s32 after = NodeOnLine(mNodes[node].mLine, StepX(x, dir, n.mHalf));
            // (measured: he slides 50 on from where he comes down)
            const s32 second = after >= 0 ? NodeOnLine(mNodes[node].mLine, StepX(mNodes[after].mX, dir, n.mHalf)) : -1;
            const bool mined = second >= 0 && second != after && mNodes[second].mHazard;
            if (after >= 0 && after != node && !mNodes[after].mHazard && !mined && (!overLift || shaft >= 0))
            {
                AddEdge(edges, from, node, EdgeKind::eRunJump, kRunJumpCost, -1, dir < 0);
                edges.back().mShaft = overLift ? shaft : -1;
                edges.back().mX = takeoff;
            }
            return;
        }
        if (!floor || mNodes[node].mHazard)
        {
            gap = true;
        }
        else if (!gap && k >= 1)
        {
            // Floor right after the take-off: walk it
            return;
        }
    }
}

void NavGraph::AddLedgeCatches(std::vector<NavEdge>& edges, s32 from, s32 dir) const
{
    // Measured in the Mudanchee trial's last room: from a stand at 37,100 on a ledge that ends at 62, turned, one
    // cell run and off at 62, he caught the hoist at 187,100 five cells on (the run jump goes as far whatever the
    // run before it was). The two ledges there are the walkthrough's way to keep clear of the Fleeches: "jump
    // across to the far ledge, pull up".
    const NavNode& n = mNodes[from];
    const s32 step = GridStep(n.mHalf);
    const s32 takeoff = StepX(n.mX, dir, n.mHalf);
    const s32 off = NodeOnLine(n.mLine, takeoff);
    if (off < 0 || std::abs(mNodes[off].mX - takeoff) > step / 2 || mNodes[off].mHazard || mNodes[off].mLow || n.mLow || WallBetween(n.mX, takeoff, n.mY, n.mHalf))
    {
        return;
    }
    const s32 y = mNodes[off].mY;
    // Up to a ledge: off the last cell of a floor, the jump catches an edge (or a hoist) that faces him two and
    // a half to three and a half cells on and up to 100 higher (measured in the Mudomo trial: off 2462,720 to
    // hang from 2537,620, the walkthrough's "immediately jump to the ledge above")
    {
        s32 ny = 0;
        const s32 beyond = FloorNear(takeoff + dir * step, y, n.mHalf, 12, &ny);
        const s32 scale = n.mHalf ? 2 : 1;
        for (std::size_t fi = 0; beyond < 0 && fi < mInput.mFeatures.size(); fi++)
        {
            const NavFeature& f = mInput.mFeatures[fi];
            if ((f.mKind != FeatureKind::eEdge && f.mKind != FeatureKind::eHoist) || f.mHalf != n.mHalf || !(f.mDir == 2 || (f.mDir == 0) == (dir < 0)))
            {
                continue;
            }
            const s32 rise = y - f.mY1;
            const s32 near = dir > 0 ? f.mX1 - takeoff : takeoff - f.mX2;
            if (rise * scale < 30 || rise * scale > 110 || near < 2 * step || near > 3 * step + step / 2 || WallBetween(takeoff, dir > 0 ? f.mX1 : f.mX2, y - rise, n.mHalf))
            {
                continue;
            }
            s32 ly = 0;
            const s32 lx = (f.mX1 + f.mX2) / 2;
            const s32 line = FloorNear(lx, f.mY1, n.mHalf, 12, &ly);
            const s32 ledge = line >= 0 ? NodeOnLine(line, lx) : -1;
            if (ledge >= 0 && std::abs(mNodes[ledge].mX - lx) <= step && !mNodes[ledge].mHazard)
            {
                AddEdge(edges, from, ledge, EdgeKind::eRunJump, kRunJumpCost + 10, static_cast<s32>(fi), dir < 0);
                edges.back().mX = takeoff;
            }
        }
    }
    s32 x = takeoff;
    for (s32 k = 1; k <= kRunJumpLedgeCells; k++)
    {
        const s32 nx = StepX(x, dir, n.mHalf);
        if (WallBetween(x, x + dir * step, y, n.mHalf) || CeilingOver(nx, y, n.mHalf))
        {
            return;
        }
        x = nx;
        s32 fy = 0;
        const s32 line = FloorNear(x, y, n.mHalf, 12, &fy);
        const s32 node = line >= 0 ? NodeOnLine(line, x) : -1;
        const bool floor = node >= 0 && std::abs(mNodes[node].mX - x) <= step / 2;
        if (k < kRunJumpLedgeCells)
        {
            // (nothing under the flight)
            if (floor)
            {
                return;
            }
            continue;
        }
        s32 hoistIdx = -1;
        const NavFeature* hoist = FeatureAt(FeatureKind::eHoist, x, y + 16, n.mHalf, &hoistIdx);
        if (floor && !mNodes[node].mHazard && hoist && (hoist->mDir == 2 || (hoist->mDir == 0) == (dir < 0)) && std::abs(hoist->mY1 - y) <= 12)
        {
            AddEdge(edges, from, node, EdgeKind::eRunJump, kRunJumpCost, hoistIdx, dir < 0);
            edges.back().mX = takeoff;
        }
    }
}

void NavGraph::Build(const NavInput& input)
{
    mInput = input;
    mNodes.clear();
    mEdges.clear();
    mExits.clear();
    // A lift's platform is a floor wherever it stops
    const std::size_t realLines = mInput.mLines.size();
    std::vector<s32> stopOfLine;
    for (std::size_t i = 0; i < mInput.mFeatures.size(); i++)
    {
        const NavFeature& f = mInput.mFeatures[i];
        if (f.mKind == FeatureKind::eLiftStop)
        {
            NavLine line;
            line.mX1 = f.mX1 + 4;
            line.mX2 = f.mX2 - 4;
            line.mY1 = f.mY1;
            line.mY2 = f.mY1;
            line.mType = f.mHalf ? kBgFloor : kFloor;
            mInput.mLines.push_back(line);
            stopOfLine.push_back(static_cast<s32>(i));
        }
        else if (f.mKind == FeatureKind::eTrapDoor)
        {
            NavLine line;
            line.mX1 = f.mX1;
            line.mX2 = f.mX2;
            line.mY1 = f.mY1;
            line.mY2 = f.mY1;
            line.mType = f.mHalf ? kBgFloor : kFloor;
            mInput.mLines.push_back(line);
            stopOfLine.push_back(static_cast<s32>(i));
        }
    }
    mLineFirst.assign(mInput.mLines.size(), 0);
    mLineCount.assign(mInput.mLines.size(), 0);

    // The cells of every floor
    for (std::size_t i = 0; i < mInput.mLines.size(); i++)
    {
        const NavLine& line = mInput.mLines[i];
        const bool half = line.mType == kBgFloor;
        mLineFirst[i] = static_cast<s32>(mNodes.size());
        if (!IsFloor(line, half))
        {
            continue;
        }
        const s32 lo = std::min(line.mX1, line.mX2) - kLineEndSlack;
        const s32 hi = std::max(line.mX1, line.mX2) + kLineEndSlack;
        for (s32 x = SnapUp(lo, half); x <= hi; x = SnapUp(x + 1, half))
        {
            if (!mInput.mIsAe && (Mod(x, kAoCamW) < (half ? kAoFirstCellXHalf : kAoFirstCellX) || Mod(x, kAoCamW) > (half ? kAoLastCellXHalf : kAoLastCellX)))
            {
                continue;
            }
            NavNode n;
            n.mX = x;
            n.mY = YOnLine(line, x);
            n.mLine = static_cast<s32>(i);
            n.mHalf = half;
            for (const NavFeature& f : mInput.mFeatures)
            {
                if (f.mKind == FeatureKind::eHazard && f.mHalf == half && x >= f.mX1 - 6 && x <= f.mX2 + 6 && n.mY >= f.mY1 - 10 && n.mY <= f.mY2 + 10)
                {
                    n.mHazard = true;
                    if (f.mSwitch > 0)
                    {
                        n.mClear = static_cast<s32>(&f - mInput.mFeatures.data());
                    }
                }
            }
            n.mLow = CeilingOver(x, n.mY, half);
            for (const NavFeature& f : mInput.mFeatures)
            {
                // (it kills what its rectangle touches with its middle no more than 3 from it: its width is not
                // in the path's data, half a cell either side covers what was measured)
                const s32 tall = half ? 30 : 60;
                // (one that comes down kills only who is within 3 of it)
                const s32 reach = f.mX2 - f.mX1 > f.mY2 - f.mY1 ? kSweepReach : 12;
                if (f.mKind == FeatureKind::eSweep && f.mHalf == half && x >= f.mX1 - reach && x <= f.mX2 + reach && n.mY - tall <= f.mY2 && n.mY >= f.mY1)
                {
                    // (measured: its box ends 6 above its rectangle's bottom, and Abe crouched is 34 high)
                    if (f.mX2 - f.mX1 > f.mY2 - f.mY1 && f.mY2 <= n.mY - tall / 2)
                    {
                        n.mSweepHigh = static_cast<s32>(&f - mInput.mFeatures.data());
                        n.mLow = true;
                    }
                    else
                    {
                        n.mSweep = static_cast<s32>(&f - mInput.mFeatures.data());
                    }
                }
            }
            for (const NavFeature& f : mInput.mFeatures)
            {
                // All of him has to be in it, and he is wider than he looks: with 12 either side he was shot in
                // the edge cells of the Mines' shadows
                if (f.mKind == FeatureKind::eInvisibleZone && x - (half ? 13 : 25) >= f.mX1 && x + (half ? 13 : 25) <= f.mX2 && n.mY >= f.mY1 + 40 && n.mY <= f.mY2 + 4)
                {
                    n.mHidden = true;
                }
            }
            const s32 madeBy = i >= realLines ? stopOfLine[i - realLines] : -1;
            const bool trap = madeBy >= 0 && mInput.mFeatures[madeBy].mKind == FeatureKind::eTrapDoor;
            n.mLiftStop = trap ? -1 : madeBy;
            n.mTrapDoor = trap ? madeBy : -1;
            mNodes.push_back(n);
        }
        mLineCount[i] = static_cast<s32>(mNodes.size()) - mLineFirst[i];
    }

    std::vector<NavEdge> edges;
    for (std::size_t i = 0; i < mNodes.size(); i++)
    {
        const NavNode& n = mNodes[i];
        const s32 from = static_cast<s32>(i);
        const s32 step = GridStep(n.mHalf);
        // (a cell with a mine in it is floor like any other once the mine is blown up: its edges are made, and
        // marked below as waiting for that)
        if (n.mHazard && n.mClear < 0)
        {
            continue;
        }

        for (s32 dir = -1; dir <= 1; dir += 2)
        {
            const bool left = dir < 0;
            const s32 nx = StepX(n.mX, dir, n.mHalf);
            // A wall that stops short of the floor lets a roll through
            if (WallBetween(n.mX, n.mX + dir * step, n.mY, n.mHalf, n.mHalf ? 10 : 20))
            {
                continue;
            }
            const bool standingWall = WallBetween(n.mX, n.mX + dir * step, n.mY, n.mHalf, n.mHalf ? 25 : 50);

            s32 fy = 0;
            const s32 nearLine = FloorNear(nx, n.mY, n.mHalf, step / 2 + 2, &fy);
            if (nearLine >= 0)
            {
                const s32 to = NodeOnLine(nearLine, nx);
                if (to >= 0 && std::abs(mNodes[to].mX - nx) <= step / 2 && (!mNodes[to].mHazard || mNodes[to].mClear >= 0))
                {
                    const bool roll = standingWall || n.mLow || mNodes[to].mLow;
                    AddEdge(edges, from, to, roll ? EdgeKind::eRoll : EdgeKind::eWalk, roll ? kRollCost : kWalkCost, BarrierBetween(n.mX, n.mX + dir * step, n.mY, n.mHalf), left);
                }
            }
            else if (!(standingWall && !n.mLow && HoistTopAt(n)))
            {
                // (under a block that leaves room to roll, from a cell he can stand in: crouched there first. Not
                // from the top of a hoist: down there is the way down the hoist.)
                const bool crouched = standingWall && !n.mLow;
                // (from under a low ceiling it is rolled off)
                // Off the end of the floor
                s32 height = 0;
                const s32 belowLine = FloorBelow(nx, n.mY + 1, n.mHalf, &fy, &height);
                bool deadly = belowLine < 0;
                // Rolled off with a ledge to catch out and below, facing the way he goes: he ends hung from it (the
                // walkthrough of the Mudomo vault's path 3: "roll under the rocks at the edge of the screen, then
                // let yourself fall out the other side to land hanging beside the exit", off 1912,980 to the
                // ledge at 2000,1200). A hoist that faces the way he goes catches him the same (the Mudomo trial,
                // off 1687,460 into the hoist at 1765..1800 by the ledge at 620).
                if (n.mLow || crouched)
                {
                    const s32 scale = n.mHalf ? 2 : 1;
                    for (const NavFeature& f : mInput.mFeatures)
                    {
                        if ((f.mKind != FeatureKind::eEdge && f.mKind != FeatureKind::eHoist) || f.mHalf != n.mHalf || !(f.mDir == 2 || (f.mDir == 0) == (dir < 0)))
                        {
                            continue;
                        }
                        const s32 drop = f.mY1 - n.mY;
                        const s32 lo = std::min(nx + dir * step, nx + dir * 4 * step) - step / 2;
                        const s32 hi = std::max(nx + dir * step, nx + dir * 4 * step) + step / 2;
                        if (drop * scale < 60 || drop * scale > 300 || f.mX2 < lo || f.mX1 > hi || (belowLine >= 0 && fy <= f.mY1 + 10))
                        {
                            continue;
                        }
                        s32 ly = 0;
                        const s32 lx = (f.mX1 + f.mX2) / 2 + dir * step / 2;
                        const s32 ledgeLine = FloorNear(lx, f.mY1, n.mHalf, 12, &ly);
                        const s32 ledge = ledgeLine >= 0 ? NodeOnLine(ledgeLine, lx) : -1;
                        if (ledge >= 0 && std::abs(mNodes[ledge].mX - lx) <= step && !mNodes[ledge].mHazard)
                        {
                            AddEdge(edges, from, ledge, EdgeKind::eFall, 50 + drop / 4, -1, left);
                            edges.back().mCrouched = crouched;
                        }
                    }
                }
                // (rolled off over a gap he clears it, where a floor two cells further out is under him: measured
                // in the Mudanchee trial, off 587,360 to the right over the pit and down onto 662,460; and in the
                // Mudomo vaults' path 10, off 2037,600 to the left and 40 down onto 1962,640)
                if (belowLine < 0 && (n.mLow || crouched))
                {
                    s32 farY = 0;
                    s32 farHeight = 0;
                    const s32 farX = nx + dir * 2 * step;
                    const s32 farLine = FloorBelow(farX, n.mY + 1, n.mHalf, &farY, &farHeight);
                    const s32 out = farLine >= 0 ? NodeOnLine(farLine, farX) : -1;
                    if (out >= 0 && farHeight * (n.mHalf ? 2 : 1) >= 30 && farHeight * (n.mHalf ? 2 : 1) < 200 && std::abs(mNodes[out].mX - farX) <= step / 2 && !mNodes[out].mHazard &&
                        // (a wall that comes down to 20 over the floor is rolled under: measured there too)
                        !WallBetween(n.mX, farX, n.mY, n.mHalf, n.mHalf ? 9 : 19))
                    {
                        AddEdge(edges, from, out, EdgeKind::eFall, 20 + farHeight / 4 + (crouched ? 10 : 0), -1, left);
                        edges.back().mCrouched = crouched;
                    }
                }
                // A well on the floor he comes down on swallows him as he lands in it, however far he fell
                // (rolled off he drifts a few cells on the way down)
                bool intoWell = false;
                // And one he falls through in the air does, floor or no floor
                const s32 airWell = WellInFall(nx, n.mY, belowLine >= 0 ? fy : n.mY + 2000, n.mHalf);
                if (airWell >= 0)
                {
                    AddWellEdges(edges, from, airWell, EdgeKind::eFall, 60 + kWellCost, -1, left);
                    intoWell = true;
                }
                for (const NavFeature& f : mInput.mFeatures)
                {
                    if (deadly || intoWell || f.mKind != FeatureKind::eWellLocal || f.mHalf != n.mHalf || f.mY2 < n.mY + 40 || std::abs(f.mY2 - fy) > 10)
                    {
                        continue;
                    }
                    // (walked off a short drop he comes straight down: measured on the Mines' path 4, off 1712,460 to
                    // the left, 260 down, where he landed three cells short of the well and died of the fall. Down
                    // the 780 of path 6's shaft he did drift into the well.)
                    const s32 wx = (f.mX1 + f.mX2) / 2;
                    const s32 drift = n.mLow || height * (n.mHalf ? 2 : 1) >= 500 ? 3 * step : 0;
                    const s32 lo = std::min(nx, nx + dir * drift) - step / 2;
                    const s32 hi = std::max(nx, nx + dir * drift) + step / 2;
                    if (wx >= lo && wx <= hi)
                    {
                        AddEdge(edges, from, WellLanding(f), EdgeKind::eFall, 20 + height / 4 + kWellCost, -1, left);
                        intoWell = true;
                    }
                }
                if (intoWell)
                {
                    deadly = true;
                }
                if (!deadly)
                {
                    for (const NavFeature& f : mInput.mFeatures)
                    {
                        if (f.mKind == FeatureKind::eDeathDrop && nx >= f.mX1 - step && nx <= f.mX2 + step && f.mY1 <= fy && f.mY2 >= n.mY)
                        {
                            deadly = true;
                        }
                    }
                    if (height >= (n.mHalf ? kDeadlyFall / 2 : kDeadlyFall) && !FeatureAt(FeatureKind::eSoftLanding, nx, fy - 5, n.mHalf) && !FeatureAt(FeatureKind::eSoftLanding, nx, fy, n.mHalf))
                    {
                        deadly = true;
                    }
                    if (!deadly)
                    {
                        s32 to = NodeOnLine(belowLine, nx);
                        // (rolled off he keeps the roll's speed and comes down further out: measured in the
                        // Mudanchee trial, off 862,360 to the right and 100 down, landing at 937 and not 887)
                        if ((n.mLow || crouched) && height * (n.mHalf ? 2 : 1) >= 80 && height * (n.mHalf ? 2 : 1) < 200)
                        {
                            const s32 out = NodeOnLine(belowLine, nx + dir * 2 * step);
                            if (out >= 0 && std::abs(mNodes[out].mX - (nx + dir * 2 * step)) <= step / 2 && !WallBetween(nx, nx + dir * 2 * step, fy, n.mHalf, n.mHalf ? 10 : 20))
                            {
                                to = out;
                            }
                        }
                        if (to >= 0 && (!mNodes[to].mHazard || mNodes[to].mClear >= 0))
                        {
                            AddEdge(edges, from, to, EdgeKind::eFall, 20 + height / 4 + (crouched ? 10 : 0), -1, left);
                            edges.back().mCrouched = crouched;
                        }
                    }
                }
            }

            if (n.mLow)
            {
                continue;
            }
            AddRunJumps(edges, from, dir);
            AddRunJumps(edges, from, dir, true);
            AddLedgeCatches(edges, from, dir);

            // A hop over a gap to a ledge on its far side, two to four cells off: he catches it as he passes or
            // drops. The game looks for a ledge from his feet to 75 over them, so one that much higher is caught
            // too (Necrum's path 2, from 487,960 up to 437,880).
            bool caught = false;
            {
                s32 x = n.mX;
                bool gap = n.mLiftStop < 0;
                // (a lift's shaft is one while the lift is away)
                s32 shaft = -1;
                for (s32 k = 1; k <= 4 && gap && !caught; k++)
                {
                    gap = !WallBetween(x, x + dir * step, n.mY, n.mHalf);
                    x = StepX(x, dir, n.mHalf);
                    for (std::size_t i = 0; gap && k >= 2 && !caught && i < mInput.mFeatures.size(); i++)
                    {
                        const NavFeature& f = mInput.mFeatures[i];
                        const s32 rise = (n.mY - f.mY1) * (n.mHalf ? 2 : 1);
                        if (f.mKind == FeatureKind::eEdge && f.mA && (f.mAnyScale || f.mHalf == n.mHalf) && (f.mDir == 2 || (f.mDir == 0) == left) && x >= f.mX1 - 2 && x <= f.mX2 + 2 && rise <= 85 && rise >= -40)
                        {
                            const s32 to = NodeAt(x, f.mY1, n.mHalf, 12);
                            if (to >= 0)
                            {
                                AddEdge(edges, from, to, EdgeKind::eHop, kHopCost + 40, static_cast<s32>(i), left);
                                edges.back().mShaft = shaft;
                            }
                            caught = true;
                        }
                    }
                    s32 gy = 0;
                    const s32 under = FloorNear(x, n.mY, n.mHalf, 12, &gy);
                    const s32 at = under >= 0 ? NodeOnLine(under, x) : -1;
                    if (k < 4 && at >= 0 && mNodes[at].mLiftStop >= 0)
                    {
                        shaft = mNodes[at].mLiftStop;
                    }
                    else
                    {
                        gap = gap && under < 0;
                    }
                }
            }

            // A hop off the end of the floor, down to a floor that is lower: he goes three cells out level, then
            // drops, drifting on about a cell. Measured in Necrum's path 2 (from 662,880 to 562,960, from 312,880
            // to 212,980 and from 212,340 to 312,460): where he is each frame from when he starts to drop.
            if (!caught && n.mLiftStop < 0)
            {
                static const s32 kDrop[][2] = {{72, 0}, {80, 6}, {82, 17}, {83, 33}, {88, 51}, {92, 70}, {96, 90}, {100, 110}, {101, 130}};
                const s32 scale = n.mHalf ? 2 : 1;
                bool clear = !WallBetween(n.mX, n.mX + dir * step * 4, n.mY, n.mHalf);
                for (s32 k = 1; k <= 3 && clear; k++)
                {
                    s32 gy = 0;
                    clear = FloorNear(n.mX + dir * k * step, n.mY, n.mHalf, 12, &gy) < 0;
                }
                // A ledge at his own height four cells off is what he comes down on instead (Necrum's path 6, from
                // ledge to ledge at 1187 and 1287: there is a hoist at its end, no edge marker)
                if (clear)
                {
                    s32 gy = 0;
                    const s32 level = FloorNear(n.mX + dir * 4 * step, n.mY, n.mHalf, 12, &gy);
                    const s32 to = level >= 0 ? NodeOnLine(level, n.mX + dir * 4 * step) : -1;
                    if (to >= 0 && std::abs(mNodes[to].mX - (n.mX + dir * 4 * step)) <= step / 2)
                    {
                        // (he falls short of it and catches that hoist as he drops. Without one he goes on down:
                        // the Mines' path 4, from 2062,1400 at 2162,1400, was a death)
                        s32 hoistIdx = -1;
                        const NavFeature* hoist = FeatureAt(FeatureKind::eHoist, mNodes[to].mX, mNodes[to].mY + 16, n.mHalf, &hoistIdx);
                        if (!mNodes[to].mHazard && !mNodes[to].mLow && hoist && (hoist->mDir == 2 || (hoist->mDir == 0) == left))
                        {
                            AddEdge(edges, from, to, EdgeKind::eHop, kHopCost + 10, -1, left);
                        }
                        clear = false;
                    }
                }
                for (std::size_t k = 1; clear && k < sizeof(kDrop) / sizeof(kDrop[0]); k++)
                {
                    const s32 x = n.mX + dir * kDrop[k][0] / scale;
                    s32 gy = 0;
                    const s32 line = FloorBelow(x, n.mY + kDrop[k - 1][1] / scale + 1, n.mHalf, &gy);
                    if (line >= 0 && gy <= n.mY + kDrop[k][1] / scale)
                    {
                        // (he ends in the middle of the cell he comes down in; or in the well that is there, which
                        // takes him: the Mudanchee vault's path 5, off the platform at 312,620 into the well at 212)
                        s32 to = NodeAt(x, gy, n.mHalf, 4);
                        const s32 well = WellInFall(x, n.mY, gy + 30 / scale, n.mHalf);
                        // (one that a switch turns: where it leads either way. The Mudomo trial's last, hopped
                        // into off the lock's ledge once the lock has turned it to throw him back over the gate.)
                        if (well >= 0 && mInput.mFeatures[well].mSwitch > 1 && gy - n.mY >= 20 / scale)
                        {
                            AddWellEdges(edges, from, well, EdgeKind::eHop, kHopCost + 10, -1, left);
                            break;
                        }
                        if (well >= 0)
                        {
                            to = WellOut(well, mInput.mFeatures[well].mSwitch == 1 ? 1 : 0, 1);
                        }
                        // (not through where a drill goes: the Mines' path 4, off 2537,1400, was two deaths)
                        bool drill = false;
                        for (const NavFeature& f : mInput.mFeatures)
                        {
                            drill = drill || (f.mKind == FeatureKind::eSweep && f.mX2 >= std::min(n.mX, x) - step && f.mX1 <= std::max(n.mX, x) + step && f.mY2 >= n.mY - 70 / scale && f.mY1 <= gy);
                        }
                        if (to >= 0 && to != from && gy - n.mY >= 20 / scale && !mNodes[to].mHazard && !mNodes[to].mLow && !drill)
                        {
                            AddEdge(edges, from, to, EdgeKind::eHop, kHopCost + 10, -1, left);
                            // (the well he falls into, if it is one)
                            edges.back().mWell = well;
                        }
                        break;
                    }
                }
            }

            // A hop, where walking can't do it: over a hazard or a gap
            const s32 hx = StepX(StepX(nx, dir, n.mHalf), dir, n.mHalf);
            const s32 hopLine = FloorNear(hx, n.mY, n.mHalf, 12, &fy);
            if (hopLine >= 0 && !mNodes[std::max(0, NodeOnLine(hopLine, hx))].mLow && !WallBetween(n.mX, n.mX + dir * step * 3, n.mY, n.mHalf))
            {
                bool needed = false;
                for (s32 k = 1; k <= 2; k++)
                {
                    const s32 mx = k == 1 ? nx : StepX(nx, dir, n.mHalf);
                    s32 my = 0;
                    const s32 midLine = FloorNear(mx, n.mY, n.mHalf, 12, &my);
                    const s32 mid = midLine >= 0 ? NodeOnLine(midLine, mx) : -1;
                    if (mid < 0 || std::abs(mNodes[mid].mX - mx) > step / 2 || mNodes[mid].mHazard)
                    {
                        needed = true;
                    }
                }
                const s32 to = NodeOnLine(hopLine, hx);
                if (needed && to >= 0 && std::abs(mNodes[to].mX - hx) <= step / 2 && !mNodes[to].mHazard)
                {
                    AddEdge(edges, from, to, EdgeKind::eHop, kHopCost, BarrierBetween(n.mX, n.mX + dir * step * 3, n.mY, n.mHalf), left);
                }
            }
        }

        if (n.mLow)
        {
            // (the way out of a path can be a crawl: Mines path 6)
            s32 exitIdx = -1;
            if (FeatureAt(FeatureKind::ePathExit, n.mX, n.mY, n.mHalf, &exitIdx) || FeatureAt(FeatureKind::ePathExit, n.mX, n.mY - 20, n.mHalf, &exitIdx))
            {
                mExits.push_back({from, exitIdx});
            }
            continue;
        }

        s32 featureIdx = -1;
        // Up: the hoist is looked for a little above the floor
        if (FeatureAt(FeatureKind::eHoist, n.mX, n.mY - 10, n.mHalf, &featureIdx))
        {
            s32 fy = 0;
            const s32 above = FloorAbove(n.mX, n.mY, n.mHalf, 20, &fy);
            if (above >= 0)
            {
                AddEdge(edges, from, NodeOnLine(above, n.mX), EdgeKind::eHoistUp, kHoistCost, featureIdx, mInput.mFeatures[featureIdx].mDir == 0);
            }
        }
        // Down: looked for a little under it
        if (FeatureAt(FeatureKind::eHoist, n.mX, n.mY + 16, n.mHalf, &featureIdx))
        {
            s32 fy = 0;
            const s32 below = FloorBelow(n.mX, n.mY + 20, n.mHalf, &fy);
            // (let go over a well he falls into it: Necrum's path 2, down the shaft at 62,620)
            s32 airWell = WellInFall(n.mX, n.mY, below >= 0 ? fy : n.mY + 2000, n.mHalf);
            // (and straight down into one that stands on the floor under him, its box reaching well up from that
            // floor: the Mudomo vault's path 12, down the shaft at 437,460 into the well at 437,1500)
            for (std::size_t i = 0; airWell < 0 && below >= 0 && i < mInput.mFeatures.size(); i++)
            {
                const NavFeature& w = mInput.mFeatures[i];
                if ((w.mKind == FeatureKind::eWellLocal || w.mKind == FeatureKind::eWellExpress) && w.mHalf == n.mHalf && n.mX >= w.mX1 && n.mX <= w.mX2 && w.mY1 > n.mY + 30 && w.mY1 <= fy - 15 && w.mY2 >= fy - 5)
                {
                    airWell = static_cast<s32>(i);
                }
            }
            if (airWell >= 0)
            {
                AddWellEdges(edges, from, airWell, EdgeKind::eHoistDown, kHoistCost + 60 + kWellCost, featureIdx, mInput.mFeatures[featureIdx].mDir == 0);
            }
            else if (below >= 0)
            {
                AddEdge(edges, from, NodeOnLine(below, n.mX), EdgeKind::eHoistDown, kHoistCost + (fy - n.mY) / 6, featureIdx, mInput.mFeatures[featureIdx].mDir == 0);
            }
        }

        // A lift is worked from the middle of its platform
        if (n.mLiftStop >= 0)
        {
            const NavFeature& stop = mInput.mFeatures[n.mLiftStop];
            if (std::abs(n.mX - (stop.mX1 + stop.mX2) / 2) <= step / 2)
            {
                for (std::size_t k = 0; k < mNodes.size(); k++)
                {
                    const NavNode& other = mNodes[k];
                    if (other.mLiftStop >= 0 && other.mLiftStop != n.mLiftStop && other.mX == n.mX && other.mHalf == n.mHalf && mInput.mFeatures[other.mLiftStop].mX1 == stop.mX1)
                    {
                        AddEdge(edges, from, static_cast<s32>(k), EdgeKind::eLift, 40 + std::abs(other.mY - n.mY) / 3, n.mLiftStop, other.mY < n.mY);
                    }
                }
            }
        }

        if (const NavFeature* door = FeatureAt(FeatureKind::eDoor, n.mX, n.mY, n.mHalf, &featureIdx))
        {
            bool inPath = false;
            if (door->mDestPath < 0)
            {
                for (const NavFeature& other : mInput.mFeatures)
                {
                    if (other.mKind == FeatureKind::eDoor && &other != door && other.mA == door->mB && other.mCamera == door->mDestCamera)
                    {
                        const s32 to = NodeAt((other.mX1 + other.mX2) / 2, other.mY2, other.mHalf, 30);
                        AddEdge(edges, from, to, EdgeKind::eDoor, kDoorCost, featureIdx, false);
                        inPath = to >= 0;
                        break;
                    }
                }
            }
            // (one that leads into this path but to no door of it is where another comes out, not a way)
            if (!inPath && door->mDestPath >= 0)
            {
                mExits.push_back({from, featureIdx});
            }
        }

        // A well: where it lets him out with its switch off and with it on (one edge if that is the same place,
        // or if it has no switch of its own)
        for (s32 kind = 0; kind < 2; kind++)
        {
            const NavFeature* well = FeatureAt(kind == 0 ? FeatureKind::eWellLocal : FeatureKind::eWellExpress, n.mX, n.mY, n.mHalf, &featureIdx);
            if (!well)
            {
                continue;
            }
            AddWellEdges(edges, from, featureIdx, EdgeKind::eWell, kWellCost, featureIdx, false);
            const bool offOut = kind == 1 && well->mSwitch != 1 && well->mDestPath >= 0;
            // Out of the path (as it goes with its switch off: which is how the level's paths are counted)
            if (offOut)
            {
                mExits.push_back({from, featureIdx});
            }
        }

        const NavFeature* tele = FeatureAt(FeatureKind::eTeleporter, n.mX, n.mY, n.mHalf, &featureIdx);
        // (one that a plate in it sets off: from the cells of the plate)
        for (const NavFeature& plate : mInput.mFeatures)
        {
            if (tele && plate.mKind == FeatureKind::ePlate && plate.mSwitch == tele->mSwitch && plate.mHalf == tele->mHalf && (plate.mX1 + plate.mX2) / 2 >= tele->mX1 && (plate.mX1 + plate.mX2) / 2 <= tele->mX2 &&
                std::abs(plate.mY2 - n.mY) <= 30 && (n.mX < plate.mX1 - 2 || n.mX > plate.mX2 + 2))
            {
                tele = nullptr;
            }
        }
        if (tele)
        {
            for (const NavFeature& other : mInput.mFeatures)
            {
                if (other.mKind == FeatureKind::eTeleporter && &other != tele && other.mA == tele->mB && other.mCamera == tele->mDestCamera)
                {
                    s32 fy = 0;
                    const s32 cx = (other.mX1 + other.mX2) / 2;
                    // (the floor in its box, not one that its top touches: the Mudanchee trial has one right
                    // under the floor of another)
                    s32 line = FloorBelow(cx, other.mY1 + 5, other.mHalf, &fy);
                    if (line < 0 || fy > other.mY2 + 12)
                    {
                        line = FloorBelow(cx, other.mY1 - 20, other.mHalf, &fy);
                    }
                    AddEdge(edges, from, NodeOnLine(line, cx), EdgeKind::eTeleport, kWellCost, featureIdx, false);
                    break;
                }
            }
        }

        // A bird portal of Abe's own: hopped into from two cells before it on the side it is entered from (the
        // game takes a hop within two cells of it, facing it, on its floor), out at its exit
        for (std::size_t k = 0; k < mInput.mFeatures.size(); k++)
        {
            const NavFeature& portal = mInput.mFeatures[k];
            if (portal.mKind != FeatureKind::ePortal || portal.mHalf != n.mHalf || std::abs(portal.mY2 - n.mY) > 30)
            {
                continue;
            }
            const s32 px = (portal.mX1 + portal.mX2) / 2;
            const s32 before = portal.mDir == 0 ? px - n.mX : n.mX - px;
            // (one that leads out of the path is a way out of it, from the cell it is hopped into from: ExitTask)
            if (portal.mDestPath >= 0)
            {
                if (before >= 0 && (before + step / 2) / step == 2)
                {
                    mExits.push_back({from, static_cast<s32>(k)});
                }
                continue;
            }
            // (from up to five cells before it: until it is open nobody may come within three cells of it, or its
            // doves fly off and it is gone, so it is chanted open from back there and then walked up to)
            // (in cells: a portal's middle is not always a cell's)
            const s32 cellsBefore = before >= 0 ? (before + step / 2) / step : -1;
            if (cellsBefore < 2 || cellsBefore > 5)
            {
                continue;
            }
            for (const NavFeature& out : mInput.mFeatures)
            {
                if (out.mKind == FeatureKind::ePortalExit && out.mCamera == portal.mDestCamera && out.mHalf == portal.mHalf)
                {
                    s32 fy = 0;
                    const s32 cx = (out.mX1 + out.mX2) / 2;
                    const s32 line = FloorBelow(cx, out.mY1 - 20, out.mHalf, &fy);
                    if (line >= 0)
                    {
                        AddEdge(edges, from, NodeOnLine(line, cx), EdgeKind::ePortal, 3 * kWellCost, static_cast<s32>(k), portal.mDir != 0);
                    }
                    break;
                }
            }
        }

        if (FeatureAt(FeatureKind::ePathExit, n.mX, n.mY, n.mHalf, &featureIdx) || FeatureAt(FeatureKind::ePathExit, n.mX, n.mY - 20, n.mHalf, &featureIdx))
        {
            mExits.push_back({from, featureIdx});
        }
        else
        {
            // Or one he drops into off the end of a floor (the Mudanchee trial: "drop off the edge")
            const s32 step = GridStep(n.mHalf);
            for (s32 dir = -1; dir <= 1; dir += 2)
            {
                const s32 next = NodeOnLine(n.mLine, n.mX + dir * step);
                s32 fy = 0;
                if ((next < 0 || std::abs(mNodes[next].mX - (n.mX + dir * step)) > step / 2) && !WallBetween(n.mX, n.mX + dir * step, n.mY, n.mHalf) &&
                    FloorBelow(n.mX + dir * step, n.mY + 20, n.mHalf, &fy) < 0 &&
                    (FeatureAt(FeatureKind::ePathExit, n.mX + dir * step, n.mY + 80, n.mHalf, &featureIdx) || FeatureAt(FeatureKind::ePathExit, n.mX + dir * step, n.mY + 140, n.mHalf, &featureIdx)))
                {
                    mExits.push_back({from, featureIdx});
                    break;
                }
            }
        }
    }

    for (std::size_t i = 0; i < mInput.mFeatures.size(); i++)
    {
        if (mInput.mFeatures[i].mKind == FeatureKind::eLevelLoader)
        {
            mExits.push_back({-1, static_cast<s32>(i)});
        }
    }

    // Onto a trap door only while it is shut. Open, what is on it falls, and so does who steps off the floor
    // beside it.
    // (and with a well under it he falls into that: the Mudomo vault's path 12, where the lock that opens the trap
    // door at 687,660 also turns the well under it to the door's ledge)
    const auto trapFall = [&](s32 from, const NavNode& over, s32 trap, bool left)
    {
        s32 fy = 0;
        const s32 below = FloorBelow(over.mX, over.mY + 20, over.mHalf, &fy);
        const s32 well = WellInFall(over.mX, over.mY - 20, below >= 0 ? fy : over.mY + 2000, over.mHalf);
        if (well >= 0)
        {
            const NavFeature& w = mInput.mFeatures[well];
            if (w.mSwitch > 1 && w.mSwitch == mInput.mFeatures[trap].mSwitch)
            {
                const s32 to = WellOut(well, 1);
                if (to >= 0 && to != from)
                {
                    AddEdge(edges, from, to, EdgeKind::eFall, kWalkCost + kWellCost, trap, left);
                    edges.back().mWell = well;
                }
            }
            else
            {
                AddWellEdges(edges, from, well, EdgeKind::eFall, kWalkCost + kWellCost, trap, left);
            }
        }
        else if (below >= 0)
        {
            AddEdge(edges, from, NodeOnLine(below, over.mX), EdgeKind::eFall, kWalkCost + (fy - over.mY) / 6, trap, left);
        }
    };
    const std::size_t made = edges.size();
    for (std::size_t i = 0; i < made; i++)
    {
        const s32 trap = mNodes[edges[i].mTo].mTrapDoor;
        if (trap < 0 || mNodes[edges[i].mFrom].mTrapDoor == trap || edges[i].mFeature >= 0 || (edges[i].mKind != EdgeKind::eWalk && edges[i].mKind != EdgeKind::eRoll))
        {
            continue;
        }
        edges[i].mFeature = trap;
        const NavNode over = mNodes[edges[i].mTo];
        trapFall(edges[i].mFrom, over, trap, edges[i].mLeft);
    }
    for (std::size_t i = 0; i < mNodes.size(); i++)
    {
        const NavNode& n = mNodes[i];
        if (n.mTrapDoor >= 0)
        {
            trapFall(static_cast<s32>(i), n, n.mTrapDoor, false);
        }
    }

    // Into a cell with a mine, or out of one: not until the mine is gone
    for (NavEdge& e : edges)
    {
        e.mClear = mNodes[e.mTo].mClear >= 0 ? mNodes[e.mTo].mClear : mNodes[e.mFrom].mClear;
    }
    std::stable_sort(edges.begin(), edges.end(), [](const NavEdge& a, const NavEdge& b)
                     { return a.mFrom < b.mFrom; });
    mEdges = std::move(edges);
    for (NavNode& n : mNodes)
    {
        n.mFirstEdge = 0;
        n.mEdgeCount = 0;
    }
    for (std::size_t i = 0; i < mEdges.size(); i++)
    {
        NavNode& n = mNodes[mEdges[i].mFrom];
        if (n.mEdgeCount == 0)
        {
            n.mFirstEdge = static_cast<s32>(i);
        }
        n.mEdgeCount++;
    }
}

void NavGraph::LearnEdge(s32 edgeIdx, s32 actualTo)
{
    if (edgeIdx >= 0 && edgeIdx < static_cast<s32>(mEdges.size()) && actualTo >= 0)
    {
        mEdges[edgeIdx].mTo = actualTo;
    }
}

std::vector<s32> NavGraph::CostsFrom(s32 from, const EdgeCost& cost) const
{
    std::vector<s32> dist(mNodes.size(), -1);
    if (from < 0 || from >= static_cast<s32>(mNodes.size()))
    {
        return dist;
    }
    using Item = std::pair<s32, s32>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    dist[from] = 0;
    open.push({0, from});
    while (!open.empty())
    {
        const Item item = open.top();
        open.pop();
        if (item.first != dist[item.second])
        {
            continue;
        }
        const NavNode& n = mNodes[item.second];
        for (s32 e = n.mFirstEdge; e < n.mFirstEdge + n.mEdgeCount; e++)
        {
            const s32 c = cost ? cost(mEdges[e]) : mEdges[e].mCost;
            if (c < 0)
            {
                continue;
            }
            const s32 nd = item.first + c;
            const s32 to = mEdges[e].mTo;
            if (dist[to] < 0 || nd < dist[to])
            {
                dist[to] = nd;
                open.push({nd, to});
            }
        }
    }
    return dist;
}

bool NavGraph::FindPath(s32 from, const std::function<bool(s32)>& isGoal, const EdgeCost& cost, std::vector<s32>& path, s32* totalCost) const
{
    path.clear();
    if (from < 0 || from >= static_cast<s32>(mNodes.size()))
    {
        return false;
    }
    std::vector<s32> dist(mNodes.size(), -1);
    std::vector<s32> via(mNodes.size(), -1);
    using Item = std::pair<s32, s32>;
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    dist[from] = 0;
    open.push({0, from});
    while (!open.empty())
    {
        const Item item = open.top();
        open.pop();
        if (item.first != dist[item.second])
        {
            continue;
        }
        if (isGoal(item.second))
        {
            for (s32 at = item.second; via[at] >= 0; at = mEdges[via[at]].mFrom)
            {
                path.push_back(via[at]);
            }
            std::reverse(path.begin(), path.end());
            if (totalCost)
            {
                *totalCost = item.first;
            }
            return true;
        }
        const NavNode& n = mNodes[item.second];
        for (s32 e = n.mFirstEdge; e < n.mFirstEdge + n.mEdgeCount; e++)
        {
            const s32 c = cost ? cost(mEdges[e]) : mEdges[e].mCost;
            if (c < 0)
            {
                continue;
            }
            const s32 nd = item.first + c;
            const s32 to = mEdges[e].mTo;
            if (dist[to] < 0 || nd < dist[to])
            {
                dist[to] = nd;
                via[to] = e;
                open.push({nd, to});
            }
        }
    }
    return false;
}

bool NavGraph::FindPath(s32 from, s32 to, const EdgeCost& cost, std::vector<s32>& path, s32* totalCost) const
{
    return FindPath(from, [to](s32 node)
                    { return node == to; }, cost, path, totalCost);
}

std::string NavGraph::Text() const
{
    std::string out;
    for (std::size_t i = 0; i < mInput.mLines.size(); i++)
    {
        if (mLineCount[i] == 0)
        {
            continue;
        }
        const NavNode& a = mNodes[mLineFirst[i]];
        const NavNode& b = mNodes[mLineFirst[i] + mLineCount[i] - 1];
        out += "floor" + std::to_string(i) + (a.mHalf ? " bg" : " fg") + " x " + std::to_string(a.mX) + ".." + std::to_string(b.mX) + " y " + std::to_string(a.mY) + (a.mY != b.mY ? ".." + std::to_string(b.mY) : "") + " cells " + std::to_string(mLineCount[i]) + "\n";
    }
    for (const NavEdge& e : mEdges)
    {
        const NavNode& a = mNodes[e.mFrom];
        const NavNode& b = mNodes[e.mTo];
        if (e.mKind == EdgeKind::eWalk && a.mLine == b.mLine)
        {
            continue;
        }
        out += std::string(EdgeKindName(e.mKind)) + " " + std::to_string(a.mX) + "," + std::to_string(a.mY) + " -> " + std::to_string(b.mX) + "," + std::to_string(b.mY) + (e.mFeature >= 0 ? " f" + std::to_string(e.mFeature) : "") + "\n";
    }
    for (const NavExit& x : mExits)
    {
        const NavFeature& f = mInput.mFeatures[x.mFeature];
        out += "exit " + (x.mNode < 0 ? "by switch " + std::to_string(f.mSwitch) : std::to_string(mNodes[x.mNode].mX) + "," + std::to_string(mNodes[x.mNode].mY)) + " f" + std::to_string(x.mFeature) + " -> level " + std::to_string(f.mDestLevel) + " path " + std::to_string(f.mDestPath) + " cam " + std::to_string(f.mDestCamera) + "\n";
    }
    return out;
}

} // namespace Bot
