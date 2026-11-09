// Parses charts and BGAs and interpolates BGA keyframes.

#include "core/song_data.h"

#include <utility>

#include "core/json.h"

namespace piu {
namespace {

double number_field(const Json& object, std::string_view key) {
  const Json* value = object.find(key);
  return value != nullptr ? value->number() : 0.0;
}

float float_field(const Json& object, std::string_view key) {
  return static_cast<float>(number_field(object, key));
}

std::uint16_t column_mask(const Json& step, std::string_view key) {
  const Json* value = step.find(key);
  if (value == nullptr) return 0;
  std::uint16_t mask = 0;
  const std::string& cells = value->string();
  for (std::size_t column = 0; column < cells.size() && column < 5; ++column) {
    if (cells[column] == '1') mask = static_cast<std::uint16_t>(mask | (1u << column));
  }
  return mask;
}

}  // namespace

std::optional<Chart> parse_chart(std::string_view json) {
  const std::optional<Json> root = Json::parse(json);
  if (!root) return std::nullopt;
  const Json* steps = root->find("steps");
  if (steps == nullptr || steps->type() != Json::Type::kArray) return std::nullopt;

  Chart chart;
  if (const Json* source = root->find("source"); source != nullptr && source->type() == Json::Type::kString) {
    chart.source = source->string();
  }
  chart.first_row_s = number_field(*root, "first_row_time_s");
  chart.row_seconds = number_field(*root, "row_seconds");
  if (chart.row_seconds <= 0.0) return std::nullopt;

  for (const Json& step : steps->items()) {
    ChartRow row;
    row.index = static_cast<int>(number_field(step, "row"));
    row.time = number_field(step, "time_s");
    row.mask = {column_mask(step, "pad_left"), column_mask(step, "pad_right")};
    if (row.mask[0] != 0 || row.mask[1] != 0) chart.rows.push_back(row);
  }
  return chart;
}

int chart_margin(std::string_view chart_name) {
  static const std::pair<std::string_view, int> kTable[] = {
      {"battle1.stf", 0}, {"battle2.stf", 0}, {"battle3.stf", 0}, {"battle4.stf", 0}, {"cat_1.stf", 2},
      {"cat_2.stf", 2},   {"cat_db.stf", 1},  {"dol_db.stf", 1},  {"dol_ez_1.stf", 0}, {"dol_ez_2.stf", 0},
      {"dol_hd_1.stf", 1}, {"dol_hd_2.stf", 1}, {"eu_1.stf", 1},  {"eu_2.stf", 1},    {"eu_db.stf", 1},
      {"ff_db.stf", 1},   {"ff_ez_1.stf", 2}, {"ff_ez_2.stf", 2}, {"ff_hd_1.stf", 1}, {"ff_hd_2.stf", 1},
      {"finkl_1.stf", 1}, {"finkl_2.stf", 1}, {"funky_1.stf", 1}, {"funky_2.stf", 1}, {"funky_db.stf", 1},
      {"hypno_1.stf", 0}, {"hypno_2.stf", 0}, {"hypno_db.stf", 0}, {"ignit_1.stf", 0}, {"ignit_2.stf", 0},
      {"ignit_db.stf", 0}, {"mix1_1.stf", 1}, {"mix1_2.stf", 1},  {"mix2_1.stf", 1},  {"mix2_2.stf", 1},
      {"mix3_1.stf", 1},  {"mix3_2.stf", 1},  {"mix4_1.stf", 0},  {"mix4_2.stf", 0},  {"molla_1.stf", 2},
      {"molla_2.stf", 2}, {"molla_db.stf", 1}, {"niga_1.stf", 1}, {"niga_2.stf", 1},  {"no_ez_1.stf", 1},
      {"no_ez_2.stf", 1}, {"no_hd_1.stf", 1}, {"no_hd_2.stf", 1}, {"nova_1.stf", 0},  {"nova_2.stf", 0},
      {"nova_db.stf", 0}, {"rap_1.stf", 1},   {"rap_2.stf", 1},   {"tiger_1.stf", 2}, {"tiger_2.stf", 2},
      {"tiger_db.stf", 2}, {"yj_db.stf", 1},  {"yj_ez_1.stf", 1}, {"yj_ez_2.stf", 1}, {"yj_hd_1.stf", 1},
      {"yj_hd_2.stf", 1},
  };
  std::string lower(chart_name);
  for (char& c : lower) c = c >= 'A' && c <= 'Z' ? static_cast<char>(c + 'a' - 'A') : c;
  for (const auto& [name, margin] : kTable) {
    if (lower == name) return margin;
  }
  return 0;
}

std::optional<Bga> parse_bga(std::string_view json) {
  const std::optional<Json> root = Json::parse(json);
  if (!root) return std::nullopt;
  const Json* layers = root->find("layers");
  if (layers == nullptr || layers->type() != Json::Type::kArray) return std::nullopt;

  Bga bga;
  bga.strt = static_cast<int>(number_field(*root, "strt"));
  for (const Json& source : layers->items()) {
    BgaLayer layer;
    layer.index = static_cast<int>(number_field(source, "index"));
    if (const Json* name = source.find("name")) layer.name = name->string();
    if (const Json* keys = source.find("keys")) {
      for (const Json& key : keys->items()) {
        layer.keys.push_back({float_field(key, "tick"), float_field(key, "x"), float_field(key, "y"),
                              float_field(key, "pivot_x"), float_field(key, "pivot_y"),
                              float_field(key, "scale"), float_field(key, "rotation"),
                              float_field(key, "red"), float_field(key, "green"),
                              float_field(key, "blue"), float_field(key, "alpha"),
                              number_field(key, "visible") != 0.0});
      }
    }
    bga.layers.push_back(std::move(layer));
  }
  return bga;
}

std::optional<BgaPose> bga_pose(const std::vector<BgaKey>& keys, float t) {
  if (keys.empty() || t < keys.front().tick) return std::nullopt;
  for (std::size_t i = 0; i < keys.size(); ++i) {
    const BgaKey& key = keys[i];
    if (t > key.tick) continue;
    if (i == 0) {
      if (!key.visible) return std::nullopt;
      return BgaPose{&key, &key, 0.0f};
    }
    const BgaKey& previous = keys[i - 1];
    if (!previous.visible) return std::nullopt;
    return BgaPose{&previous, &key, (t - previous.tick) / (key.tick - previous.tick)};
  }
  return std::nullopt;
}

}  // namespace piu
