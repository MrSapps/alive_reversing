#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include "DisplaySettings.hpp"

class BaseMap;
class ResourceManagerWrapper;
class Window;

enum class MessageBoxType
{
    eStandard,
    eError,
    eQuestion,
};

enum class MessageBoxButton
{
    eOK,
    eNo,
    eYes,
};

void Alive_Show_ErrorMsg(const char_type* fmt, ...);
u32 SYS_GetTicks();

// Turns SDL's events into game input and actions. The Engine owns the only instance and pumps
// it once per main loop iteration (see Engine::Game_Loop).
class Sys final
{
public:
    // resMan saves the display settings when the F9-F12 hotkeys change them, and reloads the
    // input settings when a joystick is plugged in. Events of pathReloadEventType carry a
    // path JSON file name the editor changed (see the Engine's IPC listener).
    Sys(Window& window, ResourceManagerWrapper& resMan, u32 pathReloadEventType);

    Sys(const Sys&) = delete;
    Sys& operator=(const Sys&) = delete;

    // Called once the window and renderer exist: applies the loaded display settings, which
    // the F9-F12 hotkeys then change.
    void SetDisplaySettings(const DisplaySettings& settings);

    enum class PumpResult
    {
        eContinue,
        eQuit, // The user confirmed they want to quit
    };

    // Handles every pending event. pMap, when given, restarts the music after the quit
    // question.
    PumpResult PumpEvents(BaseMap* pMap);

    // The path JSON files the editor has changed since this was last called, for the Engine to
    // reload (see BaseMap::ReloadPathJsonRequest)
    std::vector<std::string> TakePathReloadRequests();

    // pParent can be null, e.g. before the window exists
    static MessageBoxButton ShowMessageBox(const Window* pParent, const char_type* message, const char_type* title, MessageBoxType type = MessageBoxType::eStandard);

private:
    void KeyDownEvent(SDL_Scancode scanCode);
    void KeyUpEvent(SDL_Scancode scanCode);
    void QuitEvent(bool isRecordedEvent, bool isRecording, BaseMap* pMap);
    void DisplaySettingsChanged();

    Window& mWindow;
    ResourceManagerWrapper& mResMan;
    const u32 mPathReloadEventType;
    DisplaySettings mDisplaySettings;
    bool mQuitConfirmed = false;
    s32 mConnectedJoysticks = 0;
    std::vector<std::string> mPathReloadRequests;
};
