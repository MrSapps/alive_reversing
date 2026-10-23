#include <GL/glew.h>

#include "../../../relive_lib/FatalError.hpp"
#include "../../../relive_lib/Types.hpp"
#include "../../../relive_lib/data_conversion/rgb_conversion.hpp"
#include "GLDebug.hpp"
#include "GLTexture2D.hpp"

GLTexture2D::GLTexture2D(u32 width, u32 height, GLenum format, GLenum internalFormat)
    : mFormat(format),
    mInternalFormat(internalFormat ? internalFormat : format),
    mHeight(height),
    mWidth(width)
{
    GL_VERIFY(glGenTextures(1, &mGLId));

    BindTo(GL_TEXTURE0);

    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_VERIFY(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    GL_VERIFY(glTexImage2D(GL_TEXTURE_2D, 0, mInternalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, 0));
}

GLTexture2D::GLTexture2D(GLuint glId, u32 width, u32 height, GLenum format, GLenum internalFormat)
    : mFormat(format),
    mInternalFormat(internalFormat),
    mGLId(glId),
    mHeight(height),
    mWidth(width)
{
}

GLuint GLTexture2D::ReleaseGLId()
{
    const GLuint glId = mGLId;
    mGLId = 0;
    return glId;
}

GLTexture2D::~GLTexture2D()
{
    if (mGLId)
    {
        GL_VERIFY(glDeleteTextures(1, &mGLId));
    }
}

void GLTexture2D::BindTo(GLenum texUnit)
{
    GL_VERIFY(glActiveTexture(texUnit));
    GL_VERIFY(glBindTexture(GL_TEXTURE_2D, mGLId));
}

u32 GLTexture2D::GetHeight()
{
    return mHeight;
}

u32 GLTexture2D::GetWidth()
{
    return mWidth;
}

bool GLTexture2D::IsValid()
{
    return mGLId > 0;
}

void GLTexture2D::LoadImage(const void* pixels, GLenum type)
{
    BindTo(GL_TEXTURE0);

    // The storage was allocated when the texture was made
    SetPixelUnpacking();
    GL_VERIFY(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, mFormat, type, pixels));
}

void GLTexture2D::LoadSubImage(GLint x, GLint y, GLsizei width, GLsizei height, const void* pixels, GLenum type)
{
    BindTo(GL_TEXTURE0);

    SetPixelUnpacking();
    GL_VERIFY(glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, mFormat, type, pixels));
}


void GLTexture2D::SetPixelUnpacking()
{
    switch (mFormat)
    {
        case GL_RGBA:
            GL_VERIFY(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
            break;

        default:
            GL_VERIFY(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            break;
    }
}

static std::size_t TextureBytes(u32 width, u32 height, GLenum format)
{
    return static_cast<std::size_t>(width) * height * (format == GL_RGBA ? 4 : 3);
}

GLTexturePool::GLTexturePool()
    : mState(std::make_shared<State>())
{
}

GLTexturePool::~GLTexturePool() = default;

GLTexturePool::State::~State()
{
    for (const Entry& entry : mFree)
    {
        GL_VERIFY(glDeleteTextures(1, &entry.mGLId));
    }
}

std::shared_ptr<GLTexture2D> GLTexturePool::Acquire(u32 width, u32 height, GLenum format, GLenum internalFormat)
{
    const GLenum actualInternalFormat = internalFormat ? internalFormat : format;
    std::shared_ptr<State> state = mState;

    GLTexture2D* pTexture = nullptr;
    for (std::size_t i = 0; i < state->mFree.size(); i++)
    {
        const Entry& entry = state->mFree[i];
        if (entry.mWidth == width && entry.mHeight == height && entry.mFormat == format && entry.mInternalFormat == actualInternalFormat)
        {
            pTexture = new GLTexture2D(entry.mGLId, width, height, format, actualInternalFormat);
            state->mFreeBytes -= entry.mBytes;
            state->mFree.erase(state->mFree.begin() + i);
            state->mReuseCount++;
            break;
        }
    }

    if (!pTexture)
    {
        pTexture = new GLTexture2D(width, height, format, internalFormat);
    }

    return std::shared_ptr<GLTexture2D>(pTexture, [state](GLTexture2D* pDone)
    {
        Return(state, pDone);
    });
}

void GLTexturePool::Return(const std::shared_ptr<State>& state, GLTexture2D* pTexture)
{
    Entry entry;
    entry.mWidth = pTexture->mWidth;
    entry.mHeight = pTexture->mHeight;
    entry.mFormat = pTexture->mFormat;
    entry.mInternalFormat = pTexture->mInternalFormat;
    entry.mBytes = TextureBytes(entry.mWidth, entry.mHeight, entry.mFormat);

    if (pTexture->mGLId && state->mFreeBytes + entry.mBytes <= kMaxPooledBytes)
    {
        entry.mGLId = pTexture->ReleaseGLId();
        state->mFreeBytes += entry.mBytes;
        state->mFree.push_back(entry);
    }

    // Deletes the GL texture if it wasn't pooled
    delete pTexture;
}

u32 GLTexturePool::PooledCount() const
{
    return static_cast<u32>(mState->mFree.size());
}

u32 GLTexturePool::ReuseCount() const
{
    return mState->mReuseCount;
}
