#pragma once

#include "../Types.hpp"
#include <string>
#include "LevelGraph.hpp"
#include "NavGraph.hpp"
#include "World.hpp"

class BaseMap;
class BaseAliveGameObject;
class BinaryPath;

namespace Bot {

// Pad buttons, the same bits as InputCommands (checked in GameView.cpp), so the game neutral parts of the bot need
// none of the games' headers
namespace Btn {
enum : u32
{
    eUp = 1u << 0,
    eDown = 1u << 1,
    eLeft = 1u << 2,
    eRight = 1u << 3,
    eRun = 1u << 4,
    eAction = 1u << 5,
    eSneak = 1u << 6,
    eThrow = 1u << 7,
    eHop = 1u << 8,
    eFartOrRoll = 1u << 9,
    eSpeak1 = 1u << 10,
    eSpeak2 = 1u << 11,
    eSpeak3 = 1u << 12,
    eSpeak4 = 1u << 13,
    eSpeak5 = 1u << 14,
    eSpeak6 = 1u << 15,
    eSpeak7 = 1u << 16,
    eSpeak8 = 1u << 17,
    eChant = 1u << 18,
    eConfirm = 1u << 20,
};
} // namespace Btn

// What Abe is doing, as far as steering him needs to know. Both games' motions map to these.
enum class Move : u8
{
    eIdle,     // standing still, takes any input
    eCrouch,   // crouched and still
    eHang,     // hanging from a ledge: up climbs, down drops
    eLiftIdle, // holding a lift's rope
    eTurn,
    eWalk,
    eRun,
    eSneak,
    eRoll,
    eAir,  // hopping, jumping or falling
    eDead, // dying or dead
    eBusy, // anything else that has to play out (landing, speaking, a lever, a door, a well...)
};

const char* MoveName(Move move);

struct AbeState final
{
    bool mValid = false;
    // World pixels, and the same in 16.16 fixed point
    s32 mX = 0;
    s32 mY = 0;
    s32 mXFp = 0;
    s32 mYFp = 0;
    s32 mVelXFp = 0;
    s32 mVelYFp = 0;
    bool mFacingLeft = false;
    bool mHalfScale = false;
    Move mMove = Move::eBusy;
    // The game's own motion number and animation frame
    s16 mMotion = 0;
    s16 mAnimFrame = 0;
    bool mOnGround = false;
    // The top of what can be hit of him now (his bounding rectangle)
    s32 mTop = 0;
    // and the rest of it
    s32 mBoxX1 = 0;
    s32 mBoxX2 = 0;
    s32 mBoxBottom = 0;
    s32 mHealthFp = 0;
    s32 mLevel = 0;
    s32 mPath = 0;
    s32 mCamera = 0;
    u32 mFrame = 0;
    // A camera change, a door's fade or a movie is under way: nothing to steer
    bool mTransition = false;

    // Twice the width of a grid cell in pixels (cells are 12.5 pixels at half scale)
    s32 Grid2() const
    {
        return mHalfScale ? 25 : 50;
    }

    bool Settled() const
    {
        return mMove == Move::eIdle || mMove == Move::eCrouch || mMove == Move::eHang || mMove == Move::eLiftIdle;
    }
};

// Reads the game. The only part of the bot that knows the two games' classes.
class GameView final
{
public:
    static AbeState ReadAbe(BaseMap& map);
    static Move ClassifyMotion(bool isAe, s16 motion);
    static std::string LevelName(s32 level);
    // What the nav graph is built from: a path's floors and walls and the TLVs that change where Abe can go.
    // level and pathId are the path's own, to tell a door within it from one that leaves it.
    static NavInput BuildNavInput(BinaryPath& path, s32 level, s32 pathId);
    // The same for the path the game is in
    static NavInput BuildNavInput(BaseMap& map);
    // Every path of the level the game is in
    static std::vector<PathSummary> ReadLevel(BaseMap& map);
    // Another path of the level, as its data has it (nothing of it is about until Abe is there): for weighing
    // whether it is worth going to. False if it isn't loaded.
    static bool ReadPath(BaseMap& map, s32 pathId, NavInput& input, World& world);
    // The entities of the current path (made again if the path has changed) as they are now
    static void ReadWorld(BaseMap& map, World& world);
};

} // namespace Bot
