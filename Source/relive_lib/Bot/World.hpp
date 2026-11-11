#pragma once

#include "../Types.hpp"
#include <vector>

namespace Bot {

enum class EntityKind : u8
{
    eLever,
    ePullRing,
    eWorkWheel,
    eMudokon,
    eBirdPortal,
    // Sets its switch when all its inputs are on (a multi switch controller, a wheel syncer)
    eSwitchLogic,
    eSlig,
    eSecurityOrb,
    // Sets its switch when Abe looks into it
    eHandStone,
    // Laughing gas fills its camera while its switch is on: Mudokons there go wired
    eLaughingGas,
};

const char* EntityKindName(EntityKind kind);

enum class MudMood : u8
{
    eNormal,
    eUpset, // sad or angry: sorry puts it right
    eWired, // laughing gas: a slap puts it right
    eSick,
};

// Something in the path the bot acts on or has to mind. The first part is from the path's TLVs and never changes,
// the second is how it is now.
struct Entity final
{
    EntityKind mKind = EntityKind::eLever;
    // The same from run to run: camera number x 1000 + its place in the camera's TLVs
    u32 mId = 0;
    s32 mCamera = 0;
    // The TLV's rectangle
    s32 mX1 = 0;
    s32 mY1 = 0;
    s32 mX2 = 0;
    s32 mY2 = 0;
    bool mHalf = false;
    // What it sets (a lever, a ring, a wheel, logic), or the switch a rescued Mudokon sets
    s32 mSwitch = 0;
    // 0 sets it, 1 clears it, 2 toggles it
    s32 mOp = 0;
    s32 mInputs[6] = {};
    // A wheel that only counts while it is being turned
    bool mHoldOnly = false;
    // A portal Mudokons can be sent through
    bool mRescuePortal = false;

    // Its TLV says it is gone for good: a Mudokon saved or dead
    bool mGone = false;
    // There is an object for it now, at mLiveX, mLiveY
    bool mLive = false;
    s32 mLiveX = 0;
    s32 mLiveY = 0;
    bool mFollowing = false;
    s32 mState = 0;
    // A live Mudokon's mood
    MudMood mMood = MudMood::eNormal;

    s32 CentreX() const
    {
        return (mX1 + mX2) / 2;
    }
};

// Where a lift's platform is now
struct LiftAt final
{
    s32 mX1 = 0;
    s32 mX2 = 0;
    s32 mY = 0;
};

// A Slig that is about
struct SligAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mFacingLeft = false;
    bool mHalf = false;
    bool mAsleep = false;
    bool mPossessed = false;
    // False for one known only from the path's data: its camera isn't loaded, so it is where it starts
    bool mSeen = true;
};

// A Slog that is about. Awake it runs Abe down wherever he is.
struct SlogAt final
{
    s32 mX = 0;
    s32 mY = 0;
    bool mAsleep = false;
};

struct World final
{
    std::vector<SlogAt> mSlogs;
    std::vector<LiftAt> mLifts;
    std::vector<SligAt> mSligs;
    std::vector<Entity> mEntities;
    // The TLV of each entity, for the reader of the game only
    std::vector<const void*> mTlvs;
    // The path they are from
    const void* mPath = nullptr;
    s8 mSwitches[256] = {};
    s32 mRescued = 0;
    s32 mKilled = 0;

    s32 Switch(s32 id) const
    {
        return id >= 0 && id < 256 ? mSwitches[id] : 0;
    }
    // The Sligs to mind: the ones about, and the ones that will be when their camera loads
    std::vector<SligAt> Threats() const
    {
        std::vector<SligAt> out = mSligs;
        for (const Entity& e : mEntities)
        {
            if (e.mKind != EntityKind::eSlig || e.mGone)
            {
                continue;
            }
            bool seen = false;
            for (const SligAt& s : mSligs)
            {
                const s32 dx = s.mX - e.CentreX();
                const s32 dy = s.mY - e.mY2;
                seen = seen || (dx > -450 && dx < 450 && dy > -80 && dy < 80);
            }
            if (!seen)
            {
                SligAt s;
                s.mX = e.CentreX();
                s.mY = e.mY2;
                s.mHalf = e.mHalf;
                s.mAsleep = e.mState == 1;
                s.mFacingLeft = e.mOp == 1;
                s.mSeen = false;
                out.push_back(s);
            }
        }
        return out;
    }
    const Entity* Find(u32 id) const
    {
        for (const Entity& e : mEntities)
        {
            if (e.mId == id)
            {
                return &e;
            }
        }
        return nullptr;
    }
};

} // namespace Bot
