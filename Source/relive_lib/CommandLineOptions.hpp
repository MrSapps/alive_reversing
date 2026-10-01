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
    // -convert: convert the game data if it's out of date, without a window, then exit
    bool mConvertOnly = false;

    // AE/-AE or AO/-AO. nullopt if neither is given, or if both are (which is logged).
    std::optional<GameType> mGame;
    // -mod=<name>, empty if not given
    std::string mModName;

    bool mDdCheat = false;
    // -ddfps, -show_fps or -showfps
    bool mShowFps = false;
    bool mNoFrameSkip = false;
    // -ddslowload=<ms>: makes each resource loaded in the background take at least this long,
    // to test loading on slow storage. 0 if not given.
    u32 mSlowLoadMs = 0;
    // -max_fps=<n> or -maxfps=<n>: the most frames a second, 0 for no limit. nullopt if not given
    // (invalid values are logged and treated as not given): 30, like the original.
    std::optional<u32> mMaxFps;
    // -renderer_checks: the renderer checks for errors as it goes, which is slower. OpenGL checks
    // every call, SDL3 turns on its GPU, Vulkan and Direct3D 11 backends' validation.
    bool mRendererChecks = false;
    // -ps1_sound: play the sound effects and music on the emulated PS1 SPU (PsxSoundEngine)
    bool mPs1Sound = false;
    // -spu_filter=hq (default) or gaussian: with -ps1_sound, how voices above 44100 Hz are
    // resampled. gaussian is the PS1's own, which aliases (buzzes) on bright samples at high notes.
    bool mSpuFilterGaussian = false;
    // -dump_audio=<file.wav>: save everything the sound system renders to this WAV file
    std::optional<std::string> mDumpAudio;
    // -dump_live_audio=<file.wav>: save what the sound system renders for the audio device, as
    // it plays (not frame-locked: for checking the live path against -dump_audio)
    std::optional<std::string> mDumpLiveAudio;

    // Recording and playback
    std::optional<std::string> mRecordFile;
    bool mFlushRecording = false;
    std::optional<std::string> mPlayFile;
    bool mIgnoreDesyncs = false;

    // -automation=<file>: run the commands in this script, see Automation.hpp
    std::optional<std::string> mAutomationScript;

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
