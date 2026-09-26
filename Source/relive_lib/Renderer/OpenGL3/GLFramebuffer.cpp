#include "../../Window.hpp"
#include <GL/glew.h>

#include "../../../relive_lib/Types.hpp"
#include "GLDebug.hpp"
#include "GLFramebuffer.hpp"
#include "SDL3/SDL.h"

GLFramebuffer::GLFramebuffer(s32 width, s32 height)
{
    mHeight = height;
    mWidth = width;

    CreateGLObjects();
}

GLFramebuffer::~GLFramebuffer()
{
    if (mFramebufferGLId)
    {
        DestroyGLObjects();
    }
}


void GLFramebuffer::BindAsSourceTextureTo(GLenum texUnit)
{
    GL_VERIFY(glActiveTexture(texUnit));
    GL_VERIFY(glBindTexture(GL_TEXTURE_2D, mTextureGLId));
}

void GLFramebuffer::BindAsTarget()
{
    GL_VERIFY(glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferGLId));
    GL_VERIFY(glViewport(0, 0, mWidth, mHeight));
}

s32 GLFramebuffer::GetHeight()
{
    return mHeight;
}

s32 GLFramebuffer::GetWidth()
{
    return mWidth;
}

void GLFramebuffer::Resize(s32 newWidth, s32 newHeight)
{
    DestroyGLObjects();

    mWidth = newWidth;
    mHeight = newHeight;

    CreateGLObjects();
}

void GLFramebuffer::ReadPixels(std::vector<u8>& rgbaPixels)
{
    const std::size_t rowBytes = static_cast<std::size_t>(mWidth) * 4;
    std::vector<u8> bottomUp(rowBytes * mHeight);

    BindAsTarget();
    GL_VERIFY(glPixelStorei(GL_PACK_ALIGNMENT, 1));
    GL_VERIFY(glReadPixels(0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data()));

    // GL's rows start at the bottom
    rgbaPixels.resize(bottomUp.size());
    for (s32 y = 0; y < mHeight; y++)
    {
        memcpy(&rgbaPixels[y * rowBytes], &bottomUp[(mHeight - 1 - y) * rowBytes], rowBytes);
    }
}

void GLFramebuffer::CreateGLObjects()
{
    static const GLenum fbTargets[1] = {GL_COLOR_ATTACHMENT0};

    // Create objects
    GL_VERIFY(glGenFramebuffers(1, &mFramebufferGLId));
    GL_VERIFY(glGenTextures(1, &mTextureGLId));

    // Init texture
    GL_VERIFY(glActiveTexture(GL_TEXTURE0));
    GL_VERIFY(glBindTexture(GL_TEXTURE_2D, mTextureGLId));

    GL_VERIFY(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, mWidth, mHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0));

    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

    // Init framebuffer
    GL_VERIFY(glBindFramebuffer(GL_FRAMEBUFFER, mFramebufferGLId));
    GL_VERIFY(glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, mTextureGLId, 0));

    GL_VERIFY(glDrawBuffers(1, fbTargets));
}

void GLFramebuffer::DestroyGLObjects()
{
    GL_VERIFY(glDeleteFramebuffers(1, &mFramebufferGLId));
    GL_VERIFY(glDeleteTextures(1, &mTextureGLId));
}


void GLFramebuffer::BindScreenAsTarget(const Window& window, s32* outWidth, s32* outHeight)
{
    s32 viewportW, viewportH;

    window.GetSizeInPixels(viewportW, viewportH);

    GL_VERIFY(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    GL_VERIFY(glViewport(0, 0, viewportW, viewportH));

    if (outWidth)
    {
        *outWidth = viewportW;
    }

    if (outHeight)
    {
        *outHeight = viewportH;
    }
}
