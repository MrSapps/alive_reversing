#pragma once

struct BoolPropertyChangeData
{
    BoolPropertyChangeData(bool& boolValue, bool oldValue)
        : mBoolValue(boolValue), mOldValue(oldValue)
    {

    }

    bool& mBoolValue;
    bool mOldValue = false;
};
