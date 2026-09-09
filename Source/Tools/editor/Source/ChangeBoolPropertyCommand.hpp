#pragma once

#include <QUndoCommand>

class PropertyTreeWidget;
class IGraphicsItem;

struct BoolPropertyChangeData
{
    BoolPropertyChangeData(bool& boolValue, bool oldValue)
        : mBoolValue(boolValue), mOldValue(oldValue)
    {

    }

    bool& mBoolValue;
    bool mOldValue = false;
};

// TODO: probably merge with BasicTypePropertyChangeData ??
struct LinkedBoolProperty
{
    LinkedBoolProperty(const char* propertyName, bool& boolValue, PropertyTreeWidget* pTreeWidget, IGraphicsItem* pGraphicsItem)
        : mPropertyName(propertyName), mBoolValue(boolValue), mTreeWidget(pTreeWidget), mGraphicsItem(pGraphicsItem)
    {

    }

    const char* mPropertyName = nullptr;
    bool& mBoolValue;
    PropertyTreeWidget* mTreeWidget = nullptr;
    IGraphicsItem* mGraphicsItem = nullptr;
};

class ChangeBoolPropertyCommand final : public QUndoCommand
{
public:
    ChangeBoolPropertyCommand(LinkedBoolProperty linkedProperty, BoolPropertyChangeData propertyData)
        : mLinkedProperty(linkedProperty), mPropertyData(propertyData)
    {
        UpdateText();
    }

    void undo() override;

    void redo() override;

private:
    void UpdateText();
    LinkedBoolProperty mLinkedProperty;
    BoolPropertyChangeData mPropertyData;
};
