#include "Sdl3Context.hpp"
#include "../../Window.hpp"
#include "../../FatalError.hpp"

Sdl3Context::Sdl3Context(Window& window)
{
    mRenderer.reset(SDL_CreateRenderer(window.Get(), NULL));
    if (!mRenderer)
    {
        ALIVE_FATAL("Couldnt create SDL3 renderer: %s", SDL_GetError());
    }

    LOG_INFO("SDL3 renderer name: %s", SDL_GetRendererName(mRenderer.get()));
}

SDL_Renderer* Sdl3Context::GetRenderer()
{
    return mRenderer.get();
}

bool Sdl3Context::IsRenderTargetSupported()
{
    const SdlTexturePtr texture(SDL_CreateTexture(mRenderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 1, 1));
    return texture != nullptr;
}

SdlTexturePtr Sdl3Context::CreateTexture(SDL_PixelFormat format, SDL_TextureAccess access, u32 width, u32 height)
{
    SdlTexturePtr texture(SDL_CreateTexture(mRenderer.get(), format, access, width, height));
    if (!texture)
    {
        ALIVE_FATAL("SDL_CreateTexture(%ux%u) failed: %s", width, height, SDL_GetError());
    }
    return texture;
}

void Sdl3Context::Present()
{
    SDL_RenderPresent(mRenderer.get());
}

void Sdl3Context::RestoreFramebuffer()
{
    SDL_SetRenderTarget(mRenderer.get(), mLastFramebuffer);
    mLastFramebuffer = nullptr;

    if (mLastClipRect.x != 0 || mLastClipRect.y != 0 || mLastClipRect.w != 0 || mLastClipRect.h != 0)
    {
        SDL_SetRenderClipRect(mRenderer.get(), &mLastClipRect);
    }
}

void Sdl3Context::SaveFramebuffer()
{
    SDL_GetRenderClipRect(mRenderer.get(), &mLastClipRect);
    mLastFramebuffer = SDL_GetRenderTarget(mRenderer.get());
    SDL_SetRenderTarget(mRenderer.get(), nullptr);
}

void Sdl3Context::UseScreenFramebuffer()
{
    SDL_SetRenderTarget(mRenderer.get(), nullptr);
}

void Sdl3Context::UseTextureFramebuffer(SDL_Texture* texture)
{
    SDL_SetRenderTarget(mRenderer.get(), texture);
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
    const SdlTexturePtr texture(SDL_CreateTexture(mRenderer.get(), SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1));
    if (!texture)
    {
        return false;
    }

    const bool supported =
        SDL_SetTextureBlendMode(texture.get(), PsxTextureBlendMode()) &&
        SDL_SetTextureBlendMode(texture.get(), PsxTextureSubtractBlendMode()) &&
        SDL_SetTextureBlendMode(texture.get(), Fg1MaskBlendMode()) &&
        SDL_SetRenderDrawBlendMode(mRenderer.get(), SubtractBlendMode());

    SDL_SetRenderDrawBlendMode(mRenderer.get(), SDL_BLENDMODE_NONE);
    return supported;
}

void Sdl3Context::SetDrawBlendMode(SDL_BlendMode blendMode)
{
    if (!SDL_SetRenderDrawBlendMode(mRenderer.get(), blendMode))
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
