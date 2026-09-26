#include "Sdl3Context.hpp"
#include "../../Window.hpp"
#include "../../FatalError.hpp"

Sdl3Context::Sdl3Context(Window& window)
{
    mRenderer = SDL_CreateRenderer(window.Get(), NULL);
    if (!mRenderer)
    {
        ALIVE_FATAL("Couldnt create SDL3 renderer: %s", SDL_GetError());
    }

    LOG_INFO("SDL3 renderer name: %s", SDL_GetRendererName(mRenderer));
}

Sdl3Context::~Sdl3Context()
{
    SDL_DestroyRenderer(mRenderer);
}

SDL_Renderer* Sdl3Context::GetRenderer()
{
    return mRenderer;
}

bool Sdl3Context::IsRenderTargetSupported()
{
    SDL_Texture* texture = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 1, 1);
    const bool isTargetSupported = texture != NULL;

    if (texture)
    {
        SDL_DestroyTexture(texture);
    }

    return isTargetSupported;
}

void Sdl3Context::Present()
{
    SDL_RenderPresent(mRenderer);
}

void Sdl3Context::RestoreFramebuffer()
{
    SDL_SetRenderTarget(mRenderer, mLastFramebuffer);
    mLastFramebuffer = nullptr;

    if (mLastClipRect.x != 0 || mLastClipRect.y != 0 || mLastClipRect.w != 0 || mLastClipRect.h != 0)
    {
        SDL_SetRenderClipRect(mRenderer, &mLastClipRect);
    }
}

void Sdl3Context::SaveFramebuffer()
{
    SDL_GetRenderClipRect(mRenderer, &mLastClipRect);
    mLastFramebuffer = SDL_GetRenderTarget(mRenderer);
    SDL_SetRenderTarget(mRenderer, nullptr);
}

void Sdl3Context::UseScreenFramebuffer()
{
    SDL_SetRenderTarget(mRenderer, nullptr);
}

void Sdl3Context::UseTextureFramebuffer(SDL_Texture* texture)
{
    SDL_SetRenderTarget(mRenderer, texture);
}

SDL_BlendMode Sdl3Context::SubtractBlendMode()
{
    // dst - src. Alpha uses the same operation (dst alpha - 0) because SDL's OpenGL renderers
    // reject modes with different colour and alpha operations.
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_REV_SUBTRACT,
        SDL_BLENDFACTOR_ZERO,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_REV_SUBTRACT);
}

SDL_BlendMode Sdl3Context::PsxTextureBlendMode()
{
    // src + dst * src alpha: see Sdl3Texture::GetTextureUsePalette for what alpha is set to
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDFACTOR_SRC_ALPHA,
        SDL_BLENDOPERATION_ADD);
}

SDL_BlendMode Sdl3Context::PsxTextureSubtractBlendMode()
{
    // dst - src * src alpha
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_SRC_ALPHA,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_REV_SUBTRACT,
        SDL_BLENDFACTOR_SRC_ALPHA,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_REV_SUBTRACT);
}

SDL_BlendMode Sdl3Context::Fg1MaskBlendMode()
{
    // Keeps dst's colour and takes the mask's alpha
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ZERO,
        SDL_BLENDFACTOR_ONE,
        SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_SRC_ALPHA,
        SDL_BLENDFACTOR_ZERO,
        SDL_BLENDOPERATION_ADD);
}

bool Sdl3Context::SupportsCustomBlendModes()
{
    SDL_Texture* texture = SDL_CreateTexture(mRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1);
    if (!texture)
    {
        return false;
    }

    const bool supported =
        SDL_SetTextureBlendMode(texture, PsxTextureBlendMode()) &&
        SDL_SetTextureBlendMode(texture, PsxTextureSubtractBlendMode()) &&
        SDL_SetTextureBlendMode(texture, Fg1MaskBlendMode()) &&
        SDL_SetRenderDrawBlendMode(mRenderer, SubtractBlendMode());

    SDL_DestroyTexture(texture);
    SDL_SetRenderDrawBlendMode(mRenderer, SDL_BLENDMODE_NONE);
    return supported;
}

void Sdl3Context::SetDrawBlendMode(SDL_BlendMode blendMode)
{
    if (!SDL_SetRenderDrawBlendMode(mRenderer, blendMode))
    {
        ALIVE_FATAL("SDL_SetRenderDrawBlendMode(0x%x) failed: %s", blendMode, SDL_GetError());
    }
}

void Sdl3Context::SetTextureBlendMode(SDL_Texture* texture, SDL_BlendMode blendMode)
{
    if (!SDL_SetTextureBlendMode(texture, blendMode))
    {
        ALIVE_FATAL("SDL_SetTextureBlendMode(0x%x) failed: %s", blendMode, SDL_GetError());
    }
}
