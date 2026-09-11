// Thin, generic command-line client for relive-editor's automation channel (see
// AutomationServer.hpp / AutomationProtocol.hpp). Sends one JSON command over a QLocalSocket
// and prints the JSON response to stdout. Deliberately generic (no per-command subcommands)
// so new editor-side automation commands never require changes here.
//
// Usage:
//   relive-editor-automation-cli --socket <name> --send '{"cmd":"list_actions"}'
//   relive-editor-automation-cli --socket <name> --send -   (reads the JSON command from stdin)
//
// "id" is filled in automatically if the command doesn't already include one.

#include "AutomationProtocol.hpp"

#include <QCoreApplication>
#include <QLocalSocket>
#include <QTextStream>
#include <iostream>

namespace
{
    void PrintUsage()
    {
        std::cerr << "usage: relive-editor-automation-cli --socket <name> --send '<json command>' [--connect-timeout-ms N]\n";
        std::cerr << "       relive-editor-automation-cli --socket <name> --send -   (read command json from stdin)\n";
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();

    QString socketName;
    QString sendArg;
    int connectTimeoutMs = 5000;

    for (int i = 1; i < args.size(); ++i)
    {
        if (args[i] == "--socket" && i + 1 < args.size())
        {
            socketName = args[++i];
        }
        else if (args[i] == "--send" && i + 1 < args.size())
        {
            sendArg = args[++i];
        }
        else if (args[i] == "--connect-timeout-ms" && i + 1 < args.size())
        {
            connectTimeoutMs = args[++i].toInt();
        }
        else
        {
            PrintUsage();
            return 2;
        }
    }

    if (socketName.isEmpty() || sendArg.isEmpty())
    {
        PrintUsage();
        return 2;
    }

    std::string rawJson;
    if (sendArg == "-")
    {
        QTextStream in(stdin);
        rawJson = in.readAll().toStdString();
    }
    else
    {
        rawJson = sendArg.toStdString();
    }

    nlohmann::json command = nlohmann::json::parse(rawJson, nullptr, false);
    if (command.is_discarded() || !command.is_object())
    {
        std::cerr << "error: --send did not contain a valid JSON object\n";
        return 2;
    }
    if (!command.contains("id"))
    {
        command["id"] = 1;
    }

    QLocalSocket socket;
    socket.connectToServer(socketName);
    if (!socket.waitForConnected(connectTimeoutMs))
    {
        std::cerr << "error: failed to connect to '" << socketName.toStdString() << "': " << socket.errorString().toStdString() << "\n";
        return 1;
    }

    Automation::WriteFrame(&socket, command);

    Automation::FrameReader reader;
    while (true)
    {
        if (auto frame = reader.TryTakeFrame())
        {
            if (frame->is_discarded())
            {
                std::cerr << "error: server sent a malformed frame\n";
                return 1;
            }
            std::cout << frame->dump() << std::endl;
            return frame->value("ok", false) ? 0 : 1;
        }

        if (!socket.waitForReadyRead(connectTimeoutMs))
        {
            std::cerr << "error: timed out waiting for a response\n";
            return 1;
        }
        reader.Append(socket.readAll());
    }
}
