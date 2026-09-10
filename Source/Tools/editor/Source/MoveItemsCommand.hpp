#pragma once

#include <QUndoCommand>
#include <QCoreApplication>
#include "ItemPositionData.hpp"

class QGraphicsScene;
class Model;

class MoveItemsCommand final : public QUndoCommand
{
    Q_DECLARE_TR_FUNCTIONS(MoveItemsCommand)

public:
    MoveItemsCommand(QGraphicsScene* pScene, ItemPositionData oldPositions, ItemPositionData newPositions, Model& model);

    void redo() override;

    void undo() override;

private:
    QGraphicsScene* mScene = nullptr;
    ItemPositionData mOldPositions;
    ItemPositionData mNewPositions;
    Model& mModel;
    bool mFirst = false;
};
