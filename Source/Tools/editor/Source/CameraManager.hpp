#ifndef CAMERAMANAGER_HPP
#define CAMERAMANAGER_HPP

#include <QDialog>

namespace Ui {
class CameraManager;
}

class QPoint;
class EditorTab;
class CameraGraphicsItem;
struct EditorCamera;

enum TabImageIdx
{
    Main = 0,
    Foreground = 1,
    Background = 2,
    ForegroundWell = 3,
    BackgroundWell = 4,
};

class CameraManager : public QDialog
{
    Q_OBJECT

public:
    CameraManager(QWidget *parent, EditorTab* pParentTab, const QPoint* openedPos);
    ~CameraManager();

    void OnCameraSwapped(EditorCamera* pOld, EditorCamera* pNew);
    void OnCameraIdChanged(EditorCamera* pCam);
    void CreateCamera(bool dropEvent, QPixmap img);

    // Test/automation-only bypass for this dialog's QFileDialog-driven image picking (see the
    // "set_camera_image" automation command) - a native file picker can't be driven by
    // automation, so this constructs its own (never shown/exec()'d) CameraManager targeting
    // (camX, camY) and delegates to the same CreateCamera() a real "Select image" click would
    // call, rather than re-implementing its image-size normalization/on-disk save/command-
    // dispatch logic here too. Mirrors how EditorGraphicsView::dropEvent already uses a
    // never-shown CameraManager the same way for drag-and-drop image drops.
    static bool SetCameraImageForAutomation(EditorTab* pTab, int camX, int camY, TabImageIdx index, QPixmap img);

private slots:
    void on_btnSelectImage_clicked();

    void on_btnDeleteImage_clicked();

    void on_btnSetCameraId_clicked();

    void on_btnDeleteCamera_clicked();

    void on_lstCameras_itemSelectionChanged();

private:
    void SetTabImage(int idx, QPixmap img);

    void UpdateTabImages(CameraGraphicsItem* pItem);
    
    int NextFreeCamId();
    static bool SaveCameraImage(const QPixmap& camImage, const QString& pathDirectory, const std::string& camName, TabImageIdx imgIdx);

    CameraGraphicsItem* CameraGraphicsItemByPos(const QPoint& pos);
    CameraGraphicsItem* CameraGraphicsItemByModelPtr(const EditorCamera* cam);

    Ui::CameraManager *ui;
    EditorTab* mTab = nullptr;
};

#endif // CAMERAMANAGER_HPP
