// WAV reading, resampling and mixing in the SDL audio callback.

#include "platform/desktop/audio.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "platform/desktop/library.h"

namespace piu::desktop {
namespace {

constexpr int kSoundStop = 0xFF;

double clock_ms() {
  return static_cast<double>(SDL_GetPerformanceCounter()) * 1000.0 / static_cast<double>(SDL_GetPerformanceFrequency());
}

std::uint32_t le32(const char* p) {
  const auto* b = reinterpret_cast<const std::uint8_t*>(p);
  return static_cast<std::uint32_t>(b[0]) | static_cast<std::uint32_t>(b[1]) << 8 |
         static_cast<std::uint32_t>(b[2]) << 16 | static_cast<std::uint32_t>(b[3]) << 24;
}

std::uint16_t le16(const char* p) {
  const auto* b = reinterpret_cast<const std::uint8_t*>(p);
  return static_cast<std::uint16_t>(b[0] | b[1] << 8);
}

}  // namespace

bool Audio::open() {
  SDL_AudioSpec want{};
  want.freq = kRate;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = kBufferFrames;
  want.callback = &Audio::fill;
  want.userdata = this;
  SDL_AudioSpec have{};
  device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (device_ == 0) return false;
  SDL_PauseAudioDevice(device_, 0);
  return true;
}

void Audio::close() {
  if (device_ != 0) SDL_CloseAudioDevice(device_);
  device_ = 0;
}

namespace {

template <typename Sample>
std::shared_ptr<const Sample> read_wav(const std::filesystem::path& path) {
  const std::optional<std::string> file = read_file(path);
  if (!file || file->size() < 12 || file->compare(0, 4, "RIFF") != 0 || file->compare(8, 4, "WAVE") != 0) return nullptr;
  auto sample = std::make_shared<Sample>();
  bool valid_format = false;
  for (std::size_t at = 12; at + 8 <= file->size();) {
    const std::uint32_t size = le32(&(*file)[at + 4]);
    const std::size_t body = at + 8;
    const std::size_t available = std::min<std::size_t>(size, file->size() - body);
    if (file->compare(at, 4, "fmt ") == 0 && available >= 16) {
      sample->channels = le16(&(*file)[body + 2]);
      sample->rate = static_cast<int>(le32(&(*file)[body + 4]));
      valid_format = le16(&(*file)[body + 14]) == 16 && sample->channels >= 1 && sample->channels <= 2 && sample->rate > 0;
    } else if (file->compare(at, 4, "data") == 0) {
      sample->pcm.resize(available / 2);
      std::memcpy(sample->pcm.data(), &(*file)[body], sample->pcm.size() * 2);
    }
    at = body + size + (size & 1);
  }
  return valid_format && !sample->pcm.empty() ? sample : nullptr;
}

}  // namespace

void Audio::load_sound(int index, const std::filesystem::path& path) {
  if (index < 0) return;
  if (static_cast<std::size_t>(index) >= sounds_.size()) sounds_.resize(static_cast<std::size_t>(index) + 1);
  sounds_[static_cast<std::size_t>(index)] = read_wav<Sample>(path);
}

void Audio::play_sound(int code) {
  const int channel = code >> 8 & 0xFF;
  const int sound = code & 0xFF;
  if (channel >= kVoices) return;
  std::shared_ptr<const Sample> sample;
  if (sound != kSoundStop && static_cast<std::size_t>(sound) < sounds_.size()) sample = sounds_[static_cast<std::size_t>(sound)];
  if (sound != kSoundStop && !sample) return;
  Voice next;
  next.sample = std::move(sample);
  next.loop = (code >> 16 & 1) == 1;
  next.playing = sound != kSoundStop;
  SDL_LockAudioDevice(device_);
  std::swap(voices_[static_cast<std::size_t>(channel)], next);
  SDL_UnlockAudioDevice(device_);
}

void Audio::stop_sounds() {
  std::array<Voice, kVoices> silent;
  SDL_LockAudioDevice(device_);
  std::swap(voices_, silent);
  SDL_UnlockAudioDevice(device_);
}

bool Audio::play_music(const std::filesystem::path& path, bool loop) {
  std::shared_ptr<const Sample> sample = read_wav<Sample>(path);
  if (!sample) return false;
  Voice next;
  next.sample = std::move(sample);
  next.loop = loop;
  next.playing = true;
  SDL_LockAudioDevice(device_);
  std::swap(music_, next);
  music_at_callback_ms_ = 0.0;
  callback_ms_ = clock_ms();
  SDL_UnlockAudioDevice(device_);
  return true;
}

void Audio::pause_music() {
  SDL_LockAudioDevice(device_);
  music_.playing = false;
  SDL_UnlockAudioDevice(device_);
}

void Audio::stop_music() {
  Voice silent;
  SDL_LockAudioDevice(device_);
  std::swap(music_, silent);
  SDL_UnlockAudioDevice(device_);
}

double Audio::music_ms(double) {
  SDL_LockAudioDevice(device_);
  const bool playing = music_.playing && music_.sample != nullptr;
  const double base = music_at_callback_ms_;
  const double since = clock_ms() - callback_ms_;
  SDL_UnlockAudioDevice(device_);
  if (!playing) return -1.0;
  // Between callbacks the position runs on; never further than the audio already handed over.
  return base + std::clamp(since, 0.0, kBufferFrames * 2000.0 / kRate);
}

void Audio::fill(void* self, Uint8* out, int bytes) {
  static_cast<Audio*>(self)->mix(reinterpret_cast<std::int16_t*>(out), bytes / 4);
}

void Audio::mix_voice(Voice& voice, std::int32_t* out, int frames) {
  if (!voice.playing || !voice.sample) return;
  const Sample& sample = *voice.sample;
  const std::size_t total = sample.frames();
  const double step = static_cast<double>(sample.rate) / kRate;
  for (int i = 0; i < frames; ++i) {
    if (voice.position >= static_cast<double>(total)) {
      if (!voice.loop) {
        voice.playing = false;
        return;
      }
      voice.position -= static_cast<double>(total);
    }
    const auto index = static_cast<std::size_t>(voice.position);
    const double fraction = voice.position - static_cast<double>(index);
    const std::size_t next = index + 1 < total ? index + 1 : (voice.loop ? 0 : index);
    const auto c = static_cast<std::size_t>(sample.channels);
    for (std::size_t side = 0; side < 2; ++side) {
      const std::size_t source = c == 2 ? side : 0;
      const double a = sample.pcm[index * c + source];
      const double b = sample.pcm[next * c + source];
      out[static_cast<std::size_t>(i) * 2 + side] += static_cast<std::int32_t>(a + (b - a) * fraction);
    }
    voice.position += step;
  }
}

void Audio::mix(std::int16_t* out, int frames) {
  if (music_.sample) {
    music_at_callback_ms_ = music_.position * 1000.0 / music_.sample->rate;
  }
  callback_ms_ = clock_ms();
  const auto samples = static_cast<std::size_t>(frames) * 2;
  if (accumulator_.size() < samples) accumulator_.resize(samples);
  std::fill_n(accumulator_.begin(), samples, 0);
  mix_voice(music_, accumulator_.data(), frames);
  for (Voice& voice : voices_) mix_voice(voice, accumulator_.data(), frames);
  for (std::size_t i = 0; i < samples; ++i) {
    out[i] = static_cast<std::int16_t>(std::clamp(accumulator_[i], -32768, 32767));
  }
}

}  // namespace piu::desktop
