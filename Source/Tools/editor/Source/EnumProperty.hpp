#pragma once

#include "EnumPropertyBase.hpp"
#include "ReflectedEnumProperties.hpp"

template<typename EnumType>
class EnumProperty final : public EnumPropertyBase
{
public:
    EnumProperty(EnumType& enumValue, const char* fieldName, QUndoStack& undoStack, IGraphicsItem* pGraphicsItem)
     : EnumPropertyBase(EnumReflector<EnumType>::to_index(enumValue), &enumValue, fieldName, undoStack, pGraphicsItem)
     , mEnumValue(enumValue)
    {

    }

    QString VGetEnumName(s32 idx) const override
    {
        return EnumReflector<EnumType>::to_str(idx);
    }

    void VPopulateEnumCombo(QComboBox* pCombo) override
    {
        for (auto& item : EnumReflector<EnumType>::mArray)
        {
            pCombo->addItem(item.mName);
        }
    }

    void VSetEnumFromIdx(s32 idx) override
    {
        mOldIdx = idx;
        mEnumValue = EnumReflector<EnumType>::to_value(idx);
    }
private:
    EnumType& mEnumValue;
};
