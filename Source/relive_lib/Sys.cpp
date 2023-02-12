#include "stdafx.h"
#include "Sys.hpp"
#include "Function.hpp"
#include "../AliveLibAE/Input.hpp"
#include <assert.h>
#include "../relive/resource.h"
#include "Renderer/IRenderer.hpp"

#include "Sound/Sound.hpp"
#include "Sound/Midi.hpp"
#include "../AliveLibAE/PauseMenu.hpp"
#include "../AliveLibAE/GameAutoPlayer.hpp"
#include "relive_config.h"

static bool sAppIsActivated = false;
static TWindowHandleType sHwnd = nullptr;

#if AUTO_SWITCH_CONTROLLER // OG Change - Used for Auto-switching active controller (gamepad/keyboard)
static int totalConnectedJoysticks = 0;
#endif

#if ORIGINAL_PS1_BEHAVIOR // OG Change - Allow for exiting save menu using controller
static bool saveMenuOpen = false;

void setSaveMenuOpen(bool val)
{
    saveMenuOpen = val;
}
#endif

std::string BuildString()
{
#ifdef BUILD_NUMBER
    // Automated AppVeyor build title
    return std::string("(") + CI_PROVIDER + " Build: " + std::to_string(BUILD_NUMBER) + ")";
#else
    return "";
#endif
}

std::string BuildAndBitnesString()
{
    std::string buildAndBitness;
    std::string buildStr = BuildString();
    if (!buildStr.empty())
    {
        buildAndBitness += " ";
        buildAndBitness += buildStr;
    }

    std::string kBitness = sizeof(void*) == 4 ? " (32 bit)" : " (64 bit)";
    buildAndBitness += kBitness;
    return buildAndBitness;
}

std::string WindowTitleAO()
{
    return "R.E.L.I.V.E. Oddworld Abe's Oddysee" + BuildAndBitnesString();
}

std::string WindowTitleAE()
{
    return "R.E.L.I.V.E. Oddworld Abe's Exoddus" + BuildAndBitnesString();
}

TWindowHandleType Sys_GetHWnd()
{
    return sHwnd;
}

bool Sys_IsAnyKeyDown()
{
    return sIsAKeyDown;
}

void Sys_SetWindowText(TWindowHandleType windowHandle, const char_type* title)
{
    SDL_SetWindowTitle(windowHandle, title);
}

bool Sys_IsMouseButtonDown(MouseButtons button)
{
    if (button == MouseButtons::eRight)
    {
        return !!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_RIGHT));
    }
    return !!(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT));
}

SoundEntry* gMovieSoundEntry = nullptr;

static VK SDLKeyToVK(SDL_Scancode key)
{
    switch (key)
    {
        case SDL_SCANCODE_A:
            return ea;
        case SDL_SCANCODE_B:
            return eb;
        case SDL_SCANCODE_C:
            return ec;
        case SDL_SCANCODE_D:
            return ed;
        case SDL_SCANCODE_E:
            return ee;
        case SDL_SCANCODE_F:
            return ef;
        case SDL_SCANCODE_G:
            return eg;
        case SDL_SCANCODE_H:
            return eh;
        case SDL_SCANCODE_I:
            return ei;
        case SDL_SCANCODE_J:
            return ej;
        case SDL_SCANCODE_K:
            return ek;
        case SDL_SCANCODE_L:
            return el;
        case SDL_SCANCODE_M:
            return em;
        case SDL_SCANCODE_N:
            return en;
        case SDL_SCANCODE_O:
            return eo;
        case SDL_SCANCODE_P:
            return ep;
        case SDL_SCANCODE_Q:
            return eq;
        case SDL_SCANCODE_R:
            return er;
        case SDL_SCANCODE_S:
            return es;
        case SDL_SCANCODE_T:
            return et;
        case SDL_SCANCODE_U:
            return eu;
        case SDL_SCANCODE_V:
            return ev;
        case SDL_SCANCODE_W:
            return ew;
        case SDL_SCANCODE_X:
            return ex;
        case SDL_SCANCODE_Y:
            return ey;
        case SDL_SCANCODE_Z:
            return ez;
        case SDL_SCANCODE_1:
            return eNUMPAD1;
        case SDL_SCANCODE_2:
            return eNUMPAD2;
        case SDL_SCANCODE_3:
            return eNUMPAD3;
        case SDL_SCANCODE_4:
            return eNUMPAD4;
        case SDL_SCANCODE_5:
            return eNUMPAD5;
        case SDL_SCANCODE_6:
            return eNUMPAD6;
        case SDL_SCANCODE_7:
            return eNUMPAD7;
        case SDL_SCANCODE_8:
            return eNUMPAD8;
        case SDL_SCANCODE_9:
            return eNUMPAD9;
        case SDL_SCANCODE_0:
            return eNUMPAD0;
        case SDL_SCANCODE_RETURN:
            return eRETURN;
        case SDL_SCANCODE_ESCAPE:
            return eESCAPE;
        case SDL_SCANCODE_BACKSPACE:
            return eBACK;
        case SDL_SCANCODE_TAB:
            return eTAB;
        case SDL_SCANCODE_SPACE:
            return eSPACE;
        case SDL_SCANCODE_MINUS:
            return eSUBTRACT;
        case SDL_SCANCODE_CAPSLOCK:
            return eCAPSLOCK;
        case SDL_SCANCODE_F1:
            return eF1;
        case SDL_SCANCODE_F2:
            return eF2;
        case SDL_SCANCODE_F3:
            return eF3;
        case SDL_SCANCODE_F4:
            return eF4;
        case SDL_SCANCODE_F5:
            return eF5;
        case SDL_SCANCODE_F6:
            return eF6;
        case SDL_SCANCODE_F7:
            return eF7;
        case SDL_SCANCODE_F8:
            return eF8;
        case SDL_SCANCODE_F9:
            return eF9;
        case SDL_SCANCODE_F10:
            return eF10;
        case SDL_SCANCODE_F11:
            return eF11;
        case SDL_SCANCODE_F12:
            return eF12;
        case SDL_SCANCODE_PRINTSCREEN:
            return ePRINT;
        case SDL_SCANCODE_SCROLLLOCK:
            return eSCROLL;
        case SDL_SCANCODE_PAUSE:
            return ePAUSE;
        case SDL_SCANCODE_INSERT:
            return eINSERT;
        case SDL_SCANCODE_HOME:
            return eHOME;
        case SDL_SCANCODE_PAGEUP:
            return ePRIOR;
        case SDL_SCANCODE_DELETE:
            return eDELETE;
        case SDL_SCANCODE_END:
            return eEND;
        case SDL_SCANCODE_PAGEDOWN:
            return eNEXT;
        case SDL_SCANCODE_RIGHT:
            return eRIGHT;
        case SDL_SCANCODE_LEFT:
            return eLEFT;
        case SDL_SCANCODE_DOWN:
            return eDOWN;
        case SDL_SCANCODE_UP:
            return eUP;
        case SDL_SCANCODE_KP_MINUS:
            return eSUBTRACT;
        case SDL_SCANCODE_KP_ENTER:
            return eRETURN;
        case SDL_SCANCODE_KP_1:
            return eNUMPAD1;
        case SDL_SCANCODE_KP_2:
            return eNUMPAD2;
        case SDL_SCANCODE_KP_3:
            return eNUMPAD3;
        case SDL_SCANCODE_KP_4:
            return eNUMPAD4;
        case SDL_SCANCODE_KP_5:
            return eNUMPAD5;
        case SDL_SCANCODE_KP_6:
            return eNUMPAD6;
        case SDL_SCANCODE_KP_7:
            return eNUMPAD7;
        case SDL_SCANCODE_KP_8:
            return eNUMPAD8;
        case SDL_SCANCODE_KP_9:
            return eNUMPAD9;
        case SDL_SCANCODE_KP_0:
            return eNUMPAD0;
        case SDL_SCANCODE_LCTRL:
            return eCONTROL;
        case SDL_SCANCODE_LSHIFT:
            return eSHIFT;
        case SDL_SCANCODE_LALT:
            return eALT;
        case SDL_SCANCODE_LGUI:
            return eLSUPER;
        case SDL_SCANCODE_RCTRL:
            return eCONTROL;
        case SDL_SCANCODE_RSHIFT:
            return eSHIFT;
        case SDL_SCANCODE_RALT:
            return eALT;
        case SDL_SCANCODE_RGUI:
            return eRSUPER;
        default:
            return eNone;
    }
}

static VKMOD SDLModtoVKMOD(SDL_Keymod key)
{
    switch (key)
    {
        case KMOD_LCTRL:
            return eVKMOD_LCTRL;
        case KMOD_RCTRL:
            return eVKMOD_RCTRL;
        case KMOD_LSHIFT:
            return eVKMOD_LSHIFT;
        case KMOD_RSHIFT:
            return eKMOD_RSHIFT;
        case KMOD_LALT:
            return eKMOD_LALT;
        case KMOD_RALT:
            return eKMOD_RALT;
        case KMOD_CAPS:
            return eKMOD_CAPS;
        default:
            return eNoMod;
    }
}

// This is a combination of the window proc and the window proc "filter"

static bool bNeedToQuit = false;

bool Sys_IsAppActive()
{
    return sAppIsActivated;
}

static void KeyDownEvent(VK vk, VKMOD vkMod)
{
#if ORIGINAL_PS1_BEHAVIOR                  // OG Change - Allow for exiting save menu using controller
    const bool allowTyping = saveMenuOpen; // Allow typing if save menu is open
#else
    const bool allowTyping = !Input_GetInputEnabled_4EDDE0(); // Old method: Allow typing only if all other inputs disabled
#endif
    if (allowTyping)
    {
        if (vk >= eF1 && vk <= eF12)
        {
            return;
        }
        sLastPressedKey = vk;

        // LOG_INFO("Key down (input disabled) " << sLastPressedKey);

        // Between A-Z
        if (sLastPressedKey >= ea && sLastPressedKey <= ez)
        {
            if (vkMod & (eKMOD_SHIFT | eKMOD_CAPS))
            {
                sLastPressedKey = static_cast<VK>(sLastPressedKey - ea + eA);
            }
        }
        sIsAKeyDown = true;
    }
    else
    {
        Input_SetKeyState(vk, 1);

        if (vk == eF5)
        {
            LOG_INFO("Save next frame for %d", eF5);
            gQuicksave_SaveNextFrame = true;
        }
        else if (vk == eF6)
        {
            LOG_INFO("Load next frame for %d", eF6);
            gQuicksave_LoadNextFrame = true;
        }
        else if (vk == eF10)
        {
            IRenderer::GetRenderer()->ToggleFilterScreen();
        }
        else if (vk == eF11)
        {
            IRenderer::GetRenderer()->ToggleKeepAspectRatio();
        }
        else if (vk == eF12)
        {
            const Uint32 flags = SDL_GetWindowFlags(Sys_GetWindowHandle());
            if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
            {
                SDL_SetWindowFullscreen(Sys_GetWindowHandle(), 0);
            }
            else
            {
                SDL_SetWindowFullscreen(Sys_GetWindowHandle(), SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
        }
    }


    // Between 0-9
    if (sLastPressedKey >= eNUMPAD0 && sLastPressedKey <= eNUMPAD9)
    {
        sLastPressedKey = static_cast<VK>(sLastPressedKey - eNUMPAD0);
        sLastPressedKey = static_cast<VK>(sLastPressedKey + e0);

        if (vkMod & (eKMOD_SHIFT) && sLastPressedKey == e1)
        {
            sLastPressedKey = eEXCLAMATION;
        }
    }
}

static void KeyUpEvent(VK vk)
{
    Input_SetKeyState(vk, 0);
    sIsAKeyDown = false;
    sLastPressedKey = eNone;
}

static void QuitEvent(bool isRecordedEvent, bool isRecording)
{
    if (gMovieSoundEntry)
    {
#if !USE_SDL2_SOUND
        LPDIRECTSOUNDBUFFER pDSoundBuffer = gMovieSoundEntry->field_4_pDSoundBuffer;
        if (pDSoundBuffer)
        {
            pDSoundBuffer->Stop();
        }
#endif
    }
    if (SND_Seq_Table_Valid())
    {
        SND_StopAll();
    }

    // Full screen message boxes act really strange.. so force window mode before we show it
    const Uint32 flags = SDL_GetWindowFlags(Sys_GetWindowHandle());
    bool forcedWindowMode = false;
    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
    {
        forcedWindowMode = true;
        SDL_SetWindowFullscreen(Sys_GetWindowHandle(), 0);
        //RECT rect = {0, 0, 640, 240};
        //VGA_EndFrame(&rect);
    }

    bool actuallyQuit = false;
    if (isRecordedEvent)
    {
        // Reading the SDL_QUIT event
        const RecordedEvent recordedEvent = GetGameAutoPlayer().GetEvent();
        actuallyQuit = recordedEvent.mData ? true : false;
    }

    const MessageBoxButton recordedButtonResult = actuallyQuit ? MessageBoxButton::eYes : MessageBoxButton::eNo;
    const MessageBoxButton button = isRecordedEvent ? recordedButtonResult : Sys_MessageBox(Sys_GetWindowHandle(), "Do you really want to quit?", "R.E.L.I.V.E.", MessageBoxType::eQuestion);

    if (isRecording)
    {
        RecordedEvent recEvent;
        recEvent.mType = SDL_QUIT;
        recEvent.mData = button == MessageBoxButton::eYes ? 1 : 0;
        GetGameAutoPlayer().RecordEvent(recEvent);
    }

    if (SND_Seq_Table_Valid())
    {
        GetSoundAPI().mSND_Restart();
    }

#if !USE_SDL2_SOUND
    if (gMovieSoundEntry && gMovieSoundEntry->field_4_pDSoundBuffer)
    {
        gMovieSoundEntry->field_4_pDSoundBuffer->Play(0, 0, 1);
    }
#endif

    if (button == MessageBoxButton::eYes)
    {
        // So Sys_PumpMessages thinks we got an quit
        bNeedToQuit = true;
    }
    else
    {
        if (forcedWindowMode)
        {
            SDL_SetWindowFullscreen(Sys_GetWindowHandle(), SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
    }
}

s8 Sys_PumpMessages()
{
    GetGameAutoPlayer().SyncPoint(SyncPoints::PumpEventsStart);

    SDL_Event event = {};
    const bool isRecording = GetGameAutoPlayer().IsRecording();
    const bool isPlaying = GetGameAutoPlayer().IsPlaying();

    // Replay any recorded events
    if (isPlaying)
    {
        while (GetGameAutoPlayer().PeekNextType() == RecordTypes::EventPoint)
        {
            const RecordedEvent recordedEvent = GetGameAutoPlayer().GetEvent();
            // TODO: Recording SDL types directly might break across diff platforms/SDL2 versions/ports
            switch (recordedEvent.mType)
            {
                case SDL_KEYDOWN:
                    KeyDownEvent(SDLKeyToVK(static_cast<SDL_Scancode>(recordedEvent.mData)), SDLModtoVKMOD(SDL_GetModState()));
                    break;

                case SDL_KEYUP:
                    KeyUpEvent(SDLKeyToVK(static_cast<SDL_Scancode>(recordedEvent.mData)));
                    break;

                case 0:
                    // Hack for quit events, quit writes other events before the actual event data
                    // this is used to mark the start
                    QuitEvent(true, false);
                    break;

                default:
                    ALIVE_FATAL("Unknown event type %d", recordedEvent.mType);
            }
        }
    }

    // Even if playing back we still need to pump events for things to actually work, but we ignore
    // inputs. Any attempt to quit while playing back is an instant quit to avoid desyncs.
    while (SDL_PollEvent(&event))
    {
#if AUTO_SWITCH_CONTROLLER // OG Change - Automatically switches active controller (gamepad/keyboard)
        // Auto switch off during recording or playback as reading the ini
        // file at random times will desync.
        const bool allowAutoSwitch = !isRecording && !isPlaying;
        if (allowAutoSwitch)
        {
            if (event.type == SDL_JOYDEVICEADDED && !isRecording)
            {
                totalConnectedJoysticks++;
                LOG_INFO("User just inserted joystick!");
                Input_Init();
                Input().SetJoyStickEnabled(true);
            }
            else if (event.type == SDL_JOYDEVICEREMOVED && !isRecording)
            {
                totalConnectedJoysticks--;
                LOG_INFO("User just removed joystick!");

                if (totalConnectedJoysticks > 0)
                {
                    Input_Init(); // Ensures next joystick is usable
                }
                else
                {
                    Input().SetJoyStickEnabled(false);
                }
            }
        }
#endif // AUTO_SWITCH_CONTROLLER

        if (event.type == SDL_KEYDOWN)
        {
            if (!isPlaying)
            {
                KeyDownEvent(SDLKeyToVK(event.key.keysym.scancode), SDLModtoVKMOD(SDL_GetModState()));
            }

            if (isRecording)
            {
                RecordedEvent recEvent;
                recEvent.mType = event.type;
                recEvent.mData = static_cast<u32>(event.key.keysym.scancode);
                GetGameAutoPlayer().RecordEvent(recEvent);
            }
        }
        else if (event.type == SDL_KEYUP)
        {
            if (!isPlaying)
            {
                KeyUpEvent(SDLKeyToVK(event.key.keysym.scancode));
            }

            if (isRecording)
            {
                RecordedEvent recEvent;
                recEvent.mType = event.type;
                recEvent.mData = static_cast<u32>(event.key.keysym.scancode);
                GetGameAutoPlayer().RecordEvent(recEvent);
            }
        }
        else if (event.type == SDL_WINDOWEVENT)
        {
            if (event.window.type == SDL_WINDOWEVENT_FOCUS_GAINED)
            {
                sAppIsActivated = true;
            }
            else if (event.window.type == SDL_WINDOWEVENT_FOCUS_LOST)
            {
                sAppIsActivated = false;
            }
            else if (event.window.type == SDL_WINDOWEVENT_EXPOSED)
            {
                // Add_Dirty_Area_4ED970(0, 0, 640, 240);
            }
            // SDL_WINDOWEVENT_SIZE_CHANGED
        }
        else if (event.type == SDL_QUIT)
        {
            if (!isPlaying)
            {
                // Required to write a dummy event first because the QuitEvent can write other events first
                // which breaks the playback logic of Peeking and looping until the type isn't event.
                if (isRecording)
                {
                    RecordedEvent recEvent;
                    recEvent.mType = 0;
                    recEvent.mData = 0;
                    GetGameAutoPlayer().RecordEvent(recEvent);
                }

                // Else alllow normal quit behaviour + record the result
                QuitEvent(false, isRecording);
            }
            else
            {
                // Force quit if attempting to close the game during playback
                bNeedToQuit = true;
            }
        }
    }

    GetGameAutoPlayer().SyncPoint(SyncPoints::PumpEventsEnd);

    if (bNeedToQuit)
    {
        return 1;
    }

    return 0;
}

TWindowHandleType Sys_GetWindowHandle()
{
    return sHwnd;
}

void Sys_SetWindowPos_4EE1B1(s32 width, s32 height)
{
    SDL_SetWindowSize(Sys_GetWindowHandle(), width, height);
    SDL_SetWindowPosition(Sys_GetWindowHandle(), 0, 0);
}

void Sys_DestroyWindow()
{
    if (sHwnd)
    {
        SDL_DestroyWindow(sHwnd);
        sHwnd = nullptr;
    }
}

bool Sys_WindowClass_Register(const char_type* lpWindowName, s32 x, s32 y, s32 nWidth, s32 nHeight, s32 extraAttributes)
{
    TRACE_ENTRYEXIT;

    sHwnd = SDL_CreateWindow(lpWindowName, x, y, nWidth, nHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | extraAttributes);
    if (sHwnd)
    {
        LOG_INFO("Window created");

        Input_InitKeyStateArray_4EDD60();

        SDL_ShowCursor(SDL_DISABLE);

        // SDL will not send a window focused message on start up, so default to activated
        sAppIsActivated = true;
    }
    else
    {
        LOG_ERROR("Window create with flags %d failed with %s", extraAttributes, SDL_GetError());
    }
    return sHwnd != nullptr;
}


MessageBoxButton Sys_MessageBox(TWindowHandleType windowHandle, const char_type* message, const char_type* title, MessageBoxType type)
{
    SDL_MessageBoxData data = {};
    data.title = title;
    data.message = message;

    if (type == MessageBoxType::eQuestion)
    {
        const static SDL_MessageBoxButtonData buttons[] = {
            {0, 1, "No"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Yes"},
        };

        data.numbuttons = SDL_arraysize(buttons);
        data.buttons = buttons;
    }
    else
    {
        const static SDL_MessageBoxButtonData buttons[] = {
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK"},
        };

        data.numbuttons = SDL_arraysize(buttons);
        data.buttons = buttons;
    }

    data.window = windowHandle;

    switch (type)
    {
        case MessageBoxType::eStandard:
            data.flags = SDL_MESSAGEBOX_ERROR;
            break;
        case MessageBoxType::eError:
            data.flags = SDL_MESSAGEBOX_INFORMATION;
            break;
        case MessageBoxType::eQuestion:
            data.flags = SDL_MESSAGEBOX_WARNING;
            break;
        default:
            data.flags = SDL_MESSAGEBOX_ERROR;
    }

    s32 button = 0;

    SDL_ShowMessageBox(&data, &button);

    if (type == MessageBoxType::eQuestion)
    {
        if (button == 1)
        {
            return MessageBoxButton::eNo;
        }

        return MessageBoxButton::eYes;
    }

    return MessageBoxButton::eOK;
}
