#pragma once

#include <QUndoCommand>
#include <QUndoStack>
#include <QtGlobal>
#include "BasicTypePropertyChangeData.hpp"
#include "LinkedBasicTypeProperty.hpp"

class ChangeBasicTypePropertyCommand final : public QUndoCommand
{
public:
    ChangeBasicTypePropertyCommand(LinkedBasicTypeProperty linkedProperty, BasicTypePropertyChangeData propertyData);

    void undo() override;

    void redo() override;

    int id() const override
    {
        return 1;
    }

    bool mergeWith(const QUndoCommand* command) override;

    // Applies the edit (redo() runs immediately, same as QUndoStack::push() would do) and only
    // then pushes it onto stack - if SyncInternalObject's correction (e.g. ResizeableRectItem's
    // map-bounds/minimum-size clamp) fully absorbed the requested change, leaving the property
    // exactly where it started, there is nothing to undo and the command is discarded instead of
    // left sitting on the stack as a no-op "from X to X" entry. The property's display is still
    // corrected either way, since that happens inside the redo() call above regardless of what's
    // decided afterward.
    static void PushIfEffective(QUndoStack& stack, LinkedBasicTypeProperty linkedProperty, BasicTypePropertyChangeData propertyData);

private:
    void UpdateText();
    LinkedBasicTypeProperty mLinkedProperty;
    BasicTypePropertyChangeData mPropertyData;
    qint64 mTimeStamp = 0;
};
