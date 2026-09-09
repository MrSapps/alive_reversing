#pragma once

class IGraphicsItem;

class ISyncPropertiesToTree
{
public:
    virtual ~ISyncPropertiesToTree() { }
    virtual void Sync(IGraphicsItem* pItem) = 0;
};
