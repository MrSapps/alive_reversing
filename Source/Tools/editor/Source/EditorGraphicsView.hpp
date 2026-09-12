#pragma once

#include <QGraphicsView>

class EditorTab;

class EditorGraphicsView final : public QGraphicsView
{
    Q_OBJECT

public:
    EditorGraphicsView(EditorTab* editorTab);

    void mousePressEvent(QMouseEvent* pEvent) override;
    void mouseReleaseEvent(QMouseEvent* pEvent) override;
    void wheelEvent(QWheelEvent* pEvent) override;

    // TODO: implement proper ScrollHandDrag mode.
    // you should be able to move around by pressing and holding the middle mouse button.
    void keyPressEvent(QKeyEvent* pEvent) override;

    void keyReleaseEvent(QKeyEvent* pEvent) override;
    void focusOutEvent(QFocusEvent* pEvent) override;
    void leaveEvent(QEvent* pEvent) override;
    void contextMenuEvent(QContextMenuEvent* pEvent) override;
    void dragEnterEvent(QDragEnterEvent* pEvent) override;
    void dragMoveEvent(QDragMoveEvent* pEvent) override;
    void dropEvent(QDropEvent* pEvent) override;

private:
    EditorTab* mEditorTab = nullptr;
};
