#include "AddCollisionCommand.hpp"
#include "EditorTab.hpp"
#include "ResizeableArrowItem.hpp"
#include "CollisionObject.hpp"
#include "Model.hpp"
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

    mNewObject->SetX1(scenePos.x() + 100);
    mNewObject->SetX2(scenePos.x() + 200);

    mNewObject->SetY1(scenePos.y() + 100);
    mNewObject->SetY2(scenePos.y() + 100);

    mNewObject->SetPrevious(-1);
    mNewObject->SetNext(-1);

    mNewObject->CalculateLength();

    mArrowItem = mTab->MakeResizeableArrowItem(mNewObject.get());
}
