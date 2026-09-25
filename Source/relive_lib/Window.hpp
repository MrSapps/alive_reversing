#pragma once

#include "Types.hpp"
#include "Renderer/IRenderer.hpp"
#include <string>

// The game's window, and the renderer that draws to it.
class Window final
{
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Creates the window and its renderer, falling back to the other renderers if type can't
    // be created. Each renderer gets a window of its own, as they need different window flags.
    bool CreateWithRenderer(IRenderer::Renderers type, const std::string& title);

    // Only for SDL calls that need the window itself, like creating a renderer on it
    SDL_Window* Get() const
    {
        return mWindow;
    }

    void GetSize(s32& width, s32& height) const;
    void GetSizeInPixels(s32& width, s32& height) const;
    bool IsMinimized() const;
    bool IsFullscreen() const;
    void SetFullscreen(bool fullscreen);
    // Shows the window, bringing it to the front with input focus
    void Show();

    // The CI build number, if this is a CI build
    static std::string BuildString();
    static std::string BuildAndBitnessString();
    static std::string TitleAO(const std::string& modName = "");
    static std::string TitleAE(const std::string& modName = "");

private:
    bool Create(const std::string& title, u32 extraFlags);
    void Destroy();

    SDL_Window* mWindow = nullptr;
};
