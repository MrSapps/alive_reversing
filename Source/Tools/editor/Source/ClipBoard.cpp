#include "ClipBoard.hpp"
#include "ResizeableArrowItem.hpp"
#include "ResizeableRectItem.hpp"
#include "Model.hpp"

void ClipBoard::Set(const QList<QGraphicsItem*>& items, Model& model)
{
    
    if (items.isEmpty())
    {
        return;
    }

    mSourceGame = model.Game();

    mMapObjects.clear();
    mCollisions.clear();

    // Deep copy the items for pasting
    for (int i = 0; i < items.count(); i++)
    {
        QGraphicsItem* obj = items.at(i);
        auto pResizeableRectItem = qgraphicsitem_cast<ResizeableRectItem*>(obj);
        if (pResizeableRectItem)
        {
            mMapObjects.emplace_back(pResizeableRectItem->GetMapObject()->Clone());
        }
        else
        {
            auto pResizeableArrowItem = qgraphicsitem_cast<ResizeableArrowItem*>(obj);
            if (pResizeableArrowItem)
            {
               mCollisions.emplace_back(std::make_unique<CollisionObject>(0, *pResizeableArrowItem->GetCollisionItem()));
            }
        }
    }
}

bool ClipBoard::IsEmpty() const
{
    return mMapObjects.empty() && mCollisions.empty();
    return true;
}

GameType ClipBoard::SourceGame() const
{
    return mSourceGame;
}

std::vector<std::unique_ptr<MapObjectBase>> ClipBoard::CloneMapObjects(QPoint* pos) const
{
    QPoint offset(50, 50);
    if (pos)
    {
        offset = *pos;
    }

    // TODO: Position set or offsetting via pos
    std::vector<std::unique_ptr<MapObjectBase>> r;
    for (auto& obj : mMapObjects)
    {
        auto copy = obj->Clone();
        copy->SetXPos(copy->XPos() + offset.x());
        copy->SetYPos(copy->YPos() + offset.y());
        r.emplace_back(std::move(copy));
    }
    return r;
}

std::vector<std::unique_ptr<CollisionObject>> ClipBoard::CloneCollisions(QPoint* /*pos*/) const
{
    // TODO: Position set or offsetting via pos

    std::vector<std::unique_ptr<CollisionObject>> r;
    for (auto& obj : mCollisions)
    {
        r.emplace_back(std::make_unique<CollisionObject>(0, *obj));
    }
    return r;
}
