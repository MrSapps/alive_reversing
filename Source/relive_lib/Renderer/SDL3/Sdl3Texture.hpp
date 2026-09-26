#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include "Sdl3Context.hpp"
#include "data_conversion/rgb_conversion.hpp"

class Sdl3Texture final
{
public:
    Sdl3Texture(Sdl3Context& context, u32 width, u32 height, SDL_PixelFormat format, SDL_TextureAccess access);

    static std::shared_ptr<Sdl3Texture> FromMask(Sdl3Context& context, std::shared_ptr<Sdl3Texture> srcTex, const u8* maskPixels);

    u32 GetHeight();
    u32 GetWidth();
    SDL_Texture* GetTexture();
    // The sheet through the palette, and the colour to give the vertices: the tint (shading) is
    // done with vertex colours where that comes out the same, so every tint shares one texture
    SDL_Texture* GetTextureUsePalette(const std::shared_ptr<AnimationPal>& palette, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode, SDL_FColor& vertexColour);
    void Resize(u32 width, u32 height);
    void SetTextureBlendMode(SDL_BlendMode blendMode);
    void Update(const SDL_Rect* rect, const void* pixels);

private:
    static RGBA32 ConvertPaletteColour(RGBA32 colour, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode);
    SdlTexturePtr MakePaletteVariant(u32 paletteHash, const AnimationPal& palette, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode);

private:
    Sdl3Context& mContext;
    SDL_PixelFormat mFormat;
    SdlTexturePtr mTexture;
    SDL_TextureAccess mTextureAccess;
    u32 mHeight = 0;
    u32 mWidth = 0;

    // Palette tex related: the indexed pixels, and the RGBA textures made from them
    std::vector<u8> mIndexedPixels;

    struct PaletteVariant final
    {
        u32 mPaletteHash = 0;
        RGBA32 mShading = {};
        bool mSemiTrans = false;
        relive::TBlendModes mBlendMode = relive::TBlendModes::eBlend_0;
        SdlTexturePtr mTexture;
        u32 mLastUsed = 0;
    };
    // How much memory each sheet's variants may take: small sheets (particles, fonts) can keep
    // many, large ones (a character's whole sheet) at least a few
    static constexpr std::size_t kPaletteVariantBudgetBytes = 4 * 1024 * 1024;
    static constexpr std::size_t kMinPaletteVariants = 4;
    std::size_t MaxPaletteVariants() const;
    std::vector<PaletteVariant> mPaletteVariants;
    u32 mUseCounter = 0;

    // The last palette hashed, see GetTextureUsePalette
    const AnimationPal* mHashedPalette = nullptr;
    u32 mHashedFrame = 0;
    u32 mHashedPaletteHash = 0;
};
