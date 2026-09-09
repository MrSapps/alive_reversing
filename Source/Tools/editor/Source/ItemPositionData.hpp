#pragma once

#include <map>
#include <QList>
#include <QGraphicsItem>
#include <QRectF>
#include <QLineF>

class ResizeableArrowItem;
class ResizeableRectItem;
struct EditorCamera;
class Model;

EditorCamera* CalcContainingCamera(ResizeableRectItem* pItem, Model& model);

class ItemPositionData final
{
public:
    struct RectPos final
    {
        QRectF rect;
        EditorCamera* containingCamera = nullptr;

        bool operator == (const RectPos& rhs) const
        {
            return rect == rhs.rect && containingCamera == rhs.containingCamera;
        }
    };

    struct LinePos final
    {
        qreal x = 0;
        qreal y = 0;
        QLineF line;

        bool operator == (const LinePos& rhs) const
        {
            return x == rhs.x && y == rhs.y && line == rhs.line;
        }
    };

    void Save(QList<QGraphicsItem*>& items, Model& model, bool recalculateParentCamera);
    void Restore(Model& model);

    bool operator == (const ItemPositionData& rhs) const
    {
        if (mRects != rhs.mRects)
        {
            return false;
        }

        if (mLines != rhs.mLines)
        {
            return false;
        }

        return true;
    }

    bool operator != (const ItemPositionData& rhs) const
    {
        return !(*this == rhs);
    }

    size_t Count() const
    {
        return mRects.size() + mLines.size();
    }

    const RectPos* FirstRectPos() const
    {
        if (mRects.empty())
        {
            return nullptr;
        }
        return &mRects.begin()->second;
    }

    const LinePos* FirstLinePos() const
    {
        if (mLines.empty())
        {
            return nullptr;
        }
        return &mLines.begin()->second;
    }
private:
    void AddRect(ResizeableRectItem* pItem, Model& model, bool recalculateParentCamera);

    void AddLine(ResizeableArrowItem* pItem);

    std::map<ResizeableRectItem*, RectPos> mRects;
    std::map<ResizeableArrowItem*, LinePos> mLines;
};
