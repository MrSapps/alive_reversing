#pragma once

#include "../relive_lib/Function.hpp"
#include "../relive_lib/MapWrapper.hpp"
#include "../relive_lib/FmvInfo.hpp"
#include "../relive_lib/PathData.hpp"
#include <vector>
#include <string>

struct CollisionInfo;

enum class LevelIds : s16
{
    eNone = -1,
    eMenu_0 = 0,
    eMines_1 = 1,
    eNecrum_2 = 2,
    eMudomoVault_3 = 3,
    eMudancheeVault_4 = 4,
    eFeeCoDepot_5 = 5,
    eBarracks_6 = 6,
    eMudancheeVault_Ender_7 = 7,
    eBonewerkz_8 = 8,
    eBrewery_9 = 9,
    eBrewery_Ender_10 = 10,
    eMudomoVault_Ender_11 = 11,
    eFeeCoDepot_Ender_12 = 12,
    eBarracks_Ender_13 = 13,
    eBonewerkz_Ender_14 = 14,
    eTestLevel_15 = 15, // Probably test level?
    eCredits_16 = 16
};

struct PathRootContainer final
{
    relive::PathRoot paths[17];
};

struct PerLvlData final
{
    const char_type* displayName;
    EReliveLevelIds level;
    u16 path;
    u16 camera;
    u16 demoId;
    u16 field_C_abe_x_off;
    u16 field_E_abe_y_off;
};

struct OpenSeqHandle final
{
    const char_type* field_0_mBsqName;
    s32 field_4_generated_res_id; // A hash of the named which matches the resource Id
    s8 field_8_sound_block_idx;
    s8 field_9_volume;
    s16 field_A_id_seqOpenId;
    std::vector<u8> field_C_ppSeq_Data;
};
ALIVE_ASSERT_SIZEOF(OpenSeqHandle, 0x10);

struct SeqHandleTable final
{
    OpenSeqHandle mSeqs[145];
};

[[nodiscard]] const relive::PathBlyRec* Path_Get_Bly_Record(EReliveLevelIds lvlId, u16 pathId);

// note: has to be writable
relive::FmvInfoEntry* Path_Get_FMV_Record(EReliveLevelIds lvlId, u16 fmvId);

// All real, unique FMV (.DDV) filenames referenced by any level's FmvInfo table -
// used to drive FMV data conversion without a separately-maintained hardcoded list.
[[nodiscard]] std::vector<std::string> Path_GetAllFmvNames();

void Path_Format_CameraName(char_type* pStrBuffer, EReliveLevelIds levelId, s16 pathId, s16 cameraId);

const char_type* CdLvlName(EReliveLevelIds lvlId);

const char_type* Path_Get_Lvl_Name(EReliveLevelIds lvlId);

s16 Path_Get_Num_Paths(EReliveLevelIds lvlId);

s16 Path_Get_Unknown(EReliveLevelIds lvlId);

const char_type* Path_Get_BndName(EReliveLevelIds lvlId);

// note: has to be writable
relive::SoundBlockInfo* Path_Get_MusicInfo(EReliveLevelIds lvlId);

s16 Path_Get_Reverb(EReliveLevelIds lvlId);

const char_type* Path_Get_BsqFileName(EReliveLevelIds lvlId);

s16 Path_Get_BackGroundMusicId(EReliveLevelIds lvlId);

s32 Path_Get_Paths_Count();

relive::PathRoot* Path_Get_PathRoot(s32 lvlId);

CollisionInfo* GetCollisions(s32 lvlId);

relive::PathData* GetPathData(s32 lvlId);

void Path_SetMudsInLevel(EReliveLevelIds lvlId, u32 pathId, u32 count);

s16 Path_GetMudsInLevel(EReliveLevelIds lvlId, u32 pathId);

extern SeqHandleTable gSeqData;
