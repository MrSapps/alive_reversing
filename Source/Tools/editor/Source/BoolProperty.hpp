#pragma once

#include "PropertyTreeItemBase.hpp"
#include <QCheckBox>

class QUndoStack;
class IGraphicsItem;

class BoolProperty final : public QObject, public PropertyTreeItemBase
{
    Q_OBJECT
public:

    BoolProperty(PropertyTreeWidget* pParent, bool& boolProperty, const char* pPropertyName, QUndoStack& undoStack, IGraphicsItem* pGraphicsItem);

    QWidget* GetEditorWidget(PropertyTreeWidget* pParent) override;
    QWidget* GetPersistentEditorWidget(PropertyTreeWidget* pParent) override;

    const void* GetPropertyLookUpKey() const override
    {
        return &mBoolValue;
    }

    void Refresh() override;

private:
    bool mOldValue = false;
    bool& mBoolValue;
    const char* mPropertyName = nullptr;
    QUndoStack& mUndoStack;
    IGraphicsItem* mGraphicsItem = nullptr;
    QCheckBox* mCheckBox = nullptr;
};
