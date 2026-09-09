#pragma once

#include <QtGlobal>
#include "IntegerType.hpp"

struct BasicTypePropertyChangeData
{
    BasicTypePropertyChangeData(IntegerType intType, void* integerPtr, qint64 oldValue, qint64 newValue)
        : mIntType(intType), mIntegerPtr(integerPtr), mOldValue(oldValue), mNewValue(newValue)
    {

    }

    IntegerType mIntType = IntegerType::Int_S16;
    void* mIntegerPtr = nullptr;
    qint64 mOldValue = 0;
    qint64 mNewValue = 0;
};
