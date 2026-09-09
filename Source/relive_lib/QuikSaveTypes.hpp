#pragma once

#include "SerializedObjectData.hpp"
#include "MapWrapper.hpp"

class BaseMap;

// The part of a quicksave's world info that's genuinely the same concept in
// both games: where the player is, run/progress counters, and enough to
// preview a save file in a menu. Each engine's own Quicksave_WorldInfo
// extends this with whatever extra progression tracking is unique to it
// (AE's zulags/visited levels/meter bars have no AO equivalent).
struct QuicksaveWorldInfoBase
{
    s32 mGnFrame = 0;
    EReliveLevelIds mLevel = EReliveLevelIds::eNone;
    s16 mPath = 0;
    s16 mCam = 0;
    s16 mSaveFileId = 0;
    s16 mControlledCharX = 0;
    s16 mControlledCharY = 0;
    bool mControlledCharAtFullScale = false;
    s16 mRescuedMudokons = 0;
    s16 mKilledMudokons = 0;
    s32 mGasTimer = 0;
    bool mAbeInvincible = false;
};

// The TLV bly-flag portion of a quicksave. Which TLVs participate is driven
// by relive::QuiksaveAttribute on each TLV, which lives on the one shared
// TLV struct definitions both engines use - so this part of the save format
// is genuinely identical between AO and AE.
//
// Object state (enemies, mudokons, etc.) is NOT part of this: only AE's
// game objects currently implement VGetSaveState/CreateFromSaveState, so
// that half of a quicksave stays engine specific for now.
struct PendingObjectRestoreData
{
    SerializedObjectData mObjectsStateData;
    SerializedObjectData mObjectBlyData;
};
