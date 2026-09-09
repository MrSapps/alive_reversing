#include "ChangeColourPickerPropertyCommand.hpp"
#include "PropertyTreeWidget.hpp"
#include "PropertyTreeItemBase.hpp"

static QString RGB16ToString(const RGB16& rgb)
{
    return QString("%1,%2,%3").arg(rgb.r).arg(rgb.g).arg(rgb.b);
}

ChangeColourPickerPropertyCommand::ChangeColourPickerPropertyCommand(PropertyTreeWidget* pTreeWidget, RGB16& pProperty, QString propertyName, RGB16 oldValue, RGB16 newValue)
    : mTreeWidget(pTreeWidget)
    , mProperty(pProperty)
    , mOldValue(oldValue)
    , mNewValue(newValue)
{
    setText(QString("Change property %1 from %2 to %3").arg(propertyName).arg(RGB16ToString(oldValue)).arg(RGB16ToString(newValue)));
}

void ChangeColourPickerPropertyCommand::undo()
{
    mProperty = mOldValue;
    mTreeWidget->FindObjectPropertyByKey(&mProperty)->Refresh();
}

void ChangeColourPickerPropertyCommand::redo()
{
    mProperty = mNewValue;
    mTreeWidget->FindObjectPropertyByKey(&mProperty)->Refresh();
}
