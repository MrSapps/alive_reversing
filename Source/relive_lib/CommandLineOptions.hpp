#pragma once

#include "Types.hpp"
#include "GameType.hpp"
#include "Renderer/IRenderer.hpp"
#include <optional>
#include <string>

class CommandLineParser;

// relive's command line options, parsed once at startup. See Usage() and README.md's
// "Command Line Options".
struct CommandLineOptions final
{
    bool mShowHelp = false;

    // AE/-AE or AO/-AO. nullopt if neither is given, or if both are (which is logged).
    std::optional<GameType> mGame;
    // -mod=<name>, empty if not given
    std::string mModName;

    bool mDdCheat = false;
    bool mShowFps = false;
    bool mNoFrameSkip = false;
    // -ddslowload=<ms>: makes each resource loaded in the background take at least this long,
    // to test loading on slow storage. 0 if not given.
    u32 mSlowLoadMs = 0;

    // Recording and playback
    std::optional<std::string> mRecordFile;
    bool mFlushRecording = false;
    std::optional<std::string> mPlayFile;
    bool mPlayFastest = false;
    bool mIgnoreDesyncs = false;

    // Display settings, named after their relive.ini keys. nullopt if not given (invalid
    // values are logged and treated as not given). See DisplaySettings::ApplyCommandLine.
    std::optional<IRenderer::Renderers> mRenderer;
    std::optional<bool> mFullscreen;
    std::optional<bool> mKeepAspectRatio;
    std::optional<bool> mFilterScreen;
    std::optional<bool> mUseOriginalResolution;

    static CommandLineOptions Parse(const CommandLineParser& clp);

    // The -help text
    static const char* Usage();
};
