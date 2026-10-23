#pragma once

#include <SDL3/SDL.h>
#include "../../Types.hpp"

#include <map>
#include <memory>
#include <tuple>
#include <vector>

class Window;

// Keeps the textures nobody uses any more, see Sdl3Context::CreateTexture
struct SdlTexturePoolState;

struct SdlTextureDeleter final
{
    // The pool the texture goes back to, none if it is destroyed
    std::shared_ptr<SdlTexturePoolState> mPool;

    void operator()(SDL_Texture* pTexture) const;
};

// Owns an SDL_Texture
using SdlTexturePtr = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;

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
    // checks turns on the validation of SDL's GPU, Vulkan and Direct3D 11 backends, where they
    // have it (SDL's OpenGL backends have none)
    Sdl3Context(Window& window, bool checks);
    ~Sdl3Context();

    SDL_Renderer* GetRenderer() const;
    bool IsRenderTargetSupported();

    // Fatal if it fails. A texture that is let go of is kept for the next one of the same size,
    // format and access (creating one is the expensive part, like the PSX's VRAM that was only
    // written to), so it still has the pixels it had and the caller has to write what it samples.
    // Its blend mode and colour are the ones a new texture has.
    SdlTexturePtr CreateTexture(SDL_PixelFormat format, SDL_TextureAccess access, u32 width, u32 height);

    // How many textures are waiting to be reused, and how many CreateTexture calls reused one
    u32 PooledTextureCount() const;
    u32 ReusedTextureCount() const;

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
    static SDL_BlendMode GasMaskBlendMode();
    bool SupportsCustomBlendModes();

    // Whether vertex colours above 1 brighten what's drawn, so a sprite can be tinted by its
    // vertex colours instead of converting its colours for each tint
    bool SupportsBrightVertexColours() const
    {
        return mSupportsBrightVertexColours;
    }

    // A blend mode that doesn't get set draws the wrong picture, so these are fatal if it fails
    void SetDrawBlendMode(SDL_BlendMode blendMode);
    static void SetTextureBlendMode(SDL_Texture* texture, SDL_BlendMode blendMode);

    // Counts frames, for things that are only valid within one
    void NextFrame();
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
    bool ProbeBrightVertexColours();

    std::unique_ptr<SDL_Renderer, SdlRendererDeleter> mRenderer;
    SDL_Rect mLastClipRect;
    SDL_Texture* mLastFramebuffer;
    std::vector<u32> mScratchPixels;
    std::shared_ptr<SdlTexturePoolState> mPool;

    bool mSupportsBrightVertexColours = false;
    u32 mFrameNumber = 0;
    u32 mTextureUploads = 0;
    u32 mDrawCalls = 0;
};
