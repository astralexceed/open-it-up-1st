// Sound effect numbers and the queue a screen uses to request them.

#pragma once

#include <optional>
#include <vector>

namespace piu {

// Sounds of the sample chip.
namespace sfx {
inline constexpr int kCoin = 0x00;
inline constexpr int kCode = 0x02;
inline constexpr int kMove = 0x03;
inline constexpr int kConfirm = 0x04;
inline constexpr int kTick = 0x06;
inline constexpr int kFailEnding = 0x07;
inline constexpr int kStart = 0x08;
inline constexpr int kGrade = 0x09;
inline constexpr int kLetterA = 0x0a;   // 0x0a to 0x0e: A, B, C, D, F.
inline constexpr int kCredit = 0x15;
inline constexpr int kVoiceA = 0x17;    // 0x17 to 0x1b: A, B, C, D, F.
inline constexpr int kStop = 0xFF;      // Not a sound: silences its channel.
}  // namespace sfx

struct SoundCall {
  int sound = 0;
  int channel = 0;
  bool loop = false;
};

// Calls a screen made since the platform last asked.
class SoundQueue {
 public:
  void play(int sound, int channel, bool loop = false) { calls_.push_back({sound, channel, loop}); }
  void stop(int channel) { calls_.push_back({sfx::kStop, channel, false}); }

  std::optional<SoundCall> take() {
    if (next_ >= calls_.size()) {
      calls_.clear();
      next_ = 0;
      return std::nullopt;
    }
    return calls_[next_++];
  }

 private:
  std::vector<SoundCall> calls_;
  std::size_t next_ = 0;
};

}  // namespace piu
