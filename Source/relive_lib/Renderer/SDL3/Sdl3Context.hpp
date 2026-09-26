#pragma once

#include <SDL3/SDL.h>
#include "../../Types.hpp"

#include <map>
#include <memory>
#include <tuple>
#include <vector>

class Window;

struct SdlTextureDeleter final
{
    void operator()(SDL_Texture* pTexture) const
    {
        SDL_DestroyTexture(pTexture);
    }
};

// Owns an SDL_Texture
using SdlTexturePtr = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;

struct SdlPaletteDeleter final
{
    void operator()(SDL_Palette* pPalette) const
    {
        SDL_DestroyPalette(pPalette);
    }
};

// Owns a reference to an SDL_Palette (textures using it hold their own)
using SdlPalettePtr = std::unique_ptr<SDL_Palette, SdlPaletteDeleter>;

struct SdlRendererDeleter final
{
    void operator()(SDL_Renderer* pRenderer) const
    {
        SDL_DestroyRenderer(pRenderer);
    }
};

class Sdl3Context final
{
public:
    explicit Sdl3Context(Window& window);

    SDL_Renderer* GetRenderer();
    bool IsRenderTargetSupported();

    // Fatal if it fails
    SdlTexturePtr CreateTexture(SDL_PixelFormat format, SDL_TextureAccess access, u32 width, u32 height);

    // What a sprite sheet's colours were converted from: see Sdl3Texture::GetTextureUsePalette
    struct PaletteKey final
    {
        u32 mPaletteHash = 0;
        u32 mShading = 0;
        u32 mSemiTransAndBlendMode = 0;

        bool operator<(const PaletteKey& other) const
        {
            return std::tie(mPaletteHash, mShading, mSemiTransAndBlendMode) < std::tie(other.mPaletteHash, other.mShading, other.mSemiTransAndBlendMode);
        }
    };

    // An SDL palette holding these colours, shared by every texture that uses them: SDL's OpenGL
    // renderer makes a GL texture for each palette
    SDL_Palette* SharedPalette(const PaletteKey& key, const SDL_Color (&colours)[256]);

    // A buffer for converting pixels before they're uploaded, reused so nothing is allocated
    // each time. Only valid until the next call.
    std::vector<u32>& ScratchPixels(std::size_t count)
    {
        if (mScratchPixels.size() < count)
        {
            mScratchPixels.resize(count);
        }
        return mScratchPixels;
    }
    void Present();
    void RestoreFramebuffer();
    void SaveFramebuffer();
    void UseScreenFramebuffer();
    void UseTextureFramebuffer(SDL_Texture* texture);

    // The blend modes that SDL doesn't have built in. Not every SDL renderer supports them
    // (the software one doesn't), and nothing is drawn right without them.
    static SDL_BlendMode SubtractBlendMode();
    static SDL_BlendMode PsxTextureBlendMode();
    static SDL_BlendMode PsxTextureSubtractBlendMode();
    static SDL_BlendMode Fg1MaskBlendMode();
    bool SupportsCustomBlendModes();

    // Whether the SDL renderer can draw 8 bit textures through a palette (SDL 3.4's OpenGL
    // renderers can), so a sprite sheet's colours can be changed without converting its pixels
    bool SupportsPaletteTextures() const
    {
        return mSupportsPaletteTextures;
    }

    // A blend mode that doesn't get set draws the wrong picture, so these are fatal if it fails
    void SetDrawBlendMode(SDL_BlendMode blendMode);
    static void SetTextureBlendMode(SDL_Texture* texture, SDL_BlendMode blendMode);

    // Counts frames, for things that are only valid within one
    void NextFrame()
    {
        mFrameNumber++;
    }
    u32 FrameNumber() const
    {
        return mFrameNumber;
    }

    // Counted for IRenderer::FrameStats
    void CountTextureUpload()
    {
        mTextureUploads++;
    }
    void CountDrawCall()
    {
        mDrawCalls++;
    }
    u32 TakeTextureUploadCount()
    {
        const u32 count = mTextureUploads;
        mTextureUploads = 0;
        return count;
    }
    u32 TakeDrawCallCount()
    {
        const u32 count = mDrawCalls;
        mDrawCalls = 0;
        return count;
    }

private:
    std::unique_ptr<SDL_Renderer, SdlRendererDeleter> mRenderer;
    SDL_Rect mLastClipRect;
    SDL_Texture* mLastFramebuffer;
    std::vector<u32> mScratchPixels;

    struct SharedPaletteEntry final
    {
        SdlPalettePtr mPalette;
        u64 mLastUsed = 0;
    };
    std::map<PaletteKey, SharedPaletteEntry> mSharedPalettes;
    u64 mSharedPaletteUseCounter = 0;
    bool mSupportsPaletteTextures = false;
    u32 mFrameNumber = 0;
    u32 mTextureUploads = 0;
    u32 mDrawCalls = 0;
};
