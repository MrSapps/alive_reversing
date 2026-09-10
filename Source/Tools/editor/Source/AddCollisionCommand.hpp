#pragma once

#include <QUndoCommand>
#include <memory>
#include "SelectionSaver.hpp"

class EditorTab;
class CollisionObject;
class ResizeableArrowItem;

class AddCollisionCommand final : public QUndoCommand
{
public:
    explicit AddCollisionCommand(EditorTab* pTab);

    ~AddCollisionCommand();

    void undo() override;

    void redo() override;

private:
    void MakeNewCollision();

    SelectionSaver mSelectionSaver;
    bool mAdded = false;
    EditorTab* mTab = nullptr;
    std::unique_ptr<CollisionObject> mNewObject;
    ResizeableArrowItem* mArrowItem = nullptr;
};
