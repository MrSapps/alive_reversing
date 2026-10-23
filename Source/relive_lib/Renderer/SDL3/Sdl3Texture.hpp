#pragma once

#include <SDL3/SDL.h>
#include <unordered_map>
#include <vector>
#include "Sdl3Context.hpp"
#include "data_conversion/rgb_conversion.hpp"
#include "SpriteFx.hpp"
#include "SpriteMask.hpp"

class Sdl3Texture final
{
public:
    // A sprite sheet isn't a texture: it only keeps its pixels (true colour, and its mask if it has one, see SpriteMask.hpp),
    // and the textures are made from them for each way it's drawn
    struct TrueColourSheet final
    {
    };
    Sdl3Texture(Sdl3Context& context, u32 width, u32 height, const u8* rgbaPixels, const u8* maskPixels, TrueColourSheet);
    Sdl3Texture(Sdl3Context& context, u32 width, u32 height, SDL_PixelFormat format, SDL_TextureAccess access);

    static std::shared_ptr<Sdl3Texture> FromMask(Sdl3Context& context, std::shared_ptr<Sdl3Texture> srcTex, const u8* maskPixels, u32 maskWidth, u32 maskHeight);

    u32 GetHeight();
    u32 GetWidth();
    SDL_Texture* GetTexture();
    // The part of a sheet a sprite draws (in pixels of the sheet)
    struct SpriteRect final
    {
        s32 mX = 0;
        s32 mY = 0;
        s32 mW = 0;
        s32 mH = 0;
    };

    // The size of a variant's texture for a rect this many pixels across: the next power of two,
    // so the texture coordinates of a sprite are exact (see Sdl3Renderer::Draw)
    static s32 VariantTextureSize(s32 length)
    {
        s32 size = 1;
        while (size < length)
        {
            size *= 2;
        }
        return size;
    }

    // The part of the sheet a sprite draws, with the effects on it as its own small texture, and
    // the colour to give the vertices: the tint (shading) is done with vertex colours where that
    // comes out the same, so every tint shares one texture. Only the part that is drawn is
    // converted: a sheet is a page with many animations on it.
    SDL_Texture* GetTextureForSprite(const SpriteFx& fx, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode, const SpriteRect& rect, SDL_FColor& vertexColour);
    void Resize(u32 width, u32 height);
    void SetTextureBlendMode(SDL_BlendMode blendMode);
    void Update(const SDL_Rect* rect, const void* pixels);

private:
    // A texel as it is drawn, see the GLSL draw_default_ft4 and handle_final_color
    static RGBA32 ConvertTexel(const RGBA32& texel, u16 mask, const SpriteFx& fx, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode);
    SdlTexturePtr MakeVariant(const SpriteFx& fx, const RGBA32& shading, bool isSemiTrans, relive::TBlendModes blendMode, const SpriteRect& rect);

private:
    Sdl3Context& mContext;
    SDL_PixelFormat mFormat;
    SdlTexturePtr mTexture;
    SDL_TextureAccess mTextureAccess;
    u32 mHeight = 0;
    u32 mWidth = 0;

    // A sprite sheet: its pixels, and the RGBA textures made from them
    bool mIsSheet = false;
    bool mHasEmissive = false;
    std::vector<u8> mSheetPixels;
    // The same size as mSheetPixels, or empty
    std::vector<u8> mMaskPixels;

    struct SheetVariant final
    {
        SpriteFx mFx;
        RGBA32 mShading = {};
        bool mSemiTrans = false;
        relive::TBlendModes mBlendMode = relive::TBlendModes::eBlend_0;
        SpriteRect mRect;
        SdlTexturePtr mTexture;
        std::size_t mBytes = 0;
        u32 mLastUsed = 0;
    };
    static u64 VariantKey(const SheetVariant& variant);
    static bool SameVariant(const SheetVariant& a, const SheetVariant& b);

    // How much memory each sheet's variants may take. When it is full the ones that were not used
    // for the longest are dropped.
    static constexpr std::size_t kVariantBudgetBytes = 16 * 1024 * 1024;
    void TrimVariants();
    // By the hash of what makes them, a few can have the same
    std::unordered_multimap<u64, SheetVariant> mVariants;
    std::size_t mVariantBytes = 0;
    u32 mUseCounter = 0;
};
