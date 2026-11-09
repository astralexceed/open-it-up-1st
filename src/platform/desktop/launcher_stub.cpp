// Placeholder for systems that have no launcher window yet.

#include <cstdio>

#include "platform/desktop/game_app.h"

// Systems without a launcher window yet use the command line.
namespace piu::desktop {

void open_application(Start) {}

void bring_application_to_front() {}

int run_launcher(int, char**) {
  std::puts("Usage: --extract <file.chd>, --sounds <piu10.u9>, --play, --keys");
  return 0;
}

}  // namespace piu::desktop
