#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

// Ways to poke the running game's sound, for reproducing and fixing sound bugs: scripted
// (Automation's music, sfx, seq_play, seq_stop commands) and by hand (the "Sound" ImGui window,
// shown with the other debug windows).
//
// Music types are the running game's MusicController::MusicTypes: a number, or a name (see
// MusicNames). The game's own MusicController plays them, so its volume, ambient/music SEQ
// choice, timing and "back to ambience" timeout all run as in the game. A music type is asked for
// by Abe (the controlled character), as the game objects do.
class BaseMap;
class ResourceManagerWrapper;

class SoundDebug final
{
public:
    // Checks a command's arguments, setting error if they're wrong
    static bool Check(const std::string& name, const std::vector<std::string>& args, std::string& error);

    // Does the command: music <type> [<owner> [<flag4> [<flag20>]]], kill_other, sfx <index>
    // [<volume> [<right volume>]], seq_play <index> [<volume> [<repeat>]], seq_stop <index>. The
    // music owner is abe (default), other (a second dummy game object, made when first used) or
    // none. kill_other destroys it, so the controller finds its owner dead. False with error if it
    // can't (e.g. no game running)
    static bool Run(const std::string& name, const std::vector<std::string>& args, std::string& error, BaseMap& map, ResourceManagerWrapper& resMan);

    // Forget the second owner, when the game's objects are all destroyed (a new game)
    static void ForgetOwners();

    // True for the names Check and Run handle
    static bool IsSoundCommand(const std::string& name);

    // Under an automation script the music clock runs on the script's frames (30 a second) instead
    // of real time, so the same script always gives the same music timing. Automation sets the
    // frame each loop and clears it when it ends.
    static void SetScriptFrame(bool running, u32 frame);
    // The tick count (ms) the music controllers time themselves with: realTicks, or the script's
    // frames when a script is running
    static u32 MusicTicks(u32 realTicks);

    // RELIVE_MUSIC_LOG=<file>: logs the MusicController's inputs (CREATE, ENABLE, MUSIC, CLEAR,
    // VUPDATE) and its SND_SEQ_* calls as "<ms> <name> <args>", the music clock's ms: the same under
    // a script, real time otherwise. The PS1's own MusicController, run in the reference harness on
    // the inputs, can be compared at the SND_SEQ_* calls.
    static void LogSeqCall(const char* fmt, ...);
    // Like LogSeqCall, but only while a MusicScope is open: the SND_SEQ_* calls the music
    // controller itself makes, not those of other game objects
    static void LogSeqOutput(const char* fmt, ...);

    // Open around each entry point of the music controller
    class MusicScope final
    {
    public:
        MusicScope();
        ~MusicScope();
        MusicScope(const MusicScope&) = delete;
        MusicScope& operator=(const MusicScope&) = delete;
    };
    // A small number for an object pointer, in the order first seen (0 for null), for the log
    static s32 ObjectOrdinal(const void* p);

    // -sound_debug: show the window always, not only with the other debug windows
    static void SetAlwaysShown(bool shown);
    static bool AlwaysShown();

    // The "Sound" window. Call between ImGui::NewFrame and Render
    static void DrawWindow();

    // The running game's music types, as name and value
    struct MusicName final
    {
        const char* mName;
        s32 mValue;
    };
    static const std::vector<MusicName>& MusicNames();
};
