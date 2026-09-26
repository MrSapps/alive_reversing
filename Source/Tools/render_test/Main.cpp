#include "stdafx.h"
#include "RenderTest.hpp"
#include "../../relive_lib/CommandLineParser.hpp"
#include "../../relive_lib/GameType.hpp"
#include "../../relive_lib/data_conversion/file_system.hpp"
#include "../../AliveLibAE/GameAutoPlayer.hpp"
#include "../../AliveLibAO/GameAutoPlayer.hpp"
#include <SDL3/SDL_main.h>
#include <cstdio>

static const char* kUsage =
    "Usage: relive_render_test [options]\n"
    "Draws test scenes with the real renderers, without any game data.\n"
    "\n"
    "  -renderer=<opengl|sdl3|all>  Which renderer(s) to use (default: all)\n"
    "  -AO                          Use AO's coordinates and camera placement (default: AE)\n"
    "  -scene=<text>                Only scenes whose name contains text; -list shows them\n"
    "  -auto                        Run every scene unattended: check for leaks, time them,\n"
    "                               save captures and a report, then exit (non zero on failure)\n"
    "  -frames=<n>                  Frames timed per scene with -auto (default 120)\n"
    "  -out=<dir>                   Where -auto writes to (default render_test_out)\n"
    "  -baseline=<dir>              Fail if captures differ from an earlier -auto run's\n"
    "  -renderer_checks             Check for renderer errors as it goes (slower)\n"
    "  -list                        List the scenes and exit\n"
    "\n"
    "Keys: left/right change scene, H hide text, P pause, F filtering, U uncapped fps,\n"
    "      R next renderer, C save a capture, Escape quit\n";

// relive_lib calls out to this for recording and playback, which the render test doesn't use
BaseGameAutoPlayer& GetGameAutoPlayer()
{
    static GameAutoPlayer autoPlayerAE;
    static AO::GameAutoPlayer autoPlayerAO;
    if (GetGameType() == GameType::eAo)
    {
        return autoPlayerAO;
    }
    return autoPlayerAE;
}

static bool ParseRenderers(const std::string& value, std::vector<IRenderer::Renderers>& renderers)
{
    if (value == "all")
    {
        renderers = {IRenderer::Renderers::OpenGL, IRenderer::Renderers::Sdl3};
    }
    else if (value == "opengl" || value == "gl")
    {
        renderers = {IRenderer::Renderers::OpenGL};
    }
    else if (value == "sdl3" || value == "sdl")
    {
        renderers = {IRenderer::Renderers::Sdl3};
    }
    else
    {
        return false;
    }
    return true;
}

s32 main(s32 argc, char_type** argv)
{
    const CommandLineParser args(argc, argv);
    if (args.HasSwitch("-help") || args.HasSwitch("--help") || args.HasSwitch("-h"))
    {
        printf("%s", kUsage);
        return 0;
    }

    if (args.HasSwitch("-list"))
    {
        RenderTest::ListScenes();
        return 0;
    }

    RenderTestOptions options;
    options.mAuto = args.HasSwitch("-auto");
    options.mSceneFilter = args.GetValue("-scene").value_or("");
    options.mOutDir = args.GetValue("-out").value_or(options.mOutDir);
    options.mBaselineDir = args.GetValue("-baseline").value_or("");
    options.mRendererChecks = args.HasSwitch("-renderer_checks");
    if (const auto frames = args.GetValue("-frames"))
    {
        options.mTimedFrames = static_cast<u32>(std::max(1, std::atoi(frames->c_str())));
    }
    if (!ParseRenderers(args.GetValue("-renderer").value_or("all"), options.mRenderers))
    {
        printf("Unknown renderer\n\n%s", kUsage);
        return 1;
    }

    const GameType gameType = args.HasSwitch("-AO") ? GameType::eAo : GameType::eAe;
    SetGameType(gameType);

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    s32 exitCode = 0;
    {
        FileSystem fs;
        RenderTest test(gameType, fs, options);
        exitCode = test.Run();
    }

    SDL_Quit();
    return exitCode;
}
