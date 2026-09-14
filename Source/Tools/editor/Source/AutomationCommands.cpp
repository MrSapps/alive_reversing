#include "AutomationCommands.hpp"
#include "BigSpinBox.hpp"

#include <algorithm>
#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidget>
#include <QMouseEvent>
#include <QPixmap>
#include <QSpinBox>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QWidget>

namespace Automation
{
    namespace
    {
        QObject* ResolveTargetByName(QWidget* root, const std::string& name)
        {
            // Special-cased rather than an objectName: dialogs opened via static convenience
            // functions (QInputDialog::getInt/getItem, QMessageBox::*) are anonymous - Qt
            // never gives them an objectName - so there is no string to findChildren() for.
            // This resolves whatever such dialog is currently on screen instead.
            if (name == "@active_modal")
            {
                QWidget* modal = QApplication::activeModalWidget();
                if (!modal)
                {
                    throw CommandError("no active modal dialog");
                }
                return modal;
            }

            // Same idea as "@active_modal", one level deeper: QMessageBox's standard buttons
            // (Save/Discard/Cancel, ...) get neither an objectName nor a parent under the main
            // window (QMessageBox is often constructed with no parent at all), so they can't be
            // reached by name or by descending from root either. Find by visible text instead.
            static const std::string kActiveModalButtonPrefix = "@active_modal_button:";
            if (name.rfind(kActiveModalButtonPrefix, 0) == 0)
            {
                QWidget* modal = QApplication::activeModalWidget();
                if (!modal)
                {
                    throw CommandError("no active modal dialog");
                }

                const QString wantedText = QString::fromStdString(name.substr(kActiveModalButtonPrefix.size()));
                QAbstractButton* found = nullptr;
                int matchCount = 0;
                for (QAbstractButton* button : modal->findChildren<QAbstractButton*>())
                {
                    QString text = button->text();
                    text.remove(QLatin1Char('&')); // strip Qt's mnemonic marker, e.g. "&Discard"
                    if (text.compare(wantedText, Qt::CaseInsensitive) == 0)
                    {
                        found = button;
                        ++matchCount;
                    }
                }

                if (matchCount == 0)
                {
                    throw CommandError("no button with text '" + wantedText.toStdString() + "' in active modal dialog");
                }
                if (matchCount > 1)
                {
                    throw CommandError("ambiguous button text '" + wantedText.toStdString() + "' (" + std::to_string(matchCount) + " matches)");
                }
                return found;
            }

            // Same idea again: QInputDialog::getText's internal QLineEdit (getInt's QSpinBox,
            // getItem's QComboBox) is anonymous too - resolves to whichever one is present in
            // the active modal, so "set_value" can fill it in directly instead of only ever
            // accepting whatever default value the dialog was opened with.
            if (name == "@active_modal_input")
            {
                QWidget* modal = QApplication::activeModalWidget();
                if (!modal)
                {
                    throw CommandError("no active modal dialog");
                }
                if (QWidget* w = modal->findChild<QLineEdit*>())
                {
                    return w;
                }
                if (QWidget* w = modal->findChild<QSpinBox*>())
                {
                    return w;
                }
                if (QWidget* w = modal->findChild<QComboBox*>())
                {
                    return w;
                }
                throw CommandError("no input widget found in active modal dialog");
            }

            const QString qname = QString::fromStdString(name);
            const QList<QObject*> matches = root->findChildren<QObject*>(qname);
            if (matches.isEmpty())
            {
                throw CommandError("not found: " + name);
            }
            if (matches.size() > 1)
            {
                throw CommandError("ambiguous target (" + std::to_string(matches.size()) + " matches): " + name);
            }
            return matches.first();
        }

        // Commands where omitting "target" means "the root window itself".
        QObject* ResolveTargetOptional(QWidget* root, const nlohmann::json& request)
        {
            if (!request.contains("target") || request.at("target").is_null())
            {
                return root;
            }
            return ResolveTargetByName(root, request.at("target").get<std::string>());
        }

        // Commands where "target" must be present.
        QObject* ResolveTargetRequired(QWidget* root, const nlohmann::json& request)
        {
            if (!request.contains("target") || request.at("target").is_null())
            {
                throw CommandError("'target' is required");
            }
            return ResolveTargetByName(root, request.at("target").get<std::string>());
        }

        QWidget* RequireWidget(QObject* obj)
        {
            QWidget* widget = qobject_cast<QWidget*>(obj);
            if (!widget)
            {
                throw CommandError("target is not a widget");
            }
            return widget;
        }

        // QAbstractScrollArea-derived widgets (QGraphicsView among them) actually receive
        // mouse events on an internal viewport() child widget, not the widget itself - sending
        // synthesized events directly to the scroll area would silently never reach whatever
        // it's showing (e.g. a QGraphicsScene's items). Redirect so click/drag both work there.
        QWidget* ResolveMouseEventTarget(QWidget* widget)
        {
            if (auto* scrollArea = qobject_cast<QAbstractScrollArea*>(widget))
            {
                return scrollArea->viewport();
            }
            return widget;
        }

        nlohmann::json HandleClick(QWidget* root, const nlohmann::json& request)
        {
            QObject* target = ResolveTargetRequired(root, request);

            if (auto* action = qobject_cast<QAction*>(target))
            {
                action->trigger();
                return nlohmann::json::object();
            }

            QWidget* widget = RequireWidget(target);

            if (auto* button = qobject_cast<QAbstractButton*>(widget))
            {
                button->click();
                return nlohmann::json::object();
            }

            QWidget* eventTarget = ResolveMouseEventTarget(widget);
            const QPoint center = widget->rect().center();
            QMouseEvent press(QEvent::MouseButtonPress, center, widget->mapToGlobal(center), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, center, widget->mapToGlobal(center), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(eventTarget, &press);
            QCoreApplication::sendEvent(eventTarget, &release);
            return nlohmann::json::object();
        }

        // Clicks a QTreeWidget cell identified by its row's column-0 text (e.g. a property
        // panel row's field name, or a nested mod-tree row - searched at every depth via
        // QTreeWidgetItemIterator, not just top-level rows, since a tree like ModTreeWidget's
        // nests levels/paths/cameras/etc. several deep) rather than a pixel position or row
        // index, since a dynamic tree's rows/order aren't known ahead of time. Synthesizes real
        // mouse events at that cell so the widget's own handling fires exactly as it would for a
        // real interaction (e.g. PropertyTreeWidget's itemPressed handler, which is what spawns
        // a row's transient editor widget - see BasicTypeProperty::GetEditorWidget/"set_value").
        // "double_click"/"right_click" (booleans, default false, mutually exclusive) select a
        // double-click (ModTreeWidget's expand/open) or a context-menu event (its right-click
        // "New Level.../New Path..." menu) at that same cell instead of a plain click.
        nlohmann::json HandleClickTreeItem(QWidget* root, const nlohmann::json& request)
        {
            auto* tree = qobject_cast<QTreeWidget*>(RequireWidget(ResolveTargetRequired(root, request)));
            if (!tree)
            {
                throw CommandError("target is not a QTreeWidget");
            }
            if (!request.contains("row_text"))
            {
                throw CommandError("'row_text' is required");
            }

            const QString wanted = QString::fromStdString(request.at("row_text").get<std::string>());
            const int column = request.value("column", 1);

            QTreeWidgetItem* found = nullptr;
            for (QTreeWidgetItemIterator it(tree); *it; ++it)
            {
                if ((*it)->text(0) == wanted)
                {
                    found = *it;
                    break;
                }
            }
            if (!found)
            {
                throw CommandError("no tree row with column-0 text: " + wanted.toStdString());
            }

            // QTreeWidget::indexFromItem is protected, so build the target cell's rect by hand
            // from visualItemRect (column 0's row rect - public) combined with the requested
            // column's own viewport position/width (also public), rather than going through a
            // QModelIndex at all.
            const QRect rowRect = tree->visualItemRect(found);
            // Column 0 dead center can land on the expand/collapse branch arrow for a nested,
            // indented item (its horizontal position depends on depth) - a real left-click there
            // toggles expansion as an unrelated side effect of Qt's own item-view handling,
            // before whatever this call actually asked for even happens. Bias toward the right
            // edge of the cell instead, safely past any indentation, for column 0 only - other
            // columns (e.g. the property panel's value column) have no such decoration and keep
            // the existing dead-center targeting.
            const int cellLeft = tree->columnViewportPosition(column);
            const int cellWidth = tree->columnWidth(column);
            const int x = column == 0 ? (cellLeft + cellWidth - std::min(10, cellWidth / 2)) : (cellLeft + cellWidth / 2);
            const QPoint center(x, rowRect.center().y());
            QWidget* viewport = tree->viewport();

            if (request.value("right_click", false))
            {
                QContextMenuEvent event(QContextMenuEvent::Mouse, center, viewport->mapToGlobal(center));
                QCoreApplication::sendEvent(viewport, &event);
                return nlohmann::json::object();
            }

            QMouseEvent press(QEvent::MouseButtonPress, center, viewport->mapToGlobal(center), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, center, viewport->mapToGlobal(center), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(viewport, &press);
            QCoreApplication::sendEvent(viewport, &release);

            if (request.value("double_click", false))
            {
                // Real Qt event sequence for a double-click is press, release, DoubleClick,
                // release (not two full press/release pairs) - QAbstractItemView's
                // mouseDoubleClickEvent is what actually emits doubleClicked()/itemDoubleClicked.
                QMouseEvent dblClick(QEvent::MouseButtonDblClick, center, viewport->mapToGlobal(center), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release2(QEvent::MouseButtonRelease, center, viewport->mapToGlobal(center), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(viewport, &dblClick);
                QCoreApplication::sendEvent(viewport, &release2);
            }
            return nlohmann::json::object();
        }

        nlohmann::json DescribeTreeItem(QTreeWidgetItem* item)
        {
            nlohmann::json j = nlohmann::json::object();
            j["text"] = item->text(0).toStdString();
            j["expanded"] = item->isExpanded();
            j["selected"] = item->isSelected();
            nlohmann::json children = nlohmann::json::array();
            for (int i = 0; i < item->childCount(); ++i)
            {
                children.push_back(DescribeTreeItem(item->child(i)));
            }
            j["children"] = children;
            return j;
        }

        // Dumps a QTreeWidget's full row hierarchy (column-0 text/expanded/selected + children,
        // recursively) as JSON - QTreeWidgetItem isn't a QObject, so "get_state"'s
        // QObject::children()-based widget tree can't see into it at all; this is the only way
        // a test can inspect what a tree (e.g. ModTreeWidget) actually shows.
        nlohmann::json HandleGetTreeItems(QWidget* root, const nlohmann::json& request)
        {
            auto* tree = qobject_cast<QTreeWidget*>(RequireWidget(ResolveTargetRequired(root, request)));
            if (!tree)
            {
                throw CommandError("target is not a QTreeWidget");
            }

            nlohmann::json items = nlohmann::json::array();
            for (int i = 0; i < tree->topLevelItemCount(); ++i)
            {
                items.push_back(DescribeTreeItem(tree->topLevelItem(i)));
            }

            nlohmann::json result = nlohmann::json::object();
            result["items"] = items;
            return result;
        }

        // Clicks a QAbstractSpinBox's up/down step buttons programmatically (stepUp()/stepDown()
        // are the exact slots those buttons invoke) - e.g. to check a spin box's own step
        // clamping independently of typing a value directly (see BigSpinBox::stepBy).
        nlohmann::json HandleSpinStep(QWidget* root, const nlohmann::json& request)
        {
            auto* spin = qobject_cast<QAbstractSpinBox*>(RequireWidget(ResolveTargetRequired(root, request)));
            if (!spin)
            {
                throw CommandError("target is not a QAbstractSpinBox");
            }
            if (!request.contains("direction"))
            {
                throw CommandError("'direction' is required");
            }

            const std::string direction = request.at("direction").get<std::string>();
            const int count = request.value("count", 1);
            for (int i = 0; i < count; ++i)
            {
                if (direction == "down")
                {
                    spin->stepDown();
                }
                else if (direction == "up")
                {
                    spin->stepUp();
                }
                else
                {
                    throw CommandError("unknown 'direction': " + direction);
                }
            }
            return nlohmann::json::object();
        }

        // Sends a single mouse press, move, or release event to a target - the building block
        // "drag" is a convenience wrapper around (press+move+release in one call). Exists so a
        // test can inspect state *between* press and release (e.g. get_scene_items mid-drag),
        // which "drag" can't do since it runs all three synchronously with no way to observe
        // anything in between.
        nlohmann::json HandleMouseEvent(QWidget* root, const nlohmann::json& request)
        {
            QWidget* widget = RequireWidget(ResolveTargetRequired(root, request));
            if (!request.contains("phase") || !request.contains("x") || !request.contains("y"))
            {
                throw CommandError("'phase', 'x' and 'y' are required");
            }

            const std::string phase = request.at("phase").get<std::string>();
            const QPoint pos(request.at("x").get<int>(), request.at("y").get<int>());
            QWidget* eventTarget = ResolveMouseEventTarget(widget);

            if (phase == "down")
            {
                QMouseEvent press(QEvent::MouseButtonPress, pos, eventTarget->mapToGlobal(pos), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QCoreApplication::sendEvent(eventTarget, &press);
            }
            else if (phase == "move")
            {
                QMouseEvent move(QEvent::MouseMove, pos, eventTarget->mapToGlobal(pos), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                QCoreApplication::sendEvent(eventTarget, &move);
            }
            else if (phase == "up")
            {
                QMouseEvent release(QEvent::MouseButtonRelease, pos, eventTarget->mapToGlobal(pos), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(eventTarget, &release);
            }
            else
            {
                throw CommandError("unknown 'phase': " + phase);
            }

            return nlohmann::json::object();
        }

        nlohmann::json HandleDrag(QWidget* root, const nlohmann::json& request)
        {
            QWidget* widget = RequireWidget(ResolveTargetRequired(root, request));
            if (!request.contains("from") || !request.contains("to"))
            {
                throw CommandError("'from' and 'to' are required");
            }

            auto readPoint = [](const nlohmann::json& j) -> QPoint
            {
                return QPoint(j.at("x").get<int>(), j.at("y").get<int>());
            };
            const QPoint from = readPoint(request.at("from"));
            const QPoint to = readPoint(request.at("to"));

            QWidget* eventTarget = ResolveMouseEventTarget(widget);

            // A single move straight from press to release is enough: QGraphicsItem's default
            // move handling computes displacement from the original button-down position (not
            // the previous move event), and item-level drag handlers here only look at each
            // move event's final position.
            QMouseEvent press(QEvent::MouseButtonPress, from, eventTarget->mapToGlobal(from), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(eventTarget, &press);

            QMouseEvent move(QEvent::MouseMove, to, eventTarget->mapToGlobal(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(eventTarget, &move);

            QMouseEvent release(QEvent::MouseButtonRelease, to, eventTarget->mapToGlobal(to), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(eventTarget, &release);

            return nlohmann::json::object();
        }

        // Synthesizes a right-click (QContextMenuEvent) at a target - e.g. so a test can drive
        // graphicsView's real "right-click a camera -> Edit camera" menu instead of a
        // domain-specific automation-only bypass. Like "mouse_event"/"drag", this can open a
        // QMenu::exec() that doesn't return until the menu is dismissed - the caller must send
        // this via SendCommand (not Call) and only collect its response after the menu has been
        // driven and closed (see AboutDialog-style "@active_modal" tests for the same pattern
        // with QDialog::exec()).
        nlohmann::json HandleContextMenu(QWidget* root, const nlohmann::json& request)
        {
            QWidget* widget = RequireWidget(ResolveTargetRequired(root, request));
            if (!request.contains("x") || !request.contains("y"))
            {
                throw CommandError("'x' and 'y' are required");
            }

            const QPoint pos(request.at("x").get<int>(), request.at("y").get<int>());
            QWidget* eventTarget = ResolveMouseEventTarget(widget);

            QContextMenuEvent event(QContextMenuEvent::Mouse, pos, eventTarget->mapToGlobal(pos));
            QCoreApplication::sendEvent(eventTarget, &event);
            return nlohmann::json::object();
        }

        nlohmann::json HandleSetValue(QWidget* root, const nlohmann::json& request)
        {
            QWidget* widget = RequireWidget(ResolveTargetRequired(root, request));
            if (!request.contains("value"))
            {
                throw CommandError("'value' is required");
            }
            const nlohmann::json& value = request.at("value");

            if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
            {
                lineEdit->setText(QString::fromStdString(value.is_string() ? value.get<std::string>() : value.dump()));
            }
            else if (auto* spin = qobject_cast<QSpinBox*>(widget))
            {
                spin->setValue(value.get<int>());
            }
            else if (auto* dspin = qobject_cast<QDoubleSpinBox*>(widget))
            {
                dspin->setValue(value.get<double>());
            }
            else if (auto* check = qobject_cast<QCheckBox*>(widget))
            {
                check->setChecked(value.get<bool>());
            }
            else if (auto* combo = qobject_cast<QComboBox*>(widget))
            {
                combo->setCurrentText(QString::fromStdString(value.get<std::string>()));
            }
            else if (auto* listWidget = qobject_cast<QListWidget*>(widget))
            {
                if (value.is_number_integer())
                {
                    listWidget->setCurrentRow(value.get<int>());
                }
                else
                {
                    const QString text = QString::fromStdString(value.get<std::string>());
                    const QList<QListWidgetItem*> matches = listWidget->findItems(text, Qt::MatchExactly);
                    if (matches.isEmpty())
                    {
                        throw CommandError("no list item matching: " + text.toStdString());
                    }
                    listWidget->setCurrentItem(matches.first());
                }
            }
            else if (auto* bigSpin = qobject_cast<BigSpinBox*>(widget))
            {
                // The property panel's integer editor (BasicTypeProperty). setValue(v, true)
                // emits valueChanged(v, false) - same signal BasicTypeProperty::GetEditorWidget
                // connects to push a ChangeBasicTypePropertyCommand a real "type a value, press
                // Enter" would via BigSpinBox::OnEditComplete (which only differs in emitting
                // closeEditor=true, cosmetic - whether the transient editor widget is removed).
                bigSpin->setValue(value.get<qint64>(), true);
            }
            else
            {
                widget->setProperty("text", QString::fromStdString(value.is_string() ? value.get<std::string>() : value.dump()));
            }
            return nlohmann::json::object();
        }

        nlohmann::json HandleSendKey(QWidget* root, const nlohmann::json& request)
        {
            QWidget* widget = RequireWidget(ResolveTargetRequired(root, request));
            if (!request.contains("key"))
            {
                throw CommandError("'key' is required");
            }

            const QKeySequence seq(QString::fromStdString(request.at("key").get<std::string>()));
            if (seq.isEmpty())
            {
                throw CommandError("invalid key sequence");
            }

            // Qt5's QKeySequence::operator[] packs the key and its modifiers into one int.
            const int combined = seq[0];
            const auto mods = static_cast<Qt::KeyboardModifiers>(combined & Qt::KeyboardModifierMask);
            const int key = combined & ~Qt::KeyboardModifierMask;

            QKeyEvent press(QEvent::KeyPress, key, mods);
            QKeyEvent release(QEvent::KeyRelease, key, mods);
            QCoreApplication::sendEvent(widget, &press);
            QCoreApplication::sendEvent(widget, &release);
            return nlohmann::json::object();
        }

        nlohmann::json DescribeObject(QObject* obj)
        {
            nlohmann::json j = nlohmann::json::object();
            j["objectName"] = obj->objectName().toStdString();
            j["className"] = obj->metaObject()->className();

            if (auto* action = qobject_cast<QAction*>(obj))
            {
                j["text"] = action->text().toStdString();
                j["enabled"] = action->isEnabled();
                j["checkable"] = action->isCheckable();
                j["checked"] = action->isChecked();
            }
            else if (auto* widget = qobject_cast<QWidget*>(obj))
            {
                j["enabled"] = widget->isEnabled();
                j["visible"] = widget->isVisible();
                const QRect g = widget->geometry();
                j["geometry"] = {{"x", g.x()}, {"y", g.y()}, {"w", g.width()}, {"h", g.height()}};

                if (auto* lineEdit = qobject_cast<QLineEdit*>(widget))
                {
                    j["text"] = lineEdit->text().toStdString();
                }
                else if (auto* label = qobject_cast<QLabel*>(widget))
                {
                    j["text"] = label->text().toStdString();
                }
                else if (auto* button = qobject_cast<QAbstractButton*>(widget))
                {
                    j["text"] = button->text().toStdString();
                    j["checkable"] = button->isCheckable();
                    j["checked"] = button->isChecked();
                }
                else if (auto* box = qobject_cast<QGroupBox*>(widget))
                {
                    j["text"] = box->title().toStdString();
                }
                else if (auto* itemView = qobject_cast<QAbstractItemView*>(widget))
                {
                    // Covers QListWidget, QListView, QTreeView, QTableView, QUndoView, etc.
                    // - anything showing a QAbstractItemModel. Column 0 display text per row
                    // is enough to both read list-style contents (e.g. an undo stack's command
                    // history) and to know what set_value's row-index selection refers to.
                    nlohmann::json items = nlohmann::json::array();
                    if (QAbstractItemModel* model = itemView->model())
                    {
                        const int rowCount = model->rowCount();
                        for (int row = 0; row < rowCount; ++row)
                        {
                            items.push_back(model->index(row, 0).data(Qt::DisplayRole).toString().toStdString());
                        }
                    }
                    j["items"] = items;
                }
            }

            nlohmann::json children = nlohmann::json::array();
            for (QObject* child : obj->children())
            {
                if (qobject_cast<QWidget*>(child) || qobject_cast<QAction*>(child))
                {
                    children.push_back(DescribeObject(child));
                }
            }
            j["children"] = children;

            return j;
        }

        nlohmann::json HandleGetState(QWidget* root, const nlohmann::json& request)
        {
            return DescribeObject(ResolveTargetOptional(root, request));
        }

        nlohmann::json HandleListActions(QWidget* root, const nlohmann::json&)
        {
            nlohmann::json result = nlohmann::json::array();
            for (QAction* action : root->findChildren<QAction*>())
            {
                result.push_back({
                    {"objectName", action->objectName().toStdString()},
                    {"text", action->text().toStdString()},
                    {"enabled", action->isEnabled()},
                    {"checkable", action->isCheckable()},
                    {"checked", action->isChecked()},
                    {"shortcut", action->shortcut().toString().toStdString()},
                });
            }
            return result;
        }

        nlohmann::json HandleScreenshot(QWidget* root, const nlohmann::json& request)
        {
            QWidget* widget = RequireWidget(ResolveTargetOptional(root, request));

            const QPixmap pixmap = widget->grab();
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            if (!pixmap.save(&buffer, "PNG"))
            {
                throw CommandError("failed to encode screenshot as PNG");
            }

            nlohmann::json result = nlohmann::json::object();
            result["png_base64"] = bytes.toBase64().toStdString();
            return result;
        }

        nlohmann::json HandleClose(QWidget* root, const nlohmann::json& request)
        {
            QWidget* widget = RequireWidget(ResolveTargetOptional(root, request));
            nlohmann::json result = nlohmann::json::object();
            result["accepted"] = widget->close();
            return result;
        }
    }

    nlohmann::json ExecuteCommand(QWidget* root, const std::string& cmd, const nlohmann::json& request)
    {
        if (cmd == "ping")
        {
            return nlohmann::json::object();
        }
        if (cmd == "click")
        {
            return HandleClick(root, request);
        }
        if (cmd == "drag")
        {
            return HandleDrag(root, request);
        }
        if (cmd == "mouse_event")
        {
            return HandleMouseEvent(root, request);
        }
        if (cmd == "context_menu")
        {
            return HandleContextMenu(root, request);
        }
        if (cmd == "click_tree_item")
        {
            return HandleClickTreeItem(root, request);
        }
        if (cmd == "get_tree_items")
        {
            return HandleGetTreeItems(root, request);
        }
        if (cmd == "spin_step")
        {
            return HandleSpinStep(root, request);
        }
        if (cmd == "set_value")
        {
            return HandleSetValue(root, request);
        }
        if (cmd == "send_key")
        {
            return HandleSendKey(root, request);
        }
        if (cmd == "get_state")
        {
            return HandleGetState(root, request);
        }
        if (cmd == "list_actions")
        {
            return HandleListActions(root, request);
        }
        if (cmd == "screenshot")
        {
            return HandleScreenshot(root, request);
        }
        if (cmd == "close")
        {
            return HandleClose(root, request);
        }

        throw CommandError("unknown command: " + cmd);
    }
}
