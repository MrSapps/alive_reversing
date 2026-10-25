#pragma once

#include "../AliveLibAO/PathData.hpp" // For AO::LevelIds
#include "../AliveLibAE/PathData.hpp"

#include "../MapWrapper.hpp"
#include "nlohmann/json_fwd.hpp"

#include "ConversionProgress.hpp"
#include "file_system.hpp"
#include "../GameType.hpp"
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
        // 5: AV1 realtime encode speed 8 -> 6: speeds 7/8 drop the chroma residual on some blocks
        //    (flat desaturated squares mid-movie, e.g. GAMEBGN frame 5).
        // 6: the movies are copied as the game has them (the engine decodes them), not encoded.
        static constexpr u32 kFmvVersion = 6;
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
        // 2: not palettes any more but the change of colours from a sprite's palette to it, see
        // PaletteFit.
        // 3: the fonts' sheets in each palette have a mask (SpriteMask.hpp) instead of special alphas.
        static constexpr u32 kPaletteVersion = 3;
        // 5: AO's green glow (DoorLight) has rounded corners, see RoundAoGreenGlow.
        // 6: sprite sheets are true colour PNGs (RGBA), and the animation's json has its palette_depth.
        // 7: what a palette entry said besides its colour (no STP bit, glowing, left alone by
        // invisibility) is in a mask image next to the sheet, see SpriteMask.hpp.
        // 8: the sheets are packed into pages shared by the animations of a group, see SpritePageWriter.
        // 9: the pages are packed frame by frame instead of sheet by sheet, so they have far fewer holes.
        // 10: the animations' sheets are kept (animation.png/json, for mods and the Blender add-on) and
        // the pages are made from them, with the packed jsons saying where the frames are on a page.
        static constexpr u32 kAnimationVersion = 12;
        // 9: dropped the redundant "paths" subdir under each level, same as kPathVersion 17.
        static constexpr u32 kCameraVersion = 9;
        static constexpr u32 kSaveFileVersion = 3;
        static constexpr u32 kFontFileVersion = 4;
        static constexpr u32 kDemoFileVersion = 2;

        static constexpr char_type kDataVersionFileName[] = "data_version.json";
    public:
        // The biggest sprite page there is, and the smallest one we make, see SpritePageSizeFor
        static constexpr u32 kDefaultSpritePageSize = 2048;
        static constexpr u32 kMinSpritePageSize = 512;

        // The size of the sprite pages for a GPU whose biggest texture is maxTextureSize (0 if
        // that isn't known). A page has its mask under it, so it is half of that at most.
        static u32 SpritePageSizeFor(u32 maxTextureSize)
        {
            if (maxTextureSize == 0)
            {
                return kDefaultSpritePageSize;
            }

            u32 size = kMinSpritePageSize;
            while (size * 2 <= kDefaultSpritePageSize && size * 4 <= maxTextureSize)
            {
                size *= 2;
            }
            return size;
        }

        static DataVersions LatestVersion(u32 spritePageSize = kDefaultSpritePageSize)
        {
            DataVersions dv;
            dv.mSpritePageSize = spritePageSize;
            dv.mWantedSpritePageSize = spritePageSize;
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
        // The size of the sprite pages (0 if there aren't any)
        u32 mSpritePageSize = 0;
        // The size the pages are wanted at, for the GPU: not saved
        u32 mWantedSpritePageSize = kDefaultSpritePageSize;
        // A sheet is newer than the pages (or there are no pages): not saved
        bool mPagesStale = false;

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

        // The pages are made again from the animations' sheets, which doesn't need the game's
        // files: the GPU wants another page size, or a sheet changed
        bool RepackPages() const
        {
            return !ConvertAnimations() && (mSpritePageSize != mWantedSpritePageSize || mPagesStale);
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
            return ConvertFmvs() || ConvertPaths() || ConvertPalettes() || ConvertAnimations() || RepackPages() || ConvertCameras() || ConvertSaves() || ConvertFonts() || ConvertDemos();
        }
    };

    // The versions of the converted data, if there is any. spritePageSize is the size of sprite
    // page this GPU wants: the animations are converted again if the data's pages are another size.
    std::optional<DataVersions> DataVersionAO(u32 spritePageSize = DataVersions::kDefaultSpritePageSize);
    std::optional<DataVersions> DataVersionAE(u32 spritePageSize = DataVersions::kDefaultSpritePageSize);

    // Whether the movies are encoded to webm (-encode_webm) instead of copied
    bool mEncodeWebm = false;

    void ConvertDataAO(const DataVersions& dv);
    void ConvertDataAE(const DataVersions& dv);

    // Converts what data_version.json says is out of date (everything without one), waits for
    // the jobs on the thread pool to finish, then updates data_version.json unless it was
    // cancelled. The first run conversion (DataConversionUI) and relive -convert use this.
    // modLayers are the data directories of the active mod and the games' (see
    // SpritePageWriter::Repack), empty if there is no mod: the mod's sprite pages are made once
    // the game's data is up to date.
    void ConvertAll(GameType game, u32 spritePageSize = DataVersions::kDefaultSpritePageSize, const std::vector<std::string>& modLayers = {}, bool encodeWebm = false);

private:
    std::unique_ptr<ThreadPool> mThreadPool;
    ConversionProgress mProgress;
};
