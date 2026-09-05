#include "MenuConfig.hpp"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace relive
{
static EventType ToEventType(const std::string& s)
{
    if (s == "fmv") return EventType::Fmv;
    else if (s == "animation") return EventType::Animation;
    else if (s == "effect") return EventType::Effect;
    else if (s == "show_menu") return EventType::ShowMenu;
    else if (s == "conditional_fmv") return EventType::ConditionalFmv;
    else if (s == "goto_camera") return EventType::GotoCamera;
    else return EventType::Unknown;
}

static EffectType ToEffectType(const std::string& s)
{
    if (s == "circular_fadeout") return EffectType::CircularFadeout;
    else if (s == "fade_to_black") return EffectType::FadeToBlack;
    else if (s == "pixelate") return EffectType::Pixelate;
    else return EffectType::None;
}

static ButtonActionType ToButtonActionType(const std::string& s)
{
    if (s == "start_game") return ButtonActionType::StartGame;
    else if (s == "exit_game") return ButtonActionType::ExitGame;
    else if (s == "goto_camera") return ButtonActionType::GotoCamera;
    else if (s == "input_list") return ButtonActionType::InputList;
    else if (s == "gamespeak") return ButtonActionType::GameSpeak;
    else if (s == "restart_game") return ButtonActionType::RestartGame;
    else return ButtonActionType::Unknown;
}

static InputListType ToInputListType(const std::string& s)
{
    if (s == "saves") return InputListType::Saves;
    else if (s == "demos") return InputListType::Demos;
    else if (s == "levels") return InputListType::Levels;
    else return InputListType::None;
}

static CameraType ToCameraType(const std::string& s)
{
    if (s == "gamespeak") return CameraType::GameSpeak;
    else return CameraType::Menu;
}

static ModifierKey ToModifierKey(const std::string& s)
{
    if (s == "run") return ModifierKey::Run;
    else if (s == "shift") return ModifierKey::Shift;
    else return ModifierKey::None;
}

static StartModifier ToStartModifier(const std::string& s)
{
    if (s == "infinite_grenades") return StartModifier::InfiniteGrenades;
    else if (s == "bulletproof") return StartModifier::Bulletproof;
    else if (s == "god_mode") return StartModifier::GodMode;
    else if (s == "debug_mode") return StartModifier::DebugMode;
    else return StartModifier::Unknown;
}

static MenuVec2 LoadVec2(const json& j)
{
    MenuVec2 v;
    v.x = j.value("x", 0);
    v.y = j.value("y", 0);
    return v;
}

static Event LoadEvent(const json& j)
{
    Event e;
    e.type = ToEventType(j.value("type", ""));
    e.effect = ToEffectType(j.value("effect", ""));
    e.id = j.value("id", "");
    e.duration = j.value("duration", 0.0f);
    e.speed = j.value("speed", 0);
    e.camera = j.value("camera", "");

    if (j.contains("conditions"))
    {
        for (auto& c : j["conditions"])
        {
            Event::Condition cond;
            cond.expr = c.value("if", "");
            cond.play = c.value("play", "");
            e.conditions.push_back(cond);
        }
    }

    return e;
}

static ButtonAction LoadButtonAction(const json& j)
{
    ButtonAction a;
    a.type = ToButtonActionType(j.value("type", ""));

    a.camera = j.value("camera", "");
    a.list_type = ToInputListType(j.value("list_type", ""));

    if (j.contains("position"))
    {
        a.list_position = LoadVec2(j["position"]);
    }

    a.start_level = j.value("start_level", "");
    a.start_camera = j.value("start_camera", "");

    a.set_muds_saved = j.value("set_muds_saved", -1);
    a.set_muds_killed = j.value("set_muds_killed", -1);

    if (j.contains("start_modifiers"))
    {
        for (auto& m : j["start_modifiers"])
        {
            a.start_modifiers.push_back(ToStartModifier(m.get<std::string>()));
        }
    }

    a.sound = j.value("sound", "");
    a.animation = j.value("animation", "");
    a.exit_animation = j.value("exit_animation", "");
    a.return_to_camera = j.value("return_to_camera", "");

    return a;
}

static Button LoadButton(const json& j)
{
    Button b;
    b.id = j.value("id", "");
    b.label = j.value("label", "");
    b.position = LoadVec2(j["position"]);
    b.animation = j.value("animation", "");
    b.sound = j.value("sound", "");
    b.modifier = ToModifierKey(j.value("modifier", ""));
    b.action = LoadButtonAction(j["action"]);
    return b;
}

static MenuButtons LoadMenuButtons(const json& j)
{
    MenuButtons m;
    m.default_button = j.value("default_button", "");

    for (auto& btn : j["buttons"])
    {
        m.buttons.push_back(LoadButton(btn));
    }
    return m;
}

static Camera LoadCamera(const json& j)
{
    Camera c;
    c.type = ToCameraType(j.value("type", "menu"));
    c.ignore_input_for_timeout = j.value("ignore_input_for_timeout", false);

    if (j.contains("on_enter"))
    {
        for (auto& e : j["on_enter"]["sequence"])
        {
            c.on_enter.push_back(LoadEvent(e));
        }
    }

    if (j.contains("on_exit"))
    {
        for (auto& e : j["on_exit"]["sequence"])
        {
            c.on_exit.push_back(LoadEvent(e));
        }
    }

    if (j.contains("menu"))
    {
        c.menu = LoadMenuButtons(j["menu"]);
    }
    return c;
}

static CheatCode LoadCheat(const json& j)
{
    CheatCode cc;
    cc.id = j.value("id", "");
    cc.modifier = j.value("modifier", "");
    cc.goto_camera = j.value("goto_camera", "");

    for (auto& s : j["sequence"])
    {
        cc.sequence.push_back(s);
    }

    for (auto& a : j["active_in"])
    {
        cc.active_in.push_back(a);
    }
    return cc;
}

static TimeoutRule LoadTimeout(const json& j)
{
    TimeoutRule t;
    t.camera = j.value("camera", "");
    t.after_seconds = j.value("after_seconds", 0);
    t.goto_camera = j.value("goto_camera", "");
    return t;
}

MenuConfig LoadMenuConfig(const json& root)
{
    MenuConfig cfg;

    cfg.start_camera = root.value("start_camera", "");

    if (root.contains("global"))
    {
        if (root["global"].contains("cheat_codes"))
        {
            for (auto& cc : root["global"]["cheat_codes"])
            {
                cfg.cheat_codes.push_back(LoadCheat(cc));
            }
        }

        if (root["global"].contains("timeouts"))
        {
            for (auto& t : root["global"]["timeouts"])
            {
                cfg.timeouts.push_back(LoadTimeout(t));
            }
        }
    }

    for (auto& [name, cam] : root["cameras"].items())
    {
        cfg.cameras[name] = LoadCamera(cam);
    }
    return cfg;
}

MenuConfig LoadMenuConfig(const std::string& filename)
{
    // Read entire file
    std::ifstream ifs(filename);
    if (!ifs.is_open())
    {
        ALIVE_FATAL("Failed to open menu config file");
    }

    // Parse JSON
    json root;
    ifs >> root;

    // Delegate to existing loader
    return LoadMenuConfig(root);
}
}
