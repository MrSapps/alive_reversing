#include "SpritePageWriter.hpp"
#include "PNGFile.hpp"
#include "../ResourceManager.hpp"
#include "../SpriteAtlasPacker.hpp"
#include "../SpriteMask.hpp"
#include "../logger.hpp"
#include "ConversionProgress.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <tuple>

constexpr char SpritePageWriter::kPagesDirectory[];
constexpr char SpritePageWriter::kPackedJsonExtension[];
constexpr char SpritePageWriter::kManifestPath[];

static u32 NextPowerOfTwo(u32 value, u32 minimum)
{
    u32 result = minimum;
    while (result < value)
    {
        result *= 2;
    }
    return result;
}

static bool EndsWith(const std::string& text, const char* suffix)
{
    const std::size_t length = strlen(suffix);
    return text.size() >= length && text.compare(text.size() - length, length, suffix) == 0;
}

static void SaveJson(FileSystem& fs, const std::string& jsonPath, const AnimAttributes& attributes, const std::vector<PerFrameInfo>& frames)
{
    const std::string json = AnimationConverter::ToJson(attributes, frames);
    AutoFILE jsonFile = fs.OpenFile(jsonPath.c_str(), "wb");
    jsonFile.Write(reinterpret_cast<const u8*>(json.c_str()), static_cast<u32>(json.size()));
}

void SpritePageWriter::SaveSheet(FileSystem& fs, const FileSystem::Path& animPath, const AnimationConverter::Sheet& sheet)
{
    const std::string pngPath = animPath.GetPath() + ".png";
    PNGFile png;
    png.SaveSprite(fs, pngPath.c_str(), sheet.mRgba, sheet.mWidth, sheet.mHeight);

    // The mask of an earlier conversion would still be used, if this sheet has none
    const std::string maskPath = PNGFile::MaskPathFor(pngPath);
    if (sheet.mMask.empty())
    {
        std::remove(maskPath.c_str());
    }
    else
    {
        png.SaveSprite(fs, maskPath.c_str(), sheet.mMask, sheet.mWidth, sheet.mHeight);
    }

    // The sheet is the source of the pages: they say where the frames are on it, not on a page
    AnimAttributes attributes = sheet.mAttributes;
    attributes.mSheet.clear();
    SaveJson(fs, animPath.GetPath() + ".json", attributes, sheet.mFrames);
}

// An animation's sheet: its json is read, its pixels only when they are copied to a page
struct RepackSheet final
{
    // The path without the extension
    std::string mPath;
    // The first directory under animations/
    // Where it is under animations/, without the extension
    std::string mRelative;
    std::string mGroup;
    AnimationAttributesAndFrames mJson;
};

// The frames of an animation that are in different places in its sheet, as the pixels are only
// written once for the frames that share a place
struct FrameRects final
{
    std::vector<SpriteAtlasPacker::FrameSize> mSizes;
    std::vector<std::pair<u32, u32>> mSheetPositions;
    // The rectangle of each frame
    std::vector<std::size_t> mFrameToRect;

    explicit FrameRects(const std::vector<PerFrameInfo>& frames)
    {
        std::map<std::tuple<u32, u32, u32, u32>, std::size_t> seen;
        for (const PerFrameInfo& frame : frames)
        {
            const auto key = std::make_tuple(frame.mSpriteSheetX, frame.mSpriteSheetY, frame.mSpriteWidth, frame.mSpriteHeight);
            auto [it, added] = seen.emplace(key, mSizes.size());
            if (added)
            {
                mSizes.push_back({frame.mSpriteWidth, frame.mSpriteHeight});
                mSheetPositions.push_back({frame.mSpriteSheetX, frame.mSpriteSheetY});
            }
            mFrameToRect.push_back(it->second);
        }
    }
};

// Copies a rectangle of a sheet's pixels (4 bytes each) into a page
static void CopyRect(const std::vector<u8>& src, u32 srcWidth, u32 srcX, u32 srcY, u32 width, u32 height, std::vector<u8>& dst, u32 dstWidth, u32 x, u32 y)
{
    for (u32 row = 0; row < height; row++)
    {
        memcpy(&dst[(static_cast<std::size_t>(y + row) * dstWidth + x) * 4], &src[(static_cast<std::size_t>(srcY + row) * srcWidth + srcX) * 4], static_cast<std::size_t>(width) * 4);
    }
}

// A data directory the game searches
struct PageLayer final
{
    explicit PageLayer(const std::string& dir)
        : mAnimations(std::filesystem::path(dir) / "animations")
        , mPages(std::filesystem::path(dir) / SpritePageWriter::kPagesDirectory)
        , mPrefix(mAnimations.generic_string() + "/")
    {
    }

    std::filesystem::path mAnimations;
    std::filesystem::path mPages;
    // The start of the path of everything in it
    std::string mPrefix;
};

// What is in the animations directory of a layer
struct LayerFiles final
{
    // The sheets' jsons, in path order so the pages don't depend on the order the file system
    // lists them in
    std::vector<std::string> mJsons;
    std::vector<std::string> mPackedJsons;
    // When the newest file that a page could be made from was written
    std::filesystem::file_time_type mNewestSource = std::filesystem::file_time_type::min();
};

static LayerFiles FindLayerFiles(const PageLayer& layer)
{
    LayerFiles files;
    std::error_code error;
    if (!std::filesystem::is_directory(layer.mAnimations, error))
    {
        return files;
    }

    for (auto it = std::filesystem::recursive_directory_iterator(layer.mAnimations, error); !error && it != std::filesystem::recursive_directory_iterator(); it.increment(error))
    {
        const std::filesystem::path& path = it->path();
        if (it->is_directory(error))
        {
            if (path == layer.mPages)
            {
                it.disable_recursion_pending();
            }
            continue;
        }

        const std::string name = path.generic_string();
        if (EndsWith(name, SpritePageWriter::kPackedJsonExtension))
        {
            files.mPackedJsons.push_back(name);
            continue;
        }

        if (path.extension() == ".json")
        {
            files.mJsons.push_back(name);
        }
        if (path.extension() == ".json" || path.extension() == ".png")
        {
            files.mNewestSource = std::max(files.mNewestSource, std::filesystem::last_write_time(path, error));
        }
    }
    std::sort(files.mJsons.begin(), files.mJsons.end());
    return files;
}

// A sheet the game would use: the one of the first layer that has it
struct EffectiveSheet final
{
    // Where it is under animations/, without the extension
    std::string mRelative;
    // The sheet's path without the extension
    std::string mPath;
    std::size_t mLayer = 0;
};

struct PageSources final
{
    std::vector<PageLayer> mLayers;
    std::vector<LayerFiles> mFiles;
    std::vector<EffectiveSheet> mSheets;
};

static PageSources FindPageSources(const std::vector<std::string>& layerDirs)
{
    PageSources sources;
    std::map<std::string, std::size_t> chosen;
    for (std::size_t i = 0; i < layerDirs.size(); i++)
    {
        sources.mLayers.emplace_back(layerDirs[i]);
        sources.mFiles.push_back(FindLayerFiles(sources.mLayers[i]));
        for (const std::string& json : sources.mFiles[i].mJsons)
        {
            EffectiveSheet sheet;
            sheet.mPath = json.substr(0, json.size() - strlen(".json"));
            sheet.mRelative = sheet.mPath.substr(std::min(sheet.mPath.size(), sources.mLayers[i].mPrefix.size()));
            sheet.mLayer = i;
            if (chosen.emplace(sheet.mRelative, sources.mSheets.size()).second)
            {
                sources.mSheets.push_back(std::move(sheet));
            }
        }
    }
    std::sort(sources.mSheets.begin(), sources.mSheets.end(), [](const EffectiveSheet& a, const EffectiveSheet& b)
        { return a.mRelative < b.mRelative; });
    return sources;
}

// What the pages are made from and how, so a sheet that is added, removed or comes from another
// directory, or another page size, is noticed
static std::string BuildManifest(const PageSources& sources, u32 pageSize)
{
    std::string text = "pagesize " + std::to_string(pageSize) + "\n";
    for (const PageLayer& layer : sources.mLayers)
    {
        text += "layer " + layer.mPrefix + "\n";
    }
    for (const EffectiveSheet& sheet : sources.mSheets)
    {
        text += std::to_string(sheet.mLayer) + " " + sheet.mRelative + "\n";
    }
    return text;
}

// The pages and packed jsons of an earlier conversion: they are named after what is on them, so
// they would be left behind, unused, once the animations are packed another way
static void RemoveOldPages(const PageLayer& layer, const LayerFiles& files)
{
    std::error_code error;
    for (const std::string& packed : files.mPackedJsons)
    {
        std::filesystem::remove(packed, error);
    }

    for (const auto& entry : std::filesystem::directory_iterator(layer.mPages, error))
    {
        if (entry.path().extension() == ".png" || entry.path().filename() == "sources.txt")
        {
            std::filesystem::remove(entry.path(), error);
        }
    }
}

u32 SpritePageWriter::Repack(FileSystem& fs, const FileSystem::Path& dataDir, u32 pageSize, ConversionProgress* progress)
{
    // Whatever way Repack returns, all of its units are reported
    u32 reported = 0;
    const auto reportUpTo = [&](u32 units)
    {
        if (progress && units > reported)
        {
            progress->AddCompleted(ConversionCategory::SpritePages, units - reported);
            reported = units;
        }
    };
    const u32 pages = RepackLayers(fs, std::vector<std::string>{dataDir.GetPath()}, pageSize, reportUpTo);
    reportUpTo(kProgressUnits);
    return pages;
}

u32 SpritePageWriter::Repack(FileSystem& fs, const std::vector<std::string>& layers, u32 pageSize)
{
    return RepackLayers(fs, layers, pageSize, [](u32) {});
}

u32 SpritePageWriter::RepackLayers(FileSystem& fs, const std::vector<std::string>& layers, u32 pageSize, const std::function<void(u32)>& reportUpTo)
{
    const auto startTime = std::chrono::steady_clock::now();
    const auto secondsSince = [](std::chrono::steady_clock::time_point since)
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
    };

    if (layers.empty())
    {
        return 0;
    }

    const PageSources sources = FindPageSources(layers);
    const PageLayer& outputLayer = sources.mLayers[0];

    // What an earlier packing made is no use any more
    RemoveOldPages(outputLayer, sources.mFiles[0]);

    // A mod that has no sheets of its own doesn't need pages: the game's are used
    if (layers.size() > 1 && sources.mFiles[0].mJsons.empty())
    {
        return 0;
    }

    std::vector<RepackSheet> sheets;
    PNGFile png;
    for (const EffectiveSheet& effective : sources.mSheets)
    {
        RepackSheet sheet;
        sheet.mPath = effective.mPath;
        sheet.mRelative = effective.mRelative;
        sheet.mJson = AnimationAttributesAndFrames(fs.LoadToString((effective.mPath + ".json").c_str()));
        if (!sheet.mJson.mAttributes.mSheet.empty() || sheet.mJson.mFrames.empty())
        {
            // Not a sheet's json: one from a conversion that packed the sheets away
            LOG_WARNING("%s.json isn't a sprite sheet's json, not packed", effective.mPath.c_str());
            continue;
        }

        // The pixels are read when their page is written, a page at a time: the sheets of a mod
        // can be too many to hold at once
        if (!fs.FileExists((sheet.mPath + ".png").c_str()))
        {
            LOG_WARNING("%s.json has no sheet, not packed", effective.mPath.c_str());
            continue;
        }

        sheet.mGroup = sheet.mRelative.substr(0, sheet.mRelative.find('/'));
        sheets.push_back(std::move(sheet));
    }

    const double loadSeconds = secondsSince(startTime);

    // The groups, numbered in name order so the pages don't depend on the order of conversion
    std::map<std::string, u32> groupIds;
    for (const RepackSheet& sheet : sheets)
    {
        groupIds[sheet.mGroup] = 0;
    }
    u32 nextGroupId = 0;
    for (auto& [name, id] : groupIds)
    {
        id = nextGroupId++;
    }

    std::vector<SpriteAtlasPacker::Item> items;
    std::vector<FrameRects> rects;
    for (const RepackSheet& sheet : sheets)
    {
        rects.emplace_back(sheet.mJson.mFrames);
        items.push_back({rects.back().mSizes, groupIds[sheet.mGroup]});
    }

    const auto packTime = std::chrono::steady_clock::now();
    const SpriteAtlasPacker::Result packed = SpriteAtlasPacker::Pack(items, pageSize);
    const double packSeconds = secondsSince(packTime);
    reportUpTo(10);

    // How big each page has to be
    struct Page final
    {
        u32 mWidth = 0;
        u32 mHeight = 0;
        bool mHasMask = false;
        std::string mFirstGroup;
    };
    std::vector<Page> pages(packed.mPageCount);
    for (std::size_t i = 0; i < sheets.size(); i++)
    {
        const SpriteAtlasPacker::Placement& placement = packed.mPlacements[i];
        if (placement.mPage == SpriteAtlasPacker::kNoPage)
        {
            continue;
        }

        Page& page = pages[placement.mPage];
        for (std::size_t r = 0; r < rects[i].mSizes.size(); r++)
        {
            page.mWidth = std::max(page.mWidth, placement.mFrames[r].mX + rects[i].mSizes[r].mWidth);
            page.mHeight = std::max(page.mHeight, placement.mFrames[r].mY + rects[i].mSizes[r].mHeight);
        }
        page.mHasMask = page.mHasMask || fs.FileExists(PNGFile::MaskPathFor(sheets[i].mPath + ".png").c_str());
        if (page.mFirstGroup.empty())
        {
            page.mFirstGroup = sheets[i].mGroup;
        }
    }

    std::size_t placedSheets = 0;
    for (const SpriteAtlasPacker::Placement& placement : packed.mPlacements)
    {
        placedSheets += placement.mPage != SpriteAtlasPacker::kNoPage ? 1 : 0;
    }
    std::size_t sheetsWritten = 0;

    std::error_code error;
    std::filesystem::create_directories(outputLayer.mPages, error);

    std::vector<std::string> pageNames(pages.size());
    for (std::size_t p = 0; p < pages.size(); p++)
    {
        Page& page = pages[p];
        page.mWidth = NextPowerOfTwo(page.mWidth, 16);
        page.mHeight = NextPowerOfTwo(page.mHeight, 16);

        std::vector<u8> rgba(static_cast<std::size_t>(page.mWidth) * page.mHeight * 4);
        std::vector<u8> mask;
        if (page.mHasMask)
        {
            // Nothing flagged where a sheet has no mask: all the pixels are the default
            mask.resize(rgba.size());
            for (std::size_t i = 0; i < mask.size(); i += 4)
            {
                SpriteMask::Set(&mask[i], 0);
            }
        }

        for (std::size_t i = 0; i < sheets.size(); i++)
        {
            const SpriteAtlasPacker::Placement& placement = packed.mPlacements[i];
            if (placement.mPage != p)
            {
                continue;
            }

            const RepackSheet& sheet = sheets[i];
            sheetsWritten++;
            reportUpTo(10 + 90 * sheetsWritten / std::max<std::size_t>(placedSheets, 1));
            std::vector<u8> sheetRgba;
            std::vector<u8> sheetMask;
            u32 sheetWidth = 0;
            u32 sheetHeight = 0;
            const std::string sheetPngPath = sheet.mPath + ".png";
            if (!png.LoadSprite(fs, sheetPngPath.c_str(), sheetRgba, sheetWidth, sheetHeight))
            {
                LOG_WARNING("%s can't be read, its frames are empty on the page", sheetPngPath.c_str());
                continue;
            }
            png.LoadMask(fs, sheetPngPath, sheetMask, sheetWidth, sheetHeight);

            for (std::size_t r = 0; r < rects[i].mSizes.size(); r++)
            {
                const auto& [srcX, srcY] = rects[i].mSheetPositions[r];
                const SpriteAtlasPacker::FrameSize& size = rects[i].mSizes[r];
                if (size.mWidth == 0 || size.mHeight == 0)
                {
                    continue;
                }

                // A json (a mod's, say) can say a frame is outside its sheet
                if (srcX + size.mWidth > sheetWidth || srcY + size.mHeight > sheetHeight)
                {
                    LOG_WARNING("%s has a frame outside the sheet, it is empty on the page", sheetPngPath.c_str());
                    continue;
                }

                const SpriteAtlasPacker::Position& dst = placement.mFrames[r];
                CopyRect(sheetRgba, sheetWidth, srcX, srcY, size.mWidth, size.mHeight, rgba, page.mWidth, dst.mX, dst.mY);
                if (!sheetMask.empty())
                {
                    CopyRect(sheetMask, sheetWidth, srcX, srcY, size.mWidth, size.mHeight, mask, page.mWidth, dst.mX, dst.mY);
                }
            }
        }

        // The page's name is relative to the directory the packed jsons are in
        pageNames[p] = std::string(kPagesDirectory) + "/" + page.mFirstGroup + "_" + std::to_string(p);
        const std::string pagePath = (std::filesystem::path(layers[0]) / (pageNames[p] + ".png")).generic_string();
        png.SaveSprite(fs, pagePath.c_str(), rgba, page.mWidth, page.mHeight);
        if (page.mHasMask)
        {
            png.SaveSprite(fs, PNGFile::MaskPathFor(pagePath).c_str(), mask, page.mWidth, page.mHeight);
        }
    }

    for (std::size_t i = 0; i < sheets.size(); i++)
    {
        const SpriteAtlasPacker::Placement& placement = packed.mPlacements[i];
        if (placement.mPage == SpriteAtlasPacker::kNoPage)
        {
            // Bigger than a page: it stays a sheet of its own
            continue;
        }

        AnimAttributes attributes = sheets[i].mJson.mAttributes;
        std::vector<PerFrameInfo> frames = sheets[i].mJson.mFrames;
        attributes.mSheet = pageNames[placement.mPage];
        for (std::size_t f = 0; f < frames.size(); f++)
        {
            const SpriteAtlasPacker::Position& position = placement.mFrames[rects[i].mFrameToRect[f]];
            frames[f].mSpriteSheetX = position.mX;
            frames[f].mSpriteSheetY = position.mY;
        }

        // In the first directory, even if the sheet is in another: the game looks for the packed
        // json from the first directory down
        const std::filesystem::path packedPath = outputLayer.mAnimations / (sheets[i].mRelative + kPackedJsonExtension);
        std::filesystem::create_directories(packedPath.parent_path(), error);
        SaveJson(fs, packedPath.generic_string(), attributes, frames);
    }

    {
        const std::string manifestPath = (std::filesystem::path(layers[0]) / kManifestPath).generic_string();
        const std::string manifest = BuildManifest(sources, pageSize);
        AutoFILE manifestFile = fs.OpenFile(manifestPath.c_str(), "wb");
        manifestFile.Write(reinterpret_cast<const u8*>(manifest.c_str()), static_cast<u32>(manifest.size()));
    }

    LOG_INFO("Packed %zu animations into %zu sprite pages: %.1fs reading the sheets, %.1fs packing, %.1fs in all", sheets.size(), pages.size(), loadSeconds, packSeconds, secondsSince(startTime));
    return static_cast<u32>(pages.size());
}

bool SpritePageWriter::PagesStale(const FileSystem::Path& dataDir, u32 pageSize)
{
    return PagesStale(std::vector<std::string>{dataDir.GetPath()}, pageSize);
}

bool SpritePageWriter::PagesStale(const std::vector<std::string>& layers, u32 pageSize)
{
    if (layers.empty())
    {
        return false;
    }

    const PageSources sources = FindPageSources(layers);
    const PageLayer& outputLayer = sources.mLayers[0];

    // The pages and packed jsons in the first directory
    std::error_code error;
    std::filesystem::file_time_type oldestPage = std::filesystem::file_time_type::max();
    bool anyPage = false;
    for (const auto& entry : std::filesystem::directory_iterator(outputLayer.mPages, error))
    {
        if (entry.path().extension() == ".png")
        {
            oldestPage = std::min(oldestPage, std::filesystem::last_write_time(entry.path(), error));
            anyPage = true;
        }
    }

    // A mod without sheets has no pages of its own, and one that had them is stale: its packed
    // jsons would still be used
    if (layers.size() > 1 && sources.mFiles[0].mJsons.empty())
    {
        return anyPage || !sources.mFiles[0].mPackedJsons.empty();
    }

    if (sources.mSheets.empty())
    {
        return false;
    }
    if (!anyPage)
    {
        return true;
    }

    FileSystem fs;
    const std::string manifestPath = (std::filesystem::path(layers[0]) / kManifestPath).generic_string();
    if (fs.LoadToString(manifestPath.c_str()) != BuildManifest(sources, pageSize))
    {
        return true;
    }

    // A sheet in any of the directories (the mod's or the game's) that is newer
    for (const LayerFiles& files : sources.mFiles)
    {
        if (files.mNewestSource > oldestPage)
        {
            return true;
        }
    }
    return false;
}
