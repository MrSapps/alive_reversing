#pragma once

#include "PropertyTreeItemBase.hpp"
#include "IntegerType.hpp"

class QUndoStack;
class BigSpinBox;
class IGraphicsItem;

class BasicTypeProperty final : public QObject, public PropertyTreeItemBase
{
    Q_OBJECT
public:

    BasicTypeProperty(IntegerType intType, void* pInteger, const char* pPropertyName, QUndoStack& undoStack, IGraphicsItem* pGraphicsItem);

    QWidget* GetEditorWidget(PropertyTreeWidget* pParent) override;

    const void* GetPropertyLookUpKey() const override
    {
        return mIntegerPtr;
    }

    void Refresh() override;

private:

    IntegerType mIntType = IntegerType::Int_S16;
    void* mIntegerPtr = nullptr;
    const char* mPropertyName = nullptr;
    QUndoStack& mUndoStack;
    IGraphicsItem* mGraphicsItem = nullptr;
    BigSpinBox* mSpinBox = nullptr;
    qint64 mOldValue = 0;
};
