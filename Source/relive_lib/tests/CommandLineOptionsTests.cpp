// Unit tests for CommandLineOptions (relive's options, parsed from the command line) and
// applying its display settings (DisplaySettings::ApplyCommandLine).

#include "CommandLineOptions.hpp"
#include "CommandLineParser.hpp"
#include "DisplaySettings.hpp"

#include <gtest/gtest.h>

static CommandLineOptions Parse(std::vector<std::string> args)
{
    return CommandLineOptions::Parse(CommandLineParser(std::move(args)));
}

TEST(CommandLineOptions, NothingGivenMeansDefaults)
{
    const CommandLineOptions options = Parse({});

    EXPECT_FALSE(options.mShowHelp);
    EXPECT_EQ(options.mGame, std::nullopt);
    EXPECT_EQ(options.mModName, "");
    EXPECT_FALSE(options.mDdCheat);
    EXPECT_FALSE(options.mShowFps);
    EXPECT_FALSE(options.mNoFrameSkip);
    EXPECT_EQ(options.mRecordFile, std::nullopt);
    EXPECT_EQ(options.mPlayFile, std::nullopt);
    EXPECT_EQ(options.mRenderer, std::nullopt);
    EXPECT_EQ(options.mFullscreen, std::nullopt);
}

TEST(CommandLineOptions, EveryHelpSpelling)
{
    for (const char* help : {"-help", "--help", "-h", "/?"})
    {
        EXPECT_TRUE(Parse({help}).mShowHelp) << help;
    }
}

TEST(CommandLineOptions, GameAcceptsEitherSpelling)
{
    EXPECT_EQ(Parse({"AE"}).mGame, GameType::eAe);
    EXPECT_EQ(Parse({"-AE"}).mGame, GameType::eAe);
    EXPECT_EQ(Parse({"AO"}).mGame, GameType::eAo);
    EXPECT_EQ(Parse({"-AO"}).mGame, GameType::eAo);
}

TEST(CommandLineOptions, GameIsIgnoredWhenBothAreGiven)
{
    EXPECT_EQ(Parse({"AE", "-AO"}).mGame, std::nullopt);
}

TEST(CommandLineOptions, GameIgnoresValuesContainingAGameName)
{
    EXPECT_EQ(Parse({"-record=AO.dat", "-mod=AE_mod"}).mGame, std::nullopt);
}

TEST(CommandLineOptions, ModAndDebugOptions)
{
    const CommandLineOptions options = Parse({"-mod=My Mod", "-ddcheat", "-ddfps", "-ddnoskip", "-ddslowload=250"});

    EXPECT_EQ(options.mModName, "My Mod");
    EXPECT_TRUE(options.mDdCheat);
    EXPECT_TRUE(options.mShowFps);
    EXPECT_TRUE(options.mNoFrameSkip);
    EXPECT_EQ(options.mSlowLoadMs, 250u);

    EXPECT_TRUE(Parse({"-it_is_me_your_father"}).mDdCheat);
}

TEST(CommandLineOptions, InvalidSlowLoadIsIgnored)
{
    EXPECT_EQ(Parse({}).mSlowLoadMs, 0u);
    EXPECT_EQ(Parse({"-ddslowload=soon"}).mSlowLoadMs, 0u);
}

TEST(CommandLineOptions, RecordingOptions)
{
    const CommandLineOptions options = Parse({"-record=run.dat", "-flush"});
    EXPECT_EQ(options.mRecordFile, "run.dat");
    EXPECT_TRUE(options.mFlushRecording);
    EXPECT_EQ(options.mPlayFile, std::nullopt);
}

TEST(CommandLineOptions, PlaybackOptions)
{
    const CommandLineOptions options = Parse({"-play=run.dat", "-ignore_desyncs"});
    EXPECT_EQ(options.mPlayFile, "run.dat");
    EXPECT_TRUE(options.mIgnoreDesyncs);
}

TEST(CommandLineOptions, MaxFps)
{
    EXPECT_EQ(Parse({}).mMaxFps, std::nullopt);
    EXPECT_EQ(Parse({"-max_fps=100"}).mMaxFps, 100u);
    EXPECT_EQ(Parse({"-maxfps=144"}).mMaxFps, 144u);
    EXPECT_EQ(Parse({"-max_fps=0"}).mMaxFps, 0u);
}

TEST(CommandLineOptions, InvalidMaxFpsIsIgnored)
{
    EXPECT_EQ(Parse({"-max_fps=-5"}).mMaxFps, std::nullopt);
    EXPECT_EQ(Parse({"-max_fps=fast"}).mMaxFps, std::nullopt);
    EXPECT_EQ(Parse({"-max_fps="}).mMaxFps, std::nullopt);
    EXPECT_EQ(Parse({"-maxfps=60fps"}).mMaxFps, std::nullopt);
}

TEST(CommandLineOptions, ShowFpsAliases)
{
    EXPECT_TRUE(Parse({"-ddfps"}).mShowFps);
    EXPECT_TRUE(Parse({"-show_fps"}).mShowFps);
    EXPECT_TRUE(Parse({"-showfps"}).mShowFps);
    EXPECT_FALSE(Parse({}).mShowFps);
}

TEST(CommandLineOptions, DisplayOptions)
{
    const CommandLineOptions options = Parse({
        "-renderer=gl",
        "-fullscreen",
        "-keep_aspect_ratio=false",
        "-filter_screen=true",
        "-use_original_resolution=false",
    });

    EXPECT_EQ(options.mRenderer, IRenderer::Renderers::OpenGL);
    EXPECT_EQ(options.mFullscreen, true);
    EXPECT_EQ(options.mKeepAspectRatio, false);
    EXPECT_EQ(options.mFilterScreen, true);
    EXPECT_EQ(options.mUseOriginalResolution, false);
}

TEST(CommandLineOptions, InvalidDisplayValuesAreIgnored)
{
    const CommandLineOptions options = Parse({"-renderer=vulkan", "-fullscreen=yes"});
    EXPECT_EQ(options.mRenderer, std::nullopt);
    EXPECT_EQ(options.mFullscreen, std::nullopt);
}

TEST(CommandLineOptions, UsageListsEveryOption)
{
    const std::string usage = CommandLineOptions::Usage();
    for (const char* option : {"AE", "AO", "-mod=", "-renderer=", "-fullscreen", "-keep_aspect_ratio", "-filter_screen",
                               "-use_original_resolution", "-ddcheat", "-ddfps", "-ddnoskip", "-ddslowload=", "-help",
                               "-record=", "-flush", "-play=", "-ignore_desyncs", "-show_fps", "-showfps",
                               "-max_fps=", "-maxfps="})
    {
        EXPECT_NE(usage.find(option), std::string::npos) << option;
    }
}

// DisplaySettings::ApplyCommandLine

TEST(DisplaySettingsCommandLine, NoOptionsChangeNothing)
{
    DisplaySettings settings;
    EXPECT_FALSE(settings.ApplyCommandLine(Parse({"-ddfps", "AO"})));
    EXPECT_EQ(settings, DisplaySettings{});
}

TEST(DisplaySettingsCommandLine, SetsEveryDisplaySetting)
{
    DisplaySettings settings;
    EXPECT_TRUE(settings.ApplyCommandLine(Parse({
        "-renderer=gl",
        "-fullscreen",
        "-keep_aspect_ratio=false",
        "-filter_screen=false",
        "-use_original_resolution=false",
    })));

    EXPECT_EQ(settings.mRenderer, IRenderer::Renderers::OpenGL);
    EXPECT_TRUE(settings.mFullscreen);
    EXPECT_FALSE(settings.mKeepAspectRatio);
    EXPECT_FALSE(settings.mFilterScreen);
    EXPECT_FALSE(settings.mUseOriginalResolution);
}

TEST(DisplaySettingsCommandLine, ReportsNoChangeWhenValuesAlreadyMatch)
{
    DisplaySettings settings;
    EXPECT_FALSE(settings.ApplyCommandLine(Parse({"-renderer=sdl3", "-keep_aspect_ratio", "-fullscreen=false"})));
}
