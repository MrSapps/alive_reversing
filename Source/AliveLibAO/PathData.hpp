#pragma once

#include "../relive_lib/Types.hpp"
#include "../relive_lib/FmvInfo.hpp"
#include "../relive_lib/PathData.hpp"
#include <vector>
#include <string>

enum class EReliveLevelIds : s16;

struct CollisionInfo;

namespace AO {

enum class LevelIds : s16
{
    eNone = -1,
    eMenu_0 = 0,
    eRuptureFarms_1 = 1,
    eLines_2 = 2,
    eForest_3 = 3,
    eForestTemple_4 = 4,
    eStockYards_5 = 5,
    eStockYardsReturn_6 = 6,
    eRemoved_7 = 7,
    eDesert_8 = 8,
    eDesertTemple_9 = 9,
    eCredits_10 = 10,
    eRemoved_11 = 11,
    eBoardRoom_12 = 12,
    eRuptureFarmsReturn_13 = 13,
    eForestChase_14 = 14,
    eDesertEscape_15 = 15,
};

struct Path_TLV;
class Map;

using PathData = relive::PathData;
using PathBlyRec = relive::PathBlyRec;
using FmvInfo = relive::FmvInfoEntry;
using SoundBlockInfo = relive::SoundBlockInfo;
using PathRoot = relive::PathRoot;

struct PathRootContainer final
{
    PathRoot paths[16];
};

const PathBlyRec* Path_Get_Bly_Record(EReliveLevelIds level, u16 path);

FmvInfo* Path_Get_FMV_Record(EReliveLevelIds levelId, u16 fmvId);

// All real, unique FMV (.DDV) filenames referenced by any level's FmvInfo table -
// used to drive FMV data conversion without a separately-maintained hardcoded list.
[[nodiscard]] std::vector<std::string> Path_GetAllFmvNames();

s32 Path_Format_CameraName(char_type* pNameBuffer, EReliveLevelIds level, s16 path, s16 camera);

const char_type* CdLvlName(EReliveLevelIds lvlId);

const char_type* Path_Get_Lvl_Name(EReliveLevelIds lvlId);

s16 Path_Get_Num_Paths(EReliveLevelIds lvlId);

s16 Path_Get_Unknown(EReliveLevelIds lvlId);

const char_type* Path_Get_BndName(EReliveLevelIds lvlId);

// note: has to be writable
SoundBlockInfo* Path_Get_MusicInfo(EReliveLevelIds lvlId);

s16 Path_Get_Reverb(EReliveLevelIds lvlId);

const char_type* Path_Get_BsqFileName(EReliveLevelIds lvlId);

s16 Path_Get_BackGroundMusicId(EReliveLevelIds lvlId);

s32 Path_Get_Paths_Count();

PathRoot* Path_Get_PathRoot(s32 lvlId);

s32 Path_Get_OverlayIdx(EReliveLevelIds lvlId);

CollisionInfo* GetCollisions(s32 lvlId);

PathData* GetPathData(s32 lvlId);

} // namespace AO
