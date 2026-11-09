// CHD access through libchdr, ISO9660 directory reading and audio track export.

#include "extractor/cd_image.h"

#include <libchdr/chd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace piu::extract {
namespace {

constexpr int kFrameBytes = 2448;
constexpr int kSectorBytes = 2352;
constexpr int kTrackPadding = 4;
constexpr std::uint32_t kIsoBlock = 2048;
constexpr std::uint32_t kVolumeDescriptor = 16;

std::string upper(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return text;
}

std::uint32_t le32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
         static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}

void put_le32(Bytes& out, std::size_t at, std::uint32_t v) {
  out[at] = static_cast<std::uint8_t>(v);
  out[at + 1] = static_cast<std::uint8_t>(v >> 8);
  out[at + 2] = static_cast<std::uint8_t>(v >> 16);
  out[at + 3] = static_cast<std::uint8_t>(v >> 24);
}

// "TRACK:2 TYPE:AUDIO SUBTYPE:NONE FRAMES:820 PREGAP:150 PGTYPE:VAUDIO PGSUB:NONE POSTGAP:0"
int field_int(const std::string& text, const char* key) {
  const std::size_t at = text.find(key);
  return at == std::string::npos ? 0 : std::atoi(text.c_str() + at + std::strlen(key));
}

std::string field_text(const std::string& text, const char* key) {
  const std::size_t at = text.find(key);
  if (at == std::string::npos) return {};
  std::istringstream stream(text.substr(at + std::strlen(key)));
  std::string value;
  stream >> value;
  return value;
}

// Raw sectors keep the CD header, and Mode 2 adds eight bytes more to it.
std::size_t user_data_offset(const std::string& type, const std::uint8_t* frame) {
  if (type == "MODE1_RAW" || type == "MODE2_RAW") return frame[15] == 2 ? 24 : 16;
  if (type == "MODE2" || type == "MODE2_FORM_MIX") return 8;
  return 0;
}

IsoEntry parse_record(const std::uint8_t* p) {
  IsoEntry entry;
  entry.lba = le32(p + 2);
  entry.size = le32(p + 10);
  entry.directory = (p[25] & 2) != 0;
  entry.name.assign(reinterpret_cast<const char*>(p + 33), p[32]);
  const std::size_t version = entry.name.find(';');
  if (version != std::string::npos) entry.name.erase(version);
  if (!entry.name.empty() && entry.name.back() == '.') entry.name.pop_back();
  return entry;
}

}  // namespace

struct CdImage::Handle {
  std::unique_ptr<ByteSource> source;
  std::uint64_t position = 0;
  chd_file* chd = nullptr;
  Bytes hunk;
  int hunk_number = -1;
  int frames_per_hunk = 0;
  ~Handle() {
    if (chd != nullptr) chd_close(chd);
  }
};

const core_file_callbacks kSourceCallbacks = {
    [](void* handle) -> std::uint64_t { return static_cast<CdImage::Handle*>(handle)->source->size(); },
    [](void* buffer, std::size_t size, std::size_t count, void* opaque) -> std::size_t {
      auto* handle = static_cast<CdImage::Handle*>(opaque);
      const std::size_t bytes = handle->source->read(handle->position, static_cast<std::uint8_t*>(buffer), size * count);
      handle->position += bytes;
      return size == 0 ? 0 : bytes / size;
    },
    [](void*) -> int { return 0; },
    [](void* opaque, std::int64_t offset, int whence) -> int {
      auto* handle = static_cast<CdImage::Handle*>(opaque);
      const std::int64_t base = whence == SEEK_SET ? 0
                                : whence == SEEK_CUR ? static_cast<std::int64_t>(handle->position)
                                                     : static_cast<std::int64_t>(handle->source->size());
      if (base + offset < 0) return -1;
      handle->position = static_cast<std::uint64_t>(base + offset);
      return 0;
    }};

CdImage::CdImage() : handle_(std::make_unique<Handle>()) {}
CdImage::~CdImage() = default;

bool CdImage::open(std::unique_ptr<ByteSource> source, std::string& error) {
  handle_->source = std::move(source);
  if (chd_open_core_file_callbacks(&kSourceCallbacks, handle_.get(), CHD_OPEN_READ, nullptr, &handle_->chd) != CHDERR_NONE) {
    error = "not-chd";
    return false;
  }
  const chd_header* header = chd_get_header(handle_->chd);
  if (header->unitbytes != kFrameBytes) {
    error = "not-cd";
    return false;
  }
  handle_->frames_per_hunk = static_cast<int>(header->hunkbytes) / kFrameBytes;
  handle_->hunk.resize(header->hunkbytes);

  // CHD lays tracks end to end, each padded to a multiple of four frames.
  int chd_frame = 0;
  for (std::uint32_t index = 0;; ++index) {
    char text[256] = {};
    std::uint32_t length = 0;
    if (chd_get_metadata(handle_->chd, CDROM_TRACK_METADATA2_TAG, index, text, sizeof(text) - 1, &length, nullptr, nullptr) != CHDERR_NONE) break;
    const std::string line(text);
    const int frames = field_int(line, "FRAMES:");
    const int pregap = field_int(line, "PREGAP:");
    const bool pregap_in_data = field_text(line, "PGTYPE:").rfind('V', 0) == 0;
    const int skip = pregap_in_data ? pregap : 0;
    CdTrack track;
    track.number = field_int(line, "TRACK:");
    track.audio = field_text(line, "TYPE:") == "AUDIO";
    track.first_frame = chd_frame + skip;
    track.frames = frames - skip;
    tracks_.push_back(track);
    if (tracks_.size() == 1) data_type_ = field_text(line, "TYPE:");
    chd_frame += frames + ((kTrackPadding - frames % kTrackPadding) % kTrackPadding);
  }
  if (tracks_.empty() || tracks_.front().audio) {
    error = "wrong-disc";
    return false;
  }
  data_first_frame_ = tracks_.front().first_frame;

  std::uint8_t descriptor[kIsoBlock];
  if (!read_sector(kVolumeDescriptor, descriptor) || std::memcmp(descriptor + 1, "CD001", 5) != 0) {
    error = "wrong-disc";
    return false;
  }
  root_ = parse_record(descriptor + 156);
  root_.directory = true;
  return true;
}

bool CdImage::read_frame(int frame, const std::uint8_t*& data) {
  const int hunk = frame / handle_->frames_per_hunk;
  if (hunk != handle_->hunk_number) {
    if (chd_read(handle_->chd, static_cast<std::uint32_t>(hunk), handle_->hunk.data()) != CHDERR_NONE) return false;
    handle_->hunk_number = hunk;
  }
  data = handle_->hunk.data() + (frame % handle_->frames_per_hunk) * kFrameBytes;
  return true;
}

bool CdImage::read_sector(std::uint32_t lba, std::uint8_t* out) {
  const std::uint8_t* frame = nullptr;
  if (!read_frame(data_first_frame_ + static_cast<int>(lba), frame)) return false;
  std::memcpy(out, frame + user_data_offset(data_type_, frame), kIsoBlock);
  return true;
}

std::vector<IsoEntry> CdImage::read_directory(const IsoEntry& entry) {
  Bytes data(entry.size);
  std::vector<IsoEntry> out;
  std::uint8_t block[kIsoBlock];
  for (std::uint32_t i = 0; i * kIsoBlock < entry.size; ++i) {
    if (!read_sector(entry.lba + i, block)) return out;
    std::memcpy(&data[i * kIsoBlock], block, std::min(kIsoBlock, entry.size - i * kIsoBlock));
  }
  for (std::size_t offset = 0; offset < data.size();) {
    const int length = data[offset];
    if (length == 0) {
      offset = (offset / kIsoBlock + 1) * kIsoBlock;
      continue;
    }
    if (data[offset + 33] > 1 || data[offset + 32] > 1) {
      IsoEntry record = parse_record(&data[offset]);
      if (record.name != "." && record.name != "\x01" && !record.name.empty() && record.name[0] != 0) out.push_back(record);
    }
    offset += static_cast<std::size_t>(length);
  }
  return out;
}

std::optional<IsoEntry> CdImage::find(const std::string& path) {
  IsoEntry current = root_;
  std::istringstream parts(path);
  std::string part;
  while (std::getline(parts, part, '/')) {
    if (part.empty()) continue;
    bool found = false;
    for (const IsoEntry& entry : read_directory(current)) {
      if (upper(entry.name) == upper(part)) {
        current = entry;
        found = true;
        break;
      }
    }
    if (!found) return std::nullopt;
  }
  return current;
}

std::vector<IsoEntry> CdImage::list(const std::string& directory) {
  const std::optional<IsoEntry> entry = find(directory);
  return entry && entry->directory ? read_directory(*entry) : std::vector<IsoEntry>{};
}

std::optional<Bytes> CdImage::read_file(const std::string& path) {
  const std::optional<IsoEntry> entry = find(path);
  if (!entry || entry->directory) return std::nullopt;
  Bytes data(entry->size);
  std::uint8_t block[kIsoBlock];
  for (std::uint32_t i = 0; i * kIsoBlock < entry->size; ++i) {
    if (!read_sector(entry->lba + i, block)) return std::nullopt;
    std::memcpy(&data[i * kIsoBlock], block, std::min(kIsoBlock, entry->size - i * kIsoBlock));
  }
  return data;
}

std::optional<Bytes> CdImage::read_audio_wav(const CdTrack& track) {
  const auto pcm = static_cast<std::uint32_t>(track.frames) * kSectorBytes;
  Bytes out(44 + std::size_t{pcm});
  std::memcpy(&out[0], "RIFF", 4);
  put_le32(out, 4, 36 + pcm);
  std::memcpy(&out[8], "WAVEfmt ", 8);
  put_le32(out, 16, 16);
  put_le32(out, 20, 0x00020001);  // PCM, stereo.
  put_le32(out, 24, 44100);
  put_le32(out, 28, 44100 * 4);
  put_le32(out, 32, 0x00100004);  // 4 bytes per sample, 16 bits.
  std::memcpy(&out[36], "data", 4);
  put_le32(out, 40, pcm);
  for (int f = 0; f < track.frames; ++f) {
    const std::uint8_t* frame = nullptr;
    if (!read_frame(track.first_frame + f, frame)) return std::nullopt;
    std::uint8_t* target = &out[44 + static_cast<std::size_t>(f) * kSectorBytes];
    // CHD stores CD audio big-endian.
    for (int i = 0; i < kSectorBytes; i += 2) {
      target[i] = frame[i + 1];
      target[i + 1] = frame[i];
    }
  }
  return out;
}

}  // namespace piu::extract
