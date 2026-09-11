#pragma once

#include "Types.hpp"
#include "FmvInfo.hpp"

struct CollisionInfo;

namespace relive
{
// Shared per-camera path metadata layout for AO/AE (originally two separately reversed
// structs with a handful of extra fields each that turned out to be read nowhere in
// either game - see AliveLibAE/PathData.cpp and AliveLibAO/PathData.cpp for the
// per-game literal tables using this type).
struct PathData final
{
    s16 mLeft;
    s16 mRight;
    s16 mTop;
    s16 mBottom;
    s16 mGridWidth;
    s16 mGridHeight;
    u32 mObjectOffset;
    u32 mObjectIndexTableOffset;
    s16 mAbeStartXPos = 0; // AE: level-select cheat spawn pos, TLV conversion. Unused by AO.
    s16 mAbeStartYPos = 0;
};

struct PathBlyRec final
{
    const char_type* mBlyName;
    PathData* mPathData;
    CollisionInfo* mCollisionData;
    u16 mOverlayId;
    u16 mPadding = 0; // Never read in either game.
};

struct SoundBlockInfo final
{
    const char_type* mVabHeaderName;
    const char_type* mVabBodyName;
    s32 mVabId;
    u8* mVabHeader;
};

struct PathRoot final
{
    PathBlyRec* mBlyArrayPtr;
    FmvInfoEntry* mFmvArray;
    SoundBlockInfo* mMusicInfo;
    const char_type* mBsqFileName;
    s16 mReverb;
    s16 mBgMusicId;
    const char_type* mLvlName;
    s16 mNumPaths;
    s16 mUnused; // message to display to change cd ??
    s32 mOverlayIdx;    // Real, per-level, varying values in both games - not currently
    const char_type* mLvlNameCd; // read anywhere. Likely original CD-swap bookkeeping
    s32 mField24;       // (index/track numbers) that reLIVE's PC-only file access has
    const char_type* mOvlNameCd; // no need to consult. Purpose of field24/field2C not
    s32 mField2C;       // yet confirmed.
    const char_type* mMovNameCd;
    const char_type* mIdxName;
    const char_type* mBndName;
};
} // namespace relive
