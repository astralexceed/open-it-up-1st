// Charts and background animations as the engine reads them.

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace piu {

struct ChartRow {
  int index = 0;
  double time = 0.0;
  // One bit per column, for the left and right pad.
  std::array<std::uint16_t, 2> mask{};
};

struct Chart {
  std::string source;  // The chart's file name on the disc.
  double first_row_s = 0.0;
  double row_seconds = 0.0;
  std::vector<ChartRow> rows;
};

std::optional<Chart> parse_chart(std::string_view json);

// Judgment window the original assigns to a chart.
int chart_margin(std::string_view chart_name);

struct BgaKey {
  float tick, x, y, pivot_x, pivot_y, scale, rotation, red, green, blue, alpha;
  bool visible;
};

struct BgaLayer {
  // Slot among the 40 of the file, which the screens address directly.
  int index = 0;
  std::string name;
  std::vector<BgaKey> keys;
};

struct Bga {
  int strt = 0;
  std::vector<BgaLayer> layers;
};

std::optional<Bga> parse_bga(std::string_view json);

// Segment of a layer active at time t, in BGA ticks.
struct BgaPose {
  const BgaKey* from;
  const BgaKey* to;
  float fraction;
};

std::optional<BgaPose> bga_pose(const std::vector<BgaKey>& keys, float t);

}  // namespace piu
