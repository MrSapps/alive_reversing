#pragma once

#include "Goap.hpp"
#include "NavGraph.hpp"
#include "World.hpp"
#include <functional>
#include <string>
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
inline u32 Switch(s32 id)
{
    return 1000 + static_cast<u32>(id);
}
// A Mudokon is saved (1) or dead (2)
inline u32 Mud(u32 entityId)
{
    return 1000000 + entityId;
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
    // The switches of the mines that lie between: the rock would set the first of them off instead, so they
    // have to be gone
    std::vector<s32> mNeeds;
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
};

} // namespace Bot
