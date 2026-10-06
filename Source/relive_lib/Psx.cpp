#include "stdafx.h"
#include "Psx.hpp"
#include "Sound/SDLSoundSystem.hpp"
#include "PsxDisplay.hpp"
#include "Renderer/IRenderer.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "../AliveLibAE/GameAutoPlayer.hpp"
#include "Sys.hpp"

// When the last frame's turn came, and how far apart turns are (0: no limit)
static u64 sVSyncLastNs = 0;
static u64 sFrameIntervalNs = 1000000000ull / 30;
static TPsxEmuCallBack sPsxEmu_put_disp_env_callback_C1D184 = nullptr;

bool gTurnOffRendering = false;

void PSX_EMU_SetCallBack_4F9430(TPsxEmuCallBack fnPtr)
{
    sPsxEmu_put_disp_env_callback_C1D184 = fnPtr;
}

static void PSX_PutDispEnv_Impl_4F5640()
{
    if (!gTurnOffRendering)
    {
        IRenderer::GetRenderer()->Clear(0, 0, 0);
        IRenderer::GetRenderer()->EndFrame();
    }
}

void PSX_PutDispEnv_4F5890()
{
    if (sPsxEmu_put_disp_env_callback_C1D184)
    {
        if (sPsxEmu_put_disp_env_callback_C1D184(0))
        {
            return;
        }
    }

    PSX_PutDispEnv_Impl_4F5640();

    if (sPsxEmu_put_disp_env_callback_C1D184)
    {
        sPsxEmu_put_disp_env_callback_C1D184(1);
    }
}

bool PSX_Rects_overlap_4FA0B0(const PSX_RECT* pRect1, const PSX_RECT* pRect2)
{
    return pRect1->x < (pRect2->x + pRect2->w)
        && pRect1->y < (pRect2->y + pRect2->h)
        && pRect2->x < (pRect1->x + pRect1->w)
        && pRect2->y < (pRect1->y + pRect1->h);
}


void PSX_Prevent_Rendering()
{
    gTurnOffRendering = true;
}


void PSX_SetMaxFps(u32 maxFps)
{
    sFrameIntervalNs = maxFps == 0 ? 0 : 1000000000ull / maxFps;
}

void PSX_VSync(VSyncMode mode)
{
    if (mode == VSyncMode::LimitFps)
    {
        SDLSoundSystem::DumpFrame();
    }

    const u64 nowNs = SDL_GetTicksNS();
    if (!sVSyncLastNs)
    {
        sVSyncLastNs = nowNs;
    }

    if (mode != VSyncMode::LimitFps || sFrameIntervalNs == 0 || nowNs - sVSyncLastNs >= sFrameIntervalNs)
    {
        // Late, or not limited: the next frame's turn is counted from now
        sVSyncLastNs = nowNs;
        return;
    }

    const u64 turnNs = sVSyncLastNs + sFrameIntervalNs;
    while (SDL_GetTicksNS() < turnNs)
    {
        // Not a busy wait, except during recording or playback: an undeterminate number of
        // delays could leak to de-syncs.
        if (!GetGameAutoPlayer().IsRecording() && !GetGameAutoPlayer().IsPlaying())
        {
            SDL_Delay(1);
        }
    }

    if (IRenderer* pRenderer = IRenderer::GetRenderer())
    {
        pRenderer->AddIdleTime(SDL_GetTicksNS() - nowNs);
    }
    sVSyncLastNs = turnNs;
}
