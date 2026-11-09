// Texture, sprite, chart, BGA, model and sound decoders, plus PNG and WAV output.

#include "extractor/formats.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace piu::extract {
namespace {

std::uint32_t u32_at(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
         static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}
std::uint16_t u16_at(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | p[1] << 8); }
float f32_at(const std::uint8_t* p) {
  float value;
  const std::uint32_t bits = u32_at(p);
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::string fixed_name(const std::uint8_t* p, std::size_t size) {
  std::size_t length = 0;
  while (length < size && p[length] != 0) ++length;
  return std::string(reinterpret_cast<const char*>(p), length);
}

// Prints like Python's round(value, decimals), without trailing zeros.
void append_number(std::string& out, double value, int decimals) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
  std::string text = buffer;
  if (text.find('.') != std::string::npos) {
    while (text.back() == '0') text.pop_back();
    if (text.back() == '.') text.pop_back();
  }
  if (text == "-0") text = "0";
  out += text;
}

void append_string(std::string& out, std::string_view text) {
  out += '"';
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    if (static_cast<unsigned char>(c) >= 0x20) out += c;
  }
  out += '"';
}

// ---- PTX ------------------------------------------------------------------

constexpr std::array<int, 64> kZigzag = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

constexpr std::array<std::array<int, 64>, 2> kQuant = {{
    {16, 11, 10, 16, 24,  40,  51,  61,  12, 12, 14, 19, 26,  58,  60,  55,
     14, 13, 16, 24, 40,  57,  69,  56,  14, 17, 22, 29, 51,  87,  80,  62,
     18, 22, 37, 56, 68,  109, 103, 77,  24, 35, 55, 64, 81,  104, 113, 92,
     49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99},
    {17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99, 24, 26, 56, 99, 99, 99,
     99, 99, 47, 66, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99},
}};

constexpr std::array<int, 64> kAan = {
    16384, 22725, 21407, 19266, 16384, 12873, 8867,  4520,  22725, 31521, 29692, 26722, 22725,
    17855, 12299, 6270,  21407, 29692, 27969, 25172, 21407, 16819, 11585, 5906,  19266, 26722,
    25172, 22654, 19266, 15137, 10426, 5315,  16384, 22725, 21407, 19266, 16384, 12873, 8867,
    4520,  12873, 17855, 16819, 15137, 12873, 10114, 6967,  3552,  8867,  12299, 11585, 10426,
    8867,  6967,  4799,  2446,  4520,  6270,  5906,  5315,  4520,  3552,  2446,  1247};

// ITU T.81 Annex K.
constexpr std::array<std::array<int, 16>, 2> kDcCounts = {
    {{0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0}, {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0}}};
constexpr std::array<std::array<int, 16>, 2> kAcCounts = {
    {{0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D}, {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77}}};
const char* const kAcSymbols[2] = {
    "01020300041105122131410613516107227114328191a1082342b1c11552d1f0"
    "2433627282090a161718191a25262728292a3435363738393a43444546474849"
    "4a535455565758595a636465666768696a737475767778797a83848586878889"
    "8a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3c4c5"
    "c6c7c8c9cad2d3d4d5d6d7d8d9dae1e2e3e4e5e6e7e8e9eaf1f2f3f4f5f6f7f8"
    "f9fa",
    "0001020311040521310612415107617113223281081442"
    "91a1b1c109233352f0"
    "156272d10a162434e125f11718191a262728292a35363738393a434445464748"
    "494a535455565758595a636465666768696a737475767778797a828384858687"
    "88898a92939495969798999aa2a3a4a5a6a7a8a9aab2b3b4b5b6b7b8b9bac2c3"
    "c4c5c6c7c8c9cad2d3d4d5d6d7d8d9dae2e3e4e5e6e7e8e9eaf2f3f4f5f6f7f8"
    "f9fa"};

using Lookup = std::vector<std::uint16_t>;

Lookup build_lookup(const std::array<int, 16>& counts, const std::vector<int>& symbols) {
  Lookup table(0x10000, 0);
  int code = 0;
  std::size_t index = 0;
  for (int length = 1; length <= 16; ++length) {
    for (int i = 0; i < counts[static_cast<std::size_t>(length - 1)]; ++i) {
      const int shift = 16 - length;
      const int first = code << shift;
      const auto entry = static_cast<std::uint16_t>(length << 8 | symbols[index]);
      std::fill_n(table.begin() + first, 1 << shift, entry);
      ++index;
      ++code;
    }
    code <<= 1;
  }
  return table;
}

std::vector<int> hex_symbols(const char* text) {
  std::vector<int> out;
  const auto digit = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
  for (const char* p = text; p[0] != 0 && p[1] != 0; p += 2) out.push_back(digit(p[0]) * 16 + digit(p[1]));
  return out;
}

struct Tables {
  std::array<Lookup, 2> dc;
  std::array<Lookup, 2> ac;
  std::array<int, 1024> range{};
  std::array<int, 256> cr_r{}, cb_b{}, cr_g{}, cb_g{};
  Tables() {
    std::vector<int> dc_symbols(12);
    for (int i = 0; i < 12; ++i) dc_symbols[static_cast<std::size_t>(i)] = i;
    for (std::size_t t = 0; t < 2; ++t) {
      dc[t] = build_lookup(kDcCounts[t], dc_symbols);
      ac[t] = build_lookup(kAcCounts[t], hex_symbols(kAcSymbols[t]));
    }
    for (int v = 0; v < 1024; ++v) {
      range[static_cast<std::size_t>(v)] = std::clamp((v >= 512 ? v - 1024 : v) + 128, 0, 255);
    }
    constexpr int kHalf = 1 << 15;
    for (int i = 0; i < 256; ++i) {
      const auto u = static_cast<std::size_t>(i);
      cr_r[u] = (91881 * (i - 128) + kHalf) >> 16;
      cb_b[u] = (116130 * (i - 128) + kHalf) >> 16;
      cr_g[u] = -46802 * (i - 128);
      cb_g[u] = -22554 * (i - 128) + kHalf;
    }
  }
};

const Tables& tables() {
  static const Tables instance;
  return instance;
}

void butterfly(const int* a, int* out) {
  // One pass of the AAN fast integer IDCT.
  const int even0 = a[0] + a[4];
  const int even1 = a[0] - a[4];
  const int even3 = a[2] + a[6];
  const int even2 = ((a[2] - a[6]) * 362 >> 8) - even3;
  const int t0 = even0 + even3;
  const int t3 = even0 - even3;
  const int t1 = even1 + even2;
  const int t2 = even1 - even2;
  const int z13 = a[5] + a[3];
  const int z10 = a[5] - a[3];
  const int z11 = a[1] + a[7];
  const int z12 = a[1] - a[7];
  const int t7 = z13 + z11;
  const int odd = (z11 - z13) * 362 >> 8;
  const int z5 = (z12 + z10) * 473 >> 8;
  const int t6 = ((z10 * -669 >> 8) + z5) - t7;
  const int t5 = odd - t6;
  const int t4 = ((z12 * 277 >> 8) - z5) + t5;
  out[0] = t0 + t7;
  out[1] = t1 + t6;
  out[2] = t2 + t5;
  out[3] = t3 - t4;
  out[4] = t3 + t4;
  out[5] = t2 - t5;
  out[6] = t1 - t6;
  out[7] = t0 - t7;
}

void idct_block(const std::array<int, 64>& c, const std::array<int, 64>& q, std::uint8_t* out, const Tables& t) {
  std::array<int, 64> work{};
  for (std::size_t x = 0; x < 8; ++x) {
    if ((c[x + 8] | c[x + 16] | c[x + 24] | c[x + 32] | c[x + 40] | c[x + 48] | c[x + 56]) == 0) {
      const int dc = c[x] * q[x];
      for (std::size_t r = 0; r < 64; r += 8) work[x + r] = dc;
      continue;
    }
    int column[8];
    for (std::size_t r = 0; r < 8; ++r) column[r] = c[x + r * 8] * q[x + r * 8];
    int result[8];
    butterfly(column, result);
    for (std::size_t r = 0; r < 8; ++r) work[x + r * 8] = result[r];
  }
  for (std::size_t r = 0; r < 64; r += 8) {
    int row[8];
    butterfly(&work[r], row);
    for (std::size_t x = 0; x < 8; ++x) {
      out[r + x] = static_cast<std::uint8_t>(t.range[static_cast<std::size_t>((row[x] >> 5) & 0x3FF)]);
    }
  }
}

constexpr std::size_t kPadding = 512;

// Returns the bits consumed, or 0 if the stream is damaged.
std::size_t decode_plane(const std::uint8_t* data, std::size_t size, std::size_t start, std::uint8_t* plane,
                         std::size_t pixels, std::size_t table, const std::array<int, 64>& q, const Tables& t) {
  const Lookup& dc = t.dc[table];
  const Lookup& ac = t.ac[table];
  std::size_t position = start * 8;
  int predictor = 0;
  const auto peek16 = [&]() -> int {
    const std::size_t byte = position >> 3;
    const unsigned word = static_cast<unsigned>(data[byte]) << 16 | static_cast<unsigned>(data[byte + 1]) << 8 | data[byte + 2];
    return static_cast<int>((word >> (8 - (position & 7))) & 0xFFFF);
  };
  for (std::size_t offset = 0; offset < pixels; offset += 64) {
    std::array<int, 64> coefficients{};
    int entry = dc[static_cast<std::size_t>(peek16())];
    if (entry == 0) return 0;
    position += static_cast<std::size_t>(entry >> 8);
    int bits = entry & 0xFF;
    if (bits != 0) {
      int value = peek16() >> (16 - bits);
      position += static_cast<std::size_t>(bits);
      if ((value & (1 << (bits - 1))) == 0) value -= (1 << bits) - 1;
      predictor += value;
    }
    coefficients[0] = predictor;

    for (int k = 1; k < 64;) {
      entry = ac[static_cast<std::size_t>(peek16())];
      if (entry == 0) return 0;
      position += static_cast<std::size_t>(entry >> 8);
      const int run = (entry & 0xFF) >> 4;
      bits = entry & 0x0F;
      int value = 0;
      if (bits != 0) {
        value = peek16() >> (16 - bits);
        position += static_cast<std::size_t>(bits);
        if ((value & (1 << (bits - 1))) == 0) value -= (1 << bits) - 1;
      } else if (run == 0) {
        break;
      }
      k += run;
      if (k >= 64) return 0;
      coefficients[static_cast<std::size_t>(kZigzag[static_cast<std::size_t>(k)])] = value;
      ++k;
    }
    idct_block(coefficients, q, plane + offset, t);
    if ((position >> 3) > size - kPadding) return 0;
  }
  return position - start * 8;
}

// ---- PNG ------------------------------------------------------------------

std::uint32_t crc_update(std::uint32_t crc, const std::uint8_t* p, std::size_t n) {
  static const std::array<std::uint32_t, 256> table = [] {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
      std::uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      t[i] = c;
    }
    return t;
  }();
  for (std::size_t i = 0; i < n; ++i) crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}

void put_be32(Bytes& out, std::uint32_t v) {
  out.push_back(static_cast<std::uint8_t>(v >> 24));
  out.push_back(static_cast<std::uint8_t>(v >> 16));
  out.push_back(static_cast<std::uint8_t>(v >> 8));
  out.push_back(static_cast<std::uint8_t>(v));
}

void put_chunk(Bytes& out, const char* tag, const Bytes& body) {
  put_be32(out, static_cast<std::uint32_t>(body.size()));
  const std::size_t start = out.size();
  out.insert(out.end(), tag, tag + 4);
  out.insert(out.end(), body.begin(), body.end());
  const std::uint32_t crc = crc_update(0xFFFFFFFFu, out.data() + start, out.size() - start) ^ 0xFFFFFFFFu;
  put_be32(out, crc);
}

}  // namespace

std::optional<std::vector<ResEntry>> parse_res(const Bytes& data) {
  if (data.size() < 16 || std::memcmp(data.data(), "RES\0", 4) != 0) return std::nullopt;
  const std::uint32_t count = u32_at(&data[8]);
  const std::uint32_t payload_size = u32_at(&data[12]);
  const std::size_t payload = 16 + std::size_t{count} * 24;
  if (payload + payload_size != data.size()) return std::nullopt;
  std::vector<ResEntry> entries;
  entries.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    const std::uint8_t* p = &data[16 + std::size_t{i} * 24];
    const std::uint32_t size = u32_at(p + 16);
    const std::uint32_t offset = u32_at(p + 20);
    if (std::size_t{offset} + size > payload_size) return std::nullopt;
    entries.push_back({fixed_name(p, 16), payload + offset, size});
  }
  return entries;
}

std::optional<Image> decode_ptx(const std::uint8_t* source, std::size_t source_size) {
  if (source_size < 16 || std::memcmp(source, "PTX\0", 4) != 0) return std::nullopt;
  const int blocks_w = u16_at(source + 6);
  const int blocks_h = u16_at(source + 8);
  const int format = u16_at(source + 10);
  const int quality = u16_at(source + 12);
  if (u16_at(source + 4) != 0x100 || blocks_w == 0 || blocks_h == 0 || quality <= 0) return std::nullopt;
  if (format != 0x81 && format != 0x82) return std::nullopt;
  const int channels = format == 0x81 ? 3 : 4;
  const int width = blocks_w * 8;
  const int height = blocks_h * 8;
  const auto pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

  Bytes data(source, source + source_size);
  data.resize(source_size + kPadding, 0);
  const Tables& t = tables();
  std::array<std::array<int, 64>, 2> quant{};
  for (std::size_t table = 0; table < 2; ++table) {
    for (std::size_t i = 0; i < 64; ++i) {
      quant[table][i] = (kAan[i] * kQuant[table][i] * 100 / quality + 0x800) >> 12;
    }
  }
  constexpr std::array<std::size_t, 4> kTableForPlane = {0, 1, 1, 0};
  std::array<Bytes, 4> planes;
  std::size_t used = 0;
  for (std::size_t i = 0; i < static_cast<std::size_t>(channels); ++i) {
    planes[i].resize(pixels);
    const std::size_t table = kTableForPlane[i];
    const std::size_t bits = decode_plane(data.data(), data.size(), 16 + used, planes[i].data(), pixels, table, quant[table], t);
    if (bits == 0) return std::nullopt;
    used += (bits + 7) / 8;
  }

  Image image{width, height, channels, Bytes(pixels * static_cast<std::size_t>(channels))};
  const auto clamp8 = [](int v) { return static_cast<std::uint8_t>(std::clamp(v, 0, 255)); };
  std::size_t cursor = 0;
  for (int block_y = 0; block_y < height; block_y += 8) {
    for (int block_x = 0; block_x < width; block_x += 8) {
      for (int row = 0; row < 8; ++row) {
        std::size_t target = (static_cast<std::size_t>(block_y + row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(block_x)) * static_cast<std::size_t>(channels);
        for (int x = 0; x < 8; ++x) {
          const int y = planes[0][cursor];
          const auto cb = planes[1][cursor];
          const auto cr = planes[2][cursor];
          image.pixels[target] = clamp8(y + t.cr_r[cr]);
          image.pixels[target + 1] = clamp8(y + ((t.cb_g[cb] + t.cr_g[cr]) >> 16));
          image.pixels[target + 2] = clamp8(y + t.cb_b[cb]);
          if (channels == 4) image.pixels[target + 3] = planes[3][cursor];
          target += static_cast<std::size_t>(channels);
          ++cursor;
        }
      }
    }
  }
  return image;
}

Bytes encode_png(int width, int height, int channels, const Bytes& pixels) {
  // Stored deflate blocks: no compression, so the browser opens them quickly.
  const auto stride = static_cast<std::size_t>(width) * static_cast<std::size_t>(channels);
  Bytes raw;
  raw.reserve((stride + 1) * static_cast<std::size_t>(height));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);
    const auto start = static_cast<std::size_t>(y) * stride;
    raw.insert(raw.end(), pixels.begin() + static_cast<std::ptrdiff_t>(start), pixels.begin() + static_cast<std::ptrdiff_t>(start + stride));
  }

  Bytes idat = {0x78, 0x01};
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for (std::size_t offset = 0; offset < raw.size();) {
    const std::size_t count = std::min<std::size_t>(0xFFFF, raw.size() - offset);
    idat.push_back(offset + count == raw.size() ? 1 : 0);
    idat.push_back(static_cast<std::uint8_t>(count));
    idat.push_back(static_cast<std::uint8_t>(count >> 8));
    idat.push_back(static_cast<std::uint8_t>(~count));
    idat.push_back(static_cast<std::uint8_t>(~count >> 8));
    idat.insert(idat.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset), raw.begin() + static_cast<std::ptrdiff_t>(offset + count));
    offset += count;
  }
  for (std::uint8_t byte : raw) {
    a = (a + byte) % 65521;
    b = (b + a) % 65521;
  }
  put_be32(idat, b << 16 | a);

  Bytes out = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  Bytes header;
  put_be32(header, static_cast<std::uint32_t>(width));
  put_be32(header, static_cast<std::uint32_t>(height));
  header.insert(header.end(), {8, static_cast<std::uint8_t>(channels == 4 ? 6 : 2), 0, 0, 0});
  put_chunk(out, "IHDR", header);
  put_chunk(out, "IDAT", idat);
  put_chunk(out, "IEND", {});
  return out;
}

std::optional<Image> decode_stored_png(const std::uint8_t* data, std::size_t size) {
  static constexpr std::uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (size < 8 || std::memcmp(data, kSignature, 8) != 0) return std::nullopt;
  const auto be32 = [](const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) << 24 | static_cast<std::uint32_t>(p[1]) << 16 |
           static_cast<std::uint32_t>(p[2]) << 8 | static_cast<std::uint32_t>(p[3]);
  };

  Image image{0, 0, 0, {}};
  Bytes stream;
  for (std::size_t offset = 8; offset + 12 <= size;) {
    const std::uint32_t length = be32(&data[offset]);
    if (length > size - offset - 12) return std::nullopt;
    const std::uint8_t* body = &data[offset + 8];
    if (std::memcmp(&data[offset + 4], "IHDR", 4) == 0 && length == 13) {
      if (body[8] != 8 || (body[9] != 2 && body[9] != 6)) return std::nullopt;
      image.width = static_cast<int>(be32(body));
      image.height = static_cast<int>(be32(body + 4));
      image.channels = body[9] == 6 ? 4 : 3;
    } else if (std::memcmp(&data[offset + 4], "IDAT", 4) == 0) {
      stream.insert(stream.end(), body, body + length);
    }
    offset += std::size_t{length} + 12;
  }
  if (image.width <= 0 || image.height <= 0 || stream.size() < 2) return std::nullopt;

  const auto stride = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.channels);
  const std::size_t rows = static_cast<std::size_t>(image.height) * (stride + 1);
  Bytes raw;
  raw.reserve(rows);
  for (std::size_t at = 2; raw.size() < rows;) {
    if (at + 5 > stream.size() || (stream[at] & 0x06) != 0) return std::nullopt;
    const std::size_t count = stream[at + 1] | std::size_t{stream[at + 2]} << 8;
    if (at + 5 + count > stream.size()) return std::nullopt;
    raw.insert(raw.end(), stream.begin() + static_cast<std::ptrdiff_t>(at + 5),
               stream.begin() + static_cast<std::ptrdiff_t>(at + 5 + count));
    at += 5 + count;
  }

  image.pixels.resize(stride * static_cast<std::size_t>(image.height));
  for (std::size_t y = 0; y < static_cast<std::size_t>(image.height); ++y) {
    const std::size_t row = y * (stride + 1);
    if (raw[row] != 0) return std::nullopt;
    std::memcpy(&image.pixels[y * stride], &raw[row + 1], stride);
  }
  return image;
}

std::optional<Image> intro_sheet(const Bytes& data) {
  constexpr std::uint32_t kFrames = 16 * 19;
  constexpr int kFrameW = 160;
  constexpr int kFrameH = 120;
  if (data.size() < 12 || std::memcmp(data.data(), "PTX\0", 4) != 0 || u32_at(&data[4]) != kFrames) return std::nullopt;
  const std::uint32_t size = u32_at(&data[8]);
  const std::size_t base = 12 + std::size_t{kFrames} * 4;
  if (base + size != data.size()) return std::nullopt;

  Image sheet{kFrameW * 16, kFrameH * 19, 3, Bytes(static_cast<std::size_t>(kFrameW) * 16 * kFrameH * 19 * 3)};
  const auto stride = static_cast<std::size_t>(sheet.width) * 3;
  for (std::uint32_t i = 0; i < kFrames; ++i) {
    const std::uint32_t start = u32_at(&data[12 + std::size_t{i} * 4]);
    const std::uint32_t end = i + 1 < kFrames ? u32_at(&data[12 + (std::size_t{i} + 1) * 4]) : size;
    if (start >= end || end > size) return std::nullopt;
    std::optional<Image> frame = decode_ptx(&data[base + start], end - start);
    if (!frame || frame->width != kFrameW || frame->height != kFrameH || frame->channels != 3) return std::nullopt;
    const auto cell_x = static_cast<std::size_t>(i % 16) * kFrameW;
    const auto cell_y = static_cast<std::size_t>(i / 16) * kFrameH;
    // The first byte goes into the low bits, so red and blue trade places on screen.
    for (std::size_t y = 0; y < kFrameH; ++y) {
      std::uint8_t* out = &sheet.pixels[(cell_y + y) * stride + cell_x * 3];
      const std::uint8_t* in = &frame->pixels[y * kFrameW * 3];
      for (std::size_t x = 0; x < kFrameW; ++x, in += 3, out += 3) {
        out[0] = static_cast<std::uint8_t>((in[2] >> 3) << 3 | in[2] >> 5);
        out[1] = static_cast<std::uint8_t>((in[1] >> 2) << 2 | in[1] >> 6);
        out[2] = static_cast<std::uint8_t>((in[0] >> 3) << 3 | in[0] >> 5);
      }
    }
  }
  return sheet;
}

std::vector<Bytes> decode_sounds(const Bytes& table, const Bytes& rom) {
  // The YMZ280B's 4-bit ADPCM: signal and step, with one nibble per sample.
  constexpr std::size_t kRecord = 16;
  constexpr int kDiff[16] = {1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15};
  constexpr int kScale[8] = {0xE6, 0xE6, 0xE6, 0xE6, 0x133, 0x199, 0x200, 0x266};
  constexpr int kMinStep = 0x7F;
  constexpr int kMaxStep = 0x6000;
  constexpr int kChipRate = 44100;
  // A wrong file clips constantly; the real samples almost never do.
  constexpr std::size_t kMaxClipDivisor = 100;

  if (table.empty() || table.size() % kRecord != 0) return {};
  std::vector<Bytes> sounds;
  std::size_t nibbles_total = 0;
  std::size_t clipped_total = 0;
  for (std::size_t record = 0; record < table.size(); record += kRecord) {
    const std::uint8_t* r = &table[record];
    const auto address = [r](std::size_t index) {
      return static_cast<std::size_t>(r[4 + index]) << 16 | static_cast<std::size_t>(r[8 + index]) << 8 | r[12 + index];
    };
    const std::size_t start = address(0);
    const std::size_t end = address(3);
    if (end < start || end >= rom.size()) return {};
    const std::size_t nibbles = (end - start + 1) * 2;
    const auto pcm_bytes = static_cast<std::uint32_t>(nibbles * 2);
    const int rate = kChipRate * (r[0] + 1) / 256;

    Bytes wav(44 + std::size_t{pcm_bytes});
    const auto put32 = [&wav](std::size_t at, std::uint32_t v) {
      for (std::size_t i = 0; i < 4; ++i) wav[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
    };
    std::memcpy(&wav[0], "RIFF", 4);
    put32(4, 36 + pcm_bytes);
    std::memcpy(&wav[8], "WAVEfmt ", 8);
    put32(16, 16);
    put32(20, 0x00010001);  // PCM, mono.
    put32(24, static_cast<std::uint32_t>(rate));
    put32(28, static_cast<std::uint32_t>(rate) * 2);
    put32(32, 0x00100002);  // 2 bytes per sample, 16 bits.
    std::memcpy(&wav[36], "data", 4);
    put32(40, pcm_bytes);

    int signal = 0;
    int step = kMinStep;
    for (std::size_t i = 0; i < nibbles; ++i) {
      const std::size_t position = start * 2 + i;
      const int value = (rom[position / 2] >> ((position & 1) != 0 ? 0 : 4)) & 0x0F;
      signal += step * kDiff[value] / 8;
      if (signal > 32767 || signal < -32768) ++clipped_total;
      signal = std::clamp(signal, -32768, 32767);
      step = std::clamp(step * kScale[value & 7] >> 8, kMinStep, kMaxStep);
      wav[44 + i * 2] = static_cast<std::uint8_t>(signal);
      wav[45 + i * 2] = static_cast<std::uint8_t>(signal >> 8);
    }
    nibbles_total += nibbles;
    sounds.push_back(std::move(wav));
  }
  if (clipped_total > nibbles_total / kMaxClipDivisor) return {};
  return sounds;
}

std::string bga_to_json(const Bytes& data, std::string_view source) {
  constexpr std::size_t kHeader = 16, kName = 64, kKey = 44;
  constexpr int kLayers = 40;
  if (data.size() < kHeader || std::memcmp(data.data(), "BGA\0", 4) != 0) return {};
  const int bpm = static_cast<int>(u32_at(&data[4]));
  const int track = static_cast<int>(u32_at(&data[8]));
  const auto strt = static_cast<std::int32_t>(u32_at(&data[12]));

  std::string out = "{\"source\":";
  append_string(out, source);
  out += ",\"bpm\":" + std::to_string(bpm) + ",\"track\":" + std::to_string(track) + ",\"strt\":" + std::to_string(strt) + ",\"layers\":[";
  std::size_t offset = kHeader;
  bool first_layer = true;
  for (int index = 0; index < kLayers; ++index) {
    if (offset + kName + 4 > data.size()) return {};
    const std::string name = fixed_name(&data[offset], kName);
    const std::uint32_t count = u32_at(&data[offset + kName]);
    offset += kName + 4;
    if (offset + std::size_t{count} * kKey > data.size()) return {};
    if (!name.empty() || count != 0) {
      if (!first_layer) out += ',';
      first_layer = false;
      out += "{\"index\":" + std::to_string(index) + ",\"name\":";
      append_string(out, name);
      out += ",\"keys\":[";
      for (std::uint32_t k = 0; k < count; ++k) {
        const std::uint8_t* p = &data[offset + std::size_t{k} * kKey];
        const int tick = u16_at(p);
        if (k != 0) out += ',';
        out += "{\"tick\":" + std::to_string(tick) + ",\"time_s\":";
        append_number(out, strt / 100.0 + tick / 60.0, 6);
        out += ",\"visible\":" + std::to_string(u16_at(p + 2));
        static constexpr std::array<const char*, 10> kFields = {"x", "y", "pivot_x", "pivot_y", "scale", "rotation", "red", "green", "blue", "alpha"};
        for (std::size_t f = 0; f < kFields.size(); ++f) {
          out += ",\"";
          out += kFields[f];
          out += "\":";
          append_number(out, f32_at(p + 4 + f * 4), 5);
        }
        out += '}';
      }
      out += "]}";
    }
    offset += std::size_t{count} * kKey;
  }
  if (offset != data.size()) return {};
  out += "]}";
  return out;
}

std::string stf_to_json(const Bytes& data, std::string_view source) {
  constexpr std::size_t kSize = 14616, kTitle = 252, kRows = 0x118, kRow = 14;
  constexpr int kRowCount = 1024;
  // Measured against the audio of all 23 songs: row 0 is Strt / 100 plus this.
  constexpr double kAudioOffset = 0.035;
  if (data.size() != kSize) return {};
  const double bpm = f32_at(&data[kTitle + 4]);
  const std::uint32_t rows_per_beat = u32_at(&data[kTitle + 12]);
  const std::uint32_t track = u32_at(&data[kTitle + 20]);
  const std::uint32_t strt = u32_at(&data[kTitle + 24]);
  if (bpm <= 0.0 || rows_per_beat == 0) return {};
  const double row_seconds = 60.0 / (bpm * rows_per_beat);
  const double first_row = strt / 100.0 + kAudioOffset;

  std::string out = "{\"source\":";
  append_string(out, source);
  out += ",\"bpm\":";
  append_number(out, bpm, 4);
  out += ",\"rows_per_beat\":" + std::to_string(rows_per_beat) + ",\"cd_track\":" + std::to_string(track + 1) + ",\"strt\":" + std::to_string(strt) + ",\"first_row_time_s\":";
  append_number(out, first_row, 6);
  out += ",\"row_seconds\":";
  append_number(out, row_seconds, 6);
  out += ",\"steps\":[";
  bool first = true;
  for (int i = 0; i < kRowCount; ++i) {
    const auto* row = reinterpret_cast<const char*>(&data[kRows + static_cast<std::size_t>(i) * kRow]);
    if (row[13] != 0) return {};
    const std::string_view cells(row, 13);
    if (cells.find_first_not_of('0') == std::string_view::npos) continue;
    if (!first) out += ',';
    first = false;
    out += "{\"row\":" + std::to_string(i) + ",\"time_s\":";
    append_number(out, first_row + i * row_seconds, 6);
    out += ",\"pad_left\":\"";
    out += cells.substr(0, 5);
    out += "\",\"pad_right\":\"";
    out += cells.substr(5, 5);
    out += "\",\"extra\":\"";
    out += cells.substr(10);
    out += "\"}";
  }
  out += "]}";
  return out;
}

namespace {

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

bool append_model(std::string& out, const Bytes& data, const std::string& name) {
  if (data.size() < 60) return false;
  const auto get = [&](std::size_t at) { return u32_at(&data[at]); };
  const std::uint32_t vertices = get(0), texcoords = get(4), normals = get(8), groups = get(12), polygons = get(16), elements = get(20);
  std::size_t offset = 60;
  const std::size_t v_start = offset;
  offset += std::size_t{vertices} * 12;
  const std::size_t t_start = offset;
  offset += std::size_t{texcoords} * 12 + std::size_t{normals} * 12 + std::size_t{groups} * 40;
  const std::size_t f_start = offset;
  offset += std::size_t{polygons} * 36;
  if (offset > data.size()) return false;

  out += "{\"name\":";
  append_string(out, name);
  out += ",\"positions\":[";
  for (std::uint32_t i = 0; i < vertices * 3; ++i) {
    if (i != 0) out += ',';
    append_number(out, f32_at(&data[v_start + std::size_t{i} * 4]), 4);
  }
  out += "],\"uvs\":[";
  for (std::uint32_t i = 0; i < texcoords; ++i) {
    if (i != 0) out += ',';
    append_number(out, f32_at(&data[t_start + std::size_t{i} * 12]), 5);
    out += ',';
    append_number(out, f32_at(&data[t_start + std::size_t{i} * 12 + 4]), 5);
  }
  out += "],\"parts\":[";
  for (std::uint32_t e = 0; e < elements; ++e) {
    if (offset + 88 > data.size()) return false;
    const std::uint32_t n = get(offset);
    const std::string material = fixed_name(&data[offset + 4 + 0x14], 84 - 0x14);
    offset += 88;
    if (e != 0) out += ',';
    out += "{\"material\":";
    append_string(out, material);
    out += ",\"faces\":[";
    bool first_face = true;
    for (std::uint32_t i = 0; i < n; ++i) {
      if (offset + 72 > data.size()) return false;
      const std::uint32_t first = get(offset + 64);
      const std::uint32_t count = get(offset + 68);
      offset += 72;
      if (std::size_t{first} + count > polygons) return false;
      for (std::uint32_t f = first; f < first + count; ++f) {
        if (!first_face) out += ',';
        first_face = false;
        out += '[';
        for (std::size_t v = 0; v < 6; ++v) {
          if (v != 0) out += ',';
          out += std::to_string(get(f_start + std::size_t{f} * 36 + v * 4));
        }
        out += ']';
      }
    }
    out += "]}";
  }
  out += "]}";
  return offset == data.size();
}

}  // namespace

std::string scene_to_json(const SceneFiles& files) {
  if (files.camera.size() < 2) return {};
  const std::size_t keys = u16_at(files.camera.data());
  if (files.camera.size() != 2 + keys * 24) return {};
  std::string out = "{\"camera\":[";
  for (std::size_t i = 0; i < keys; ++i) {
    if (i != 0) out += ',';
    out += '[';
    for (std::size_t v = 0; v < 6; ++v) {
      if (v != 0) out += ',';
      append_number(out, static_cast<double>(f32_at(&files.camera[2 + i * 24 + v * 4])) * static_cast<double>(files.scale), 4);
    }
    out += ']';
  }
  out += "],\"scale\":";
  append_number(out, files.scale, 7);
  out += ",\"materials\":{";

  std::istringstream lines(files.materials);
  std::string line;
  std::getline(lines, line);  // The first line is a count.
  bool first = true;
  std::vector<std::string> seen;
  while (std::getline(lines, line)) {
    std::istringstream fields(line);
    std::string texture, name;
    if (!(fields >> texture >> name)) continue;
    name = lower(name);
    if (std::find(seen.begin(), seen.end(), name) != seen.end()) continue;
    seen.push_back(name);
    const std::size_t dot = texture.rfind('.');
    if (dot != std::string::npos) texture.erase(dot);
    if (!first) out += ',';
    first = false;
    append_string(out, name);
    out += ':';
    append_string(out, lower(texture));
  }
  out += "},\"models\":[";
  char name[8];
  for (std::size_t i = 0; i < files.models.size(); ++i) {
    if (i != 0) out += ',';
    std::snprintf(name, sizeof(name), "T%02d", static_cast<int>(i + 1));
    if (!append_model(out, files.models[i], name)) return {};
  }
  out += "]}";
  return out;
}

}  // namespace piu::extract
