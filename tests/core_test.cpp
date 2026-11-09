// Tests of the game rules, screens and file formats. Needs no game data.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "core/key_map_screen.h"
#include "core/ui_atlas.h"
#include "core/calibration_screen.h"
#include "core/frame_limiter.h"
#include "core/assets.h"
#include "core/clock.h"
#include "core/game.h"
#include "core/json.h"
#include "core/attract.h"
#include "core/results.h"
#include "core/select_screen.h"
#include "core/service_menu.h"
#include "core/stage_cfg.h"
#include "core/scene.h"
#include "core/song_data.h"
#include "core/sound.h"
#include "extractor/formats.h"

namespace {

int failures = 0;

void check(bool condition, const char* name) {
  if (!condition) {
    std::printf("FALLA: %s\n", name);
    ++failures;
  }
}

bool near(double a, double b, double tolerance) { return std::fabs(a - b) <= tolerance; }

piu::GameOptions options_none() { return {}; }
piu::GameOptions double_options() {
  piu::GameOptions options;
  options.double_play = true;
  return options;
}

piu::Assets arrow_assets() {
  piu::Assets assets;
  for (const char* name : {"arrow01", "arrow02", "arrow03", "arrow04", "arrow05", "arrow06", "dec2", "dec4", "level"}) {
    assets.set_texture(name, {1, 256, 256});
  }
  std::string hud = "TYPE TILE\n";
  for (int i = 0; i < 72; ++i) hud += "T arrow01.tga 100 100 50 50 0 0 50 50\n";
  assets.set_sprite("gamescr", hud);
  return assets;
}

// With row_seconds = 0.25 at 1x, y = (time - now) * 120 + 35.
piu::Chart chart_with_rows(std::initializer_list<std::pair<double, int>> rows) {
  piu::Chart chart;
  chart.first_row_s = 0.0;
  chart.row_seconds = 0.25;
  for (const auto& [time, mask] : rows) {
    piu::ChartRow row;
    row.index = static_cast<int>(std::lround(time / chart.row_seconds));
    row.time = time;
    row.mask = {static_cast<std::uint16_t>(mask), 0};
    chart.rows.push_back(row);
  }
  return chart;
}

// Logic frames run every 4 ticks, so at 1x a row only reaches odd y values.
std::int64_t tick_for_y(double time, int y) {
  return std::llround((time - (y - 35) / 120.0) * 60.0) * 4;
}

piu::Game make_game(piu::Chart chart, int margin = 0) {
  piu::GameOptions options;
  options.margin = margin;
  return piu::Game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options);
}

void run_until(piu::Game& game, std::int64_t& tick, std::int64_t target) {
  for (; tick < target; ++tick) game.step();
}

void tap(piu::Game& game, std::int64_t& tick, int panel, std::int64_t at) {
  game.queue_input({at, panel, true});
  game.queue_input({at, panel, false});
  run_until(game, tick, at + 1);
}

void test_clock_has_no_drift() {
  piu::FixedClock clock;
  clock.start(1000.0, 0.0);
  std::int64_t total = 0;
  double now = 1000.0;
  for (int i = 0; i < 360000; ++i) {
    now += 1000.0 / 144.0;
    total += clock.advance(now);
  }
  check(total == 600000, "2500 s a 144 FPS deben dar 600000 pasos exactos");
  check(clock.alpha() >= 0.0 && clock.alpha() <= 1.0, "alpha dentro de rango");
}

void test_clock_skips_suspended_time() {
  piu::FixedClock clock;
  clock.start(0.0, 0.0);
  clock.advance(0.0);
  check(clock.advance(60000.0) == piu::FixedClock::kMaxCatchUp, "una pausa larga no se reproduce");
  check(clock.advance(60000.0 + 100.0) == 24, "después sigue a velocidad normal");
}

void test_clock_follows_audio() {
  piu::FixedClock clock;
  clock.start(0.0, 0.0);
  clock.resync(1000.0, 1500.0);
  check(clock.advance(1000.0, piu::FixedClock::kNoCatchUpLimit) == 360, "salta si el audio se adelanta mucho");
  for (int i = 1; i <= 600; ++i) {
    const double now = 1000.0 + i * 10.0;
    clock.resync(now, now + 500.0 + 20.0);
    clock.advance(now, piu::FixedClock::kNoCatchUpLimit);
  }
  check(std::llabs(clock.ticks() - std::llround(7520 * 0.24)) <= 1, "corrige poco a poco un desfase pequeño");
}

void test_json() {
  const auto value = piu::Json::parse(R"({"a":[1,-2.5e1,true,null],"b":"x\"é😀"})");
  check(value.has_value(), "JSON válido");
  check(near(value->find("a")->items()[1].number(), -25.0, 1e-9), "número exponencial");
  check(value->find("b")->string() == "x\"\xC3\xA9\xF0\x9F\x98\x80", "escapes y UTF-8");
  check(!piu::Json::parse("{\"a\":}").has_value(), "JSON roto se rechaza");
  check(!piu::Json::parse("[1,2").has_value(), "arreglo sin cerrar se rechaza");
  check(!piu::Json::parse(std::string(200, '[')).has_value(), "profundidad limitada");
}

void test_chart_parse() {
  const auto chart = piu::parse_chart(
      R"({"first_row_time_s":1.5,"row_seconds":0.25,"steps":[
      {"row":0,"time_s":1.5,"pad_left":"10001","pad_right":"00000"},
      {"row":1,"time_s":1.75,"pad_left":"00000","pad_right":"00000"},
      {"row":2,"time_s":2.0,"pad_left":"00000","pad_right":"01000"}]})");
  check(chart.has_value(), "chart válido");
  check(chart->rows.size() == 2, "las filas vacías no se guardan");
  check(chart->rows[0].mask[0] == 0b10001, "paneles izquierdos");
  check(chart->rows[1].mask[1] == 0b00010, "paneles derechos");
  check(!piu::parse_chart(R"({"steps":[]})").has_value(), "chart sin tiempo se rechaza");
}

void test_bga_pose() {
  std::vector<piu::BgaKey> keys(3);
  keys[0] = {10, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true};
  keys[1] = {20, 10, 0, 0, 0, 1, 0, 1, 1, 1, 1, false};
  keys[2] = {30, 20, 0, 0, 0, 1, 0, 1, 1, 1, 1, true};
  check(!piu::bga_pose(keys, 9.9f).has_value(), "antes de la primera clave no se dibuja");
  check(piu::bga_pose(keys, 10.0f).has_value(), "se dibuja en la primera clave");
  const auto middle = piu::bga_pose(keys, 15.0f);
  check(middle && near(middle->fraction, 0.5, 1e-6) && middle->from == &keys[0], "interpola");
  check(!piu::bga_pose(keys, 25.0f).has_value(), "visible = 0 oculta el tramo siguiente");
  check(!piu::bga_pose(keys, 30.5f).has_value(), "después de la última clave no se dibuja");
}

void test_sprite_and_asset_names() {
  const piu::Sprite sprite = piu::parse_sprite("TYPE PATTERN 2 3 4\r\nT Foo.tga 1 2 3 4 5 6 7 8\r\nT x\n");
  check(sprite.type == piu::SpriteType::kPattern, "tipo");
  check(sprite.pattern_step == 2 && sprite.columns == 3 && sprite.rows == 4, "parámetros");
  check(sprite.quads.size() == 1 && sprite.quads[0].texture == "Foo.tga" && sprite.quads[0].v1 == 8.0f, "cuadro");
  check(piu::asset_key(" DIR\\Sub/Arrow01.TGA ") == "arrow01", "nombre normalizado");
  check(piu::has_extension("a.SPR ", "spr") && !piu::has_extension("spr", "spr"), "extensión");

  piu::Assets assets;
  assets.request_texture("a.png");
  assets.request_texture("A.tga");
  check(*assets.next_request() == "tex:a", "primera petición");
  check(!assets.next_request().has_value(), "no se repiten peticiones");
  assets.mark_texture_missing("a");
  check(assets.texture("a") == nullptr, "textura faltante");
}

void test_each_judgment_band() {
  struct Case {
    int y;
    piu::Judgment expected;
    int points;
  };
  const Case cases[] = {{33, piu::Judgment::kPerfect, 1000},
                        {27, piu::Judgment::kGreat, 500},
                        {25, piu::Judgment::kGood, 100},
                        {21, piu::Judgment::kPressMiss, 0}};
  for (const Case& c : cases) {
    piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
    std::int64_t tick = 0;
    const std::int64_t at = tick_for_y(1.0, c.y);
    tap(game, tick, 0, at);
    check(game.stats(0).last == c.expected, "banda de calificación");
    check(game.stats(0).points == c.points, "puntos de la banda");
  }
}

void test_margin_changes_bands() {
  piu::Game wide = make_game(chart_with_rows({{1.0, 1}}), 0);
  piu::Game tight = make_game(chart_with_rows({{1.0, 1}}), 5);
  std::int64_t a = 0, b = 0;
  const std::int64_t at = tick_for_y(1.0, 37);
  tap(wide, a, 0, at);
  tap(tight, b, 0, at);
  check(wide.stats(0).last == piu::Judgment::kPerfect, "y = 37 es Perfect con margen 0");
  check(tight.stats(0).last == piu::Judgment::kGood, "y = 37 es Good con margen 5");
}

void test_late_miss() {
  piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
  std::int64_t tick = 0;
  run_until(game, tick, tick_for_y(1.0, 8));
  check(game.stats(0).last == piu::Judgment::kLateMiss, "pasar y = 10 sin tocar es fallo");
  check(game.stats(0).points == 0, "los puntos no bajan de cero");
}

void test_row_needs_all_panels() {
  piu::Game game = make_game(chart_with_rows({{1.0, 0b101}}));
  std::int64_t tick = 0;
  const std::int64_t at = tick_for_y(1.0, 37);
  game.queue_input({at, 0, true});
  run_until(game, tick, at + 1);
  check(game.stats(0).last == piu::Judgment::kNone, "un solo panel no califica");
  game.queue_input({at + 4, 2, true});
  run_until(game, tick, at + 5);
  check(game.stats(0).last == piu::Judgment::kPerfect, "con todos los paneles sí");
}

void test_short_tap_is_not_lost() {
  piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
  std::int64_t tick = 0;
  tap(game, tick, 0, tick_for_y(1.0, 33));
  check(game.stats(0).last == piu::Judgment::kPerfect, "un toque dentro de un paso cuenta");
}

void test_press_expires_after_a_few_frames() {
  // The press mask clears every fifth frame, so holding a panel does not hit later rows.
  piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
  std::int64_t tick = 0;
  const std::int64_t at = tick_for_y(1.0, 41) - 40;
  game.queue_input({at, 0, true});
  run_until(game, tick, tick_for_y(1.0, 8));
  check(game.stats(0).last == piu::Judgment::kLateMiss, "una pisada anticipada ya no cuenta");
}

void test_press_counts_within_window() {
  piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
  std::int64_t tick = 0;
  // Press at the start of a mask period, so it stays active for the next frames.
  game.queue_input({8, 0, true});
  run_until(game, tick, 12);
  check(game.stats(0).last == piu::Judgment::kNone, "todavía no llega la fila");
  check(game.stats(0).combo == 0, "sin combo antes de la fila");
}

void test_combo_bonus_and_miss_penalty() {
  piu::Game game = make_game(chart_with_rows({{1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1}, {9, 1}}));
  std::int64_t tick = 0;
  for (double time : {1.0, 2.0, 3.0, 4.0}) tap(game, tick, 0, tick_for_y(time, 33));
  check(game.stats(0).combo == 4 && game.stats(0).points == 5000, "combo mayor que 3 paga 1000 extra");
  check(game.stats(0).tally[0] == 4 && game.stats(0).max_combo == 4, "cuenta Perfect y combo máximo");
  run_until(game, tick, tick_for_y(5.0, 8));
  check(game.stats(0).combo == 0 && game.stats(0).points == 4000, "fallo tardío");
  run_until(game, tick, tick_for_y(8.0, 8));
  check(game.stats(0).points == 0, "cuatro fallos seguidos restan 1500 y no baja de cero");
  check(game.stats(0).tally[4] == 4 && game.stats(0).max_combo == 4, "cuenta Miss y conserva el combo máximo");
}

void test_missing_assets_are_reported() {
  piu::Bga bga;
  piu::BgaLayer layer;
  layer.name = "nada.spr";
  layer.keys.push_back({0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true});
  bga.layers.push_back(layer);
  piu::Game with_arrows(chart_with_rows({{1.0, 1}}), bga, piu::Bga{}, arrow_assets(), options_none());
  check(with_arrows.has_arrows() && with_arrows.missing().size() == 1, "reporta el recurso faltante");

  piu::Assets empty;
  piu::Game without(chart_with_rows({{1.0, 1}}), piu::Bga{}, piu::Bga{}, empty, options_none());
  check(!without.has_arrows(), "detecta que faltan flechas");
}

void test_draw_matches_expected_geometry() {
  piu::Assets assets = arrow_assets();
  assets.set_texture("bg", {9, 640, 480});
  piu::Bga bga;
  piu::BgaLayer layer;
  layer.name = "bg.png";
  // Rotates 90 degrees around the pivot (10, 20) at position (100, 200).
  layer.keys.push_back({0, 100, 200, 10, 20, 2, 90, 1, 1, 1, 0.5f, true});
  layer.keys.push_back({60, 100, 200, 10, 20, 2, 90, 1, 1, 1, 0.5f, true});
  bga.layers.push_back(layer);

  piu::Game game(chart_with_rows({{1.0, 1}}), bga, piu::Bga{}, assets, options_none());
  piu::DrawList list;
  game.draw(list, 0.0);
  check(!list.batches().empty() && list.batches()[0].texture == 9, "el BGA se dibuja primero");
  const piu::Vertex& origin = list.vertices()[0];
  // Pivot maps to (110, 220), (0,0) rotates around it.
  check(near(origin.x, 110 - 2 * 20, 1e-3) && near(origin.y, 220 + 2 * 10, 1e-3), "matriz de capa");
  check(near(origin.color.a, 0.5, 1e-6), "alfa de capa");
}

piu::Chart double_chart() {
  piu::Chart chart;
  chart.row_seconds = 0.25;
  piu::ChartRow row;
  row.time = 1.0;
  row.mask = {0b00001, 0b00001};
  chart.rows.push_back(row);
  return chart;
}

void test_double_is_one_player_with_ten_columns() {
  piu::Game game(double_chart(), piu::Bga{}, piu::Bga{}, arrow_assets(), double_options());
  std::int64_t tick = 0;
  const std::int64_t at = tick_for_y(1.0, 37);
  game.queue_input({at, 0, true});
  run_until(game, tick, at + 1);
  check(game.stats(0).last == piu::Judgment::kNone, "Double exige ambos tapetes");
  game.queue_input({at + 4, 5, true});
  run_until(game, tick, at + 5);
  check(game.stats(0).last == piu::Judgment::kPerfect && game.stats(0).tally[0] == 1, "una sola calificación por fila");
  check(game.uses_player(0) && !game.uses_player(1), "un solo jugador");

  piu::Game fresh(double_chart(), piu::Bga{}, piu::Bga{}, arrow_assets(), double_options());
  piu::DrawList list;
  fresh.draw(list, 0.0);
  float low = 1000.0f;
  float high = -1000.0f;
  for (std::size_t i = 0; i < 60; ++i) {
    low = std::min(low, list.vertices()[i].x);
    high = std::max(high, list.vertices()[i].x);
  }
  check(near(low, 80.0, 1e-3) && near(high, 512.0 + 69.0, 1e-3), "campo centrado");
}

// The life bar alternates with each frame drawn, so comparisons keep the same phase.
void skip_drawn_frame(piu::Game& game) {
  piu::DrawList unused;
  game.draw(unused, 0.0);
}

void test_receptor_effects_and_judgment_text() {
  piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
  std::int64_t tick = 0;
  run_until(game, tick, 4);
  piu::DrawList idle;
  game.draw(idle, 0.0);

  // A press adds the wave effect on the next frame.
  tap(game, tick, 3, 8);
  run_until(game, tick, 12);
  skip_drawn_frame(game);
  piu::DrawList pressed;
  game.draw(pressed, 0.0);
  check(pressed.vertices().size() == idle.vertices().size() + 6, "la pisada agrega la onda");

  // The effect ends after 16 frames.
  run_until(game, tick, 8 + 17 * 4);
  skip_drawn_frame(game);
  piu::DrawList ended;
  game.draw(ended, 0.0);
  check(ended.vertices().size() == idle.vertices().size(), "la onda termina");

  // A Perfect hides the arrow, shows the flash and the text.
  const std::int64_t at = tick_for_y(1.0, 33);
  tap(game, tick, 0, at);
  run_until(game, tick, at + 8);
  skip_drawn_frame(game);
  piu::DrawList hit;
  game.draw(hit, 0.0);
  check(game.stats(0).last == piu::Judgment::kPerfect, "califica");
  check(hit.vertices().size() == ended.vertices().size() - 6 + 6 + 6, "destello y texto, sin la flecha");
}

void test_players_keep_separate_stats() {
  piu::Chart chart = chart_with_rows({{1.0, 1}});
  chart.rows[0].mask[1] = 1;
  piu::Game game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options_none());
  std::int64_t tick = 0;
  tap(game, tick, 5, tick_for_y(1.0, 33));
  run_until(game, tick, tick_for_y(1.0, 8));
  check(game.stats(1).tally[0] == 1 && game.stats(1).points == 1000, "el jugador 2 acierta");
  check(game.stats(0).tally[4] == 1 && game.stats(0).points == 0, "el jugador 1 falla por separado");
}

void test_grades() {
  struct Case {
    int perfect, great, bad;
    char expected;
  };
  const Case cases[] = {{20, 0, 0, 'S'}, {18, 0, 2, 'A'}, {16, 0, 4, 'B'}, {12, 2, 6, 'C'}, {12, 0, 8, 'D'}};
  for (const Case& c : cases) {
    piu::Chart chart;
    chart.row_seconds = 0.25;
    for (int i = 0; i < 20; ++i) {
      piu::ChartRow row;
      row.index = i;
      row.time = 1.0 + i;
      row.mask = {1, 0};
      chart.rows.push_back(row);
    }
    piu::Game game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options_none());
    std::int64_t tick = 0;
    for (int i = 0; i < 20; ++i) {
      const int y = i < c.perfect ? 33 : i < c.perfect + c.great ? 27 : 21;
      tap(game, tick, 0, tick_for_y(1.0 + i, y));
    }
    check(game.grade(0) == c.expected, "letra de calificación");
  }
}

void test_finishes_after_ending() {
  // Row 4 is the last; the song settles five rows later and the ending lasts 240 frames.
  piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
  std::int64_t tick = 0;
  run_until(game, tick, 240 * 2);
  check(!game.finished(), "no termina antes de tiempo");
  check(game.stats(0).state == piu::LifeState::kPlaying, "sigue jugando");
  run_until(game, tick, 240 * 3);
  check(game.stats(0).state == piu::LifeState::kBelowLine || game.stats(0).state == piu::LifeState::kCleared, "decide al terminar");
  run_until(game, tick, 240 * 11);
  check(game.finished(), "la animación final termina");
}

void test_life_changes_and_clamps() {
  piu::Game game = make_game(chart_with_rows({{1.0, 1}, {2.0, 1}}));
  std::int64_t tick = 0;
  check(near(game.stats(0).life, 15.0, 1e-6), "empieza en 15");
  tap(game, tick, 0, tick_for_y(1.0, 33));
  run_until(game, tick, tick_for_y(1.0, 33) + 4);
  check(near(game.stats(0).life, 15.5, 1e-5), "Perfect suma 0.5 en el nivel 0");
  run_until(game, tick, tick_for_y(2.0, 8) + 4);
  check(near(game.stats(0).life, 13.2, 1e-5), "Miss resta 2.3 en el nivel 0");
  check(game.stats(0).late_misses == 1, "cuenta el Miss");
}

void test_failure_after_long_danger() {
  piu::Chart chart;
  chart.row_seconds = 0.25;
  for (int i = 0; i < 800; ++i) {
    piu::ChartRow row;
    row.index = i;
    row.time = 1.0 + i * 0.25;
    row.mask = {1, 0};
    chart.rows.push_back(row);
  }
  piu::GameOptions options;
  options.stage = 2;
  piu::Game game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options);
  std::int64_t tick = 0;
  run_until(game, tick, 240 * 130);
  check(game.stats(0).danger || game.stats(0).state == piu::LifeState::kFailed, "entra a peligro");
  check(game.stats(0).life <= -5.0f + 1e-4f, "no baja del mínimo");
  run_until(game, tick, 240 * 195);
  check(game.stats(0).state == piu::LifeState::kFailed, "falla tras 600 cuadros en peligro");
  check(game.finished(), "termina la animación de fallo");
}

piu::Chart long_chart() {
  piu::Chart chart;
  chart.row_seconds = 0.25;
  for (int i = 0; i < 800; ++i) {
    piu::ChartRow row;
    row.index = i;
    row.time = 1.0 + i * 0.25;
    row.mask = {1, 0};
    chart.rows.push_back(row);
  }
  return chart;
}

void test_failure_hides_the_song_bga() {
  piu::Assets assets = arrow_assets();
  assets.set_texture("bg", {9, 640, 480});
  piu::Bga bga;
  piu::BgaLayer layer;
  layer.name = "bg.png";
  layer.keys.push_back({0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true});
  layer.keys.push_back({1000000, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true});
  bga.layers.push_back(layer);
  piu::GameOptions options;
  options.stage = 2;
  piu::Game game(long_chart(), bga, piu::Bga{}, assets, options);
  const auto draws_bga = [&] {
    piu::DrawList list;
    game.draw(list, 0.0);
    return std::any_of(list.batches().begin(), list.batches().end(), [](const auto& b) { return b.texture == 9; });
  };
  std::int64_t tick = 0;
  run_until(game, tick, 240 * 5);
  check(draws_bga(), "el BGA se ve mientras se juega");
  while (game.stats(0).state != piu::LifeState::kFailed && tick < 240 * 195) run_until(game, tick, tick + 24);
  check(game.stats(0).state == piu::LifeState::kFailed, "llega a fallar");
  run_until(game, tick, tick + 240 * 2);
  check(!draws_bga(), "el BGA de la cancion se oculta en la animacion de fallo");
}

void test_scene_flight() {
  const auto scene = piu::parse_scene(R"({"models":[{"positions":[0,0,0, 1,0,0, 0,1,0],"uvs":[0,0, 1,0, 0,1],
      "parts":[{"material":"WAV02","faces":[[0,1,2,0,1,2]]}]}],"materials":{"wav02":"arrow04"},
      "camera":[[0,0,0,0,1,0],[1,0,0,1,1,0]]})");
  check(scene.has_value(), "escena válida");
  check(scene->models[0].parts[0].vertices.size() == 15 && scene->models[0].parts[0].texture == "arrow04", "convierte caras");
  check(piu::scene_base_model(0) == 0 && piu::scene_base_model(25) == 1 && piu::scene_base_model(498) == 17, "rangos");

  const auto matrix = piu::scene_matrix(scene->camera[0]);
  const double t = 1.0 / std::tan(40.0 * 3.14159265358979 / 180.0);
  check(near(matrix[0], t * 0.75, 1e-4) && near(matrix[9], t, 1e-4) && near(matrix[7], 1.0, 1e-5), "proyección con Z hacia arriba");
  check(near(matrix[6], 17.0 / 15.0, 1e-4), "dibuja hasta 16 unidades");

  piu::GameOptions options;
  options.flight = &*scene;
  piu::Game game(chart_with_rows({{1.0, 1}}), piu::Bga{}, piu::Bga{}, arrow_assets(), options);
  std::int64_t tick = 0;
  run_until(game, tick, 16);
  piu::DrawList list;
  game.draw(list, 0.0);
  check(list.scene() == &*scene, "usa el vuelo como fondo");

  piu::GameOptions remix;
  remix.flight = &*scene;
  remix.stage = 2;
  piu::Game nonstop(long_chart(), piu::Bga{}, piu::Bga{}, arrow_assets(), remix);
  std::int64_t nonstop_tick = 0;
  run_until(nonstop, nonstop_tick, 240 * 130);
  check(!nonstop.stats(0).danger && nonstop.stats(0).state != piu::LifeState::kFailed, "Non-Stop no entra a peligro");
}

void test_stage_one_cannot_fail() {
  piu::Chart chart;
  chart.row_seconds = 0.25;
  for (int i = 0; i < 800; ++i) {
    piu::ChartRow row;
    row.index = i;
    row.time = 1.0 + i * 0.25;
    row.mask = {1, 0};
    chart.rows.push_back(row);
  }
  piu::Game game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options_none());
  std::int64_t tick = 0;
  run_until(game, tick, 240 * 195);
  check(game.stats(0).state == piu::LifeState::kPlaying && !game.stats(0).danger, "en la etapa 1 no se falla");
}

void test_frame_limiter_holds_60_fps() {
  const auto frames_in_a_second = [](double hz) {
    piu::FrameLimiter limiter;
    int drawn = 0;
    const int refreshes = static_cast<int>(hz) * 10;
    for (int i = 0; i < refreshes; ++i) drawn += limiter.due(i * 1000.0 / hz) ? 1 : 0;
    return drawn / 10.0;
  };
  check(near(frames_in_a_second(60.0), 60.0, 0.2), "a 60 Hz dibuja todos los cuadros");
  check(near(frames_in_a_second(120.0), 60.0, 0.2), "a 120 Hz se queda en 60");
  check(near(frames_in_a_second(144.0), 60.0, 0.2), "a 144 Hz se queda en 60");
  check(near(frames_in_a_second(240.0), 60.0, 0.2), "a 240 Hz se queda en 60");
}

void test_display_swap_interval() {
  check(piu::display_swap_interval(60) == 1 && piu::display_swap_interval(59) == 1, "a 60 Hz cada cuadro dura un refresco");
  check(piu::display_swap_interval(120) == 2 && piu::display_swap_interval(119) == 2, "a 120 Hz cada cuadro dura dos");
  check(piu::display_swap_interval(180) == 3 && piu::display_swap_interval(240) == 4, "los multiplos de 60 quedan parejos");
  check(piu::display_swap_interval(144) == 1 && piu::display_swap_interval(165) == 1, "otras frecuencias no se pueden repartir");
  check(piu::display_swap_interval(75) == 1 && piu::display_swap_interval(100) == 1, "ni las intermedias");
  check(piu::display_swap_interval(0) == 1, "sin dato espera un refresco");
}

void test_calibration() {
  std::vector<double> taps(16, 37.0);
  piu::TapMeasure steady = piu::measure_taps(taps);
  check(steady.offset_ms == 37 && steady.steady, "toques iguales dan su promedio");

  // Around the seam of the beat: 595 and 5 are 10 ms apart, and their center is 0.
  taps.clear();
  for (int i = 0; i < 8; ++i) {
    taps.push_back(595.0);
    taps.push_back(5.0);
  }
  steady = piu::measure_taps(taps);
  check(std::abs(steady.offset_ms) <= 1 && steady.steady, "promedia a traves del final del beat");

  taps.assign(16, 10.0);
  taps[0] = 280.0;
  taps[1] = 250.0;
  steady = piu::measure_taps(taps);
  check(near(steady.offset_ms, 10, 1) && steady.steady, "descarta los toques sueltos");

  taps.clear();
  for (int i = 0; i < 16; ++i) taps.push_back(i * 12.0);
  check(!piu::measure_taps(taps).steady, "toques irregulares no se aceptan");

  taps.assign(16, 590.0);
  check(piu::measure_taps(taps).offset_ms == -10, "tocar antes del beat da negativo");
  taps.assign(16, 299.0);
  check(piu::measure_taps(taps).offset_ms == 299, "mide retrasos grandes");

  const std::string wav = piu::make_click_track();
  check(wav.compare(0, 4, "RIFF") == 0 && wav.size() == 44 + 40 * 26460 * 2, "pista del beat");
  const auto sample = [&](std::size_t frame) {
    return static_cast<std::int16_t>(static_cast<unsigned char>(wav[44 + frame * 2]) |
                                     static_cast<unsigned char>(wav[45 + frame * 2]) << 8);
  };
  check(std::abs(sample(6)) > 3000 && std::abs(sample(26460 + 6)) > 3000, "un golpe en cada beat");
  check(sample(13000) == 0, "silencio entre golpes");

  const piu::Texture none;
  piu::CalibrationScreen screen(25, false, none);
  check(screen.wants_audio(), "el beat suena al abrir");
  screen.tap(-5.0);
  for (int i = 0; i < 4; ++i) screen.tap(i * piu::kCalibrationBeatMs + 200.0);
  for (int i = 4; i < 19; ++i) screen.tap(i * piu::kCalibrationBeatMs + 40.0);
  check(screen.wants_audio(), "falta un toque");
  screen.click(500.0f, 440.0f);
  check(screen.result() == piu::CalibrationScreen::Result::kRunning, "no se guarda antes de terminar");
  screen.tap(19 * piu::kCalibrationBeatMs + 40.0);
  check(!screen.wants_audio(), "el beat para al terminar");
  check(screen.offset_ms() == 25, "aun no cambia lo guardado");
  screen.click(500.0f, 440.0f);
  check(screen.result() == piu::CalibrationScreen::Result::kSaved && screen.offset_ms() == 40, "guarda sin el calentamiento");

  piu::CalibrationScreen again(25, false, none);
  for (int i = 0; i < 20; ++i) again.tap(i * piu::kCalibrationBeatMs + i * 12.0);
  again.click(500.0f, 440.0f);
  check(again.result() == piu::CalibrationScreen::Result::kRunning, "irregular no se puede guardar");
  again.click(320.0f, 440.0f);
  check(again.wants_audio(), "repetir vuelve a sonar");
  again.click(100.0f, 440.0f);
  check(again.result() == piu::CalibrationScreen::Result::kSaved && again.offset_ms() == 0, "poner en cero");

  piu::CalibrationScreen left(25, false, none);
  left.cancel();
  check(left.result() == piu::CalibrationScreen::Result::kCancelled && left.offset_ms() == 25, "cancelar conserva");

  piu::FixedClock clock;
  clock.start(1000.0, 200.0);
  check(near(clock.song_ms_at(1100.0), 300.0, 1e-9), "posicion de la cancion");
}

void test_coins() {
  piu::CoinBox box;
  check(box.coins_per_credit() == 5 && box.text() == "0", "por defecto 5 monedas");
  box.add_credits(2);
  check(box.coins() == 10 && box.credits() == 2, "los creditos gratis valen monedas completas");
  box.reset();
  check(box.coins() == 0, "apagar la maquina borra los creditos");
  for (int i = 0; i < 7; ++i) box.insert();
  check(box.credits() == 1 && box.text() == "1(2/5)", "texto con monedas pendientes");
  check(box.can_start(1) && !box.can_start(2), "alcanza para un jugador");
  check(box.consume() && box.coins() == 2, "descuenta un crédito");
  check(!box.consume(), "sin crédito no descuenta");
  box.configure(1, false);
  check(box.text() == "2", "con una moneda por crédito no hay pendientes");
  box.configure(0, true);
  check(box.coins_per_credit() == 1 && box.can_start(5) && box.consume() && box.coins() == 2, "juego libre");
}

void test_results_screen() {
  piu::Assets assets;
  assets.set_texture("dec2", {1, 256, 256});
  assets.set_texture("arrow01", {2, 256, 256});
  std::string sprites = "TYPE TILE\n";
  for (int i = 0; i < 0xdd; ++i) sprites += "T arrow01.tga 10 10 20 20 0 0 20 20\n";
  assets.set_sprite("gamescr", sprites);

  std::array<piu::ResultPlayer, 2> players{};
  players[0].active = true;
  players[0].counts = {3, 2, 0, 0, 0, 3};
  players[0].points = 12345;
  players[0].grade = 'A';

  piu::ResultsScreen count(players, assets);
  for (int i = 0; i < 4; ++i) count.step();
  piu::DrawList first;
  count.draw(first);
  std::size_t base_quads = first.vertices().size() / 6;
  check(base_quads == 8 + 2 + 1 + 7 + 1 + 3, "fondo, score y primera fila");
  int frames = 0;
  while (!count.finished() && frames < 2000) {
    for (int i = 0; i < 4; ++i) count.step();
    ++frames;
  }
  check(count.finished(), "termina");
  check(frames > 180 + 60 + 6 * 21 - 10 && frames < 420, "conteo, 1 s de pausa y 3 s con la letra");

  piu::ResultsScreen skipped(players, assets);
  for (int i = 0; i < 4; ++i) skipped.step();
  skipped.press_center(1);
  for (int i = 0; i < 8; ++i) skipped.step();
  piu::DrawList not_skipped;
  skipped.draw(not_skipped);
  check(not_skipped.vertices().size() == first.vertices().size(), "el centro de un jugador ausente no salta");
  skipped.press_center(0);
  for (int i = 0; i < 4; ++i) skipped.step();
  piu::DrawList hold;
  skipped.draw(hold);
  check(hold.vertices().size() / 6 == 8 + 2 + 1 + 7 + 6 + 1 + 3 * 6, "salta a ver todas las filas");
  for (int i = 0; i < 4 * (60 + 2); ++i) skipped.step();
  piu::DrawList final_screen;
  skipped.draw(final_screen);
  check(final_screen.vertices().size() / 6 == 8 + 2 + 1 + 7 + 6 + 1 + 3 * 6 + 0xa, "la letra aparece al final");
}

void test_ending_clip_time() {
  // Same chart, cleared and failed: each ending uses its own part of gameview.bga.
  piu::Chart chart;
  chart.row_seconds = 0.25;
  for (int i = 0; i < 8; ++i) {
    piu::ChartRow row;
    row.index = i;
    row.time = 1.0 + i * 0.25;
    row.mask = {1, 0};
    chart.rows.push_back(row);
  }
  piu::Chart missed = chart;
  piu::Game cleared = make_game(std::move(chart));
  std::int64_t tick = 0;
  for (int i = 0; i < 8; ++i) tap(cleared, tick, 0, tick_for_y(1.0 + i * 0.25, 37));
  run_until(cleared, tick, 240 * 12);
  check(cleared.stats(0).state == piu::LifeState::kCleared && cleared.finished(), "pasa con vida");
  check(cleared.ending_time() == 0x78, "al pasar la animación se detiene en 0x78");

  piu::Game below = make_game(std::move(missed));
  tick = 0;
  run_until(below, tick, 720 + 4 * 20);
  check(below.stats(0).state == piu::LifeState::kBelowLine, "falló por misses");
  check(below.ending_time() < 0x1f, "antes de 0x1f el clip va directo");
  run_until(below, tick, 720 + 4 * 60);
  check(below.ending_time() > 0x70, "después salta a la parte de stage failed");
}

void test_stage_break() {
  const auto make = [](int stage, int stage_break) {
    piu::Chart chart;
    chart.row_seconds = 0.25;
    for (int i = 0; i < 800; ++i) {
      piu::ChartRow row;
      row.index = i;
      row.time = 1.0 + i * 0.25;
      row.mask = {1, 0};
      chart.rows.push_back(row);
    }
    piu::GameOptions options;
    options.stage = stage;
    options.stage_break = stage_break;
    return piu::Game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options);
  };
  piu::Game one = make(1, 0);
  std::int64_t tick = 0;
  run_until(one, tick, 240 * 195);
  check(one.stats(0).danger, "con 1 STAGE ya se puede fallar la etapa 1");
  piu::Game three = make(2, 2);
  tick = 0;
  run_until(three, tick, 240 * 195);
  check(!three.stats(0).danger, "con 3 STAGE la etapa 2 no se falla");
}

void test_modifiers() {
  const auto chart_with = [](std::uint16_t p1, std::uint16_t p2) {
    piu::Chart chart;
    chart.row_seconds = 0.25;
    piu::ChartRow row;
    row.index = 0;
    row.time = 1.0;
    row.mask = {p1, p2};
    chart.rows.push_back(row);
    return chart;
  };
  // Mirror swaps down-left with up-right.
  piu::GameOptions mirror;
  mirror.option_words = {piu::option_bits::kMirror, 0};
  piu::Game mirrored(chart_with(1u, 0), piu::Bga{}, piu::Bga{}, arrow_assets(), mirror);
  std::int64_t tick = 0;
  const std::int64_t at = tick_for_y(1.0, 37);
  mirrored.queue_input({at, 3, true});
  run_until(mirrored, tick, at + 8);
  check(mirrored.stats(0).last == piu::Judgment::kPerfect, "mirror mueve la flecha 0 a la columna 3");

  // Speed is per player: X2 reaches the zone at another moment.
  piu::GameOptions fast;
  fast.option_words = {0x10, 0};
  piu::Game speedy(chart_with(1u, 1u), piu::Bga{}, piu::Bga{}, arrow_assets(), fast);
  tick = 0;
  const std::int64_t early = at - 4 * 2;
  speedy.queue_input({early, 5, true});
  run_until(speedy, tick, early + 8);
  check(speedy.stats(1).last != piu::Judgment::kPerfect || speedy.stats(0).last == piu::Judgment::kNone,
        "la velocidad cambia por jugador");

  // Mirror alone is the only special option that mirrors.
  piu::GameOptions both;
  both.option_words = {piu::option_bits::kMirror | piu::option_bits::kVanish, 0};
  piu::Game plain(chart_with(1u, 0), piu::Bga{}, piu::Bga{}, arrow_assets(), both);
  tick = 0;
  plain.queue_input({at, 0, true});
  run_until(plain, tick, at + 8);
  check(plain.stats(0).last == piu::Judgment::kPerfect, "con otro truco ya no hace mirror, como el original");

  // Copy sends player one's arrows to player two.
  piu::GameOptions copy;
  copy.option_words = {piu::option_bits::kCopy, 0};
  piu::Game copied(chart_with(2u, 16u), piu::Bga{}, piu::Bga{}, arrow_assets(), copy);
  tick = 0;
  copied.queue_input({at, 5 + 1, true});
  run_until(copied, tick, at + 8);
  check(copied.stats(1).last == piu::Judgment::kPerfect, "J2 copia a J1");
}

void test_please_wait_and_icons() {
  piu::Assets assets = arrow_assets();
  std::string sprites = "TYPE TILE\n";
  for (int i = 0; i < 0x90; ++i) sprites += "T arrow01.tga 10 10 20 20 0 0 20 20\n";
  assets.set_sprite("gamescr", sprites);
  const auto vertices_with = [&](piu::CoinBox& box, std::uint16_t p1, std::uint16_t p2, std::uint32_t word = 0) {
    piu::Chart chart;
    chart.row_seconds = 0.25;
    piu::ChartRow row;
    row.index = 0;
    row.time = 100.0;
    row.mask = {p1, p2};
    chart.rows.push_back(row);
    piu::GameOptions options;
    options.coins = &box;
    options.option_words = {word, 0};
    piu::Game game(std::move(chart), piu::Bga{}, piu::Bga{}, assets, options);
    std::int64_t tick = 0;
    // Frame 20 of the 60-frame cycle: pulse visible and the blink is on.
    run_until(game, tick, 4 * 20);
    piu::DrawList list;
    game.draw(list, 0.0);
    return list.vertices().size();
  };
  piu::CoinBox credit;
  for (int i = 0; i < 5; ++i) credit.insert();
  piu::CoinBox none;
  const std::size_t one_with_credit = vertices_with(credit, 1, 0);
  const std::size_t both_with_credit = vertices_with(credit, 1, 1);
  const std::size_t one_without = vertices_with(none, 1, 0);
  check(vertices_with(credit, 1, 0, 0x30 | piu::option_bits::kMirror) == one_with_credit + 2 * 6, "un ícono por modificador");
  check(one_with_credit > 0 && one_without > 0, "se pinta");
  check(both_with_credit > one_with_credit, "con dos jugadores no hay leyenda pero sí su hud");
}

void test_demo_autoplay() {
  piu::Chart chart;
  chart.row_seconds = 0.25;
  for (int i = 0; i < 200; ++i) {
    piu::ChartRow row;
    row.index = i;
    row.time = 1.0 + i * 0.25;
    row.mask = {1, 1};
    chart.rows.push_back(row);
  }
  piu::CoinBox box;
  piu::GameOptions options;
  options.demo = true;
  options.coins = &box;
  piu::Game game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options);
  std::int64_t tick = 0;
  game.queue_input({100, 0, true});
  run_until(game, tick, 240 * 50);
  check(!game.finished(), "el demo sigue a los 50 segundos");
  for (int lane = 0; lane < 2; ++lane) {
    const piu::PlayerStats& stats = game.stats(lane);
    const int rows = stats.tally[0] + stats.tally[2];
    check(rows >= 195 && rows <= 198, "acierta todas las filas que ya pasaron");
    check(stats.tally[1] == 0 && stats.tally[3] == 0 && stats.tally[4] == 0, "nunca Great, Bad ni Miss");
    check(stats.tally[0] > rows / 2 && stats.tally[2] > rows / 6, "mezcla de Perfect y Good");
    check(stats.state == piu::LifeState::kPlaying, "no decide final");
  }
  run_until(game, tick, 240 * 51 + 1);
  check(game.finished(), "termina a los 51 segundos");

  piu::Chart short_chart;
  short_chart.row_seconds = 0.25;
  piu::ChartRow row;
  row.index = 0;
  row.time = 1.0;
  row.mask = {1, 0};
  short_chart.rows.push_back(row);
  piu::GameOptions coin_options;
  coin_options.demo = true;
  coin_options.coins = &box;
  piu::Game with_coin(std::move(short_chart), piu::Bga{}, piu::Bga{}, arrow_assets(), coin_options);
  tick = 0;
  run_until(with_coin, tick, 240);
  check(!with_coin.finished(), "sigue sin moneda");
  box.insert();
  check(with_coin.finished(), "una moneda termina el demo");
}

void test_attract_loop() {
  piu::Assets assets;
  assets.set_texture("intro", {1, 2560, 2280});
  assets.set_texture("white", {2, 4, 4});
  piu::Bga empty;
  piu::CoinBox box;
  piu::MachineSettings settings;
  piu::AttractLoop loop(empty, empty, assets, box, settings);
  const auto frames = [&](int count) {
    for (int i = 0; i < count * 4; ++i) loop.step();
  };
  using Phase = piu::AttractLoop::Phase;
  using Event = piu::AttractLoop::Event;
  frames(424);
  check(loop.phase() == Phase::kAndamiro, "Andamiro sigue en el cuadro 424");
  frames(2);
  check(loop.phase() == Phase::kIntro, "después va el intro");
  check(loop.take_event() == Event::kIntroMusicStart, "suena la pista del intro");
  frames(600);
  check(loop.phase() == Phase::kIntro, "el intro son 304 imágenes a 30 por segundo");
  frames(10);
  check(loop.phase() == Phase::kTitle, "después la pantalla de inicio");
  check(loop.take_event() == Event::kIntroMusicStop, "se detiene la música");
  frames(488);
  check(loop.phase() == Phase::kTitle, "sigue la pantalla de inicio");
  frames(28);
  check(loop.phase() == Phase::kDemo, "sin monedas pasa al demo");
  check(loop.take_event() == Event::kStartDemo && loop.demo_index() == 0, "pide el primer demo");
  loop.demo_finished();
  check(loop.phase() == Phase::kAndamiro, "vuelve a Andamiro");

  // A coin skips the intro and keeps the title on screen.
  box.configure(1, false);
  box.insert();
  box.insert();
  frames(2);
  check(loop.phase() == Phase::kTitle, "con moneda salta al inicio");
  frames(2000);
  check(loop.phase() == Phase::kTitle, "con monedas no se acaba el tiempo");
  loop.press_center(0);
  frames(1);
  check(box.coins() == 1, "cobra al entrar");
  frames(100);
  check(loop.phase() == Phase::kTitle, "espera al segundo jugador");
  frames(30);
  check(loop.take_event() == Event::kPlayersReady && loop.players() == 1, "empieza con un jugador");
}

void test_stage_cfg() {
  const char* text =
      "OFFSET 5 28\nTRACK 0 none none\nTRACK 3 eternal.bga t_love.spr\nMODE_NAME EASY\n"
      "SONG_NUM 2\nSONG 1 3 a_1.stf a_2.stf\nSONG 2 3 b_1.stf b_2.stf // comment\nSONG_NUM 1\nSONG 3 3 c_1.stf c_2.stf\n";
  const auto config = piu::parse_stage_cfg(text);
  check(config && config->cd_offset == 5 && config->cd_preview_offset == 28 && config->tracks.size() == 4, "lee las pistas");
  check(config && config->modes.size() == 1 && config->modes[0].stages.size() == 2 &&
            config->modes[0].stages[0][1].p2_step == "b_2.stf" && config->tracks[3].title == "t_love.spr",
        "lee modos, etapas y canciones");
}

void test_sound_test_goes_silent_on_exit() {
  piu::Assets assets;
  assets.set_texture("hfont1", {1, 256, 256});
  assets.set_texture("white", {2, 4, 4});
  piu::ServiceMenu menu(piu::MachineSettings{}, assets);
  for (int i = 0; i < 5; ++i) menu.press_test();  // SOUND TEST
  menu.press_service();
  menu.press_service();  // CD
  check(menu.take_track_request() == 1, "suena el CD");
  menu.press_test();
  menu.press_service();  // EFFECT
  menu.press_test();
  menu.press_service();  // EXIT
  check(menu.take_track_request() == 0, "al salir se detiene el CD");
  int last = -1;
  while (auto call = menu.sounds().take()) last = call->sound;
  check(last == piu::sfx::kStop, "al salir se detiene el efecto");
}

void test_key_map() {
  piu::KeyMap keys;
  check(keys.code(2) == "KeyS" && keys.action_for("Digit5") == piu::kKeyCoin1 && keys.action_for("KeyA") == -1, "teclas por defecto");
  check(piu::KeyMap::parse(keys.serialize()).has_value(), "lee lo que guarda");
  check(!piu::KeyMap::parse("KeyQ,KeyQ,KeyS,KeyE,KeyC,KeyM,KeyU,KeyK,KeyO,Period,Digit5,Digit6,F2,F3,F4").has_value(), "rechaza teclas repetidas");
  check(!piu::KeyMap::parse("").has_value() && !piu::KeyMap::parse("KeyQ").has_value(), "rechaza un guardado danado");
  check(piu::key_label("KeyQ") == "Q" && piu::key_label("Numpad7") == "N7" && piu::key_label("F12") == "F12" && piu::key_label("Period") == ".", "nombres de teclas");
  check(piu::key_label("Escape").empty() && piu::key_label("MetaLeft").empty(), "teclas reservadas");

  const piu::Texture atlas{1, 384, 192};
  piu::KeyMapScreen screen(keys, true, atlas);
  screen.press_key("KeyA");
  check(screen.keys().action_for("KeyA") == -1, "sin seleccion una tecla no hace nada");
  screen.click(166.0f, 186.0f);  // The center of the left pad.
  check(screen.selected() == 2, "clic en el centro del tapete");
  screen.press_key("KeyC");
  check(screen.selected() == 2 && screen.keys().code(2) == "KeyS", "una tecla en uso se rechaza");
  screen.press_key("KeyA");
  check(screen.selected() == -1 && screen.keys().code(2) == "KeyA", "asigna la tecla");
  screen.click(166.0f, 186.0f);
  screen.press_key("Escape");
  check(screen.selected() == -1 && screen.result() == piu::KeyMapScreen::Result::kRunning, "Esc cancela la captura");
  screen.click(100.0f, 440.0f);
  check(screen.keys().code(2) == "KeyS", "restablecer");
  screen.click(500.0f, 440.0f);
  check(screen.result() == piu::KeyMapScreen::Result::kSaved, "guardar");
  piu::KeyMapScreen other(keys, false, atlas);
  other.press_key("Escape");
  check(other.result() == piu::KeyMapScreen::Result::kCancelled, "Esc sale sin guardar");
  const piu::UiAtlas pixels = piu::make_ui_atlas();
  check(pixels.rgba.size() == static_cast<std::size_t>(pixels.width) * pixels.height * 4, "atlas completo");
  check(piu::ui::text_width("Map controls", 14.0f) > 40.0f, "el texto tiene ancho");
  check(piu::ui::text_width("Map controls", 22.0f) > piu::ui::text_width("Map controls", 14.0f), "crece con el tamano");
  check(piu::ui::text_width("Gu\xC3\xAD" "ate", 14.0f) > piu::ui::text_width("Guate", 14.0f), "letras con acento");
}

void test_second_player_turns_double_into_battle() {
  piu::StageConfig config;
  piu::StageMode mode;
  mode.stages = {{{1, 1, "a_1.stf", "a_2.stf"}}};
  config.tracks.resize(2);
  config.modes = {mode, mode, mode, mode, mode, mode};
  piu::Assets assets;
  assets.set_texture("bg", {9, 640, 480});
  piu::Bga screens;
  piu::BgaLayer battle;
  battle.index = 0x18 + piu::kModeBattle;
  battle.name = "bg.png";
  battle.keys.push_back({0x600, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true});
  battle.keys.push_back({0x640, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true});
  screens.layers.push_back(battle);
  piu::Bga empty;
  const auto make_session = [] {
    piu::SelectSession session;
    session.players = 1;
    session.stage = 1;
    return session;
  };
  piu::CoinBox box;
  box.configure(1, false);
  for (int i = 0; i < 2; ++i) box.insert();
  piu::SelectSession session = make_session();
  piu::SelectScreen screen(config, screens, empty, empty, assets, box, session, false);
  const auto advance = [&](piu::SelectScreen& s, int loops) {
    for (int i = 0; i < loops * 8; ++i) s.step();
  };
  const auto shows_battle = [&] {
    piu::DrawList list;
    screen.draw(list);
    return std::count_if(list.batches().begin(), list.batches().end(), [](const auto& b) { return b.texture == 9; }) > 0;
  };
  advance(screen, 2);
  check(!shows_battle(), "con un jugador la esquina es Double");
  screen.press(7);  // Center of the right pad.
  advance(screen, 2);
  check(session.players == 3 && shows_battle(), "al entrar el segundo jugador la esquina es Battle");

  piu::CoinBox other;
  other.configure(1, false);
  other.insert();
  piu::SelectSession blocked = make_session();
  piu::SelectScreen waiting(config, screens, empty, empty, assets, other, blocked, false);
  advance(waiting, 2);
  waiting.press(0);  // Down left: Double.
  advance(waiting, 2);
  waiting.press(7);
  advance(waiting, 2);
  check(blocked.players == 1, "con Double elegido no entra el segundo jugador");
}

void test_event_mode_selection() {
  piu::StageConfig config;
  config.cd_offset = 5;
  config.tracks.resize(4);
  piu::StageMode mode;
  mode.stages = {{{1, 1, "a_1.stf", "a_2.stf"}, {1, 2, "b_1.stf", "b_2.stf"}, {1, 3, "c_1.stf", "c_2.stf"}, {1, 3, "d_1.stf", "d_2.stf"}}};
  config.modes = {mode, mode, mode, mode, mode, mode};
  piu::Assets assets;
  assets.set_texture("bg", {9, 640, 480});
  piu::Bga music;
  piu::BgaLayer label;
  label.index = 0x10;  // The Non-Stop title, which EVENT does not show.
  label.name = "bg.png";
  label.keys.push_back({1200, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true});
  label.keys.push_back({1800, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, true});
  music.layers.push_back(label);
  piu::Bga empty;
  piu::CoinBox box;
  box.configure(1, true);
  piu::SelectSession session;
  session.players = 1;
  session.stage = 1;
  piu::SelectScreen screen(config, empty, music, empty, assets, box, session, true);
  check(session.mode == piu::kModeEvent && screen.take_event() == piu::SelectScreen::Event::kPlaySelectMusic, "EVENT va directo a la musica");
  const auto shows_label = [&] {
    piu::DrawList list;
    screen.draw(list);
    return std::any_of(list.batches().begin(), list.batches().end(), [](const auto& b) { return b.texture == 9; });
  };
  const auto advance = [&](int loops) {
    for (int i = 0; i < loops * 8; ++i) screen.step();
  };
  advance(2);
  check(!shows_label(), "EVENT no muestra titulo de modo");
  screen.press(0);  // Down left: the third song.
  advance(2);
  screen.press(0);
  advance(2);
  check(session.mode == piu::kModeNonstop && shows_label(), "las canciones 3 y 4 de EVENT son Non-Stop");
}

void test_select_screen() {
  piu::StageConfig config;
  config.cd_offset = 5;
  config.cd_preview_offset = 28;
  config.tracks.resize(4);
  config.tracks[3] = {"eternal.bga", "t_love.spr"};
  piu::StageMode easy;
  easy.name = "EASY";
  easy.stages = {{{2, 3, "a_1.stf", "a_2.stf"}, {3, 3, "b_1.stf", "b_2.stf"}, {4, 3, "c_1.stf", "c_2.stf"}, {4, 3, "d_1.stf", "d_2.stf"}}, {{1, 3, "e_1.stf", "e_2.stf"}}};
  config.modes = {easy, easy, easy, easy, easy, easy};
  piu::Assets assets;
  piu::Bga empty;
  piu::CoinBox box;
  box.configure(1, false);
  piu::SelectSession session;
  session.players = 1;
  session.stage = 1;
  piu::SelectScreen screen(config, empty, empty, empty, assets, box, session, false);
  using Event = piu::SelectScreen::Event;
  const auto advance = [&](int loops) {
    for (int i = 0; i < loops * 8; ++i) screen.step();
  };
  check(screen.take_event() == Event::kPlayModeMusic && screen.track() == 4, "pista de modo");
  advance(2);
  screen.press(3);  // Up right: Hard.
  advance(2);
  screen.press(3);  // Same panel confirms.
  advance(2);
  check(session.mode == piu::kModeHard, "pisar dos veces confirma el modo");
  advance(34);
  check(screen.take_event() == Event::kPlaySelectMusic && screen.track() == 5, "pasa a música");
  // 55159357 is Mirror for this player.
  for (int panel : {2, 2, 0, 2, 3, 4, 2, 1}) {
    screen.press(panel);
    advance(1);
  }
  check((session.option_words[0] & 0x200) != 0, "el código activa Mirror");
  while (screen.take_event()) {}
  screen.press(3);  // Up right: second CD.
  advance(2);
  check(screen.take_event() == Event::kPreview && screen.track() == 31, "suena la pista de la canción");
  screen.press(3);
  advance(40);
  std::optional<Event> event;
  while ((event = screen.take_event()) && *event != Event::kNeedTitle) {}
  check(event == Event::kNeedTitle && screen.song().p1_step == "b_1.stf", "elige la canción");
  screen.title_ready(assets);
  for (int i = 0; i < 4 * 59; ++i) screen.step();
  check(!screen.take_event().has_value(), "la tarjeta dura un segundo");
  for (int i = 0; i < 4 * 2; ++i) screen.step();
  check(screen.take_event() == Event::kDone, "después carga la canción");
}

void test_io_panels_match_their_boxes() {
  piu::Assets assets;
  assets.set_texture("white", {2, 4, 4});
  // Engine panel, then the corner of its box on the left pad of the screen.
  const std::array<std::array<float, 3>, 5> panels = {{{0, 0x28, 0x140}, {1, 0x28, 0xa0}, {2, 0x78, 0xf0}, {3, 0xc8, 0xa0}, {4, 0xc8, 0x140}}};
  for (const auto& p : panels) {
    piu::ServiceMenu menu(piu::MachineSettings{}, assets);
    menu.press_service();  // I/O TEST
    menu.set_panel(static_cast<int>(p[0]), true);
    piu::DrawList list;
    menu.draw(list);
    bool lit_inside = false;
    bool lit_outside = false;
    for (const piu::Vertex& v : list.vertices()) {
      if (v.color.b != 0.0f || v.color.r != 1.0f || v.color.g != 1.0f) continue;
      const bool inside = v.x >= p[1] && v.x <= p[1] + 0x50 && v.y >= p[2] && v.y <= p[2] + 0x50;
      lit_inside = lit_inside || inside;
      lit_outside = lit_outside || !inside;
    }
    check(lit_inside && !lit_outside, "cada flecha prende su propio panel");
  }
}

void test_service_menu() {
  piu::Assets assets;
  assets.set_texture("hfont1", {1, 256, 256});
  assets.set_texture("white", {2, 4, 4});
  piu::MachineSettings start;
  piu::ServiceMenu menu(start, assets);
  for (int i = 0; i < 3; ++i) menu.press_test();  // GAME OPTION
  menu.press_service();
  menu.press_service();  // GAME MODE -> EVENT
  menu.press_test();
  menu.press_service();  // DIFFICULTY -> NORMAL
  for (int i = 0; i < 6; ++i) menu.press_test();  // EXIT
  menu.press_service();
  check(menu.settings().game_mode == 0 && menu.settings().difficulty == 0, "EXIT no guarda");

  menu.press_service();  // Back in GAME OPTION with the cursor on EXIT.
  for (int i = 0; i < 2; ++i) menu.press_test();
  menu.press_service();  // Cursor wrapped to row 1: DIFFICULTY.
  for (int i = 0; i < 5; ++i) menu.press_test();  // SAVE AND EXIT
  menu.press_service();
  check(menu.settings().difficulty == 1, "SAVE AND EXIT guarda");

  menu.press_test();  // COIN OPTION
  menu.press_service();
  menu.press_service();  // 5 -> 6
  menu.press_test();
  menu.press_test();  // SAVE AND EXIT
  menu.press_service();
  check(menu.settings().coins_per_credit == 6, "monedas por crédito");
  piu::MachineSettings wrap;
  wrap.coins_per_credit = 10;
  piu::ServiceMenu coin(wrap, assets);
  for (int i = 0; i < 4; ++i) coin.press_test();
  coin.press_service();
  coin.press_service();  // 10 -> 1
  coin.press_test();
  coin.press_test();
  coin.press_service();
  check(coin.settings().coins_per_credit == 1, "de 10 regresa a 1");

  for (int i = 0; i < 2; ++i) menu.press_test();  // EXIT
  piu::DrawList list;
  menu.draw(list);
  check(!list.vertices().empty(), "pinta el menú");
  menu.press_service();
  check(menu.exited(), "EXIT sale del menú");
  check(!piu::MachineSettings{0, 0, 3, 1, 1, 5}.valid(), "valores fuera de rango se rechazan");
}

void test_chart_margin_table() {
  check(piu::chart_margin("CAT_1.stf") == 2 && piu::chart_margin("nova_db.stf") == 0, "tabla de márgenes");
  check(piu::chart_margin("desconocido.stf") == 0, "por defecto");
}

}  // namespace

void test_life_bar_alternates_per_drawn_frame() {
  piu::Game game = make_game(chart_with_rows({{1.0, 1}}));
  std::int64_t tick = 0;
  run_until(game, tick, 40);
  std::array<std::size_t, 4> sizes{};
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    piu::DrawList list;
    game.draw(list, 0.0);
    sizes[i] = list.vertices().size();
  }
  check(sizes[0] == sizes[1] + 6 && sizes[2] == sizes[0] && sizes[3] == sizes[1], "el relleno alterna sin avanzar el reloj");
}

void test_two_life_bars_take_turns() {
  piu::Chart chart = chart_with_rows({{1.0, 1}});
  chart.rows[0].mask[1] = 1;
  piu::Game game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options_none());
  std::int64_t tick = 0;
  run_until(game, tick, 40);
  std::array<std::size_t, 4> sizes{};
  for (std::size_t i = 0; i < sizes.size(); ++i) {
    piu::DrawList list;
    game.draw(list, 0.0);
    sizes[i] = list.vertices().size();
  }
  check(sizes[0] > 0 && sizes[0] == sizes[1] && sizes[1] == sizes[2] && sizes[2] == sizes[3], "siempre hay exactamente una barra encendida");
}

void test_demo_label_is_bright_with_the_left_bar() {
  piu::GameOptions options;
  options.demo = true;
  piu::Game game(chart_with_rows({{1.0, 1}}), piu::Bga{}, piu::Bga{}, arrow_assets(), options);
  std::int64_t tick = 0;
  run_until(game, tick, 40);
  piu::DrawList bright;
  game.draw(bright, 0.0);
  piu::DrawList dim;
  game.draw(dim, 0.0);
  check(!bright.vertices().empty() && near(bright.vertices().back().color.r, 1.0, 1e-6), "brillante en el cuadro de la barra izquierda");
  check(!dim.vertices().empty() && near(dim.vertices().back().color.a, 0.3, 1e-6), "tenue en el cuadro de la barra derecha");
}

piu::Game battle_game() {
  // Same timing as chart_with_rows: row 2 reaches y = 35 at 0.5 s.
  piu::Chart chart;
  chart.source = "BATTLE1.STF";
  chart.first_row_s = 0.0;
  chart.row_seconds = 0.25;
  for (int index = 0; index < 34; index += 4) {
    piu::ChartRow row;
    row.index = index;
    row.time = index * 0.25;
    row.mask = {0x1f, 0x1f};
    chart.rows.push_back(row);
  }
  piu::GameOptions options;
  options.option_words = {0, 0x400 | 0x30 | 3};
  return piu::Game(std::move(chart), piu::Bga{}, piu::Bga{}, arrow_assets(), options);
}

void test_battle_sends_steps_to_the_rival() {
  piu::Game game = battle_game();
  std::int64_t tick = 0;
  // Battle1 attacks while (row - 2) / 32 is even; row 12 is an attack row.
  tap(game, tick, 0, tick_for_y(12 * 0.25, 33));
  run_until(game, tick, tick_for_y(12 * 0.25, 28));
  check(game.stats(0).tally[0] == 1, "pisar cualquier panel en la fila blanca califica");
  check(game.stats(0).life > 15.0f, "la barra de J1 se llena");
  check(game.stats(1).tally[0] == 0, "J2 no recibe esa calificación");

  // Sent to row 12 + 32 = 44. J2 misses it if nobody steps.
  run_until(game, tick, tick_for_y(44 * 0.25, 8));
  check(game.stats(1).tally[4] == 1, "J2 falla la fila que le mandaron");
  check(game.stats(0).tally[4] == 0, "J1 no tiene filas que no le mandaron");
}

void test_battle_defence_is_judged_on_what_was_sent() {
  piu::Game game = battle_game();
  std::int64_t tick = 0;
  tap(game, tick, 5, tick_for_y(16 * 0.25, 33));
  run_until(game, tick, tick_for_y(16 * 0.25, 20));
  check(game.stats(1).tally[0] == 1 && game.stats(1).life > 15.0f, "J2 también ataca y llena su barra");
  // Row 48 for J1 now holds only column 0.
  tap(game, tick, 0, tick_for_y(48 * 0.25, 33));
  run_until(game, tick, tick_for_y(48 * 0.25, 20));
  check(game.stats(0).tally[0] == 1, "J1 acierta con solo el panel enviado");
}

void test_battle_cannot_fail_and_ends_together() {
  piu::Game game = battle_game();
  std::int64_t tick = 0;
  // Last row 32 + block 32 + 10 = row 74, then a 60-frame ending.
  run_until(game, tick, static_cast<std::int64_t>(74.0 * 0.25 * 240.0) + 8);
  check(game.stats(0).state == piu::LifeState::kCleared && game.stats(1).state == piu::LifeState::kCleared,
        "los dos terminan juntos");
  run_until(game, tick, tick + 62 * 4);
  check(game.finished(), "el cierre de Battle dura 60 cuadros");
}

void test_battle_field_is_empty_before_the_first_block() {
  piu::Game game = battle_game();
  std::int64_t tick = 0;
  run_until(game, tick, 24);
  piu::DrawList before;
  game.draw(before, 0.0);
  run_until(game, tick, 300);
  skip_drawn_frame(game);
  piu::DrawList attacking;
  game.draw(attacking, 0.0);
  check(attacking.vertices().size() > before.vertices().size(), "las filas aparecen al empezar el primer bloque");
}

void test_battle_sends_only_the_panels_of_the_first_frame() {
  const auto play = [](std::initializer_list<int> defence) {
    piu::Game game = battle_game();
    std::int64_t tick = 0;
    const std::int64_t at = tick_for_y(12 * 0.25, 33);
    tap(game, tick, 0, at);
    tap(game, tick, 1, at + 4);
    tap(game, tick, 3, at + 8);
    run_until(game, tick, tick_for_y(12 * 0.25, 20));
    check(game.stats(0).tally[0] == 1, "el ataque cuenta una sola vez");
    const std::int64_t defend = tick_for_y(44 * 0.25, 33);
    for (int panel : defence) game.queue_input({defend, panel, true});
    run_until(game, tick, tick_for_y(44 * 0.25, 20));
    return game.stats(1).tally[0];
  };
  check(play({5}) == 1, "se envia solo el panel del primer cuadro");
}

void test_battle_results_show_win_and_lose() {
  piu::Assets assets;
  assets.set_texture("dec2", {1, 256, 256});
  assets.set_texture("arrow01", {2, 256, 256});
  std::string sprites = "TYPE TILE\n";
  for (int i = 0; i < 0xdd; ++i) sprites += "T arrow01.tga 10 10 20 20 0 0 20 20\n";
  assets.set_sprite("gamescr", sprites);
  std::array<piu::ResultPlayer, 2> players{};
  players[0].active = players[1].active = true;
  players[0].points = 2000;
  players[1].points = 1000;
  players[0].grade = players[1].grade = 'A';
  const auto final_quads = [&](bool battle, std::vector<int>& sounds) {
    piu::ResultsScreen screen(players, assets, true, battle);
    for (int i = 0; i < 240 * 12 && !screen.closing(); ++i) {
      screen.step();
      while (auto call = screen.sounds().take()) sounds.push_back(call->sound);
    }
    piu::DrawList list;
    screen.draw(list);
    return list.vertices().size() / 6;
  };
  std::vector<int> normal_sounds, battle_sounds;
  const std::size_t normal = final_quads(false, normal_sounds);
  const std::size_t battle = final_quads(true, battle_sounds);
  check(normal == battle + 2 * 10 - 2, "dos sprites en lugar de las dos letras");
  check(std::find(normal_sounds.begin(), normal_sounds.end(), piu::sfx::kLetterA) != normal_sounds.end(), "la letra suena fuera de Battle");
  check(std::find(battle_sounds.begin(), battle_sounds.end(), piu::sfx::kLetterA) == battle_sounds.end(), "Battle no tiene sonidos de letra");
  players[1].points = 2000;
  std::vector<int> tie_sounds;
  check(final_quads(true, tie_sounds) + 2 == battle, "en empate no sale ninguno");
}

void test_results_stay_dark_after_closing() {
  piu::Assets assets;
  assets.set_texture("dec2", {1, 256, 256});
  assets.set_texture("arrow01", {2, 256, 256});
  std::string sprites = "TYPE TILE\n";
  for (int i = 0; i < 0xdd; ++i) sprites += "T arrow01.tga 10 10 20 20 0 0 20 20\n";
  assets.set_sprite("gamescr", sprites);
  std::array<piu::ResultPlayer, 2> players{};
  players[0].active = true;
  players[0].grade = 'A';
  piu::ResultsScreen screen(players, assets, true);
  for (int i = 0; i < 240 * 20 && !screen.finished(); ++i) screen.step();
  check(screen.finished(), "termina el cierre");
  piu::DrawList list;
  screen.draw(list);
  bool dark = !list.vertices().empty();
  for (const piu::Vertex& vertex : list.vertices()) dark = dark && vertex.color.r <= 0.0f;
  check(dark, "no vuelve a verse la pantalla al terminar el fundido");
}

void test_sprite_edges_stay_inside_their_tile() {
  piu::DrawList list;
  piu::emit_quad(list, {1, 256, 256}, 0.0f, 0.0f, 64.0f, 32.0f, 0.0f, 32.0f, 64.0f, 64.0f, piu::kIdentity,
                 {1.0f, 1.0f, 1.0f, 1.0f}, 0);
  const piu::Vertex& first = list.vertices()[0];
  check(near(first.u, 0.5 / 256.0, 1e-6) && near(first.v, 32.5 / 256.0, 1e-6), "el borde no toca la fila vecina");
}

void test_battle_needs_two_players() {
  piu::StageConfig config;
  config.tracks.resize(1);
  piu::StageMode mode;
  mode.stages = {{{1, 0, "a_1.stf", "a_2.stf"}}};
  config.modes = {mode, mode, mode, mode, mode, mode};
  piu::Assets assets;
  piu::Bga empty;
  piu::CoinBox box;
  box.configure(1, false);
  for (int players : {1, 3}) {
    piu::SelectSession session;
    session.players = players;
    session.stage = 1;
    piu::SelectScreen screen(config, empty, empty, empty, assets, box, session, false);
    for (int i = 0; i < 16; ++i) screen.step();
    screen.press(0);
    for (int i = 0; i < 16; ++i) screen.step();
    screen.press(0);
    for (int i = 0; i < 16; ++i) screen.step();
    check(session.mode == (players == 3 ? piu::kModeBattle : piu::kModeDouble), "abajo a la izquierda: Battle con dos jugadores");
  }
}

void test_codes_work_on_mode_and_music_screens() {
  piu::StageConfig config;
  config.tracks.resize(4);
  piu::StageMode mode;
  mode.stages = {{{1, 1, "a_1.stf", "a_2.stf"}, {1, 2, "b_1.stf", "b_2.stf"}, {1, 3, "c_1.stf", "c_2.stf"}, {1, 1, "d_1.stf", "d_2.stf"}}};
  config.modes = {mode, mode, mode, mode, mode, mode};
  piu::Assets assets;
  piu::Bga empty;
  piu::CoinBox box;
  box.configure(1, false);
  piu::SelectSession session;
  session.players = 1;
  session.stage = 1;
  piu::SelectScreen screen(config, empty, empty, empty, assets, box, session, false);
  const auto enter = [&](std::initializer_list<int> panels) {
    for (int panel : panels) {
      screen.press(panel);
      for (int i = 0; i < 16; ++i) screen.step();
    }
  };
  for (int i = 0; i < 16; ++i) screen.step();
  enter({2, 2, 0, 2, 3, 4, 2, 1});  // 55159357: Mirror, on the mode screen.
  check((session.option_words[0] & 0x200) != 0, "truco en la pantalla de modo");
  session.mode = piu::kModeEasy;
  screen.press(3);
  for (int i = 0; i < 16; ++i) screen.step();
  screen.press(3);
  for (int i = 0; i < 16 * 40; ++i) screen.step();
  enter({2, 2, 3, 1, 2, 4, 0, 2});  // 55975315: Vanish, on the music screen.
  check((session.option_words[0] & 0x100) != 0, "truco en la pantalla de música");
  check((session.option_words[0] & 0x200) != 0, "el primer truco sigue activo");
}

void test_preview_cuts_the_grade_sounds() {
  piu::StageConfig config;
  config.tracks.resize(4);
  piu::StageMode mode;
  mode.stages = {{{1, 1, "a_1.stf", "a_2.stf"}, {1, 2, "b_1.stf", "b_2.stf"}, {1, 3, "c_1.stf", "c_2.stf"}, {1, 1, "d_1.stf", "d_2.stf"}}};
  config.modes = {mode, mode, mode, mode, mode, mode};
  piu::Assets assets;
  piu::Bga empty;
  piu::CoinBox box;
  box.configure(1, false);
  piu::SelectSession session;
  session.players = 1;
  session.stage = 2;  // Later stages go straight to the music screen.
  piu::SelectScreen screen(config, empty, empty, empty, assets, box, session, false);
  for (int i = 0; i < 16; ++i) screen.step();
  while (screen.sounds().take()) {
  }
  screen.press(1);
  for (int i = 0; i < 16; ++i) screen.step();
  std::vector<int> stopped;
  while (auto call = screen.sounds().take()) {
    if (call->sound == piu::sfx::kStop) stopped.push_back(call->channel);
  }
  check(stopped == std::vector<int>({3, 6}), "el preview corta los canales de la letra");
}

void test_coin_sounds() {
  piu::CoinBox box;
  box.configure(3, false);
  check(box.next_coin_sound() == piu::sfx::kCoin, "moneda normal");
  box.insert();
  box.insert();
  check(box.next_coin_sound() == piu::sfx::kCredit, "la moneda que completa el crédito");
  box.configure(1, false);
  check(box.next_coin_sound() == piu::sfx::kCredit, "con una moneda por crédito siempre completa");
}

void test_results_sounds() {
  std::array<piu::ResultPlayer, 2> players{};
  players[0].active = true;
  players[0].grade = 'B';
  piu::Assets assets;
  piu::ResultsScreen screen(players, assets);
  std::vector<std::pair<int, int>> calls;
  for (int i = 0; i < 240 * 12 && !screen.finished(); ++i) {
    screen.step();
    while (auto call = screen.sounds().take()) calls.emplace_back(call->sound, call->channel);
  }
  const auto count = [&](int sound, int channel) { return std::count(calls.begin(), calls.end(), std::make_pair(sound, channel)); };
  check(count(piu::sfx::kStart, 1) == 6, "una entrada por fila");
  check(count(piu::sfx::kGrade, 2) == 1, "la voz antes de la letra");
  check(count(piu::sfx::kLetterA + 1, 3) == 1 && count(piu::sfx::kVoiceA + 1, 6) == 1, "los dos sonidos de la B");
}

void test_sound_decoder() {
  piu::extract::Bytes table(16, 0);
  table[0] = 255;
  table[12 + 3] = 7;  // Ends at byte 7.
  piu::extract::Bytes silence(8, 0x80);
  const auto sounds = piu::extract::decode_sounds(table, silence);
  check(sounds.size() == 1 && sounds[0].size() == 44 + 32, "un WAV de 16 muestras");
  const auto rate = sounds.empty() ? 0 : sounds[0][24] | sounds[0][25] << 8;
  check(rate == 44100, "FN 255 es 44.1 kHz");
  const auto sample = sounds.empty() ? 1000 : static_cast<std::int16_t>(sounds[0][44 + 2] | sounds[0][45 + 2] << 8);
  check(std::abs(sample) < 200, "la secuencia 8, 0 es silencio");
  check(piu::extract::decode_sounds(table, piu::extract::Bytes(8, 0x77)).empty(), "rechaza datos que saturan");
  check(piu::extract::decode_sounds(table, piu::extract::Bytes(4, 0x80)).empty(), "rechaza una ROM corta");
}

void test_res_container() {
  piu::extract::Bytes data = {'R', 'E', 'S', 0, 1, 0, 0, 0, 1, 0, 0, 0, 3, 0, 0, 0};
  data.resize(16 + 24, 0);
  std::memcpy(&data[16], "A.PTX", 5);
  data[16 + 16] = 3;
  data.insert(data.end(), {7, 8, 9});
  const auto entries = piu::extract::parse_res(data);
  check(entries.has_value(), "entries.has_value()");
  check(entries->size() == 1, "entries->size() == 1");
  check((*entries)[0].name == "A.PTX", "(*entries)[0].name == \"A.PTX\"");
  check(data[(*entries)[0].offset + 2] == 9, "data[(*entries)[0].offset + 2] == 9");
  data.pop_back();
  check(!piu::extract::parse_res(data).has_value(), "!piu::extract::parse_res(data).has_value()");
}

void test_ptx_all_zero_block_is_mid_gray() {
  // One 8x8 block, three planes: DC category 0 and end of block in each.
  const piu::extract::Bytes data = {'P', 'T', 'X', 0, 0, 1, 1, 0, 1, 0, 0x81, 0, 50, 0, 0, 0, 0x28, 0x00, 0x00};
  const auto image = piu::extract::decode_ptx(data.data(), data.size());
  check(image.has_value(), "image.has_value()");
  check(image->width == 8 && image->height == 8 && image->channels == 3, "image->width == 8 && image->height == 8 && image->channels == 3");
  check(image->pixels.size() == 8 * 8 * 3, "image->pixels.size() == 8 * 8 * 3");
  for (std::uint8_t value : image->pixels) check(value == 128, "value == 128");
  check(!piu::extract::decode_ptx(data.data(), 8).has_value(), "!piu::extract::decode_ptx(data.data(), 8).has_value()");
}

void test_png_is_stored_rgb() {
  const piu::extract::Bytes pixels = {1, 2, 3};
  const auto png = piu::extract::encode_png(1, 1, 3, pixels);
  const piu::extract::Bytes big(70000 * 4, 7);
  const auto large = piu::extract::encode_png(350, 200, 4, big);
  const auto read_back = piu::extract::decode_stored_png(large.data(), large.size());
  check(read_back && read_back->width == 350 && read_back->channels == 4 && read_back->pixels == big, "lee los PNG del extractor");
  check(!piu::extract::decode_stored_png(png.data(), 7).has_value(), "rechaza un PNG cortado");
  check(png[0] == 0x89 && png[1] == 'P', "png[0] == 0x89 && png[1] == 'P'");
  check(png[16 + 3] == 1 && png[20 + 3] == 1, "png[16 + 3] == 1 && png[20 + 3] == 1");  // Width and height.
  check(png[24] == 8 && png[25] == 2, "png[24] == 8 && png[25] == 2");          // 8-bit RGB.
  // The zlib header, a final stored block of 4 bytes, then filter and pixels.
  const std::size_t idat = 8 + 25 + 8;
  check(png[idat] == 0x78 && png[idat + 2] == 1 && png[idat + 3] == 4, "png[idat] == 0x78 && png[idat + 2] == 1 && png[idat + 3] == 4");
  check(png[idat + 7] == 0 && png[idat + 8] == 1 && png[idat + 10] == 3, "png[idat + 7] == 0 && png[idat + 8] == 1 && png[idat + 10] == 3");
}

void test_bga_and_chart_json() {
  piu::extract::Bytes bga = {'B', 'G', 'A', 0, 100, 0, 0, 0, 3, 0, 0, 0, 44, 1, 0, 0};
  bga.resize(16 + 40 * 68, 0);
  std::memcpy(&bga[16], "layer", 5);
  bga[16 + 64] = 1;
  piu::extract::Bytes key(44, 0);
  key[0] = 60;
  key[2] = 1;
  bga.insert(bga.begin() + 16 + 68, key.begin(), key.end());
  const std::string json = piu::extract::bga_to_json(bga, "x.bga");
  check(piu::Json::parse(json).has_value(), "piu::Json::parse(json).has_value()");
  check(json.find("\"strt\":300") != std::string::npos, "json.find(\"\\\"strt\\\":300\") != std::string::npos");
  check(json.find("\"time_s\":4") != std::string::npos, "json.find(\"\\\"time_s\\\":4\") != std::string::npos");
  check(json.find("\"name\":\"layer\"") != std::string::npos, "json.find(\"\\\"name\\\":\\\"layer\\\"\") != std::string::npos");
  check(piu::extract::bga_to_json({1, 2}, "x.bga").empty(), "piu::extract::bga_to_json({1, 2}, \"x.bga\").empty()");

  piu::extract::Bytes stf(14616, 0);
  const float bpm = 120.0f;
  std::memcpy(&stf[252 + 4], &bpm, 4);
  stf[252 + 12] = 4;
  stf[252 + 20] = 7;
  stf[252 + 24] = 100;
  for (std::size_t row = 0; row < 1024; ++row) std::memset(&stf[0x118 + row * 14], '0', 13);
  stf[0x118 + 2 * 14 + 1] = '1';
  const std::string chart = piu::extract::stf_to_json(stf, "x.stf");
  check(piu::Json::parse(chart).has_value(), "piu::Json::parse(chart).has_value()");
  check(chart.find("\"cd_track\":8") != std::string::npos, "chart.find(\"\\\"cd_track\\\":8\") != std::string::npos");
  check(chart.find("\"first_row_time_s\":1.035") != std::string::npos, "chart.find(\"\\\"first_row_time_s\\\":1.035\") != std::string::npos");
  check(chart.find("\"row\":2,\"time_s\":1.285") != std::string::npos, "chart.find(\"\\\"row\\\":2,\\\"time_s\\\":1.285\") != std::string::npos");
  check(chart.find("\"pad_left\":\"01000\"") != std::string::npos, "chart.find(\"\\\"pad_left\\\":\\\"01000\\\"\") != std::string::npos");
}

int main() {
  test_clock_has_no_drift();
  test_clock_skips_suspended_time();
  test_clock_follows_audio();
  test_json();
  test_chart_parse();
  test_bga_pose();
  test_sprite_and_asset_names();
  test_each_judgment_band();
  test_margin_changes_bands();
  test_late_miss();
  test_row_needs_all_panels();
  test_short_tap_is_not_lost();
  test_press_expires_after_a_few_frames();
  test_press_counts_within_window();
  test_combo_bonus_and_miss_penalty();
  test_missing_assets_are_reported();
  test_draw_matches_expected_geometry();
  test_double_is_one_player_with_ten_columns();
  test_receptor_effects_and_judgment_text();
  test_players_keep_separate_stats();
  test_grades();
  test_finishes_after_ending();
  test_life_changes_and_clamps();
  test_failure_after_long_danger();
  test_failure_hides_the_song_bga();
  test_scene_flight();
  test_stage_one_cannot_fail();
  test_coins();
  test_frame_limiter_holds_60_fps();
  test_display_swap_interval();
  test_calibration();
  test_results_screen();
  test_ending_clip_time();
  test_stage_break();
  test_modifiers();
  test_please_wait_and_icons();
  test_demo_autoplay();
  test_stage_cfg();
  test_select_screen();
  test_event_mode_selection();
  test_key_map();
  test_sound_test_goes_silent_on_exit();
  test_second_player_turns_double_into_battle();
  test_attract_loop();
  test_io_panels_match_their_boxes();
  test_service_menu();
  test_chart_margin_table();
  test_life_bar_alternates_per_drawn_frame();
  test_two_life_bars_take_turns();
  test_demo_label_is_bright_with_the_left_bar();
  test_battle_sends_steps_to_the_rival();
  test_battle_defence_is_judged_on_what_was_sent();
  test_battle_cannot_fail_and_ends_together();
  test_battle_field_is_empty_before_the_first_block();
  test_battle_sends_only_the_panels_of_the_first_frame();
  test_battle_results_show_win_and_lose();
  test_results_stay_dark_after_closing();
  test_sprite_edges_stay_inside_their_tile();
  test_battle_needs_two_players();
  test_codes_work_on_mode_and_music_screens();
  test_preview_cuts_the_grade_sounds();
  test_coin_sounds();
  test_results_sounds();
  test_sound_decoder();
  test_res_container();
  test_ptx_all_zero_block_is_mid_gray();
  test_png_is_stored_rgb();
  test_bga_and_chart_json();
  if (failures == 0) std::printf("OK\n");
  return failures == 0 ? 0 : 1;
}
