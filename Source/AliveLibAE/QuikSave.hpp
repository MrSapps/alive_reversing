#pragma once

#include "../relive_lib/SwitchStates.hpp"
#include "../relive_lib/FatalError.hpp"
#include "../relive_lib/QuikSaveTypes.hpp"
#include "Abe.hpp"

class BaseMap;
class FileSystem;

enum class EReliveLevelIds : s16;

// AE's own extra progression tracking on top of QuicksaveWorldInfoBase - none
// of this has an AO equivalent (zulags, meter bars, visited-level flags are
// all Exoddus-only concepts).
struct Quicksave_WorldInfo final : QuicksaveWorldInfoBase
{
    s8 field_16_muds_in_area;
    s8 field_17_last_saved_killed_muds_per_path;
    s8 field_18_saved_killed_muds_per_zulag[20];
    s8 field_2C_current_zulag_number;
    s8 mTotalMeterBars;
    s16 field_30_bDrawMeterCountDown;
    s16 mVisitedBonewerkz;
    s16 mVisitedBarracks;
    s16 mVisitedFeecoEnder;
};

// AE's full quicksave format. The object/bly-flag portion (PendingObjectRestoreData)
// is shared with AO; everything else here - world info, the restart-path
// snapshot, Abe's layout - is AE specific.
struct Quicksave final : PendingObjectRestoreData
{
    Quicksave_WorldInfo mWorldInfo;
    Quicksave_WorldInfo mRestartPathWorldInfo;
    AbeSaveState mRestartPathAbeState;
    SwitchStates mRestartPathSwitchStates;
    SwitchStates mSwitchStates;
};

struct SaveFileRec final
{
    char_type mFileName[32];
    u32 mLastWriteTimeStamp;
};

class QuikSave final
{
public:
    static void LoadActive(BaseMap& map);
    static void DoQuicksave(BaseMap& map);
    static void RestoreWorldInfo(const Quicksave_WorldInfo& rInfo);
    static void SaveWorldInfo(Quicksave_WorldInfo* pInfo, BaseMap& map);
    static void FindSaves(FileSystem& fs);
    static void RestoreBlyData(PendingObjectRestoreData& pSaveData, ResourceManagerWrapper& resMan, BaseMap& map);

public:
    static SaveFileRec gSaveFileRecords[128];
    static Quicksave gActiveQuicksaveData;
    static s32 gSavedGameToLoadIdx;
    static s32 gTotalSaveFilesCount;

private:
    static void SaveToMemory_4C91A0(Quicksave& pSave, BaseMap& map);
};
