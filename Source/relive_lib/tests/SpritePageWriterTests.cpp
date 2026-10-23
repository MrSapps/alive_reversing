#include <gtest/gtest.h>
#include "ScratchDir.hpp"
#include "data_conversion/SpritePageWriter.hpp"
#include "data_conversion/PNGFile.hpp"
#include "SpriteMask.hpp"
#include "data_conversion/data_conversion.hpp"
#include <chrono>
#include <filesystem>

// A sheet of one colour with a frame over all of it
static AnimationConverter::Sheet MakeSheet(u32 width, u32 height, RGBA32 colour, bool withMask)
{
    AnimationConverter::Sheet sheet;
    sheet.mWidth = width;
    sheet.mHeight = height;
    sheet.mRgba.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t i = 0; i < sheet.mRgba.size(); i += 4)
    {
        memcpy(&sheet.mRgba[i], &colour, 4);
    }
    if (withMask)
    {
        sheet.mMask.resize(sheet.mRgba.size());
        for (std::size_t i = 0; i < sheet.mMask.size(); i += 4)
        {
            SpriteMask::Set(&sheet.mMask[i], SpriteMask::kKeepVisible);
        }
    }

    PerFrameInfo frame;
    frame.mSpriteWidth = width;
    frame.mSpriteHeight = height;
    frame.mWidth = width;
    frame.mHeight = height;
    sheet.mFrames.push_back(frame);
    sheet.mAttributes.mMaxWidth = width;
    sheet.mAttributes.mMaxHeight = height;
    return sheet;
}

static RGBA32 PixelAt(const std::vector<u8>& rgba, u32 width, u32 x, u32 y)
{
    RGBA32 pixel;
    memcpy(&pixel, &rgba[(static_cast<std::size_t>(y) * width + x) * 4], 4);
    return pixel;
}

// A data directory with the animations/group directory in it
struct PageWriterDir final
{
    PageWriterDir()
        : mDataDir(mDir.Path())
    {
        mAnimDir = mDataDir;
        mAnimDir.Append("animations");
        mAnimDir.Append("group");
        mFs.CreateDirectory(mAnimDir);
    }

    FileSystem::Path AnimPath(const char* name) const
    {
        FileSystem::Path path = mAnimDir;
        path.Append(name);
        return path;
    }

    AnimationAttributesAndFrames Json(const FileSystem::Path& path, const char* extension = ".json")
    {
        return AnimationAttributesAndFrames(mFs.LoadToString((path.GetPath() + extension).c_str()));
    }

    bool LoadPage(const std::string& pageName, std::vector<u8>& page, std::vector<u8>& mask, u32& width, u32& height)
    {
        PNGFile png;
        const std::string pagePath = FileSystem::Path(mDir.Path()).Append(pageName + ".png").GetPath();
        const bool ok = png.LoadSprite(mFs, pagePath.c_str(), page, width, height);
        png.LoadMask(mFs, pagePath, mask, width, height);
        return ok;
    }

    ScratchDir mDir;
    FileSystem mFs;
    FileSystem::Path mDataDir;
    FileSystem::Path mAnimDir;
};

TEST(SpritePageWriter, SheetsOfAGroupShareAPageAndTheirFramesFollow)
{
    PageWriterDir dir;
    const FileSystem::Path pathA = dir.AnimPath("a");
    const FileSystem::Path pathB = dir.AnimPath("b");
    SpritePageWriter::SaveSheet(dir.mFs, pathA, MakeSheet(40, 30, {10, 20, 30, 255}, true));
    SpritePageWriter::SaveSheet(dir.mFs, pathB, MakeSheet(50, 20, {200, 100, 50, 255}, false));
    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 256), 1u);

    AnimationAttributesAndFrames a = dir.Json(pathA, SpritePageWriter::kPackedJsonExtension);
    AnimationAttributesAndFrames b = dir.Json(pathB, SpritePageWriter::kPackedJsonExtension);
    ASSERT_FALSE(a.mAttributes.mSheet.empty());
    EXPECT_EQ(a.mAttributes.mSheet, b.mAttributes.mSheet);

    std::vector<u8> page;
    std::vector<u8> mask;
    u32 width = 0;
    u32 height = 0;
    ASSERT_TRUE(dir.LoadPage(a.mAttributes.mSheet, page, mask, width, height));
    EXPECT_LE(width, 256u);
    EXPECT_LE(height, 256u);

    // The corners of each frame are the sheet's colour, where the packed json says they are
    for (const auto& [sheet, colour, solid] : {std::tuple{&a, RGBA32{10, 20, 30, 255}, true}, std::tuple{&b, RGBA32{200, 100, 50, 255}, false}})
    {
        const PerFrameInfo& frame = sheet->mFrames[0];
        for (const auto& [x, y] : {std::pair{0u, 0u}, {frame.mSpriteWidth - 1, frame.mSpriteHeight - 1}})
        {
            const RGBA32 pixel = PixelAt(page, width, frame.mSpriteSheetX + x, frame.mSpriteSheetY + y);
            EXPECT_EQ(pixel.ToU32(), colour.ToU32());
            const u8 flags = SpriteMask::Get(&mask[(static_cast<std::size_t>(frame.mSpriteSheetY + y) * width + frame.mSpriteSheetX + x) * 4]);
            EXPECT_EQ((flags & SpriteMask::kKeepVisible) != 0, solid);
        }
    }
}

TEST(SpritePageWriter, TheSheetsAreTheSourceAndPackingDoesNotChangeThem)
{
    PageWriterDir dir;
    const FileSystem::Path path = dir.AnimPath("a");
    SpritePageWriter::SaveSheet(dir.mFs, path, MakeSheet(40, 30, {10, 20, 30, 255}, true));
    SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 256);

    // The json and the sheet are as they were saved: no page in the json, the frame in the sheet
    const AnimationAttributesAndFrames source = dir.Json(path);
    EXPECT_TRUE(source.mAttributes.mSheet.empty());
    ASSERT_EQ(source.mFrames.size(), 1u);
    EXPECT_EQ(source.mFrames[0].mSpriteSheetX, 0u);
    EXPECT_EQ(source.mFrames[0].mSpriteSheetY, 0u);
    EXPECT_TRUE(dir.mFs.FileExists((path.GetPath() + ".png").c_str()));
    EXPECT_TRUE(dir.mFs.FileExists(PNGFile::MaskPathFor(path.GetPath() + ".png").c_str()));
}

TEST(SpritePageWriter, PagesAreMadeAgainFromTheSheetsAtAnotherPageSize)
{
    PageWriterDir dir;
    const FileSystem::Path path = dir.AnimPath("a");
    SpritePageWriter::SaveSheet(dir.mFs, path, MakeSheet(40, 30, {10, 20, 30, 255}, false));
    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 256), 1u);
    ASSERT_TRUE(dir.mFs.FileExists((path.GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));

    // A GPU that only takes pages smaller than the sheet: it is used as the sheet it is
    EXPECT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 32), 0u);
    EXPECT_FALSE(dir.mFs.FileExists((path.GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));
    EXPECT_TRUE(dir.Json(path).mAttributes.mSheet.empty());
    EXPECT_TRUE(dir.mFs.FileExists((path.GetPath() + ".png").c_str()));

    // And a bigger GPU later packs it again
    EXPECT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 1024), 1u);
    EXPECT_TRUE(dir.mFs.FileExists((path.GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));
}

TEST(SpritePageWriter, OnlyTheFramesAreOnThePageAndSharedFramesAreWrittenOnce)
{
    PageWriterDir dir;
    const FileSystem::Path path = dir.AnimPath("sparse");

    // A 200 x 200 sheet that is empty but for a red 20 x 10 frame at one corner and a green
    // 10 x 10 frame at the other. A third frame shows the red one again.
    AnimationConverter::Sheet sheet;
    sheet.mWidth = 200;
    sheet.mHeight = 200;
    sheet.mRgba.assign(static_cast<std::size_t>(200) * 200 * 4, 0);
    const auto fill = [&](u32 x0, u32 y0, u32 w, u32 h, RGBA32 colour)
    {
        for (u32 y = y0; y < y0 + h; y++)
        {
            for (u32 x = x0; x < x0 + w; x++)
            {
                memcpy(&sheet.mRgba[(static_cast<std::size_t>(y) * 200 + x) * 4], &colour, 4);
            }
        }
    };
    const RGBA32 red{255, 0, 0, 255};
    const RGBA32 green{0, 255, 0, 255};
    fill(0, 0, 20, 10, red);
    fill(190, 190, 10, 10, green);

    const auto addFrame = [&](u32 x, u32 y, u32 w, u32 h)
    {
        PerFrameInfo frame;
        frame.mSpriteSheetX = x;
        frame.mSpriteSheetY = y;
        frame.mSpriteWidth = w;
        frame.mSpriteHeight = h;
        sheet.mFrames.push_back(frame);
    };
    addFrame(0, 0, 20, 10);
    addFrame(190, 190, 10, 10);
    addFrame(0, 0, 20, 10);

    SpritePageWriter::SaveSheet(dir.mFs, path, sheet);
    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 256), 1u);

    const AnimationAttributesAndFrames anim = dir.Json(path, SpritePageWriter::kPackedJsonExtension);
    std::vector<u8> page;
    std::vector<u8> mask;
    u32 width = 0;
    u32 height = 0;
    ASSERT_TRUE(dir.LoadPage(anim.mAttributes.mSheet, page, mask, width, height));

    // Nothing like the 200 x 200 of the sheet
    EXPECT_LE(width, 64u);
    EXPECT_LE(height, 64u);

    ASSERT_EQ(anim.mFrames.size(), 3u);
    EXPECT_EQ(PixelAt(page, width, anim.mFrames[0].mSpriteSheetX, anim.mFrames[0].mSpriteSheetY).ToU32(), red.ToU32());
    EXPECT_EQ(PixelAt(page, width, anim.mFrames[0].mSpriteSheetX + 19, anim.mFrames[0].mSpriteSheetY + 9).ToU32(), red.ToU32());
    EXPECT_EQ(PixelAt(page, width, anim.mFrames[1].mSpriteSheetX, anim.mFrames[1].mSpriteSheetY).ToU32(), green.ToU32());
    // The same place in the sheet is the same place on the page
    EXPECT_EQ(anim.mFrames[2].mSpriteSheetX, anim.mFrames[0].mSpriteSheetX);
    EXPECT_EQ(anim.mFrames[2].mSpriteSheetY, anim.mFrames[0].mSpriteSheetY);
}

TEST(SpritePageWriter, PagesOfAnEarlierConversionAreRemoved)
{
    PageWriterDir dir;
    FileSystem::Path pagesDir = dir.mDataDir;
    pagesDir.Append(SpritePageWriter::kPagesDirectory);
    dir.mFs.CreateDirectory(pagesDir);
    FileSystem::Path stale = pagesDir;
    stale.Append("old_7.png");
    ASSERT_TRUE(dir.mFs.Save(stale, {1, 2, 3}));

    SpritePageWriter::SaveSheet(dir.mFs, dir.AnimPath("a"), MakeSheet(40, 30, {10, 20, 30, 255}, false));
    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 256), 1u);

    EXPECT_FALSE(dir.mFs.FileExists(stale.GetPath().c_str()));
}

TEST(SpritePageWriter, SheetBiggerThanAPageIsUsedAsASheet)
{
    PageWriterDir dir;
    const FileSystem::Path path = dir.AnimPath("big");
    SpritePageWriter::SaveSheet(dir.mFs, path, MakeSheet(100, 20, {1, 2, 3, 255}, false));
    EXPECT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 64), 0u);

    EXPECT_FALSE(dir.mFs.FileExists((path.GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));
    EXPECT_TRUE(dir.Json(path).mAttributes.mSheet.empty());
    EXPECT_TRUE(dir.mFs.FileExists((path.GetPath() + ".png").c_str()));
}

TEST(SpritePageWriter, PagesAreStaleWithoutPagesOrWithANewerSheet)
{
    PageWriterDir dir;
    // Nothing to pack
    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.mDataDir, 256));

    const FileSystem::Path path = dir.AnimPath("a");
    SpritePageWriter::SaveSheet(dir.mFs, path, MakeSheet(40, 30, {10, 20, 30, 255}, false));
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.mDataDir, 256));

    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 256), 1u);
    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.mDataDir, 256));

    // A sheet that is saved after the pages (by the Blender add-on, say)
    std::filesystem::last_write_time(path.GetPath() + ".png", std::filesystem::file_time_type::clock::now() + std::chrono::hours(1));
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.mDataDir, 256));

    // The packed jsons are made by the packing, not sheets
    SpritePageWriter::Repack(dir.mFs, dir.mDataDir, 256);
    std::filesystem::last_write_time(path.GetPath() + ".png", std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
    std::filesystem::last_write_time(path.GetPath() + SpritePageWriter::kPackedJsonExtension, std::filesystem::file_time_type::clock::now() + std::chrono::hours(1));
    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.mDataDir, 256));
}

// A mod's data directory next to the game's, as the game searches them
struct ModPagesDir final
{
    ModPagesDir()
    {
        mModDir = FileSystem::Path(mDir.Path()).Append("mod");
        mBaseDir = FileSystem::Path(mDir.Path()).Append("base");
        mFs.CreateDirectory(mModDir);
        mFs.CreateDirectory(mBaseDir);
    }

    static FileSystem::Path AnimPath(const FileSystem::Path& dataDir, const char* group, const char* name)
    {
        return FileSystem::Path(dataDir).Append("animations").Append(group).Append(name);
    }

    void Save(const FileSystem::Path& dataDir, const char* group, const char* name, u32 width, u32 height, RGBA32 colour)
    {
        mFs.CreateDirectory(FileSystem::Path(dataDir).Append("animations").Append(group));
        SpritePageWriter::SaveSheet(mFs, AnimPath(dataDir, group, name), MakeSheet(width, height, colour, false));
    }

    std::vector<std::string> Layers() const
    {
        return {mModDir.GetPath(), mBaseDir.GetPath()};
    }

    AnimationAttributesAndFrames Packed(const FileSystem::Path& dataDir, const char* group, const char* name)
    {
        return AnimationAttributesAndFrames(mFs.LoadToString((AnimPath(dataDir, group, name).GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));
    }

    RGBA32 FirstPixel(const FileSystem::Path& dataDir, const AnimationAttributesAndFrames& anim)
    {
        PNGFile png;
        std::vector<u8> page;
        u32 width = 0;
        u32 height = 0;
        const std::string pagePath = FileSystem::Path(dataDir).Append(anim.mAttributes.mSheet + ".png").GetPath();
        EXPECT_TRUE(png.LoadSprite(mFs, pagePath.c_str(), page, width, height));
        return PixelAt(page, width, anim.mFrames[0].mSpriteSheetX, anim.mFrames[0].mSpriteSheetY);
    }

    ScratchDir mDir;
    FileSystem mFs;
    FileSystem::Path mModDir;
    FileSystem::Path mBaseDir;
};

TEST(SpritePageWriter, ModPagesHaveTheGamesAnimationsAndTheModsOverridesOnThem)
{
    ModPagesDir dir;
    const RGBA32 baseColour{10, 20, 30, 255};
    const RGBA32 modColour{200, 100, 50, 255};
    dir.Save(dir.mBaseDir, "group", "a", 40, 30, baseColour);
    dir.Save(dir.mBaseDir, "group", "b", 40, 30, baseColour);
    // The mod replaces b and adds c
    dir.Save(dir.mModDir, "group", "b", 20, 20, modColour);
    dir.Save(dir.mModDir, "group", "c", 20, 20, modColour);

    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.Layers(), 256), 1u);

    // All three are packed in the mod's directory, together, with what the game's has for a and
    // the mod's own for b
    const AnimationAttributesAndFrames a = dir.Packed(dir.mModDir, "group", "a");
    const AnimationAttributesAndFrames b = dir.Packed(dir.mModDir, "group", "b");
    const AnimationAttributesAndFrames c = dir.Packed(dir.mModDir, "group", "c");
    ASSERT_FALSE(a.mAttributes.mSheet.empty());
    EXPECT_EQ(a.mAttributes.mSheet, b.mAttributes.mSheet);
    EXPECT_EQ(a.mAttributes.mSheet, c.mAttributes.mSheet);
    EXPECT_EQ(dir.FirstPixel(dir.mModDir, a).ToU32(), baseColour.ToU32());
    EXPECT_EQ(dir.FirstPixel(dir.mModDir, b).ToU32(), modColour.ToU32());
    EXPECT_EQ(dir.FirstPixel(dir.mModDir, c).ToU32(), modColour.ToU32());
    EXPECT_EQ(b.mFrames[0].mSpriteWidth, 20u);

    // The game's directory has nothing new, and the mod's sheet of b is as it was
    EXPECT_FALSE(dir.mFs.FileExists((ModPagesDir::AnimPath(dir.mBaseDir, "group", "a").GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));
    EXPECT_FALSE(dir.mFs.FileExists(FileSystem::Path(dir.mBaseDir).Append(SpritePageWriter::kManifestPath).GetPath().c_str()));
    EXPECT_TRUE(AnimationAttributesAndFrames(dir.mFs.LoadToString((ModPagesDir::AnimPath(dir.mModDir, "group", "b").GetPath() + ".json").c_str())).mAttributes.mSheet.empty());
}

TEST(SpritePageWriter, ModWithoutSheetsGetsNoPages)
{
    ModPagesDir dir;
    dir.Save(dir.mBaseDir, "group", "a", 40, 30, {10, 20, 30, 255});

    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.Layers(), 256));
    EXPECT_EQ(SpritePageWriter::Repack(dir.mFs, dir.Layers(), 256), 0u);
    EXPECT_FALSE(dir.mFs.FileExists((ModPagesDir::AnimPath(dir.mModDir, "group", "a").GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));
}

TEST(SpritePageWriter, ModPagesAreStaleWhenAnythingTheyAreMadeFromChanges)
{
    ModPagesDir dir;
    dir.Save(dir.mBaseDir, "group", "a", 40, 30, {10, 20, 30, 255});
    dir.Save(dir.mModDir, "group", "b", 20, 20, {200, 100, 50, 255});
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.Layers(), 256));

    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.Layers(), 256), 1u);
    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.Layers(), 256));

    // Another GPU
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.Layers(), 128));

    // The game's sheet is made again
    const auto later = std::filesystem::file_time_type::clock::now() + std::chrono::hours(1);
    std::filesystem::last_write_time(ModPagesDir::AnimPath(dir.mBaseDir, "group", "a").GetPath() + ".png", later);
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.Layers(), 256));
    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.Layers(), 256), 1u);
    // (the clock of the file system is not the one the test moved the sheet to, so it is put back)
    std::filesystem::last_write_time(ModPagesDir::AnimPath(dir.mBaseDir, "group", "a").GetPath() + ".png", std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.Layers(), 256));

    // The mod gets a sheet of an animation of the game's
    dir.Save(dir.mModDir, "group", "a", 20, 20, {1, 2, 3, 255});
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.Layers(), 256));
    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.Layers(), 256), 1u);
    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.Layers(), 256));

    // And takes it away again: the game's is used again
    std::filesystem::remove(ModPagesDir::AnimPath(dir.mModDir, "group", "a").GetPath() + ".json");
    std::filesystem::remove(ModPagesDir::AnimPath(dir.mModDir, "group", "a").GetPath() + ".png");
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.Layers(), 256));
}

TEST(SpritePageWriter, PagesOfAModThatLostItsSheetsAreStaleAndRemoved)
{
    ModPagesDir dir;
    dir.Save(dir.mBaseDir, "group", "a", 40, 30, {10, 20, 30, 255});
    dir.Save(dir.mModDir, "group", "b", 20, 20, {200, 100, 50, 255});
    ASSERT_EQ(SpritePageWriter::Repack(dir.mFs, dir.Layers(), 256), 1u);

    std::filesystem::remove(ModPagesDir::AnimPath(dir.mModDir, "group", "b").GetPath() + ".json");
    std::filesystem::remove(ModPagesDir::AnimPath(dir.mModDir, "group", "b").GetPath() + ".png");

    // The packed jsons in the mod's directory would be used for the game's animations still
    EXPECT_TRUE(SpritePageWriter::PagesStale(dir.Layers(), 256));
    EXPECT_EQ(SpritePageWriter::Repack(dir.mFs, dir.Layers(), 256), 0u);
    EXPECT_FALSE(dir.mFs.FileExists((ModPagesDir::AnimPath(dir.mModDir, "group", "a").GetPath() + SpritePageWriter::kPackedJsonExtension).c_str()));
    EXPECT_FALSE(SpritePageWriter::PagesStale(dir.Layers(), 256));
}

TEST(DataVersions, SpritePageSizeFitsTheGpuWithItsMask)
{
    using DV = DataConversion::DataVersions;
    // A page has its mask under it, so the texture has to be twice as tall as the page
    EXPECT_EQ(DV::SpritePageSizeFor(0), 2048u);
    EXPECT_EQ(DV::SpritePageSizeFor(16384), 2048u);
    EXPECT_EQ(DV::SpritePageSizeFor(4096), 2048u);
    EXPECT_EQ(DV::SpritePageSizeFor(2048), 1024u);
    EXPECT_EQ(DV::SpritePageSizeFor(1024), 512u);
    EXPECT_EQ(DV::SpritePageSizeFor(256), 512u);
    for (u32 maxTexture : {1024u, 2048u, 4096u, 8192u})
    {
        EXPECT_LE(DV::SpritePageSizeFor(maxTexture) * 2, std::max(maxTexture, 1024u));
    }
}

TEST(DataVersions, PagesAreMadeAgainFromTheSheetsWhenTheGpuWantsAnotherPageSize)
{
    using DV = DataConversion::DataVersions;
    DV dv = DV::LatestVersion(2048);
    EXPECT_FALSE(dv.ConvertAnimations());
    EXPECT_FALSE(dv.RepackPages());
    EXPECT_FALSE(dv.AnyConversionRequired());

    // The same data on a GPU that takes smaller textures: the game's files aren't needed again
    dv.mWantedSpritePageSize = DV::SpritePageSizeFor(2048);
    EXPECT_FALSE(dv.ConvertAnimations());
    EXPECT_TRUE(dv.RepackPages());
    EXPECT_TRUE(dv.AnyConversionRequired());
}

TEST(DataVersions, PagesAreMadeAgainWhenASheetIsNewer)
{
    using DV = DataConversion::DataVersions;
    DV dv = DV::LatestVersion(2048);
    dv.mPagesStale = true;
    EXPECT_FALSE(dv.ConvertAnimations());
    EXPECT_TRUE(dv.RepackPages());

    // Converting the animations makes the pages anyway
    dv.mAnimationVersion = 0;
    EXPECT_TRUE(dv.ConvertAnimations());
    EXPECT_FALSE(dv.RepackPages());
}
