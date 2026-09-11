#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

class EditorMainWindow;

namespace Automation
{
    // Editor-domain commands operating on the graphics scene (collision lines, map objects,
    // cameras) - deliberately kept separate from the generic, editor-agnostic
    // AutomationCommands.cpp/AutomationServer.cpp. Returns std::nullopt for any cmd it doesn't
    // recognise, so it can be wired in as an AutomationServer::DomainCommandHandler and fall
    // through to the generic dispatch for everything else. May throw Automation::CommandError
    // (see AutomationCommands.hpp) the same way the generic handlers do.
    std::optional<nlohmann::json> TryExecuteSceneCommand(EditorMainWindow* mainWindow, const std::string& cmd, const nlohmann::json& request);
}
