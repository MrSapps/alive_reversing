#pragma once

#include "PropertyTreeItemBase.hpp"

class QUndoStack;

class StringProperty : public QObject, public PropertyTreeItemBase
{
    Q_OBJECT
public:
    StringProperty(QUndoStack& undoStack, QTreeWidgetItem* pParent, QString propertyName, std::string* pProperty);

    virtual QWidget* GetEditorWidget(PropertyTreeWidget* pParent) override;

    virtual void Refresh() override;

    const void* GetPropertyLookUpKey() const override
    {
        return mProperty;
    }

private:
    std::string* mProperty = nullptr;
    QString mPrevValue;
    QUndoStack& mUndoStack;
};
