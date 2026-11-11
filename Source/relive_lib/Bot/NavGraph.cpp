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
    }
    else if (f.mKind == FeatureKind::eDoor && edge.mKind == EdgeKind::eDoor && f.mStartClosed && f.mSwitch > 1)
    {
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

s32 NavGraph::WellLanding(const NavFeature& well) const
{
    s32 fy = 0;
    const s32 under = FloorBelow(well.mC, well.mD - 40, well.mHalf, &fy);
    if (under >= 0 && fy - well.mD < 200)
    {
        return NodeOnLine(under, well.mC);
    }
    // Nothing under it: he flies on the way he was thrown and drops onto, or catches the edge of, what is next
    const s32 step = GridStep(well.mHalf);
    const s32 dir = well.mC < (well.mX1 + well.mX2) / 2 ? -1 : 1;
    for (s32 k = 1; k <= 4; k++)
    {
        const s32 x = well.mC + dir * k * step;
        const s32 line = FloorBelow(x, well.mD - 40, well.mHalf, &fy);
        if (line >= 0 && fy - well.mD < 200)
        {
            return NodeOnLine(line, x);
        }
    }
    return under >= 0 ? NodeOnLine(under, well.mC) : -1;
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
            else if (!standingWall || n.mLow)
            {
                // (from under a low ceiling it is rolled off)
                // Off the end of the floor
                s32 height = 0;
                const s32 belowLine = FloorBelow(nx, n.mY + 1, n.mHalf, &fy, &height);
                bool deadly = belowLine < 0;
                // A well on the floor he comes down on swallows him as he lands in it, however far he fell
                // (rolled off he drifts a few cells on the way down)
                bool intoWell = false;
                for (const NavFeature& f : mInput.mFeatures)
                {
                    if (deadly || intoWell || f.mKind != FeatureKind::eWellLocal || f.mHalf != n.mHalf || f.mY2 < n.mY + 40 || std::abs(f.mY2 - fy) > 10)
                    {
                        continue;
                    }
                    const s32 wx = (f.mX1 + f.mX2) / 2;
                    const s32 lo = std::min(nx, nx + dir * 3 * step) - step / 2;
                    const s32 hi = std::max(nx, nx + dir * 3 * step) + step / 2;
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
                        const s32 to = NodeOnLine(belowLine, nx);
                        if (to >= 0 && (!mNodes[to].mHazard || mNodes[to].mClear >= 0))
                        {
                            AddEdge(edges, from, to, EdgeKind::eFall, 20 + height / 4, -1, left);
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

            // A hop over a gap to a ledge on its far side, four cells off: he catches it as he drops
            {
                s32 x = n.mX;
                bool gap = n.mLiftStop < 0;
                // (a lift's shaft is one while the lift is away)
                s32 shaft = -1;
                for (s32 k = 1; k <= 4 && gap; k++)
                {
                    gap = !WallBetween(x, x + dir * step, n.mY, n.mHalf);
                    x = StepX(x, dir, n.mHalf);
                    s32 gy = 0;
                    const s32 under = FloorNear(x, n.mY, n.mHalf, 12, &gy);
                    const s32 at = under >= 0 ? NodeOnLine(under, x) : -1;
                    if (k < 4 && at >= 0 && mNodes[at].mLiftStop >= 0)
                    {
                        shaft = mNodes[at].mLiftStop;
                    }
                    else
                    {
                        gap = gap && (k == 4 || under < 0);
                    }
                }
                for (std::size_t k = 0; gap && k < mInput.mFeatures.size(); k++)
                {
                    const NavFeature& f = mInput.mFeatures[k];
                    if (f.mKind == FeatureKind::eEdge && f.mA && (f.mAnyScale || f.mHalf == n.mHalf) && (f.mDir == 2 || (f.mDir == 0) == left) && x >= f.mX1 - 2 && x <= f.mX2 + 2 && f.mY1 >= n.mY - 10 && f.mY1 <= n.mY + 40)
                    {
                        const s32 to = NodeAt(x, f.mY1, n.mHalf, 12);
                        if (to >= 0)
                        {
                            AddEdge(edges, from, to, EdgeKind::eHop, kHopCost + 40, static_cast<s32>(k), left);
                            edges.back().mShaft = shaft;
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
            if (below >= 0)
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
            if (!inPath)
            {
                mExits.push_back({from, featureIdx});
            }
        }

        if (const NavFeature* well = FeatureAt(FeatureKind::eWellLocal, n.mX, n.mY, n.mHalf, &featureIdx))
        {
            AddEdge(edges, from, WellLanding(*well), EdgeKind::eWell, kWellCost, featureIdx, false);
        }

        if (const NavFeature* well = FeatureAt(FeatureKind::eWellExpress, n.mX, n.mY, n.mHalf, &featureIdx))
        {
            bool inPath = false;
            if (well->mDestPath < 0)
            {
                for (const NavFeature& other : mInput.mFeatures)
                {
                    if ((other.mKind == FeatureKind::eWellExpress || other.mKind == FeatureKind::eWellLocal) && &other != well && other.mB == well->mA && other.mCamera == well->mDestCamera)
                    {
                        s32 fy = 0;
                        const s32 cx = (other.mX1 + other.mX2) / 2;
                        const s32 line = FloorBelow(cx, other.mY1 - 60, other.mHalf, &fy);
                        const s32 to = NodeOnLine(line, cx);
                        AddEdge(edges, from, to, EdgeKind::eWell, kWellCost, featureIdx, false);
                        inPath = to >= 0;
                        break;
                    }
                }
            }
            if (!inPath)
            {
                mExits.push_back({from, featureIdx});
            }
        }

        if (FeatureAt(FeatureKind::ePathExit, n.mX, n.mY, n.mHalf, &featureIdx) || FeatureAt(FeatureKind::ePathExit, n.mX, n.mY - 20, n.mHalf, &featureIdx))
        {
            mExits.push_back({from, featureIdx});
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
    const std::size_t made = edges.size();
    for (std::size_t i = 0; i < made; i++)
    {
        const s32 trap = mNodes[edges[i].mTo].mTrapDoor;
        if (trap < 0 || mNodes[edges[i].mFrom].mTrapDoor == trap || edges[i].mFeature >= 0 || (edges[i].mKind != EdgeKind::eWalk && edges[i].mKind != EdgeKind::eRoll))
        {
            continue;
        }
        edges[i].mFeature = trap;
        const NavNode& over = mNodes[edges[i].mTo];
        s32 fy = 0;
        const s32 below = FloorBelow(over.mX, over.mY + 20, over.mHalf, &fy);
        if (below >= 0)
        {
            AddEdge(edges, edges[i].mFrom, NodeOnLine(below, over.mX), EdgeKind::eFall, kWalkCost + (fy - over.mY) / 6, trap, edges[i].mLeft);
        }
    }
    for (std::size_t i = 0; i < mNodes.size(); i++)
    {
        const NavNode& n = mNodes[i];
        s32 fy = 0;
        if (n.mTrapDoor >= 0)
        {
            const s32 below = FloorBelow(n.mX, n.mY + 20, n.mHalf, &fy);
            if (below >= 0)
            {
                AddEdge(edges, static_cast<s32>(i), NodeOnLine(below, n.mX), EdgeKind::eFall, kWalkCost + (fy - n.mY) / 6, n.mTrapDoor, false);
            }
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
