// The table of modes, stages and songs.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace piu {

struct StageTrack {
  std::string bga;
  std::string title;
};

struct StageSong {
  int level = 0;
  int cd_id = 0;
  std::string p1_step;
  std::string p2_step;
};

struct StageMode {
  std::string name;
  std::vector<std::vector<StageSong>> stages;
};

// stage.cfg, the song table of the game.
struct StageConfig {
  // CD track = CD_ID + offset: the full song, and its short preview clip.
  int cd_offset = 0;
  int cd_preview_offset = 0;
  std::vector<StageTrack> tracks;
  std::vector<StageMode> modes;
};

std::optional<StageConfig> parse_stage_cfg(std::string_view text);

}  // namespace piu
