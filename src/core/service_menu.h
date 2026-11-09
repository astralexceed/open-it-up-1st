// The operator service menu and its test screens.

#pragma once

#include <array>
#include <optional>
#include <string>

#include "core/sound.h"

#include "core/assets.h"
#include "core/draw_list.h"
#include "core/machine_settings.h"

namespace piu {

// The service mode of the machine, with its seven screens.
class ServiceMenu {
 public:
  ServiceMenu(MachineSettings settings, const Assets& assets);

  // TEST MODE button: moves the cursor. SERVICE button: selects and leaves test screens.
  void press_test();
  void press_service();
  // State of the switches shown on the I/O TEST screen.
  void set_panel(int panel, bool down);
  void set_test_held(bool down) { test_held_ = down; }
  void set_clear_held(bool down) { clear_held_ = down; }

  void draw(DrawList& out) const;

  bool exited() const { return exited_; }
  const MachineSettings& settings() const { return saved_; }
  // CD track the SOUND TEST asks to play, 0 to stop, nothing if unchanged.
  std::optional<int> take_track_request();

  SoundQueue& sounds() { return sounds_; }

 private:
  SoundQueue sounds_;
  enum class Page { kMain, kIo, kEeprom, kScreen, kGame, kCoin, kSound };
  enum class Align { kLeft, kCenter, kRight };

  int rows() const;
  int& cursor() { return cursors_[static_cast<std::size_t>(page_)]; }
  void select_game_option();
  void select_coin_option();
  void select_sound_option();
  void reload();

  void text(DrawList& out, float x, float y, const std::string& value, Align align, Color color) const;
  void rect(DrawList& out, float x, float y, float w, float h, Color color) const;
  void outline(DrawList& out, float x, float y, float w, float h, Color color) const;
  void draw_list_rows(DrawList& out, const std::string* labels, int count, float x, Align align) const;
  void draw_title(DrawList& out, const char* title) const;
  void draw_exit_hint(DrawList& out) const;
  void draw_main(DrawList& out) const;
  void draw_io(DrawList& out) const;
  void draw_eeprom(DrawList& out) const;
  void draw_screen_test(DrawList& out) const;
  void draw_game(DrawList& out) const;
  void draw_coin(DrawList& out) const;
  void draw_sound(DrawList& out) const;

  MachineSettings settings_;
  MachineSettings saved_;
  Texture font_;
  Texture white_;
  Page page_ = Page::kMain;
  std::array<int, 7> cursors_{};
  std::array<bool, 10> panels_{};
  bool test_held_ = false;
  bool clear_held_ = false;
  bool exited_ = false;
  int track_ = 0;
  int effect_ = 0;
  std::optional<int> track_request_;
};

// Files the service menu needs.
void request_service_assets(Assets& assets);

}  // namespace piu
