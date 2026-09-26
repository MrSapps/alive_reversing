#include <GL/glew.h>

#include "../../../relive_lib/FatalError.hpp"
#include "../../../relive_lib/Types.hpp"
#include "GLDebug.hpp"

bool GLDebug::sChecks = false;

void GLDebug::CheckError()
{
    const GLenum lastGLError = glGetError();
    if (lastGLError != GL_NO_ERROR)
    {
        auto msg = reinterpret_cast<const char_type*>(glewGetString(lastGLError));
        ALIVE_FATAL("OpenGL error raised: %s %d", msg ? msg : "(null)", lastGLError);
    }
}
