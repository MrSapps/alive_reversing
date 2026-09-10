#pragma once

#include <QUndoCommand>
#include <QList>
#include <QGraphicsItem>
#include <QCoreApplication>

class EditorTab;
class QGraphicsScene;

class SetSelectionCommand final : public QUndoCommand
{
    Q_DECLARE_TR_FUNCTIONS(SetSelectionCommand)

public:
    SetSelectionCommand(EditorTab* pTab, QGraphicsScene* pScene, QList<QGraphicsItem*>& oldSelection, QList<QGraphicsItem*>& newSelection);

    void redo() override;

    void undo() override;

private:
    EditorTab* mTab = nullptr;
    QGraphicsScene* mScene = nullptr;
    QList<QGraphicsItem*> mOldSelection;
    QList<QGraphicsItem*> mNewSelection;
    bool mFirst = false;
};
