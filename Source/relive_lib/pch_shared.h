#pragma once

#include "Types.hpp"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif

#include <windows.h>
#undef CreateDirectory

#else
    using PVOID = void*;
    using LPVOID = void*;
    using HINSTANCE = void*;
    using DWORD = unsigned int;
    using PCHAR = char*;
    using BOOL = long;
    using LONG = int;
    using LPSTR = char*;
    using LPCSTR = const char*;
    using BYTE = unsigned char;
    using WORD = unsigned short int;
    using HDC = void*;
    using UINT = unsigned int;
    using byte = unsigned char;
    using HANDLE = void*;
    using CHAR = char;

    #define TRUE 1
    #define FALSE 0

    struct RECT
    {
        LONG left;
        LONG top;
        LONG right;
        LONG bottom;
    };

    using LPRECT = RECT * ;
    struct POINT
    {
        LONG x;
        LONG y;
    };

    using HWND = void*; // Temp as is the rest of this till all win32 junk is nuked
    using IDirectDrawSurface = void*;
    using WPARAM = unsigned int;
    using LPARAM = unsigned int;

    #define LRESULT
    #define WINAPI
    #define CALLBACK
    #define DWORD_PTR DWORD
    #define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
    #define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))

#endif

#include "logger.hpp"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
