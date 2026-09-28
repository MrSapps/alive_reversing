#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

class BaseMap;
class FileSystem;
class ResourceManagerWrapper;

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
//       Let n frames run.
//   wait_until in_game
//       Until there's a controlled character on a loaded camera outside the menus.
//   wait_until camera <level> <path> <camera>
//       Until that camera is loaded.
//   screenshot <file.png>
//       The next frame's 640x240 framebuffer, before it's scaled to the window.
//   dump_state <file.json>
//       The game, level/path/camera, frame numbers, whether a movie or other modal is running.
//   dump_objects <file.json>
//       Every game object, with its position and animation (blend mode, tint, layer...).
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

    Automation(FileSystem& fs, ResourceManagerWrapper& resMan, std::vector<Command> commands);

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
    bool Screenshot(const std::string& path);
    bool WaitUntil(const Command& command, BaseMap& map, bool betweenFrames) const;
    bool SaveText(const std::string& path, const std::string& text);
    std::string StateJson(BaseMap& map) const;
    std::string ObjectsJson() const;
    void Fail(const Command& command, const std::string& why);

    FileSystem& mFs;
    ResourceManagerWrapper& mResMan;
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
};
