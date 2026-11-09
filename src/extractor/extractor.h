// Turns the user's disc into the library of files the engine reads.

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "extractor/cd_image.h"
#include "extractor/formats.h"

namespace piu::extract {

struct OutputFile {
  std::string name;  // Relative to the game library, in lower case.
  Bytes data;
};

// Turns the user's CHD into the library the engine reads, one file at a time.
class Extractor {
 public:
  bool open(std::unique_ptr<ByteSource> source);
  // Produces the next file. False when done or on error; error() is then a short
  // code the platform turns into a message.
  bool next(OutputFile& out);
  const std::string& error() const { return error_; }
  // From 0 to 1.
  double progress() const { return total_weight_ > 0.0 ? done_weight_ / total_weight_ : 0.0; }

 private:
  struct Task {
    double weight;
    std::function<bool(OutputFile&)> run;
  };

  void add_task(double weight, std::function<bool(OutputFile&)> run);
  bool prepare();
  bool fail(const std::string& message);

  CdImage cd_;
  std::vector<Task> tasks_;
  std::size_t index_ = 0;
  double done_weight_ = 0.0;
  double total_weight_ = 0.0;
  std::string error_;
  std::shared_ptr<Bytes> textures_;
  std::shared_ptr<Bytes> sprites_;
};

}  // namespace piu::extract
