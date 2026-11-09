// Desktop program entry: launcher, game or command-line extraction.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include "platform/desktop/game_app.h"
#include "platform/desktop/library.h"

namespace {

int report(const std::string& error) {
  if (error.empty()) {
    std::puts("OK");
    return 0;
  }
  std::fprintf(stderr, "%s\n", error.c_str());
  return 1;
}

}  // namespace

// With no arguments this is the launcher. The launcher starts a new instance with --play.
int main(int argc, char** argv) {
  using namespace piu::desktop;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--play") == 0) {
      const GameExit result = run_game();
      if (result == GameExit::kBackToLauncher) open_application(Start::kLauncher);
      return result == GameExit::kFailed ? 1 : 0;
    }
    if (std::strcmp(argv[i], "--keys") == 0) {
      const GameExit result = run_key_map();
      if (result == GameExit::kBackToLauncher) open_application(Start::kLauncher);
      return result == GameExit::kFailed ? 1 : 0;
    }
    if (std::strcmp(argv[i], "--extract") == 0 && i + 1 < argc) return report(extract_game(argv[i + 1], nullptr));
    if (std::strcmp(argv[i], "--sounds") == 0 && i + 1 < argc) return report(extract_sounds(argv[i + 1], nullptr));
  }
  return run_launcher(argc, argv);
}
