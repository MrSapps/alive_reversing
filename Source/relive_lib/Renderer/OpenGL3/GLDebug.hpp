#pragma once

#define GL_INFOLOG_MAX_LENGTH 250

// Checks for a GL error after every call, when turned on (-renderer_checks). glGetError can make
// the driver wait for the GPU, so it's off by default.
class GLDebug final
{
public:
    static void SetChecks(bool checks)
    {
        sChecks = checks;
    }

    static bool ChecksEnabled()
    {
        return sChecks;
    }

    // For the debug menu's toggle
    static bool& Checks()
    {
        return sChecks;
    }

    // Fatal if the last GL call raised an error
    static void CheckError();

private:
    // Here rather than in the renderer because every GL object's calls are checked, and they
    // don't know about it
    static bool sChecks;
};

#define GL_VERIFY(x)                   \
    (x);                               \
    if (GLDebug::ChecksEnabled())      \
    {                                  \
        GLDebug::CheckError();         \
    }
