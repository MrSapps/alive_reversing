#include "stdafx.h"
#include "Text.hpp"
#include "../relive_lib/Function.hpp"
#include "stdlib.hpp"
#include "../relive_lib/PsxDisplay.hpp"
#include "Input.hpp"
#include "../relive_lib/Sound/Midi.hpp"
#include "../relive_lib/Sound/Sound.hpp"
#include "GameAutoPlayer.hpp"
#include "../relive_lib/ResourceManagerWrapper.hpp"
#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

void Text::VUpdate()
{
    // Empty
}

void Text::VScreenChanged()
{
    // Empty
}

Text::Text(const char_type* pMessage, s32 renderCount, s32 bShadow, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetSurviveDeathReset(true);
    SetDrawable(true);

    SetType(ReliveTypes::eText);

    gObjListDrawables->Push_Back(this);

    mFontContext.LoadFontType(FontType::PauseMenu, mResMan);
    mPal = mResMan.LoadPal(PalId::MainMenuFont_PauseMenu);
    field_20_font.Load(static_cast<s32>((bShadow + 1) * strlen(pMessage)), mPal, &mFontContext);

    field_5C_xpos = static_cast<s16>(field_20_font.MeasureTextWidth(pMessage));
    field_5E_ypos = 0;

    field_58_k0_unused = 0; // never used?
    field_5A_k4_unused = 4; // never used?

    memcpy(field_68_txt_buffer, pMessage, strlen(pMessage) + 1);

    field_60_bShadow = static_cast<s16>(bShadow);
    field_64_render_count = renderCount;
}


Text::~Text()
{
    gObjListDrawables->Remove_Item(this);
    SetDrawable(false);
}

void Text::SetYPos(s32 /*not_used*/, s16 ypos)
{
    field_5E_ypos = ypos;
}

void Text::VRender(OrderingTable& ot)
{
    const s16 ypos = field_5E_ypos + 100;
    const s32 xpos = (368 / 2) - (field_5C_xpos / 2);

    s32 drawRet1 = field_20_font.DrawString(
        ot,
        field_68_txt_buffer,
        xpos,
        ypos,
        relive::TBlendModes::eBlend_0,
        1,
        0,
        Layer::eLayer_Text_42,
        210,
        150,
        80,
        0,
        FP_FromDouble(1.0),
        640,
        0);

    // Draw again with offsets on x/y to create a shadow effect
    if (field_60_bShadow)
    {
        s32 drawRet2 = field_20_font.DrawString(
            ot,
            field_68_txt_buffer,
            xpos + 2,
            ypos + 2,
            relive::TBlendModes::eBlend_0,
            1,
            0,
            Layer::eLayer_Text_42,
            0,
            0,
            0,
            drawRet1,
            FP_FromDouble(1.0),
            640,
            0);

        field_20_font.DrawString(
            ot,
            field_68_txt_buffer,
            xpos - 1,
            ypos - 1,
            relive::TBlendModes::eBlend_0,
            1,
            0,
            Layer::eLayer_Text_42,
            0,
            0,
            0,
            drawRet2,
            FP_FromDouble(1.0),
            640,
            0);
    }

    if (field_64_render_count > 0)
    {
        field_64_render_count--;
        if (field_64_render_count <= 0)
        {
            SetDead(true);
        }
    }
}

// ResourceManager::vLoadFile_StateMachine_464A70 will call with type 0 (Displays Oddworld Abe's Exoddus.. but why?).
// Movie::vUpdate_4E0030 will call with type 1 which does nothing (trying to display movie skip message when it can't be found?).
// MainMenuController::LoadDemo_Update_4D1040 will call with type 2 (trying to display demo skip message when it can't be found?).
// MainMenuController::ChangeScreenAndIntroLogic_4CF640 will call with type 3 (Shown on boot, says Abe's Exoddus).
// Only type 2 is used now.
FullScreenMessage::FullScreenMessage(MessageType messageType, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetSurviveDeathReset(true);
    SetUpdateDuringCamSwap(true);

    mText = relive_new Text("       Oddworld Abe's Exoddus        ", 1, 0, resMan, map);

    switch (messageType)
    {
        case MessageType::eSkipDemo_2:
            mText2 = relive_new Text("or esc to skip the demo", 1, 0, resMan, map);
            if (mText2)
            {
                mText2->SetYPos(0, 30);
            }
            break;
    }

    mText->VRender(gPsxDisplay.mDrawEnv.mOrderingTable);

    if (mText2)
    {
        mText2->VRender(gPsxDisplay.mDrawEnv.mOrderingTable);
    }

    gDisplayRenderFrame = false;
    gPsxDisplay.RenderOrderingTable();

    if (SND_Seq_Table_Valid())
    {
        SND_StopAll();
    }

    mDisplayUntil = GetGameAutoPlayer().SysGetTicks() + 1000;

    if (GetGameAutoPlayer().IsRecording() || GetGameAutoPlayer().IsPlaying())
    {
        mDisplayUntil = 200; // Get rid of these quickly for recordings
    }

    if (GetGameAutoPlayer().SysGetTicks() >= mDisplayUntil)
    {
        Close();
        return;
    }

    StartModal();
}

FullScreenMessage::~FullScreenMessage()
{
    for (Text* pText : {mText, mText2})
    {
        if (pText)
        {
            gBaseGameObjects->Remove_Item(pText);
            relive_delete pText;
        }
    }
}

ModalState FullScreenMessage::VModalUpdate()
{
    switch (mState)
    {
        case State::eWaitForKey:
            if (Input_IsVKPressed_4EDD40(VK_RETURN))
            {
                // Wait for return to come back up, so it doesn't also press whatever comes next
                if (!GetGameAutoPlayer().IsRecording() && !GetGameAutoPlayer().IsPlaying())
                {
                    mState = State::eWaitForReturnRelease;
                }
                else
                {
                    Close();
                }
            }
            else if (Input_IsVKPressed_4EDD40(VK_ESCAPE))
            {
                // User quit
                mClosedResult = Result::eEscape;
                mState = State::eWaitForEscapeRelease;
            }
            else if (GetGameAutoPlayer().SysGetTicks() >= mDisplayUntil)
            {
                Close();
            }
            break;

        case State::eWaitForEscapeRelease:
            if (!Input_IsVKPressed_4EDD40(VK_ESCAPE))
            {
                Close();
            }
            break;

        case State::eWaitForReturnRelease:
            if (!Input_IsVKPressed_4EDD40(VK_RETURN))
            {
                Close();
            }
            break;
    }
    return mResult == Result::eOpen ? ModalState::eRunning : ModalState::eFinished;
}

void FullScreenMessage::Close()
{
    if (SND_Seq_Table_Valid())
    {
        GetSoundAPI().mSND_Restart(mMap);
    }

    gDisplayRenderFrame = false;
    gPsxDisplay.RenderOrderingTable();

    for (Text** ppText : {&mText, &mText2})
    {
        if (*ppText)
        {
            gBaseGameObjects->Remove_Item(*ppText);
            relive_delete *ppText;
            *ppText = nullptr;
        }
    }

    mResult = mClosedResult;
}
