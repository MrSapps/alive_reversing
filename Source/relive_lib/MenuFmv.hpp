#pragma once

#include "Types.hpp"

enum class EReliveLevelIds : s16;

namespace relive
{
    // Generic "menu list entry" layout shared by AO/AE's FMV list and level-select list -
    // identical field layout in both games, just previously duplicated per-game.
    struct MenuFmvEntry final
    {
        const char_type* mName;
        EReliveLevelIds mLevel;
        s16 mPath;
        s16 mCamera;
        s16 mFmvId;
        s16 mAbeSpawnX;
        s16 mAbeSpawnY;
    };
} // namespace relive
