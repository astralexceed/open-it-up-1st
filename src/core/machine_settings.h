// The operator settings saved by the service menu.

#pragma once

namespace piu {

// The six bytes the service menu saves to EEPROM.
struct MachineSettings {
  int game_mode = 0;         // 0 Normal, 1 Event.
  int difficulty = 0;        // 0 Easy, 1 Normal, 2 Hard.
  int stage_break = 1;       // 0, 1 or 2: stages that cannot be failed.
  int show_help = 1;         // 0 All, 1 Easy only, 2 Easy and Normal.
  int demo_sound = 1;        // 0 Off, 1 On.
  int coins_per_credit = 5;  // 1 to 10.

  // DEFAULT SETTING leaves the coins alone.
  void restore_game_defaults() {
    game_mode = 0;
    difficulty = 0;
    stage_break = 1;
    show_help = 1;
    demo_sound = 1;
  }

  // Out-of-range values make the original reset everything on load.
  bool valid() const {
    return game_mode >= 0 && game_mode <= 1 && difficulty >= 0 && difficulty <= 2 && stage_break >= 0 &&
           stage_break <= 2 && show_help >= 0 && show_help <= 2 && demo_sound >= 0 && demo_sound <= 1 &&
           coins_per_credit >= 1 && coins_per_credit <= 10;
  }
};

}  // namespace piu
