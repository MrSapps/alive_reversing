#include "AddCollisionCommand.hpp"
#include "EditorTab.hpp"
#include "ResizeableArrowItem.hpp"
#include "CollisionObject.hpp"
#include "Model.hpp"
#include "GridPlacement.hpp"
#include <algorithm>
#include <cmath>

AddCollisionCommand::AddCollisionCommand(EditorTab* pTab, QPoint p1, QPoint p2)
 : mSelectionSaver(pTab), mTab(pTab)
{
    MakeNewCollision(p1, p2);

    setText("Add collision line");
}

AddCollisionCommand::~AddCollisionCommand()
{
    if (!mAdded)
    {
        delete mArrowItem;
    }
}

void AddCollisionCommand::undo()
{
    mTab->GetScene().removeItem(mArrowItem);

    mNewObject = mTab->GetModel().RemoveCollisionItem(mArrowItem->GetCollisionItem());

    mAdded = false;

    mSelectionSaver.undo();
}

void AddCollisionCommand::redo()
{
    mTab->GetScene().addItem(mArrowItem);
    mTab->GetModel().CollisionItems().push_back(std::move(mNewObject));

    // Set the new item as the only thing selected
    mTab->GetScene().clearSelection();
    mArrowItem->setSelected(true);

    mAdded = true;

    mSelectionSaver.redo();
}

void AddCollisionCommand::MakeNewCollision(QPoint p1, QPoint p2)
{
    mNewObject = std::make_unique<CollisionObject>(mTab->GetModel().NextCollisionId());

    const Model& model = mTab->GetModel();
    const unsigned int mapWidthPixels = model.XSize() * model.CameraGridWidth();
    const unsigned int mapHeightPixels = model.YSize() * model.CameraGridHeight();

    // Keep the whole line within the map bounds by shifting it back in as a rigid unit,
    // preserving its exact shape - clamping each endpoint independently could collapse the line
    // to a single point if its bounding box straddles a boundary (e.g. p1.x() = -180, p2.x() =
    // -80: both clamp to 0 independently, losing the line's length entirely). Same reasoning as
    // ResizeableArrowItem's whole-line drag clamp.
    const int left = std::min(p1.x(), p2.x());
    const int top = std::min(p1.y(), p2.y());
    const int width = std::abs(p2.x() - p1.x());
    const int height = std::abs(p2.y() - p1.y());
    const int clampedLeft = GridPlacement::ClampRangeStartToMapBounds(left, width, mapWidthPixels);
    const int clampedTop = GridPlacement::ClampRangeStartToMapBounds(top, height, mapHeightPixels);
    const int dx = clampedLeft - left;
    const int dy = clampedTop - top;

    mNewObject->SetX1(p1.x() + dx);
    mNewObject->SetY1(p1.y() + dy);
    mNewObject->SetX2(p2.x() + dx);
    mNewObject->SetY2(p2.y() + dy);

    mNewObject->SetPrevious(-1);
    mNewObject->SetNext(-1);

    mNewObject->CalculateLength();

    mArrowItem = mTab->MakeResizeableArrowItem(mNewObject.get());
}
