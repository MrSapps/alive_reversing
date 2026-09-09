#pragma once

#include <QUndoCommand>
#include "EnumPropertyChangeData.hpp"
#include "LinkedEnumProperty.hpp"

class ChangeEnumPropertyCommand : public QUndoCommand
{
public:
    ChangeEnumPropertyCommand(LinkedEnumProperty linkedProperty, EnumPropertyChangeData propertyData);

    void undo() override;

    void redo() override;

private:
    LinkedEnumProperty mLinkedProperty;
    EnumPropertyChangeData mPropertyData;
};
