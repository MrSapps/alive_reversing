#pragma once

#include <QList>
#include <QGraphicsItem>
#include <memory>
#include <vector>
#include "../../../relive_lib/GameType.hpp"

class Model;
class MapObjectBase;
class CollisionObject;

class ClipBoard final
{
public:
    void Set(const QList<QGraphicsItem*>& items, Model& model);
    bool IsEmpty() const;

    GameType SourceGame() const;

    std::vector<std::unique_ptr<MapObjectBase>> CloneMapObjects(QPoint* pos) const;
    std::vector<std::unique_ptr<CollisionObject>> CloneCollisions(QPoint* pos) const;
private:
    GameType mSourceGame;

    std::vector<std::unique_ptr<CollisionObject>> mCollisions;
    std::vector<std::unique_ptr<MapObjectBase>> mMapObjects;
};
