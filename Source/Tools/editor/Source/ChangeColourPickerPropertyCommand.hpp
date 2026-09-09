#pragma once

#include <QUndoCommand>
#include "../../../relive_lib/RGB16.hpp"

class PropertyTreeWidget;

class ChangeColourPickerPropertyCommand : public QUndoCommand
{
public:
    ChangeColourPickerPropertyCommand(PropertyTreeWidget* pTreeWidget, RGB16& pProperty, QString propertyName, RGB16 oldValue, RGB16 newValue);

    void undo() override;

    void redo() override;

private:
    PropertyTreeWidget* mTreeWidget = nullptr;
    RGB16& mProperty;
    RGB16 mOldValue;
    RGB16 mNewValue;
};
