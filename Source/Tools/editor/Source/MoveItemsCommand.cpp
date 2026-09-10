#include "MoveItemsCommand.hpp"
#include <QGraphicsScene>

MoveItemsCommand::MoveItemsCommand(QGraphicsScene* pScene, ItemPositionData oldPositions, ItemPositionData newPositions, Model& model)
    : mScene(pScene),
    mOldPositions(oldPositions),
    mNewPositions(newPositions),
    mModel(model)
{
    mFirst = true;

    if (mNewPositions.Count() == 1)
    {
        auto pNewLine = mNewPositions.FirstLinePos();
        auto pOldRect = mOldPositions.FirstRectPos();
        if (pNewLine)
        {
            auto pOldLine = mOldPositions.FirstLinePos();
            const bool posChange = pOldLine->x != pNewLine->x || pOldLine->y != pNewLine->y;
            const bool lineChanged = pOldLine->line != pNewLine->line;
            if (posChange && lineChanged)
            {
                setText(tr("Move and resize collision"));
            }
            else if (posChange && !lineChanged)
            {
                setText(tr("Move collision"));
            }
            else
            {
                setText(tr("Move collision point"));
            }
        }
        else
        {
            auto pNewRect = mNewPositions.FirstRectPos();
            const bool xOryChanged = pOldRect->rect.x() != pNewRect->rect.x() || pOldRect->rect.y() != pNewRect->rect.y();
            const bool wOrhChanged = pOldRect->rect.width() != pNewRect->rect.width() || pOldRect->rect.height() != pNewRect->rect.height();
            if (xOryChanged && wOrhChanged)
            {
                setText(tr("Move and resize map object"));
            }
            else if (xOryChanged && !wOrhChanged)
            {
                setText(tr("Move map object"));
            }
            else // only wOrhChanged
            {
                setText(tr("Resize map object"));
            }
        }
    }
    else
    {
        setText(tr("Move %1 item(s)").arg(mNewPositions.Count()));
    }
}

void MoveItemsCommand::redo()
{
    if (!mFirst)
    {
        mNewPositions.Restore(mModel);
        mScene->update();
    }
    mFirst = false;
}

void MoveItemsCommand::undo()
{
    mOldPositions.Restore(mModel);
    mScene->update();
}
