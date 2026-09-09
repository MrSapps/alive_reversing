#include "ReadOnlyStringProperty.hpp"

ReadOnlyStringProperty::ReadOnlyStringProperty(QTreeWidgetItem* pParent, QString propertyName, QString propertyValue)
    : PropertyTreeItemBase(pParent, QStringList{ propertyName, propertyValue })
{

}
