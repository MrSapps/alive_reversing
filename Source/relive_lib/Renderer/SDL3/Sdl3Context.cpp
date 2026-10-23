#include "Sdl3Context.hpp"
#include "../../Window.hpp"
#include "../../FatalError.hpp"

// The textures that are waiting to be reused. A texture goes back here when its SdlTexturePtr is
// let go of, and the pool is shared so that a texture that outlives the context's members still
// has somewhere to go.
struct SdlTexturePoolState final
{
    // Like the OpenGL renderer's pool
    static constexpr std::size_t kMaxPooledBytes = 96 * 1024 * 1024;

    struct Entry final
    {
        SDL_Texture* mTexture = nullptr;
        SDL_PixelFormat mFormat = SDL_PIXELFORMAT_UNKNOWN;
        SDL_TextureAccess mAccess = SDL_TEXTUREACCESS_STATIC;
        u32 mWidth = 0;
        u32 mHeight = 0;
        std::size_t mBytes = 0;
        // The frame it was let go of in: SDL has to draw what is queued with a texture before
        // the texture changes, so it isn't reused until the next frame
        u32 mFrame = 0;
    };

    ~SdlTexturePoolState()
    {
        Clear();
    }

    void Clear()
    {
        for (const Entry& entry : mFree)
        {
            SDL_DestroyTexture(entry.mTexture);
        }
        mFree.clear();
        mFreeBytes = 0;
    }

    void Give(SDL_Texture* pTexture)
    {
        if (!pTexture)
        {
            return;
        }

        if (mClosed)
        {
            SDL_DestroyTexture(pTexture);
            return;
        }

        const SDL_PropertiesID props = SDL_GetTextureProperties(pTexture);
        Entry entry;
        entry.mTexture = pTexture;
        entry.mFormat = static_cast<SDL_PixelFormat>(SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, SDL_PIXELFORMAT_UNKNOWN));
        entry.mAccess = static_cast<SDL_TextureAccess>(SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC));
        entry.mWidth = static_cast<u32>(SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_WIDTH_NUMBER, 0));
        entry.mHeight = static_cast<u32>(SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_HEIGHT_NUMBER, 0));
        entry.mBytes = static_cast<std::size_t>(entry.mWidth) * entry.mHeight * SDL_BYTESPERPIXEL(entry.mFormat);
        entry.mFrame = mFrame;

        mFree.push_back(entry);
        mFreeBytes += entry.mBytes;

        // The ones let go of longest ago are given up first
        while (mFreeBytes > kMaxPooledBytes && !mFree.empty())
        {
            mFreeBytes -= mFree.front().mBytes;
            SDL_DestroyTexture(mFree.front().mTexture);
            mFree.erase(mFree.begin());
        }
    }

    SDL_Texture* Take(SDL_PixelFormat format, SDL_TextureAccess access, u32 width, u32 height)
    {
        for (std::size_t i = mFree.size(); i-- > 0;)
        {
            const Entry& entry = mFree[i];
            if (entry.mFormat == format && entry.mAccess == access && entry.mWidth == width && entry.mHeight == height && entry.mFrame != mFrame)
            {
                SDL_Texture* pTexture = entry.mTexture;
                mFreeBytes -= entry.mBytes;
                mFree.erase(mFree.begin() + static_cast<std::ptrdiff_t>(i));
                mReuseCount++;

                // What a new texture has: SDL blends the ones with an alpha channel
                SDL_SetTextureBlendMode(pTexture, SDL_ISPIXELFORMAT_ALPHA(format) ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
                SDL_SetTextureColorMod(pTexture, 255, 255, 255);
                SDL_SetTextureAlphaMod(pTexture, 255);
                return pTexture;
            }
        }
        return nullptr;
    }

    std::vector<Entry> mFree;
    std::size_t mFreeBytes = 0;
    u32 mFrame = 0;
    u32 mReuseCount = 0;
    // The renderer is gone: textures are destroyed, not kept
    bool mClosed = false;
};

void SdlTextureDeleter::operator()(SDL_Texture* pTexture) const
{
    if (mPool)
    {
        mPool->Give(pTexture);
    }
    else
    {
        SDL_DestroyTexture(pTexture);
    }
}

Sdl3Context::Sdl3Context(Window& window, bool checks)
    : mPool(std::make_shared<SdlTexturePoolState>())
{
    if (checks)
    {
        SDL_SetHint(SDL_HINT_RENDER_GPU_DEBUG, "1");
        SDL_SetHint(SDL_HINT_RENDER_VULKAN_DEBUG, "1");
        SDL_SetHint(SDL_HINT_RENDER_DIRECT3D11_DEBUG, "1");
    }

    mRenderer.reset(SDL_CreateRenderer(window.Get(), NULL));
    if (!mRenderer)
    {
        ALIVE_FATAL("Couldnt create SDL3 renderer: %s", SDL_GetError());
    }

    LOG_INFO("SDL3 renderer name: %s", SDL_GetRendererName(mRenderer.get()));

    mSupportsBrightVertexColours = ProbeBrightVertexColours();
    LOG_INFO("SDL3 renderer vertex colours above 1: %s", mSupportsBrightVertexColours ? "yes" : "no");
}

Sdl3Context::~Sdl3Context()
{
    LOG_INFO("SDL3 texture pool: %u textures reused, %u waiting", mPool->mReuseCount, static_cast<u32>(mPool->mFree.size()));

    // The textures have to go before the renderer they belong to
    mPool->Clear();
    mPool->mClosed = true;
}

void Sdl3Context::NextFrame()
{
    mFrameNumber++;
    mPool->mFrame = mFrameNumber;
}

u32 Sdl3Context::PooledTextureCount() const
{
    return static_cast<u32>(mPool->mFree.size());
}

u32 Sdl3Context::ReusedTextureCount() const
{
    return mPool->mReuseCount;
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

SDL_Renderer* Sdl3Context::GetRenderer() const
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
    if (SDL_Texture* pPooled = mPool->Take(format, access, width, height))
    {
        return SdlTexturePtr(pPooled, SdlTextureDeleter{mPool});
    }

    SdlTexturePtr texture(SDL_CreateTexture(mRenderer.get(), format, access, width, height), SdlTextureDeleter{mPool});
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
    // src + dst * src alpha: see Sdl3Texture::ConvertTexel for what alpha is set to
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
