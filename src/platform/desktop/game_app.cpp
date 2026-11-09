// Desktop game: window, keyboard and the flow from one screen to the next.

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "core/frame_limiter.h"
#include "core/key_map.h"
#include "extractor/formats.h"
#include "platform/desktop/audio.h"
#include "platform/desktop/game_app.h"
#include "platform/desktop/library.h"
#include "platform/engine_api.h"
#include "render/gl.h"

namespace piu::desktop {
namespace {

constexpr int kTrackOffset = 5;
constexpr int kIntroTrack = 2;
constexpr int kHelpTrack = 3;

struct SongTrack {
  const char* name;
  int id;
};
constexpr SongTrack kSongTracks[] = {{"ignition", 1}, {"hypnosis", 2}, {"eternal", 3}, {"passion", 4}, {"cat", 5},
                                     {"formlife", 6}, {"rapkyo", 7},   {"dol", 8},     {"funky", 9},   {"youwanna", 10},
                                     {"badblood", 11}, {"nova2", 12},  {"iwantyou", 13}, {"dontknow", 14}, {"cause", 15},
                                     {"mix1", 16},    {"mix2", 17},    {"mix3", 18},   {"mix4", 19}};

struct DemoSong {
  const char* chart;
  int cd;
};
// The first ten songs of the EASY table in stage.cfg, in order.
constexpr DemoSong kDemoSongs[10] = {{"finkl_2", 3}, {"ff_ez_2", 6},  {"rap_2", 7},  {"tiger_2", 13}, {"tiger_2", 13},
                                     {"yj_ez_2", 4}, {"dol_ez_2", 8}, {"no_ez_2", 12}, {"molla_2", 14}, {"eu_2", 15}};

// The name of a key, the way the layout stores it. Empty for keys that cannot be assigned.
std::string key_code(SDL_Scancode code) {
  if (code >= SDL_SCANCODE_A && code <= SDL_SCANCODE_Z) return std::string("Key") + static_cast<char>('A' + (code - SDL_SCANCODE_A));
  if (code >= SDL_SCANCODE_1 && code <= SDL_SCANCODE_9) return std::string("Digit") + static_cast<char>('1' + (code - SDL_SCANCODE_1));
  if (code >= SDL_SCANCODE_KP_1 && code <= SDL_SCANCODE_KP_9) return std::string("Numpad") + static_cast<char>('1' + (code - SDL_SCANCODE_KP_1));
  if (code >= SDL_SCANCODE_F1 && code <= SDL_SCANCODE_F12) return "F" + std::to_string(code - SDL_SCANCODE_F1 + 1);
  switch (code) {
    case SDL_SCANCODE_0: return "Digit0";
    case SDL_SCANCODE_KP_0: return "Numpad0";
    case SDL_SCANCODE_MINUS: return "Minus";
    case SDL_SCANCODE_EQUALS: return "Equal";
    case SDL_SCANCODE_LEFTBRACKET: return "BracketLeft";
    case SDL_SCANCODE_RIGHTBRACKET: return "BracketRight";
    case SDL_SCANCODE_BACKSLASH: return "Backslash";
    case SDL_SCANCODE_SEMICOLON: return "Semicolon";
    case SDL_SCANCODE_APOSTROPHE: return "Quote";
    case SDL_SCANCODE_GRAVE: return "Backquote";
    case SDL_SCANCODE_COMMA: return "Comma";
    case SDL_SCANCODE_PERIOD: return "Period";
    case SDL_SCANCODE_SLASH: return "Slash";
    case SDL_SCANCODE_SPACE: return "Space";
    case SDL_SCANCODE_RETURN: return "Enter";
    case SDL_SCANCODE_TAB: return "Tab";
    case SDL_SCANCODE_BACKSPACE: return "Backspace";
    case SDL_SCANCODE_LSHIFT: return "ShiftLeft";
    case SDL_SCANCODE_RSHIFT: return "ShiftRight";
    case SDL_SCANCODE_LCTRL: return "ControlLeft";
    case SDL_SCANCODE_RCTRL: return "ControlRight";
    case SDL_SCANCODE_LALT: return "AltLeft";
    case SDL_SCANCODE_RALT: return "AltRight";
    case SDL_SCANCODE_CAPSLOCK: return "CapsLock";
    case SDL_SCANCODE_UP: return "ArrowUp";
    case SDL_SCANCODE_DOWN: return "ArrowDown";
    case SDL_SCANCODE_LEFT: return "ArrowLeft";
    case SDL_SCANCODE_RIGHT: return "ArrowRight";
    case SDL_SCANCODE_INSERT: return "Insert";
    case SDL_SCANCODE_DELETE: return "Delete";
    case SDL_SCANCODE_HOME: return "Home";
    case SDL_SCANCODE_END: return "End";
    case SDL_SCANCODE_PAGEUP: return "PageUp";
    case SDL_SCANCODE_PAGEDOWN: return "PageDown";
    case SDL_SCANCODE_KP_ENTER: return "NumpadEnter";
    case SDL_SCANCODE_KP_PLUS: return "NumpadAdd";
    case SDL_SCANCODE_KP_MINUS: return "NumpadSubtract";
    case SDL_SCANCODE_KP_MULTIPLY: return "NumpadMultiply";
    case SDL_SCANCODE_KP_DIVIDE: return "NumpadDivide";
    case SDL_SCANCODE_KP_PERIOD: return "NumpadDecimal";
    default: return {};
  }
}

std::string base_name(std::string_view name) {
  while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.remove_prefix(1);
  while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.remove_suffix(1);
  const std::size_t slash = name.find_last_of("\\/");
  if (slash != std::string_view::npos) name.remove_prefix(slash + 1);
  const std::size_t dot = name.rfind('.');
  if (dot != std::string_view::npos) name = name.substr(0, dot);
  std::string out(name);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return out;
}

// The launcher closes as this window opens, and the system may then bring another application
// forward. For this long the window insists on being in front.
constexpr double kFocusClaimMs = 2000.0;
constexpr double kFocusRetryMs = 150.0;

const char* track_key_prefix = "wav:track";

std::string track_key(int track) {
  std::string number = std::to_string(track);
  if (number.size() < 2) number.insert(0, 1, '0');
  return track_key_prefix + number;
}

// The flow of the machine, the same one web/shell.html runs in the browser.
class GameApp {
 public:
  GameExit run();
  GameExit run_key_map();

 private:
  enum class Flow { kNone, kHelp, kSelect, kGame };

  bool start_video();
  void claim_focus();
  bool create_window(const Settings& settings, int width, int height);
  double now() const;
  bool load_flow();
  void restore_machine_settings();
  void apply_machine_settings();
  void save_machine_settings();

  void provide_assets();
  void provide_texture(const std::string& key);
  void provide_sprite(const std::string& key);
  bool play_track(int track, bool loop = false);

  bool start_attract();
  bool prepare_flow();
  void back_to_attract();
  void start_credit(int players);
  void begin_select();
  void handle_select_events();
  void play_selected();
  void start_demo(int index);
  void end_demo();
  void handle_attract_events();
  void open_service();
  void close_service();
  void end_song();
  void finish_results();
  void frame();
  void handle_key(const SDL_KeyboardEvent& key, bool down);
  void release_held();
  std::string read_or(const char* key, const char* fallback);
  bool prepare_song(const std::string& chart, const std::string& bga);

  SDL_Window* window_ = nullptr;
  Settings settings_;
  KeyMap keys_;
  Library library_;
  Audio audio_;
  std::set<int> held_;
  Uint64 clock_base_ = 0;
  double clock_scale_ = 0.0;
  double focus_deadline_ = 0.0;
  double next_focus_try_ = 0.0;

  bool running_ = true;
  GameExit exit_ = GameExit::kQuit;
  bool playing_ = false;
  bool has_audio_ = false;
  bool in_results_ = false;
  bool in_service_ = false;
  bool in_attract_ = false;
  bool in_demo_ = false;
  int last_stage_ = 1;
  Flow flow_ = Flow::kNone;
};

double GameApp::now() const { return static_cast<double>(SDL_GetPerformanceCounter() - clock_base_) * clock_scale_; }

bool GameApp::create_window(const Settings& settings, int width, int height) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

  const Uint32 mode = settings.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_RESIZABLE;
  const Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | mode;
  window_ = SDL_CreateWindow("Open It UP: 1st Preservation", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
  if (window_ == nullptr) {
    // Some systems have no multisampling; the game looks the same otherwise.
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
    window_ = SDL_CreateWindow("Open It UP: 1st Preservation", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
  }
  if (window_ == nullptr || SDL_GL_CreateContext(window_) == nullptr) return false;
#if defined(_WIN32)
  if (!load_gl_functions()) return false;
#endif
  SDL_DisplayMode display{};
  const bool known_display = SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(window_), &display) == 0;
  SDL_GL_SetSwapInterval(known_display ? piu::display_swap_interval(display.refresh_rate) : 1);
  if (settings.fullscreen) SDL_ShowCursor(SDL_DISABLE);
  focus_deadline_ = now() + kFocusClaimMs;
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  return true;
}

void GameApp::restore_machine_settings() {
  if (!settings_.machine) return;
  const auto& v = *settings_.machine;
  piu_settings_load(v[0], v[1], v[2], v[3], v[4], v[5]);
}

void GameApp::apply_machine_settings() { piu_coin_configure(piu_settings_get(5), piu_settings_get(0) == 1 ? 1 : 0); }

void GameApp::save_machine_settings() {
  std::array<int, 6> values{};
  for (int i = 0; i < 6; ++i) values[static_cast<std::size_t>(i)] = piu_settings_get(i);
  settings_.machine = values;
  save_settings(settings_);
}

void GameApp::provide_texture(const std::string& key) {
  const auto* path = library_.find("png:" + key);
  const std::optional<std::string> bytes = path != nullptr ? read_file(*path) : std::nullopt;
  const std::optional<extract::Image> image =
      bytes ? extract::decode_stored_png(reinterpret_cast<const std::uint8_t*>(bytes->data()), bytes->size()) : std::nullopt;
  if (!image) {
    piu_missing_texture(key.c_str());
    return;
  }
  piu_begin_texture(key.c_str(), image->width, image->height);
  const GLenum format = image->channels == 4 ? GL_RGBA : GL_RGB;
  glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(format), image->width, image->height, 0, format, GL_UNSIGNED_BYTE, image->pixels.data());
}

void GameApp::provide_sprite(const std::string& key) {
  const auto* path = library_.find("spr:" + key);
  const std::optional<std::string> text = path != nullptr ? read_file(*path) : std::nullopt;
  if (!text) {
    piu_missing_sprite(key.c_str());
    return;
  }
  piu_provide_sprite(key.c_str(), text->data(), static_cast<int>(text->size()));
}

void GameApp::provide_assets() {
  for (;;) {
    const char* pointer = piu_next_asset();
    if (pointer == nullptr) return;
    const std::string request = pointer;
    if (request.rfind("spr:", 0) == 0) provide_sprite(request.substr(4));
    else provide_texture(request.substr(4));
  }
}

bool GameApp::play_track(int track, bool loop) {
  const auto* path = library_.find(track_key(track));
  return path != nullptr && audio_.play_music(*path, loop);
}

std::string GameApp::read_or(const char* key, const char* fallback) {
  const auto* path = library_.find(key);
  const std::optional<std::string> text = path != nullptr ? read_file(*path) : std::nullopt;
  return text ? *text : fallback;
}

bool GameApp::prepare_flow() {
  const std::string keys[] = {"cfg:stage", "bga:mode", "bga:music", "bga:cdsel", "bga:help"};
  std::string data[5];
  for (int i = 0; i < 5; ++i) {
    const auto* path = library_.find(keys[i]);
    const std::optional<std::string> text = path != nullptr ? read_file(*path) : std::nullopt;
    if (!text) return false;
    data[i] = *text;
  }
  const auto size = [](const std::string& s) { return static_cast<int>(s.size()); };
  return piu_flow_prepare(data[0].data(), size(data[0]), data[1].data(), size(data[1]), data[2].data(), size(data[2]),
                          data[3].data(), size(data[3]), data[4].data(), size(data[4])) != 0;
}

bool GameApp::start_attract() {
  if (in_attract_) return true;
  const std::string andamiro = read_or("bga:andamiro", "");
  const std::string start = read_or("bga:start", "");
  if (andamiro.empty() || start.empty()) return false;
  if (piu_attract_prepare(andamiro.data(), static_cast<int>(andamiro.size()), start.data(), static_cast<int>(start.size())) == 0) return false;
  if (!prepare_flow()) return false;
  provide_assets();
  piu_attract_begin(now());
  in_attract_ = true;
  return true;
}

void GameApp::back_to_attract() {
  flow_ = Flow::kNone;
  playing_ = false;
  in_results_ = false;
  audio_.stop_music();
  piu_flow_to_attract(now());
}

void GameApp::start_credit(int players) {
  piu_flow_new_credit(players);
  // SHOW HELP is compared with the machine difficulty.
  const int help = piu_settings_get(3);
  const int difficulty = piu_settings_get(1);
  if (help == 0 || (help == 1 && difficulty == 0) || (help == 2 && difficulty <= 1)) {
    flow_ = Flow::kHelp;
    piu_help_begin(now());
    play_track(kHelpTrack);
  } else {
    begin_select();
  }
}

void GameApp::begin_select() {
  flow_ = Flow::kSelect;
  audio_.stop_music();
  piu_select_begin(now());
}

void GameApp::handle_select_events() {
  for (;;) {
    const int event = piu_select_event();
    if (event == 0) return;
    if (event >= 1 && event <= 3) play_track(piu_select_track());
    else if (event == 4) audio_.stop_music();
    else if (event == 5) {
      piu_select_request_title();
      provide_assets();
      if (flow_ == Flow::kSelect) piu_select_title_ready();
    } else if (event == 6) {
      play_selected();
      return;
    }
  }
}

bool GameApp::prepare_song(const std::string& chart, const std::string& bga) {
  const std::string gameview = read_or("bga:gameview", "{}");
  return piu_prepare(chart.data(), static_cast<int>(chart.size()), bga.data(), static_cast<int>(bga.size()), gameview.data(),
                     static_cast<int>(gameview.size())) != 0;
}

void GameApp::play_selected() {
  flow_ = Flow::kGame;
  const std::string chart_name = base_name(piu_select_text(0));
  const std::string bga_name = base_name(piu_select_text(1));
  const auto* chart_path = library_.find_chart(chart_name);
  const int cd = piu_select_cd();
  const int offset = piu_select_cd_offset();
  const int mode = piu_select_mode();
  const int players = piu_select_players();
  const int level = piu_select_level();
  const int stage = piu_flow_stage();
  const int flag = mode == 2 ? 4 : mode == 3 ? 0x800 : mode == 4 ? 0x400 : 0;
  const int p1 = piu_select_word(0);
  const int p2 = piu_select_word(1) | flag | players;
  piu_select_end();

  const bool is_remix = bga_name.rfind("remix", 0) == 0;
  const bool is_battle = bga_name.rfind("bat", 0) == 0;
  const std::string bga_key = "bga:" + bga_name;
  if (chart_path == nullptr || (!is_remix && !is_battle && !library_.has(bga_key))) {
    back_to_attract();
    return;
  }
  const std::optional<std::string> chart = read_file(*chart_path);
  const std::string bga = is_remix || is_battle ? R"({"layers":[]})" : read_or(bga_key.c_str(), "");
  if (!chart || !prepare_song(*chart, bga)) {
    back_to_attract();
    return;
  }
  int use_flight = 0;
  if (is_remix) {
    if (const auto* scene_path = library_.find("scene:scene")) {
      if (const std::optional<std::string> scene = read_file(*scene_path)) {
        use_flight = piu_load_flight(scene->data(), static_cast<int>(scene->size())) != 0 ? 1 : 0;
      }
    }
  }
  provide_assets();

  const bool has_sound = play_track(cd + offset);
  has_audio_ = has_sound;
  const int machine = piu_settings_get(1);
  const std::string source = chart_name + ".stf";
  const int margin = machine == 0 ? 0 : std::min(5, piu_chart_margin(source.c_str()) + (machine == 2 ? 1 : 0));
  last_stage_ = stage;
  const int double_play = mode == 2 ? 1 : 0;
  const int right_only = players == 2 && double_play == 0 ? 1 : 0;
  const double time = now();
  if (piu_begin(p1, p2, margin, double_play, stage, level, use_flight, static_cast<unsigned>(std::time(nullptr)), 0, right_only, time,
                has_sound ? std::max(0.0, audio_.music_ms(time)) : 0.0) != 0) {
    playing_ = true;
  } else {
    back_to_attract();
  }
}

void GameApp::start_demo(int index) {
  const DemoSong& demo = kDemoSongs[std::clamp(index, 0, 9)];
  const auto* chart_path = library_.find_chart(demo.chart);
  std::string bga_key;
  for (const SongTrack& track : kSongTracks) {
    if (track.id == demo.cd) bga_key = std::string("bga:") + track.name;
  }
  const auto* bga_path = bga_key.empty() ? nullptr : library_.find(bga_key);
  const std::optional<std::string> chart = chart_path != nullptr ? read_file(*chart_path) : std::nullopt;
  const std::optional<std::string> bga = bga_path != nullptr ? read_file(*bga_path) : std::nullopt;
  if (!chart || !bga || !prepare_song(*chart, *bga)) {
    piu_attract_demo_done(now());
    return;
  }
  provide_assets();
  if (!in_attract_) return;
  const bool has_sound = piu_settings_get(4) == 1 && play_track(demo.cd + kTrackOffset);
  has_audio_ = has_sound;
  const int margin = piu_settings_get(1) == 2 ? 1 : 0;
  const double time = now();
  if (piu_begin(0, 0, margin, 0, 1, 0, 0, static_cast<unsigned>(std::time(nullptr)), 1, 0, time,
                has_sound ? std::max(0.0, audio_.music_ms(time)) : 0.0) != 0) {
    playing_ = true;
    in_demo_ = true;
  } else {
    piu_attract_demo_done(now());
  }
}

void GameApp::end_demo() {
  in_demo_ = false;
  playing_ = false;
  audio_.stop_music();
  piu_attract_demo_done(now());
}

void GameApp::handle_attract_events() {
  for (;;) {
    const int event = piu_attract_event();
    if (event == 0) return;
    if (event == 1) play_track(kIntroTrack);
    else if (event == 2) audio_.stop_music();
    else if (event == 3) start_demo(piu_attract_demo_index());
    else if (event == 4) {
      start_credit(piu_attract_players());
      return;
    }
  }
}

void GameApp::open_service() {
  flow_ = Flow::kNone;
  if (in_service_) return;
  if (playing_ || in_results_) {
    playing_ = in_results_ = false;
    audio_.pause_music();
    piu_stop();
    held_.clear();
  }
  piu_service_request();
  provide_assets();
  piu_service_open();
  in_service_ = true;
}

void GameApp::close_service() {
  in_service_ = false;
  held_.clear();
  audio_.pause_music();
  audio_.stop_sounds();
  save_machine_settings();
  apply_machine_settings();
  // Leaving the menu in EVENT mode pays two credits.
  if (piu_settings_get(0) == 1) piu_coin_add(2);
  // Leaving the menu restarts the machine at the Andamiro screen.
  if (!start_attract()) running_ = false;
}

void GameApp::end_song() {
  if (!playing_) return;
  audio_.pause_music();
  playing_ = false;
  held_.clear();
  if (piu_show_results(last_stage_, now()) != 0) in_results_ = true;
  else back_to_attract();
}

void GameApp::finish_results() {
  in_results_ = false;
  if (piu_flow_game_over() != 0) back_to_attract();
  else begin_select();
}

void GameApp::release_held() {
  const double time = now();
  for (int panel : held_) piu_input(panel, 0, time);
  held_.clear();
}

void GameApp::handle_key(const SDL_KeyboardEvent& key, bool down) {
  const SDL_Scancode code = key.keysym.scancode;
  const double time = static_cast<double>(key.timestamp);
  const int action = keys_.action_for(key_code(code));
  if (action == kKeyTest && down && key.repeat == 0 && !in_service_) {
    if (in_attract_) {
      in_attract_ = false;
      in_demo_ = false;
      audio_.stop_music();
    }
    open_service();
    return;
  }
  if (in_service_ && (action == kKeyTest || action == kKeyService || action == kKeyClear)) {
    if (key.repeat == 0) piu_service_button(action == kKeyTest ? 0 : action == kKeyService ? 1 : 2, down ? 1 : 0);
    return;
  }
  if (action == kKeyCoin1 || action == kKeyCoin2) {
    if (down && key.repeat == 0) piu_coin_insert();
    return;
  }
  if (code == SDL_SCANCODE_ESCAPE && down && in_attract_) {
    exit_ = GameExit::kBackToLauncher;
    running_ = false;
    return;
  }
  const int panel = action >= 0 && action < 10 ? action : -1;
  if (panel < 0 || (!playing_ && !in_results_ && !in_service_ && !in_attract_)) return;
  if (key.repeat != 0) return;
  if (down) held_.insert(panel);
  else held_.erase(panel);
  piu_input(panel, down ? 1 : 0, time);
}

void GameApp::frame() {
  const double time = now();
  int width = 1;
  int height = 1;
  SDL_GL_GetDrawableSize(window_, &width, &height);
  const double music = playing_ && has_audio_ ? audio_.music_ms(time) : -1.0;
  piu_frame(time, music, std::max(1, width), std::max(1, height));
  for (int code = piu_next_sound(); code >= 0; code = piu_next_sound()) audio_.play_sound(code);
  if (playing_ && piu_take_audio_stop() != 0) audio_.pause_music();
  if (in_demo_) {
    if (piu_finished() != 0) end_demo();
  } else if (playing_ && piu_finished() != 0) {
    end_song();
  }
  if (in_attract_ && flow_ == Flow::kNone) handle_attract_events();
  if (flow_ == Flow::kHelp && piu_help_finished() != 0) begin_select();
  if (flow_ == Flow::kSelect) handle_select_events();
  if (in_results_ && piu_results_finished() != 0) finish_results();
  if (in_service_) {
    const int track = piu_service_track();
    if (track == 0) audio_.pause_music();
    else if (track > 0) play_track(track);
    if (piu_service_finished() != 0) close_service();
  }
}

void GameApp::claim_focus() {
  const double time = now();
  if (time > focus_deadline_ || time < next_focus_try_) return;
  next_focus_try_ = time + kFocusRetryMs;
  if ((SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0) return;
  bring_application_to_front();
  SDL_RaiseWindow(window_);
}

bool GameApp::start_video() {
  SDL_SetMainReady();
  SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
  SDL_SetHint(SDL_HINT_FORCE_RAISEWINDOW, "1");
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) return false;
  const auto frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  // Same origin as the timestamps SDL puts on key events.
  clock_base_ = SDL_GetPerformanceCounter() - static_cast<Uint64>(static_cast<double>(SDL_GetTicks64()) * frequency / 1000.0);
  clock_scale_ = 1000.0 / frequency;
  return true;
}

GameExit GameApp::run_key_map() {
  if (!start_video()) return GameExit::kFailed;
  settings_ = load_settings();
  Settings windowed = settings_;
  windowed.fullscreen = false;
  if (!create_window(windowed, 640, 480) || piu_engine_start() == 0) {
    SDL_Quit();
    return GameExit::kFailed;
  }
  piu_keymap_open(settings_.keys.c_str(), settings_.language == "es" ? 1 : 0);

  while (piu_keymap_finished() == 0) {
    claim_focus();
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) {
        SDL_Quit();
        return GameExit::kQuit;
      }
      if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        const std::string code = event.key.keysym.scancode == SDL_SCANCODE_ESCAPE ? "Escape" : key_code(event.key.keysym.scancode);
        if (!code.empty()) piu_keymap_key(code.c_str());
      } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        int width = 1;
        int height = 1;
        SDL_GetWindowSize(window_, &width, &height);
        piu_keymap_click(event.button.x, event.button.y, width, height);
      }
    }
    if (piu_frame_due(now()) == 0) {
      SDL_Delay(1);
      continue;
    }
    int width = 1;
    int height = 1;
    SDL_GL_GetDrawableSize(window_, &width, &height);
    piu_frame(now(), -1.0, std::max(1, width), std::max(1, height));
    SDL_GL_SwapWindow(window_);
  }
  if (piu_keymap_finished() == 1) {
    settings_.keys = piu_keymap_result();
    save_settings(settings_);
  }
  SDL_Quit();
  return GameExit::kBackToLauncher;
}

GameExit GameApp::run() {
  if (!start_video()) return GameExit::kFailed;

  const auto fail = [&](const char* reason) {
    std::fprintf(stderr, "%s %s\n", reason, SDL_GetError());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Open It UP", reason, window_);
    audio_.close();
    SDL_Quit();
    return GameExit::kFailed;
  };
  settings_ = load_settings();
  if (!read_library_state().ready || !library_.open()) return fail("The game has not been extracted yet.");
  if (!create_window(settings_, settings_.wide ? 1280 : 960, 720)) return fail("The game window could not be opened.");
  if (piu_engine_start() == 0) return fail("OpenGL could not be started.");
  audio_.open();
  for (int i = 0; i < 256; ++i) {
    if (const auto* path = library_.find("sfx:" + std::to_string(i))) audio_.load_sound(i, *path);
  }
  keys_ = KeyMap::parse(settings_.keys).value_or(KeyMap());
  restore_machine_settings();
  apply_machine_settings();
  piu_set_wide(settings_.wide ? 1 : 0);

  if (!start_attract()) return fail("The extraction is incomplete. Choose the CHD again.");

  while (running_) {
    claim_focus();
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_QUIT) running_ = false;
      else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) handle_key(event.key, event.type == SDL_KEYDOWN);
      else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) release_held();
    }
    if (piu_frame_due(now()) == 0) {
      SDL_Delay(1);
      continue;
    }
    const Uint64 before = SDL_GetPerformanceCounter();
    frame();
    SDL_GL_SwapWindow(window_);
    // A hidden window does not wait for the display; do not spin.
    if (static_cast<double>(SDL_GetPerformanceCounter() - before) * clock_scale_ < 2.0) SDL_Delay(1);
  }

  audio_.close();
  SDL_Quit();
  return exit_;
}

}  // namespace

GameExit run_game() {
  GameApp app;
  return app.run();
}

GameExit run_key_map() {
  GameApp app;
  return app.run_key_map();
}

}  // namespace piu::desktop
