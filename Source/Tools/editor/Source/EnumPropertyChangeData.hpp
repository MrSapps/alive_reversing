#pragma once

#include <QString>

struct EnumPropertyChangeData
{
    EnumPropertyChangeData(QString oldEnumName, QString newEnumName, int oldIdx, int newIdx)
        : mOldEnumName(oldEnumName), mNewEnumName(newEnumName), mOldIdx(oldIdx), mNewIdx(newIdx)
    {

    }

    QString mOldEnumName;
    QString mNewEnumName;
    int mOldIdx = 0;
    int mNewIdx = 0;
};
