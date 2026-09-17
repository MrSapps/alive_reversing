#include "Sys.hpp"
#include "logger.hpp"
#include <stdarg.h>
#include <vector>

[[noreturn]] void ALIVE_FATAL(const char_type* fmt, ...)
{
    char_type stackBuf[2048] = {};

    va_list args;
    va_start(args, fmt);
    va_list argsCopy;
    va_copy(argsCopy, args);
    const int required = vsnprintf(stackBuf, sizeof(stackBuf), fmt, args);
    va_end(args);

    // vsnprintf returns the length the fully formatted string would have needed, regardless of
    // whether it fit - if that's too big for the stack buffer above, reformat into a heap one
    // that's actually big enough instead of silently truncating.
    std::vector<char_type> dynamicBuf;
    const char_type* pMessage = stackBuf;
    if (required >= static_cast<int>(sizeof(stackBuf)))
    {
        dynamicBuf.resize(static_cast<size_t>(required) + 1);
        vsnprintf(dynamicBuf.data(), dynamicBuf.size(), fmt, argsCopy);
        pMessage = dynamicBuf.data();
    }
    va_end(argsCopy);

    LOG_ERROR("%s", pMessage);

    Sys_MessageBox(nullptr, pMessage, "R.E.L.I.V.E fatal error.");
    abort();
}
