// Parses the stage configuration file.

#include "core/stage_cfg.h"

#include <sstream>
#include <string>

namespace piu {

std::optional<StageConfig> parse_stage_cfg(std::string_view text) {
  StageConfig config;
  std::istringstream lines{std::string(text)};
  std::string line;
  while (std::getline(lines, line)) {
    const std::size_t comment = line.find("//");
    if (comment != std::string::npos) line.erase(comment);
    std::istringstream fields(line);
    std::string key;
    if (!(fields >> key)) continue;
    if (key == "OFFSET") {
      fields >> config.cd_offset >> config.cd_preview_offset;
    } else if (key == "TRACK") {
      std::size_t id = 0;
      StageTrack track;
      if (!(fields >> id >> track.bga >> track.title)) return std::nullopt;
      if (config.tracks.size() <= id) config.tracks.resize(id + 1);
      config.tracks[id] = std::move(track);
    } else if (key == "MODE_NAME") {
      StageMode mode;
      fields >> mode.name;
      config.modes.push_back(std::move(mode));
    } else if (key == "SONG_NUM") {
      if (config.modes.empty()) return std::nullopt;
      config.modes.back().stages.emplace_back();
    } else if (key == "SONG") {
      if (config.modes.empty() || config.modes.back().stages.empty()) return std::nullopt;
      StageSong song;
      if (!(fields >> song.level >> song.cd_id >> song.p1_step >> song.p2_step)) return std::nullopt;
      config.modes.back().stages.back().push_back(std::move(song));
    }
  }
  if (config.modes.empty() || config.tracks.empty()) return std::nullopt;
  return config;
}

}  // namespace piu
