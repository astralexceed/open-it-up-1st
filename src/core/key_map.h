// The keyboard layout: which key does each input of the machine.

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace piu {

// Panels 0 to 9 first (down-left, up-left, center, up-right, down-right; left pad, then right pad).
enum KeyAction { kKeyCoin1 = 10, kKeyCoin2, kKeyTest, kKeyService, kKeyClear };
inline constexpr std::size_t kKeyActions = 15;

// Text shown for a key, or empty if that key cannot be assigned. Keys are named as browsers
// name them ("KeyQ", "Digit5"...), so the layout means the same on every platform.
std::string key_label(const std::string& code);

class KeyMap {
 public:
  KeyMap();
  // Reads a saved layout. Empty if it is damaged or repeats a key.
  static std::optional<KeyMap> parse(const std::string& text);
  std::string serialize() const;

  // The input a key does, or -1.
  int action_for(const std::string& code) const;
  const std::string& code(int action) const { return codes_[static_cast<std::size_t>(action)]; }
  void assign(int action, const std::string& code) { codes_[static_cast<std::size_t>(action)] = code; }

 private:
  std::array<std::string, kKeyActions> codes_;
};

}  // namespace piu
