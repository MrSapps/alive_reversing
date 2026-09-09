#pragma once

class PropertyTreeWidget;
class IGraphicsItem;

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
