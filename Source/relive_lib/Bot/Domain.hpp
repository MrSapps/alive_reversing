#pragma once

#include "Goap.hpp"
#include "NavGraph.hpp"
#include "World.hpp"
#include "FlierFlight.hpp"
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
// Abe has meat to throw (what a path's sacks give is the one or the other)
static const u32 kMeat = 6;
// Meat lies (or flies) on his screen: what the Paramites that team up over the way out go for instead of him
static const u32 kPackFed = 7;
// The foot switch (its entity) he last stepped on to turn a gate over, with nothing done since
static const u32 kPlate = 8;
inline u32 Switch(s32 id)
{
    return 1000 + static_cast<u32>(id);
}
// A Mudokon is saved (1) or dead (2)
inline u32 Mud(u32 entityId)
{
    return 1000000 + entityId;
}
// He has drunk a Brew: his next fart is one that goes off
static const u32 kBrew = 9;
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
    eDisarm,    // turn UXB mEntity off with a slap, crouched in the cell mEdge beside it
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
    eMeatUse,   // throw meat to draw Paramite mOther off entity mEntity, then use it
    eFleechLure, // lead the Fleeches that guard entity mEntity off and use it
    eSligDrop,  // at lever mEntity until Slig mOther walks onto the trap door (feature mEdge) the lever opens,
                // then pull it
    eStepOn,    // onto the cell mEdge, where a foot switch that turns a gate over is
    eParamiteClear, // take over Paramite mOther from the cell mEdge, have it eat the Fleeches of its floors, call
                // the other Paramites of those floors to follow it, and leave them all at the end away from
                // the way out
    eParamiteTour, // take over Paramite mOther from the cell mEdge and have it pull every ring it can get to
                // (Domain::ParamiteTour), the nearest first, and let it go under the last
    eMeatFeed,  // on the way to the cell mEdge, from the last cell before the level of Paramites that team up:
                // throw meat onto their level away from the way, and wait until they are off after it
    eFleechPen, // pull lever mEntity, which wakes Fleeches, lead them over the top to behind the gate (feature
                // mOther) that one foot switch opens and another shuts, and shut it on them
    eFleechLead, // let Fleeches out with the lever mEntity, lead them to where they drop to a Scrab and kill them with it
    eScrabPen,  // work lever mEntity so that the gate it moves (feature mOther) is shut with every Scrab of that
                // floor on one side of it: left of it if mEdge is 1, else right
    eFartBomb,  // drink at Brew machine mOther, fart, take the fart over and fly it to mine or Slig mEntity. With
                // no machine (mOther 0): from the cell mEdge with the Brew he has in him, at flying Slig mEntity
    eDrink,     // drink at Brew machine mEntity
    eSligLock,  // call Slig mOther with lever mEdge (an entity id), take it over, walk it to Slig lock mEntity
                // and say the lock's code with it
    eFlierBomb, // from the cell mEdge take over the flying Slig that spawner mEntity makes and throw its grenades
                // at the Sligs it can get at (Domain::FlierTargets)
    eFlierPull, // from the cell mEdge take over the flying Slig that spawner mEntity makes and pull lever mOther
    eFlierLock, // the same, and fly it to Slig lock mOther and say the lock's code back to it
                // with it
    eGrenade,   // throw a grenade at Slig mEntity from the stretch of floor of the cell mEdge
                // (Domain::GrenadeTargets)
    eCrawlerButton, // from the cell mEdge take over a naked Slig and press the button mEntity with it
    eSligLever,     // the Glukkon he is in goes to the cell mEdge and has a Slig it calls pull the lever mEntity
    eVoiceLock,     // from the cell mEdge (-1: he has one already) take over a Glukkon and answer the voice lock mEntity
                    // (Domain::CrawlerFor)
    eGrenadeOrb, // hold a grenade and throw it late from the cell mEdge, to go off in the air by security orb
                 // mEntity (Domain::OrbTargets)
    eZone,      // into the cell mEdge, which is in a zone that sets switch mOther when he is in it
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
    // The wheels no Mudokon stood at when it was planned, and where the Mudokons brought to them come onto
    // their floor
    std::vector<u32> mFree;
    s32 mFromX = 0;
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
    // A Slig that a flying Slig Abe has taken over can throw a grenade at: one he can't take over himself (a
    // security orb on its screen), with a place in the air off its level to throw from that can be flown to
    // from where the flying Slig is made. Where it stands and its floor, and those places, the likeliest first.
    struct FlierTarget final
    {
        u32 mSlig = 0;
        s32 mX = 0;
        s32 mFloorY = 0;
        // The places to throw from that can be flown to as things are
        std::vector<FlierFlight::Shot> mShots;
        // With none: the switches (and what each has to be) that open the way to one
        std::vector<std::pair<s32, s32>> mNeeds;
    };
    static std::vector<FlierTarget> FlierTargets(const NavInput& in, const World& world, const Entity& spawner);
    // A Slig that a grenade from a boom machine is thrown at: one he can't take over (a security orb on its
    // screen) on a floor with no shadow to go by it in, in a path with such a machine. The stretch of its floor
    // that is on its screen, and where a grenade thrown comes to lie on that stretch (ThrowArc::Grenade): from
    // a cell of another floor, far enough from where it goes off, the cells he isn't seen in first.
    struct GrenadeShot final
    {
        // The cell thrown from (its middle, its floor), and how
        s32 mX = 0;
        s32 mY = 0;
        bool mFaceLeft = false;
        ThrowSpot::Kind mKind = ThrowSpot::eUp;
        // Where it goes off
        s32 mLandX = 0;
        // He isn't seen in that cell (an invisible zone)
        bool mHidden = false;
    };
    struct GrenadeTarget final
    {
        u32 mSlig = 0;
        s32 mFloorY = 0;
        s32 mX1 = 0;
        s32 mX2 = 0;
        // All of its floor
        s32 mLineX1 = 0;
        s32 mLineX2 = 0;
        std::vector<GrenadeShot> mShots;
    };
    static std::vector<GrenadeTarget> GrenadeTargets(const NavInput& in, const World& world);
    // A security orb that a grenade is to go off by. An orb hangs over the floor, higher than the blast of a
    // grenade that lies on it reaches (AirExplosion.cpp: 60 up from 5 over the grenade), so the grenade has to
    // go off in the air: its fuse (Grenade.cpp: 90 frames) runs from when he takes it in his hand, and he holds
    // it for some of that before he throws. (The walkthrough: "Throw the grenade forward on the fifth beep to
    // blow up the Chant Protector".)
    struct OrbShot final
    {
        // The cell thrown from (its middle, its floor), and how
        s32 mX = 0;
        s32 mY = 0;
        bool mFaceLeft = false;
        ThrowSpot::Kind mKind = ThrowSpot::eUp;
        // The frames it is held for longer than a throw at once
        s32 mHold = 0;
        // Where it goes off
        s32 mAtX = 0;
        s32 mAtY = 0;
        bool mHidden = false;
    };
    struct OrbTarget final
    {
        u32 mOrb = 0;
        std::vector<OrbShot> mShots;
    };
    static std::vector<OrbTarget> OrbTargets(const NavInput& in, const World& world);
    // Whether a naked Slig is on the floor of a button for them with nothing shut between the two: taken over,
    // it crawls there and presses it. x: where such a one is.
    static bool CrawlerFor(const NavInput& in, const World& world, const Entity& button, s32* x = nullptr);
    // The way a Glukkon that is taken over has from (x, y) into a voice lock's box: along floors a cell at a time,
    // off the end of one down to the next (no further than 200), through a teleporter, and in a jump over mines
    // or a gap of up to three cells. needs: the switches that have to be at a value for it that are not now (a
    // gate on the way, a teleporter's). nodes: the cells of the way, the first the one it is in; hops: whether
    // the step into each is a jump.
    static constexpr s32 kGlukkonJump = 4;
    static bool GlukkonWay(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, const Entity& lock, std::vector<Goap::Fact>* needs = nullptr, std::vector<s32>* nodes = nullptr,
                           std::vector<bool>* hops = nullptr);
    // The same to any cell with its middle in x1..x2 on the floor at toY. A mine on the way has to be gone first
    // and a trap door it is to drop through open: those are needs as a gate is.
    static bool GlukkonWayTo(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, s32 x1, s32 x2, s32 toY, std::vector<Goap::Fact>* needs = nullptr, std::vector<s32>* nodes = nullptr,
                             std::vector<bool>* hops = nullptr);
    // Nothing solid that is shut between two places of a floor (a slam door)
    static bool FloorOpen(const NavInput& in, const World& world, s32 x1, s32 x2, s32 y, bool half);
    // Whether a grenade that goes off there gets an orb at (x, y). By what was seen: one that lay on the floor 60
    // under it did not, one that went off in the air a little over that floor did. The blast is 60 up and 30 down
    // from 5 over the grenade.
    static bool BlastGetsOrb(s32 atX, s32 atY, s32 orbX, s32 orbY, bool half);
    // How near where a grenade goes off a Slig is sure to be in its blast (the blast is 60 to each side and a
    // Slig 24), and how far from it a cell has to be to throw from (ThrowArc::kGrenadeKillX)
    static const s32 kGrenadeSure = 60;
    // How far up or down from such a Slig's floor a cell counts as on it
    static const s32 kGrenadeFloorY = 30;
    // A way through the air for a flying Slig (FartFlight::Route) that keeps to the screens the path has:
    // those with something of its floors and walls in them. Past the last screen there is nothing to stop a
    // way on the grid, and nothing to fly in either.
    static bool FlierWay(const NavInput& in, bool half, s32 fromX, s32 fromY, s32 toX, s32 toY, const std::vector<NavLine>* shut, std::vector<std::pair<s32, s32>>& corners);
    // How near such a Slig, along its floor and up or down from it, a cell is one it shoots him in
    static const s32 kFlierTargetX = 250;
    static const s32 kFlierTargetY = 30;
    // Where Abe takes the flying Slig of a spawner over from: of the cells he can get to (reach, by cell), one on
    // its screen where he isn't seen, or else the one furthest from its track. With no security orb on that
    // screen. -1: none.
    // (ownOnly: only where the spawner's own track goes, for one that has still to be turned on. A cell with a
    // floor, ceiling or wall between it and all of the track is taken first: it is not seen there,
    // FlyingSlig::IsWallBetween.)
    static s32 FlierSpot(const NavGraph& nav, const World& world, const Entity& spawner, const std::vector<u8>& reach, bool ownOnly = false);
    // Whether a floor, ceiling or wall of that scale is between two points
    // (flying: and the lines that only stop what flies, type 17)
    static bool SightBlocked(const NavInput& in, bool half, s32 x1, s32 y1, s32 x2, s32 y2, bool flying = false);
    // Whether a cell is on that Slig's floor, near enough for it to shoot him there
    static bool OnFlierTargetsFloor(const NavGraph& nav, const FlierTarget& target, s32 node);

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
    // Whether two or more Paramites are on the floor at x, y with no wall between them and there: together they
    // hunt him along it, faster than he runs (alone one keeps off him)
    static bool ParamitePack(const NavGraph& nav, const World& world, s32 x, s32 y, bool half);
    // A Paramite on its own that he would have against the end of its floor there: it turns on him
    static bool ParamiteCornered(const NavGraph& nav, const World& world, s32 x, s32 y, bool half);
    // A stretch of the way of a Paramite that is played: along floors to a cell, up or down a web (mX: the
    // web's), a hop over a gap of up to three cells, or off the end of a floor to the one under it
    struct ParaLeg final
    {
        enum Kind : u8
        {
            eWalk,
            eWebUp,
            eWebDown,
            eHop,
            eFall,
        };
        Kind mKind = eWalk;
        s32 mTo = -1;
        s32 mX = 0;
    };
    // The way from one cell to another for a Paramite that is played (Paramite.cpp: it walks off the ends of
    // floors, grabs a web it walks, hops or falls into, climbs it while up or down is held and steps off at its
    // ends, and hops four cells). A lift's stop is floor only while the lift is at it. cost: in cells, about.
    static bool ParamiteRoute(const NavGraph& nav, const World& world, s32 from, s32 to, std::vector<ParaLeg>* legs, s32* cost = nullptr);
    // The rings a Paramite at a cell can be walked to and has not pulled, the nearest first and each next from
    // the one before
    static std::vector<u32> ParamiteTour(const NavGraph& nav, const World& world, s32 from);
    // The cell a Paramite can stand in at a place, or -1: floor there, no shut gate in it, and if it is a lift's
    // stop the lift at it
    // A wall between two places that stops a Paramite: one that spans 20 over its feet, 10 at half size
    // (Paramite.cpp's WallHit). Lower than what stops a Scrab: it goes under blocks he has to roll under.
    static bool ParamiteWall(const NavGraph& nav, s32 x1, s32 x2, s32 y, bool half);
    static s32 ParamiteFloor(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, s32 tolerance = 8);
    // A cell on the level of Paramites that team up, by where the level has them (they are only about once he is
    // on their screen): two or more of his size on its screen, one at least on its level. Paramite.cpp,
    // AnotherParamiteNear: with another on the screen, one that finds him on its level goes for him and the
    // others with it, over a gap as well (measured: the Mudomo vault's path 4, up the hoist at 862,400 between
    // two of them a lift shaft apart, dead 20 frames later).
    static bool PackLevel(const NavGraph& nav, const World& world, s32 node);
    // A lever that wakes Fleeches, with a gate on its floor that one foot switch beyond it opens and one on the
    // lever's side of it shuts: the Fleeches follow him to the first and are shut in as he comes back over the
    // second (the walkthrough's way with the fifth Mudomo vault: "Walk right off the top level onto the pressure
    // plate, which drops the gate. Once the Fleeches are coming down to meet you, run left over the other pressure
    // plate to trap them"). gate: its feature. open, shut: the cells of the two switches. group: the Fleeches.
    static bool FleechPen(const NavGraph& nav, const World& world, const Entity& lever, s32* gate, s32* open, s32* shut, std::vector<u32>* group);
    // Whether a loose Scrab or a pack of Paramites gets at him at x, y if he is seen
    static bool HuntedAt(const NavGraph& nav, const World& world, s32 x, s32 y, bool half)
    {
        return ScrabReaches(nav, world, x, y, half, false) || ParamitePack(nav, world, x, y, half) || ParamiteCornered(nav, world, x, y, half);
    }
    // Whether a Paramite on the floor of the cell from can get up a web to the floor of the cell to (webX: where
    // the web hangs)
    static bool WebUp(const NavGraph& nav, s32 from, s32 to, s32* webX);
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
    // The floor (its line) an awake Slig of the map walks that the cell is on (one not gone, not asleep, not a
    // spawner's), or -1
    static s32 SligBeat(const NavGraph& nav, const World& world, s32 node);
    // What a crossing onto such a floor from off it (or from another's) costs more than its move: a way that
    // keeps off it is taken where there is one about as short
    static const s32 kSligBeatCost = 400;
    // Whether the cell at (x, y) is on a floor a Slig that is awake watches, with no shadow on that floor on its
    // screen to go by: there he is shot wherever he stands, so it is a place to be only unseen. (A Slig sees who
    // is on its floor and its screen.) Only in a path where the power to go unseen is to be had: elsewhere such a
    // floor is crossed behind the Sligs' backs, as it always was.
    // watchers: the Sligs (entities) that watch it are added. anon: set when one of them is not one of the map's.
    static bool SligWatched(const NavGraph& nav, const World& world, s32 x, s32 y, bool half, std::vector<u32>* watchers = nullptr, bool* anon = nullptr);
    // The way a Slig that Abe has taken over gets from a cell to one of the cells goal says yes to, as edges: it
    // walks (not under a low ceiling), walks off the end of a floor, rides a lift and takes a teleporter, whose
    // lever it pulls itself. It can't jump, climb, crouch, or take a well or a door. Empty with no way, or when
    // it is there.
    // gates: what has to be open on the way (planning: every gate counts as one that can be opened).
    // world: with it and no gates, only what is open now is gone through.
    static std::vector<s32> SligWay(const NavGraph& nav, s32 from, const std::function<bool(s32)>& goal, std::vector<Goap::Fact>* gates, const World* world);
    // The cell a Slig (or one a spawner makes) is on where the map puts it: the floor under it. -1 if none.
    static s32 SligNode(const NavGraph& nav, const Entity& slig);
    // The cells a Slig can say a Slig lock's code from
    static bool AtSligLock(const NavGraph& nav, const Entity& lock, s32 node);
    // The Sligs (entities) a Slig on the cells of a way walks into: those on the same stretches of floor, with
    // no gate or wall between
    static std::vector<u32> SligsMet(const NavGraph& nav, const World& world, s32 from, const std::vector<s32>& way, u32 self);
    // The box a flying Slig's track is in: the track lines joined end to end that go by (fx, fy), where the map puts it.
    // ends: the two ends of that track. False with no track there.
    // (join: how far apart the ends of two stretches may be and still be one track)
    static bool FlierTrack(const NavInput& in, s32 fx, s32 fy, s32& x1, s32& y1, s32& x2, s32& y2, std::vector<std::pair<s32, s32>>* ends = nullptr, std::vector<NavLine>* lines = nullptr, s32 join = 2);
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
