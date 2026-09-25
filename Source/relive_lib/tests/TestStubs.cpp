// Link-time-only stand-ins for the handful of symbols relive_lib expects the game/editor binary
// to provide, so relive_lib_tests can link the real relive_lib target directly instead of
// hand-picking translation units. None of these tests actually exercise recording/playback or
// fatal-error UI, so these are dummy/no-op implementations, not real behaviour.

#include "FatalError.hpp"
#include "BaseGameAutoPlayer.hpp"

#include <cstdio>
#include <cstdarg>
#include <cstdlib>

// The real ALIVE_FATAL (FatalError.cpp) calls Sys::ShowMessageBox, which pulls in the whole
// windowing/SDL subsystem this lightweight test target doesn't need. None of these tests are
// expected to hit a fatal error; if one somehow does, abort loudly with the message instead of
// silently misbehaving or failing to link.
void ALIVE_FATAL(const char_type* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    abort();
}

namespace
{
    // Minimal concrete BaseRecorder/BasePlayer/BaseGameAutoPlayer - relive_lib code that isn't
    // actually exercised by these tests calls out to GetGameAutoPlayer(), which is normally
    // implemented at the game/editor binary level (see relive/Exe.cpp). These bodies are never
    // meant to be hit at runtime by the filesystem/mods tests; they exist purely so the real
    // relive_lib target link-completes.
    class TestRecorder final : public BaseRecorder
    {
    public:
        void SaveObjectStates() override
        {
        }
    };

    class TestPlayer final : public BasePlayer
    {
    public:
        bool ValidateObjectStates() override
        {
            return false;
        }
    };

    class TestGameAutoPlayer final : public BaseGameAutoPlayer
    {
    public:
        TestGameAutoPlayer(BaseRecorder& recorder, BasePlayer& player)
            : BaseGameAutoPlayer(recorder, player)
        {
        }

        u32 ReadInput(u32 /*padIdx*/) override
        {
            return 0;
        }
    };
} // namespace

BaseGameAutoPlayer& GetGameAutoPlayer()
{
    static TestRecorder recorder;
    static TestPlayer player;
    static TestGameAutoPlayer autoPlayer(recorder, player);
    return autoPlayer;
}
