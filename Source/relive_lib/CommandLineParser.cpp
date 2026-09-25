#include "CommandLineParser.hpp"
#include <utility>

CommandLineParser::CommandLineParser(s32 argc, const char* const* argv)
{
    for (s32 i = 1; i < argc; i++)
    {
        mArgs.emplace_back(argv[i]);
    }
}

CommandLineParser::CommandLineParser(std::vector<std::string> args)
    : mArgs(std::move(args))
{
}

bool CommandLineParser::HasSwitch(const std::string& name) const
{
    for (const std::string& arg : mArgs)
    {
        if (arg == name)
        {
            return true;
        }
    }
    return false;
}

std::optional<std::string> CommandLineParser::GetValue(const std::string& name) const
{
    const std::string prefix = name + "=";
    std::optional<std::string> value;
    for (const std::string& arg : mArgs)
    {
        if (arg.compare(0, prefix.size(), prefix) == 0)
        {
            value = arg.substr(prefix.size());
        }
    }
    return value;
}

std::optional<bool> CommandLineParser::GetBool(const std::string& name) const
{
    const std::string prefix = name + "=";
    std::optional<bool> result;
    for (const std::string& arg : mArgs)
    {
        if (arg == name || arg == prefix + "true")
        {
            result = true;
        }
        else if (arg == prefix + "false")
        {
            result = false;
        }
        else if (arg.compare(0, prefix.size(), prefix) == 0)
        {
            result = std::nullopt;
        }
    }
    return result;
}

bool CommandLineParser::Has(const std::string& name) const
{
    return HasSwitch(name) || GetValue(name).has_value();
}
