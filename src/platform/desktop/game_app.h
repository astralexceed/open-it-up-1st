// Entry points of the desktop game and launcher.

#pragma once

namespace piu::desktop {

// What the game asks for when it closes.
enum class GameExit { kQuit, kBackToLauncher, kFailed };

GameExit run_game();
// The window where the player reassigns the keys.
GameExit run_key_map();
GameExit run_calibration();

enum class Start { kLauncher, kGame, kKeyMap, kCalibration };
// Opens that part of the program as a new instance of the application.
void open_application(Start start);

// Asks the system to put this application in front of the others.
void bring_application_to_front();

// The launcher window. Returns the process's exit code.
int run_launcher(int argc, char** argv);

}  // namespace piu::desktop
