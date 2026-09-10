#pragma once

#include "../../relive_lib/Types.hpp"

#include <string>

#define API_EXPORT

class FileSystem;

namespace ReliveAPI {

using TAliveFatalCb = void(*)(const char_type*);

API_EXPORT void SetAliveFatalCallBack(TAliveFatalCb callBack);
API_EXPORT [[nodiscard]] s32 GetApiVersion();

API_EXPORT [[nodiscard]] std::string UpgradePathJson(FileSystem& fs, const std::string& jsonFile);

} // namespace ReliveAPI
