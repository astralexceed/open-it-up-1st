// Creates the WebGL context for the browser version.

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "platform/engine_api.h"

extern "C" EMSCRIPTEN_KEEPALIVE int piu_start() {
  EmscriptenWebGLContextAttributes attributes;
  emscripten_webgl_init_context_attributes(&attributes);
  attributes.majorVersion = 2;
  attributes.minorVersion = 0;
  attributes.alpha = 0;
  attributes.depth = 1;
  attributes.stencil = 0;
  attributes.antialias = 1;

  const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context =
      emscripten_webgl_create_context("#canvas", &attributes);
  if (context <= 0 || emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS) {
    return 0;
  }
  return piu_engine_start();
}

int main() { return 0; }
