#pragma once

#include <QLabel>
#include "PropertyTreeItemBase.hpp"
#include "../../../relive_lib/RGB16.hpp"

class QUndoStack;
class IGraphicsItem;

class ColourPickerProperty final : public QObject, public PropertyTreeItemBase
{
    Q_OBJECT
public:
    ColourPickerProperty(PropertyTreeWidget* pParent, RGB16& pProperty, const char* pPropertyName, QUndoStack& undoStack, IGraphicsItem* pGraphicsItem);

    QWidget* GetEditorWidget(PropertyTreeWidget* pParent) override;
    QWidget* GetPersistentEditorWidget(PropertyTreeWidget* pParent) override;
    bool HasBothWidgets() const override { return true; }

    void Refresh() override;

    const void* GetPropertyLookUpKey() const override
    {
        return &mProperty;
    }

private:
    RGB16& mProperty;
    RGB16 mPrevValue;
    const char* mPropertyName = nullptr;
    QUndoStack& mUndoStack;
    IGraphicsItem* mGraphicsItem = nullptr;
    QLabel* mLabel = nullptr;
};
