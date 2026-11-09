// Windows only offers OpenGL 1.1 directly, so the rest is looked up once the context exists.

#include <SDL.h>

#include "render/gl.h"

#define PIU_DEFINE(type, name) type piu_##name = nullptr;
PIU_GL_FUNCTIONS(PIU_DEFINE)

namespace piu {

bool load_gl_functions() {
  bool ok = true;
#define PIU_LOAD(type, name)                                   \
  piu_##name = reinterpret_cast<type>(SDL_GL_GetProcAddress(#name)); \
  ok = ok && piu_##name != nullptr;
  PIU_GL_FUNCTIONS(PIU_LOAD)
  return ok;
}

}  // namespace piu
