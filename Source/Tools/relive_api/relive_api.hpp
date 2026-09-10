#pragma once

#include "../../relive_lib/Types.hpp"
#include "ApiContext.hpp"

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

#define API_EXPORT

class FileSystem;

namespace ReliveAPI {

class CameraImageAndLayers;

struct EnumeratePathsResult final
{
    std::string pathBndName;
    std::vector<s32> paths;
};

using TAliveFatalCb = void(*)(const char_type*);

API_EXPORT void SetAliveFatalCallBack(TAliveFatalCb callBack);
API_EXPORT [[nodiscard]] s32 GetApiVersion();

// TODO: Remove (data conversion already made them all json)
API_EXPORT void ExportPathBinaryToJson(FileSystem& fs, const std::string& jsonOutputFile, const std::string& inputLvlFile, s32 pathResourceId, Context& context);

API_EXPORT [[nodiscard]] std::string UpgradePathJson(FileSystem& fs, const std::string& jsonFile);

// TODO: change to look at path jsons on disc
API_EXPORT [[nodiscard]] EnumeratePathsResult EnumeratePaths(FileSystem& fs, const std::string& inputLvlFile);

namespace Detail {

// TODO: Remove (everything is json, no need to open binary paths)
API_EXPORT void ExportPathBinaryToJson(std::vector<u8>& fileDataBuffer, FileSystem& fs, const std::string& jsonOutputFile, const std::string& inputLvlFile, s32 pathResourceId, Context& context);

// TODO: change to look at path jsons on disc
API_EXPORT [[nodiscard]] EnumeratePathsResult EnumeratePaths(std::vector<u8>& fileDataBuffer, FileSystem& fs, const std::string& inputLvlFile);
}

} // namespace ReliveAPI
