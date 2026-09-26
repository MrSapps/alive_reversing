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

    const SDL_PixelFormat* pFormats = static_cast<const SDL_PixelFormat*>(
        SDL_GetPointerProperty(SDL_GetRendererProperties(mRenderer.get()), SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, nullptr));
    for (; pFormats && *pFormats != SDL_PIXELFORMAT_UNKNOWN; pFormats++)
    {
        if (*pFormats == SDL_PIXELFORMAT_INDEX8)
        {
            mSupportsPaletteTextures = true;
        }
    }
    LOG_INFO("SDL3 renderer palette textures: %s", mSupportsPaletteTextures ? "yes" : "no");

    mSupportsBrightVertexColours = ProbeBrightVertexColours();
    LOG_INFO("SDL3 renderer vertex colours above 1: %s", mSupportsBrightVertexColours ? "yes" : "no");
}

// Draws a dark grey texel with vertex colour 2 and checks it comes out twice as bright. SDL passes
// vertex colours to the GPU as floats, but a renderer could clamp them to 1.
bool Sdl3Context::ProbeBrightVertexColours()
{
    SDL_Renderer* pRenderer = mRenderer.get();
    const SdlTexturePtr target(SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 1, 1));
    const SdlTexturePtr source(SDL_CreateTexture(pRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, 1, 1));
    if (!target || !source)
    {
        return false;
    }

    const u8 grey[4] = {64, 64, 64, 255};
    SDL_UpdateTexture(source.get(), nullptr, grey, 4);
    SDL_SetTextureBlendMode(source.get(), SDL_BLENDMODE_NONE);

    SDL_Texture* pOldTarget = SDL_GetRenderTarget(pRenderer);
    SDL_SetRenderTarget(pRenderer, target.get());

    // One triangle over the whole target
    const SDL_FColor bright = {2.0f, 2.0f, 2.0f, 1.0f};
    const SDL_Vertex vertices[3] = {
        {{-1.0f, -1.0f}, bright, {0.5f, 0.5f}},
        {{3.0f, -1.0f}, bright, {0.5f, 0.5f}},
        {{-1.0f, 3.0f}, bright, {0.5f, 0.5f}}};
    SDL_RenderGeometry(pRenderer, source.get(), vertices, 3, nullptr, 0);

    bool supported = false;
    if (SDL_Surface* pSurface = SDL_RenderReadPixels(pRenderer, nullptr))
    {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        SDL_ReadSurfacePixel(pSurface, 0, 0, &r, &g, &b, nullptr);
        supported = r > 96;
        SDL_DestroySurface(pSurface);
    }

    SDL_SetRenderTarget(pRenderer, pOldTarget);
    return supported;
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

    // Unfiltered when scaled, as the OpenGL renderer samples every texture. SDL's default is linear.
    SDL_SetTextureScaleMode(texture.get(), SDL_SCALEMODE_NEAREST);
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

SDL_BlendMode Sdl3Context::GasMaskBlendMode()
{
    // Colour: dst * src (the mask keeps or blacks out the gas). Alpha: src (the mask's)
    return SDL_ComposeCustomBlendMode(
        SDL_BLENDFACTOR_ZERO,
        SDL_BLENDFACTOR_SRC_COLOR,
        SDL_BLENDOPERATION_ADD,
        SDL_BLENDFACTOR_ONE,
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
        SDL_SetTextureBlendMode(texture.get(), GasMaskBlendMode()) &&
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

SDL_Palette* Sdl3Context::SharedPalette(const PaletteKey& key, const SDL_Color (&colours)[256])
{
    // Textures hold their own reference to their palette, so dropping one from here is safe
    constexpr std::size_t kMaxSharedPalettes = 512;

    auto it = mSharedPalettes.find(key);
    if (it == mSharedPalettes.end())
    {
        if (mSharedPalettes.size() >= kMaxSharedPalettes)
        {
            auto leastRecent = mSharedPalettes.begin();
            for (auto i = mSharedPalettes.begin(); i != mSharedPalettes.end(); ++i)
            {
                if (i->second.mLastUsed < leastRecent->second.mLastUsed)
                {
                    leastRecent = i;
                }
            }
            mSharedPalettes.erase(leastRecent);
        }

        SdlPalettePtr palette(SDL_CreatePalette(256));
        if (!palette || !SDL_SetPaletteColors(palette.get(), colours, 0, 256))
        {
            ALIVE_FATAL("Creating an SDL palette failed: %s", SDL_GetError());
        }
        it = mSharedPalettes.emplace(key, SharedPaletteEntry{std::move(palette), 0}).first;
    }

    it->second.mLastUsed = ++mSharedPaletteUseCounter;
    return it->second.mPalette.get();
}
