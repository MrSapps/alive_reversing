#include "ChangeStringPropertyCommand.hpp"
#include "PropertyTreeWidget.hpp"
#include "PropertyTreeItemBase.hpp"

ChangeStringPropertyCommand::ChangeStringPropertyCommand(PropertyTreeWidget* pTreeWidget, std::string* pProperty, QString propertyName, QString oldValue, QString newValue)
    : mTreeWidget(pTreeWidget), mProperty(pProperty), mOldValue(oldValue), mNewValue(newValue)
{
    setText(QString("Change property %1 from %2 to %3").arg(propertyName.trimmed(), oldValue, newValue));
}

void ChangeStringPropertyCommand::undo()
{
    *mProperty = mOldValue.toStdString();
    mTreeWidget->FindObjectPropertyByKey(mProperty)->Refresh();
}

void ChangeStringPropertyCommand::redo()
{
    *mProperty = mNewValue.toStdString();
    mTreeWidget->FindObjectPropertyByKey(mProperty)->Refresh();
}
