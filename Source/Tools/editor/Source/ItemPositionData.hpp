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
class IGridPointSnapper;
class QGraphicsScene;

EditorCamera* CalcContainingCamera(ResizeableRectItem* pItem, Model& model);

// Live version of ItemPositionData::ClampToMapBounds: clamps the union bounding box of whatever
// is *currently selected* in the scene to the map bounds as a rigid group, reading/writing the
// live QGraphicsItems directly rather than a captured before/after snapshot - so it can run on
// every mouseMoveEvent during a multi-item drag (not just once when the drag finishes), the same
// way a single selected item is already clamped live while being dragged alone. No Model
// dependency (unlike ItemPositionData::Save) since nothing here needs to recompute which camera
// contains an object - that's already handled separately once the drag ends. Returns true if a
// correction was applied.
bool ClampSelectedItemsToMapBounds(QGraphicsScene* scene, IGridPointSnapper& snapper);

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

    // Keeps a whole multi-item selection within the map's bounds as a rigid group: clamps the
    // union of every recorded item's bounding box to the map, and if that needed a correction,
    // shifts every item (not just one) by that same (dx, dy) - preserving the selection's shape
    // and relative layout, rather than each item clamping independently to its own bounds (which
    // is all that happened before: only the one item the user actually dragged ever got clamped;
    // Qt's own default multi-select drag moves every other selected item by the raw delta with
    // no clamping at all). Returns true if a correction was applied.
    bool ClampToMapBounds(IGridPointSnapper& snapper);

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
