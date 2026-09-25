#pragma once

#include "Types.hpp"
#include <optional>
#include <string>
#include <vector>

// The command line as separate arguments, without argv[0] (the executable). relive's own
// options are parsed from this by CommandLineOptions. Arguments are
// matched whole, so "-ddfps" doesn't match "-ddfps2" and a value containing "AO" isn't the AO
// switch. Named values are "-name=value"; the value may contain spaces if the shell passed it
// as one argument. If an argument is given more than once, the last one wins.
class CommandLineParser final
{
public:
    CommandLineParser(s32 argc, const char* const* argv);
    explicit CommandLineParser(std::vector<std::string> args);

    bool HasSwitch(const std::string& name) const;

    // The value of "name=value", e.g. GetValue("-mod") for "-mod=my_mod". nullopt if not given.
    std::optional<std::string> GetValue(const std::string& name) const;

    // true for "name" or "name=true", false for "name=false". nullopt if the argument isn't
    // given, or has any other value.
    std::optional<bool> GetBool(const std::string& name) const;

    // Whether "name" or "name=..." is given at all.
    bool Has(const std::string& name) const;

    const std::vector<std::string>& Args() const
    {
        return mArgs;
    }

private:
    std::vector<std::string> mArgs;
};
