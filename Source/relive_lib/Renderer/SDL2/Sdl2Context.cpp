#include "Sdl2Context.hpp"

Sdl2Context::Sdl2Context(SDL_Window* window)
    : mWindow(window)
{
    mRenderer = SDL_CreateRenderer(window, -1, 0);
}

Sdl2Context::~Sdl2Context()
{
}

SDL_Renderer* Sdl2Context::GetRenderer()
{
    return mRenderer;
}

bool Sdl2Context::IsRenderTargetSupported()
{
    return SDL_RenderTargetSupported(mRenderer);
}

void Sdl2Context::Present()
{
    SDL_RenderPresent(mRenderer);
}

void Sdl2Context::RestoreFramebuffer()
{
    SDL_SetRenderTarget(mRenderer, mLastFramebuffer);
    mLastFramebuffer = NULL;

    if (mLastClipRect.x != 0 || mLastClipRect.y != 0 || mLastClipRect.w != 0 || mLastClipRect.h != 0)
    {
        SDL_RenderSetClipRect(mRenderer, &mLastClipRect);
    }
}

void Sdl2Context::SaveFramebuffer()
{
    SDL_RenderGetClipRect(mRenderer, &mLastClipRect);
    mLastFramebuffer = SDL_GetRenderTarget(mRenderer);
    SDL_SetRenderTarget(mRenderer, NULL);
}

void Sdl2Context::UseScreenFramebuffer()
{
    SDL_SetRenderTarget(mRenderer, NULL);
}

void Sdl2Context::UseTextureFramebuffer(SDL_Texture* texture)
{
    SDL_SetRenderTarget(mRenderer, texture);
}
