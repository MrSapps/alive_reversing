#pragma once

#include "Types.hpp"
#include "Renderer/IRenderer.hpp"
#include <optional>
#include <string>

struct CommandLineOptions;
class IniFile;

// The display settings, saved in the [Display] section of relive.ini (see
// ResourceManagerWrapper::LoadSettingsIni) so they persist between runs. The F9-F12 hotkeys
// change them while running (see Sys.cpp), and so can the command line. The renderer is
// chosen at startup and can't be switched while running.
struct DisplaySettings final
{
    static constexpr const char* kSection = "Display";

    IRenderer::Renderers mRenderer = IRenderer::Renderers::Sdl3;
    bool mFullscreen = false;
    // true = letterboxed 4:3, false = stretched to fill the window (e.g. 16:9)
    bool mKeepAspectRatio = true;
    bool mFilterScreen = true;
    bool mUseOriginalResolution = true;

    // Missing or invalid values keep their defaults.
    static DisplaySettings FromIni(const IniFile& ini);

    // Replaces the [Display] section of ini, keeping the others.
    void WriteTo(IniFile& ini) const;

    // Applies the display settings given on the command line. Returns true if anything changed.
    bool ApplyCommandLine(const CommandLineOptions& options);

    // Applies everything but mRenderer, which is only used when the renderer is created.
    void ApplyTo(IRenderer& renderer, SDL_Window* pWindow) const;

    // Names as used by -renderer= and the ini: "sdl"/"sdl3", "gl"/"gl3"/"opengl"/"opengl3".
    static std::optional<IRenderer::Renderers> RendererFromString(const std::string& name);
    static const char* RendererToString(IRenderer::Renderers renderer);

    bool operator==(const DisplaySettings& rhs) const;
    bool operator!=(const DisplaySettings& rhs) const
    {
        return !(*this == rhs);
    }
};
