#pragma once

#include "../Types.hpp"
#include "../Font.hpp"

class IRenderer;
class ResourceManagerWrapper;

// The renderer, frame rate, frame time, draw calls and cached textures, in the top right of the
// screen (-ddfps, and the render test). The frame time leaves out time spent waiting for the next
// frame's turn, which whatever waits reports with AddIdleTime. Averaged over half a second.
class FrameStatsOverlay final
{
public:
    explicit FrameStatsOverlay(ResourceManagerWrapper& resMan);

    void AddIdleTime(u64 ns)
    {
        mIdleNs += ns;
    }

    // Once a frame, as the renderer finishes it
    void Draw(IRenderer& renderer);

private:
    void UpdateTimes();
    // Lays the text out in the font's quads from polyOffset on, returning where they end
    s32 LayOut(const char* text, s32 x, s32 y, u8 r, u8 g, u8 b, s32 polyOffset);
    s32 Width(const char* text);

    FontContext mFontContext;
    AliveFont mFont;

    u64 mLastFrameNs = 0;
    u64 mIdleNs = 0;
    u64 mWindowStartNs = 0;
    u64 mWindowWorkNs = 0;
    u32 mWindowFrames = 0;

    f64 mFps = 0.0;
    f64 mFrameMs = 0.0;
};
