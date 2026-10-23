#pragma once

#include "../IRenderer.hpp"
#include "../TextureCache.hpp"
#include "../SpriteFxCache.hpp"

#include <GL/glew.h>
#include <memory>
#include <SDL3/SDL_opengl.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include "../../../relive_lib/data_conversion/rgb_conversion.hpp"
#include "../../../relive_lib/Animation.hpp"
#include "../../../relive_lib/ResourceManagerWrapper.hpp"
#include "GLContext.hpp"
#include "GLFramebuffer.hpp"
#include "GLShader.hpp"
#include "GLShaderProgram.hpp"
#include "GLTexture2D.hpp"

#include "Batcher.hpp"

enum class AnimId;

class OpenGLRenderer final : public IRenderer
{
public:
    OpenGLRenderer(Window& window, bool checks);
    ~OpenGLRenderer() override;

    void Clear(u8 r, u8 g, u8 b) override;
    void Draw(const Prim_GasEffect& gasEffect) override;
    void Draw(const Line_G2& line) override;
    void Draw(const Line_G4& line) override;
    void Draw(const Poly_G3& poly) override;
    void Draw(const Poly_FT4& poly) override;
    void Draw(const Prim_ScreenWave& wave) override;
    void Draw(const Poly_G4& poly) override;
    void EndFrame() override;
    void SetClip(const Prim_ScissorRect& clipper) override;
    void StartFrame() override;

    Renderers GetType() const override
    {
        return Renderers::OpenGL;
    }

    u32 MaxTextureSize() const override;

private:
    static constexpr u32 kSpriteTextureUnitCount = 12;

    struct BatchData
    {

    };

public:
    using GLBatcher = Batcher<GLTexture2D, BatchData, kSpriteTextureUnitCount>;

protected:
    void ReadPsxFramebuffer(std::vector<u8>& rgbaPixels, s32& width, s32& height) override;

private:
    // The table of sprite effects: a row of kFxTexelsPerRow RGBA float texels for each, see
    // SetFxRow. Row 0 is no effect.
    static constexpr u32 kAvailableFx = 256;
    static constexpr u32 kFxTexelsPerRow = 16;

    static constexpr u32 kCamTextureLifetime = 300;
    static constexpr u32 kSpriteTextureLifetime = 300;

private:
    struct PassthruVertexData final
    {
        f32 x, y;
        f32 u, v;
    };

    struct Stats final
    {
        u32 mCamUploadCount = 0;
        u32 mFg1UploadCount = 0;
        u32 mAnimUploadCount = 0;
        u32 mFxUploadCount = 0;
        u32 mFontUploadCount = 0;
        u32 mInvalidationsCount = 0;

        void Reset()
        {
            mCamUploadCount = 0;
            mFg1UploadCount = 0;
            mAnimUploadCount = 0;
            mFxUploadCount = 0;
            mFontUploadCount = 0;
            mInvalidationsCount = 0;
        }
    };

private:
    // The row of the table of sprite effects for fx, uploading it if it's new. 0 for no effect.
    u32 PrepareFx(const SpriteFx& fx);
    std::shared_ptr<GLTexture2D> PrepareTextureFromPoly(const Poly_FT4& poly);

    void DrawFramebufferToScreen(s32 x, s32 y, s32 width, s32 height);
    // Draws a rectangle with the bound shader: from (x, y) to (x + width, y + height), with
    // texture coordinates from (0, texHeight) to (texWidth, 0)
    void DrawQuad(f32 x, f32 y, f32 width, f32 height, f32 texWidth, f32 texHeight);
    void SetScissorTest(bool enabled);
    void SetupBlendMode(relive::TBlendModes blendMode);
    void UpdateFilterFramebuffer();

    GLFramebuffer& GetSourcePsxFramebuffer();
    GLFramebuffer& GetDestinationPsxFramebuffer();
    void SwapSrcDstForPsxFramebuffers();

    void DecreaseResourceLifetimes();

    void DebugWindow();

    void DrawBatches();
    void ScaledScissor(s32 x, s32 y, s32 width, s32 height);

private:
    GLContext mContext;

    // The batches' vertices and indices, re-filled each frame
    GLuint mPsxVao = 0;
    GLuint mPsxVbo = 0;
    GLuint mPsxEbo = 0;

    // The rectangles the framebuffers are drawn with
    GLuint mQuadVao = 0;
    GLuint mQuadVbo = 0;

    // Tracked so it never has to be read back from GL
    bool mScissorEnabled = false;

    GLShaderProgram mPassthruShader;
    GLShaderProgram mPassthruFilterShader;
    GLShaderProgram mPsxShader;

    GLFramebuffer mFilterFramebuffer;
    GLFramebuffer mPsxFramebuffer[2];
    u8            mSrcPsxFramebufferIdx = 0;

    Stats mStats;

    bool mFrameStarted = false;

    GLBatcher mBatcher;

    SpriteFxCache mFxCache;
    // Before the caches, so it is there while they let go of their textures
    GLTexturePool mTexturePool;
    TextureCache<std::shared_ptr<GLTexture2D>> mTextureCache;

    std::shared_ptr<GLTexture2D> mFxTexture;
    std::shared_ptr<GLTexture2D> mCurGasTexture;
};
