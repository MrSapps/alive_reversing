// A test-only ALIVE_FATAL - the real one (FatalError.cpp) calls Sys_MessageBox, which pulls in
// the whole windowing/SDL subsystem this lightweight test target deliberately doesn't link (see
// FileSystemTests' own CMakeLists.txt comment for why it only compiles the specific translation
// units it needs rather than linking all of relive_lib). None of these tests are expected to hit
// a fatal error; if one somehow does, abort loudly with the message instead of silently
// misbehaving or failing to link.

#include "FatalError.hpp"
#include <cstdio>
#include <cstdarg>
#include <cstdlib>

void ALIVE_FATAL(const char_type* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    abort();
}
