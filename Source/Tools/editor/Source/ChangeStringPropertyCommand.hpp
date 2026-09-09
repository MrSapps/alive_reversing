#pragma once

#include <QUndoCommand>
#include <QString>
#include <string>

class PropertyTreeWidget;

class ChangeStringPropertyCommand : public QUndoCommand
{
public:
    ChangeStringPropertyCommand(PropertyTreeWidget* pTreeWidget, std::string* pProperty, QString propertyName, QString oldValue, QString newValue);

    void undo() override;

    void redo() override;

private:
    PropertyTreeWidget* mTreeWidget = nullptr;
    std::string* mProperty = nullptr;
    QString mOldValue;
    QString mNewValue;
};
