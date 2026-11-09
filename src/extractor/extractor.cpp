// The list of extraction tasks, run one file at a time.

#include "extractor/extractor.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

namespace piu::extract {
namespace {

// The camera scale of the 3D flight, read from the game's own executable.
constexpr std::size_t kCameraScaleOffset = 0x121480 - 0xCC00;
constexpr int kModels = 19;
constexpr double kAudioWeightPerFrame = 1.0 / 150.0;
constexpr double kIntroWeight = 30.0;

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

std::string stem_of(const std::string& name) {
  const std::size_t dot = name.rfind('.');
  return lower(dot == std::string::npos ? name : name.substr(0, dot));
}

bool has_suffix(const std::string& name, const char* suffix) {
  const std::string lowered = lower(name);
  const std::string wanted = suffix;
  return lowered.size() >= wanted.size() && lowered.compare(lowered.size() - wanted.size(), wanted.size(), wanted) == 0;
}

Bytes to_bytes(const std::string& text) { return Bytes(text.begin(), text.end()); }

}  // namespace

bool Extractor::fail(const std::string& message) {
  error_ = message;
  return false;
}

void Extractor::add_task(double weight, std::function<bool(OutputFile&)> run) {
  total_weight_ += weight;
  tasks_.push_back({weight, std::move(run)});
}

bool Extractor::open(std::unique_ptr<ByteSource> source) {
  if (!cd_.open(std::move(source), error_)) return false;
  return prepare();
}

bool Extractor::prepare() {
  add_task(1.0, [this](OutputFile& out) {
    auto file = cd_.read_file("Piu/stage.cfg");
    if (!file) return fail("wrong-disc");
    out = {"stage.cfg", std::move(*file)};
    return true;
  });

  add_task(1.0, [this](OutputFile& out) {
    auto file = cd_.read_file("Piu/PIU.BIN");
    if (!file) return fail("wrong-disc");
    out = {"piu.bin", std::move(*file)};
    return true;
  });

  std::optional<Bytes> sprites = cd_.read_file("Piu/SPR.RES");
  std::optional<Bytes> textures = cd_.read_file("Piu/datas/PIU.DAT");
  if (!sprites || !textures) return fail("wrong-disc");
  sprites_ = std::make_shared<Bytes>(std::move(*sprites));
  textures_ = std::make_shared<Bytes>(std::move(*textures));
  const auto sprite_entries = parse_res(*sprites_);
  const auto texture_entries = parse_res(*textures_);
  if (!sprite_entries || !texture_entries) return fail("damaged");

  for (const ResEntry& entry : *sprite_entries) {
    add_task(0.2, [this, entry](OutputFile& out) {
      const auto begin = sprites_->begin() + static_cast<std::ptrdiff_t>(entry.offset);
      out = {"spr/" + lower(entry.name), Bytes(begin, begin + static_cast<std::ptrdiff_t>(entry.size))};
      return true;
    });
  }
  for (const ResEntry& entry : *texture_entries) {
    add_task(1.0, [this, entry](OutputFile& out) {
      const auto image = decode_ptx(textures_->data() + entry.offset, entry.size);
      if (!image) return fail("damaged");
      out = {"png/" + stem_of(entry.name) + ".png", encode_png(image->width, image->height, image->channels, image->pixels)};
      return true;
    });
  }

  add_task(kIntroWeight, [this](OutputFile& out) {
    auto file = cd_.read_file("Piu/INTRO.ANI");
    if (!file) return fail("wrong-disc");
    const auto sheet = intro_sheet(*file);
    if (!sheet) return fail("damaged");
    out = {"png/intro.png", encode_png(sheet->width, sheet->height, sheet->channels, sheet->pixels)};
    return true;
  });

  for (const IsoEntry& entry : cd_.list("Piu/datas/BGA")) {
    if (!has_suffix(entry.name, ".bga")) continue;
    add_task(0.3, [this, entry](OutputFile& out) {
      auto file = cd_.read_file("Piu/datas/BGA/" + entry.name);
      if (!file) return fail("read-failed");
      const std::string json = bga_to_json(*file, entry.name);
      if (json.empty()) return fail("damaged");
      out = {"bga/" + stem_of(entry.name) + ".json", to_bytes(json)};
      return true;
    });
  }
  for (const IsoEntry& entry : cd_.list("Piu/datas/step")) {
    if (!has_suffix(entry.name, ".stf")) continue;
    add_task(0.3, [this, entry](OutputFile& out) {
      auto file = cd_.read_file("Piu/datas/step/" + entry.name);
      if (!file) return fail("read-failed");
      const std::string json = stf_to_json(*file, entry.name);
      if (json.empty()) return fail("damaged");
      out = {"charts/" + stem_of(entry.name) + ".json", to_bytes(json)};
      return true;
    });
  }

  add_task(2.0, [this](OutputFile& out) {
    SceneFiles scene;
    char name[32];
    for (int i = 1; i <= kModels; ++i) {
      std::snprintf(name, sizeof(name), "Piu/datas/model/T%02d.3DM", i);
      auto model = cd_.read_file(name);
      if (!model) return fail("wrong-disc");
      scene.models.push_back(std::move(*model));
    }
    auto camera = cd_.read_file("Piu/datas/model/NONSTOP.CAM");
    auto materials = cd_.read_file("Piu/datas/piu.mtl");
    auto exe = cd_.read_file("Piu/PIU.EXE");
    if (!camera || !materials || !exe || exe->size() < kCameraScaleOffset + 4) return fail("wrong-disc");
    scene.camera = std::move(*camera);
    scene.materials.assign(materials->begin(), materials->end());
    std::uint32_t bits = 0;
    for (int i = 3; i >= 0; --i) bits = bits << 8 | (*exe)[kCameraScaleOffset + static_cast<std::size_t>(i)];
    std::memcpy(&scene.scale, &bits, sizeof(scene.scale));
    const std::string json = scene_to_json(scene);
    if (json.empty()) return fail("damaged");
    out = {"model/scene.json", to_bytes(json)};
    return true;
  });

  for (const CdTrack& track : cd_.tracks()) {
    if (!track.audio) continue;
    add_task(track.frames * kAudioWeightPerFrame, [this, track](OutputFile& out) {
      auto wav = cd_.read_audio_wav(track);
      if (!wav) return fail("read-failed");
      char name[32];
      std::snprintf(name, sizeof(name), "wav/track%02d.wav", track.number);
      out = {name, std::move(*wav)};
      return true;
    });
  }
  return true;
}

bool Extractor::next(OutputFile& out) {
  if (!error_.empty() || index_ >= tasks_.size()) return false;
  Task& task = tasks_[index_];
  if (!task.run(out)) return false;
  done_weight_ += task.weight;
  ++index_;
  return true;
}

}  // namespace piu::extract
