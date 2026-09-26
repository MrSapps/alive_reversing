#include "../../../relive_lib/Primitives.hpp"
#include "../../../relive_lib/Font.hpp"
#include "FatalError.hpp"
#include "Sdl3Renderer.hpp"
#include "../../Window.hpp"
#include <cmath>
#include <algorithm>

Sdl3Renderer::Sdl3Renderer(Window& window)
    : IRenderer(window),
    mContext(window),
    mPsxFbTexture(mContext, kPsxFramebufferWidth, kPsxFramebufferHeight, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET),
    mGasTexture(mContext, kPsxFramebufferWidth, kPsxFramebufferHeight, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING),
    mGasTarget(mContext, kPsxFramebufferWidth, kPsxFramebufferHeight, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET)
{
    // Render target support is required for things like FG1 mask and
    // framebuffer textures
    if (!mContext.IsRenderTargetSupported())
    {
        ALIVE_FATAL("%s", "SDL3 renderer requires render target support.");
    }

    // Thrown rather than fatal, so Window::CreateWithRenderer can fall back to another renderer
    if (!mContext.SupportsCustomBlendModes())
    {
        throw RendererException("The SDL3 renderer needs custom blend modes, which this SDL renderer doesn't support");
    }

    // The laughing gas checkerboard, in the OpenGL renderer's pattern (its framebuffer rows count
    // from the bottom): white with half alpha where the gas is blended in, and black with full
    // alpha ("leave alone" once drawn) elsewhere
    std::vector<RGBA32> mask(kPsxFramebufferWidth * kPsxFramebufferHeight);
    for (s32 y = 0; y < kPsxFramebufferHeight; y++)
    {
        for (s32 x = 0; x < kPsxFramebufferWidth; x++)
        {
            const bool blended = ((x + (kPsxFramebufferHeight - 1 - y)) & 1) == 0;
            mask[y * kPsxFramebufferWidth + x] = blended ? RGBA32{255, 255, 255, 128} : RGBA32{0, 0, 0, 255};
        }
    }
    mGasMask = mContext.CreateTexture(SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, kPsxFramebufferWidth, kPsxFramebufferHeight);
    if (!SDL_UpdateTexture(mGasMask.get(), nullptr, mask.data(), kPsxFramebufferWidth * 4))
    {
        ALIVE_FATAL("SDL_UpdateTexture failed: %s", SDL_GetError());
    }
    Sdl3Context::SetTextureBlendMode(mGasMask.get(), Sdl3Context::GasMaskBlendMode());
}

Sdl3Renderer::~Sdl3Renderer()
{
}

void Sdl3Renderer::Clear(u8 r, u8 g, u8 b)
{
    // Perform the clear now
    mContext.UseScreenFramebuffer();

    SDL_SetRenderDrawColor(mContext.GetRenderer(), r, g, b, 255);
    SDL_RenderClear(mContext.GetRenderer());

    mContext.UseTextureFramebuffer(GetActiveFbTexture().GetTexture());
    ApplyClip();
}

static SDL_FColor ToSDLColor(u8 r, u8 g, u8 b, u8 a)
{
    return {
        static_cast<f32>(r) / 255.0f,
        static_cast<f32>(g) / 255.0f,
        static_cast<f32>(b) / 255.0f,
        static_cast<f32>(a) / 255.0f
    };
}

void Sdl3Renderer::Draw(const Prim_GasEffect& gasEffect)
{
    mScreenWaveSourceValid = false;

    // The gas is a low resolution image (a quarter of the width and half the height of the
    // area) stretched over the area. The OpenGL renderer blends it in half and half on every
    // other pixel, in a checkerboard, and leaves the rest alone. Here that's done by stretching
    // it into a render target at half strength, cutting the checkerboard out of that with a mask,
    // and drawing the result over the frame.
    const s32 gasWidth = (gasEffect.w - gasEffect.x) / 4;
    const s32 gasHeight = (gasEffect.h - gasEffect.y) / 2;
    if (!gasEffect.pGasPixels || gasWidth <= 0 || gasHeight <= 0)
    {
        return;
    }

    const SDL_Rect gasRect = {0, 0, gasWidth, gasHeight};
    mGasTexture.Update(&gasRect, gasEffect.pGasPixels);

    const f32 x0 = static_cast<f32>(gasEffect.x);
    const f32 y0 = static_cast<f32>(gasEffect.y);
    const f32 x1 = static_cast<f32>(gasEffect.w);
    const f32 y1 = static_cast<f32>(gasEffect.h);
    SDL_Renderer* pRenderer = mContext.GetRenderer();
    constexpr s32 indexList[6] = { 0, 1, 2, 1, 2 , 3 };

    // 1: the gas at half strength, stretched over the area of the render target
    mContext.UseTextureFramebuffer(mGasTarget.GetTexture());
    const SDL_FColor half = {0.5f, 0.5f, 0.5f, 0.5f};
    const f32 gasU = static_cast<f32>(gasWidth) / kPsxFramebufferWidth;
    const f32 gasV = static_cast<f32>(gasHeight) / kPsxFramebufferHeight;
    const SDL_Vertex gasVerts[] = {
        { {x0, y0}, half, { 0.0f, 0.0f } },
        { {x1, y0}, half, { gasU, 0.0f } },
        { {x0, y1}, half, { 0.0f, gasV } },
        { {x1, y1}, half, { gasU, gasV } },
    };
    SDL_RenderGeometry(pRenderer, mGasTexture.GetTexture(), gasVerts, 4, indexList, 6);
    mContext.CountDrawCall();

    // 2: the checkerboard cut out of it, the same area of the mask
    const SDL_FColor white = {1.0f, 1.0f, 1.0f, 1.0f};
    const f32 u0 = x0 / kPsxFramebufferWidth;
    const f32 v0 = y0 / kPsxFramebufferHeight;
    const f32 u1 = x1 / kPsxFramebufferWidth;
    const f32 v1 = y1 / kPsxFramebufferHeight;
    const SDL_Vertex areaVerts[] = {
        { {x0, y0}, white, { u0, v0 } },
        { {x1, y0}, white, { u1, v0 } },
        { {x0, y1}, white, { u0, v1 } },
        { {x1, y1}, white, { u1, v1 } },
    };
    SDL_RenderGeometry(pRenderer, mGasMask.get(), areaVerts, 4, indexList, 6);
    mContext.CountDrawCall();

    // 3: over the frame, as src + dst * src alpha, like the OpenGL renderer
    mContext.UseTextureFramebuffer(GetActiveFbTexture().GetTexture());
    ApplyClip();
    SDL_Vertex frameVerts[4];
    std::copy(std::begin(areaVerts), std::end(areaVerts), frameVerts);
    mGasTarget.SetTextureBlendMode(Sdl3Context::PsxTextureBlendMode());
    DrawVertices(frameVerts, 4, indexList, 6, mGasTarget.GetTexture(), false, relive::TBlendModes::eBlend_0);
    mGasTarget.SetTextureBlendMode(SDL_BLENDMODE_NONE);
}

void Sdl3Renderer::Draw(const Line_G2& line)
{
    mScreenWaveSourceValid = false;

    const IRenderer::Point2D points[] = {
        IRenderer::Point2D(line.X0(), line.Y0()),
        IRenderer::Point2D(line.X1(), line.Y1())
    };

    // Gouraud shaded from one end to the other
    const SDL_FColor colours[] = {
        ToSDLColor(line.R0(), line.G0(), line.B0(), 255),
        ToSDLColor(line.R1(), line.G1(), line.B1(), 255)
    };

    DrawLines(points, colours, 2, line.mSemiTransparent ? line.mBlendMode : relive::TBlendModes::None);
}

void Sdl3Renderer::Draw(const Line_G4& line)
{
    mScreenWaveSourceValid = false;

    const IRenderer::Point2D points[] = {
        IRenderer::Point2D(line.X0(), line.Y0()),
        IRenderer::Point2D(line.X1(), line.Y1()),
        IRenderer::Point2D(line.X2(), line.Y2()),
        IRenderer::Point2D(line.X3(), line.Y3())
    };

    const SDL_FColor colours[] = {
        ToSDLColor(line.R0(), line.G0(), line.B0(), 255),
        ToSDLColor(line.R1(), line.G1(), line.B1(), 255),
        ToSDLColor(line.R2(), line.G2(), line.B2(), 255),
        ToSDLColor(line.R3(), line.G3(), line.B3(), 255)
    };

    DrawLines(points, colours, 4, line.mSemiTransparent ? line.mBlendMode : relive::TBlendModes::None);
}

void Sdl3Renderer::Draw(const Poly_G3& poly)
{
    mScreenWaveSourceValid = false;

    SDL_Vertex vertices[] = {
        { { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()) }, { ToSDLColor(poly.R0(), poly.G0(), poly.B0(), 255) }, { 0.0f, 0.0f } },
        { { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()) }, { ToSDLColor(poly.R1(), poly.G1(), poly.B1(), 255) }, { 0.0f, 0.0f } },
        { { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()) }, { ToSDLColor(poly.R2(), poly.G2(), poly.B2(), 255) }, { 0.0f, 0.0f } },
    };

    DrawVertices(vertices, 3, nullptr, 0, nullptr, poly.mSemiTransparent, poly.mBlendMode);
}

void Sdl3Renderer::Draw(const Poly_FT4& poly)
{
    SDL_Texture* tex = nullptr;
    mScreenWaveSourceValid = false;

    constexpr s32 indexList[6] = { 0, 1, 2, 1, 2 , 3 };
    SDL_Vertex vertices[] = {
        { { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()) }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
        { { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()) }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
        { { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()) }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
        { { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()) }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } },
    };

    if (poly.mFg1)
    {
        std::shared_ptr<Sdl3Texture> texFG1 = PrepareTextureFromPoly(poly);

        if (texFG1)
        {
            tex = texFG1->GetTexture();
        }
    }
    else if (poly.mCam)
    {
        tex = PrepareTextureFromPoly(poly)->GetTexture();
    }
    else if (poly.mAnim)
    {
        RGBA32 shading = {
            poly.R0(),
            poly.G0(),
            poly.B0(),
            255
        };

        if (!poly.mIsShaded)
        {
            shading.a = 0;
        }

        AnimResource& animRes = poly.mAnim->mAnimRes;
        const PerFrameInfo* pHeader = poly.mAnim->Get_FrameHeader(-1);
        std::shared_ptr<PngData> pPng = animRes.mPngPtr;

        SDL_FColor vertexColour = {};
        tex =
            PrepareTextureFromPoly(poly)->GetTextureUsePalette(
                poly.mAnim->mAnimRes.mCurPal,
                shading,
                poly.mSemiTransparent,
                poly.mBlendMode,
                vertexColour
            );
        for (SDL_Vertex& vertex : vertices)
        {
            vertex.color = vertexColour;
        }

        // Fiddle with UVs...
        f32 u0 = static_cast<f32>(pHeader->mSpriteSheetX) / pPng->mWidth;
        f32 v0 = static_cast<f32>(pHeader->mSpriteSheetY) / pPng->mHeight;
        f32 u1 = u0 + (static_cast<f32>(pHeader->mSpriteWidth - 1) / pPng->mWidth);
        f32 v1 = v0 + (static_cast<f32>(pHeader->mSpriteHeight - 1) / pPng->mHeight);

        if (poly.mFlipX)
        {
            std::swap(u0, u1);
        }

        if (poly.mFlipY)
        {
            std::swap(v0, v1);
        }

        vertices[0].tex_coord.x = u0;
        vertices[0].tex_coord.y = v0;

        vertices[1].tex_coord.x = u1;
        vertices[1].tex_coord.y = v0;

        vertices[2].tex_coord.x = u0;
        vertices[2].tex_coord.y = v1;

        vertices[3].tex_coord.x = u1;
        vertices[3].tex_coord.y = v1;
    }
    else if (poly.mFont)
    {
        RGBA32 shading = {
            poly.R0(),
            poly.G0(),
            poly.B0(),
            255
        };

        std::shared_ptr<PngData> pPng = poly.mFont->mFntResource.mPngPtr;

        f32 u0 = poly.U0() / static_cast<f32>(pPng->mWidth);
        f32 v0 = poly.V0() / static_cast<f32>(pPng->mHeight);

        f32 u1 = poly.U3() / static_cast<f32>(pPng->mWidth);
        f32 v1 = poly.V3() / static_cast<f32>(pPng->mHeight);

        SDL_FColor vertexColour = {};
        tex =
            PrepareTextureFromPoly(poly)->GetTextureUsePalette(
                poly.mFont->mFntResource.mCurPal,
                shading,
                poly.mSemiTransparent,
                poly.mBlendMode,
                vertexColour
            );
        for (SDL_Vertex& vertex : vertices)
        {
            vertex.color = vertexColour;
        }

        vertices[0].tex_coord.x = u0;
        vertices[0].tex_coord.y = v0;

        vertices[1].tex_coord.x = u1;
        vertices[1].tex_coord.y = v0;

        vertices[2].tex_coord.x = u0;
        vertices[2].tex_coord.y = v1;

        vertices[3].tex_coord.x = u1;
        vertices[3].tex_coord.y = v1;
    }
    else
    {
        return;
    }

    DrawVertices(vertices, 4, indexList, 6, tex, poly.mSemiTransparent, poly.mBlendMode);
}

void Sdl3Renderer::Draw(const Prim_ScreenWave& wave)
{
    // The pieces in a run all draw from the frame as it was before the first
    if (!mScreenWaveSourceValid)
    {
        UpdateScreenWaveSource();
        mScreenWaveSourceValid = true;
    }

    // mScreenWaveSource is the frame with a 1 pixel border round it
    const f32 frameW = static_cast<f32>(GetActiveFbTexture().GetWidth());
    const f32 frameH = static_cast<f32>(GetActiveFbTexture().GetHeight());
    constexpr s32 indexList[6] = {0, 1, 2, 1, 2, 3};
    SDL_Vertex vertices[4] = {};
    for (s32 i = 0; i < 4; i++)
    {
        vertices[i].position = {static_cast<f32>(wave.mVerts[i].x), static_cast<f32>(wave.mVerts[i].y)};
        vertices[i].color = {1.0f, 1.0f, 1.0f, 1.0f};
        vertices[i].tex_coord.x = (wave.mSource[i].x * frameW / kPsxFramebufferWidth + 1.0f) / (frameW + 2.0f);
        vertices[i].tex_coord.y = (wave.mSource[i].y * frameH / kPsxFramebufferHeight + 1.0f) / (frameH + 2.0f);
    }

    // Clamped, so anything from outside the screen reads the border. SDL's default wraps.
    SDL_SetRenderTextureAddressMode(mContext.GetRenderer(), SDL_TEXTURE_ADDRESS_CLAMP, SDL_TEXTURE_ADDRESS_CLAMP);
    DrawVertices(vertices, 4, indexList, 6, mScreenWaveSource.get(), false, relive::TBlendModes::eBlend_0);
    SDL_SetRenderTextureAddressMode(mContext.GetRenderer(), SDL_TEXTURE_ADDRESS_AUTO, SDL_TEXTURE_ADDRESS_AUTO);
}

// A copy of the frame for the screen wave to draw from, with a 1 pixel border round it so that
// anything from outside the screen reads the border. The border and black (anything that would be
// black in 16 bit colour) are left out: colour 0 and alpha 1, for PsxTextureBlendMode's
// src + dst * src alpha. Everything else has alpha 0.
void Sdl3Renderer::UpdateScreenWaveSource()
{
    SDL_Surface* pSurface = SDL_RenderReadPixels(mContext.GetRenderer(), nullptr);
    SDL_Surface* pFrame = pSurface ? SDL_ConvertSurface(pSurface, SDL_PIXELFORMAT_RGBA32) : nullptr;
    SDL_DestroySurface(pSurface);
    if (!pFrame)
    {
        LOG_ERROR("Reading the frame for the screen wave failed: %s", SDL_GetError());
        return;
    }

    const s32 texW = pFrame->w + 2;
    const s32 texH = pFrame->h + 2;
    constexpr RGBA32 kLeftOut = {0, 0, 0, 255};
    if (!mScreenWaveSource || mScreenWaveSource->w != texW || mScreenWaveSource->h != texH)
    {
        mScreenWaveSource = mContext.CreateTexture(SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, texW, texH);
        Sdl3Context::SetTextureBlendMode(mScreenWaveSource.get(), Sdl3Context::PsxTextureBlendMode());

        const std::vector<RGBA32> border(static_cast<std::size_t>(texW) * texH, kLeftOut);
        SDL_UpdateTexture(mScreenWaveSource.get(), nullptr, border.data(), texW * 4);
        mContext.CountTextureUpload();
    }

    std::vector<u32>& pixels = mContext.ScratchPixels(static_cast<std::size_t>(pFrame->w) * pFrame->h);
    RGBA32* pOut = reinterpret_cast<RGBA32*>(pixels.data());
    for (s32 y = 0; y < pFrame->h; y++)
    {
        const u8* pIn = static_cast<const u8*>(pFrame->pixels) + y * pFrame->pitch;
        for (s32 x = 0; x < pFrame->w; x++, pIn += 4)
        {
            const bool black = pIn[0] < 8 && pIn[1] < 4 && pIn[2] < 8;
            *pOut++ = black ? kLeftOut : RGBA32{pIn[0], pIn[1], pIn[2], 0};
        }
    }

    const SDL_Rect frameRect = {1, 1, pFrame->w, pFrame->h};
    SDL_UpdateTexture(mScreenWaveSource.get(), &frameRect, pixels.data(), pFrame->w * 4);
    mContext.CountTextureUpload();
    SDL_DestroySurface(pFrame);
}

void Sdl3Renderer::Draw(const Poly_G4& poly)
{
    mScreenWaveSourceValid = false;

    constexpr s32 indexList[6] = { 0, 1, 2, 1, 2 , 3 };
    SDL_Vertex vertices[4] = {
        { { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()) }, { ToSDLColor(poly.R0(), poly.G0(), poly.B0(), 255) }, { 0.0f, 0.0f } },
        { { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()) }, { ToSDLColor(poly.R1(), poly.G1(), poly.B1(), 255) }, { 0.0f, 0.0f } },
        { { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()) }, { ToSDLColor(poly.R2(), poly.G2(), poly.B2(), 255) }, { 0.0f, 0.0f } },
        { { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()) }, { ToSDLColor(poly.R3(), poly.G3(), poly.B3(), 255) }, { 0.0f, 0.0f } },
    };

    DrawVertices(vertices, 4, indexList, 6, nullptr, poly.mSemiTransparent, poly.mBlendMode);
}

void Sdl3Renderer::EndFrame()
{
    CaptureIfRequested();

    mTextureCache.DecreaseResourceLifetimes();

    mLastFrameStats.mDrawCalls = mContext.TakeDrawCallCount();
    mLastFrameStats.mTextureUploads = mContext.TakeTextureUploadCount();
    mLastFrameStats.mCachedTextures = mTextureCache.Size();

    mContext.UseScreenFramebuffer();

    // Copy framebuffer to screen
    SDL_Rect drawRect = GetTargetDrawRect();
    SDL_FRect fdrawRect;

    SDL_RectToFRect(&drawRect, &fdrawRect);

    // Filtering only applies to scaling the frame to the window. The framebuffer stays unfiltered
    // while the frame is drawn, like every other texture.
    SDL_Texture* pFrame = GetActiveFbTexture().GetTexture();
    SDL_SetTextureScaleMode(pFrame, mFramebufferFilter ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
    SDL_RenderTexture(mContext.GetRenderer(), pFrame, nullptr, &fdrawRect);
    SDL_SetTextureScaleMode(pFrame, SDL_SCALEMODE_NEAREST);

    mContext.Present();
}

void Sdl3Renderer::ReadPsxFramebuffer(std::vector<u8>& rgbaPixels, s32& width, s32& height)
{
    width = 0;
    height = 0;
    rgbaPixels.clear();

    mContext.UseTextureFramebuffer(GetActiveFbTexture().GetTexture());
    SDL_Surface* pSurface = SDL_RenderReadPixels(mContext.GetRenderer(), nullptr);
    if (!pSurface)
    {
        LOG_ERROR("SDL_RenderReadPixels failed: %s", SDL_GetError());
        return;
    }

    SDL_Surface* pRgba = SDL_ConvertSurface(pSurface, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(pSurface);
    if (!pRgba)
    {
        LOG_ERROR("SDL_ConvertSurface failed: %s", SDL_GetError());
        return;
    }

    width = pRgba->w;
    height = pRgba->h;
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4;
    rgbaPixels.resize(rowBytes * height);
    for (s32 y = 0; y < height; y++)
    {
        memcpy(&rgbaPixels[y * rowBytes], static_cast<const u8*>(pRgba->pixels) + y * pRgba->pitch, rowBytes);
    }
    SDL_DestroySurface(pRgba);
}

void Sdl3Renderer::SetClip(const Prim_ScissorRect& clipper)
{
    SDL_Rect rect = {};

    f32 factorW = static_cast<f32>(GetActiveFbTexture().GetWidth()) / kPsxFramebufferWidth;
    f32 factorH = static_cast<f32>(GetActiveFbTexture().GetHeight()) / kPsxFramebufferHeight;

    rect.x = static_cast<s32>(clipper.mRect.x * factorW);
    rect.y = static_cast<s32>(clipper.mRect.y * factorH);
    rect.w = static_cast<s32>(clipper.mRect.w * factorW);
    rect.h = static_cast<s32>(clipper.mRect.h * factorH);

    // (0, 0, 1, 1) means no clipping
    mClipEnabled = !(clipper.mRect.x == 0 && clipper.mRect.y == 0 && clipper.mRect.w == 1 && clipper.mRect.h == 1);
    mClipRect = rect;
    ApplyClip();

    // Like the OpenGL renderer's batches, a new clip rectangle ends a run of screen wave pieces
    mScreenWaveSourceValid = false;
}

void Sdl3Renderer::ApplyClip()
{
    // Each SDL render target has its own clip rectangle, so this is set again after switching
    SDL_SetRenderClipRect(mContext.GetRenderer(), mClipEnabled ? &mClipRect : nullptr);
}

void Sdl3Renderer::StartFrame()
{
    IRenderer::StartFrame();

    mContext.NextFrame();

    mOffsetX = 0;
    mOffsetY = 0;

    // Resize the framebuffer if needed
    SDL_Rect desiredFbSize = GetFramebufferRect();

    u32 desiredW = static_cast<u32>(desiredFbSize.w);
    u32 desiredH = static_cast<u32>(desiredFbSize.h);

    if (
        mPsxFbTexture.GetWidth() != desiredW ||
        mPsxFbTexture.GetHeight() != desiredH
    )
    {
        mPsxFbTexture.Resize(desiredW, desiredH);
    }

    mContext.UseTextureFramebuffer(GetActiveFbTexture().GetTexture());

    // A clip rectangle only lasts until the end of the frame that set it, as with OpenGL
    mClipEnabled = false;
    ApplyClip();
    mScreenWaveSourceValid = false;
}

void Sdl3Renderer::DrawLines(const IRenderer::Point2D points[], const SDL_FColor colours[], s32 numPoints, relive::TBlendModes blendMode)
{
    constexpr s32 indexList[6] = { 0, 1, 2, 1, 2 , 3 };

    for (s32 i = 1; i < numPoints; i++)
    {
        // The first two corners are at point A's end, the last two at B's
        const IRenderer::Quad2D quad = IRenderer::LineToQuad(points[i - 1], points[i]);
        const SDL_FColor colourA = colours[i - 1];
        const SDL_FColor colourB = colours[i];

        SDL_Vertex vertices[4] = {
            { { quad.verts[0].x, quad.verts[0].y }, colourA, { 0.0f, 0.0f } },
            { { quad.verts[1].x, quad.verts[1].y }, colourA, { 0.0f, 0.0f } },
            { { quad.verts[2].x, quad.verts[2].y }, colourB, { 0.0f, 0.0f } },
            { { quad.verts[3].x, quad.verts[3].y }, colourB, { 0.0f, 0.0f } },
        };

        DrawVertices(vertices, 4, indexList, 6, nullptr, blendMode != relive::TBlendModes::None, blendMode);
    }
}

void Sdl3Renderer::DrawVertices(SDL_Vertex vertices[], s32 numVertices, const s32 indices[], s32 numIndices, SDL_Texture* texture, bool isSemiTrans, relive::TBlendModes blendMode)
{
    ScaleVertices(vertices, numVertices);

    // This blend mode stuff is only needed for untextured polys, textures
    // have their own blend modes set up in Sdl3Texture
    if (!texture && isSemiTrans)
    {
        switch (blendMode)
        {
            // 50% DST + 50% SRC
            //
            // In order to achieve the desired result, we draw an identical set
            // of vertices as the input, but with the colours set to 128 and
            // then draw with BLENDMODE_MOD - this results in 50% dst colour
            //
            // Then the input vertices can be drawn as normal for 50% src
            case relive::TBlendModes::eBlend_0:
            {
                std::vector<SDL_Vertex> dstVertices;

                dstVertices.reserve(numVertices);

                for (s32 i = 0; i < numVertices; i++)
                {
                    dstVertices.push_back(vertices[i]);
                    dstVertices[i].color = { 0.5f, 0.5f, 0.5f, 1.0f };
                    vertices[i].color.a = 0.5f;
                }

                mContext.SetDrawBlendMode(SDL_BLENDMODE_MOD);
                SDL_RenderGeometry(mContext.GetRenderer(), nullptr, dstVertices.data(), numVertices, indices, numIndices);
                mContext.CountDrawCall();

                mContext.SetDrawBlendMode(SDL_BLENDMODE_ADD);
                break;
            }

            // 100% DST + 100% SRC
            case relive::TBlendModes::eBlend_1:
                mContext.SetDrawBlendMode(SDL_BLENDMODE_ADD);
                break;

            // 100% DST - 100% SRC
            case relive::TBlendModes::eBlend_2:
                mContext.SetDrawBlendMode(Sdl3Context::SubtractBlendMode());
                break;

            // 100% DST + 25% SRC
            case relive::TBlendModes::eBlend_3:
                for (s32 i = 0; i < numVertices; i++)
                {
                    vertices[i].color.a = 0.25f;
                }

                mContext.SetDrawBlendMode(SDL_BLENDMODE_ADD);
                break;

            default:
                ALIVE_FATAL("Unknown blend mode.");
                break;
        }
    }

    SDL_RenderGeometry(mContext.GetRenderer(), texture, vertices, numVertices, indices, numIndices);
    mContext.CountDrawCall();
    mContext.SetDrawBlendMode(SDL_BLENDMODE_NONE);
}

Sdl3Texture& Sdl3Renderer::GetActiveFbTexture()
{
    return mPsxFbTexture;
}

std::shared_ptr<Sdl3Texture> Sdl3Renderer::PrepareTextureFromPoly(const Poly_FT4& poly)
{
    std::shared_ptr<Sdl3Texture> texture;

    if (poly.mFg1)
    {
        // TODO: Implement this
        // FIXME: kCamLifetime should be in IRenderer ?
        texture = mTextureCache.GetCachedTexture(poly.mFg1->mUniqueId.Id(), 1);

        if (!texture || mFg1CamId != mLastTouchedCamId)
        {
            std::shared_ptr<Sdl3Texture> camRefTex = mTextureCache.GetCachedTexture(mLastTouchedCamId, 1);

            if (camRefTex)
            {
                std::shared_ptr<Sdl3Texture> fg1Tex = Sdl3Texture::FromMask(mContext, camRefTex, poly.mFg1->mImage.mPixels->data());

                texture = mTextureCache.Add(
                    poly.mFg1->mUniqueId.Id(),
                    1,
                    fg1Tex);

                mFg1CamId = mLastTouchedCamId;

                LOG("SDL3 FG1 cache miss %u", poly.mFg1->mUniqueId.Id());
            }
            else
            {
                LOG("SDL3 FG1 with no CAM");
            }
        }
    }
    else if (poly.mCam)
    {
        mLastTouchedCamId = poly.mCam->mUniqueId.Id();

        // FIXME: kCamLifetime should be in IRenderer ?
        texture = mTextureCache.GetCachedTexture(poly.mCam->mUniqueId.Id(), 1);

        if (!texture)
        {
            auto camTex =
                std::make_shared<Sdl3Texture>(
                    mContext,
                    poly.mCam->mData.mWidth,
                    poly.mCam->mData.mHeight,
                    SDL_PIXELFORMAT_RGBA32,
                    SDL_TEXTUREACCESS_STATIC
                );

            camTex->Update(nullptr, poly.mCam->mData.mPixels->data());

            texture =
                mTextureCache.Add(
                    poly.mCam->mUniqueId.Id(),
                    1,
                    camTex
                );

            LOG("SDL3 CAM cache miss %u", poly.mCam->mUniqueId.Id());
        }
    }
    else if (poly.mAnim)
    {
        // FIXME: Temp bump amount
        texture = mTextureCache.GetCachedTexture(poly.mAnim->mAnimRes.mUniqueId.Id(), 255);

        if (!texture)
        {
            auto animTex =
                std::make_shared<Sdl3Texture>(
                    mContext,
                    poly.mAnim->mAnimRes.mPngPtr->mWidth,
                    poly.mAnim->mAnimRes.mPngPtr->mHeight,
                    SDL_PIXELFORMAT_INDEX8,
                    SDL_TEXTUREACCESS_STREAMING
                );

            animTex->Update(nullptr, poly.mAnim->mAnimRes.mPngPtr->mPixels.data());

            texture =
                mTextureCache.Add(
                    poly.mAnim->mAnimRes.mUniqueId.Id(),
                    255,
                    animTex
                );
        }
    }
    else if (poly.mFont)
    {
        // FIXME: Temp bump amount
        texture = mTextureCache.GetCachedTexture(poly.mFont->mFntResource.mUniqueId.Id(), 255);

        if (!texture)
        {
            std::shared_ptr<PngData> pPng = poly.mFont->mFntResource.mPngPtr;

            auto fontTex =
                std::make_shared<Sdl3Texture>(
                    mContext,
                    pPng->mWidth,
                    pPng->mHeight,
                    SDL_PIXELFORMAT_INDEX8,
                    SDL_TEXTUREACCESS_STREAMING
                );

            fontTex->Update(nullptr, pPng->mPixels.data());

            texture =
                mTextureCache.Add(
                    poly.mFont->mFntResource.mUniqueId.Id(),
                    255,
                    fontTex
                );
        }
    }

    return texture;
}

SDL_FPoint Sdl3Renderer::PointToViewport(const SDL_FPoint& point)
{
    if (mUseOriginalResolution)
    {
        return point;
    }

    f32 factorW = static_cast<f32>(GetActiveFbTexture().GetWidth()) / kPsxFramebufferWidth;
    f32 factorH = static_cast<f32>(GetActiveFbTexture().GetHeight()) / kPsxFramebufferHeight;

    SDL_FPoint scaledPoint = {
        point.x * factorW,
        point.y * factorH
    };

    return scaledPoint;
}

void Sdl3Renderer::ScaleVertices(SDL_Vertex vertices[], s32 numVertices)
{
    for (u8 i = 0; i < numVertices; i++)
    {
        vertices[i].position = PointToViewport(vertices[i].position);
    }
}

