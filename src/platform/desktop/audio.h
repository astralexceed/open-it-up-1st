// Music and sound effects on desktop, mixed through SDL.

#pragma once

#include <SDL.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace piu::desktop {

// Plays the CD music and the sound effects, replacing the browser's audio.
class Audio {
 public:
  bool open();
  void close();

  // Loads one effect from its WAV file.
  void load_sound(int index, const std::filesystem::path& path);
  // One sound request: sound | channel << 8 | loop << 16. A new sound replaces its channel's.
  void play_sound(int code);
  void stop_sounds();

  bool play_music(const std::filesystem::path& path, bool loop = false);
  // The same, from a WAV file already in memory.
  bool play_music_data(const std::string& wav, bool loop);
  void pause_music();
  void stop_music();
  // Song position in milliseconds, or negative if no music is playing.
  double music_ms(double now_ms);

 private:
  struct Sample {
    std::vector<std::int16_t> pcm;
    int channels = 1;
    int rate = 44100;
    std::size_t frames() const { return pcm.size() / static_cast<std::size_t>(channels); }
  };
  struct Voice {
    std::shared_ptr<const Sample> sample;
    double position = 0.0;
    bool loop = false;
    bool playing = false;
  };

  static void fill(void* self, Uint8* out, int bytes);
  void mix(std::int16_t* out, int frames);
  bool start_music(std::shared_ptr<const Sample> sample, bool loop);
  void mix_voice(Voice& voice, std::int32_t* out, int frames);

  static constexpr int kVoices = 16;
  static constexpr int kRate = 44100;
  static constexpr int kBufferFrames = 512;

  SDL_AudioDeviceID device_ = 0;
  std::array<Voice, kVoices> voices_;
  Voice music_;
  std::vector<std::shared_ptr<const Sample>> sounds_;
  std::vector<std::int32_t> accumulator_;
  double music_at_callback_ms_ = 0.0;
  double callback_ms_ = 0.0;
};

}  // namespace piu::desktop
