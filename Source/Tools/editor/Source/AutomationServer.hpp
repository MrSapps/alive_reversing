#pragma once

#include "AutomationProtocol.hpp"
#include <QHash>
#include <QObject>
#include <functional>
#include <optional>

class QLocalServer;
class QLocalSocket;
class QWidget;

// Listens on a QLocalSocket (Unix domain socket / Windows named pipe) for length-prefixed
// JSON commands and dispatches them, via AutomationCommands, against the widget tree
// rooted at whatever QWidget is passed to the constructor (normally the main window).
//
// Runs entirely on the thread it's constructed on (the GUI thread, in practice) - QLocalServer
// and QLocalSocket deliver everything through the event loop, so command handlers execute
// directly, with no cross-thread dispatch needed.
//
// This is an unauthenticated local command channel that can read and manipulate the running
// app; it must only be started when explicitly requested (see EditorMainWindow's
// --automation-socket handling), never unconditionally.
class AutomationServer final : public QObject
{
    Q_OBJECT

public:
    explicit AutomationServer(QWidget* root, QObject* parent = nullptr);
    ~AutomationServer() override;

    // Starts listening on the given QLocalServer name. Returns false on failure (check
    // errorString() on failure for details).
    bool Listen(const QString& name);
    QString ErrorString() const;

    using DomainCommandHandler = std::function<std::optional<nlohmann::json>(const std::string& cmd, const nlohmann::json& request)>;

    // Optional extension point tried before the generic Automation::ExecuteCommand, so
    // editor-domain-specific commands (e.g. inspecting the graphics scene) don't require
    // AutomationCommands/AutomationServer themselves to know about editor types - see
    // AutomationSceneCommands.hpp, wired in by EditorMainWindow. Returning std::nullopt falls
    // through to the generic dispatch.
    void SetDomainCommandHandler(DomainCommandHandler handler);

private slots:
    void OnNewConnection();
    void OnReadyRead();
    void OnDisconnected();

private:
    void ScheduleProcessNextFrame(QLocalSocket* socket);
    void ProcessNextFrame(QLocalSocket* socket);
    void HandleFrame(QLocalSocket* socket, const nlohmann::json& request);

    QWidget* mRoot = nullptr;
    QLocalServer* mServer = nullptr;
    QHash<QLocalSocket*, Automation::FrameReader> mReaders;
    DomainCommandHandler mDomainCommandHandler;
};
