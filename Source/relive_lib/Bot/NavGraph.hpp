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
    eSwitchZone,  // a box that sets mSwitch as he stands in it (an invisible switch): mA 0 on, 1 off, 2 other
    eDoor,        // mA door id, mB target door id, mDestCamera (and level/path if it leaves the path), mSwitch if it starts closed
    eWellLocal,   // mB its own id, mC, mD: where it throws Abe (world x, y)
    eWellExpress, // mA: the id of the well it comes out of, mB its own id, mDest*
    eDeathDrop,
    eSoftLanding,
    eHazard,      // a mine or the like on the floor: never stood on, hopped over
    ePathExit,    // walking into it leaves the path, mDest*
    eInvisibleZone, // standing wholly inside it Abe isn't seen
    eLiftStop,    // a floor a lift stops at: its rectangle's top is the floor, as wide as the platform. mA: the
                  // lift starts here, mB its id, mSwitch: a lift mover brings it here once that switch is on,
                  // mC: 1 the ride up ends here, 2 the ride down does (only a lift mover takes it on past)
    eLevelLoader, // the game leaves the path by itself once switch mSwitch is on, mDest*
    eBarrier,     // a slam door, an electric wall...: nothing passes while it is shut. Open when switch mSwitch is
                  // on, or off if mStartClosed is false
    eSweep,       // a drill goes to and fro in its rectangle: the cells it reaches are crossed when it is out of
                  // the way (see GoToTask)
    eTrapDoor,    // floor along its rectangle's top while it is shut. Open while switch mSwitch is on if
                  // mStartClosed, else while it is off
    eSaw,         // AO's meat saw that comes down once each time switch mSwitch is set: what stands under it then
                  // dies. Nothing to the graph: see Domain's Slig drop
    eTeleporter,  // whoever stands in it when its switch mSwitch changes (a lever at it does that) is put in the
                  // one with id mB in camera mDestCamera. mA: its own id
    ePortal,      // a bird portal Abe goes through himself: there once switch mSwitch is on (always if that is 0 or
                  // 1), opened by chanting, hopped into from the side mDir (0 from its left, 1 from its right).
                  // It lets him out at the ePortalExit in camera mDestCamera
    ePortalExit,
    eDanger,      // the stretch a flying Slig that can be got rid of goes along: nothing is crossed into it until
                  // the Slig is gone, which the world gives as switch mSwitch
    eZCover,      // AE: wholly inside it Abe isn't seen by the Sligs that shoot from the background
    eFlyingMine,  // AE: a mine that goes round a track (BombCycle): mX1, mY1 is where it is put
    ePlate,       // a foot switch: stepping on it does mA to switch mSwitch (0 on, 1 off, 2 the other way)
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
    // A barrier that is a wall while it is shut (a slam door): a rock comes back off it
    bool mSolid = false;
    // A barrier that kills what touches it (an electric wall): its beam is at mX1
    bool mBeam = false;
    s32 mDestLevel = -1;
    s32 mDestPath = -1;
    s32 mDestCamera = -1;
    // A well goes one way while its switch is off and another while it is on. The fields above are for off; these
    // are for on: the place a local well throws to, or the well an express well leads to (its id, its camera, and
    // its level and path if that is not this one)
    s32 mOnA = 0;
    s32 mOnC = 0;
    s32 mOnD = 0;
    s32 mOnDestLevel = -1;
    s32 mOnDestPath = -1;
    s32 mOnDestCamera = -1;
    // The camera it is in (its number), and a name that stays the same from run to run
    s32 mCamera = 0;
    // and where that camera is, as NavGraph::CameraOf numbers them (-1: not said)
    s32 mCell = -1;
    u32 mId = 0;

    bool Contains(s32 x, s32 y) const
    {
        return x >= mX1 && x <= mX2 && y >= mY1 && y <= mY2;
    }
};

struct NavInput final
{
    bool mIsAe = true;
    // The path it is of (-1: not said)
    s32 mPath = -1;
    // The size of a camera in world units, to tell which camera a point is in
    s32 mCamW = 375;
    s32 mCamH = 260;
    std::vector<NavLine> mLines;
    std::vector<NavFeature> mFeatures;
    // The path has a Brew machine: mines go a cluster at a time there (a fart's blast is wide), not one by one
    bool mBrew = false;
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
    ePortal,    // chant the bird portal open and hop into it: he comes out of its exit
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
    // Running jumps one after the other with no stop between: where the second and the third take off (0: none).
    // Only for when something is after him (Bot::EdgeCostFn).
    s32 mX2 = 0;
    s32 mX3 = 0;
    // An edge that goes through a well (stood in, or fallen into): the well's feature, and the value its switch
    // has to have for the well to go this way (-1: either)
    s32 mWell = -1;
    s32 mWhen = -1;
    // The hazard (its feature) that has to be gone before it can be taken: a mine in the cell it leads into
    s32 mClear = -1;
    // The lift stop (its feature) over whose shaft it jumps: only while the lift is away from it
    s32 mShaft = -1;
    // A fall off a floor's end under something too low to walk off under: crouched first, and rolled off
    bool mCrouched = false;
    // A hoist up that ends facing an electric wall (its feature) in the next cell: only while that is off. He
    // reaches into it as he pulls himself up and is fried (the Barracks' first path, at 1812,1660).
    s32 mFence = -1;
    // A fall that ends hung from a ledge under where he went off, not stood on the floor it is the end of
    bool mHung = false;
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
    // The same for one who stands still in it (his box is narrower then)
    bool mHiddenStill = false;
    // Standing here he is wholly in a Z cover: the Sligs in the background don't see him
    bool mZCover = false;
    // Crouched here he is: under something too low to stand behind
    bool mZLow = false;
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
    // Whether a wall stands between two places of a floor
    bool Walled(s32 xa, s32 xb, s32 y, bool half) const
    {
        return WallBetween(xa, xb, y, half);
    }

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
    // What a running jump up a hoist from a stand costs (AddRunJumps): one more than the others of its kind, by
    // which it is told from them. It is a way of getting off a floor in a hurry, and not the crossing a plan
    // names where there is another (Domain).
    static const s32 kStandJumpCost = 401;
    // A running jump off a floor's end down to a floor under it: dear, the way only at a run from Slogs
    static constexpr s32 kRunDownCost = 3400;
    s32 CameraOf(s32 x, s32 y) const;
    // His box (from x + lo to x + hi, `tall` up from the floor at y) is wholly inside a Z cover
    bool ZCovered(s32 x, s32 y, s32 lo, s32 hi, s32 tall) const;

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
    // A hoist's top is at this cell: down held here climbs down it
    bool HoistTopAt(const NavNode& n) const;
    bool CeilingOver(s32 x, s32 y, bool half) const;
    // overLift: the jumps there are over a lift's shaft when the lift is away (the edge's feature is the stop)
    void AddRunJumps(std::vector<NavEdge>& edges, s32 from, s32 dir, bool overLift = false) const;
    // From a ledge too short for that run-up: over a gap to the hoist at the edge of the ledge across, which he
    // catches and pulls himself up by
    // Whether that edge or hoist is one a running jump off `takeoff` at height y catches on its way up
    bool CatchesUp(const NavFeature& f, s32 takeoff, s32 y, bool half, s32 dir) const;
    void AddLedgeCatches(std::vector<NavEdge>& edges, s32 from, s32 dir) const;
    s32 BarrierBetween(s32 xa, s32 xb, s32 y, bool half) const;
    const NavFeature* FeatureAt(FeatureKind kind, s32 x, s32 y, bool half, s32* index = nullptr) const;
    void AddEdge(std::vector<NavEdge>& edges, s32 from, s32 to, EdgeKind kind, s32 cost, s32 feature, bool left) const;
    s32 NodeOnLine(s32 line, s32 x) const;
    // The cell a local well throws Abe to: under where it aims, or the ledge just beyond that he catches
    s32 WellLanding(const NavFeature& well) const;
    // Where Abe ends up thrown at a point from a well at fromX: into the well that is there, or onto the floor
    // (knownSwitch, knownValue: the switch the first well of a chain was taken with, and its value, for the wells
    // after it that go by the same one)
    s32 Thrown(s32 x, s32 y, s32 fromX, bool half, s32 depth, s32 knownSwitch = -1, s32 knownValue = 0) const;
    // Where a well lets him out, with its switch off (0) or on (1), followed through the wells it leads to and
    // throws him into: a node, or -1 (nowhere known, or out of the path)
    s32 WellOut(s32 featureIdx, s32 when, s32 depth = 0, s32 knownSwitch = -1, s32 knownValue = 0) const;
    // The edges through a well: one, or one for its switch off and one for on if that changes where he comes out
    void AddWellEdges(std::vector<NavEdge>& edges, s32 from, s32 wellIdx, EdgeKind kind, s32 cost, s32 feature, bool left) const;
    // The local well a fall straight down at x goes through in the air between two heights: its feature or -1
    s32 WellInFall(s32 x, s32 fromY, s32 toY, bool half) const;

    NavInput mInput;
    std::vector<NavNode> mNodes;
    std::vector<NavEdge> mEdges;
    std::vector<NavExit> mExits;
    // Per line: its first node and how many (in x order)
    std::vector<s32> mLineFirst;
    std::vector<s32> mLineCount;
};

} // namespace Bot
