// Decoders for the game's file formats and writers for the files the engine reads.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace piu::extract {

using Bytes = std::vector<std::uint8_t>;

struct ResEntry {
  std::string name;
  std::size_t offset = 0;
  std::size_t size = 0;
};

// A RES container: PIU.DAT and SPR.RES. Offsets point into the same buffer.
std::optional<std::vector<ResEntry>> parse_res(const Bytes& data);

struct Image {
  int width = 0;
  int height = 0;
  int channels = 0;
  Bytes pixels;
};

// A .PTX texture: JPEG-style planes with no markers.
std::optional<Image> decode_ptx(const std::uint8_t* data, std::size_t size);

Bytes encode_png(int width, int height, int channels, const Bytes& pixels);
// Reads back only what encode_png writes: unfiltered rows in stored deflate blocks.
std::optional<Image> decode_stored_png(const std::uint8_t* data, std::size_t size);

// INTRO.ANI as one sheet of 16 by 19 frames, with what the screen showed.
std::optional<Image> intro_sheet(const Bytes& data);

// These write the JSON the engine reads. Empty means the input is damaged.
std::string bga_to_json(const Bytes& data, std::string_view source);
std::string stf_to_json(const Bytes& data, std::string_view source);

struct SceneFiles {
  std::vector<Bytes> models;  // T01 to T19.
  Bytes camera;
  std::string materials;      // piu.mtl
  float scale = 0.0f;
};
std::string scene_to_json(const SceneFiles& files);

// One sound effect as a WAV file, or empty if the sound map and ROM do not match.
// `rom` is the sample ROM of the sound board and `table` is PIU.BIN.
std::vector<Bytes> decode_sounds(const Bytes& table, const Bytes& rom);

}  // namespace piu::extract
