#include "SetSelectionCommand.hpp"
#include "EditorTab.hpp"
#include <QGraphicsScene>

SetSelectionCommand::SetSelectionCommand(EditorTab* pTab, QGraphicsScene* pScene, QList<QGraphicsItem*>& oldSelection, QList<QGraphicsItem*>& newSelection)
    : mTab(pTab),
    mScene(pScene),
    mOldSelection(oldSelection),
    mNewSelection(newSelection)
{
    mFirst = true;
    if (mNewSelection.count() > 0)
    {
        setText(tr("Select %1 item(s)").arg(mNewSelection.count()));
    }
    else
    {
        setText(tr("Clear selection"));
    }
}

void SetSelectionCommand::redo()
{
    if (!mFirst)
    {
        mScene->clearSelection();
        for (auto& item : mNewSelection)
        {
            item->setSelected(true);
        }
        mScene->update();
    }
    mFirst = false;
    mTab->SyncPropertyEditor();
}

void SetSelectionCommand::undo()
{
    mScene->clearSelection();
    for (auto& item : mOldSelection)
    {
        item->setSelected(true);
    }
    mScene->update();
    mTab->SyncPropertyEditor();
}
