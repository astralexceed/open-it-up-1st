// The audio calibration: the beat, the measure of the taps, and its screen.

#include "core/calibration_screen.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/ui_atlas.h"

namespace piu {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kTrimmedTaps = 2;
constexpr double kMaxSpreadMs = 35.0;

constexpr int kRate = 44100;
constexpr int kBeats = 40;
constexpr double kClickHz = 1500.0;
constexpr double kClickDecayMs = 18.0;
constexpr double kClickLengthMs = 90.0;
constexpr double kClickVolume = 0.7;

constexpr ui::Box kResetArea = {32.0f, 412.0f, 150.0f, 36.0f};
constexpr ui::Box kRepeatArea = {245.0f, 412.0f, 150.0f, 36.0f};
constexpr ui::Box kSaveArea = {458.0f, 412.0f, 150.0f, 36.0f};

void put_u32(std::string& out, std::uint32_t value) {
  for (int shift = 0; shift < 32; shift += 8) out.push_back(static_cast<char>(value >> shift & 0xFF));
}

void put_u16(std::string& out, std::uint16_t value) {
  out.push_back(static_cast<char>(value & 0xFF));
  out.push_back(static_cast<char>(value >> 8 & 0xFF));
}

std::string signed_ms(int value) {
  return (value > 0 ? "+" : "") + std::to_string(value) + " ms";
}

}  // namespace

TapMeasure measure_taps(const std::vector<double>& taps) {
  if (taps.size() <= static_cast<std::size_t>(kTrimmedTaps) * 2) return {};
  // The beat repeats, so taps are averaged around a circle: 590 and 10 are 20 ms apart.
  double sine = 0.0;
  double cosine = 0.0;
  for (const double tap : taps) {
    sine += std::sin(tap * 2.0 * kPi / kCalibrationBeatMs);
    cosine += std::cos(tap * 2.0 * kPi / kCalibrationBeatMs);
  }
  const double center = std::atan2(sine, cosine) * kCalibrationBeatMs / (2.0 * kPi);

  std::vector<double> around;
  around.reserve(taps.size());
  for (const double tap : taps) around.push_back(std::remainder(tap - center, kCalibrationBeatMs));
  std::sort(around.begin(), around.end());
  around.erase(around.begin(), around.begin() + kTrimmedTaps);
  around.erase(around.end() - kTrimmedTaps, around.end());

  double sum = 0.0;
  for (const double value : around) sum += value;
  const double mean = sum / static_cast<double>(around.size());
  double squares = 0.0;
  for (const double value : around) squares += (value - mean) * (value - mean);
  const double spread = std::sqrt(squares / static_cast<double>(around.size()));

  const int offset = static_cast<int>(std::lround(center + mean));
  return {std::clamp(offset, kAudioOffsetMinMs, kAudioOffsetMaxMs), spread <= kMaxSpreadMs};
}

std::string make_click_track() {
  const auto beat_frames = static_cast<std::uint32_t>(kCalibrationBeatMs * kRate / 1000.0);
  const std::uint32_t frames = beat_frames * kBeats;
  std::vector<std::int16_t> pcm(frames, 0);
  const auto click_frames = static_cast<std::uint32_t>(kClickLengthMs * kRate / 1000.0);
  for (std::uint32_t beat = 0; beat < kBeats; ++beat) {
    for (std::uint32_t i = 0; i < click_frames; ++i) {
      const double seconds = static_cast<double>(i) / kRate;
      const double value = std::sin(2.0 * kPi * kClickHz * seconds) * std::exp(-seconds * 1000.0 / kClickDecayMs);
      pcm[beat * beat_frames + i] = static_cast<std::int16_t>(value * kClickVolume * 32767.0);
    }
  }

  const std::uint32_t bytes = frames * 2;
  std::string wav;
  wav.reserve(44 + bytes);
  wav += "RIFF";
  put_u32(wav, 36 + bytes);
  wav += "WAVEfmt ";
  put_u32(wav, 16);
  put_u16(wav, 1);
  put_u16(wav, 1);
  put_u32(wav, kRate);
  put_u32(wav, kRate * 2);
  put_u16(wav, 2);
  put_u16(wav, 16);
  wav += "data";
  put_u32(wav, bytes);
  wav.resize(44 + bytes);
  std::memcpy(&wav[44], pcm.data(), bytes);
  return wav;
}

CalibrationScreen::CalibrationScreen(int current_ms, bool spanish, const Texture& atlas)
    : saved_ms_(current_ms), spanish_(spanish), atlas_(atlas) {}

void CalibrationScreen::tap(double song_ms) {
  if (measured_ || song_ms < 0.0) return;
  taps_.push_back(std::fmod(song_ms, kCalibrationBeatMs));
  if (static_cast<int>(taps_.size()) < kCalibrationWarmupTaps + kCalibrationMeasuredTaps) return;
  measure_ = measure_taps({taps_.begin() + kCalibrationWarmupTaps, taps_.end()});
  measured_ = true;
}

void CalibrationScreen::click(float x, float y) {
  if (kResetArea.contains(x, y)) {
    saved_ms_ = 0;
    result_ = Result::kSaved;
  } else if (measured_ && kRepeatArea.contains(x, y)) {
    taps_.clear();
    measured_ = false;
  } else if (measured_ && measure_.steady && kSaveArea.contains(x, y)) {
    saved_ms_ = measure_.offset_ms;
    result_ = Result::kSaved;
  }
}

void CalibrationScreen::draw(DrawList& out) const {
  ui::fill(out, atlas_, {0.0f, 0.0f, 640.0f, 480.0f}, ui::kPanel);
  ui::text(out, atlas_, 32.0f, 36.0f, 22.0f, text("Calibrate audio", "Calibrar audio"), ui::kInk);
  ui::fill(out, atlas_, {32.0f, 60.0f, 576.0f, 1.0f}, ui::kLine);

  const auto center = [&](float y, float height, float size, const std::string& value, Color color) {
    ui::text_centered(out, atlas_, {0.0f, y, 640.0f, height}, size, value, color);
  };
  center(84.0f, 18.0f, 15.0f, text("Listen to the beat and press any key on each one", "Escucha el beat y presiona una tecla en cada uno"), ui::kInk);
  center(110.0f, 16.0f, 13.0f, text("Use your ears, not your eyes", "Gu\xC3\xAD" "ate por el o\xC3\xAD" "do, no por la vista"), ui::kSoft);

  const int total = kCalibrationWarmupTaps + kCalibrationMeasuredTaps;
  constexpr float kPip = 20.0f;
  constexpr float kStep = 27.0f;
  const float left = (640.0f - kStep * static_cast<float>(total) + (kStep - kPip)) / 2.0f;
  for (int i = 0; i < total; ++i) {
    const ui::Box pip = {left + kStep * static_cast<float>(i), 150.0f, kPip, kPip};
    if (i < static_cast<int>(taps_.size())) ui::rounded(out, atlas_, pip, 3.0f, i < kCalibrationWarmupTaps ? ui::kSoft : ui::kBlue);
    else ui::panel(out, atlas_, pip, 3.0f, ui::kRaised, ui::kLine);
  }

  const ui::Box card = {120.0f, 194.0f, 400.0f, 124.0f};
  ui::panel(out, atlas_, card, 6.0f, ui::kRaised, ui::kLine);
  const int count = static_cast<int>(taps_.size());
  if (!measured_) {
    if (count < kCalibrationWarmupTaps) center(card.y, card.h, 18.0f, text("Get the rhythm", "Agarra el ritmo"), ui::kInk);
    else center(card.y, card.h, 40.0f, std::to_string(count - kCalibrationWarmupTaps) + " / " + std::to_string(kCalibrationMeasuredTaps), ui::kInk);
  } else if (measure_.steady) {
    center(card.y + 16.0f, 60.0f, 44.0f, signed_ms(measure_.offset_ms), ui::kSky);
    center(card.y + 86.0f, 18.0f, 13.0f, text("Press Save to use it", "Presiona Guardar para usarlo"), ui::kSoft);
  } else {
    center(card.y + 32.0f, 28.0f, 18.0f, text("Taps too uneven", "Muy irregular"), ui::kWarn);
    center(card.y + 70.0f, 18.0f, 13.0f, text("Press Repeat and try again", "Presiona Repetir e intenta otra vez"), ui::kSoft);
  }

  center(336.0f, 18.0f, 13.0f, std::string(text("Saved now: ", "Guardado: ")) + signed_ms(saved_ms_), ui::kSoft);
  center(362.0f, 18.0f, 13.0f, text("Esc: cancel", "Esc: cancelar"), ui::kSoft);

  ui::button(out, atlas_, kResetArea, text("Set to 0", "Poner en 0"), false, true);
  if (measured_) ui::button(out, atlas_, kRepeatArea, text("Repeat", "Repetir"), false, true);
  ui::button(out, atlas_, kSaveArea, text("Save", "Guardar"), true, measured_ && measure_.steady);
}

}  // namespace piu
