#include "Profile.hpp"

namespace Bot {

bool Profile::Parse(const std::string& text, Profile& profile, std::string& error)
{
    profile = {};
    std::size_t at = 0;
    bool first = true;
    while (at <= text.size())
    {
        std::size_t end = text.find(',', at);
        if (end == std::string::npos)
        {
            end = text.size();
        }
        const std::string word = text.substr(at, end - at);
        at = end + 1;
        if (first)
        {
            first = false;
            if (word == "any")
            {
                profile.mEnding = Ending::eAny;
            }
            else if (word == "save_all")
            {
                profile.mEnding = Ending::eSaveAll;
            }
            else if (word == "kill_all")
            {
                profile.mEnding = Ending::eKillAll;
            }
            else if (word == "average")
            {
                profile.mEnding = Ending::eAverage;
            }
            else
            {
                error = "unknown profile '" + word + "' (any, save_all, kill_all, average)";
                return false;
            }
        }
        else if (word == "human")
        {
            profile.mHuman = true;
        }
        else if (word.rfind("seed=", 0) == 0 && word.size() > 5 && word.find_first_not_of("0123456789", 5) == std::string::npos && word.size() < 24)
        {
            profile.mSeed = std::stoull(word.substr(5));
        }
        else
        {
            error = "unknown profile option '" + word + "' (seed=<n>, human)";
            return false;
        }
    }
    return true;
}

std::string Profile::Text() const
{
    const char* ending = mEnding == Ending::eSaveAll ? "save_all" : mEnding == Ending::eKillAll ? "kill_all" : mEnding == Ending::eAverage ? "average" : "any";
    return std::string(ending) + ",seed=" + std::to_string(mSeed) + (mHuman ? ",human" : "");
}

bool Profile::Rescues(u32 index) const
{
    switch (mEnding)
    {
        case Ending::eSaveAll:
            return true;
        case Ending::eAverage:
            return index % 2 == 0;
        default:
            return false;
    }
}

bool Profile::Kills(u32) const
{
    return mEnding == Ending::eKillAll;
}

} // namespace Bot
