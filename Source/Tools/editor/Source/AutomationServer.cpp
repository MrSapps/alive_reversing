#include "AutomationServer.hpp"
#include "AutomationCommands.hpp"

#include <QLocalServer>
#include <QLocalSocket>
#include <QWidget>
#include <QDebug>
#include <QTimer>

AutomationServer::AutomationServer(QWidget* root, QObject* parent)
    : QObject(parent)
    , mRoot(root)
{
    mServer = new QLocalServer(this);
    connect(mServer, &QLocalServer::newConnection, this, &AutomationServer::OnNewConnection);
}

AutomationServer::~AutomationServer() = default;

bool AutomationServer::Listen(const QString& name)
{
    // Clear a stale socket file/pipe name from a previous run that didn't shut down cleanly.
    QLocalServer::removeServer(name);
    return mServer->listen(name);
}

QString AutomationServer::ErrorString() const
{
    return mServer->errorString();
}

void AutomationServer::OnNewConnection()
{
    while (QLocalSocket* socket = mServer->nextPendingConnection())
    {
        mReaders.insert(socket, Automation::FrameReader());
        connect(socket, &QLocalSocket::readyRead, this, &AutomationServer::OnReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &AutomationServer::OnDisconnected);
    }
}

void AutomationServer::OnReadyRead()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket)
    {
        return;
    }

    const auto it = mReaders.find(socket);
    if (it == mReaders.end())
    {
        return;
    }

    it->Append(socket->readAll());
    ScheduleProcessNextFrame(socket);
}

void AutomationServer::ScheduleProcessNextFrame(QLocalSocket* socket)
{
    // Deferred rather than processed inline: a command's handler can itself nest the Qt
    // event loop (e.g. click opening a modal dialog via exec()), and this queued call is
    // exactly what lets a second command - already fully received and buffered - still get
    // dispatched and answered while the first one's handler hasn't returned yet.
    QTimer::singleShot(0, this, [this, socket]() { ProcessNextFrame(socket); });
}

void AutomationServer::ProcessNextFrame(QLocalSocket* socket)
{
    const auto it = mReaders.find(socket);
    if (it == mReaders.end())
    {
        // Disconnected (and possibly already deleted) since this was scheduled.
        return;
    }

    auto frame = it->TryTakeFrame();
    if (!frame)
    {
        return;
    }

    // Queue the check for any further already-buffered frame *before* dispatching this
    // one, so it still runs even if this frame's handler nests its own event loop.
    ScheduleProcessNextFrame(socket);

    if (frame->is_discarded())
    {
        nlohmann::json response;
        response["id"] = nullptr;
        response["ok"] = false;
        response["error"] = "malformed frame";
        Automation::WriteFrame(socket, response);
        return;
    }

    HandleFrame(socket, *frame);
}

void AutomationServer::OnDisconnected()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket)
    {
        return;
    }
    mReaders.remove(socket);
    socket->deleteLater();
}

void AutomationServer::HandleFrame(QLocalSocket* socket, const nlohmann::json& request)
{
    nlohmann::json response;
    response["id"] = request.value("id", nlohmann::json());

    try
    {
        if (!request.contains("cmd") || !request.at("cmd").is_string())
        {
            throw Automation::CommandError("missing 'cmd'");
        }

        response["result"] = Automation::ExecuteCommand(mRoot, request.at("cmd").get<std::string>(), request);
        response["ok"] = true;
    }
    catch (const std::exception& e)
    {
        response["ok"] = false;
        response["error"] = e.what();
    }

    Automation::WriteFrame(socket, response);
}
