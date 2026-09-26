#include "Sdl3Texture.hpp"
#include <algorithm>
#include "FatalError.hpp"
#include "Renderer/PaletteCache.hpp"
#include "data_conversion/AnimationConverter.hpp"

Sdl3Texture::Sdl3Texture(Sdl3Context& context, u32 width, u32 height, SDL_PixelFormat format, SDL_TextureAccess access)
    : mContext(context), mFormat(format), mTextureAccess(access), mHeight(height), mWidth(width)
{
    // SDL2 does not support palette textures, if we want to store indexed
    // colour we have to handle that internally (using mIndexedPixels)
    // TODO: SDL now supports palette textures since version 3.4
    if (mFormat == SDL_PIXELFORMAT_INDEX8)
    {
        mIndexedPixels.resize(mWidth * mHeight);
    }
    else
    {
        mTexture = mContext.CreateTexture(mFormat, mTextureAccess, mWidth, mHeight);
        mContext.CountTextureUpload();

        Sdl3Context::SetTextureBlendMode(mTexture.get(), SDL_BLENDMODE_NONE);
    }
}

std::shared_ptr<Sdl3Texture> Sdl3Texture::FromMask(Sdl3Context& context, std::shared_ptr<Sdl3Texture> srcTex, const u8* maskPixels)
{
    auto resultTex = std::make_shared<Sdl3Texture>(context, srcTex->mWidth, srcTex->mHeight, srcTex->mFormat, SDL_TEXTUREACCESS_TARGET);

    // Create mask texture
    u8* texPixels;
    s32 pitch;
    const SdlTexturePtr maskTex = context.CreateTexture(SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, srcTex->mWidth, srcTex->mHeight);

    SDL_LockTexture(maskTex.get(), NULL, (void**) &texPixels, &pitch);

    for (s32 i = 0; i < pitch * static_cast<s32>(srcTex->mHeight); i += 4)
    {
        s32 r = i;
        s32 g = i + 1;
        s32 b = i + 2;
        s32 a = i + 3;

        // Mask out black pixels
        if (maskPixels[r] == 0 && maskPixels[g] == 0 && maskPixels[b] == 0)
        {
            texPixels[a] = 0;
        }
        else
        {
            texPixels[a] = 255;
        }
    }

    SDL_UnlockTexture(maskTex.get());

    // Paint result texture
    const SDL_BlendMode blendMode = Sdl3Context::Fg1MaskBlendMode();

    context.SaveFramebuffer();
    context.UseTextureFramebuffer(resultTex->GetTexture());

    SDL_RenderTexture(context.GetRenderer(), srcTex->GetTexture(), NULL, NULL);

    Sdl3Context::SetTextureBlendMode(maskTex.get(), blendMode);
    SDL_RenderTexture(context.GetRenderer(), maskTex.get(), NULL, NULL);

    Sdl3Context::SetTextureBlendMode(resultTex->GetTexture(), SDL_BLENDMODE_BLEND);

    context.RestoreFramebuffer();

    return resultTex;
}

u32 Sdl3Texture::GetHeight()
{
    return mHeight;
}

u32 Sdl3Texture::GetWidth()
{
    return mWidth;
}

SDL_Texture* Sdl3Texture::GetTexture()
{
    if (mFormat == SDL_PIXELFORMAT_INDEX8)
    {
        ALIVE_FATAL("%s", "SDL3 use GetTextureUsePalette with INDEX8 tex");
    }

    return mTexture.get();
}

SDL_Texture* Sdl3Texture::GetTextureUsePalette(const std::shared_ptr<AnimationPal>& palette, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode)
{
    if (mFormat != SDL_PIXELFORMAT_INDEX8)
    {
        ALIVE_FATAL("%s", "SDL3 attempt to use palette on non-indexed tex");
    }

    PaletteVariant key;
    key.mPaletteHash = PaletteCache::HashPalette(palette.get());
    // The tint only matters when shading is on
    key.mShading = shading.a == 255 ? shading : RGBA32{0, 0, 0, 0};
    key.mSemiTrans = isSemiTrans;
    key.mBlendMode = blendMode;
    key.mLastUsed = ++mUseCounter;

    // A sprite sheet is often drawn several ways in one frame (palettes, tints, blend modes),
    // so a few converted copies are kept rather than converting it again each time
    for (PaletteVariant& variant : mPaletteVariants)
    {
        if (variant.mPaletteHash == key.mPaletteHash &&
            variant.mShading.ToU32() == key.mShading.ToU32() &&
            variant.mSemiTrans == key.mSemiTrans &&
            variant.mBlendMode == key.mBlendMode)
        {
            variant.mLastUsed = key.mLastUsed;
            return variant.mTexture.get();
        }
    }

    if (mPaletteVariants.size() >= MaxPaletteVariants())
    {
        auto leastRecent = std::min_element(mPaletteVariants.begin(), mPaletteVariants.end(),
            [](const PaletteVariant& a, const PaletteVariant& b) { return a.mLastUsed < b.mLastUsed; });
        mPaletteVariants.erase(leastRecent);
    }

    key.mTexture = MakePaletteVariant(*palette, key.mShading, isSemiTrans, blendMode);
    SDL_Texture* pTexture = key.mTexture.get();
    mPaletteVariants.push_back(std::move(key));
    return pTexture;
}

std::size_t Sdl3Texture::MaxPaletteVariants() const
{
    const std::size_t variantBytes = static_cast<std::size_t>(mWidth) * mHeight * 4;
    return std::max(kMinPaletteVariants, kPaletteVariantBudgetBytes / std::max<std::size_t>(variantBytes, 1));
}

RGBA32 Sdl3Texture::ConvertPaletteColour(RGBA32 colour, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode)
{
    // This logic is basically a mirror of the GLSL shader, but in
    // software - see ShaderPsx.cpp, and the blend mode stuff in
    // OpenGLRenderer.cpp::DrawBatches

    if (colour.ToU32() == 0)
    {
        return {0, 0, 0, 255};
    }

    if (shading.a == 255) // Shading required
    {
        colour.r = HandleShading(colour.r, shading.r);
        colour.g = HandleShading(colour.g, shading.g);
        colour.b = HandleShading(colour.b, shading.b);
    }

    if (isSemiTrans && colour.a == 255)
    {
        switch (blendMode)
        {
            case relive::TBlendModes::eBlend_0: // HALF_DST_ADD_HALF_SRC
                return {static_cast<u8>(colour.r / 2), static_cast<u8>(colour.g / 2), static_cast<u8>(colour.b / 2), 128};

            case relive::TBlendModes::eBlend_1: // ONE_DST_ADD_ONE_SRC
            case relive::TBlendModes::eBlend_2: // ONE_DST_SUB_ONE_SRC
                return {colour.r, colour.g, colour.b, 255};

            case relive::TBlendModes::eBlend_3: // ONE_DST_ADD_QRT_SRC
                return {static_cast<u8>(colour.r / 4), static_cast<u8>(colour.g / 4), static_cast<u8>(colour.b / 4), 255};

            default:
                ALIVE_FATAL("SDL3 Invalid blend mode %u", blendMode);
        }
    }

    return {colour.r, colour.g, colour.b, 0};
}

SdlTexturePtr Sdl3Texture::MakePaletteVariant(const AnimationPal& palette, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode)
{
    SdlTexturePtr texture = mContext.CreateTexture(SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, mWidth, mHeight);
    mContext.CountTextureUpload();

    // Each palette entry is converted once, then the pixels are just looked up
    RGBA32 converted[256];
    for (s32 i = 0; i < 256; i++)
    {
        converted[i] = ConvertPaletteColour(palette.mPal[i], shading, isSemiTrans, blendMode);
    }

    u8* pixelsTarget = nullptr;
    s32 pitchTarget = 0;
    SDL_LockTexture(texture.get(), nullptr, reinterpret_cast<void**>(&pixelsTarget), &pitchTarget);
    for (u32 y = 0; y < mHeight; y++)
    {
        RGBA32* pRow = reinterpret_cast<RGBA32*>(pixelsTarget + y * pitchTarget);
        const u8* pIndices = mIndexedPixels.data() + y * mWidth;
        for (u32 x = 0; x < mWidth; x++)
        {
            pRow[x] = converted[pIndices[x]];
        }
    }
    SDL_UnlockTexture(texture.get());

    Sdl3Context::SetTextureBlendMode(texture.get(),
        blendMode == relive::TBlendModes::eBlend_2
            ? Sdl3Context::PsxTextureSubtractBlendMode()
            : Sdl3Context::PsxTextureBlendMode());

    return texture;
}

void Sdl3Texture::Resize(u32 width, u32 height)
{
    mTexture.reset();

    mWidth = width;
    mHeight = height;

    mTexture = mContext.CreateTexture(mFormat, mTextureAccess, mWidth, mHeight);
    Sdl3Context::SetTextureBlendMode(mTexture.get(), SDL_BLENDMODE_NONE);
}

void Sdl3Texture::SetTextureBlendMode(SDL_BlendMode blendMode)
{
    Sdl3Context::SetTextureBlendMode(mTexture.get(), blendMode);
}

void Sdl3Texture::Update(const SDL_Rect* rect, const void* pixels)
{
    if (mFormat == SDL_PIXELFORMAT_INDEX8)
    {
        memcpy(mIndexedPixels.data(), pixels, mIndexedPixels.size());

        // Made from the old pixels
        mPaletteVariants.clear();
    }
    else
    {
        u32 pitch = 0;

        switch (mFormat)
        {
            case SDL_PIXELFORMAT_RGBA32:
                pitch = mWidth * 4;
                break;

            case SDL_PIXELFORMAT_RGB565:
            {
                pitch = rect ? rect->w * 2 : mWidth * 2;
                break;
            }

            default:
                ALIVE_FATAL("SDL3 - Unsupported texture format %d", mFormat);
                break;
        }

        if (!SDL_UpdateTexture(mTexture.get(), rect, pixels, pitch))
        {
            ALIVE_FATAL("Sdl3Texture::Update: SDL_UpdateTexture failed (format=%d, %ux%u, pixels=%p): %s",
                        mFormat, mWidth, mHeight, pixels, SDL_GetError());
        }
    }
}

u8 Sdl3Texture::HandleShading(const u8 src, const u8 shade)
{
    f32 result = (src * (shade / 255.0f)) / 0.5f;

    if (result > 255.0f)
    {
        return 255;
    }

    return static_cast<u8>(result);
}
