#include "../../relive_lib/stdafx.h"
#include "relive_api.hpp"
#include "../../AliveLibAE/Path.hpp"
#include "../../AliveLibAE/PathData.hpp"
#include "../../relive_lib/Collisions.hpp"
#include "../../relive_lib/data_conversion/LvlReaderWriter.hpp"
#include "JsonUpgraderRelive.hpp"
#include "JsonModelTypes.hpp"
#include "JsonReaderRelive.hpp"
#include "JsonWriterRelive.hpp"
#include "JsonMapRootInfoReader.hpp"
#include "ApiContext.hpp"
#include "TypesCollectionRelive.hpp"
#include "../../relive_lib/PathDataExtensionsTypes.hpp"
#include <iostream>
#include <type_traits>
#include <typeindex>
#include "../../AliveLibAO/PathData.hpp"
#include "../../AliveLibAE/PathData.hpp"

bool RunningAsInjectedDll()
{
    return false;
}

static ReliveAPI::TAliveFatalCb fnAliveFatalCb = nullptr;

// ditto for AliveLibCommon
[[noreturn]] void ALIVE_FATAL(const char_type* fmt, ...)
{
    if (fnAliveFatalCb)
    {
        fnAliveFatalCb(fmt);
    }
    // fnAliveFatalCb should throw or exit - compiler doesn't know this and we can't
    // mark a function pointer as noreturn. So abort in the impossible case that the function
    // pointer impl does return.
    abort();
}

namespace ReliveAPI {

enum class OpenPathBndResult;
struct PathBND;
namespace Detail {
[[nodiscard]] static OpenPathBndResult OpenPathBndGeneric(std::vector<u8>& fileDataBuffer, PathBND& ret, LvlReader& lvl, Game game, s32* pathId);
}

void SetAliveFatalCallBack(TAliveFatalCb callBack)
{
    fnAliveFatalCb = callBack;
}

struct PathBND
{
    std::string mPathBndName;
    std::vector<s32> mPaths;
    std::vector<u8> mFileData;
    PathInfo mPathInfo;
};

[[nodiscard]] static PathInfo ToPathInfo(const AO::PathData& data, const CollisionInfo& collisionInfo)
{
    PathInfo info = {};
    info.mGridWidth = data.field_C_grid_width;
    info.mGridHeight = data.field_E_grid_height;
    info.mWidth = (data.field_8_bTop - data.field_4_bLeft) / data.field_C_grid_width;
    info.mHeight = (data.field_A_bBottom - data.field_6_bRight) / data.field_E_grid_height;
    info.mIndexTableOffset = data.field_18_object_index_table_offset;
    info.mObjectOffset = data.field_14_object_offset;

    info.mNumCollisionItems = collisionInfo.mNumCollisionItems;
    info.mCollisionOffset = collisionInfo.mCollisionOffset;

    info.mAbeStartXPos = 0; // doesn't apply to AO
    info.mAbeStartYPos = 0; // doesn't apply to AO

    info.mNumMudsInPath = 0; // doesn't apply to AO
    info.mTotalMuds = 99;
    info.mBadEndingMuds = 75;
    info.mGoodEndingMuds = 50;

    return info;
}


[[nodiscard]] static PathInfo ToPathInfo(const PathData& data, const CollisionInfo& collisionInfo, u32 idx)
{
    PathInfo info = {};
    info.mGridWidth = data.field_A_grid_width;
    info.mGridHeight = data.field_C_grid_height;
    info.mWidth = (data.field_4_bTop - data.field_0_bLeft) / data.field_A_grid_width;
    info.mHeight = (data.field_6_bBottom - data.field_2_bRight) / data.field_C_grid_height;
    info.mIndexTableOffset = data.field_16_object_indextable_offset;
    info.mObjectOffset = data.field_12_object_offset;

    info.mNumCollisionItems = collisionInfo.mNumCollisionItems;
    info.mCollisionOffset = collisionInfo.mCollisionOffset;

    info.mAbeStartXPos = data.field_1A_abe_start_xpos;
    info.mAbeStartYPos = data.field_1C_abe_start_ypos;

    info.mNumMudsInPath = Path_GetMudsInLevel(MapWrapper::FromAE(static_cast<LevelIds>(idx)), 0); // Can pass 0 for the path id as this is not overriding any hardcoded path data
    info.mTotalMuds = 300;
    info.mBadEndingMuds = 20;
    info.mGoodEndingMuds = 255;

    return info;
}

class PathBlyRecAdapter
{
public:
    PathBlyRecAdapter(const AO::PathBlyRec* pBlyRec, u32 idx)
        : mBlyRecAO(pBlyRec)
        , mIdx(idx)
    {
    }

    PathBlyRecAdapter(const PathBlyRec* pBlyRec, u32 idx)
        : mBlyRecAE(pBlyRec)
        , mIdx(idx)
    {
    }

    const char_type* BlyName() const
    {
        return mBlyRecAO ? mBlyRecAO->field_0_blyName : mBlyRecAE->field_0_blyName;
    }

    PathInfo ConvertPathInfo() const
    {
        return mBlyRecAO ? ToPathInfo(*mBlyRecAO->field_4_pPathData, *mBlyRecAO->field_8_pCollisionData) : ToPathInfo(*mBlyRecAE->field_4_pPathData, *mBlyRecAE->field_8_pCollisionData, mIdx);
    }

private:
    const AO::PathBlyRec* mBlyRecAO = nullptr;
    const PathBlyRec* mBlyRecAE = nullptr;
    u32 mIdx = 0;
};

class PathRootAdapter
{
public:
    explicit PathRootAdapter(const AO::PathRoot* pRoot)
        : mRootAO(pRoot)
    {
    }

    explicit PathRootAdapter(const PathRoot* pRoot)
        : mRootAE(pRoot)
    {
    }

    const char_type* BndName() const
    {
        return mRootAO ? mRootAO->field_38_bnd_name : mRootAE->field_38_bnd_name;
    }

    s32 PathCount() const
    {
        return mRootAO ? mRootAO->field_18_num_paths : mRootAE->field_18_num_paths;
    }

    PathBlyRecAdapter PathAt(s32 idx) const
    {
        return mRootAO ? PathBlyRecAdapter(&mRootAO->field_0_pBlyArrayPtr[idx], idx) : PathBlyRecAdapter(&mRootAE->field_0_pBlyArrayPtr[idx], idx);
    }

private:
    const AO::PathRoot* mRootAO = nullptr;
    const PathRoot* mRootAE = nullptr;
};

class PathRootContainerAdapter
{
public:
    explicit PathRootContainerAdapter(Game gameType)
        : mGameType(gameType)
    {
    }

    s32 PathRootCount() const
    {
        return mGameType == Game::AO ? AO::Path_Get_Paths_Count() : Path_Get_Paths_Count();
    }

    PathRootAdapter PathAt(s32 idx) const
    {
        return mGameType == Game::AO ? PathRootAdapter(AO::Path_Get_PathRoot(idx)) : PathRootAdapter(Path_Get_PathRoot(idx));
    }

private:
    Game mGameType = {};
};

enum class OpenPathBndResult
{
    OK,
    PathResourceChunkNotFound,
    NoPaths
};

[[nodiscard]] static PathBND OpenPathBnd(LvlReader& lvlReader, std::vector<u8>& fileDataBuffer, Game& game, s32* pathId)
{
    PathBND ret = {};

    // Find AE Path BND
    if (Detail::OpenPathBndGeneric(fileDataBuffer, ret, lvlReader, Game::AE, pathId) == OpenPathBndResult::OK)
    {
        game = Game::AE;
        return ret;
    }

    // Failed, look for AO Path BND
    if (Detail::OpenPathBndGeneric(fileDataBuffer, ret, lvlReader, Game::AO, pathId) == OpenPathBndResult::OK)
    {
        game = Game::AO;
        return ret;
    }

    // Both failed
    throw ReliveAPI::OpenPathException();
}

// Increment when a breaking change to the JSON is made and implement an
// upgrade step that converts from the last version to the current.
constexpr s32 kApiVersion = 4;

[[nodiscard]] s32 GetApiVersion()
{
    return kApiVersion;
}


void ExportPathBinaryToJson(FileSystem& fs, const std::string& jsonOutputFile, const std::string& inputLvlFile, s32 pathResourceId, Context& context)
{
    std::vector<u8> buffer;
    Detail::ExportPathBinaryToJson(buffer, fs, jsonOutputFile, inputLvlFile, pathResourceId, context);
}

std::string UpgradePathJson(FileSystem& fs, const std::string& jsonFile)
{
    // Read the game and version, we assume this never changes in the file format
    JsonMapRootInfoReader rootInfo;
    rootInfo.Read(fs, jsonFile);

    // TODO: impl
    // TODO: Pass in rootInfo.mMapRootInfo.mGame
    JsonUpgraderRelive upgrader;
    TypesCollectionRelive reliveTypes;
    return upgrader.Upgrade(reliveTypes, fs, jsonFile, rootInfo.mMapRootInfo.mVersion, GetApiVersion());
}

template <typename ReturnType, typename ContainerType>
static ReturnType* ItemAtXY(ContainerType& container, s32 x, s32 y)
{
    for (auto& iteratedItem : container)
    {
        if (iteratedItem.x == x && iteratedItem.y == y)
        {
            return &iteratedItem;
        }
    }
    return nullptr;
}

template <typename ItemType, typename ContainerType, typename FnOnItem>
static void ForEachItemAtXY(u32 xSize, u32 ySize, ContainerType& container, FnOnItem onItem)
{
    for (u32 y = 0; y < ySize; y++)
    {
        for (u32 x = 0; x < xSize; x++)
        {
            auto item = ItemAtXY<ItemType>(container, x, y);
            if (item)
            {
                onItem(*item);
            }
        }
    }
}

/*
static void WriteCollisionLine(ByteStream& s, const ::PathLine& line)
{
    s.Write(line.mRect.x);
    s.Write(line.mRect.y);
    s.Write(line.mRect.w);
    s.Write(line.mRect.h);

    s.Write(static_cast<u8>(line.mLineType));

    s.Write(line.mPrevious);
    s.Write(line.mNext);

    s.Write(line.mLineLength);
}
*/

/*
static void WriteStringTable(const std::vector<std::string>& strings, ByteStream& s)
{
    // String count
    s.Write(static_cast<u64>(strings.size()));

    // String lengths (size of a pointer)
    for (const auto& str : strings)
    {
        s.Write(static_cast<u64>(str.length() + 1));
    }

    // String data
    for (const auto& str : strings)
    {
        s.Write(str);
        s.Write(u8(0));
    }
}
*/

[[nodiscard]] EnumeratePathsResult EnumeratePaths(FileSystem& fs, const std::string& inputLvlFile)
{
    std::vector<u8> buffer;
    return Detail::EnumeratePaths(buffer, fs, inputLvlFile);
}

namespace Detail {


[[nodiscard]] static OpenPathBndResult OpenPathBndGeneric(std::vector<u8>& fileDataBuffer, PathBND& ret, LvlReader& lvl, Game game, s32* pathId)
{
    const PathRootContainerAdapter adapter(game);
    for (s32 i = 0; i < adapter.PathRootCount(); ++i)
    {
        const auto pathRoot = adapter.PathAt(i);
        if (!pathRoot.BndName())
        {
            continue;
        }

        // Try to open the BND
        const bool goodRead = lvl.ReadFileInto(fileDataBuffer, pathRoot.BndName());
        if (!goodRead)
        {
            continue;
        }

        ret.mPathBndName = pathRoot.BndName();

        ChunkedLvlFile pathChunks(fileDataBuffer);

        if (pathId)
        {
            // Open the specific path if we have one
            std::optional<LvlFileChunk> chunk = pathChunks.ChunkById(*pathId);
            if (!chunk)
            {
                return OpenPathBndResult::PathResourceChunkNotFound;
            }

            // Save the actual path resource block data
            ret.mFileData = std::move(chunk)->Data();

            // Path id in range?
            if (*pathId >= 0 && *pathId < 99)
            {
                // Path at this id have a name?
                const PathBlyRecAdapter pBlyRec = pathRoot.PathAt(*pathId);

                // See if we have extended info for this path entry
                std::optional<LvlFileChunk> extChunk = pathChunks.ChunkById(*pathId | *pathId << 8);
                if (extChunk)
                {
                    auto pChunkData = extChunk->Data().data();
                    auto pExt = reinterpret_cast<const PerPathExtension*>(pChunkData);
                    ret.mPathBndName = pathRoot.BndName();
                    ret.mPathInfo.mObjectOffset = pExt->mObjectOffset;
                    ret.mPathInfo.mIndexTableOffset = pExt->mIndexTableOffset;
                    ret.mPathInfo.mNumCollisionItems = pExt->mNumCollisionLines;
                    ret.mPathInfo.mHeight = pExt->mYSize;
                    ret.mPathInfo.mWidth = pExt->mXSize;
                    ret.mPathInfo.mCollisionOffset = pExt->mCollisionOffset;
                    ret.mPathInfo.mGridWidth = pExt->mGridWidth;
                    ret.mPathInfo.mGridHeight = pExt->mGridHeight;

                    ret.mPathInfo.mAbeStartXPos = pExt->mAbeStartXPos;
                    ret.mPathInfo.mAbeStartYPos = pExt->mAbeStartYPos;

                    ret.mPathInfo.mTotalMuds = pExt->mTotalMuds;

                    ret.mPathInfo.mNumMudsInPath = pExt->mNumMudsInPath;
                    ret.mPathInfo.mBadEndingMuds = pExt->mBadEndingMuds;
                    ret.mPathInfo.mGoodEndingMuds = pExt->mGoodEndingMuds;

                    pChunkData += sizeof(PerPathExtension);

                    // Read LCD Screen messages
                    auto pLCDScreenMsgs = reinterpret_cast<StringTable*>(pChunkData);
                    pChunkData = StringTable::MakeTable(pLCDScreenMsgs);
                    for (u64 j = 0u; j < pLCDScreenMsgs->mStringCount; j++)
                    {
                        ret.mPathInfo.mLCDScreenMessages.emplace_back(pLCDScreenMsgs->mStrings[j].string_ptr);
                    };

                    // Read hint fly messages, will be empty for AE
                    auto pHintFlyMsgs = reinterpret_cast<StringTable*>(pChunkData);
                    pChunkData = StringTable::MakeTable(pHintFlyMsgs);
                    for (u64 j = 0u; j < pHintFlyMsgs->mStringCount; j++)
                    {
                        ret.mPathInfo.mHintFlyMessages.emplace_back(pHintFlyMsgs->mStrings[j].string_ptr);
                    };

                    return OpenPathBndResult::OK;
                }
                else if (pBlyRec.BlyName())
                {
                    // Copy out its info
                    ret.mPathBndName = pathRoot.BndName();
                    ret.mPathInfo = pBlyRec.ConvertPathInfo();
                    return OpenPathBndResult::OK;
                }
            }

            // Path id out of bounds or the entry is blank
            return OpenPathBndResult::PathResourceChunkNotFound;
        }

        // Add all path ids
        for (s32 j = 1; j < 99; ++j)
        {
            // Only add paths that are not blank entries
            const PathBlyRecAdapter pBlyRec = pathRoot.PathAt(j);
            if (pBlyRec.BlyName())
            {
                ret.mPaths.push_back(j);
            }
            else
            {
                // OG has no path, but did one get added via the editor?
                std::optional<LvlFileChunk> extChunk = pathChunks.ChunkById(j | j << 8);
                if (extChunk)
                {
                    ret.mPaths.push_back(j);
                }
            }
        }

        // Return all of the file data
        ret.mFileData = fileDataBuffer;

        return OpenPathBndResult::OK;
    }

    return OpenPathBndResult::NoPaths;
}


[[nodiscard]] EnumeratePathsResult EnumeratePaths(std::vector<u8>& fileDataBuffer, FileSystem& fs, const std::string& inputLvlFile)
{
    EnumeratePathsResult ret = {};
    Game game = {};

    LvlReader lvl(fs, inputLvlFile.c_str());
    PathBND pathBnd = OpenPathBnd(lvl, fileDataBuffer, game, nullptr);
    ret.paths = pathBnd.mPaths;
    ret.pathBndName = pathBnd.mPathBndName;
    return ret;
}

void ExportPathBinaryToJson(std::vector<u8>& fileDataBuffer, FileSystem& fs, const std::string& jsonOutputFile, const std::string& inputLvlFile, s32 pathResourceId, Context& context)
{
    Game game = {};

    LvlReader lvl(fs, inputLvlFile.c_str());
    ReliveAPI::PathBND pathBnd = ReliveAPI::OpenPathBnd(lvl, fileDataBuffer, game, &pathResourceId);

    JsonWriterRelive doc(game, pathResourceId, pathBnd.mPathBndName, pathBnd.mPathInfo);
    doc.Save(fileDataBuffer, lvl, pathBnd.mPathInfo, pathBnd.mFileData, fs, jsonOutputFile, context);
}

} // namespace Detail


} // namespace ReliveAPI

