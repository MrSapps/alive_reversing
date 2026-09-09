#pragma once

#include "IntegerType.hpp"

class PropertyTreeWidget;
class IGraphicsItem;

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
