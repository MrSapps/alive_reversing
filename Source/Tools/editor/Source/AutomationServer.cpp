#include "AutomationServer.hpp"
#include "AutomationCommands.hpp"

#include <QLocalServer>
#include <QLocalSocket>
#include <QWidget>
#include <QDebug>
#include <QLoggingCategory>
#include <QTimer>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

// Traces each request through the server, so a test that times out shows how far it got.
// Silence with QT_LOGGING_RULES="relive.automation=false".
Q_LOGGING_CATEGORY(lcAutomation, "relive.automation")

AutomationServer::AutomationServer(QWidget* root, QObject* parent)
    : QObject(parent)
    , mRoot(root)
{
    mServer = new QLocalServer(this);
    connect(mServer, &QLocalServer::newConnection, this, &AutomationServer::OnNewConnection);
}

namespace {
    // How long exit waits, per connected client, for it to read the responses still in flight.
    constexpr int kFlushTimeoutMs = 2000;

#ifdef _WIN32
    DWORD WINAPI FlushPipeThread(LPVOID param)
    {
        const HANDLE pipe = static_cast<HANDLE>(param);
        FlushFileBuffers(pipe);
        CloseHandle(pipe);
        return 0;
    }
#endif

    // Makes sure the client can still read a response written just before the editor exits
    // (e.g. the one to the click on Exit itself).
    void FlushToClient(QLocalSocket* socket)
    {
        // Get whatever Qt still holds into the socket/pipe.
        while (socket->bytesToWrite() > 0 && socket->waitForBytesWritten(kFlushTimeoutMs))
        {
        }

#ifdef _WIN32
        // Once written, that's enough on Unix: the kernel keeps the data after we close. But Qt
        // destroys a Windows QLocalSocket with DisconnectNamedPipe, which throws away anything
        // the client hasn't read yet. FlushFileBuffers blocks until it has. It runs on its own
        // thread, on its own handle, so a client that never reads can only delay exit, not hang it.
        HANDLE pipe = nullptr;
        if (!DuplicateHandle(GetCurrentProcess(), reinterpret_cast<HANDLE>(socket->socketDescriptor()), GetCurrentProcess(), &pipe, 0, FALSE, DUPLICATE_SAME_ACCESS))
        {
            return;
        }
        const HANDLE thread = CreateThread(nullptr, 0, FlushPipeThread, pipe, 0, nullptr);
        if (!thread)
        {
            CloseHandle(pipe);
            return;
        }
        WaitForSingleObject(thread, kFlushTimeoutMs);
        CloseHandle(thread);
#endif
    }
}

AutomationServer::~AutomationServer()
{
    // The sockets are (grand)children of this, so they're all still alive here.
    for (auto it = mReaders.keyBegin(); it != mReaders.keyEnd(); ++it)
    {
        FlushToClient(*it);
    }
}

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

void AutomationServer::SetDomainCommandHandler(DomainCommandHandler handler)
{
    mDomainCommandHandler = std::move(handler);
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

    const QByteArray data = socket->readAll();
    qCInfo(lcAutomation) << "read" << data.size() << "bytes";
    it->Append(data);
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
    qCInfo(lcAutomation) << "handling id" << response["id"].dump().c_str() << request.value("cmd", std::string()).c_str()
                         << request.value("target", std::string()).c_str();

    try
    {
        if (!request.contains("cmd") || !request.at("cmd").is_string())
        {
            throw Automation::CommandError("missing 'cmd'");
        }
        const std::string cmd = request.at("cmd").get<std::string>();

        std::optional<nlohmann::json> domainResult;
        if (mDomainCommandHandler)
        {
            domainResult = mDomainCommandHandler(cmd, request);
        }

        response["result"] = domainResult ? *domainResult : Automation::ExecuteCommand(mRoot, cmd, request);
        response["ok"] = true;
    }
    catch (const std::exception& e)
    {
        response["ok"] = false;
        response["error"] = e.what();
    }

    qCInfo(lcAutomation) << "responding to id" << response["id"].dump().c_str() << "ok" << response.value("ok", false);
    Automation::WriteFrame(socket, response);
}
