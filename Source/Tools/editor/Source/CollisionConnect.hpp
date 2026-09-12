#pragma once

#include <QUndoCommand>

class ResizeableArrowItem;

// Snaps the nearest pair of endpoints between exactly two collision lines together and links
// them via CollisionObject::mNext/mPrevious - fully undoable/redoable, since both the endpoint
// position that moved and the next/previous fields are captured before and after.
class CollisionConnectCommand final : public QUndoCommand
{
public:
    CollisionConnectCommand(ResizeableArrowItem* pLineA, ResizeableArrowItem* pLineB);

    void undo() override;
    void redo() override;

private:
    struct LineState final
    {
        int x1 = 0;
        int y1 = 0;
        int x2 = 0;
        int y2 = 0;
        int next = 0;
        int previous = 0;
    };

    static LineState CaptureState(ResizeableArrowItem* pLine);
    static void ApplyState(ResizeableArrowItem* pLine, const LineState& state);

    ResizeableArrowItem* mLineA = nullptr;
    ResizeableArrowItem* mLineB = nullptr;

    LineState mBeforeA;
    LineState mBeforeB;
    LineState mAfterA;
    LineState mAfterB;
};
