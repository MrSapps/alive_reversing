#pragma once

#include <SDL3/SDL.h>

class Window;

class Sdl3Context final
{
public:
    explicit Sdl3Context(Window& window);
    ~Sdl3Context();

    SDL_Renderer* GetRenderer();
    bool IsRenderTargetSupported();
    void Present();
    void RestoreFramebuffer();
    void SaveFramebuffer();
    void UseScreenFramebuffer();
    void UseTextureFramebuffer(SDL_Texture* texture);

private:
    SDL_Renderer* mRenderer;
    SDL_Rect mLastClipRect;
    SDL_Texture* mLastFramebuffer;
};
