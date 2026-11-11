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
};

// What a planned action means to the bot
struct DomainStep final
{
    StepKind mKind = StepKind::eCross;
    s32 mEdge = -1;
    u32 mEntity = 0;
    u32 mOther = 0;
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
    static s32 UseNode(const NavGraph& nav, const Entity& entity, bool* faceLeft, s32 firstSide = -1);
    // The wheels a switch logic entity needs turned at once and the nearest Mudokons that can walk to them. False
    // if it isn't that kind of logic or there aren't enough Mudokons. pre gets what the action needs.
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
