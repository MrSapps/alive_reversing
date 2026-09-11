#include "AutomationCommands.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMouseEvent>
#include <QPixmap>
#include <QSpinBox>
#include <QWidget>

namespace Automation
{
    namespace
    {
        QObject* ResolveTargetByName(QWidget* root, const std::string& name)
        {
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

            const QPoint center = widget->rect().center();
            QMouseEvent press(QEvent::MouseButtonPress, center, widget->mapToGlobal(center), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, center, widget->mapToGlobal(center), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(widget, &press);
            QCoreApplication::sendEvent(widget, &release);
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
