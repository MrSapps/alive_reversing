#pragma once

class PropertyTreeWidget;
class IGraphicsItem;

// TODO: Merge with EnumPropertyChangeData ?
class LinkedEnumProperty
{
public:
    LinkedEnumProperty(PropertyTreeWidget* pTree, const char* pFieldName, void* pEnumKey, IGraphicsItem* pGraphicsItem)
     : mTree(pTree), mFieldName(pFieldName), mEnumKey(pEnumKey), mGraphicsItem(pGraphicsItem)
    {

    }

    PropertyTreeWidget* mTree = nullptr;
    const char* mFieldName = nullptr;
    void* mEnumKey = nullptr;
    IGraphicsItem* mGraphicsItem = nullptr;
};
