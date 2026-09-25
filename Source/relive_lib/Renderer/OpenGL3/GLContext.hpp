#pragma once

#include <SDL3/SDL.h>

class Window;

class GLContext final
{
public:
    explicit GLContext(Window& window);
    ~GLContext();

    void SwapBuffers();

private:
    Window& mWindow;
    SDL_GLContext mContext = nullptr;
};
