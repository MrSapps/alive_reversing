#pragma once

#include <QUndoCommand>
#include <memory>
#include <vector>
#include "SelectionSaver.hpp"

class EditorTab;
class ClipBoard;
class ResizeableRectItem;
class ResizeableArrowItem;
struct EditorCamera;
class CollisionObject;
class MapObjectBase;

class PasteItemsCommand final : public QUndoCommand
{
public:
    PasteItemsCommand(EditorTab* pTab, ClipBoard& clipBoard);
    ~PasteItemsCommand();
    void redo() override;
    void undo() override;
private:
    EditorTab* mTab = nullptr;

    struct PastedMapObject final
    {
        std::unique_ptr<MapObjectBase> mPastedMapObject;
        EditorCamera* mContainingCamera;
    };
    std::vector<PastedMapObject> mMapObjects;
    std::vector<ResizeableRectItem*> mMapGraphicsObjects;

    std::vector<std::unique_ptr<CollisionObject>> mCollisions;
    std::vector<ResizeableArrowItem*> mCollisionGraphicsObjects;

    SelectionSaver mSelectionSaver;

    bool mPasted = false;
};
