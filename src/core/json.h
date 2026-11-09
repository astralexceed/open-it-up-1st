// Small JSON reader for the extracted game data.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace piu {

class Json {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  static std::optional<Json> parse(std::string_view text);

  Type type() const { return type_; }
  double number(double fallback = 0.0) const { return type_ == Type::kNumber ? number_ : fallback; }
  const std::string& string() const { return string_; }
  // Array elements, or the values of an object.
  const std::vector<Json>& items() const { return items_; }
  const Json* find(std::string_view key) const;

 private:
  friend class JsonParser;

  Type type_ = Type::kNull;
  double number_ = 0.0;
  std::string string_;
  std::vector<std::string> keys_;
  std::vector<Json> items_;
};

}  // namespace piu
