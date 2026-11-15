#include "FartFlight.hpp"
#include "GameView.hpp"
#include "World.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <queue>

namespace Bot {

// The way is looked for on a grid of points a cell apart, in the middles of the cells: floors and walls lie on
// the cells' sides, so no point is on one
static const s32 kStepX = 25;
static const s32 kStepY = 20;
static const s32 kAccel = 20;
static const s32 kTopSpeed = 400;

// Whether a line stops it sideways (walls) or up and down (floors, ceilings): PathLine types, the second set for
// half scale. Type 17/18 is the wall only flying things meet, 32/36 a platform's floor.
static bool StopsSideways(s32 type, bool half)
{
    return half ? (type == 18 || type == 5 || type == 6) : (type == 17 || type == 1 || type == 2);
}

static bool StopsUpDown(s32 type, bool half)
{
    return half ? (type == 18 || type == 4 || type == 7 || type == 36) : (type == 17 || type == 0 || type == 3 || type == 32);
}

static s64 Cross(s64 ax, s64 ay, s64 bx, s64 by)
{
    return ax * by - ay * bx;
}

// Whether two segments meet, ends counted
static bool Meet(s32 ax, s32 ay, s32 bx, s32 by, s32 cx, s32 cy, s32 dx, s32 dy)
{
    if (std::max(ax, bx) < std::min(cx, dx) || std::max(cx, dx) < std::min(ax, bx) || std::max(ay, by) < std::min(cy, dy) || std::max(cy, dy) < std::min(ay, by))
    {
        return false;
    }
    const s64 d1 = Cross(bx - ax, by - ay, cx - ax, cy - ay);
    const s64 d2 = Cross(bx - ax, by - ay, dx - ax, dy - ay);
    const s64 d3 = Cross(dx - cx, dy - cy, ax - cx, ay - cy);
    const s64 d4 = Cross(dx - cx, dy - cy, bx - cx, by - cy);
    return ((d1 <= 0 && d2 >= 0) || (d1 >= 0 && d2 <= 0)) && ((d3 <= 0 && d4 >= 0) || (d3 >= 0 && d4 <= 0));
}

// Nothing of the path is between two points, for a move that goes sideways, up or down, or both
static bool Clear(const NavInput& in, bool half, const std::vector<NavLine>* more, s32 ax, s32 ay, s32 bx, s32 by)
{
    for (std::size_t i = 0; more && i < more->size(); i++)
    {
        const NavLine& line = (*more)[i];
        if (((ax != bx && line.mType == 1) || (ay != by && line.mType == 0)) && Meet(ax, ay, bx, by, line.mX1, line.mY1, line.mX2, line.mY2))
        {
            return false;
        }
    }
    for (const NavLine& line : in.mLines)
    {
        const bool stops = (ax != bx && StopsSideways(line.mType, half)) || (ay != by && StopsUpDown(line.mType, half));
        if (stops && Meet(ax, ay, bx, by, line.mX1, line.mY1, line.mX2, line.mY2))
        {
            return false;
        }
    }
    return true;
}

bool FartFlight::Route(const NavInput& in, bool half, Point from, Point to, s32 maxLen, std::vector<Point>& corners, s32* length, const std::vector<NavLine>* more)
{
    corners.clear();
    if (Clear(in, half, more, from.mX, from.mY, to.mX, to.mY))
    {
        const s32 len = std::abs(to.mX - from.mX) + std::abs(to.mY - from.mY);
        if (len > maxLen)
        {
            return false;
        }
        corners.push_back(to);
        if (length)
        {
            *length = len;
        }
        return true;
    }
    // The grid: every cell within reach of both ends
    const s32 x1 = std::min(from.mX, to.mX) - maxLen / 2;
    const s32 y1 = std::min(from.mY, to.mY) - maxLen / 2;
    const s32 x2 = std::max(from.mX, to.mX) + maxLen / 2;
    const s32 y2 = std::max(from.mY, to.mY) + maxLen / 2;
    const s32 col0 = static_cast<s32>(std::floor(static_cast<double>(x1) / kStepX));
    const s32 row0 = static_cast<s32>(std::floor(static_cast<double>(y1) / kStepY));
    const s32 cols = (x2 - x1) / kStepX + 2;
    const s32 rows = (y2 - y1) / kStepY + 2;
    const auto px = [col0](s32 c)
    {
        return (col0 + c) * kStepX + kStepX / 2;
    };
    const auto py = [row0](s32 r)
    {
        return (row0 + r) * kStepY + kStepY / 2;
    };
    // Which steps right and down a line of the path cuts: each line marks the cells about it
    std::vector<u8> right(static_cast<std::size_t>(cols) * rows, 0);
    std::vector<u8> down(static_cast<std::size_t>(cols) * rows, 0);
    for (std::size_t li = 0; li < in.mLines.size() + (more ? more->size() : 0); li++)
    {
        const bool extra = li >= in.mLines.size();
        const NavLine& line = extra ? (*more)[li - in.mLines.size()] : in.mLines[li];
        const bool sideways = extra ? line.mType == 1 : StopsSideways(line.mType, half);
        const bool upDown = extra ? line.mType == 0 : StopsUpDown(line.mType, half);
        if (!sideways && !upDown)
        {
            continue;
        }
        const s32 c1 = std::max(0, (std::min(line.mX1, line.mX2) - x1) / kStepX - 2);
        const s32 c2 = std::min(cols - 1, (std::max(line.mX1, line.mX2) - x1) / kStepX + 2);
        const s32 r1 = std::max(0, (std::min(line.mY1, line.mY2) - y1) / kStepY - 2);
        const s32 r2 = std::min(rows - 1, (std::max(line.mY1, line.mY2) - y1) / kStepY + 2);
        for (s32 r = r1; r <= r2; r++)
        {
            for (s32 c = c1; c <= c2; c++)
            {
                const std::size_t i = static_cast<std::size_t>(r) * cols + c;
                if (sideways && Meet(px(c), py(r), px(c + 1), py(r), line.mX1, line.mY1, line.mX2, line.mY2))
                {
                    right[i] = 1;
                }
                if (upDown && Meet(px(c), py(r), px(c), py(r + 1), line.mX1, line.mY1, line.mX2, line.mY2))
                {
                    down[i] = 1;
                }
            }
        }
    }
    // An end's cell: the nearest point of the grid it can get to in a straight line
    const auto cell = [&](Point p, s32& c, s32& r)
    {
        const s32 pc = static_cast<s32>(std::floor(static_cast<double>(p.mX) / kStepX)) - col0;
        const s32 pr = static_cast<s32>(std::floor(static_cast<double>(p.mY) / kStepY)) - row0;
        s32 best = -1;
        for (s32 dr = -1; dr <= 1; dr++)
        {
            for (s32 dc = -1; dc <= 1; dc++)
            {
                const s32 cc = pc + dc;
                const s32 rr = pr + dr;
                if (cc < 0 || rr < 0 || cc >= cols || rr >= rows || !Clear(in, half, more, p.mX, p.mY, px(cc), py(rr)))
                {
                    continue;
                }
                const s32 d = std::abs(px(cc) - p.mX) + std::abs(py(rr) - p.mY);
                if (best < 0 || d < best)
                {
                    best = d;
                    c = cc;
                    r = rr;
                }
            }
        }
        return best >= 0;
    };
    s32 fc = 0;
    s32 fr = 0;
    s32 tc = 0;
    s32 tr = 0;
    if (!cell(from, fc, fr) || !cell(to, tc, tr))
    {
        return false;
    }
    // The shortest way over the grid, a turn costing a little so that it has few
    static const s32 kTurn = 8;
    struct Item final
    {
        s32 mCost;
        s32 mAt;
        bool operator>(const Item& o) const
        {
            return mCost > o.mCost;
        }
    };
    // (a state is a cell and the way it was come into: 0 sideways, 1 up or down)
    const std::size_t count = static_cast<std::size_t>(cols) * rows * 2;
    std::vector<s32> cost(count, -1);
    std::vector<s32> prev(count, -1);
    std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
    const s32 start = (fr * cols + fc) * 2;
    cost[start] = 0;
    cost[start + 1] = 0;
    open.push({0, start});
    open.push({0, start + 1});
    const s32 goal = tr * cols + tc;
    s32 end = -1;
    while (!open.empty())
    {
        const Item item = open.top();
        open.pop();
        if (item.mCost != cost[item.mAt])
        {
            continue;
        }
        const s32 at = item.mAt / 2;
        if (at == goal)
        {
            end = item.mAt;
            break;
        }
        if (item.mCost > maxLen)
        {
            break;
        }
        const s32 c = at % cols;
        const s32 r = at / cols;
        const s32 was = item.mAt % 2;
        const auto step = [&](s32 nc, s32 nr, bool blocked, s32 len, s32 way)
        {
            if (blocked || nc < 0 || nr < 0 || nc >= cols || nr >= rows)
            {
                return;
            }
            const s32 next = (nr * cols + nc) * 2 + way;
            const s32 total = item.mCost + len + (way != was ? kTurn : 0);
            if (cost[next] < 0 || total < cost[next])
            {
                cost[next] = total;
                prev[next] = item.mAt;
                open.push({total, next});
            }
        };
        step(c + 1, r, right[at] != 0, kStepX, 0);
        step(c - 1, r, c > 0 && right[at - 1] != 0, kStepX, 0);
        step(c, r + 1, down[at] != 0, kStepY, 1);
        step(c, r - 1, r > 0 && down[at - cols] != 0, kStepY, 1);
    }
    if (end < 0)
    {
        return false;
    }
    std::vector<Point> points;
    points.push_back(to);
    for (s32 at = end; at >= 0; at = prev[at])
    {
        const Point p{px((at / 2) % cols), py((at / 2) / cols)};
        if (points.back().mX != p.mX || points.back().mY != p.mY)
        {
            points.push_back(p);
        }
    }
    std::reverse(points.begin(), points.end());
    // Only where it turns
    s32 len = std::abs(points[0].mX - from.mX) + std::abs(points[0].mY - from.mY);
    for (std::size_t i = 0; i < points.size(); i++)
    {
        if (i > 0)
        {
            len += std::abs(points[i].mX - points[i - 1].mX) + std::abs(points[i].mY - points[i - 1].mY);
        }
        if (i > 0 && i + 1 < points.size())
        {
            const bool straightX = points[i - 1].mY == points[i].mY && points[i].mY == points[i + 1].mY;
            const bool straightY = points[i - 1].mX == points[i].mX && points[i].mX == points[i + 1].mX;
            if (straightX || straightY)
            {
                continue;
            }
        }
        corners.push_back(points[i]);
    }
    if (length)
    {
        *length = len;
    }
    return len <= maxLen;
}

std::vector<NavLine> FartFlight::Gates(const NavInput& in, bool half, const World* world, std::vector<s32>* features)
{
    std::vector<NavLine> lines;
    for (std::size_t fi = 0; fi < in.mFeatures.size(); fi++)
    {
        const NavFeature& f = in.mFeatures[fi];
        if (f.mHalf != half || (f.mKind != FeatureKind::eBarrier && f.mKind != FeatureKind::eTrapDoor))
        {
            continue;
        }
        if (world && f.mSwitch > 1 && (world->Switch(f.mSwitch) != 0) == f.mStartClosed)
        {
            // (open)
            continue;
        }
        NavLine line;
        if (f.mKind == FeatureKind::eBarrier)
        {
            // (it stands on its floor, a corridor high)
            line.mX1 = line.mX2 = (f.mX1 + f.mX2) / 2;
            line.mY1 = f.mY1 - 80;
            line.mY2 = f.mY2;
            line.mType = 1;
        }
        else
        {
            line.mX1 = f.mX1;
            line.mX2 = f.mX2;
            line.mY1 = line.mY2 = f.mY1;
            line.mType = 0;
        }
        lines.push_back(line);
        if (features)
        {
            features->push_back(static_cast<s32>(fi));
        }
    }
    return lines;
}

bool FartFlight::Through(Point from, const std::vector<Point>& corners, const NavLine& line)
{
    Point at = from;
    for (const Point& p : corners)
    {
        if (Meet(at.mX, at.mY, p.mX, p.mY, line.mX1, line.mY1, line.mX2, line.mY2))
        {
            return true;
        }
        at = p;
    }
    return false;
}

s32 FartFlight::Coast(s32 vel100)
{
    const s32 v = std::abs(vel100);
    const s32 n = v / kAccel;
    return v * n - kAccel * n * (n + 1) / 2;
}

// One way of the two: 1 to hold on towards the place, 0 to let go
static bool HoldOn(s32 err, s32 vel)
{
    if (err == 0)
    {
        return false;
    }
    const s32 dir = err > 0 ? 1 : -1;
    const s32 towards = vel * dir;
    if (towards < 0)
    {
        return true;
    }
    // (held a frame more it goes that much faster, and from then on it still coasts)
    const s32 faster = std::min(kTopSpeed, towards + kAccel);
    return std::abs(err) > faster + FartFlight::Coast(faster);
}

u32 FartFlight::Steer(s32 x100, s32 y100, s32 velX100, s32 velY100, s32 tx, s32 ty)
{
    u32 held = 0;
    const s32 ex = tx * 100 - x100;
    const s32 ey = ty * 100 - y100;
    if (HoldOn(ex, velX100))
    {
        held |= ex > 0 ? Btn::eRight : Btn::eLeft;
    }
    if (HoldOn(ey, velY100))
    {
        held |= ey > 0 ? Btn::eDown : Btn::eUp;
    }
    return held;
}

bool FartFlight::Hits(Point at, s32 x1, s32 y1, s32 x2, s32 y2)
{
    return x2 >= at.mX - kBlastX && x1 <= at.mX + kBlastX && y2 >= at.mY - kBlastUp && y1 <= at.mY + kBlastDown;
}

} // namespace Bot
