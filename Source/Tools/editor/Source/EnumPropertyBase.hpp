#pragma once

#include "PropertyTreeItemBase.hpp"
#include "../../relive_lib/Types.hpp"
#include <QComboBox>

class QUndoStack;

class EnumPropertyBase : public QObject, public PropertyTreeItemBase
{
    Q_OBJECT
public:
    EnumPropertyBase(s32 oldIdx, void* pEnumValue, const char* fieldName, QUndoStack& undoStack, IGraphicsItem* pGraphicsItem);

    QWidget* GetEditorWidget(PropertyTreeWidget* pParent) override;

    void Refresh() override;

    const void* GetPropertyLookUpKey() const override
    {
        return mEnumVale;
    }

    virtual QString VGetEnumName(s32 idx) const = 0;
    virtual void VPopulateEnumCombo(QComboBox* pCombo) = 0;
    virtual void VSetEnumFromIdx(s32 idx) = 0;
protected:
    s32 mOldIdx = 0;
private:
    void* mEnumVale = nullptr;
    const char* mFieldName;
    QUndoStack& mUndoStack;
    IGraphicsItem* mGraphicsItem = nullptr;
    QComboBox* mCombo = nullptr;
};
