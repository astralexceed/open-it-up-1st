// Extracted game files and saved settings on disk.

#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace piu::desktop {

// What the launcher chooses and the game uses. It stays on disk between runs.
struct Settings {
  std::string language = "en";
  bool wide = false;
  bool fullscreen = false;
  // The key of each input, as KeyMap saves it. Empty means the default layout.
  std::string keys;
  // How late, in ms, the player hears the music.
  int audio_offset_ms = 0;
  // The machine's EEPROM: mode, difficulty, stage break, help, demo sound, coins.
  std::optional<std::array<int, 6>> machine;
};

Settings load_settings();
bool save_settings(const Settings& settings);

// The folder next to the application that holds the extracted game and the settings.
std::filesystem::path data_folder();

struct LibraryState {
  bool ready = false;
  bool sounds_ready = false;
  std::string source;  // Name of the CHD it came from.
};
LibraryState read_library_state();

using Progress = std::function<void(int percent)>;
// Both return an empty string, or a short code the launcher turns into a message.
std::string extract_game(const std::filesystem::path& chd, const Progress& progress);
std::string extract_sounds(const std::filesystem::path& rom, const Progress& progress);

// The extracted files, by the keys the engine asks for ("png:arrow01", "wav:track02"...).
class Library {
 public:
  bool open();
  const std::filesystem::path* find(const std::string& key) const;
  const std::filesystem::path* find_chart(const std::string& stem) const;
  bool has(const std::string& key) const { return find(key) != nullptr; }

 private:
  std::unordered_map<std::string, std::filesystem::path> files_;
  std::unordered_map<std::string, std::filesystem::path> charts_;
};

std::optional<std::string> read_file(const std::filesystem::path& path);

}  // namespace piu::desktop
