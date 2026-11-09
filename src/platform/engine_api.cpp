// Holds the engine state and connects each screen to the platform.

#include "platform/engine_api.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/assets.h"
#include "core/attract.h"
#include "core/clock.h"
#include "core/frame_limiter.h"
#include "core/game.h"
#include "core/calibration_screen.h"
#include "core/key_map_screen.h"
#include "core/ui_atlas.h"
#include "core/machine_settings.h"
#include "core/results.h"
#include "core/scene.h"
#include "core/select_screen.h"
#include "core/stage_cfg.h"
#include "core/service_menu.h"
#include "core/song_data.h"
#include "render/gl_renderer.h"

namespace {

struct Engine {
  piu::FixedClock clock;
  piu::FrameLimiter frame_limiter;
  std::optional<piu::KeyMapScreen> key_map;
  std::optional<piu::CalibrationScreen> calibration;
  std::string calibration_track;
  piu::FixedClock calibration_clock;
  bool calibration_clock_running = false;
  int audio_offset_ms = 0;
  piu::Texture key_map_atlas;
  std::string key_map_text;
  piu::GlRenderer renderer;
  piu::DrawList draw_list;
  piu::Assets assets;
  std::optional<piu::Chart> chart;
  std::optional<piu::Bga> bga;
  piu::Bga gameview;
  std::optional<piu::Scene> flight;
  piu::CoinBox coins;
  std::string credit_text;
  std::optional<piu::Game> game;
  std::optional<piu::ResultsScreen> results;
  piu::MachineSettings settings;
  std::optional<piu::ServiceMenu> service;
  std::optional<piu::Bga> andamiro;
  std::optional<piu::Bga> start_screen;
  std::optional<piu::AttractLoop> attract;
  std::optional<piu::StageConfig> stage_config;
  std::optional<piu::Bga> mode_bga;
  std::optional<piu::Bga> music_bga;
  std::optional<piu::Bga> cd_bga;
  std::optional<piu::Bga> help_bga;
  piu::SelectSession session;
  std::optional<piu::SelectScreen> select;
  std::optional<piu::HelpScreen> help;
  std::string select_text;
  bool game_over = true;
  piu::SoundQueue coin_sounds;
  std::string request;
  std::string missing;
};

Engine* engine = nullptr;

}  // namespace

extern "C" {

PIU_EXPORT int piu_engine_start() {
  engine = new Engine();
  return engine->renderer.init() ? 1 : 0;
}

PIU_EXPORT void* piu_alloc(int size) { return std::malloc(static_cast<std::size_t>(size)); }
PIU_EXPORT void piu_free(void* data) { std::free(data); }

// Parses a song and queues the files it needs.
PIU_EXPORT int piu_prepare(const char* chart, int chart_size, const char* bga, int bga_size,
                                     const char* gameview, int gameview_size) {
  engine->game.reset();
  engine->chart = piu::parse_chart(std::string_view(chart, static_cast<std::size_t>(chart_size)));
  engine->bga = piu::parse_bga(std::string_view(bga, static_cast<std::size_t>(bga_size)));
  if (!engine->chart || !engine->bga) {
    return 0;
  }
  std::optional<piu::Bga> overlay = piu::parse_bga(std::string_view(gameview, static_cast<std::size_t>(gameview_size)));
  engine->gameview = overlay ? std::move(*overlay) : piu::Bga{};
  piu::request_song_assets(*engine->bga, engine->assets);
  piu::request_song_assets(engine->gameview, engine->assets);
  return 1;
}

// Loads the 3D flight once and queues its textures.
PIU_EXPORT int piu_load_flight(const char* json, int size) {
  if (engine->flight) return 1;
  engine->flight = piu::parse_scene(std::string_view(json, static_cast<std::size_t>(size)));
  if (!engine->flight) return 0;
  piu::request_scene_assets(*engine->flight, engine->assets);
  return 1;
}

PIU_EXPORT const char* piu_next_asset() {
  std::optional<std::string> next = engine->assets.next_request();
  if (!next) {
    return nullptr;
  }
  engine->request = std::move(*next);
  return engine->request.c_str();
}

PIU_EXPORT void piu_provide_sprite(const char* key, const char* text, int size) {
  engine->assets.set_sprite(key, std::string_view(text, static_cast<std::size_t>(size)));
}

PIU_EXPORT void piu_missing_sprite(const char* key) { engine->assets.mark_sprite_missing(key); }
PIU_EXPORT void piu_missing_texture(const char* key) { engine->assets.mark_texture_missing(key); }

// Creates and binds a texture; the page uploads the decoded image right after.
PIU_EXPORT void piu_begin_texture(const char* key, int width, int height) {
  engine->assets.set_texture(key, {engine->renderer.begin_texture(), width, height});
}

// Starts the prepared song. Returns 0 if the arrow textures are missing.
PIU_EXPORT void piu_coin_configure(int per_credit, int free_play) {
  engine->coins.configure(per_credit, free_play != 0);
}
PIU_EXPORT void piu_coin_add(int credits) { engine->coins.add_credits(credits); }
PIU_EXPORT void piu_coin_reset() { engine->coins.reset(); }
PIU_EXPORT void piu_coin_insert() {
  engine->coin_sounds.play(engine->coins.next_coin_sound(), 0);
  engine->coins.insert();
}
PIU_EXPORT int piu_coin_can_start(int credits) { return engine->coins.can_start(credits) ? 1 : 0; }
PIU_EXPORT int piu_coin_consume() { return engine->coins.consume() ? 1 : 0; }
PIU_EXPORT const char* piu_coin_text() {
  engine->credit_text = engine->coins.text();
  return engine->credit_text.c_str();
}

PIU_EXPORT int piu_begin(int p1_word, int p2_word, int margin, int double_play, int stage, int life_level,
                                   int use_flight, unsigned seed, int demo, int right_only, double now_ms, double song_ms) {
  const piu::Scene* flight = nullptr;
  if (use_flight != 0 && engine->flight) {
    piu::resolve_scene(*engine->flight, engine->assets);
    flight = &*engine->flight;
  }
  piu::GameOptions options;
  options.option_words = {static_cast<std::uint32_t>(p1_word), static_cast<std::uint32_t>(p2_word)};
  options.random_seed = seed;
  options.margin = margin;
  options.stage = stage;
  options.life_level = life_level;
  options.double_play = double_play != 0;
  options.flight = flight;
  options.machine_difficulty = engine->settings.difficulty;
  options.stage_break = engine->settings.stage_break;
  options.coins = &engine->coins;
  options.demo = demo != 0;
  options.right_side_only = right_only != 0;
  piu::Game game(std::move(*engine->chart), *engine->bga, engine->gameview, engine->assets, options);
  engine->chart.reset();

  engine->missing.clear();
  for (const std::string& name : game.missing()) {
    if (!engine->missing.empty()) engine->missing += ", ";
    engine->missing += name;
  }
  if (!game.has_arrows()) {
    return 0;
  }
  engine->game.emplace(std::move(game));
  engine->clock.start(now_ms, song_ms - engine->audio_offset_ms);
  return 1;
}

PIU_EXPORT const char* piu_missing() { return engine->missing.c_str(); }
PIU_EXPORT void piu_stop() {
  engine->game.reset();
  engine->results.reset();
  engine->service.reset();
  engine->attract.reset();
  engine->select.reset();
  engine->help.reset();
}

// Moves to DANCE GRADE if somebody cleared the song. Otherwise there is no such screen.
PIU_EXPORT int piu_show_results(int stage, double now_ms) {
  if (!engine->game) return 0;
  const piu::Game& game = *engine->game;
  std::array<piu::ResultPlayer, 2> players{};
  bool cleared = false;
  int players_in_game = 0;
  int players_with_f = 0;
  for (int p = 0; p < 2; ++p) {
    if (!game.uses_player(p)) continue;
    const piu::PlayerStats& stats = game.stats(p);
    cleared = cleared || stats.state == piu::LifeState::kCleared;
    auto& player = players[static_cast<std::size_t>(p)];
    player.active = true;
    for (std::size_t i = 0; i < 5; ++i) player.counts[i] = stats.tally[i];
    player.counts[5] = stats.max_combo;
    player.points = stats.points;
    player.grade = game.grade(p);
    ++players_in_game;
    if (player.grade == 'F') ++players_with_f;
  }
  if (!cleared) {
    engine->game_over = true;
    return 0;
  }
  // These cases end the credit, and the screen closes with a fade.
  const bool game_over =
      engine->settings.game_mode == 1 || game.uses_flight() || game.is_battle() || stage == 3 || players_with_f == players_in_game;
  engine->game_over = game_over;
  engine->results.emplace(players, engine->assets, game_over, game.is_battle());
  engine->clock.start(now_ms, 0.0);
  return 1;
}

PIU_EXPORT int piu_results_finished() {
  return engine->results && engine->results->finished() ? 1 : 0;
}

PIU_EXPORT void piu_input(int panel, int down, double time_ms) {
  if (engine != nullptr && engine->results && down != 0 && (panel == 2 || panel == 7)) {
    engine->results->press_center(panel / 5);
  }
  if (engine != nullptr && engine->attract && !engine->game && !engine->select && !engine->help && down != 0 &&
      (panel == 2 || panel == 7)) {
    engine->attract->press_center(panel / 5);
  }
  if (engine != nullptr && engine->help && down != 0 && (panel == 2 || panel == 7)) {
    engine->help->press_center(panel / 5);
  }
  if (engine != nullptr && engine->select && down != 0) engine->select->press(panel);
  if (engine != nullptr && engine->service) {
    engine->service->set_panel(panel, down != 0);
  }
  if (engine != nullptr && engine->game && !engine->results && !engine->select) {
    engine->game->queue_input({engine->clock.tick_at(time_ms), panel, down != 0});
  }
}

// Returns the layout if the saved text is valid, or the default one.
PIU_EXPORT const char* piu_keymap_normalize(const char* text) {
  engine->key_map_text = piu::KeyMap::parse(text).value_or(piu::KeyMap()).serialize();
  return engine->key_map_text.c_str();
}
static void make_screen_atlas() {
  if (engine->key_map_atlas.valid()) return;
  const piu::UiAtlas atlas = piu::make_ui_atlas();
  engine->key_map_atlas = {engine->renderer.begin_texture(), atlas.width, atlas.height};
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas.width, atlas.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlas.rgba.data());
}

PIU_EXPORT void piu_keymap_open(const char* text, int spanish) {
  make_screen_atlas();
  engine->key_map.emplace(piu::KeyMap::parse(text).value_or(piu::KeyMap()), spanish != 0, engine->key_map_atlas);
}
PIU_EXPORT void piu_keymap_key(const char* code) {
  if (engine->key_map) engine->key_map->press_key(code);
}
PIU_EXPORT void piu_keymap_click(double px, double py, int width, int height) {
  if (!engine->key_map) return;
  float x = 0.0f;
  float y = 0.0f;
  engine->renderer.to_screen(static_cast<float>(px), static_cast<float>(py), width, height, x, y);
  engine->key_map->click(x, y);
}
// 0 still open, 1 saved, 2 cancelled.
PIU_EXPORT int piu_keymap_finished() { return engine->key_map ? static_cast<int>(engine->key_map->result()) : 0; }
PIU_EXPORT const char* piu_keymap_result() {
  engine->key_map_text = engine->key_map ? engine->key_map->keys().serialize() : "";
  return engine->key_map_text.c_str();
}
PIU_EXPORT void piu_keymap_close() { engine->key_map.reset(); }

PIU_EXPORT const char* piu_calibration_track() {
  if (engine->calibration_track.empty()) engine->calibration_track = piu::make_click_track();
  return engine->calibration_track.data();
}
PIU_EXPORT int piu_calibration_track_size() {
  piu_calibration_track();
  return static_cast<int>(engine->calibration_track.size());
}
PIU_EXPORT void piu_calibration_open(int current_ms, int spanish) {
  make_screen_atlas();
  engine->calibration.emplace(current_ms, spanish != 0, engine->key_map_atlas);
  engine->calibration_clock_running = false;
}
PIU_EXPORT void piu_calibration_tap(double time_ms) {
  if (engine->calibration && engine->calibration_clock_running) {
    engine->calibration->tap(engine->calibration_clock.song_ms_at(time_ms));
  }
}
PIU_EXPORT void piu_calibration_cancel() {
  if (engine->calibration) engine->calibration->cancel();
}
PIU_EXPORT void piu_calibration_click(double px, double py, int width, int height) {
  if (!engine->calibration) return;
  float x = 0.0f;
  float y = 0.0f;
  engine->renderer.to_screen(static_cast<float>(px), static_cast<float>(py), width, height, x, y);
  engine->calibration->click(x, y);
}
PIU_EXPORT int piu_calibration_wants_audio() { return engine->calibration && engine->calibration->wants_audio() ? 1 : 0; }
PIU_EXPORT int piu_calibration_finished() {
  return engine->calibration ? static_cast<int>(engine->calibration->result()) : 0;
}
PIU_EXPORT int piu_calibration_result() { return engine->calibration ? engine->calibration->offset_ms() : 0; }
PIU_EXPORT void piu_calibration_close() {
  engine->calibration.reset();
  engine->calibration_track.clear();
  engine->calibration_track.shrink_to_fit();
}
PIU_EXPORT void piu_set_audio_offset(int ms) { engine->audio_offset_ms = ms; }

// 1 when the next frame may be drawn.
PIU_EXPORT int piu_frame_due(double now_ms) { return engine != nullptr && engine->frame_limiter.due(now_ms) ? 1 : 0; }

// audio_ms is the position of the song, or negative if no audio is playing.
PIU_EXPORT void piu_frame(double now_ms, double audio_ms, int width, int height) {
  if (engine == nullptr) {
    return;
  }
  engine->draw_list.clear();
  if (engine->calibration) {
    // The same clock as a song, so what it measures is what a song needs.
    if (audio_ms < 0.0) {
      engine->calibration_clock_running = false;
    } else if (engine->calibration_clock_running) {
      engine->calibration_clock.resync(now_ms, audio_ms);
    } else {
      engine->calibration_clock.start(now_ms, audio_ms);
      engine->calibration_clock_running = true;
    }
    engine->calibration->draw(engine->draw_list);
  } else if (engine->key_map) {
    engine->key_map->draw(engine->draw_list);
  } else if (engine->service) {
    engine->service->draw(engine->draw_list);
  } else if (engine->results) {
    const int due = engine->clock.advance(now_ms);
    for (int i = 0; i < due; ++i) engine->results->step();
    engine->results->draw(engine->draw_list);
    if (engine->results->closing() && engine->game) {
      engine->game->draw_overlay(engine->draw_list, engine->results->overlay_time(),
                                 engine->results->overlay_brightness());
    }
  } else if (engine->game) {
    const bool has_audio = audio_ms >= 0.0;
    if (has_audio) {
      engine->clock.resync(now_ms, audio_ms - engine->audio_offset_ms);
    }
    const int due = engine->clock.advance(
        now_ms, has_audio ? piu::FixedClock::kNoCatchUpLimit : piu::FixedClock::kMaxCatchUp);
    for (int i = 0; i < due; ++i) {
      engine->game->step();
    }
    engine->game->draw(engine->draw_list, engine->clock.alpha());
  } else if (engine->select) {
    const int due = engine->clock.advance(now_ms);
    for (int i = 0; i < due; ++i) engine->select->step();
    engine->select->draw(engine->draw_list);
  } else if (engine->help) {
    const int due = engine->clock.advance(now_ms);
    for (int i = 0; i < due; ++i) engine->help->step();
    engine->help->draw(engine->draw_list);
  } else if (engine->attract) {
    const int due = engine->clock.advance(now_ms);
    for (int i = 0; i < due; ++i) engine->attract->step();
    engine->attract->draw(engine->draw_list);
  }
  engine->renderer.render(engine->draw_list, width, height);
}

// Service menu. Kinds: 0 TEST (move), 1 SERVICE (select), 2 CLEAR.
PIU_EXPORT void piu_settings_load(int mode, int difficulty, int stage_break, int help, int demo, int coins) {
  piu::MachineSettings loaded{mode, difficulty, stage_break, help, demo, coins};
  if (loaded.valid()) engine->settings = loaded;
}
PIU_EXPORT int piu_settings_get(int index) {
  const piu::MachineSettings& s = engine->settings;
  const int values[6] = {s.game_mode, s.difficulty, s.stage_break, s.show_help, s.demo_sound, s.coins_per_credit};
  return index >= 0 && index < 6 ? values[index] : 0;
}
PIU_EXPORT void piu_service_request() { piu::request_service_assets(engine->assets); }
PIU_EXPORT void piu_service_open() {
  engine->game.reset();
  engine->results.reset();
  engine->service.emplace(engine->settings, engine->assets);
}
PIU_EXPORT void piu_service_button(int kind, int down) {
  if (!engine->service) return;
  if (kind == 0) {
    engine->service->set_test_held(down != 0);
    if (down != 0) engine->service->press_test();
  } else if (kind == 1) {
    if (down != 0) engine->service->press_service();
  } else if (kind == 2) {
    engine->service->set_clear_held(down != 0);
  }
}
// Track the SOUND TEST wants to hear: -1 nothing new, 0 stop, 1 to 14 play.
PIU_EXPORT int piu_service_track() {
  if (!engine->service) return -1;
  return engine->service->take_track_request().value_or(-1);
}
// Returns 1 once the operator left the menu, and keeps what was saved.
PIU_EXPORT int piu_service_finished() {
  if (!engine->service || !engine->service->exited()) return 0;
  engine->settings = engine->service->settings();
  engine->service.reset();
  return 1;
}

// Attract loop. Parses the two screens and queues what they need.
PIU_EXPORT int piu_attract_prepare(const char* andamiro, int andamiro_size, const char* start, int start_size) {
  engine->andamiro = piu::parse_bga(std::string_view(andamiro, static_cast<std::size_t>(andamiro_size)));
  engine->start_screen = piu::parse_bga(std::string_view(start, static_cast<std::size_t>(start_size)));
  if (!engine->andamiro || !engine->start_screen) return 0;
  piu::request_song_assets(*engine->andamiro, engine->assets);
  piu::request_song_assets(*engine->start_screen, engine->assets);
  engine->assets.request_texture("intro");
  engine->assets.request_texture("white");
  return 1;
}
PIU_EXPORT void piu_attract_begin(double now_ms) {
  engine->game.reset();
  engine->results.reset();
  engine->attract.emplace(*engine->andamiro, *engine->start_screen, engine->assets, engine->coins, engine->settings);
  engine->clock.start(now_ms, 0.0);
}
// 0 nothing, 1 start the intro music, 2 stop it, 3 start a demo, 4 players ready.
PIU_EXPORT int piu_attract_event() {
  if (!engine->attract) return 0;
  const std::optional<piu::AttractLoop::Event> event = engine->attract->take_event();
  return event ? static_cast<int>(*event) + 1 : 0;
}
PIU_EXPORT int piu_attract_demo_index() { return engine->attract ? engine->attract->demo_index() : 0; }
PIU_EXPORT int piu_attract_players() { return engine->attract ? engine->attract->players() : 0; }
PIU_EXPORT void piu_attract_demo_done(double now_ms) {
  engine->game.reset();
  if (engine->attract) engine->attract->demo_finished();
  engine->clock.start(now_ms, 0.0);
}

// The credit screens. Parses stage.cfg and the four selection BGAs.
PIU_EXPORT int piu_flow_prepare(const char* cfg, int cfg_size, const char* mode, int mode_size,
                                          const char* music, int music_size, const char* cds, int cds_size,
                                          const char* help, int help_size) {
  const auto view = [](const char* data, int size) { return std::string_view(data, static_cast<std::size_t>(size)); };
  engine->stage_config = piu::parse_stage_cfg(view(cfg, cfg_size));
  engine->mode_bga = piu::parse_bga(view(mode, mode_size));
  engine->music_bga = piu::parse_bga(view(music, music_size));
  engine->cd_bga = piu::parse_bga(view(cds, cds_size));
  engine->help_bga = piu::parse_bga(view(help, help_size));
  if (!engine->stage_config || !engine->mode_bga || !engine->music_bga || !engine->cd_bga || !engine->help_bga) return 0;
  piu::request_select_assets(*engine->mode_bga, *engine->music_bga, *engine->cd_bga, *engine->help_bga, engine->assets);
  return 1;
}
PIU_EXPORT void piu_flow_new_credit(int players) { engine->session = {}; engine->session.players = players; engine->session.stage = 0; }
PIU_EXPORT int piu_flow_stage() { return engine->session.stage; }
PIU_EXPORT int piu_flow_game_over() { return engine->game_over ? 1 : 0; }
PIU_EXPORT void piu_flow_to_attract(double now_ms) {
  engine->game.reset();
  engine->results.reset();
  engine->select.reset();
  engine->help.reset();
  if (engine->attract) engine->attract->restart();
  engine->clock.start(now_ms, 0.0);
}
PIU_EXPORT void piu_help_begin(double now_ms) {
  engine->game.reset();
  engine->results.reset();
  engine->help.emplace(*engine->help_bga, engine->assets, engine->coins, engine->session.players);
  engine->clock.start(now_ms, 0.0);
}
PIU_EXPORT int piu_help_finished() { return engine->help && engine->help->finished() ? 1 : 0; }
PIU_EXPORT void piu_select_begin(double now_ms) {
  engine->game.reset();
  engine->results.reset();
  engine->help.reset();
  ++engine->session.stage;
  engine->select.emplace(*engine->stage_config, *engine->mode_bga, *engine->music_bga, *engine->cd_bga, engine->assets,
                         engine->coins, engine->session, engine->settings.game_mode == 1);
  engine->clock.start(now_ms, 0.0);
}
// 0 nothing, 1 mode music, 2 select music, 3 preview, 4 stop music, 5 needs title, 6 done.
PIU_EXPORT int piu_select_event() {
  if (!engine->select) return 0;
  const std::optional<piu::SelectScreen::Event> event = engine->select->take_event();
  return event ? static_cast<int>(*event) + 1 : 0;
}
PIU_EXPORT int piu_select_track() { return engine->select ? engine->select->track() : 0; }
PIU_EXPORT void piu_select_request_title() { engine->assets.request_sprite(engine->select->song_track().title); }
PIU_EXPORT void piu_select_title_ready() { engine->select->title_ready(engine->assets); }
// Chosen song, as plain fields: 0 chart stem, 1 bga name.
PIU_EXPORT const char* piu_select_text(int which) {
  const piu::StageSong& song = engine->select->song();
  const piu::SelectSession& session = engine->session;
  if (which == 0) {
    engine->select_text = session.players == 3 && session.mode != piu::kModeDouble ? song.p2_step : song.p1_step;
  } else {
    engine->select_text = engine->select->song_track().bga;
  }
  return engine->select_text.c_str();
}
PIU_EXPORT int piu_select_cd() { return engine->select->song().cd_id; }
PIU_EXPORT int piu_select_level() { return std::max(0, engine->select->song().level - 1); }
PIU_EXPORT int piu_select_mode() { return engine->session.mode; }
PIU_EXPORT int piu_select_players() { return engine->session.players; }
PIU_EXPORT int piu_select_word(int player) { return static_cast<int>(engine->session.option_words[static_cast<std::size_t>(player)]); }
PIU_EXPORT int piu_select_cd_offset() { return engine->stage_config->cd_offset; }
PIU_EXPORT void piu_select_end() { engine->select.reset(); }

PIU_EXPORT void piu_set_wide(int wide) { engine->renderer.set_wide(wide != 0); }
// Next sound effect as sound | channel << 8 | loop << 16, or -1 when there are no more.
PIU_EXPORT int piu_next_sound() {
  const auto take = [](piu::SoundQueue& queue) { return queue.take(); };
  std::optional<piu::SoundCall> call = take(engine->coin_sounds);
  if (!call && engine->attract) call = take(engine->attract->sounds());
  if (!call && engine->select) call = take(engine->select->sounds());
  if (!call && engine->help) call = take(engine->help->sounds());
  if (!call && engine->game) call = take(engine->game->sounds());
  if (!call && engine->results) call = take(engine->results->sounds());
  if (!call && engine->service) call = take(engine->service->sounds());
  if (!call) return -1;
  return call->sound | call->channel << 8 | (call->loop ? 1 << 16 : 0);
}
PIU_EXPORT int piu_chart_margin(const char* name) { return piu::chart_margin(name); }
PIU_EXPORT int piu_take_audio_stop() { return engine->game && engine->game->take_audio_stop() ? 1 : 0; }
PIU_EXPORT int piu_finished() { return engine->game && engine->game->finished() ? 1 : 0; }
PIU_EXPORT int piu_uses_player(int player) {
  return engine->game && engine->game->uses_player(player) ? 1 : 0;
}
PIU_EXPORT int piu_points(int player) { return engine->game ? engine->game->stats(player).points : 0; }
PIU_EXPORT int piu_combo(int player) { return engine->game ? engine->game->stats(player).combo : 0; }
PIU_EXPORT int piu_max_combo(int player) { return engine->game ? engine->game->stats(player).max_combo : 0; }
PIU_EXPORT int piu_tally(int player, int index) {
  return engine->game ? engine->game->stats(player).tally[static_cast<std::size_t>(index)] : 0;
}
PIU_EXPORT int piu_last_judgment(int player) {
  return engine->game ? static_cast<int>(engine->game->stats(player).last) : 0;
}
PIU_EXPORT int piu_grade(int player) { return engine->game ? engine->game->grade(player) : 0; }

}  // extern "C"
