#include "Sys.hpp"
#include "logger.hpp"
#include <stdarg.h>
#include <vector>

#if defined(__SANITIZE_ADDRESS__) && !defined(_WIN32)
    #include <sanitizer/common_interface_defs.h>
#elif defined(__linux__)
    #include <execinfo.h>
    #include <unistd.h>
#endif

// Where the fatal error came from, on stderr
static void PrintCallStack()
{
#if defined(__SANITIZE_ADDRESS__) && !defined(_WIN32)
    // Symbolized, with file and line
    __sanitizer_print_stack_trace();
#elif defined(__linux__)
    void* frames[64];
    const int count = backtrace(frames, 64);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
#endif
}

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
    PrintCallStack();

    Sys::ShowMessageBox(nullptr, pMessage, "R.E.L.I.V.E fatal error.");
    abort();
}
