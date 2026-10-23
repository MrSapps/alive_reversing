#pragma once

#include <GL/glew.h>

#include "../../../relive_lib/Types.hpp"
#include <memory>
#include <vector>

class GLTexturePool;

class GLTexture2D final
{
public:
    // internalFormat is the format the GPU keeps the pixels in, format if it's 0
    GLTexture2D(u32 width, u32 height, GLenum format, GLenum internalFormat = 0);
    ~GLTexture2D();

    void BindTo(GLenum texUnit);
    u32 GetHeight();
    u32 GetWidth();
    bool IsValid();
    void LoadImage(const void* pixels, GLenum type = GL_UNSIGNED_BYTE);
    void LoadSubImage(GLint x, GLint y, GLsizei width, GLsizei height, const void* pixels, GLenum type = GL_UNSIGNED_BYTE);

private:
    friend class GLTexturePool;

    // A texture that takes over an existing GL texture of that size and format
    GLTexture2D(GLuint glId, u32 width, u32 height, GLenum format, GLenum internalFormat);
    // Gives the GL texture up: the destructor won't delete it
    GLuint ReleaseGLId();

    GLenum mFormat = 0;
    GLenum mInternalFormat = 0;
    GLuint mGLId = 0;
    u32 mHeight = 0;
    u32 mWidth = 0;

    void SetPixelUnpacking();
};

// Keeps the GL textures of sprite sheets and cameras that aren't used any more, so the next one
// of the same size and format is uploaded into one of them: the allocation is the expensive part
// of making a texture, and a level's sheets come in a few sizes (like the PSX's VRAM, which was
// never allocated, only written). The pool holds at most kMaxPooledBytes.
class GLTexturePool final
{
public:
    GLTexturePool();
    ~GLTexturePool();

    // A texture of that size, with whatever pixels it had if it was pooled. It goes back in the
    // pool when its last reference is dropped.
    std::shared_ptr<GLTexture2D> Acquire(u32 width, u32 height, GLenum format, GLenum internalFormat = 0);

    // How many textures are waiting, and how many Acquire calls took one of them
    u32 PooledCount() const;
    u32 ReuseCount() const;

private:
    static constexpr std::size_t kMaxPooledBytes = 96 * 1024 * 1024;

    struct Entry final
    {
        GLuint mGLId = 0;
        u32 mWidth = 0;
        u32 mHeight = 0;
        GLenum mFormat = 0;
        GLenum mInternalFormat = 0;
        std::size_t mBytes = 0;
    };

    // The textures outlive the renderer's members in places (a batch holds on to them), so what
    // they return to is shared
    struct State final
    {
        ~State();
        std::vector<Entry> mFree;
        std::size_t mFreeBytes = 0;
        u32 mReuseCount = 0;
    };

    static void Return(const std::shared_ptr<State>& state, GLTexture2D* pTexture);

    std::shared_ptr<State> mState;
};
