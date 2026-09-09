#pragma once

#include <QUndoCommand>
#include <QtGlobal>

class PropertyTreeWidget;
class IGraphicsItem;

enum class IntegerType
{
    Int_S16,
    Int_U16,
    Int_S32,
    Int_U32,
};

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

// TODO: probably merge with BasicTypePropertyChangeData ??
struct LinkedBasicTypeProperty
{
    LinkedBasicTypeProperty(const char* propertyName, IntegerType intType, void* pIntPtr, PropertyTreeWidget* pTreeWidget, IGraphicsItem* pGraphicsItem)
        : mPropertyName(propertyName), mIntegerType(intType), mIntPtr(pIntPtr), mTreeWidget(pTreeWidget), mGraphicsItem(pGraphicsItem)
    {

    }

    const char* mPropertyName = nullptr;
    IntegerType mIntegerType;
    void* mIntPtr = nullptr;
    PropertyTreeWidget* mTreeWidget = nullptr;
    IGraphicsItem* mGraphicsItem = nullptr;
};

class ChangeBasicTypePropertyCommand final : public QUndoCommand
{
public:
    ChangeBasicTypePropertyCommand(LinkedBasicTypeProperty linkedProperty, BasicTypePropertyChangeData propertyData);

    void undo() override;

    void redo() override;

    int id() const override
    {
        return 1;
    }

    bool mergeWith(const QUndoCommand* command) override;

private:
    void UpdateText();
    LinkedBasicTypeProperty mLinkedProperty;
    BasicTypePropertyChangeData mPropertyData;
    qint64 mTimeStamp = 0;
};
