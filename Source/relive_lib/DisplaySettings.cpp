#include "stdafx.h"
#include "DisplaySettings.hpp"
#include "IniFile.hpp"
#include "CommandLineOptions.hpp"
#include <cctype>

static void ReadBool(const IniFile& ini, const char* key, bool& value)
{
    if (const auto parsed = ini.GetBool(DisplaySettings::kSection, key))
    {
        value = *parsed;
    }
}

DisplaySettings DisplaySettings::FromIni(const IniFile& ini)
{
    DisplaySettings settings;
    if (const auto name = ini.GetValue(kSection, "renderer"))
    {
        if (const auto renderer = RendererFromString(*name))
        {
            settings.mRenderer = *renderer;
        }
    }
    ReadBool(ini, "fullscreen", settings.mFullscreen);
    ReadBool(ini, "keep_aspect_ratio", settings.mKeepAspectRatio);
    ReadBool(ini, "filter_screen", settings.mFilterScreen);
    ReadBool(ini, "use_original_resolution", settings.mUseOriginalResolution);
    return settings;
}

void DisplaySettings::WriteTo(IniFile& ini) const
{
    IniFile::Section section{kSection, {}};
    section.mEntries.push_back({"renderer", RendererToString(mRenderer)});
    section.mEntries.push_back({"fullscreen", mFullscreen ? "true" : "false"});
    section.mEntries.push_back({"keep_aspect_ratio", mKeepAspectRatio ? "true" : "false"});
    section.mEntries.push_back({"filter_screen", mFilterScreen ? "true" : "false"});
    section.mEntries.push_back({"use_original_resolution", mUseOriginalResolution ? "true" : "false"});
    ini.ReplaceSection(std::move(section));
}

template <typename T>
static bool ApplyOption(const std::optional<T>& option, T& value)
{
    if (!option || *option == value)
    {
        return false;
    }
    value = *option;
    return true;
}

bool DisplaySettings::ApplyCommandLine(const CommandLineOptions& options)
{
    bool changed = false;
    changed |= ApplyOption(options.mRenderer, mRenderer);
    changed |= ApplyOption(options.mFullscreen, mFullscreen);
    changed |= ApplyOption(options.mKeepAspectRatio, mKeepAspectRatio);
    changed |= ApplyOption(options.mFilterScreen, mFilterScreen);
    changed |= ApplyOption(options.mUseOriginalResolution, mUseOriginalResolution);
    return changed;
}

void DisplaySettings::ApplyTo(IRenderer& renderer, SDL_Window* pWindow) const
{
    renderer.SetUseOriginalResolution(mUseOriginalResolution);
    renderer.SetFilterScreen(mFilterScreen);
    renderer.SetKeepAspectRatio(mKeepAspectRatio);
    SDL_SetWindowFullscreen(pWindow, mFullscreen);
}

std::optional<IRenderer::Renderers> DisplaySettings::RendererFromString(const std::string& name)
{
    std::string lower = name;
    for (char& c : lower)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    if (lower == "sdl" || lower == "sdl3")
    {
        return IRenderer::Renderers::Sdl3;
    }
    if (lower == "gl" || lower == "gl3" || lower == "opengl" || lower == "opengl3")
    {
        return IRenderer::Renderers::OpenGL;
    }
    return std::nullopt;
}

const char* DisplaySettings::RendererToString(IRenderer::Renderers renderer)
{
    return renderer == IRenderer::Renderers::OpenGL ? "opengl" : "sdl3";
}

bool DisplaySettings::operator==(const DisplaySettings& rhs) const
{
    return mRenderer == rhs.mRenderer
        && mFullscreen == rhs.mFullscreen
        && mKeepAspectRatio == rhs.mKeepAspectRatio
        && mFilterScreen == rhs.mFilterScreen
        && mUseOriginalResolution == rhs.mUseOriginalResolution;
}
