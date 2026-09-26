#pragma once

#include <SDL3/SDL.h>

class Window;

class GLContext final
{
public:
    // checks makes every GL call check for errors, see GLDebug
    GLContext(Window& window, bool checks);
    ~GLContext();

    void SwapBuffers();

private:
    Window& mWindow;
    SDL_GLContext mContext = nullptr;
};
