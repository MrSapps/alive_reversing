#pragma once

#include "Goap.hpp"
#include "NavGraph.hpp"
#include "World.hpp"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Bot {

// The fact keys of the bot's planning problem
namespace Key {
// Which region of the nav graph Abe is in (its number + 1)
static const u32 kRegion = 1;
// The path's exit has been taken
static const u32 kExited = 2;
// Abe has rocks to throw
static const u32 kRocks = 3;
// Abe has (or has had) the power to go invisible that a slap lock of this path gives
static const u32 kInvis = 4;
// He has that power in hand now, or is invisible
static const u32 kInvisNow = 5;
inline u32 Switch(s32 id)
{
    return 1000 + static_cast<u32>(id);
}
// A Mudokon is saved (1) or dead (2)
inline u32 Mud(u32 entityId)
{
    return 1000000 + entityId;
}
// A stretch of floor (Domain::ScrabZone) has no loose Scrab on it that could get at Abe there
inline u32 ScrabClear(s32 zone)
{
    return 3000000 + static_cast<u32>(zone);
}
} // namespace Key

enum class StepKind : u8
{
    eCross,     // take mEdge into another region
    eUse,       // pull the lever or ring mEntity, turn the wheel
    eRescue,    // lead Mudokon mEntity to portal mOther and chant
    eLogic,     // nothing to do: a switch that follows from others
    eExit,      // leave the path by exit mEdge (an index into the graph's exits)
    eTogether,  // turn the wheels of switch logic mEntity at once, with Mudokons
    eKill,      // slap Mudokon mEntity until it is dead
    eRocks,     // jump into rock sack mEntity and pick up what falls out
    eBlow,      // throw rocks at mine mEntity until it goes off
    eSend,      // call blind Mudokon mEntity so that it walks to wheel mOther, and tell it to work
    eRide,      // get into mine car mEntity, drive it to the cell mEdge and get out
    eScrabRampage, // take over a Scrab of the floor Scrab mEntity is on from the cell mEdge, kill every other Scrab
                // and Fleech of that floor with it, walk it into the trap its foot switches make and let it go
    eScrabKill, // take over Scrab mOther from by lever mEdge (an entity id), which opens the gate between it and
                // Fleech mEntity, stamp on the Fleech, walk the Scrab home, let it go and shut the gate
    eScrabHunt, // take over Scrab mEntity from the cell mEdge, walk it to the Fleeches on the floor of its floor and
                // stamp on them, then leave it where a gate can be shut on it and let it go
    eScrabLure, // get the Scrabs of a floor to run off its end: up the hoist to the cell mEdge, seen, down to hang
                // from it while they run at him and over the edge, and up again
    eParamitePull, // take over Paramite mOther from the cell mEdge, walk it under pull ring mEntity, have it pull
                // the ring and let it go
    eFleechLure, // lead the Fleeches that guard entity mEntity off and use it
    eFleechLead, // let Fleeches out with the lever mEntity, lead them to where they drop to a Scrab and kill them with it
    eScrabPen,  // work lever mEntity so that the gate it moves (feature mOther) is shut with every Scrab of that
                // floor on one side of it: left of it if mEdge is 1, else right
};

// What a planned action means to the bot
struct DomainStep final
{
    StepKind mKind = StepKind::eCross;
    s32 mEdge = -1;
    u32 mEntity = 0;
    u32 mOther = 0;
};

// A place to throw a rock at something from, and how
struct ThrowSpot final
{
    enum Kind : s32
    {
        eUp,       // standing, up held: a high arc that comes down a few cells off
        eLob,      // standing, back held: nearly straight up and down
        eForward,  // standing, forward held: far and flat
        eDown,     // standing, down held: at the floor a few cells ahead, and it skips on
        eCrouched, // crouched: low and flat
    };
    s32 mNode = -1;
    Kind mKind = eUp;
    // Tried in this order: the likeliest first
    s32 mOrder = 0;
    // How it lies to the target, for telling what worked: cells away, and -1 below it, 0 level, 1 above
    s32 mCells = 0;
    s32 mLevel = 0;
    // Whether the rock is on the mine whatever frame the mine's light is at. If not it may miss: such places
    // come last.
    bool mSure = true;
    // The switches of the mines that lie between: the rock would set the first of them off instead, so they
    // have to be gone
    std::vector<s32> mNeeds;
    // The slam doors the rock goes through: each one's switch and the value it is open at
    std::vector<std::pair<s32, s32>> mOpen;
    // A drill that goes to and fro where the rock would fly (its entity), or 0: while it runs the rock mostly
    // hits it
    u32 mDrill = 0;
};

// Who turns which wheel when several have to turn at once
struct Team final
{
    std::vector<u32> mWheels;
    std::vector<u32> mMuds;
    // The one left for Abe: the last on the Mudokons' way
    u32 mAbeWheel = 0;
};

// What a path off the way has for a run that wants Mudokons, seen from where Abe would come into it
struct SideTrip final
{
    // Mudokons there at all, in reach or not
    s32 mThere = 0;
    // Mudokons Abe could get to (taking every door as one that can be opened)
    s32 mMudokons = 0;
    // And of those, the ones with a bird portal he could get to as well
    s32 mRescuable = 0;
    // A way out of it that leads on is in reach too
    bool mWayBack = false;
};

// The planning problem of one path as it stands: actions from the nav graph's regions and the world's entities
struct Domain final
{
    std::vector<Goap::Action> mActions;
    // By Action::mTag
    std::vector<DomainStep> mSteps;
    Goap::State mStart;
    std::vector<s32> mRegions;
    // Mudokons a rescue action exists for
    std::vector<u32> mRescuable;

    // edgeUsable: false for edges that turned out not to work. abeNode: where Abe is.
    // kills: with the actions that kill a Mudokon
    static Domain Build(const NavGraph& nav, const World& world, s32 abeNode, const std::function<bool(s32 edge)>& edgeUsable, bool kills = false);
    // The cell Abe uses a lever, ring or wheel from, or stands at for a portal: -1 if there's none. faceLeft is the way to face.
    // firstSide: for a lever, the side of it tried first (-1 left of it, 1 right)
    // The way a blind Mudokon can be walked from a cell to where it would see a portal open (on the portal's
    // floor, no wall between, within a screen): along floors, off their ends, and down through a trap door it
    // stands on. As edges. strict: only what is open and safe as things are now; else also what a switch would
    // open or stop.
    static bool BlindWay(const NavGraph& nav, const World& world, s32 from, const Entity& portal, bool strict, std::vector<s32>& way);
    // Whether a cell is one to call a blind Mudokon from so that it walks to a wheel: on its screen (it hears
    // nothing from another) and on the wheel's side of it
    static bool CallSpot(const NavGraph& nav, const Entity& mud, const Entity& wheel, s32 node);
    // The drill at a cell (one that comes down on it, or goes overhead), or null
    // A sack that hangs over a gap, with no floor under it to jump up from: it is hit by a running jump off the
    // end of a ledge beside it. The cell to start the run from, the take-off cell's x and the way to face.
    static bool SackJump(const NavGraph& nav, const Entity& sack, s32* from, s32* takeoffX, bool* left);
    static const Entity* DrillAt(const NavGraph& nav, const World& world, s32 node, bool high);
    // Whether a Scrab that is loose can get at a cell at (x, floor y): one on that floor with no shut gate between.
    // Not while Abe is invisible or has the power to be.
    // charged: the power not yet used (or its ring on the way) counts as cover too, as it does for finding a route.
    // keep: the shut gates that are all that keeps a Scrab from there (each as the fact that it stays shut);
    // shut: for the Scrabs that can get there, the open gate nearest each, shut; canShut: every one of them has one
    static bool ScrabReaches(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, bool charged = true, std::vector<Goap::Fact>* keep = nullptr, std::vector<Goap::Fact>* shut = nullptr, bool* canShut = nullptr);
    // Whether a gate that is shut now stands on the floor at y between x1 and x2
    static bool GateBetween(const NavGraph& nav, const World& world, s32 x1, s32 x2, s32 y, bool half);
    // The stretch of floor a cell is in, as Scrabs see it: its floor's line, cut at the gates on it. A number that
    // is the same for every cell of the stretch.
    static s32 ScrabZone(const NavGraph& nav, s32 node);
    // The Scrabs that are loose and strike at once (the ones ScrabReaches is about), each with where it is now
    struct LooseScrab final
    {
        u32 mId = 0;
        s32 mX = 0;
        s32 mY = 0;
        bool mHalf = false;
        // The frames it waits before it strikes
        s32 mDelay = 0;
    };
    // waiting: the ones that wait before they strike instead (ScrabReaches leaves those out: they are run from)
    static std::vector<LooseScrab> LooseScrabs(const World& world, bool waiting = false);
    // Whether a wall stands on the floor at y between x1 and x2
    static bool WallBetween(const NavGraph& nav, s32 x1, s32 x2, s32 y, bool half);
    // The cell a Scrab chased to the top of a hoist at the end of its floor comes down on when it runs off that
    // end, or -1 if this is no such hoist top (it is not at an end, there is floor to jump to, a wall stops it)
    static s32 LureLanding(const NavGraph& nav, s32 top);
    // A way to get at something Fleeches guard by leading them off. They come at where he is, not along the way
    // he went, crawl at 3.57 px a frame against his 6.24, go up a hoist after him (some 40 frames) and off the end
    // of a floor, and cannot take a well or a jump. So: he shows himself (mOut, from mBait towards them), goes back
    // to mBait with them after him, and from there to the thing by another way (mBack), which they follow. At
    // mCommit, the last place to turn back from, where they really are decides it. mAway is the way out of their
    // reach after (empty: none found).
    struct FleechLure final
    {
        bool mFound = false;
        s32 mBait = -1;
        std::vector<s32> mOut;
        std::vector<s32> mBack;
        s32 mCommit = -1;
        std::vector<s32> mAway;
        // The frames they lose on him over mBack
        s32 mLead = 0;
        // The switch that has to be set first to let them out, and to what (0: none)
        s32 mSwitch = 0;
        s32 mValue = 0;
    };
    // homes: the cells the Fleeches are on. abe: the frames an edge takes him as things stand (-1: shut).
    // after: the same once the thing is used (a lock that turns a well round).
    // assume: a switch whose barriers are taken to be open (-1: none), to see what letting them out would do.
    // from: where he is (-1: not known): only places he gets to well before they do will do.
    static FleechLure LureFleeches(const NavGraph& nav, const World& world, const std::vector<s32>& homes, s32 target, const NavGraph::EdgeCost& abe, const NavGraph::EdgeCost& after, s32 assume = -1, s32 from = -1);
    // Fleeches let out by a switch and led to a Scrab's floor (the walkthrough's way with the sixth Mudanchee vault:
    // "pull the lever to free the Fleeches. Lead them back across the top. When they drop down to the Scrab,
    // they'll cower in fear. Possess the Scrab and take them out"). They follow him along mWay (edges they can
    // take, from where the switch is set) to mDrop, where he goes on by mOver, a hop they cannot make: they go off
    // the end there and come down at mLand, on the floor of the Scrab mScrab and near enough to it to stay scared.
    // mSpot is where he chants for the Scrab from. mGroup: the Fleeches the switch lets out.
    struct FleechLead final
    {
        bool mFound = false;
        u32 mScrab = 0;
        std::vector<s32> mWay;
        s32 mDrop = -1;
        s32 mOver = -1;
        s32 mLand = -1;
        s32 mSpot = -1;
        std::vector<u32> mGroup;
    };
    // from: the cell the switch is set from. assume: the switch, taken to be on.
    static FleechLead LeadFleeches(const NavGraph& nav, const World& world, s32 from, s32 assume);
    // The frames an edge takes a Fleech (-1: it cannot take it, or a barrier on it is shut), and Abe at a run
    static s32 FleechFrames(const NavGraph& nav, const World& world, const NavEdge& e, s32 assume = -1);
    static s32 RunFrames(const NavGraph& nav, const NavEdge& e);
    // The frames at least each cell is from a Fleech that makes for the cells in zone (100000: it cannot get there)
    // (away: the frames it needs from there to each cell instead)
    static std::vector<s32> FleechTimes(const NavGraph& nav, const World& world, const std::vector<s32>& zone, s32 assume = -1, bool away = false);
    // The cells a Fleech can lick him from while he is on the cell at: its floor within three cells (and one more)
    static std::vector<s32> FleechReach(const NavGraph& nav, s32 at);
    // The switches that have to be on before a lever is pulled, where the order matters (Fleeches: see the .cpp)
    static std::vector<s32> PulledFirst(const NavGraph& nav, const World& world, const Entity& lever);
    static s32 UseNode(const NavGraph& nav, const Entity& entity, bool* faceLeft, s32 firstSide = -1);
    // The wheels a switch logic entity needs turned at once and the nearest Mudokons that can walk to them. False
    // if it isn't that kind of logic or there aren't enough Mudokons. pre gets what the action needs.
    // Where a rock could be thrown from at something on the floor at (x, y), likeliest first. Nothing is worked
    // out about how a rock flies: these are the places worth a try, and the bot keeps what it finds
    // world: to leave out the cells a drill that runs comes by (without it, the cells of any drill)
    static std::vector<ThrowSpot> ThrowSpots(const NavGraph& nav, s32 x, s32 y, bool half, const World* world = nullptr);
    static bool PlanTogether(const NavGraph& nav, const World& world, const Entity& logic, Team& team, std::vector<Goap::Fact>& pre);
    // Weighs a detour into a path: entryNode is where Abe comes in, leadsOn says which of its exits are a way on
    static SideTrip WeighSideTrip(const NavGraph& nav, const World& world, s32 entryNode, const std::function<bool(const NavFeature&)>& leadsOn);
    std::string Text() const;
    // Why a goal has no plan, as far as the regions tell: what is in reach of Abe as things stand, and which
    // barriers on the edge of that have a switch nothing reachable sets
    std::string Frontier() const;
    // The switch and Mudokon facts that aren't true now but something in reach could make true
    std::vector<Goap::Fact> Reachable() const;
    // The regions (as kRegion values) that can be got to from the start, by whatever it takes
    std::vector<s32> ReachableRegions() const;
};

} // namespace Bot
