#include "ItemPositionData.hpp"
#include "ResizeableArrowItem.hpp"
#include "ResizeableRectItem.hpp"
#include "Model.hpp"
#include "IGridPointSnapper.hpp"
#include <QGraphicsScene>
#include <algorithm>

EditorCamera* CalcContainingCamera(ResizeableRectItem* pItem, Model& model)
{
    QPoint midPoint = pItem->CurrentRect().center().toPoint();

    int camX = midPoint.x() / model.CameraGridWidth();
    if (camX < 0)
    {
        camX = 0;
    }

    if (camX >= static_cast<int>(model.XSize()))
    {
        camX = model.XSize() - 1;
    }

    int camY = midPoint.y() / model.CameraGridHeight();
    if (camY < 0)
    {
        camY = 0;
    }

    if (camY >= static_cast<int>(model.YSize()))
    {
        camY = model.YSize() - 1;
    }

    return model.CameraAt(camX, camY);
}

void ItemPositionData::Save(QList<QGraphicsItem*>& items, Model& model, bool recalculateParentCamera)
{
    mRects.clear();
    mLines.clear();

    for (auto& item : items)
    {
        ResizeableRectItem* pRect = qgraphicsitem_cast<ResizeableRectItem*>(item);
        if (pRect)
        {
            AddRect(pRect, model, recalculateParentCamera);
        }
        else
        {
            ResizeableArrowItem* pArrow = qgraphicsitem_cast<ResizeableArrowItem*>(item);
            if (pArrow)
            {
                AddLine(pArrow);
            }
        }
    }
}

void ItemPositionData::Restore(Model& model)
{
    for (auto& [rect, pos] : mRects)
    {
        rect->SetRect(pos.rect);
        model.SwapContainingCamera(rect->GetMapObject(), pos.containingCamera);
    }

    for (auto& [line, pos] : mLines)
    {
        line->RestoreLine(pos.line);
        line->setX(pos.x);
        line->setY(pos.y);
    }
}

bool ItemPositionData::ClampToMapBounds(IGridPointSnapper& snapper)
{
    if (mRects.empty() && mLines.empty())
    {
        return false;
    }

    bool haveBBox = false;
    QRectF bbox;
    auto includeRect = [&](const QRectF& r)
    {
        bbox = haveBBox ? bbox.united(r) : r;
        haveBBox = true;
    };

    for (auto& [item, pos] : mRects)
    {
        includeRect(pos.rect);
    }
    for (auto& [item, pos] : mLines)
    {
        const QPointF p1 = QPointF(pos.x, pos.y) + pos.line.p1();
        const QPointF p2 = QPointF(pos.x, pos.y) + pos.line.p2();
        includeRect(QRectF(p1, p2).normalized());
    }

    const int left = static_cast<int>(bbox.left());
    const int top = static_cast<int>(bbox.top());
    const int width = static_cast<int>(bbox.width());
    const int height = static_cast<int>(bbox.height());

    const int clampedLeft = snapper.ClampRangeStartX(left, width);
    const int clampedTop = snapper.ClampRangeStartY(top, height);
    const int dx = clampedLeft - left;
    const int dy = clampedTop - top;

    if (dx == 0 && dy == 0)
    {
        return false;
    }

    for (auto& [item, pos] : mRects)
    {
        pos.rect.translate(dx, dy);
        item->SetRect(pos.rect);
    }
    for (auto& [item, pos] : mLines)
    {
        pos.x += dx;
        pos.y += dy;
        item->Translate(dx, dy);
    }

    return true;
}

bool ClampSelectedItemsToMapBounds(QGraphicsScene* scene, IGridPointSnapper& snapper)
{
    QList<ResizeableRectItem*> rects;
    QList<ResizeableArrowItem*> lines;
    for (QGraphicsItem* item : scene->selectedItems())
    {
        if (auto* pRect = qgraphicsitem_cast<ResizeableRectItem*>(item))
        {
            rects.append(pRect);
        }
        else if (auto* pLine = qgraphicsitem_cast<ResizeableArrowItem*>(item))
        {
            lines.append(pLine);
        }
    }

    if (rects.empty() && lines.empty())
    {
        return false;
    }

    bool haveBBox = false;
    QRectF bbox;
    auto includeRect = [&](const QRectF& r)
    {
        bbox = haveBBox ? bbox.united(r) : r;
        haveBBox = true;
    };

    for (auto* pRect : rects)
    {
        includeRect(pRect->CurrentRect());
    }
    for (auto* pLine : lines)
    {
        const QLineF line = pLine->SaveLine();
        const QPointF p1 = QPointF(pLine->x(), pLine->y()) + line.p1();
        const QPointF p2 = QPointF(pLine->x(), pLine->y()) + line.p2();
        includeRect(QRectF(p1, p2).normalized());
    }

    const int left = static_cast<int>(bbox.left());
    const int top = static_cast<int>(bbox.top());
    const int width = static_cast<int>(bbox.width());
    const int height = static_cast<int>(bbox.height());

    const int clampedLeft = snapper.ClampRangeStartX(left, width);
    const int clampedTop = snapper.ClampRangeStartY(top, height);
    const int dx = clampedLeft - left;
    const int dy = clampedTop - top;

    if (dx == 0 && dy == 0)
    {
        return false;
    }

    for (auto* pRect : rects)
    {
        pRect->SetRect(pRect->CurrentRect().translated(dx, dy));
    }
    for (auto* pLine : lines)
    {
        pLine->Translate(dx, dy);
    }

    return true;
}

void ItemPositionData::AddRect(ResizeableRectItem* pItem, Model& model, bool recalculateParentCamera)
{
    EditorCamera* pContainingCamera = nullptr;
    MapObjectBase* pMapObject = pItem->GetMapObject();

    if (recalculateParentCamera)
    {
        pContainingCamera = CalcContainingCamera(pItem, model);

       model.SwapContainingCamera(pMapObject, pContainingCamera);
    }
    else
    {
        pContainingCamera = model.GetContainingCamera(pMapObject);
    }

    mRects[pItem] = { pItem->CurrentRect(), pContainingCamera };
}

void ItemPositionData::AddLine(ResizeableArrowItem* pItem)
{
    mLines[pItem] = { pItem->x(), pItem->y(), pItem->SaveLine() };
}
