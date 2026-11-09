// JSON parser.

#include "core/json.h"

#include <cstdlib>
#include <utility>

namespace piu {

class JsonParser {
 public:
  explicit JsonParser(std::string_view text) : text_(text) {}

  std::optional<Json> document() {
    Json value;
    if (!parse_value(value, 0)) return std::nullopt;
    skip_space();
    if (pos_ != text_.size()) return std::nullopt;
    return value;
  }

 private:
  static constexpr int kMaxDepth = 32;

  void skip_space() {
    while (pos_ < text_.size() &&
           (text_[pos_] == ' ' || text_[pos_] == '\n' || text_[pos_] == '\r' || text_[pos_] == '\t')) {
      ++pos_;
    }
  }

  bool take(char expected) {
    skip_space();
    if (pos_ < text_.size() && text_[pos_] == expected) {
      ++pos_;
      return true;
    }
    return false;
  }

  bool literal(std::string_view word) {
    if (text_.substr(pos_, word.size()) != word) return false;
    pos_ += word.size();
    return true;
  }

  bool parse_value(Json& out, int depth) {
    if (depth > kMaxDepth) return false;
    skip_space();
    if (pos_ >= text_.size()) return false;
    switch (text_[pos_]) {
      case '{': return parse_object(out, depth);
      case '[': return parse_array(out, depth);
      case '"':
        out.type_ = Json::Type::kString;
        return parse_string(out.string_);
      case 't':
        out.type_ = Json::Type::kBool;
        out.number_ = 1.0;
        return literal("true");
      case 'f':
        out.type_ = Json::Type::kBool;
        return literal("false");
      case 'n':
        return literal("null");
      default:
        return parse_number(out);
    }
  }

  bool parse_object(Json& out, int depth) {
    out.type_ = Json::Type::kObject;
    ++pos_;
    if (take('}')) return true;
    do {
      skip_space();
      std::string key;
      if (!parse_string(key) || !take(':')) return false;
      Json value;
      if (!parse_value(value, depth + 1)) return false;
      out.keys_.push_back(std::move(key));
      out.items_.push_back(std::move(value));
    } while (take(','));
    return take('}');
  }

  bool parse_array(Json& out, int depth) {
    out.type_ = Json::Type::kArray;
    ++pos_;
    if (take(']')) return true;
    do {
      Json value;
      if (!parse_value(value, depth + 1)) return false;
      out.items_.push_back(std::move(value));
    } while (take(','));
    return take(']');
  }

  bool parse_number(Json& out) {
    const std::size_t start = pos_;
    while (pos_ < text_.size()) {
      const char c = text_[pos_];
      if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
        ++pos_;
      } else {
        break;
      }
    }
    if (pos_ == start) return false;
    const std::string token(text_.substr(start, pos_ - start));
    char* end = nullptr;
    out.number_ = std::strtod(token.c_str(), &end);
    out.type_ = Json::Type::kNumber;
    return end == token.c_str() + token.size();
  }

  bool parse_hex4(unsigned& value) {
    if (pos_ + 4 > text_.size()) return false;
    value = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = text_[pos_++];
      value <<= 4;
      if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
      else return false;
    }
    return true;
  }

  static void append_utf8(std::string& out, unsigned code) {
    if (code < 0x80) {
      out += static_cast<char>(code);
    } else if (code < 0x800) {
      out += static_cast<char>(0xC0 | (code >> 6));
      out += static_cast<char>(0x80 | (code & 0x3F));
    } else if (code < 0x10000) {
      out += static_cast<char>(0xE0 | (code >> 12));
      out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (code & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (code >> 18));
      out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (code & 0x3F));
    }
  }

  bool parse_string(std::string& out) {
    if (pos_ >= text_.size() || text_[pos_] != '"') return false;
    ++pos_;
    while (pos_ < text_.size()) {
      const char c = text_[pos_++];
      if (c == '"') return true;
      if (c != '\\') {
        out += c;
        continue;
      }
      if (pos_ >= text_.size()) return false;
      const char escape = text_[pos_++];
      switch (escape) {
        case '"': case '\\': case '/': out += escape; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          unsigned code = 0;
          if (!parse_hex4(code)) return false;
          if (code >= 0xD800 && code < 0xDC00 && literal("\\u")) {
            unsigned low = 0;
            if (!parse_hex4(low)) return false;
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
          }
          append_utf8(out, code);
          break;
        }
        default: return false;
      }
    }
    return false;
  }

  std::string_view text_;
  std::size_t pos_ = 0;
};

std::optional<Json> Json::parse(std::string_view text) { return JsonParser(text).document(); }

const Json* Json::find(std::string_view key) const {
  for (std::size_t i = 0; i < keys_.size(); ++i) {
    if (keys_[i] == key) return &items_[i];
  }
  return nullptr;
}

}  // namespace piu
