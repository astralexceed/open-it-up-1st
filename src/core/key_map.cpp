// Default layout, saved format and the names shown for each key.

#include "core/key_map.h"

#include <cstdlib>
#include <set>
#include <utility>

namespace piu {
namespace {

constexpr std::pair<const char*, const char*> kLabels[] = {
    {"Minus", "-"},          {"Equal", "="},         {"BracketLeft", "["},  {"BracketRight", "]"}, {"Backslash", "\\"},
    {"Semicolon", ";"},      {"Quote", "'"},         {"Backquote", "`"},    {"Comma", ","},        {"Period", "."},
    {"Slash", "/"},          {"Space", "SPACE"},     {"Enter", "ENTER"},    {"Tab", "TAB"},        {"Backspace", "BKSP"},
    {"ShiftLeft", "LSHFT"},  {"ShiftRight", "RSHFT"}, {"ControlLeft", "LCTRL"}, {"ControlRight", "RCTRL"}, {"AltLeft", "LALT"},
    {"AltRight", "RALT"},    {"CapsLock", "CAPS"},   {"ArrowUp", "UP"},     {"ArrowDown", "DOWN"}, {"ArrowLeft", "LEFT"},
    {"ArrowRight", "RIGHT"}, {"Insert", "INS"},      {"Delete", "DEL"},     {"Home", "HOME"},      {"End", "END"},
    {"PageUp", "PGUP"},      {"PageDown", "PGDN"},   {"NumpadEnter", "N-ENT"}, {"NumpadAdd", "N+"},  {"NumpadSubtract", "N-"},
    {"NumpadMultiply", "N*"}, {"NumpadDivide", "N/"}, {"NumpadDecimal", "N."},
};

}  // namespace

std::string key_label(const std::string& code) {
  if (code.size() == 4 && code.rfind("Key", 0) == 0 && code[3] >= 'A' && code[3] <= 'Z') return code.substr(3);
  if (code.size() == 6 && code.rfind("Digit", 0) == 0 && code[5] >= '0' && code[5] <= '9') return code.substr(5);
  if (code.size() == 7 && code.rfind("Numpad", 0) == 0 && code[6] >= '0' && code[6] <= '9') return "N" + code.substr(6);
  if (code.size() >= 2 && code.size() <= 3 && code[0] == 'F') {
    const int number = std::atoi(code.c_str() + 1);
    if (number >= 1 && number <= 12 && std::to_string(number) == code.substr(1)) return code;
  }
  for (const auto& [name, label] : kLabels) {
    if (code == name) return label;
  }
  return {};
}

KeyMap::KeyMap()
    : codes_{"KeyZ", "KeyQ", "KeyS", "KeyE", "KeyC", "KeyM", "KeyU", "KeyK", "KeyO", "Period", "Digit5", "Digit6", "F2", "F3", "F4"} {}

std::optional<KeyMap> KeyMap::parse(const std::string& text) {
  KeyMap map;
  std::set<std::string> seen;
  std::size_t start = 0;
  for (std::size_t i = 0; i < kKeyActions; ++i) {
    const std::size_t end = text.find(',', start);
    if ((end == std::string::npos) != (i == kKeyActions - 1)) return std::nullopt;
    const std::string code = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (key_label(code).empty() || !seen.insert(code).second) return std::nullopt;
    map.codes_[i] = code;
    start = end + 1;
  }
  return map;
}

std::string KeyMap::serialize() const {
  std::string text;
  for (const std::string& code : codes_) {
    if (!text.empty()) text += ',';
    text += code;
  }
  return text;
}

int KeyMap::action_for(const std::string& code) const {
  for (std::size_t i = 0; i < kKeyActions; ++i) {
    if (codes_[i] == code) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace piu
