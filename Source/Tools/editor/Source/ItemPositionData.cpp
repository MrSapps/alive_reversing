#include "ItemPositionData.hpp"
#include "ResizeableArrowItem.hpp"
#include "ResizeableRectItem.hpp"
#include "Model.hpp"

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
