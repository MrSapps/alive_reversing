#pragma once

#include <QUndoCommand>
#include <vector>
#include "CollisionConnectData.hpp"

class ResizeableArrowItem;

class CollisionConnectCommand final : public QUndoCommand
{
public:
    CollisionConnectCommand(std::vector<CollisionConnectData> collisionConnectData);

    void undo() override;

    void redo() override;

    static std::vector<CollisionConnectData> getConnectCollisionsChanges(const std::vector<ResizeableArrowItem *> &collisions);

private:
    std::vector<CollisionConnectData> mCollisionConnectData;

};
