#include "PasteItemsCommand.hpp"
#include "ClipBoard.hpp"
#include "EditorTab.hpp"
#include "ResizeableArrowItem.hpp"
#include "ResizeableRectItem.hpp"
#include "Model.hpp"
#include "ItemPositionData.hpp"
#include "IGridPointSnapper.hpp"

PasteItemsCommand::PasteItemsCommand(EditorTab* pTab, ClipBoard& clipBoard)
    : mTab(pTab), mSelectionSaver(pTab)
{
    // Access the clamp helpers via the interface, not EditorTab directly - EditorTab re-declares
    // them as private overrides (see ResizeableArrowItem/ResizeableRectItem for the same idiom).
    IGridPointSnapper& snapper = *mTab;

    // Make another deep copy of the items and create graphics items for them
    mCollisions = clipBoard.CloneCollisions(nullptr);
    for (auto& obj : mCollisions)
    {
        // Fix collision line ids
        obj->mId = mTab->GetModel().NextCollisionId();

        // MakeResizeableArrowItem's ResizeableArrowItem ctor calls SyncFromCollisionItem(),
        // which shrinks the line if it's bigger than the destination map and repositions it to
        // stay within bounds, writing the correction back into obj - handles a paste across
        // paths where the destination map is smaller than the one the line was copied from.
        mCollisionGraphicsObjects.emplace_back(mTab->MakeResizeableArrowItem(obj.get()));
    }

    auto clonedMapObjects = clipBoard.CloneMapObjects(nullptr);
    for (auto& obj : clonedMapObjects)
    {
        // Keep the pasted object within the map bounds as a rigid unit (same clamp a manual
        // move/resize gets) - paste previously applied no bounds check at all.
        obj->SetXPos(snapper.ClampRangeStartX(obj->XPos(), obj->Width()));
        obj->SetYPos(snapper.ClampRangeStartY(obj->YPos(), obj->Height()));

        // Create the graphics item
        ResizeableRectItem* mapObjectGraphicsItem = mTab->MakeResizeableRectItem(obj.get());

        // Ensure map object is in the correct camera
        EditorCamera* containingCamera = CalcContainingCamera(mapObjectGraphicsItem, mTab->GetModel());

        PastedMapObject pastedMapObject;
        pastedMapObject.mContainingCamera = containingCamera;
        pastedMapObject.mPastedMapObject = std::move(obj);

        // Keep track of the graphics item and the camera it lives in
        mMapObjects.emplace_back(std::move(pastedMapObject));
        mMapGraphicsObjects.emplace_back(mapObjectGraphicsItem);
    }

    setText("Paste " + QString::number(mCollisions.size() + mMapObjects.size()) +  " item(s)");
}

PasteItemsCommand::~PasteItemsCommand()
{
    // Delete copies if not added
    if (!mPasted)
    {
        for (auto& obj : mMapGraphicsObjects)
        {
            delete obj;
        }

        for (auto& obj : mCollisionGraphicsObjects)
        {
            delete obj;
        }
    }
}

void PasteItemsCommand::redo()
{
    // Add to scene
    for (auto& obj : mMapGraphicsObjects)
    {
        mTab->GetScene().addItem(obj);
    }

    // Add to model
    for (auto& obj : mMapObjects)
    {
        obj.mContainingCamera->mMapObjects.emplace_back(std::move(obj.mPastedMapObject));
    }
    mMapObjects.clear();

    // Add to scene
    for (auto& obj : mCollisionGraphicsObjects)
    {
        mTab->GetScene().addItem(obj);
    }

    // Add to model
    for (auto& obj : mCollisions)
    {
        mTab->GetModel().CollisionItems().emplace_back(std::move(obj));
    }
    mCollisions.clear();

    mTab->GetScene().clearSelection();

    // Select what was pasted
    for (auto& obj : mMapGraphicsObjects)
    {
        obj->setSelected(true);
    }

    for (auto& obj : mCollisionGraphicsObjects)
    {
        obj->setSelected(true);
    }

    mPasted = true;

    mSelectionSaver.redo();
}

void PasteItemsCommand::undo()
{
    for (auto& obj : mMapGraphicsObjects)
    {
        // Remove from model
        PastedMapObject pastedMapObject;
        pastedMapObject.mContainingCamera = mTab->GetModel().GetContainingCamera(obj->GetMapObject());
        pastedMapObject.mPastedMapObject = mTab->GetModel().TakeFromContainingCamera(obj->GetMapObject());
        
        mMapObjects.emplace_back(std::move(pastedMapObject));

        // Remove from scene
        mTab->GetScene().removeItem(obj);
    }

    for (auto& obj : mCollisionGraphicsObjects)
    {
        // Remove from model
        mCollisions.emplace_back(mTab->GetModel().RemoveCollisionItem(obj->GetCollisionItem()));

        // Remove from scene
        mTab->GetScene().removeItem(obj);
    }

    mPasted = false;

    mSelectionSaver.undo();
}
