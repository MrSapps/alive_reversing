#pragma once

#include "../../relive_lib/Types.hpp"

struct CollisionConnectData
{
    CollisionConnectData(s16* mCurrentValue, int mOldValue, int mNewValue)
        : mCurrentValue(mCurrentValue)
        , mOldValue(mOldValue)
        , mNewValue(mNewValue)
    {

    }

    s16* mCurrentValue;
    s16 mOldValue;
    s16 mNewValue;
};
