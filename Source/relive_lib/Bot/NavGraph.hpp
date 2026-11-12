#pragma once

#include "../Types.hpp"
#include <functional>
#include <string>
#include <vector>

namespace Bot {

// A collision line, with the games' line type numbers (eLineTypes)
struct NavLine final
{
    s32 mX1 = 0;
    s32 mY1 = 0;
    s32 mX2 = 0;
    s32 mY2 = 0;
    s32 mType = 0;
};

enum class FeatureKind : u8
{
    eHoist,       // mDir: the way Abe must face (0 left, 1 right, 2 either)
    eEdge,
    eDoor,        // mA door id, mB target door id, mDestCamera (and level/path if it leaves the path), mSwitch if it starts closed
    eWellLocal,   // mB its own id, mC, mD: where it throws Abe (world x, y)
    eWellExpress, // mA: the id of the well it comes out of, mB its own id, mDest*
    eDeathDrop,
    eSoftLanding,
    eHazard,      // a mine or the like on the floor: never stood on, hopped over
    ePathExit,    // walking into it leaves the path, mDest*
    eInvisibleZone, // standing wholly inside it Abe isn't seen
    eLiftStop,    // a floor a lift stops at: its rectangle's top is the floor, as wide as the platform. mA: the
                  // lift starts here, mB its id, mSwitch: a lift mover brings it here once that switch is on
    eLevelLoader, // the game leaves the path by itself once switch mSwitch is on, mDest*
    eBarrier,     // a slam door, an electric wall...: nothing passes while it is shut. Open when switch mSwitch is
                  // on, or off if mStartClosed is false
    eSweep,       // a drill goes to and fro in its rectangle: the cells it reaches are crossed when it is out of
                  // the way (see GoToTask)
    eTrapDoor,    // floor along its rectangle's top while it is shut. Open while switch mSwitch is on if
                  // mStartClosed, else while it is off
    eTeleporter,  // whoever stands in it when its switch mSwitch changes (a lever at it does that) is put in the
                  // one with id mB in camera mDestCamera. mA: its own id
};

// Something in the map that changes where Abe can go. From the path's TLVs, see GameView.
struct NavFeature final
{
    FeatureKind mKind = FeatureKind::eHoist;
    s32 mX1 = 0;
    s32 mY1 = 0;
    s32 mX2 = 0;
    s32 mY2 = 0;
    bool mHalf = false;
    // Works at either scale (AO's hoists)
    bool mAnyScale = false;
    s32 mDir = 2;
    s32 mA = 0;
    s32 mB = 0;
    s32 mC = 0;
    s32 mD = 0;
    s32 mSwitch = 0;
    bool mStartClosed = false;
    s32 mDestLevel = -1;
    s32 mDestPath = -1;
    s32 mDestCamera = -1;
    // The camera it is in (its number), and a name that stays the same from run to run
    s32 mCamera = 0;
    u32 mId = 0;

    bool Contains(s32 x, s32 y) const
    {
        return x >= mX1 && x <= mX2 && y >= mY1 && y <= mY2;
    }
};

struct NavInput final
{
    bool mIsAe = true;
    // The size of a camera in world units, to tell which camera a point is in
    s32 mCamW = 375;
    s32 mCamH = 260;
    std::vector<NavLine> mLines;
    std::vector<NavFeature> mFeatures;
};

enum class EdgeKind : u8
{
    eWalk,
    eFall,      // walk off the end of the floor and land below
    eHop,       // a standing hop, three cells
    eRunJump,   // a running jump over a wider gap, from three cells before where it leaves the floor (mX)
    eLift,      // ride the lift from one of its stops to another
    eRoll,      // a step that only fits crouched: under a low ceiling or a wall that stops short of the floor
    eHoistUp,
    eHoistDown,
    eDoor,
    eWell,
    eTeleport,  // pull the lever of the teleporter he stands in: he is put in the one it leads to
};

const char* EdgeKindName(EdgeKind kind);

struct NavEdge final
{
    s32 mFrom = 0;
    s32 mTo = 0;
    EdgeKind mKind = EdgeKind::eWalk;
    // Rough frames to take it
    s32 mCost = 0;
    // The feature that makes it, or -1
    s32 mFeature = -1;
    bool mLeft = false;
    // A running jump: the cell it takes off from
    s32 mX = 0;
    // The hazard (its feature) that has to be gone before it can be taken: a mine in the cell it leads into
    s32 mClear = -1;
    // The lift stop (its feature) over whose shaft it jumps: only while the lift is away from it
    s32 mShaft = -1;
};

// A grid cell of floor Abe can stand on
struct NavNode final
{
    s32 mX = 0;
    s32 mY = 0;
    s32 mLine = 0;
    bool mHalf = false;
    bool mHazard = false;
    // The hazard is one that can be got rid of (a mine, with a rock): its feature, or -1
    s32 mClear = -1;
    // A drill reaches Abe here (its feature), or -1
    s32 mSweep = -1;
    // A drill goes to and fro over Abe's head here (its feature), or -1: he is safe under it crouched, so the
    // cell is one to roll through (mLow), and to stand up in only when the drill is away or stopped
    s32 mSweepHigh = -1;
    // A ceiling too low to stand under
    bool mLow = false;
    // On a lift's platform when it is at this stop (the stop's feature), or -1
    s32 mLiftStop = -1;
    // On a trap door (its feature), or -1: floor only while it is shut
    s32 mTrapDoor = -1;
    // In an invisible zone: not seen there
    bool mHidden = false;
    s32 mFirstEdge = 0;
    s32 mEdgeCount = 0;
};

struct NavExit final
{
    // Where to stand, or -1 for one that needs no place (a level loader)
    s32 mNode = 0;
    s32 mFeature = 0;
};

// Where Abe can stand in a path and how he gets from one place to the next. Built from the lines and features alone:
// it is a guess at what the game allows, which the bot corrects by doing (see Bot::LearnEdge).
class NavGraph final
{
public:
    void Build(const NavInput& input);

    const std::vector<NavNode>& Nodes() const
    {
        return mNodes;
    }
    const std::vector<NavEdge>& Edges() const
    {
        return mEdges;
    }
    const std::vector<NavExit>& Exits() const
    {
        return mExits;
    }
    const NavInput& Input() const
    {
        return mInput;
    }

    // The cell at a world position standing on a floor near y, or -1
    s32 NodeAt(s32 x, s32 y, bool half, s32 yTolerance = 20) const;

    // Edge cost in frames, or negative for one that can't be taken now
    using EdgeCost = std::function<s32(const NavEdge&)>;
    // The cheapest way from a node to any node isGoal accepts, as edge indices. False if there's none.
    bool FindPath(s32 from, const std::function<bool(s32)>& isGoal, const EdgeCost& cost, std::vector<s32>& edges, s32* totalCost = nullptr) const;
    bool FindPath(s32 from, s32 to, const EdgeCost& cost, std::vector<s32>& edges, s32* totalCost = nullptr) const;
    // Every node's cost from a node (negative: not reachable)
    std::vector<s32> CostsFrom(s32 from, const EdgeCost& cost) const;

    // Groups of cells Abe can move about in freely both ways, whatever the switches: region number per node.
    // Getting from one region to another takes an edge that is one way or has a barrier.
    std::vector<s32> Regions(s32* count = nullptr) const;

    // Whether an edge can only be taken while a switch has a value (through a slam door, an electric wall, a door
    // that starts shut): which switch, and the value that lets Abe through
    bool Condition(const NavEdge& edge, s32* switchId, s32* openValue) const;

    // What doing an edge really led to: it goes there from now on
    void LearnEdge(s32 edgeIdx, s32 actualTo);

    // The floor a point over it would land on: line index or -1, with the y there
    // fallDistance is how far the fall is (AO's cameras have gaps between them that a fall skips)
    s32 FloorBelow(s32 x, s32 y, bool half, s32* floorY, s32* fallDistance = nullptr) const;
    // The nearest floor above a point, at least minRise higher
    s32 FloorAbove(s32 x, s32 y, bool half, s32 minRise, s32* floorY) const;
    // The cell centre one step left (dir -1) or right (1) of a cell: AO skips the gap between two cameras
    s32 StepX(s32 x, s32 dir, bool half) const;
    s32 FloorNear(s32 x, s32 y, bool half, s32 tolerance, s32* floorY) const;
    s32 CameraOf(s32 x, s32 y) const;

    std::string Text() const;
    static s32 GridStep(bool half)
    {
        return half ? 13 : 25;
    }
    // The cell centre at or after x
    s32 SnapUp(s32 x, bool half) const;

private:
    bool IsFloor(const NavLine& line, bool half) const;
    // height: how far above the floor to look (0: at standing and at crouching height)
    bool WallBetween(s32 xa, s32 xb, s32 y, bool half, s32 height = 0) const;
    bool CeilingOver(s32 x, s32 y, bool half) const;
    // overLift: the jumps there are over a lift's shaft when the lift is away (the edge's feature is the stop)
    void AddRunJumps(std::vector<NavEdge>& edges, s32 from, s32 dir, bool overLift = false) const;
    s32 BarrierBetween(s32 xa, s32 xb, s32 y, bool half) const;
    const NavFeature* FeatureAt(FeatureKind kind, s32 x, s32 y, bool half, s32* index = nullptr) const;
    void AddEdge(std::vector<NavEdge>& edges, s32 from, s32 to, EdgeKind kind, s32 cost, s32 feature, bool left) const;
    s32 NodeOnLine(s32 line, s32 x) const;
    // The cell a local well throws Abe to: under where it aims, or the ledge just beyond that he catches
    s32 WellLanding(const NavFeature& well) const;

    NavInput mInput;
    std::vector<NavNode> mNodes;
    std::vector<NavEdge> mEdges;
    std::vector<NavExit> mExits;
    // Per line: its first node and how many (in x order)
    std::vector<s32> mLineFirst;
    std::vector<s32> mLineCount;
};

} // namespace Bot
