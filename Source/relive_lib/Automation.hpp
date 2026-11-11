#pragma once

#include "Types.hpp"
#include "data_conversion/guid.hpp"
#include "StateHash.hpp"
#include <memory>
#include <map>
#include <string>
#include <vector>

class BaseMap;
class FileSystem;
class ResourceManager;
namespace Bot {
class Bot;
}

// Runs a script of commands (-automation=<file>), so a tool or a test can drive the game and see
// what it's doing without anyone watching it. With SDL_VIDEO_DRIVER=offscreen it needs no display.
//
// One command a line, its arguments separated by spaces. # starts a comment.
//
//   start <level> <path> <camera> [<x> <y>]
//       Start a new game at a camera, like the menus' level select: no movies, Abe made and put at
//       world position (x, y), or near the middle of the camera. Levels are named as in the path
//       JSON files, e.g. rupture_farms (see data_conversion/EnumSerialization.hpp).
//   wait_frames <n>
//       Let n frames run. A modal's frames (the pause menu, a movie) count too.
//   wait_until in_game
//       Until there's a controlled character on a loaded camera outside the menus.
//   wait_until camera <level> <path> <camera>
//       Until that camera is loaded.
//   hold <buttons> <frames>
//       Hold these buttons down on pad 1 for that many frames, then release them. Buttons are
//       joined with +, e.g. hold left+run 20. Names: up, down, left, right, run, action, sneak,
//       throw, hop, roll (AO) or fart (AE), speak1-speak8 (AE's 1-8 or AO's 4), chant,
//       pause, confirm (the menus' OK), back, cheat.
//   switch <id> <0|1>
//       Set a switch state, as a lever would.
//   electrocute [extra]
//       Electrocute the controlled character, as an electric wall does (it dies at the end). extra
//       adds the third, black sweep: the character is erased (AE: the greeter's and wall's).
//   invisible <out|in|now>
//       AE only. Abe's invisibility power: fade out, fade back in, or invisible straight away.
//   expect_objects <type> <count>
//       Fail unless exactly that many live objects have that type name (as dump_objects shows it,
//       e.g. eSlamDoor).
//   screenshot <file.png>
//       The next frame's 640x240 framebuffer, before it's scaled to the window.
//   dump_state <file.json>
//       The game, level/path/camera, frame numbers, whether a movie or other modal is running.
//   dump_objects <file.json>
//       Every game object, with its position and animation (blend mode, tint, layer...).
//   fmv <name>
//       Play a movie (e.g. BA1114), the way the game's camera changes do. It takes over the main loop
//       until it ends or is skipped: wait_frames counts its frames, and quit quits through it.
//   music <type> [<owner> [<flag4> [<flag20>]]]
//       Ask the game's MusicController for a music type (name or number, see SoundDebug.hpp) as
//       abe (default), other (a second dummy object) or none, with the controller's two flags.
//   kill_other
//       Destroy the dummy owner, so the controller's owner is dead.
//   sfx <index> [<volume> [<right volume>]]
//       Play a relive::SoundEffects entry (its index in Sfx.hpp), at its default volume if none;
//       with a right volume, in stereo.
//   sfx_pitch <index> <volume> <pitch>
//       Play a sound effect with a pitch offset in 1/128 semitones (SFX_Play_Pitch).
//   seq_play <index> [<volume> [<repeat>]]
//       Play a SEQ by its index in the game's SEQ table (volume default 100, repeat 0 = forever,
//       default 1). seq_stop <index> stops it.
//   hash_log <file> [full]
//       From now on, write a line "<game frame> <hash>" for each game frame to the file when the
//       script ends. The hash covers the whole world (see StateHash.hpp), so two runs of a script
//       that match line for line are deterministic. full adds a line for each thing hashed, to find
//       which one first differs.
//   hash_mark <name>
//       Remember the whole world (see StateHash.hpp) under that name.
//   dump_raw <file>
//       Every object's memory as hex words, to find which bytes differ when a "raw" part of hash_log full does.
//   expect_hash <name> [ignore <class>...]
//       Fail unless the world is the one hash_mark <name> saw: the same frame, random seed, camera,
//       switches and objects (in any order). ignore leaves out objects of those C++ classes (Spark...),
//       e.g. cosmetic effects a save doesn't keep. The failure says what differs.
//   restore_check <frames> [<buttons>]
//       Save, run the frames with the buttons held, restore, check the world is the one that was saved,
//       run the frames again and check it's what the first run came to. Logs a line for the camera and goes
//       on whatever happens (expect_restore_checks fails at the end), so a script can sweep many cameras.
//       Skipped when the controlled character is dead, as a quicksave isn't taken then.
//   expect_restore_checks
//       Fail if any restore_check did.
//   quicksave
//       Take a quicksave (AE's DoQuicksave, AO's quicksave) into memory.
//   quickload
//       Restore the last quicksave and wait for the camera to load. With hash_log, the lines after
//       it should repeat the ones after the quicksave if a restore is exact.
//   draw <0|1>
//       0: run frames without drawing or showing them and without waiting for the frame time (much faster, the
//       game's state is the same). Screenshots need 1, the default.
//   bot_do <what> ...
//       One step of the bot's pilot (see Bot/Pilot.hpp), run until it's done; a failure fails the script.
//         bot_do wait <frames> [<buttons>]
//         bot_do move <x> [walk|run|sneak|roll] to the grid cell of world x (roll ends crouched)
//         bot_do face <left|right>
//         bot_do press <buttons> [<max frames>] hold until Abe starts something, then let it play out
//   bot <profile>
//       The bot plays from here on by a profile: save_all, kill_all, average or any, then options after commas
//       (seed=<n>, human), e.g. bot save_all,seed=7. See Bot/BOT_DESIGN.md.
//   bot_wait <max frames>
//       Until the bot is done or stuck, or that many frames have run. Logs its report.
//   expect_bot done|stuck|running
//       Fail unless that is the bot's state.
//   bot_report <file>
//       What the bot is doing and sees, in a few lines, with the end of its log.
//   bot_inputs <file>
//       The buttons of every frame of the bot's run that counted, to play back.
//   expect_bot_restores <at least>
//       Fail unless the bot has gone back to a checkpoint (after a death) that many times.
//   bot_stop
//       The bot stops playing and lets go of the buttons.
//   replay <inputs file>
//       Hold the buttons a bot_inputs file lists, a line a frame, with no bot: started at the same point of the
//       same script it gives the same game (see tests/automation/run_bot_replay.cmake). Fails if the game frame
//       isn't the one the inputs start at.
//   bot_goto <x> <y> [half]
//       The bot takes Abe to the floor cell at that world position (half: on a background floor), by its nav graph.
//       Fails if it can't.
//   bot_log <file>
//       The bot's decisions, written when the script ends.
//   bot_nav_dump <file>
//       The bot's nav graph of the current path as text: its floors, and every edge that isn't a plain step.
//   bot_trace <file>
//       From now on, a line per frame of what Abe does and the buttons held, written when the script ends.
//   timeout <seconds>
//       Fail if the script is still running after this long, counted from the start (default 120).
//   quit
//       Quit, which is also done when the script runs out.
//
// File paths are relative to the working directory (the game data). A command that fails logs an
// error and quits, and relive then returns 1.
class Automation final
{
public:
    struct Command final
    {
        s32 mLine = 0;
        std::string mName;
        std::vector<std::string> mArgs;
    };

    // Splits a script into commands, checking each one's name and arguments. On an error, returns
    // false with error saying what's wrong and on which line.
    static bool Parse(const std::string& script, std::vector<Command>& commands, std::string& error);

    Automation(FileSystem& fs, ResourceManager& resMan, std::vector<Command> commands);
    ~Automation();

    enum class Result
    {
        eContinue,
        eQuit,
    };

    // Called every main loop iteration. betweenFrames says nothing is part way through (a frame, a
    // modal such as a movie, a camera change or loading), so the game can be changed.
    Result Update(BaseMap& map, bool betweenFrames);

    bool Failed() const
    {
        return mFailed;
    }

private:
    // Runs the current command. True once it's done.
    bool RunCommand(const Command& command, BaseMap& map, bool betweenFrames);
    bool Start(const Command& command, BaseMap& map, bool betweenFrames);
    void DumpRestoreCheckRaw(const char* suffix);
    bool RestoreCheck(const Command& command, BaseMap& map, bool betweenFrames);
    bool Screenshot(const std::string& path);
    bool WaitUntil(const Command& command, BaseMap& map, bool betweenFrames) const;
    // The InputCommands bits of a +-joined button list, false with error for an unknown name
public:
    static bool ParseButtons(const std::string& buttons, u32& bits, std::string& error);

private:
    bool SaveText(const std::string& path, const std::string& text);
    void LogHash(BaseMap& map);
    void FlushHashLog();
    std::string StateJson(BaseMap& map) const;
    std::string ObjectsJson() const;
    void Fail(const Command& command, const std::string& why);

    FileSystem& mFs;
    ResourceManager& mResMan;
    std::vector<Command> mCommands;
    std::size_t mNextCommand = 0;
    bool mFailed = false;

    u32 mStartTicks = 0;
    u32 mTimeoutMs = 120 * 1000;
    // Frames started since the script began
    u32 mFrames = 0;

    // The current command's progress
    s32 mStep = 0;
    u32 mWaitUntilFrame = 0;
    // hash_log's file, whether it logs each part, the log so far and the last frame logged
    std::string mHashLogPath;
    bool mHashLogFull = false;
    std::string mHashLog;
    u32 mHashLoggedFrame = 0;
    u64 mHashLoggedTotal = 0;
    // restore_check's progress: the world saved and after the frames run, what differed once restored and
    // again the frames on, and the counts over a script
    StateHash mCheckSaved;
    StateHash mCheckLater;
    std::string mCheckJson;
    std::string mCheckAtRestore;
    std::string mCheckWhere;
    u32 mCheckButtons = 0;
    // The controlled character died while it waited: the game puts the last quicksave back, so what is left isn't a
    // continuation of the world that was saved
    s32 mCheckDiedMotion = -1;
    bool mCheckDied = false;
    // A restore_check that is skipped waits for the world to be quiet again before the script goes on, up to this frame
    u32 mSettleUntil = 0;
    bool mSettleRetry = false;
    bool mCheckRetried = false;
    s32 mRestoreChecks = 0;
    s32 mFailedRestoreChecks = 0;
    // hash_mark's worlds
    std::map<std::string, StateHash> mHashMarks;
    // quickload is waiting for the restore to finish; nothing is logged until it has
    bool mRestoring = false;

    Bot::Bot& TheBot();
    std::unique_ptr<Bot::Bot> mBot;
    std::string mBotTracePath;
    std::string mBotLogPath;
    // replay's buttons and how far it is through them
    std::vector<u32> mReplay;
    std::size_t mReplayAt = 0;
    u32 mReplayStartFrame = 0;

    // The invisibility effect the invisible command made
    Guid mInvisibleEffectId;
};
