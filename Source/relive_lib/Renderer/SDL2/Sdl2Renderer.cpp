#include "../../../relive_lib/Primitives.hpp"
#include "../../../AliveLibAE/Font.hpp"
#include "FatalError.hpp"
#include "Sdl2Renderer.hpp"

Sdl2Renderer::Sdl2Renderer(TWindowHandleType window)
    : IRenderer(window),
    mContext(window),
    mPsxFbTexture{
        Sdl2Texture(mContext, kPsxFramebufferWidth, kPsxFramebufferHeight, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET),
        Sdl2Texture(mContext, kPsxFramebufferWidth, kPsxFramebufferHeight, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET)
    },
    mGasTexture(mContext, kPsxFramebufferWidth, kPsxFramebufferHeight, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING)
{
    // Set up the gas blend mode
    //
    mGasTexture.SetTextureBlendMode(SDL_BLENDMODE_ADD);

    // Render target support is required for things like FG1 mask and
    // framebuffer textures
    if (!mContext.IsRenderTargetSupported())
    {
        ALIVE_FATAL("%s", "SDL2 renderer requires render target support.");
    }
}

Sdl2Renderer::~Sdl2Renderer()
{
}

void Sdl2Renderer::Clear(u8 r, u8 g, u8 b)
{
    LOG("%s", "SDL2: Clear viewport");

    mContext.UseScreenFramebuffer();

    SDL_SetRenderDrawColor(mContext.GetRenderer(), r, g, b, 255);
    SDL_RenderClear(mContext.GetRenderer());

    mContext.UseTextureFramebuffer(GetActiveFbTexture().GetTexture());
}

void Sdl2Renderer::Draw(const Prim_GasEffect& gasEffect)
{
    LOG("%s", "SDL2: Draw Prim_GasEffect");

    // TODO: Implement this
    const f32 x = static_cast<f32>(gasEffect.x);
    const f32 y = static_cast<f32>(gasEffect.y);
    const f32 w = static_cast<f32>(gasEffect.w);
    const f32 h = static_cast<f32>(gasEffect.h);

    const f32 gasWidth = std::floor(static_cast<f32>(gasEffect.w - gasEffect.x) / 4);
    const f32 gasHeight = std::floor(static_cast<f32>(gasEffect.h - gasEffect.y) / 2);

    const SDL_Rect gasRect = { 0, 0, static_cast<s32>(gasWidth), static_cast<s32>(gasHeight) };

    mGasTexture.Update(&gasRect, gasEffect.pGasPixels);

    SDL_Color c = { 127, 127, 127, 255 };

    f32 u1 = gasWidth / kPsxFramebufferWidth;
    f32 v1 = gasHeight / kPsxFramebufferHeight;

    constexpr s32 indexList[6] = { 0, 1, 2, 1, 2 , 3 };
    SDL_Vertex gasVerts[] = {
        { {x, y}, c, { 0,   0 } },
        { {w, y}, c, { u1,  0 } },
        { {x, h}, c, { 0,  v1 } },
        { {w, h}, c, { u1, v1 } },
    };

    DrawVertices(gasVerts, 4, indexList, 6, mGasTexture.GetTexture(), false, relive::TBlendModes::eBlend_0);
}

void Sdl2Renderer::Draw(const Line_G2& line)
{
    LOG("%s", "SDL2: Draw Line_G2");

    // FIXME: Handle thickness, and colour correctly (aka use a quad)
    SDL_SetRenderDrawColor(mContext.GetRenderer(), line.R0(), line.G0(), line.B0(), 255);
    SDL_RenderDrawLine(mContext.GetRenderer(), line.X0(), line.Y0(), line.X1(), line.Y1());
}

void Sdl2Renderer::Draw(const Line_G4& line)
{
    LOG("%s", "SDL2: Draw Line_G4");

    // FIXME: Handle thickness, and colour correctly (aka use a quad)
    SDL_Point points[] = {
        { line.X0(), line.Y0() },
        { line.X1(), line.Y1() },
        { line.X2(), line.Y2() },
        { line.X3(), line.Y3() },
    };

    SDL_SetRenderDrawColor(mContext.GetRenderer(), line.R0(), line.G0(), line.B0(), 255);
    SDL_RenderDrawLines(mContext.GetRenderer(), points, 4);
}

void Sdl2Renderer::Draw(const Poly_G3& poly)
{
    LOG("%s", "SDL2: Draw Poly_G3");

    SDL_Vertex vertices[] = {
        { { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()) }, { poly.R0(), poly.G0(), poly.B0(), 255 }, { 0, 0 } },
        { { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()) }, { poly.R1(), poly.G1(), poly.B1(), 255 }, { 0, 0 } },
        { { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()) }, { poly.R2(), poly.G2(), poly.B2(), 255 }, { 0, 0 } },
    };

    DrawVertices(vertices, 3, nullptr, 0, nullptr, poly.mSemiTransparent, poly.mBlendMode);
}

void Sdl2Renderer::Draw(const Poly_FT4& poly)
{
    SDL_Texture* tex = nullptr;

    constexpr s32 indexList[6] = { 0, 1, 2, 1, 2 , 3 };
    SDL_Vertex vertices[] = {
        { { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()) }, { 255, 255, 255, 255 }, { 0, 0 } },
        { { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()) }, { 255, 255, 255, 255 }, { 1, 0 } },
        { { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()) }, { 255, 255, 255, 255 }, { 0, 1 } },
        { { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()) }, { 255, 255, 255, 255 }, { 1, 1 } },
    };

    if (poly.mFg1)
    {
        LOG("%s", "SDL2: Draw Poly_FT4 (FG1)");

        std::shared_ptr<Sdl2Texture> texFG1 = PrepareTextureFromPoly(poly);

        if (texFG1)
        {
            tex = texFG1->GetTexture();
        }
    }
    else if (poly.mCam)
    {
        LOG("%s", "SDL2: Draw Poly_FT4 (CAM)");

        tex = PrepareTextureFromPoly(poly)->GetTexture();
    }
    else if (poly.mAnim)
    {
        LOG("%s", "SDL2: Draw Poly_FT4 (ANIM)");

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

        tex =
            PrepareTextureFromPoly(poly)->GetTextureUsePalette(
                poly.mAnim->mAnimRes.mPngPtr->mPal,
                shading,
                poly.mSemiTransparent,
                poly.mBlendMode
            );

        // Fiddle with UVs...
        f32 u0 = static_cast<f32>(pHeader->mSpriteSheetX) / pPng->mWidth;
        f32 v0 = static_cast<f32>(pHeader->mSpriteSheetY) / pPng->mHeight;
        f32 u1 = u0 + (static_cast<f32>(pHeader->mSpriteWidth) / pPng->mWidth);
        f32 v1 = v0 + (static_cast<f32>(pHeader->mSpriteHeight) / pPng->mHeight);

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

        LOG("%s", "SDL2: Draw Poly_FT4 (Font)");

        std::shared_ptr<PngData> pPng = poly.mFont->mFntResource.mPngPtr;

        f32 u0 = poly.U0() / static_cast<f32>(pPng->mWidth);
        f32 v0 = poly.V0() / static_cast<f32>(pPng->mHeight);

        f32 u1 = poly.U3() / static_cast<f32>(pPng->mWidth);
        f32 v1 = poly.V3() / static_cast<f32>(pPng->mHeight);

        tex =
            PrepareTextureFromPoly(poly)->GetTextureUsePalette(
                poly.mFont->mFntResource.mCurPal,
                shading,
                poly.mSemiTransparent,
                poly.mBlendMode
            );

        vertices[0].tex_coord.x = u0;
        vertices[0].tex_coord.y = v0;

        vertices[1].tex_coord.x = u1;
        vertices[1].tex_coord.y = v0;

        vertices[2].tex_coord.x = u0;
        vertices[2].tex_coord.y = v1;

        vertices[3].tex_coord.x = u1;
        vertices[3].tex_coord.y = v1;
    }
    else // Assume ScreenWave!
    {
        LOG("%s", "SDL2: Draw Poly_FT4 (ScreenWave)");

        // TODO: Implement this
    }

    DrawVertices(vertices, 4, indexList, 6, tex, poly.mSemiTransparent, poly.mBlendMode);
}

void Sdl2Renderer::Draw(const Poly_G4& poly)
{
    LOG("%s", "SDL2: Draw Poly_G4");

    constexpr s32 indexList[6] = { 0, 1, 2, 1, 2 , 3 };
    SDL_Vertex vertices[4] = {
        { { static_cast<f32>(poly.X0()), static_cast<f32>(poly.Y0()) }, { poly.R0(), poly.G0(), poly.B0(), 255 }, { 0, 0 } },
        { { static_cast<f32>(poly.X1()), static_cast<f32>(poly.Y1()) }, { poly.R1(), poly.G1(), poly.B1(), 255 }, { 0, 0 } },
        { { static_cast<f32>(poly.X2()), static_cast<f32>(poly.Y2()) }, { poly.R2(), poly.G2(), poly.B2(), 255 }, { 0, 0 } },
        { { static_cast<f32>(poly.X3()), static_cast<f32>(poly.Y3()) }, { poly.R3(), poly.G3(), poly.B3(), 255 }, { 0, 0 } },
    };

    DrawVertices(vertices, 4, indexList, 6, nullptr, poly.mSemiTransparent, poly.mBlendMode);
}

void Sdl2Renderer::EndFrame()
{
    LOG("%s", "SDL2: End frame");

    mTextureCache.DecreaseResourceLifetimes();

    mContext.UseScreenFramebuffer();

    // FIXME: Just drawing the whole thing for now - handle this properly
    //        with screen offset + aspect ratio (centred)
    SDL_RenderCopy(mContext.GetRenderer(), GetActiveFbTexture().GetTexture(), nullptr, nullptr);

    mContext.Present();
}

void Sdl2Renderer::SetClip(const Prim_ScissorRect& clipper)
{
    LOG("%s", "SDL2: Set viewport clip");

    // FIXME: Dedupe with GL render, move to IRenderer?
    SDL_Rect rect;

    rect.x = clipper.mRect.x;
    rect.y = clipper.mRect.y;
    rect.w = clipper.mRect.w;
    rect.h = clipper.mRect.h;

    if (rect.x == 0 && rect.y == 0 && rect.w == 1 && rect.h == 1)
    {
        // No scissor
        rect = {};
    }

    SDL_RenderSetClipRect(mContext.GetRenderer(), &rect);
}

void Sdl2Renderer::StartFrame()
{
    LOG("%s", "SDL2: Start frame");

    IRenderer::StartFrame();

    mOffsetX = 0;
    mOffsetY = 0;

    // FIXME: Need to check if framebuffer textures require resizing here!

    // Default back to render target
    mContext.UseTextureFramebuffer(mPsxFbTexture[0].GetTexture());
}

void Sdl2Renderer::DrawVertices(SDL_Vertex vertices[], s32 numVertices, const s32 indices[], s32 numIndices, SDL_Texture* texture, bool isSemiTrans, relive::TBlendModes blendMode)
{
    ScaleVertices(vertices, numVertices);

    // This blend mode stuff is only needed for untextured polys, textures
    // have their own blend modes set up in Sdl2Texture
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
                    dstVertices[i].color = { 128, 128, 128, 255 };
                    vertices[i].color.a = 128;
                }

                SDL_SetRenderDrawBlendMode(mContext.GetRenderer(), SDL_BLENDMODE_MOD);
                SDL_RenderGeometry(mContext.GetRenderer(), nullptr, dstVertices.data(), numVertices, indices, numIndices);

                SDL_SetRenderDrawBlendMode(mContext.GetRenderer(), SDL_BLENDMODE_ADD);
                break;
            }

            // 100% DST + 100% SRC
            case relive::TBlendModes::eBlend_1:
                SDL_SetRenderDrawBlendMode(mContext.GetRenderer(), SDL_BLENDMODE_ADD);
                break;

            // 100% DST - 100% SRC
            case relive::TBlendModes::eBlend_2:
            {
                SDL_BlendMode customBlendMode =
                    SDL_ComposeCustomBlendMode(
                        SDL_BLENDFACTOR_ONE,
                        SDL_BLENDFACTOR_ONE,
                        SDL_BLENDOPERATION_REV_SUBTRACT,
                        SDL_BLENDFACTOR_ZERO,
                        SDL_BLENDFACTOR_ONE,
                        SDL_BLENDOPERATION_ADD
                    );

                if (SDL_SetRenderDrawBlendMode(mContext.GetRenderer(), customBlendMode) < 0)
                {
                    // Not ideal... fallback to MOD, it's kind of close-ish, since the game
                    // mainly uses this blend mode to darken stuff uniformly, so MOD roughly ends
                    // up with a similar result
                    SDL_SetRenderDrawBlendMode(mContext.GetRenderer(), SDL_BLENDMODE_MOD);
                }
                break;
            }

            // 100% DST + 25% SRC
            case relive::TBlendModes::eBlend_3:
                for (s32 i = 0; i < numVertices; i++)
                {
                    vertices[i].color.a = 64;
                }

                SDL_SetRenderDrawBlendMode(mContext.GetRenderer(), SDL_BLENDMODE_ADD);
                break;

            default:
                ALIVE_FATAL("Unknown blend mode.");
                break;
        }
    }

    SDL_RenderGeometry(mContext.GetRenderer(), texture, vertices, numVertices, indices, numIndices);
    SDL_SetRenderDrawBlendMode(mContext.GetRenderer(), SDL_BLENDMODE_NONE);
}

Sdl2Texture& Sdl2Renderer::GetActiveFbTexture()
{
    return mPsxFbTexture[mActiveFbTexture];
}

std::shared_ptr<Sdl2Texture> Sdl2Renderer::PrepareTextureFromPoly(const Poly_FT4& poly)
{
    static u32 fg1CamId = 0;
    static u32 lastTouchedCamId = 0;

    std::shared_ptr<Sdl2Texture> texture;

    if (poly.mFg1)
    {
        // TODO: Implement this
        // FIXME: kCamLifetime should be in IRenderer ?
        texture = mTextureCache.GetCachedTexture(poly.mFg1->mUniqueId.Id(), 1);

        if (!texture || fg1CamId != lastTouchedCamId)
        {
            std::shared_ptr<Sdl2Texture> camRefTex = mTextureCache.GetCachedTexture(lastTouchedCamId, 1);

            if (camRefTex)
            {
                std::shared_ptr<Sdl2Texture> fg1Tex = Sdl2Texture::FromMask(mContext, camRefTex, poly.mFg1->mImage.mPixels->data());

                texture = mTextureCache.Add(
                    poly.mFg1->mUniqueId.Id(),
                    1,
                    fg1Tex);

                fg1CamId = lastTouchedCamId;

                LOG("SDL2 FG1 cache miss %u", poly.mFg1->mUniqueId.Id());
            }
            else
            {
                LOG("SDL2 FG1 with no CAM");
            }
        }
    }
    else if (poly.mCam)
    {
        lastTouchedCamId = poly.mCam->mUniqueId.Id();

        // FIXME: kCamLifetime should be in IRenderer ?
        texture = mTextureCache.GetCachedTexture(poly.mCam->mUniqueId.Id(), 1);

        if (!texture)
        {
            auto camTex =
                std::make_shared<Sdl2Texture>(
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

            LOG("SDL2 CAM cache miss %u", poly.mCam->mUniqueId.Id());
        }
    }
    else if (poly.mAnim)
    {
        // FIXME: Temp bump amount
        texture = mTextureCache.GetCachedTexture(poly.mAnim->mAnimRes.mUniqueId.Id(), 255);

        if (!texture)
        {
            auto animTex =
                std::make_shared<Sdl2Texture>(
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
                std::make_shared<Sdl2Texture>(
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

SDL_FPoint Sdl2Renderer::PointToViewport(const SDL_FPoint& point)
{
    if (mUseOriginalResolution)
    {
        return point;
    }

    SDL_Rect   wndRect     = GetTargetDrawRect();
    SDL_FPoint scaledPoint = {
        point.x * (wndRect.w / kPsxFramebufferWidth),
        point.y * (wndRect.h / kPsxFramebufferHeight)
    };

    return scaledPoint;
}

void Sdl2Renderer::ScaleVertices(SDL_Vertex vertices[], s32 numVertices)
{
    for (u8 i = 0; i < numVertices; i++)
    {
        vertices[i].position = PointToViewport(vertices[i].position);
    }
}
