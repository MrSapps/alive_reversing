#include "CommandLineOptions.hpp"
#include "CommandLineParser.hpp"
#include "DisplaySettings.hpp"
#include "logger.hpp"

static std::optional<GameType> ParseGame(const CommandLineParser& clp)
{
    const bool ae = clp.HasSwitch("AE") || clp.HasSwitch("-AE");
    const bool ao = clp.HasSwitch("AO") || clp.HasSwitch("-AO");
    if (ae && ao)
    {
        LOG_WARNING("Both AE and AO given on the command line, ignoring them");
        return std::nullopt;
    }
    if (ae)
    {
        return GameType::eAe;
    }
    if (ao)
    {
        return GameType::eAo;
    }
    return std::nullopt;
}

static std::optional<bool> ParseDisplayBool(const CommandLineParser& clp, const std::string& name)
{
    const auto value = clp.GetBool(name);
    if (!value && clp.Has(name))
    {
        LOG_WARNING("Ignoring %s, it should be %s, %s=true or %s=false", name.c_str(), name.c_str(), name.c_str(), name.c_str());
    }
    return value;
}

CommandLineOptions CommandLineOptions::Parse(const CommandLineParser& clp)
{
    CommandLineOptions options;

    options.mShowHelp = clp.HasSwitch("-help") || clp.HasSwitch("--help") || clp.HasSwitch("-h") || clp.HasSwitch("/?");

    options.mGame = ParseGame(clp);
    options.mModName = clp.GetValue("-mod").value_or("");

    options.mDdCheat = clp.HasSwitch("-ddcheat") || clp.HasSwitch("-it_is_me_your_father");
    options.mShowFps = clp.HasSwitch("-ddfps") || clp.HasSwitch("-show_fps") || clp.HasSwitch("-showfps");
    options.mNoFrameSkip = clp.HasSwitch("-ddnoskip");
    if (const auto slowLoad = clp.GetValue("-ddslowload"))
    {
        try
        {
            options.mSlowLoadMs = static_cast<u32>(std::stoul(*slowLoad));
        }
        catch (const std::exception&)
        {
            LOG_WARNING("Ignoring -ddslowload=%s, it should be a number of milliseconds", slowLoad->c_str());
        }
    }

    const char* maxFpsName = clp.Has("-max_fps") ? "-max_fps" : "-maxfps";
    if (const auto maxFps = clp.GetValue(maxFpsName))
    {
        if (!maxFps->empty() && maxFps->size() <= 6 && maxFps->find_first_not_of("0123456789") == std::string::npos)
        {
            options.mMaxFps = static_cast<u32>(std::stoul(*maxFps));
        }
        else
        {
            LOG_WARNING("Ignoring %s=%s, it should be a number of frames a second (0 for no limit)", maxFpsName, maxFps->c_str());
        }
    }

    options.mRendererChecks = clp.HasSwitch("-renderer_checks");

    options.mRecordFile = clp.GetValue("-record");
    options.mFlushRecording = clp.HasSwitch("-flush");
    options.mPlayFile = clp.GetValue("-play");
    options.mIgnoreDesyncs = clp.HasSwitch("-ignore_desyncs");

    if (const auto name = clp.GetValue("-renderer"))
    {
        options.mRenderer = DisplaySettings::RendererFromString(*name);
        if (!options.mRenderer)
        {
            LOG_WARNING("Ignoring unknown -renderer=%s", name->c_str());
        }
    }
    options.mFullscreen = ParseDisplayBool(clp, "-fullscreen");
    options.mKeepAspectRatio = ParseDisplayBool(clp, "-keep_aspect_ratio");
    options.mFilterScreen = ParseDisplayBool(clp, "-filter_screen");
    options.mUseOriginalResolution = ParseDisplayBool(clp, "-use_original_resolution");

    return options;
}

// Keep in sync with README.md's "Command Line Options"
const char* CommandLineOptions::Usage()
{
    return "Usage: relive [options]\n"
           "Run from the game data directory. Values use -name=value.\n"
           "\n"
           "  AE, -AE                     Run Abe's Exoddus (the default)\n"
           "  AO, -AO                     Run Abe's Oddysee\n"
           "  -mod=<name>                 Run a mod from relive_data/mods, by directory or name\n"
           "  -renderer=<name>            sdl3 (default) or opengl\n"
           "  -fullscreen                 Start fullscreen\n"
           "  -keep_aspect_ratio          Letterbox to 4:3 (=false stretches to the window)\n"
           "  -filter_screen              Filter (smooth) the scaled image\n"
           "  -use_original_resolution    Render at 640x240 and scale up\n"
           "  -ddcheat                    Enable the debug cheat menu\n"
           "  -ddfps, -show_fps, -showfps Show the frame rate and frame time\n"
           "  -max_fps=<n>, -maxfps=<n>   Run at up to n frames a second (default 30), 0 for no limit.\n"
           "                              The game runs faster the more frames it shows\n"
           "  -ddnoskip                   Render every frame instead of skipping to keep up\n"
           "  -ddslowload=<ms>            Make each resource take at least this long to load\n"
           "  -renderer_checks            Check for renderer errors as it goes (slower)\n"
           "  -help, --help, -h, /?       Show this and exit\n"
           "\n"
           "The display options also take =true or =false, and are saved to relive.ini.\n"
           "\n"
           "Recording and playback:\n"
           "  -record=<file>              Record this session\n"
           "  -flush                      With -record, write to disk straight away\n"
           "  -play=<file>                Play back a recording, stopping if it desyncs\n"
           "  -ignore_desyncs             With -play, keep going after a desync\n";
}
