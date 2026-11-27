#include "FollowBelow.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace Bot {

// The cells of the floor at the height of start, joined to it by walks and hops (whatever barriers are shut)
static std::vector<s32> FloorCells(const NavGraph& nav, s32 start)
{
    const NavNode& s = nav.Nodes()[start];
    std::vector<s32> cells{start};
    std::set<s32> seen{start};
    for (std::size_t i = 0; i < cells.size(); i++)
    {
        const NavNode& n = nav.Nodes()[cells[i]];
        for (s32 e = n.mFirstEdge; e < n.mFirstEdge + n.mEdgeCount; e++)
        {
            const NavEdge& edge = nav.Edges()[e];
            const NavNode& to = nav.Nodes()[edge.mTo];
            if ((edge.mKind == EdgeKind::eWalk || edge.mKind == EdgeKind::eHop) && to.mHalf == s.mHalf && std::abs(to.mY - s.mY) <= 2 && seen.insert(edge.mTo).second)
            {
                cells.push_back(edge.mTo);
            }
        }
    }
    return cells;
}

FollowBelow::Result FollowBelow::Find(const NavGraph& nav, const World& world, s32 abeNode)
{
    Result result;
    if (abeNode < 0 || !nav.Input().mIsAe)
    {
        return result;
    }
    const NavNode& abe = nav.Nodes()[abeNode];
    // Those that follow, a floor under him: the floor most of them are on
    std::map<s32, std::vector<const Entity*>> byFloor;
    std::map<s32, s32> floorOf;
    for (const Entity& mud : world.mEntities)
    {
        if (mud.mKind != EntityKind::eMudokon || mud.mGone || !mud.mLive || !mud.mFollowing || mud.mHalf != abe.mHalf || mud.mLiveY < abe.mY + 40 || mud.mLiveY > abe.mY + 300)
        {
            continue;
        }
        const s32 at = nav.NodeAt(mud.mLiveX, mud.mLiveY, mud.mHalf, 30);
        if (at < 0)
        {
            continue;
        }
        if (floorOf.count(at) == 0)
        {
            for (const s32 cell : FloorCells(nav, at))
            {
                floorOf[cell] = at;
            }
        }
        byFloor[floorOf[at]].push_back(&mud);
    }
    s32 under = -1;
    for (const auto& entry : byFloor)
    {
        if (under < 0 || entry.second.size() > byFloor[under].size())
        {
            under = entry.first;
        }
    }
    if (under < 0)
    {
        return result;
    }
    std::vector<s32> xs;
    for (const Entity* mud : byFloor[under])
    {
        result.mMuds.push_back(mud->mId);
        xs.push_back(mud->mLiveX);
    }
    std::sort(xs.begin(), xs.end());
    const s32 mudX = xs[xs.size() / 2];
    const s32 fy = nav.Nodes()[under].mY;
    s32 fx1 = nav.Nodes()[under].mX;
    s32 fx2 = fx1;
    for (const s32 cell : FloorCells(nav, under))
    {
        fx1 = std::min(fx1, nav.Nodes()[cell].mX);
        fx2 = std::max(fx2, nav.Nodes()[cell].mX);
    }
    const std::vector<s32> cells = FloorCells(nav, abeNode);
    for (const Entity& plate : world.mEntities)
    {
        const s32 px = plate.CentreX();
        if (plate.mKind != EntityKind::eFootSwitch || plate.mGone || plate.mInputs[0] != 1 || plate.mHalf != abe.mHalf || plate.mSwitch <= 1 || std::abs(plate.mY2 - fy) > 30 || px < fx1 - 12 || px > fx2 + 12)
        {
            continue;
        }
        Plate p;
        p.mId = plate.mId;
        p.mX = px;
        p.mSwitch = plate.mSwitch;
        p.mOp = plate.mOp;
        for (const s32 cell : cells)
        {
            if (std::abs(nav.Nodes()[cell].mX - px) <= kSpread + 12)
            {
                p.mFrom.push_back(cell);
            }
        }
        std::sort(p.mFrom.begin(), p.mFrom.end(), [&](s32 a, s32 b)
        {
            return nav.Nodes()[a].mX < nav.Nodes()[b].mX;
        });
        bool way = true;
        for (const NavFeature& f : nav.Input().mFeatures)
        {
            const s32 gx = (f.mX1 + f.mX2) / 2;
            if (f.mKind == FeatureKind::eBarrier && (f.mSolid || f.mBeam) && f.mHalf == abe.mHalf && std::abs(f.mY2 - fy) <= 40 && gx > std::min(mudX, px) && gx < std::max(mudX, px))
            {
                way = way && f.mSwitch > 1;
                p.mOpen.push_back({f.mSwitch, f.mStartClosed ? 1 : 0});
            }
        }
        if (way && !p.mFrom.empty())
        {
            result.mPlates.push_back(p);
        }
    }
    result.mFound = !result.mPlates.empty();
    return result;
}

} // namespace Bot
