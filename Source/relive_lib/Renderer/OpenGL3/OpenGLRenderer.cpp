#include "../../Window.hpp"
#include <algorithm>

#include "../../../relive_lib/data_conversion/rgb_conversion.hpp"
#include "../../../relive_lib/Primitives.hpp"
#include "../../../relive_lib/Animation.hpp"
#include "../../Compression.hpp"
#include "../../../relive_lib/Font.hpp"
#include "../../../relive_lib/BaseGameAutoPlayer.hpp"
#include "../../../relive_lib/FatalError.hpp"
#include "GLDebug.hpp"
#include "GLFramebuffer.hpp"
#include "GLShader.hpp"
#include "GLShaderProgram.hpp"
#include "GLTexture2D.hpp"
#include "OpenGLRenderer.hpp"
#include <cmath>

#define GL_TO_IMGUI_TEX(v) *reinterpret_cast<ImTextureID*>(&v)

extern bool gDDCheat_FlyingEnabled;
namespace AO
{
    extern bool gDDCheat_FlyingEnabled;
}

static bool gRenderEnable_Batching = true;

static bool gRenderEnable_GAS = true;
static bool gRenderEnable_FT4 = true;
static bool gRenderEnable_G4 = true;
static bool gRenderEnable_G3 = true;
static bool gRenderEnable_G2 = true;

OpenGLRenderer::OpenGLRenderer(Window& window, bool checks)
    : IRenderer(window),
    mContext(window, checks),
    mFilterFramebuffer(kTargetFramebufferWidth, kTargetFramebufferHeight),
    mPsxFramebuffer{
        GLFramebuffer(kPsxFramebufferWidth, kPsxFramebufferHeight),
        GLFramebuffer(kPsxFramebufferWidth, kPsxFramebufferHeight)
    },
    mPaletteCache(kAvailablePalettes)
{
    // The batches' vertex layout. The element buffer binding is part of the VAO too.
    GL_VERIFY(glGenVertexArrays(1, &mPsxVao));
    GL_VERIFY(glGenBuffers(1, &mPsxVbo));
    GL_VERIFY(glGenBuffers(1, &mPsxEbo));
    GL_VERIFY(glBindVertexArray(mPsxVao));
    GL_VERIFY(glBindBuffer(GL_ARRAY_BUFFER, mPsxVbo));
    GL_VERIFY(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mPsxEbo));
    GL_VERIFY(glEnableVertexAttribArray(0));
    GL_VERIFY(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PsxVertexData), (void*) offsetof(PsxVertexData, x)));
    GL_VERIFY(glEnableVertexAttribArray(1));
    GL_VERIFY(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PsxVertexData), (void*) offsetof(PsxVertexData, r)));
    GL_VERIFY(glEnableVertexAttribArray(2));
    GL_VERIFY(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PsxVertexData), (void*) offsetof(PsxVertexData, u)));
    GL_VERIFY(glEnableVertexAttribArray(3));
    GL_VERIFY(glVertexAttribIPointer(3, 4, GL_UNSIGNED_INT, sizeof(PsxVertexData), (void*) offsetof(PsxVertexData, drawMode)));
    GL_VERIFY(glEnableVertexAttribArray(4));
    GL_VERIFY(glVertexAttribIPointer(4, 2, GL_UNSIGNED_INT, sizeof(PsxVertexData), (void*) offsetof(PsxVertexData, paletteIndex)));

    // The framebuffer quads' vertex layout
    GL_VERIFY(glGenVertexArrays(1, &mQuadVao));
    GL_VERIFY(glGenBuffers(1, &mQuadVbo));
    GL_VERIFY(glBindVertexArray(mQuadVao));
    GL_VERIFY(glBindBuffer(GL_ARRAY_BUFFER, mQuadVbo));
    GL_VERIFY(glEnableVertexAttribArray(0));
    GL_VERIFY(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(PassthruVertexData), (void*) offsetof(PassthruVertexData, x)));
    GL_VERIFY(glEnableVertexAttribArray(1));
    GL_VERIFY(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PassthruVertexData), (void*) offsetof(PassthruVertexData, u)));

    // Enable blending
    GL_VERIFY(glEnable(GL_BLEND));

    // Preload the cache texture with all black
    const static RGBA32 black[256] = {};

    mPaletteTexture = std::make_shared<GLTexture2D>(kPaletteDepth, kAvailablePalettes, GL_RGBA);

    for (u32 i = 0; i < kAvailablePalettes; i++)
    {
        mPaletteTexture->LoadSubImage(0, i, kPaletteDepth, 1, black);
    }

    // Load shaders
    GLShader passthruVS(gShader_PassthruVSH, GL_VERTEX_SHADER);
    GLShader passthruFS(gShader_PassthruFSH, GL_FRAGMENT_SHADER);
    GLShader passthruFilterFS(gShader_PassthruFilterFSH, GL_FRAGMENT_SHADER);
    GLShader psxVS(gShader_PsxVSH, GL_VERTEX_SHADER);
    GLShader psxFS(gShader_PsxFSH, GL_FRAGMENT_SHADER);

    mPassthruShader.LinkShaders(passthruVS, passthruFS);
    mPassthruFilterShader.LinkShaders(passthruVS, passthruFilterFS);
    mPsxShader.LinkShaders(psxVS, psxFS);

    // Uniforms that never change. The sprite sheets use the units from GL_TEXTURE7 on.
    GLint spriteTextureUnits[kSpriteTextureUnitCount];
    for (u32 i = 0; i < kSpriteTextureUnitCount; i++)
    {
        spriteTextureUnits[i] = i + 7;
    }

    mPsxShader.Use();
    mPsxShader.UniformVec2("vsViewportSize", kPsxFramebufferWidth, kPsxFramebufferHeight);
    mPsxShader.Uniform1i("texPalette", 0);
    mPsxShader.Uniform1i("texGas", 1);
    mPsxShader.Uniform1i("texCamera", 2);
    mPsxShader.Uniform1i("texFramebuffer", 7);
    mPsxShader.Uniform1iv("texSpriteSheets", kSpriteTextureUnitCount, spriteTextureUnits);

    mPassthruShader.Use();
    mPassthruShader.Uniform1i("texTextureData", 0);
    mPassthruShader.Uniform1i("fsFlipUV", false);

    mPassthruFilterShader.Use();
    mPassthruFilterShader.Uniform1i("texTextureData", 0);
    mPassthruFilterShader.UniformVec2("vsViewportSize", kTargetFramebufferWidth, kTargetFramebufferHeight);
    mPassthruFilterShader.UniformVec2("fsTexSize", kPsxFramebufferWidth, kPsxFramebufferHeight);
}

OpenGLRenderer::~OpenGLRenderer()
{
    mTextureCache.Clear();

    GL_VERIFY(glUseProgram(0));

    GL_VERIFY(glBindVertexArray(0));
    GL_VERIFY(glDeleteVertexArrays(1, &mPsxVao));
    GL_VERIFY(glDeleteVertexArrays(1, &mQuadVao));
    GL_VERIFY(glDeleteBuffers(1, &mPsxVbo));
    GL_VERIFY(glDeleteBuffers(1, &mPsxEbo));
    GL_VERIFY(glDeleteBuffers(1, &mQuadVbo));

    GLFramebuffer::BindScreenAsTarget(mWindow);
}

void OpenGLRenderer::Clear(u8 r, u8 g, u8 b)
{
    if (!mFrameStarted || mWindow.IsMinimized())
    {
        return;
    }

    const bool scissoring = mScissorEnabled;

    // We clear the screen framebuffer here
    GLFramebuffer::BindScreenAsTarget(mWindow);

    SetScissorTest(false);

    GL_VERIFY(glClearColor(static_cast<f32>(r), static_cast<f32>(g), static_cast<f32>(b), 1.0f));
    GL_VERIFY(glClear(GL_COLOR_BUFFER_BIT));

    // Set back to the dest PSX framebuffer
    GetDestinationPsxFramebuffer().BindAsTarget();

    SetScissorTest(scissoring);
}

void OpenGLRenderer::StartFrame()
{
    IRenderer::StartFrame();

    // Always reset stats and batcher states so we do not build up memory
    // if the window is minimized
    mStats.Reset();
    mBatcher.StartFrame();

    if (mWindow.IsMinimized())
    {
        return;
    }

    mFrameStarted = true;

    // Set offsets for the screen (this is for the screen shake effect)
    mOffsetX = 0;
    mOffsetY = 0;

    // Check if we need to recreate the framebuffers if the viewport has
    // changed size
    SDL_Rect desiredFbSize = GetFramebufferRect();

    if (
        mPsxFramebuffer[0].GetWidth() != desiredFbSize.w ||
        mPsxFramebuffer[0].GetHeight() != desiredFbSize.h
    )
    {
        mPsxFramebuffer[0].Resize(desiredFbSize.w, desiredFbSize.h);
        mPsxFramebuffer[1].Resize(desiredFbSize.w, desiredFbSize.h);
    }

    // Ensure bound to destination framebuffer
    GetDestinationPsxFramebuffer().BindAsTarget();
}

void OpenGLRenderer::EndFrame()
{
    DrawFrameStats();
    mBatcher.EndFrame();

    DrawBatches();

    if (mFrameStarted && !mWindow.IsMinimized())
    {
        CaptureIfRequested();
    }

    mLastFrameStats.mDrawCalls = mStats.mInvalidationsCount;
    mLastFrameStats.mTextureUploads = mStats.mCamUploadCount + mStats.mFg1UploadCount + mStats.mAnimUploadCount + mStats.mFontUploadCount;

    // Always decrease resource lifetimes regardless of drawing to prevent
    // memory leaks
    DecreaseResourceLifetimes();

    mLastFrameStats.mCachedTextures = mTextureCache.Size();

    // The rest of this method writes to the screen, we early return now
    // because:
    //     Sometimes EndFrame is called before StartFrame
    //     When minimised, rendering to the screen blows up Intel HD 2000
    if (!mFrameStarted || mWindow.IsMinimized())
    {
        return;
    }

    // Draw the final composed framebuffer to the screen
    SDL_Rect drawRect = GetTargetDrawRect();

    SetScissorTest(false);
    DrawFramebufferToScreen(
        drawRect.x,
        drawRect.y,
        drawRect.w,
        drawRect.h);

    // Do ImGui, only when there's something to show
    if (gDDCheat_FlyingEnabled || AO::gDDCheat_FlyingEnabled || GetGameAutoPlayer().IsPlaying())
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        DebugWindow();

        ImGui::Render();
        ImGui::EndFrame();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Throw away any errors caused by ImGui - this is necessary for AMD GPUs
        // (AMD Radeon HD 7310 with driver 8.982.10.5000)
        glGetError();
    }

    // Render end
    mContext.SwapBuffers();

    mFrameStarted = false;

    // Set the framebuffer target back to the destination PSX framebuffer
    GetDestinationPsxFramebuffer().BindAsTarget();
}

void OpenGLRenderer::ReadPsxFramebuffer(std::vector<u8>& rgbaPixels, s32& width, s32& height)
{
    GLFramebuffer& framebuffer = GetDestinationPsxFramebuffer();
    framebuffer.ReadPixels(rgbaPixels);
    width = framebuffer.GetWidth();
    height = framebuffer.GetHeight();
}

void OpenGLRenderer::SetClip(const Prim_ScissorRect& clipper)
{
    SDL_Rect rect = {};
    if (!IRenderer::IsScissorDisabled(clipper))
    {
        rect = {clipper.mRect.x, clipper.mRect.y, clipper.mRect.w, clipper.mRect.h};
    }

    mBatcher.SetScissor(rect);
}

void OpenGLRenderer::Draw(const Prim_GasEffect& gasEffect)
{
    if (!gRenderEnable_GAS)
    {
        return;
    }

    if (!mCurGasTexture || !mCurGasTexture->IsValid())
    {
        mCurGasTexture = std::make_shared<GLTexture2D>(kPsxFramebufferWidth, kPsxFramebufferHeight, GL_RGB);
    }

    const f32 gasWidth = std::floor(static_cast<f32>(gasEffect.w - gasEffect.x) / 4);
    const f32 gasHeight = std::floor(static_cast<f32>(gasEffect.h - gasEffect.y) / 2);
    mCurGasTexture->LoadSubImage(0, 0, static_cast<GLsizei>(gasWidth), static_cast<GLsizei>(gasHeight), gasEffect.pGasPixels, GL_UNSIGNED_SHORT_5_6_5);

    // TODO: If there is more than 1 gas in a frame break the batch ?
    mBatcher.PushGas(gasEffect);
}

void OpenGLRenderer::Draw(const Line_G2& line)
{
    if (!gRenderEnable_G2)
    {
        return;
    }

    mBatcher.PushLine(line, line.mBlendMode);
}

void OpenGLRenderer::Draw(const Line_G4& line)
{
    if (!gRenderEnable_G4)
    {
        return;
    }

    mBatcher.PushLine(line, line.mBlendMode);
}

void OpenGLRenderer::Draw(const Poly_G3& poly)
{
    if (!gRenderEnable_G3)
    {
        return;
    }

    mBatcher.PushPolyG3(poly, poly.mBlendMode);
}

void OpenGLRenderer::Draw(const Poly_FT4& poly)
{
    if (!gRenderEnable_FT4)
    {
        return;
    }

    std::shared_ptr<GLTexture2D> texture = PrepareTextureFromPoly(poly);

    if (poly.mFg1)
    {
        mBatcher.PushFG1(poly, texture);
    }
    else if (poly.mCam)
    {
        mBatcher.PushCAM(poly, texture);
    }
    else if (poly.mAnim)
    {
        const u32 palIndex = PreparePalette(*poly.mAnim->mAnimRes.mCurPal);
        mBatcher.PushAnim(poly, palIndex, texture);
    }
    else if (poly.mFont)
    {
        FontResource& fontRes = poly.mFont->mFntResource;

        auto pPal = fontRes.mCurPal;
        const u32 palIndex = PreparePalette(*pPal);

        mBatcher.PushFont(poly, palIndex, texture);
    }
}

void OpenGLRenderer::Draw(const Prim_ScreenWave& wave)
{
    // The source corners go in as the texture coordinates, in screen pixels
    PsxVertexData verts[4] = {};
    for (s32 i = 0; i < 4; i++)
    {
        verts[i] = {
            static_cast<f32>(wave.mVerts[i].x), static_cast<f32>(wave.mVerts[i].y),
            127.0f, 127.0f, 127.0f,
            static_cast<f32>(wave.mSource[i].x), static_cast<f32>(wave.mSource[i].y),
            PsxDrawMode::ScreenWave, 0, 0, relive::TBlendModes::eBlend_0, 0, 0};
    }

    mBatcher.PushFramebufferVertexData(verts, ALIVE_COUNTOF(verts));
}

void OpenGLRenderer::Draw(const Poly_G4& poly)
{
    if (!gRenderEnable_G4)
    {
        return;
    }

    mBatcher.PushPolyG4(poly, poly.mBlendMode);
}

u32 OpenGLRenderer::PreparePalette(AnimationPal& pCache)
{
    const PaletteCache::AddResult addRet = mPaletteCache.Add(pCache);

    if (addRet.mAllocated)
    {
        // Write palette data
        mPaletteTexture->LoadSubImage(0, addRet.mIndex, kPaletteDepth, 1, pCache.mPal);

        mStats.mPalUploadCount++;
    }

    return addRet.mIndex;
}

std::shared_ptr<GLTexture2D> OpenGLRenderer::PrepareTextureFromPoly(const Poly_FT4& poly)
{
    // Makes a texture of the given format holding pixels, counted in uploadCount
    auto upload = [](u32 width, u32 height, GLenum format, const void* pixels, u32& uploadCount)
    {
        auto texture = std::make_shared<GLTexture2D>(width, height, format);
        texture->LoadImage(pixels);
        uploadCount++;
        return texture;
    };

    if (poly.mFg1)
    {
        const auto& image = poly.mFg1->mImage;
        return mTextureCache.GetOrAdd(poly.mFg1->mUniqueId.Id(), kCamTextureLifetime, [&]()
            { return upload(image.mWidth, image.mHeight, GL_RGBA, image.mPixels->data(), mStats.mFg1UploadCount); });
    }
    else if (poly.mCam)
    {
        const auto& data = poly.mCam->mData;
        return mTextureCache.GetOrAdd(poly.mCam->mUniqueId.Id(), kCamTextureLifetime, [&]()
            { return upload(data.mWidth, data.mHeight, GL_RGBA, data.mPixels->data(), mStats.mCamUploadCount); });
    }
    else if (poly.mAnim)
    {
        const AnimResource& res = poly.mAnim->mAnimRes;
        return mTextureCache.GetOrAdd(res.mUniqueId.Id(), kSpriteTextureLifetime, [&]()
            { return upload(res.mPngPtr->mWidth, res.mPngPtr->mHeight, GL_RED, res.mPngPtr->mPixels.data(), mStats.mAnimUploadCount); });
    }
    else if (poly.mFont)
    {
        const FontResource& res = poly.mFont->mFntResource;
        return mTextureCache.GetOrAdd(res.mUniqueId.Id(), kSpriteTextureLifetime, [&]()
            { return upload(res.mPngPtr->mWidth, res.mPngPtr->mHeight, GL_RED, res.mPngPtr->mPixels.data(), mStats.mFontUploadCount); });
    }

    return nullptr;
}

void OpenGLRenderer::DrawFramebufferToScreen(s32 x, s32 y, s32 width, s32 height)
{
    // Ensure blend mode is back to normal alpha compositing
    GL_VERIFY(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_VERIFY(glBlendEquation(GL_FUNC_ADD));

    // Set up the texture we're going to draw...
    f32 texWidth = 0;
    f32 texHeight = 0;

    // Handle filtering (do not filter if not using original game res, it looks
    // terrible)
    if (mFramebufferFilter && mUseOriginalResolution)
    {
        UpdateFilterFramebuffer();

        mFilterFramebuffer.BindAsSourceTextureTo(GL_TEXTURE0);

        texWidth = static_cast<f32>(mFilterFramebuffer.GetWidth());
        texHeight = static_cast<f32>(mFilterFramebuffer.GetHeight());
    }
    else
    {
        GetDestinationPsxFramebuffer().BindAsSourceTextureTo(GL_TEXTURE0);
        texWidth = static_cast<f32>(GetDestinationPsxFramebuffer().GetWidth());
        texHeight = static_cast<f32>(GetDestinationPsxFramebuffer().GetHeight());
    }

    s32 viewportW, viewportH;

    GLFramebuffer::BindScreenAsTarget(mWindow, &viewportW, &viewportH);

    mPassthruShader.Use();
    mPassthruShader.UniformVec2("vsViewportSize", static_cast<f32>(viewportW), static_cast<f32>(viewportH));
    mPassthruShader.UniformVec2("fsTexSize", texWidth, texHeight);

    DrawQuad(static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(width), static_cast<f32>(height), texWidth, texHeight);
}

void OpenGLRenderer::DrawQuad(f32 x, f32 y, f32 width, f32 height, f32 texWidth, f32 texHeight)
{
    const PassthruVertexData vertices[] = {
        {x, y, 0.0f, texHeight},
        {x, y + height, 0.0f, 0.0f},
        {x + width, y, texWidth, texHeight},

        {x + width, y, texWidth, texHeight},
        {x, y + height, 0.0f, 0.0f},
        {x + width, y + height, texWidth, 0.0f}};

    GL_VERIFY(glBindVertexArray(mQuadVao));
    GL_VERIFY(glBindBuffer(GL_ARRAY_BUFFER, mQuadVbo));
    GL_VERIFY(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW));
    GL_VERIFY(glDrawArrays(GL_TRIANGLES, 0, 6));
}

void OpenGLRenderer::SetScissorTest(bool enabled)
{
    if (enabled != mScissorEnabled)
    {
        if (enabled)
        {
            GL_VERIFY(glEnable(GL_SCISSOR_TEST));
        }
        else
        {
            GL_VERIFY(glDisable(GL_SCISSOR_TEST));
        }
        mScissorEnabled = enabled;
    }
}

void OpenGLRenderer::SetupBlendMode(relive::TBlendModes blendMode)
{
    if (blendMode == relive::TBlendModes::eBlend_2)
    {
        GL_VERIFY(glBlendFunc(GL_SRC_ALPHA, GL_ONE));
        GL_VERIFY(glBlendEquation(GL_FUNC_REVERSE_SUBTRACT));
    }
    else
    {
        GL_VERIFY(glBlendFunc(GL_ONE, GL_SRC_ALPHA));
        GL_VERIFY(glBlendEquation(GL_FUNC_ADD));
    }
}

void OpenGLRenderer::UpdateFilterFramebuffer()
{
    mPassthruFilterShader.Use();

    mFilterFramebuffer.BindAsTarget();
    GetDestinationPsxFramebuffer().BindAsSourceTextureTo(GL_TEXTURE0);

    DrawQuad(0.0f, 0.0f, kTargetFramebufferWidth, kTargetFramebufferHeight, kPsxFramebufferWidth, kPsxFramebufferHeight);
}

GLFramebuffer& OpenGLRenderer::GetSourcePsxFramebuffer()
{
    return mPsxFramebuffer[mSrcPsxFramebufferIdx];
}

GLFramebuffer& OpenGLRenderer::GetDestinationPsxFramebuffer()
{
    return mSrcPsxFramebufferIdx == 0 ? mPsxFramebuffer[1] : mPsxFramebuffer[0];
}

void OpenGLRenderer::SwapSrcDstForPsxFramebuffers()
{
    mSrcPsxFramebufferIdx = mSrcPsxFramebufferIdx == 0 ? 1 : 0;
}

void OpenGLRenderer::DecreaseResourceLifetimes()
{
    mTextureCache.DecreaseResourceLifetimes();

    mPaletteCache.ResetUseFlags();
}

void OpenGLRenderer::DebugWindow()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Developer"))
        {
            if (ImGui::BeginMenu("Renderer Debug"))
            {
                ImGui::MenuItem("Batching Enabled", nullptr, &gRenderEnable_Batching);
                mBatcher.SetBatching(gRenderEnable_Batching);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Render Elements"))
            {
                ImGui::MenuItem("GAS", nullptr, &gRenderEnable_GAS);
                ImGui::MenuItem("FT4", nullptr, &gRenderEnable_FT4);
                ImGui::MenuItem("G4", nullptr, &gRenderEnable_G4);
                ImGui::MenuItem("G3", nullptr, &gRenderEnable_G3);
                ImGui::MenuItem("G2", nullptr, &gRenderEnable_G2);

                ImGui::MenuItem("gl_debug", nullptr, &GLDebug::Checks());

                ImGui::MenuItem("filter", nullptr, &mFramebufferFilter);

                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (ImGui::Begin("Render stats"))
    {
        ImGui::Text("Cams %d", mStats.mCamUploadCount);
        ImGui::Text("Fg1s %d", mStats.mFg1UploadCount);
        ImGui::Text("Anims %d", mStats.mAnimUploadCount);
        ImGui::Text("Pals %d", mStats.mPalUploadCount);
        ImGui::Text("Fonts %d", mStats.mFontUploadCount);
        ImGui::Text("Invalidations %d", mStats.mInvalidationsCount);
    }
    ImGui::End();
}

void OpenGLRenderer::DrawBatches()
{
    if (!mFrameStarted || mBatcher.mBatches.size() == 0)
    {
        return;
    }

    mPsxShader.Use();

    // Re-filling the buffers with glBufferData lets the driver give them new storage, so this
    // doesn't wait for last frame's draws
    GL_VERIFY(glBindVertexArray(mPsxVao));
    GL_VERIFY(glBindBuffer(GL_ARRAY_BUFFER, mPsxVbo));
    GL_VERIFY(glBufferData(GL_ARRAY_BUFFER, sizeof(PsxVertexData) * mBatcher.mVertices.size(), mBatcher.mVertices.data(), GL_STREAM_DRAW));
    GL_VERIFY(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * mBatcher.mIndices.size(), mBatcher.mIndices.data(), GL_STREAM_DRAW));

    // Bind palette texture
    mPaletteTexture->BindTo(GL_TEXTURE0);

    // Bind camera (if needed)
    if (mBatcher.mCamTexture && mBatcher.mCamTexture->IsValid())
    {
        mBatcher.mCamTexture->BindTo(GL_TEXTURE2);
    }

    // Bind gas
    if (mCurGasTexture && mCurGasTexture->IsValid())
    {
        mCurGasTexture->BindTo(GL_TEXTURE1);
    }

    u32 idxOffset = 0;
    u32 baseTextureIdx = 0;
    s32 drawingFramebuffer = -1;
    for (const GLBatcher::RenderBatch& batch : mBatcher.mBatches)
    {
        if (batch.mScissor.x == 0 && batch.mScissor.y == 0 && batch.mScissor.w == 0 && batch.mScissor.h == 0)
        {
            SetScissorTest(false);
        }
        else
        {
            SetScissorTest(true);
            ScaledScissor(batch.mScissor.x, batch.mScissor.y, batch.mScissor.w, batch.mScissor.h);
        }

        if (batch.mSourceIsFramebuffer)
        {
            SwapSrcDstForPsxFramebuffers();

            // We use GL_TEXTURE7 because it will always be overwritten
            // by the next batch, so it's safe to use
            GetSourcePsxFramebuffer().BindAsSourceTextureTo(GL_TEXTURE7);
            GetDestinationPsxFramebuffer().BindAsTarget();

            SetupBlendMode(relive::TBlendModes::eBlend_0); // Ensure we're using additive blend mode
        }
        else
        {
            // Bind sprite sheets
            for (u32 i = 0; i < batch.mTexturesInBatch; i++)
            {
                mBatcher.mBatchTextures[baseTextureIdx + i]->BindTo(GL_TEXTURE7 + i);
            }

            // Assign blend mode
            if (batch.mBlendMode != relive::TBlendModes::None)
            {
                SetupBlendMode(batch.mBlendMode);
            }
        }

        if (drawingFramebuffer != static_cast<s32>(batch.mSourceIsFramebuffer))
        {
            drawingFramebuffer = static_cast<s32>(batch.mSourceIsFramebuffer);
            mPsxShader.Uniform1i("bDrawingFramebuffer", drawingFramebuffer);
        }

        // Set index data and render
        GL_VERIFY(glDrawElements(GL_TRIANGLES, (batch.mNumTrisToDraw) * 3, GL_UNSIGNED_INT, (void*) (idxOffset * sizeof(GLuint))));

        idxOffset += (batch.mNumTrisToDraw) * 3;
        baseTextureIdx += batch.mTexturesInBatch;

        mStats.mInvalidationsCount++;
    }

    // Do not clear gas here - it's released later
}

void OpenGLRenderer::ScaledScissor(s32 x, s32 y, s32 width, s32 height)
{
    f32 wScale = static_cast<f32>(mPsxFramebuffer[0].GetWidth()) / kPsxFramebufferWidth;
    f32 hScale = static_cast<f32>(mPsxFramebuffer[0].GetHeight()) / kPsxFramebufferHeight;

    s32 scaledX = static_cast<s32>(x * wScale);
    s32 scaledY = static_cast<s32>(y * hScale);
    s32 scaledW = static_cast<s32>(width * wScale);
    s32 scaledH = static_cast<s32>(height * hScale);

    GL_VERIFY(glScissor(scaledX, mPsxFramebuffer[0].GetHeight() - scaledY - scaledH, scaledW, scaledH));
}
