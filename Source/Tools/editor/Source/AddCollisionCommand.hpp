#pragma once

#include <QUndoCommand>
#include <QPoint>
#include <memory>
#include "SelectionSaver.hpp"

class EditorTab;
class CollisionObject;
class ResizeableArrowItem;

class AddCollisionCommand final : public QUndoCommand
{
public:
    // p1/p2 are the two user-clicked endpoints, in scene coordinates (see
    // EditorGraphicsScene::HandlePlaceCollisionLineClick) - clamped to the map bounds as a rigid
    // unit (preserving the line's exact length/shape) the same way a drag or resize is.
    AddCollisionCommand(EditorTab* pTab, QPoint p1, QPoint p2);

    ~AddCollisionCommand();

    void undo() override;

    void redo() override;

private:
    void MakeNewCollision(QPoint p1, QPoint p2);

    SelectionSaver mSelectionSaver;
    bool mAdded = false;
    EditorTab* mTab = nullptr;
    std::unique_ptr<CollisionObject> mNewObject;
    ResizeableArrowItem* mArrowItem = nullptr;
};
