#pragma once

#include <SDL3/SDL.h>
#include "../../Types.hpp"

class Window;

class Sdl3Context final
{
public:
    explicit Sdl3Context(Window& window);
    ~Sdl3Context();

    SDL_Renderer* GetRenderer();
    bool IsRenderTargetSupported();
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

    // A blend mode that doesn't get set draws the wrong picture, so these are fatal if it fails
    void SetDrawBlendMode(SDL_BlendMode blendMode);
    static void SetTextureBlendMode(SDL_Texture* texture, SDL_BlendMode blendMode);

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
    SDL_Renderer* mRenderer;
    SDL_Rect mLastClipRect;
    SDL_Texture* mLastFramebuffer;
    u32 mTextureUploads = 0;
    u32 mDrawCalls = 0;
};
