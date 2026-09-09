#pragma once

#include <QUndoCommand>
#include "BoolPropertyChangeData.hpp"
#include "LinkedBoolProperty.hpp"

class ChangeBoolPropertyCommand final : public QUndoCommand
{
public:
    ChangeBoolPropertyCommand(LinkedBoolProperty linkedProperty, BoolPropertyChangeData propertyData)
        : mLinkedProperty(linkedProperty), mPropertyData(propertyData)
    {
        UpdateText();
    }

    void undo() override;

    void redo() override;

private:
    void UpdateText();
    LinkedBoolProperty mLinkedProperty;
    BoolPropertyChangeData mPropertyData;
};
