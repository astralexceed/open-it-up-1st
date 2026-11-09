// Chooses the OpenGL headers of each platform.

#pragma once

// The same OpenGL ES 2 subset works on every platform; only the headers differ.
#if defined(__EMSCRIPTEN__)
#include <GLES2/gl2.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#elif defined(_WIN32)
#include <SDL_opengl.h>
#define PIU_GL_FUNCTIONS(X)                                  \
  X(PFNGLATTACHSHADERPROC, glAttachShader)                   \
  X(PFNGLBINDBUFFERPROC, glBindBuffer)                       \
  X(PFNGLBUFFERDATAPROC, glBufferData)                       \
  X(PFNGLCOMPILESHADERPROC, glCompileShader)                 \
  X(PFNGLCREATEPROGRAMPROC, glCreateProgram)                 \
  X(PFNGLCREATESHADERPROC, glCreateShader)                   \
  X(PFNGLDELETESHADERPROC, glDeleteShader)                   \
  X(PFNGLDISABLEVERTEXATTRIBARRAYPROC, glDisableVertexAttribArray) \
  X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray)   \
  X(PFNGLGENBUFFERSPROC, glGenBuffers)                       \
  X(PFNGLGETATTRIBLOCATIONPROC, glGetAttribLocation)         \
  X(PFNGLGETPROGRAMIVPROC, glGetProgramiv)                   \
  X(PFNGLGETSHADERIVPROC, glGetShaderiv)                     \
  X(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation)       \
  X(PFNGLLINKPROGRAMPROC, glLinkProgram)                     \
  X(PFNGLSHADERSOURCEPROC, glShaderSource)                   \
  X(PFNGLUNIFORMMATRIX4FVPROC, glUniformMatrix4fv)           \
  X(PFNGLUSEPROGRAMPROC, glUseProgram)                       \
  X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer)
#define PIU_DECLARE(type, name) extern type piu_##name;
PIU_GL_FUNCTIONS(PIU_DECLARE)
#undef PIU_DECLARE
#define glAttachShader piu_glAttachShader
#define glBindBuffer piu_glBindBuffer
#define glBufferData piu_glBufferData
#define glCompileShader piu_glCompileShader
#define glCreateProgram piu_glCreateProgram
#define glCreateShader piu_glCreateShader
#define glDeleteShader piu_glDeleteShader
#define glDisableVertexAttribArray piu_glDisableVertexAttribArray
#define glEnableVertexAttribArray piu_glEnableVertexAttribArray
#define glGenBuffers piu_glGenBuffers
#define glGetAttribLocation piu_glGetAttribLocation
#define glGetProgramiv piu_glGetProgramiv
#define glGetShaderiv piu_glGetShaderiv
#define glGetUniformLocation piu_glGetUniformLocation
#define glLinkProgram piu_glLinkProgram
#define glShaderSource piu_glShaderSource
#define glUniformMatrix4fv piu_glUniformMatrix4fv
#define glUseProgram piu_glUseProgram
#define glVertexAttribPointer piu_glVertexAttribPointer
namespace piu {
// Looks up the OpenGL functions; call it once the context exists.
bool load_gl_functions();
}  // namespace piu
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif
