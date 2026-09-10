#pragma once

#include "Types.hpp"
#include <vector>
#include <string>

namespace relive
{
// Shared per-FMV metadata layout for AO/AE (originally two separately reversed structs
// with a handful of extra fields each that turned out to be read nowhere in either
// game - see AliveLibAE/PathData.cpp and AliveLibAO/PathData.cpp for the per-game
// literal tables using this type).
struct FmvInfoEntry final
{
    const char_type* mName;
    u16 mId;     // AE: shown in the movie-select debug cheat (DDCheat.cpp). Unused by AO.
    u16 mFlags;  // AE: compared "== 1" when deciding camera-swap FMV behaviour (Map.cpp).
                 // AO: "stop background music for this FMV" flag (Map.cpp).
    s16 mFlag2;  // AO: compared "== 1", feeds a second CameraSwapper flag (Map.cpp).
                 // Unused by AE (was a "volume" field there).
};

struct FmvInfoArray final
{
    const FmvInfoEntry* mArray;
    size_t mCount;
};

// Walks a game's set of per-level FmvInfo tables and returns every real, unique FMV
// filename referenced by any of them - used to drive FMV data conversion.
[[nodiscard]] std::vector<std::string> CollectUniqueFmvNames(std::initializer_list<FmvInfoArray> arrays);
} // namespace relive
