#pragma once

#include "../AliveLibAO/PathData.hpp" // For AO::LevelIds
#include "../AliveLibAE/PathData.hpp"

#include "../MapWrapper.hpp"
#include "nlohmann/json_fwd.hpp"

#include "ConversionProgress.hpp"
#include "file_system.hpp"
#include <optional>

class ThreadPool;

bool SaveJson(const nlohmann::json& j, FileSystem& fs, const FileSystem::Path& path);
bool SaveJson(const nlohmann::json& j, FileSystem& fs, const char_type* path);

inline const char* ToString(AO::LevelIds lvlId)
{
    switch (lvlId)
    {
        case AO::LevelIds::eMenu_0:
            return "menu";
        case AO::LevelIds::eRuptureFarms_1:
            return "rupture_farms";
        case AO::LevelIds::eLines_2:
            return "monsaic_lines";
        case AO::LevelIds::eForest_3:
            return "paramonia";
        case AO::LevelIds::eForestTemple_4:
            return "paramonia_temple";
        case AO::LevelIds::eStockYards_5:
            return "stock_yards";
        case AO::LevelIds::eStockYardsReturn_6:
            return "stock_yards_return";
        case AO::LevelIds::eDesert_8:
            return "scrabania";
        case AO::LevelIds::eDesertTemple_9:
            return "scrabania_temple";
        case AO::LevelIds::eCredits_10:
            return "credits";
        case AO::LevelIds::eBoardRoom_12:
            return "board_room";
        case AO::LevelIds::eRuptureFarmsReturn_13:
            return "rupture_farms_return";
        case AO::LevelIds::eForestChase_14:
            return "paramonia_temple_escape";
        case AO::LevelIds::eDesertEscape_15:
            return "scrabania_temple_escape";
        default:
            return "unknown";
    }
}


inline const char* ToString(::LevelIds lvlId)
{
    switch (lvlId)
    {
        case ::LevelIds::eMenu_0:
            return "menu";
        case ::LevelIds::eMines_1:
            return "mines";
        case ::LevelIds::eNecrum_2:
            return "necrum";
        case ::LevelIds::eMudomoVault_3:
            return "mudomo_vault";
        case ::LevelIds::eMudomoVault_Ender_11:
            return "mudomo_vault_ender";
        case ::LevelIds::eMudancheeVault_4:
            return "mudanchee_vault";
        case ::LevelIds::eMudancheeVault_Ender_7:
            return "mudanchee_vault_ender";
        case ::LevelIds::eFeeCoDepot_5:
            return "feeco_depot";
        case ::LevelIds::eFeeCoDepot_Ender_12:
            return "feeco_depot_ender";
        case ::LevelIds::eBarracks_6:
            return "barracks";
        case ::LevelIds::eBarracks_Ender_13:
            return "barracks_ender";
        case ::LevelIds::eBonewerkz_8:
            return "bonewerkz";
        case ::LevelIds::eBonewerkz_Ender_14:
            return "bonewerkz_ender";
        case ::LevelIds::eBrewery_9:
            return "brewery";
        case ::LevelIds::eBrewery_Ender_10:
            return "brewery_ender";
        case ::LevelIds::eTestLevel_15:
            return "test_level";
        case ::LevelIds::eCredits_16:
            return "credits";
        default:
            return "unknown";
    }
}

class [[nodiscard]] DataConversion final
{
public:
    DataConversion();
    ~DataConversion();

    bool AsyncTasksInProgress() const;

    // Cooperative cancellation for jobs still running in the background - see
    // ThreadPool::RequestCancel(). Used to stop long-running conversion (FMVs specifically, so
    // far) from leaving a corrupt/incomplete file behind if the user quits mid-conversion - see
    // Engine.cpp's quit handling, which calls this (via ActiveDataConversion()) before exiting.
    void RequestCancel();
    [[nodiscard]] bool IsCancelRequested() const;

    // Weighted overall progress (paths/animations/cameras/misc/fmvs) - see
    // ConversionProgress.hpp. Reflects every conversion category, including the ones
    // (paths/animations/palettes/saves/fonts/demos) that convert synchronously and never go
    // through ThreadPool::AddJob - unlike ThreadPool's own job counters, which only ever
    // reflected cameras+fmvs and were removed from here as redundant once this existed.
    [[nodiscard]] ConversionProgress::Snapshot ProgressSnapshot() const;

    struct [[nodiscard]] DataVersions final
    {
    private:
        // Bump this if any data format breaks are made so that OG/mod data is re-converted/upgraded
        // 2: encode periodic keyframes instead of only ever forcing one at frame 0 - fixes
        // external players being unable to seek partway into an FMV.
        // 3: tightened the keyframe cadence from ~2s to ~1s, matching normal seek-to-the-second
        // expectations - negligible size cost at this resolution/framerate.
        // 4: audio track switched from raw A_PCM to A_VORBIS (much smaller files, and fixes the
        // ".DDV.webm" naming bug where the audio codec ID didn't matter yet).
        static constexpr u32 kFmvVersion = 4;
        // 17: dropped the redundant "paths" subdir under each level (levels/<name>/<pathId>/...
        // instead of levels/<name>/paths/<pathId>/...) and lower-cased "Sounds" to "sounds".
        // 18: moved vh_file/vb_file/seq_files out of every path.json's own sound_info (where
        // changing sound_theme without also hand-updating them left them pointing at the
        // previous theme's files) into one sounds/<theme>/sound_info.json per theme; also
        // split the AE ender themes (barracks/bonewerkz/feeco_depot/mudanchee_vault/
        // mudomo_vault) that used to share their base theme's dir under two different vb/vh
        // pairs into their own "<theme>_ender" theme dirs, matching brewery/brewery_ender.
        // 19: FMV ids on TLVs (door/teleporter/path_transition/bird_portal/movie_stone/
        // well_express/level_loader/glukkon) are now resolved movie name strings instead of
        // raw numeric indices into a level's FMV table - see relive::Path_Door::mMovieName etc.
        // 20: door/teleporter/well_express/level_loader's raw mMovieId can be a packed
        // double/triple FMV chain (same convention as bird_portal/path_transition, decoded via
        // the original FMV_Camera_Change's >100/>10000 thresholds) - not just a single index as
        // 19 assumed, which left multi-movie ids resolving out of range (blank movie name).
        // These 4 TLVs now have mMovie1/2/3 like bird_portal/path_transition instead of a single
        // mMovieName - see DecodeFmvChain_AE/AO.
        static constexpr u32 kPathVersion = 20;
        static constexpr u32 kPaletteVersion = 1;
        static constexpr u32 kAnimationVersion = 4;
        // 9: dropped the redundant "paths" subdir under each level, same as kPathVersion 17.
        static constexpr u32 kCameraVersion = 9;
        static constexpr u32 kSaveFileVersion = 3;
        static constexpr u32 kFontFileVersion = 1;
        static constexpr u32 kDemoFileVersion = 2;

        static constexpr char_type kDataVersionFileName[] = "data_version.json";
    public:
        static DataVersions LatestVersion()
        {
            DataVersions dv;
            dv.mFmvVersion = kFmvVersion;
            dv.mPathVersion = kPathVersion;
            dv.mPaletteVersion = kPaletteVersion;
            dv.mAnimationVersion = kAnimationVersion;
            dv.mCameraVersion = kCameraVersion;
            dv.mSaveFileVersion = kSaveFileVersion;
            dv.mFontFileVersion = kFontFileVersion;
            dv.mDemoFileVersion = kDemoFileVersion;
            return dv;
        }

        u32 mFmvVersion = 0;
        u32 mPathVersion = 0;
        u32 mPaletteVersion = 0;
        u32 mAnimationVersion = 0;
        u32 mCameraVersion = 0;
        u32 mSaveFileVersion = 0;
        u32 mFontFileVersion = 0;
        u32 mDemoFileVersion = 0;

        void Save(const FileSystem::Path& dataDir) const;

        bool Load(const FileSystem::Path& dataDir);

        bool ConvertFmvs() const
        {
            return mFmvVersion != kFmvVersion;
        }

        bool ConvertPaths() const
        {
            return mPathVersion != kPathVersion;
        }

        bool ConvertPalettes() const
        {
            return mPaletteVersion != kPaletteVersion;
        }

        bool ConvertAnimations() const
        {
            return mAnimationVersion != kAnimationVersion;
        }

        bool ConvertCameras() const
        {
            return mCameraVersion != kCameraVersion;
        }

        bool ConvertSaves() const
        {
            return mSaveFileVersion != kSaveFileVersion;
        }

        bool ConvertFonts() const
        {
            return mFontFileVersion != kFontFileVersion;
        }

        bool ConvertDemos() const
        {
            return mDemoFileVersion != kDemoFileVersion;
        }

        bool AnyConversionRequired() const
        {
            return ConvertFmvs() || ConvertPaths() || ConvertPalettes() || ConvertAnimations() || ConvertCameras() || ConvertSaves() || ConvertFonts() || ConvertDemos();
        }
    };

    std::optional<DataVersions> DataVersionAO();
    std::optional<DataVersions> DataVersionAE();

    void ConvertDataAO(const DataVersions& dv);
    void ConvertDataAE(const DataVersions& dv);

private:
    std::unique_ptr<ThreadPool> mThreadPool;
    ConversionProgress mProgress;
};
