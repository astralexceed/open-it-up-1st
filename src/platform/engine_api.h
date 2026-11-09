// The engine as plain C functions, for the web page and the desktop program.

#pragma once

// The engine as plain C functions: the web page calls them from JavaScript,
// and the desktop program from C++.
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#define PIU_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define PIU_EXPORT
#endif

extern "C" {

PIU_EXPORT int piu_engine_start();
PIU_EXPORT void* piu_alloc(int size);
PIU_EXPORT void piu_free(void* data);
PIU_EXPORT int piu_prepare(const char* chart, int chart_size, const char* bga, int bga_size, const char* gameview, int gameview_size);
PIU_EXPORT int piu_load_flight(const char* json, int size);
PIU_EXPORT const char* piu_next_asset();
PIU_EXPORT void piu_provide_sprite(const char* key, const char* text, int size);
PIU_EXPORT void piu_missing_sprite(const char* key);
PIU_EXPORT void piu_missing_texture(const char* key);
PIU_EXPORT void piu_begin_texture(const char* key, int width, int height);
PIU_EXPORT void piu_coin_configure(int per_credit, int free_play);
PIU_EXPORT void piu_coin_add(int credits);
PIU_EXPORT void piu_coin_reset();
PIU_EXPORT void piu_coin_insert();
PIU_EXPORT int piu_coin_can_start(int credits);
PIU_EXPORT int piu_coin_consume();
PIU_EXPORT const char* piu_coin_text();
PIU_EXPORT int piu_begin(int p1_word, int p2_word, int margin, int double_play, int stage, int life_level, int use_flight, unsigned seed, int demo, int right_only, double now_ms, double song_ms);
PIU_EXPORT const char* piu_missing();
PIU_EXPORT void piu_stop();
PIU_EXPORT int piu_show_results(int stage, double now_ms);
PIU_EXPORT int piu_results_finished();
PIU_EXPORT void piu_input(int panel, int down, double time_ms);
PIU_EXPORT const char* piu_keymap_normalize(const char* text);
PIU_EXPORT void piu_keymap_open(const char* text, int spanish);
PIU_EXPORT void piu_keymap_key(const char* code);
PIU_EXPORT void piu_keymap_click(double px, double py, int width, int height);
PIU_EXPORT int piu_keymap_finished();
PIU_EXPORT const char* piu_keymap_result();
PIU_EXPORT void piu_keymap_close();
PIU_EXPORT int piu_frame_due(double now_ms);
PIU_EXPORT void piu_frame(double now_ms, double audio_ms, int width, int height);
PIU_EXPORT void piu_settings_load(int mode, int difficulty, int stage_break, int help, int demo, int coins);
PIU_EXPORT int piu_settings_get(int index);
PIU_EXPORT void piu_service_request();
PIU_EXPORT void piu_service_open();
PIU_EXPORT void piu_service_button(int kind, int down);
PIU_EXPORT int piu_service_track();
PIU_EXPORT int piu_service_finished();
PIU_EXPORT int piu_attract_prepare(const char* andamiro, int andamiro_size, const char* start, int start_size);
PIU_EXPORT void piu_attract_begin(double now_ms);
PIU_EXPORT int piu_attract_event();
PIU_EXPORT int piu_attract_demo_index();
PIU_EXPORT int piu_attract_players();
PIU_EXPORT void piu_attract_demo_done(double now_ms);
PIU_EXPORT int piu_flow_prepare(const char* cfg, int cfg_size, const char* mode, int mode_size, const char* music, int music_size, const char* cds, int cds_size, const char* help, int help_size);
PIU_EXPORT void piu_flow_new_credit(int players);
PIU_EXPORT int piu_flow_stage();
PIU_EXPORT int piu_flow_game_over();
PIU_EXPORT void piu_flow_to_attract(double now_ms);
PIU_EXPORT void piu_help_begin(double now_ms);
PIU_EXPORT int piu_help_finished();
PIU_EXPORT void piu_select_begin(double now_ms);
PIU_EXPORT int piu_select_event();
PIU_EXPORT int piu_select_track();
PIU_EXPORT void piu_select_request_title();
PIU_EXPORT void piu_select_title_ready();
PIU_EXPORT const char* piu_select_text(int which);
PIU_EXPORT int piu_select_cd();
PIU_EXPORT int piu_select_level();
PIU_EXPORT int piu_select_mode();
PIU_EXPORT int piu_select_players();
PIU_EXPORT int piu_select_word(int player);
PIU_EXPORT int piu_select_cd_offset();
PIU_EXPORT void piu_select_end();
PIU_EXPORT void piu_set_wide(int wide);
PIU_EXPORT int piu_next_sound();
PIU_EXPORT int piu_chart_margin(const char* name);
PIU_EXPORT int piu_take_audio_stop();
PIU_EXPORT int piu_finished();
PIU_EXPORT int piu_uses_player(int player);
PIU_EXPORT int piu_points(int player);
PIU_EXPORT int piu_combo(int player);
PIU_EXPORT int piu_max_combo(int player);
PIU_EXPORT int piu_tally(int player, int index);
PIU_EXPORT int piu_last_judgment(int player);
PIU_EXPORT int piu_grade(int player);

}  // extern "C"
