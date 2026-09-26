#include "../stdafx.h"
#include "FrameStatsOverlay.hpp"
#include "IRenderer.hpp"
#include "../Primitives.hpp"
#include "../Layer.hpp"
#include "../../AliveLibAE/PsxRender.hpp"
#include <algorithm>
#include <cstdio>

FrameStatsOverlay::FrameStatsOverlay(ResourceManagerWrapper& resMan)
{
    mFontContext.LoadFontType(FontType::Debug, resMan);

    PalResource pal;
    pal.mPal = mFontContext.mFntResource.mCurPal;
    mFont.Load(256, pal, &mFontContext);
}

void FrameStatsOverlay::UpdateTimes()
{
    const u64 nowNs = SDL_GetTicksNS();
    if (mLastFrameNs == 0)
    {
        mWindowStartNs = nowNs;
    }
    else
    {
        const u64 frameNs = nowNs - mLastFrameNs;
        mWindowWorkNs += frameNs > mIdleNs ? frameNs - mIdleNs : 0;
        mWindowFrames++;
    }
    mLastFrameNs = nowNs;
    mIdleNs = 0;

    constexpr u64 kWindowNs = 500 * 1000 * 1000;
    const u64 windowNs = nowNs - mWindowStartNs;
    if (windowNs >= kWindowNs && mWindowFrames > 0)
    {
        mFps = mWindowFrames * 1e9 / static_cast<f64>(windowNs);
        mFrameMs = mWindowWorkNs / 1e6 / mWindowFrames;
        mWindowStartNs = nowNs;
        mWindowWorkNs = 0;
        mWindowFrames = 0;
    }
}

s32 FrameStatsOverlay::LayOut(const char* text, s32 x, s32 y, u8 r, u8 g, u8 b, s32 polyOffset)
{
    OrderingTable unused;
    return mFont.DrawString(unused, text, x, static_cast<s16>(y), relive::TBlendModes::eBlend_0, 0, 0, Layer::eLayer_Text_42, r, g, b, polyOffset, FP_FromInteger(1), 10000, 0);
}

s32 FrameStatsOverlay::Width(const char* text)
{
    // What DrawString actually lays out: MeasureTextWidth doesn't size spaces the same way in AO
    const s32 count = LayOut(text, 0, 0, 0, 0, 0, 0);
    s32 width = 0;
    for (s32 i = 0; i < count; i++)
    {
        width = std::max<s32>(width, mFont.mFntPolyArray[i].X1());
    }
    return width;
}

void FrameStatsOverlay::Draw(IRenderer& renderer)
{
    UpdateTimes();

    const IRenderer::FrameStats& stats = renderer.GetLastFrameStats();

    char text[128] = {};
    snprintf(text, sizeof(text), "%s %.1f fps %.2f ms %u calls %u tex",
             IRenderer::TypeToString(renderer.GetType()), mFps, mFrameMs, stats.mDrawCalls, stats.mCachedTextures);

    // A black shadow, then the text, right aligned. Drawn straight to the renderer so they go over
    // everything the frame drew, whatever clip rectangle or screen shake it left set.
    const bool screenSpace = gFontDrawScreenSpace;
    gFontDrawScreenSpace = true;
    const s32 x = IRenderer::kPsxFramebufferWidth - 4 - Width(text);
    s32 polyCount = LayOut(text, x + 1, 3, 0, 0, 0, 0);
    polyCount = LayOut(text, x, 2, 0, 127, 127, polyCount);
    gFontDrawScreenSpace = screenSpace;

    Prim_ScissorRect noClip;
    noClip.SetRect({0, 0, 1, 1});
    renderer.SetClip(noClip);
    Prim_ScreenOffset noOffset;
    renderer.SetScreenOffset(noOffset);

    for (s32 i = 0; i < polyCount; i++)
    {
        renderer.Draw(mFont.mFntPolyArray[i]);
    }
}
