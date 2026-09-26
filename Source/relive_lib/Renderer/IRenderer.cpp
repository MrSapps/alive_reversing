#include "../stdafx.h"
#include "IRenderer.hpp"
#include "FrameStatsOverlay.hpp"
#include "OpenGL3/OpenGLRenderer.hpp"
#include "SDL3/Sdl3Renderer.hpp"

#include "../../relive_lib/FatalError.hpp"
#include "../../relive_lib/Window.hpp"

#include "../Animation.hpp"
#include "../Primitives.hpp"

#include <cmath>
#include <utility>

static IRenderer* gRenderer = nullptr;

IRenderer* IRenderer::GetRenderer()
{
    return gRenderer;
}

template<typename T>
static void MakeRenderer(Window& window, bool checks)
{
    TRACE_ENTRYEXIT;
    try
    {
        gRenderer = new T(window, checks);
    }
    catch (const std::exception& e)
    {
        LOG_ERROR("Failed to create renderer [%s]", e.what());
    }
}

bool IRenderer::CreateRenderer(Renderers type, Window& window, bool checks)
{
    if (gRenderer)
    {
        ALIVE_FATAL("Renderer already created");
    }

    switch (type)
    {
        case Renderers::Sdl3:
            LOG_INFO("Create SDL3 renderer");
            MakeRenderer<Sdl3Renderer>(window, checks);
            break;

        case Renderers::OpenGL:
            LOG_INFO("Create OpenGL renderer");
            MakeRenderer<OpenGLRenderer>(window, checks);
            break;

        default:
            ALIVE_FATAL("Unknown or unsupported renderer type");
            break;
    }
    return gRenderer != nullptr;
}

const char* IRenderer::TypeToString(Renderers type)
{
    switch (type)
    {
        case Renderers::Sdl3:
            return "SDL3";
        case Renderers::OpenGL:
            return "OpenGL";
    }
    return "Unknown";
}

bool IRenderer::TakeCapture(std::vector<u8>& rgbaPixels, s32& width, s32& height)
{
    if (!mCaptureReady)
    {
        return false;
    }

    rgbaPixels = std::move(mCapturePixels);
    width = mCaptureWidth;
    height = mCaptureHeight;

    mCapturePixels.clear();
    mCaptureReady = false;
    return true;
}

IRenderer::IRenderer(Window& window)
    : mWindow(window)
{
}

IRenderer::~IRenderer() = default;

void IRenderer::ShowFrameStats(std::unique_ptr<FrameStatsOverlay> frameStats)
{
    mFrameStats = std::move(frameStats);
}

void IRenderer::AddIdleTime(u64 ns)
{
    if (mFrameStats)
    {
        mFrameStats->AddIdleTime(ns);
    }
}

void IRenderer::DrawFrameStats()
{
    if (mFrameStats)
    {
        mFrameStats->Draw(*this);
    }
}

void IRenderer::CaptureIfRequested()
{
    if (!mCaptureRequested)
    {
        return;
    }

    ReadPsxFramebuffer(mCapturePixels, mCaptureWidth, mCaptureHeight);

    // Only the colour matters, the alpha channel holds whatever blending left there
    for (std::size_t i = 3; i < mCapturePixels.size(); i += 4)
    {
        mCapturePixels[i] = 255;
    }

    mCaptureRequested = false;
    mCaptureReady = true;
}

void IRenderer::StartFrame()
{
    if (mIsFirstStartFrame)
    {
        // Make the window visible only on the first frame otherwise you can see
        // some unclear framebuffer crap for a half second or so
        mWindow.Show();

        mIsFirstStartFrame = false;
    }
}

void IRenderer::FreeRenderer()
{
    delete gRenderer;
    gRenderer = nullptr;
}

void IRenderer::SetScreenOffset(const Prim_ScreenOffset& offset)
{
    mOffsetX = offset.field_C_xoff;
    mOffsetY = offset.field_E_yoff;
}

SDL_Rect IRenderer::GetFramebufferRect()
{
    SDL_Rect rect = {};

    s32 desiredW = kPsxFramebufferWidth;
    s32 desiredH = kPsxFramebufferHeight;

    if (!mUseOriginalResolution)
    {
        // If we're maintaining aspect ratio, then the framebuffer needs
        // to be equal to the size of the rect otherwise the result will
        // be a warped image
        if (mKeepAspectRatio)
        {
            SDL_Rect r = GetTargetDrawRect();

            desiredW = r.w;
            desiredH = r.h;
        }
        else
        {
            mWindow.GetSizeInPixels(desiredW, desiredH);
        }
    }

    rect.w = desiredW;
    rect.h = desiredH;

    return rect;
}

SDL_Rect IRenderer::GetTargetDrawRect()
{
    SDL_Rect rect = {};

    s32 wndWidth = 0;
    s32 wndHeight = 0;

    mWindow.GetSize(wndWidth, wndHeight);

    // Calculate the draw size, aspect ratio dealt with here
    rect.w = wndWidth;
    rect.h = wndHeight;

    if (mKeepAspectRatio)
    {
        if (3 * wndWidth > 4 * wndHeight)
        {
            rect.w = (wndHeight * 4) / 3;
        }
        else
        {
            rect.h = (wndWidth * 3) / 4;
        }
    }

    // Calculate any screen shake. The offsets are in PSX framebuffer pixels (640x240).
    const s32 shakeX = static_cast<s32>(mOffsetX * (rect.w / static_cast<f32>(kPsxFramebufferWidth)));
    const s32 shakeY = static_cast<s32>(mOffsetY * (rect.h / static_cast<f32>(kPsxFramebufferHeight)));

    rect.x = shakeX + ((wndWidth - rect.w) / 2);
    rect.y = shakeY + ((wndHeight - rect.h) / 2);

    return rect;
}

IRenderer::QuadUVs IRenderer::GetAnimUVs(const Poly_FT4& poly)
{
    const PerFrameInfo* pHeader = poly.mAnim->Get_FrameHeader(-1);

    QuadUVs uvs;
    uvs.u0 = static_cast<f32>(pHeader->mSpriteSheetX);
    uvs.v0 = static_cast<f32>(pHeader->mSpriteSheetY);
    uvs.u1 = uvs.u0 + pHeader->mSpriteWidth - 1;
    uvs.v1 = uvs.v0 + pHeader->mSpriteHeight - 1;

    if (poly.mFlipX)
    {
        std::swap(uvs.u0, uvs.u1);
    }

    if (poly.mFlipY)
    {
        std::swap(uvs.v0, uvs.v1);
    }

    return uvs;
}

IRenderer::QuadUVs IRenderer::GetFontUVs(const Poly_FT4& poly)
{
    return {static_cast<f32>(poly.U0()), static_cast<f32>(poly.V0()), static_cast<f32>(poly.U3()), static_cast<f32>(poly.V3())};
}

bool IRenderer::IsScissorDisabled(const Prim_ScissorRect& clipper)
{
    return clipper.mRect.x == 0 && clipper.mRect.y == 0 && clipper.mRect.w == 1 && clipper.mRect.h == 1;
}

IRenderer::Quad2D IRenderer::LineToQuad(const Point2D& p1, const Point2D& p2)
{
    constexpr f32 halfPi = 1.57f;
    constexpr f32 halfThickness = 0.5f;

    // Always orient the line to be drawn left to right, so don't need to faff
    // with trig more than necessary
    Point2D leftPoint = p1;
    Point2D rightPoint = p2;

    if (p1.x > p2.x)
    {
        leftPoint = p2;
        rightPoint = p1;
    }

    // Our trig expands the line out in both directions, so the actual line
    // itself is 'centered' - here we push the line out by half the thickness
    // so that the expansion lands on, or close to, whole number pixel values
    f32 x0 = (f32) leftPoint.x + halfThickness;
    f32 y0 = (f32) leftPoint.y + halfThickness;

    f32 x1 = (f32) rightPoint.x + halfThickness;
    f32 y1 = (f32) rightPoint.y + halfThickness;

    // Our trig here, we expand the line into a quad, the normals are for the
    // thickness along the line, and the tangent is used for the thickness
    // on either end of the line
    f32 dx = x1 - x0;
    f32 dy = y1 - y0;

    f32 angle = std::atan(dy / dx);
    f32 normal = halfPi - angle;

    f32 dxTargetTangent = halfThickness * std::cos(angle);
    f32 dyTargetTangent = halfThickness * std::sin(angle);
    f32 dxTargetNormal = halfThickness * std::cos(normal);
    f32 dyTargetNormal = halfThickness * std::sin(normal);

    f32 finalX0 = x0 + dxTargetNormal - dxTargetTangent;
    f32 finalY0 = y0 - dyTargetNormal - dyTargetTangent;

    f32 finalX1 = x0 - dxTargetNormal - dxTargetTangent;
    f32 finalY1 = y0 + dyTargetNormal - dyTargetTangent;

    f32 finalX2 = x1 + dxTargetNormal + dxTargetTangent;
    f32 finalY2 = y1 - dyTargetNormal + dyTargetTangent;

    f32 finalX3 = x1 - dxTargetNormal + dxTargetTangent;
    f32 finalY3 = y1 + dyTargetNormal + dyTargetTangent;

    // The quad is like so (original line in the center):
    //
    // xy0 ---------- xy2
    //  | \__________/ |
    //  | /          \ |
    // xy1 ---------- xy3
    //
    // The first two corners are at p1's end and the last two at p2's, so that colours given
    // per end go the right way however the line was drawn
    if (p1.x > p2.x)
    {
        return {
            {
                { finalX2, finalY2 },
                { finalX3, finalY3 },
                { finalX0, finalY0 },
                { finalX1, finalY1 }
            }
        };
    }

    return {
        {
            { finalX0, finalY0 },
            { finalX1, finalY1 },
            { finalX2, finalY2 },
            { finalX3, finalY3 }
        }
    };
}
