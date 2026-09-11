#include "AutomationSceneCommands.hpp"
#include "AutomationCommands.hpp" // for Automation::CommandError

#include "EditorMainWindow.hpp"
#include "EditorTab.hpp"
#include "EditorGraphicsScene.hpp"
#include "ResizeableArrowItem.hpp"
#include "ResizeableRectItem.hpp"
#include "CameraGraphicsItem.hpp"
#include "CollisionObject.hpp"
#include "EditorCamera.hpp"
#include "../../relive_api/TlvsRelive.hpp"

#include <QGraphicsItem>
#include <QGraphicsView>

namespace Automation
{
    namespace
    {
        EditorTab* RequireCurrentTab(EditorMainWindow* mainWindow)
        {
            EditorTab* tab = mainWindow->CurrentTab();
            if (!tab)
            {
                throw CommandError("no tab is open");
            }
            return tab;
        }

        // dynamic_cast rather than qgraphicsitem_cast: CameraGraphicsItem doesn't define its
        // own QGraphicsItem::Type (it just inherits QGraphicsRectItem's), so qgraphicsitem_cast
        // can't distinguish it from a plain QGraphicsRectItem. Using the same (RTTI-based)
        // mechanism for all three item types here keeps this consistent rather than mixing
        // qgraphicsitem_cast and dynamic_cast case by case.
        nlohmann::json DescribeSceneItem(QGraphicsItem* item)
        {
            nlohmann::json j = nlohmann::json::object();
            j["selected"] = item->isSelected();

            if (auto* line = dynamic_cast<ResizeableArrowItem*>(item))
            {
                CollisionObject* obj = line->GetCollisionItem();
                j["kind"] = "collision_line";
                j["id"] = obj->mId;
                j["x1"] = obj->X1();
                j["y1"] = obj->Y1();
                j["x2"] = obj->X2();
                j["y2"] = obj->Y2();
                j["lineType"] = static_cast<int>(obj->mLine.mLineType);
            }
            else if (auto* rect = dynamic_cast<ResizeableRectItem*>(item))
            {
                // ResizeableRectItem reads/writes mBaseTlv->mBottomRightX/Y directly as a raw
                // width/height (see SyncFromMapObject/SyncToMapObject) - MapObjectBase::Width()/
                // Height() instead compute mBottomRightX - mTopLeftX, treating the same field as
                // a coordinate. The two are inconsistent; CurrentRect() is the item's own
                // authoritative, always-synced state (same principle as ResizeableArrowItem's
                // X1()/Y1()/X2()/Y2() above), so use that rather than MapObjectBase's accessors.
                const QRectF r = rect->CurrentRect();
                MapObjectBase* obj = rect->GetMapObject();
                j["kind"] = "map_object";
                j["xpos"] = r.x();
                j["ypos"] = r.y();
                j["width"] = r.width();
                j["height"] = r.height();
                j["tlvType"] = static_cast<int>(obj->mBaseTlv->mTlvType);
            }
            else if (auto* camera = dynamic_cast<CameraGraphicsItem*>(item))
            {
                j["kind"] = "camera";
                j["gridX"] = camera->GetCamera()->mX;
                j["gridY"] = camera->GetCamera()->mY;
            }
            else
            {
                // Not one of the editor's own item types (or a decoration/child item, e.g. an
                // arrowhead sub-path) - report just enough to know something is there.
                j["kind"] = "unknown";
                j["qGraphicsItemType"] = item->type();
                const QRectF b = item->sceneBoundingRect();
                j["sceneBoundingRect"] = {{"x", b.x()}, {"y", b.y()}, {"w", b.width()}, {"h", b.height()}};
            }

            return j;
        }

        nlohmann::json HandleGetSceneItems(EditorMainWindow* mainWindow, const nlohmann::json&)
        {
            EditorTab* tab = RequireCurrentTab(mainWindow);

            nlohmann::json items = nlohmann::json::array();
            for (QGraphicsItem* item : tab->GetScene().items())
            {
                items.push_back(DescribeSceneItem(item));
            }

            nlohmann::json result = nlohmann::json::object();
            result["items"] = items;
            return result;
        }

        nlohmann::json HandleSceneToView(EditorMainWindow* mainWindow, const nlohmann::json& request)
        {
            EditorTab* tab = RequireCurrentTab(mainWindow);
            if (!request.contains("x") || !request.contains("y"))
            {
                throw CommandError("'x' and 'y' are required");
            }

            QGraphicsView* view = tab->GetScene().views().at(0);
            const QPoint viewPos = view->mapFromScene(QPointF(request.at("x").get<qreal>(), request.at("y").get<qreal>()));

            nlohmann::json result = nlohmann::json::object();
            result["x"] = viewPos.x();
            result["y"] = viewPos.y();
            return result;
        }

        std::string RequirePath(const nlohmann::json& request)
        {
            if (!request.contains("path") || !request.at("path").is_string())
            {
                throw CommandError("'path' is required");
            }
            return request.at("path").get<std::string>();
        }

        // Bypasses the real QFileDialog-based Save As flow (EditorTab::SaveAs()) so tests can
        // save to a known (e.g. temp) location without driving a file dialog.
        nlohmann::json HandleSavePathAs(EditorMainWindow* mainWindow, const nlohmann::json& request)
        {
            EditorTab* tab = RequireCurrentTab(mainWindow);
            const std::string path = RequirePath(request);

            nlohmann::json result = nlohmann::json::object();
            result["saved"] = tab->SaveAsPath(QString::fromStdString(path));
            return result;
        }

        // Bypasses the real QFileDialog-based Open flow (EditorMainWindow::on_action_open_path_triggered())
        // so tests can open a known path directly.
        nlohmann::json HandleOpenPath(EditorMainWindow* mainWindow, const nlohmann::json& request)
        {
            const std::string path = RequirePath(request);

            nlohmann::json result = nlohmann::json::object();
            result["opened"] = mainWindow->OpenPath(QString::fromStdString(path));
            return result;
        }
    }

    std::optional<nlohmann::json> TryExecuteSceneCommand(EditorMainWindow* mainWindow, const std::string& cmd, const nlohmann::json& request)
    {
        if (cmd == "get_scene_items")
        {
            return HandleGetSceneItems(mainWindow, request);
        }
        if (cmd == "scene_to_view")
        {
            return HandleSceneToView(mainWindow, request);
        }
        if (cmd == "save_path_as")
        {
            return HandleSavePathAs(mainWindow, request);
        }
        if (cmd == "open_path")
        {
            return HandleOpenPath(mainWindow, request);
        }
        return std::nullopt;
    }
}
