#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

class QWidget;

namespace Automation
{
    class CommandError final : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    // Executes cmd against the widget tree rooted at root (normally the main window).
    // request is the full decoded request object ({"id", "cmd", ...args}); handlers pull
    // whatever args they need out of it directly. Throws CommandError on any failure
    // (unknown command, missing/ambiguous target, bad args); callers turn that into an
    // {"ok": false, "error": ...} response.
    nlohmann::json ExecuteCommand(QWidget* root, const std::string& cmd, const nlohmann::json& request);
}
