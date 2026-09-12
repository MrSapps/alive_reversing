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
#include "CameraManager.hpp"
#include "../../relive_api/TlvsRelive.hpp"

#include <QGraphicsItem>
#include <QGraphicsView>
#include <QGraphicsLineItem>
#include <QPixmap>
#include <map>

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
                j["next"] = obj->Next();
                j["previous"] = obj->Previous();
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
                j["camName"] = camera->GetCamera()->mName;
                j["hasMainImage"] = !camera->GetCamera()->mCameraImageandLayers.mCameraImage.isNull();
                j["hasForegroundLayer"] = !camera->GetCamera()->mCameraImageandLayers.mForegroundLayer.isNull();
                j["hasBackgroundLayer"] = !camera->GetCamera()->mCameraImageandLayers.mBackgroundLayer.isNull();
            }
            else if (auto* plainLine = dynamic_cast<QGraphicsLineItem*>(item))
            {
                // The click-to-place "add collision" tool's ghost preview line (a plain
                // QGraphicsLineItem, not a ResizeableArrowItem - it's non-interactive and never
                // becomes a real CollisionObject unless/until the second click commits it).
                // Checked after ResizeableArrowItem above (which is itself a QGraphicsLineItem)
                // so a real collision line is never misreported as a ghost.
                j["kind"] = "collision_ghost_line";
                j["x1"] = plainLine->line().x1();
                j["y1"] = plainLine->line().y1();
                j["x2"] = plainLine->line().x2();
                j["y2"] = plainLine->line().y2();
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

        // Bypasses CameraManager's QFileDialog-based image picking (see
        // CameraManager::SetCameraImageForAutomation) so tests can create/update a camera's
        // image (main, or an FG1 layer) at a known grid cell without driving a native file
        // picker.
        nlohmann::json HandleSetCameraImage(EditorMainWindow* mainWindow, const nlohmann::json& request)
        {
            EditorTab* tab = RequireCurrentTab(mainWindow);
            if (!request.contains("x") || !request.contains("y") || !request.contains("image_path"))
            {
                throw CommandError("'x', 'y' and 'image_path' are required");
            }

            const int x = request.at("x").get<int>();
            const int y = request.at("y").get<int>();
            const QString imagePath = QString::fromStdString(request.at("image_path").get<std::string>());

            static const std::map<std::string, TabImageIdx> kLayers = {
                {"main", TabImageIdx::Main},
                {"foreground", TabImageIdx::Foreground},
                {"background", TabImageIdx::Background},
                {"foreground_well", TabImageIdx::ForegroundWell},
                {"background_well", TabImageIdx::BackgroundWell},
            };
            const std::string layer = request.value("layer", std::string("main"));
            const auto it = kLayers.find(layer);
            if (it == kLayers.end())
            {
                throw CommandError("unknown 'layer': " + layer);
            }

            QPixmap img(imagePath);

            nlohmann::json result = nlohmann::json::object();
            result["ok"] = CameraManager::SetCameraImageForAutomation(tab, x, y, it->second, img);
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
        if (cmd == "set_camera_image")
        {
            return HandleSetCameraImage(mainWindow, request);
        }
        return std::nullopt;
    }
}
