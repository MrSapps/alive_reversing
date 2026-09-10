#include "../../relive_lib/stdafx.h"
#include "relive_api.hpp"
#include "JsonUpgraderRelive.hpp"
#include "JsonMapRootInfoReader.hpp"
#include "ApiContext.hpp"
#include "TypesCollectionRelive.hpp"

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

void SetAliveFatalCallBack(TAliveFatalCb callBack)
{
    fnAliveFatalCb = callBack;
}

// Increment when a breaking change to the JSON is made and implement an
// upgrade step that converts from the last version to the current.
constexpr s32 kApiVersion = 4;

[[nodiscard]] s32 GetApiVersion()
{
    return kApiVersion;
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

} // namespace ReliveAPI
