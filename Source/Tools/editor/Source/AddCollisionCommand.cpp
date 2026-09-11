#include "AddCollisionCommand.hpp"
#include "EditorTab.hpp"
#include "ResizeableArrowItem.hpp"
#include "CollisionObject.hpp"
#include "Model.hpp"
#include "GridPlacement.hpp"
#include <QGraphicsView>

AddCollisionCommand::AddCollisionCommand(EditorTab* pTab)
 : mSelectionSaver(pTab), mTab(pTab)
{
    MakeNewCollision();

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

void AddCollisionCommand::MakeNewCollision()
{
    mNewObject = std::make_unique<CollisionObject>(mTab->GetModel().NextCollisionId());

    QGraphicsView* pView = mTab->GetScene().views().at(0);
    QPoint scenePos = pView->mapToScene(pView->pos()).toPoint();

    const Model& model = mTab->GetModel();
    const unsigned int mapWidthPixels = model.XSize() * model.CameraGridWidth();
    const unsigned int mapHeightPixels = model.YSize() * model.CameraGridHeight();

    // Originally a fixed-size 100px-long horizontal line at (scenePos + 100, scenePos + 100)
    // to (scenePos + 200, scenePos + 100). Clamping x1/x2 independently to the map bounds can
    // collapse that to a zero-length point (e.g. scenePos.x() = -280: x1 = -180 and x2 = -80
    // both clamp to 0), so the X span is clamped by its start instead, keeping the full
    // length whenever the map is at least that wide (always true here - even a 1x1 AE map is
    // 375px wide). Y1 == Y2 always (it's a horizontal line), so there's no length to preserve
    // there - a plain pixel clamp is enough.
    constexpr int kLineLength = 100;
    const int x1 = GridPlacement::ClampRangeStartToMapBounds(scenePos.x() + 100, kLineLength, mapWidthPixels);
    const int x2 = GridPlacement::ClampPixelToMapBounds(x1 + kLineLength, mapWidthPixels);
    const int y = GridPlacement::ClampPixelToMapBounds(scenePos.y() + 100, mapHeightPixels);

    mNewObject->SetX1(x1);
    mNewObject->SetX2(x2);
    mNewObject->SetY1(y);
    mNewObject->SetY2(y);

    mNewObject->SetPrevious(-1);
    mNewObject->SetNext(-1);

    mNewObject->CalculateLength();

    mArrowItem = mTab->MakeResizeableArrowItem(mNewObject.get());
}
