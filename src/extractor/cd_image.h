// Reads the CHD disc image: tracks and the ISO9660 file system.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "extractor/formats.h"

namespace piu::extract {

// Where the CHD's bytes come from, so each platform reads them its own way.
class ByteSource {
 public:
  virtual ~ByteSource() = default;
  virtual std::uint64_t size() = 0;
  // Returns how many bytes it read.
  virtual std::size_t read(std::uint64_t offset, std::uint8_t* out, std::size_t count) = 0;
};

struct CdTrack {
  int number = 0;
  bool audio = false;
  int first_frame = 0;  // Frame in the CHD, after any pregap.
  int frames = 0;
};

struct IsoEntry {
  std::string name;
  std::uint32_t lba = 0;
  std::uint32_t size = 0;
  bool directory = false;
};

// A compressed CD in CHD format, with the ISO9660 file system of its data track.
class CdImage {
 public:
  CdImage();
  ~CdImage();
  CdImage(const CdImage&) = delete;
  CdImage& operator=(const CdImage&) = delete;

  bool open(std::unique_ptr<ByteSource> source, std::string& error);
  const std::vector<CdTrack>& tracks() const { return tracks_; }

  std::vector<IsoEntry> list(const std::string& directory);
  // Path relative to the root, any case. Empty if the file does not exist.
  std::optional<Bytes> read_file(const std::string& path);
  // Audio track as a WAV file: 44.1 kHz, 16-bit stereo.
  std::optional<Bytes> read_audio_wav(const CdTrack& track);

  struct Handle;

 private:

  bool read_frame(int frame, const std::uint8_t*& data);
  bool read_sector(std::uint32_t lba, std::uint8_t* out);
  std::vector<IsoEntry> read_directory(const IsoEntry& entry);
  std::optional<IsoEntry> find(const std::string& path);

  std::unique_ptr<Handle> handle_;
  std::vector<CdTrack> tracks_;
  int data_first_frame_ = 0;
  std::string data_type_;
  IsoEntry root_;
};

}  // namespace piu::extract
