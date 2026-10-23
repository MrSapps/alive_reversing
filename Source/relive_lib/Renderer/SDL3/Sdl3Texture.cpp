#include "Sdl3Texture.hpp"
#include <algorithm>
#include "FatalError.hpp"

Sdl3Texture::Sdl3Texture(Sdl3Context& context, u32 width, u32 height, SDL_PixelFormat format, SDL_TextureAccess access)
    : mContext(context), mFormat(format), mTextureAccess(access), mHeight(height), mWidth(width)
{
    mTexture = mContext.CreateTexture(mFormat, mTextureAccess, mWidth, mHeight);
    mContext.CountTextureUpload();

    Sdl3Context::SetTextureBlendMode(mTexture.get(), SDL_BLENDMODE_NONE);
}

Sdl3Texture::Sdl3Texture(Sdl3Context& context, u32 width, u32 height, const u8* rgbaPixels, const u8* maskPixels, TrueColourSheet)
    : mContext(context), mFormat(SDL_PIXELFORMAT_RGBA32), mTextureAccess(SDL_TEXTUREACCESS_STATIC), mHeight(height), mWidth(width)
{
    mIsSheet = true;
    mSheetPixels.assign(rgbaPixels, rgbaPixels + static_cast<std::size_t>(width) * height * 4);

    if (maskPixels)
    {
        mMaskPixels.assign(maskPixels, maskPixels + mSheetPixels.size());

        // Texels that glow aren't tinted, which the vertex colours can't do
        for (std::size_t i = 0; i < mMaskPixels.size(); i += 4)
        {
            if (SpriteMask::Glow(&mMaskPixels[i]) != 0)
            {
                mHasEmissive = true;
                break;
            }
        }
    }
}

std::shared_ptr<Sdl3Texture> Sdl3Texture::FromMask(Sdl3Context& context, std::shared_ptr<Sdl3Texture> srcTex, const u8* maskPixels, u32 maskWidth, u32 maskHeight)
{
    auto resultTex = std::make_shared<Sdl3Texture>(context, srcTex->mWidth, srcTex->mHeight, srcTex->mFormat, SDL_TEXTUREACCESS_TARGET);

    // Create mask texture
    u8* texPixels;
    s32 pitch;
    const SdlTexturePtr maskTex = context.CreateTexture(SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, srcTex->mWidth, srcTex->mHeight);

    SDL_LockTexture(maskTex.get(), NULL, (void**) &texPixels, &pitch);

    // The mask is the size of the original cam, an HD cam texture can be bigger, so each pixel
    // reads the mask pixel at the same place
    for (u32 y = 0; y < srcTex->mHeight; y++)
    {
        const u32 maskY = static_cast<u32>(static_cast<u64>(y) * maskHeight / srcTex->mHeight);
        for (u32 x = 0; x < srcTex->mWidth; x++)
        {
            const u32 maskX = static_cast<u32>(static_cast<u64>(x) * maskWidth / srcTex->mWidth);
            const u8* pMask = &maskPixels[(maskY * maskWidth + maskX) * 4];

            // Mask out black pixels
            texPixels[y * pitch + x * 4 + 3] = (pMask[0] == 0 && pMask[1] == 0 && pMask[2] == 0) ? 0 : 255;
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
    if (mIsSheet)
    {
        ALIVE_FATAL("%s", "SDL3 use GetTextureForSprite with a sprite sheet");
    }

    return mTexture.get();
}

u64 Sdl3Texture::VariantKey(const SheetVariant& variant)
{
    // Not the effect: it is a lot of bytes to hash for every sprite drawn, and almost always the
    // same for the sprites of a rect. Variants that differ only in it are compared in full.
    u64 hash = 1469598103934665603ull;
    auto mix = [&](u64 value)
    {
        hash = (hash ^ value) * 1099511628211ull;
    };
    mix(variant.mShading.ToU32());
    mix(variant.mSemiTrans ? 1 : 0);
    mix(static_cast<u64>(variant.mBlendMode));
    mix(static_cast<u32>(variant.mRect.mX));
    mix(static_cast<u32>(variant.mRect.mY));
    mix(static_cast<u32>(variant.mRect.mW));
    mix(static_cast<u32>(variant.mRect.mH));
    return hash;
}

bool Sdl3Texture::SameVariant(const SheetVariant& a, const SheetVariant& b)
{
    return a.mFx == b.mFx &&
        a.mShading.ToU32() == b.mShading.ToU32() &&
        a.mSemiTrans == b.mSemiTrans &&
        a.mBlendMode == b.mBlendMode &&
        a.mRect.mX == b.mRect.mX && a.mRect.mY == b.mRect.mY && a.mRect.mW == b.mRect.mW && a.mRect.mH == b.mRect.mH;
}

SDL_Texture* Sdl3Texture::GetTextureForSprite(const SpriteFx& fx, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode, const SpriteRect& rect, SDL_FColor& vertexColour)
{
    // A tint makes each colour min(255, colour * tint / 127.5), which is what the texture's colour
    // times a vertex colour of tint / 127.5 comes out as. Half and quarter semi-transparency then
    // scale the colour, and rounding it both before and after the tint makes overlapping sprites
    // (zap lines) visibly darker, so those tints are still converted into the colours. So are
    // the tints of a sheet with glowing texels, as those aren't tinted.
    const bool shaded = shading.a == 255;
    const bool scaledDown = isSemiTrans && (blendMode == relive::TBlendModes::eBlend_0 || blendMode == relive::TBlendModes::eBlend_3);
    const bool tintByVertexColour = shaded && !scaledDown && !mHasEmissive && mContext.SupportsBrightVertexColours();
    vertexColour = {1.0f, 1.0f, 1.0f, 1.0f};
    RGBA32 convertedShading = shading;
    if (tintByVertexColour)
    {
        vertexColour = {shading.r / 127.5f, shading.g / 127.5f, shading.b / 127.5f, 1.0f};
        convertedShading.a = 0;
    }

    if (!mIsSheet)
    {
        ALIVE_FATAL("%s", "SDL3 attempt to use a sprite on a texture that isn't a sheet");
    }

    SheetVariant key;
    key.mFx = fx;
    // The tint only matters when it's converted into the colours
    key.mShading = convertedShading.a == 255 ? convertedShading : RGBA32{0, 0, 0, 0};
    key.mSemiTrans = isSemiTrans;
    key.mBlendMode = blendMode;
    key.mRect = rect;
    key.mLastUsed = ++mUseCounter;

    // A sprite is often drawn several ways in one frame (effects, tints, blend modes), so the
    // converted copies are kept rather than converting it again each time
    const u64 hash = VariantKey(key);
    auto range = mVariants.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it)
    {
        if (SameVariant(it->second, key))
        {
            it->second.mLastUsed = key.mLastUsed;
            return it->second.mTexture.get();
        }
    }

    key.mTexture = MakeVariant(key.mFx, key.mShading, isSemiTrans, blendMode, rect);
    key.mBytes = static_cast<std::size_t>(VariantTextureSize(rect.mW)) * VariantTextureSize(rect.mH) * 4;
    SDL_Texture* pTexture = key.mTexture.get();
    mVariantBytes += key.mBytes;
    mVariants.emplace(hash, std::move(key));
    TrimVariants();
    return pTexture;
}

void Sdl3Texture::TrimVariants()
{
    if (mVariantBytes <= kVariantBudgetBytes)
    {
        return;
    }

    // The least recently used half goes: dropping one at a time would look for them every time
    std::vector<u32> lastUsed;
    lastUsed.reserve(mVariants.size());
    for (const auto& [hash, variant] : mVariants)
    {
        lastUsed.push_back(variant.mLastUsed);
    }
    std::nth_element(lastUsed.begin(), lastUsed.begin() + lastUsed.size() / 2, lastUsed.end());
    const u32 threshold = lastUsed[lastUsed.size() / 2];

    for (auto it = mVariants.begin(); it != mVariants.end();)
    {
        // Not the one that was just made
        if (it->second.mLastUsed < threshold && it->second.mLastUsed != mUseCounter)
        {
            mVariantBytes -= it->second.mBytes;
            it = mVariants.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// Where a brightness comes among the sprite's colours, 0 (darkest) to 1, see the GLSL luma_rank
static f32 LumaRank(const SpriteFx& fx, f32 luma)
{
    u32 k = 0;
    while (k < SpriteFx::kLumaPoints - 2 && luma > fx.mLuma[k + 1])
    {
        k++;
    }

    const f32 lo = fx.mLuma[k];
    const f32 hi = fx.mLuma[k + 1];
    const f32 f = hi > lo ? std::min(1.0f, std::max(0.0f, (luma - lo) / (hi - lo))) : (luma > hi ? 1.0f : 0.0f);
    return (k + f) / (SpriteFx::kLumaPoints - 1);
}

RGBA32 Sdl3Texture::ConvertTexel(const RGBA32& texel, u32 mask, const SpriteFx& fx, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode)
{
    // This logic is basically a mirror of the GLSL shader, but in
    // software - see ShaderPsx.cpp, and the blend mode stuff in
    // OpenGLRenderer.cpp::DrawBatches. Like the shader it's worked out in floating point and
    // rounded once at the end: rounding at each step makes semi-transparent sprites a little
    // darker, which adds up where many overlap (zap lines).

    if (texel.a == 0)
    {
        return {0, 0, 0, 255};
    }

    // What the sheet's mask says about the texel, see SpriteMask.hpp
    const f32 glow = SpriteMask::PackedGlow(mask) / 255.0f;
    const bool keepVisible = SpriteMask::PackedKeepVisible(mask);
    // Alpha 255 is solid, anything less blends in a semi-transparent sprite
    bool stp = texel.a < 255;
    f32 a = texel.a / 255.0f;

    f32 rgb[3] = {texel.r / 255.0f, texel.g / 255.0f, texel.b / 255.0f};

    if (!fx.IsIdentity())
    {
        // How bright the colour is among the sprite's, before the effects change it
        // (the palette order if the mask has it, see SpriteMask.hpp)
        const u8 paletteRank = SpriteMask::PackedPaletteRank(mask);
        const f32 rank = paletteRank != 0 ? paletteRank / 255.0f : LumaRank(fx, rgb[0] * 0.299f + rgb[1] * 0.587f + rgb[2] * 0.114f);
        const f32 in[4] = {rgb[0], rgb[1], rgb[2], 1.0f};
        for (s32 ch = 0; ch < 3; ch++)
        {
            f32 v = 0.0f;
            for (s32 c = 0; c < 4; c++)
            {
                v += fx.mMatrix[ch][c] * in[c];
            }
            rgb[ch] = std::min(1.0f, std::max(0.0f, v - 0.0f));
        }

        // The invisibility leaves some of the colours alone
        if (!keepVisible)
        {
            for (s32 ch = 0; ch < 3; ch++)
            {
                rgb[ch] = std::min(1.0f, std::max(0.0f, rgb[ch] - fx.mSubtract));
            }

            if (fx.mForceBlend)
            {
                stp = true;
            }
        }

        for (const SpriteFx::Sweep& sweep : fx.mSweeps)
        {
            if (a > 0.0f && sweep.mProgress > 0.0f && rank <= sweep.mProgress + 0.003f)
            {
                if (sweep.mKind == 2)
                {
                    // The transparent colour: erased
                    a = 0.0f;
                }
                else
                {
                    rgb[0] = sweep.mR;
                    rgb[1] = sweep.mG;
                    rgb[2] = sweep.mB;
                    a = 1.0f;
                    stp = sweep.mKind == 1;
                }
            }
        }

        if (a == 0.0f)
        {
            return {0, 0, 0, 255};
        }
    }

    for (f32& channel : rgb)
    {
        channel *= 255.0f;
    }

    if (shading.a == 255 && glow < 1.0f) // Shading required, less where the texel glows
    {
        const u8 shade[3] = {shading.r, shading.g, shading.b};
        for (s32 i = 0; i < 3; i++)
        {
            const f32 shaded = std::min(255.0f, (rgb[i] * (shade[i] / 255.0f)) / 0.5f);
            rgb[i] += (shaded - rgb[i]) * (1.0f - glow);
        }
    }

    // The colour is what's added, the alpha how much of what's already there stays
    f32 scale = a;
    f32 dstFactor = 1.0f - a;
    if (isSemiTrans && stp)
    {
        switch (blendMode)
        {
            case relive::TBlendModes::eBlend_0: // HALF_DST_ADD_HALF_SRC
                scale = 0.5f * a;
                dstFactor = 1.0f - 0.5f * a;
                break;

            case relive::TBlendModes::eBlend_1: // ONE_DST_ADD_ONE_SRC
                scale = a;
                dstFactor = 1.0f;
                break;

            case relive::TBlendModes::eBlend_2: // ONE_DST_SUB_ONE_SRC: taken away by the alpha
                scale = 1.0f;
                dstFactor = a;
                break;

            case relive::TBlendModes::eBlend_3: // ONE_DST_ADD_QRT_SRC
                scale = 0.25f * a;
                dstFactor = 1.0f;
                break;

            default:
                ALIVE_FATAL("SDL3 Invalid blend mode %u", static_cast<u32>(blendMode));
        }
    }
    else if (blendMode == relive::TBlendModes::eBlend_2)
    {
        // Not blended, in a texture that subtracts: leave it
        scale = 1.0f;
        dstFactor = 0.0f;
    }

    return {
        static_cast<u8>(rgb[0] * scale + 0.5f),
        static_cast<u8>(rgb[1] * scale + 0.5f),
        static_cast<u8>(rgb[2] * scale + 0.5f),
        static_cast<u8>(dstFactor * 255.0f + 0.5f)};
}

SdlTexturePtr Sdl3Texture::MakeVariant(const SpriteFx& fx, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode, const SpriteRect& rect)
{
    SdlTexturePtr texture = mContext.CreateTexture(SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, VariantTextureSize(rect.mW), VariantTextureSize(rect.mH));

    const std::size_t pixelCount = static_cast<std::size_t>(rect.mW) * rect.mH;
    RGBA32* pPixels = reinterpret_cast<RGBA32*>(mContext.ScratchPixels(pixelCount).data());
    const RGBA32* pSource = reinterpret_cast<const RGBA32*>(mSheetPixels.data());

    // Sprites are made of a few colours, so each colour is only converted once: the last results
    // are kept by colour and mask in a small table
    struct Memo final
    {
        u32 mSource = 0;
        u32 mFlags = 0;
        RGBA32 mResult = {};
        bool mValid = false;
    };
    constexpr u32 kMemoSize = 1024;
    std::vector<Memo> memo(kMemoSize);
    const bool hasMask = !mMaskPixels.empty();
    const RGBA32 transparent = ConvertTexel({0, 0, 0, 0}, 0, fx, shading, isSemiTrans, blendMode);
    for (s32 y = 0; y < rect.mH; y++)
    {
        const std::size_t sheetRow = static_cast<std::size_t>(rect.mY + y) * mWidth + rect.mX;
        for (s32 x = 0; x < rect.mW; x++)
        {
            const std::size_t i = sheetRow + x;
            RGBA32& out = pPixels[static_cast<std::size_t>(y) * rect.mW + x];

            // Most of a sheet is nothing
            if (pSource[i].a == 0)
            {
                out = transparent;
                continue;
            }

            const u32 source = pSource[i].ToU32();
            const u32 flags = hasMask ? SpriteMask::Pack(&mMaskPixels[i * 4]) : 0;
            Memo& entry = memo[((source ^ (flags * 0x9E3779B1u)) * 2654435761u) >> 22];
            if (!entry.mValid || entry.mSource != source || entry.mFlags != flags)
            {
                entry.mSource = source;
                entry.mFlags = flags;
                entry.mResult = ConvertTexel(pSource[i], flags, fx, shading, isSemiTrans, blendMode);
                entry.mValid = true;
            }
            out = entry.mResult;
        }
    }

    // Only the rect is filled in, the rest of the texture is never drawn
    const SDL_Rect filled = {0, 0, rect.mW, rect.mH};
    if (!SDL_UpdateTexture(texture.get(), &filled, pPixels, rect.mW * 4))
    {
        ALIVE_FATAL("SDL_UpdateTexture failed: %s", SDL_GetError());
    }
    mContext.CountTextureUpload();

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
