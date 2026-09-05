#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

namespace relive
{
enum class EventType
{
    Fmv,
    Animation,
    Effect,
    ShowMenu,
    ConditionalFmv,
    GotoCamera,
    Unknown
};

enum class EffectType
{
    CircularFadeout,
    FadeToBlack,
    Pixelate,
    None
};

enum class ButtonActionType
{
    StartGame,
    ExitGame,
    GotoCamera,
    InputList,
    GameSpeak,
    RestartGame,
    Unknown
};

enum class InputListType
{
    Saves,
    Demos,
    Levels,
    None
};

enum class CameraType
{
    Menu,
    GameSpeak
};

enum class ModifierKey
{
    Run,
    Shift,
    None
};

enum class StartModifier
{
    InfiniteGrenades,
    Bulletproof,
    GodMode,
    DebugMode,
    Unknown
};

struct MenuVec2 final
{
    s32 x = 0;
    s32 y = 0;
};

struct Event final
{
    EventType type = EventType::Unknown;
    EffectType effect = EffectType::None;

    std::string id;
    f32 duration = 0.0f;
    s32 speed = 0;
    std::string camera;

    struct Condition
    {
        std::string expr;
        std::string play;
    };

    std::vector<Condition> conditions;
};

struct ButtonAction final
{
    ButtonActionType type = ButtonActionType::Unknown;

    std::string camera;
    InputListType list_type = InputListType::None;
    MenuVec2 list_position;

    std::string start_level;
    std::string start_camera;

    s32 set_muds_saved = -1;
    s32 set_muds_killed = -1;

    std::vector<StartModifier> start_modifiers;

    std::string sound;
    std::string animation;
    std::string exit_animation;
    std::string return_to_camera;
};

struct Button final
{
    std::string id;
    std::string label;
    MenuVec2 position;
    std::string animation;
    std::string sound;

    ModifierKey modifier = ModifierKey::None;

    ButtonAction action;
};

struct MenuButtons final
{
    std::string default_button;
    std::vector<Button> buttons;
};

struct Camera final
{
    CameraType type = CameraType::Menu;
    bool ignore_input_for_timeout = false;

    std::vector<Event> on_enter;
    std::vector<Event> on_exit;

    std::optional<MenuButtons> menu;
};

struct CheatCode final
{
    std::string id;
    std::vector<std::string> sequence;
    std::string modifier;
    std::string goto_camera;
    std::vector<std::string> active_in;
};

struct TimeoutRule final
{
    std::string camera;
    s32 after_seconds = 0;
    std::string goto_camera;
};

struct MenuConfig final
{
    std::string start_camera;

    std::vector<CheatCode> cheat_codes;
    std::vector<TimeoutRule> timeouts;

    std::unordered_map<std::string, Camera> cameras;
};

MenuConfig LoadMenuConfig(const std::string& filename);
}
