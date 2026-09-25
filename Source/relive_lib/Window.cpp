#include "stdafx.h"
#include "Window.hpp"
#include "AppIcon.hpp"
#include "relive_config.h"

#include <vector>

Window::~Window()
{
    Destroy();
}

static void AddRenderer(std::vector<IRenderer::Renderers>& renderers, IRenderer::Renderers toAdd)
{
    for (auto r : renderers)
    {
        if (r == toAdd)
        {
            return;
        }
    }
    renderers.emplace_back(toAdd);
}

bool Window::CreateWithRenderer(IRenderer::Renderers type, const std::string& title)
{
    std::vector<IRenderer::Renderers> creationOrder{type};
    AddRenderer(creationOrder, IRenderer::Renderers::Sdl3);
    AddRenderer(creationOrder, IRenderer::Renderers::OpenGL);

    for (IRenderer::Renderers typeToCreate : creationOrder)
    {
        const bool isOpenGL = typeToCreate == IRenderer::Renderers::OpenGL;
        if (!Create(title + (isOpenGL ? " [OpenGL3]" : " [SDL3]"), isOpenGL ? SDL_WINDOW_OPENGL : 0))
        {
            LOG_INFO("No window, skip creating renderer");
            continue;
        }

        if (IRenderer::CreateRenderer(typeToCreate, *this))
        {
            return true;
        }
        Destroy();
    }
    return false;
}

static void SetWindowIcon(SDL_Window* pWindow)
{
    SDL_Surface* iconSurface = SDL_CreateSurfaceFrom(kAppIconWidth, kAppIconHeight, SDL_PIXELFORMAT_RGBA32, const_cast<u8*>(kAppIconRGBA32), kAppIconWidth * 4);
    if (iconSurface)
    {
        // Window managers (X11/Wayland/Windows taskbar) only pick this up from the running
        // process - the exe resource icon (see resource.rc) doesn't cover it.
        SDL_SetWindowIcon(pWindow, iconSurface);
        SDL_DestroySurface(iconSurface);
    }
    else
    {
        LOG_ERROR("Failed to create window icon surface %s", SDL_GetError());
    }
}

bool Window::Create(const std::string& title, u32 extraFlags)
{
    TRACE_ENTRYEXIT;

    mWindow = SDL_CreateWindow(title.c_str(), 640, 480, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | extraFlags);
    if (!mWindow)
    {
        LOG_ERROR("Window create with flags %d failed with %s", extraFlags, SDL_GetError());
        return false;
    }

    LOG_INFO("Window created");

    SetWindowIcon(mWindow);

    if (!SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED))
    {
        // wayland doesn't allow setting the window pos
        LOG_INFO("%s", SDL_GetError());
    }

    SDL_HideCursor();
    return true;
}

void Window::Destroy()
{
    // The renderer draws to the window, so it has to go first
    IRenderer::FreeRenderer();

    if (mWindow)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

void Window::GetSize(s32& width, s32& height) const
{
    SDL_GetWindowSize(mWindow, &width, &height);
}

void Window::GetSizeInPixels(s32& width, s32& height) const
{
    SDL_GetWindowSizeInPixels(mWindow, &width, &height);
}

bool Window::IsMinimized() const
{
    return (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MINIMIZED) != 0;
}

bool Window::IsFullscreen() const
{
    return (SDL_GetWindowFlags(mWindow) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Window::SetFullscreen(bool fullscreen)
{
    SDL_SetWindowFullscreen(mWindow, fullscreen);
}

void Window::Show()
{
    SDL_ShowWindow(mWindow);
    SDL_RaiseWindow(mWindow);
}

std::string Window::BuildString()
{
#ifdef BUILD_NUMBER
    // Automated CI build title
    return std::string("(") + CI_PROVIDER + " Build: " + std::to_string(BUILD_NUMBER) + ")";
#else
    return "";
#endif
}

std::string Window::BuildAndBitnessString()
{
    std::string buildAndBitness;
    std::string buildStr = BuildString();

    LOG_INFO("Build String is: %s", buildStr.c_str());

    if (!buildStr.empty())
    {
        buildAndBitness += " ";
        buildAndBitness += buildStr;
    }

    std::string kBitness = sizeof(void*) == 4 ? " (32 bit)" : " (64 bit)";
    buildAndBitness += kBitness;
    return buildAndBitness;
}

static std::string ModNameSuffix(const std::string& modName)
{
    return modName.empty() ? "" : (" [" + modName + "]");
}

std::string Window::TitleAO(const std::string& modName)
{
    return "R.E.L.I.V.E. Oddworld Abe's Oddysee" + ModNameSuffix(modName) + BuildAndBitnessString();
}

std::string Window::TitleAE(const std::string& modName)
{
    return "R.E.L.I.V.E. Oddworld Abe's Exoddus" + ModNameSuffix(modName) + BuildAndBitnessString();
}
