// Browser extractor: reads the disc the user picked and hands back each file.

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <vector>
#include <optional>

#include "extractor/extractor.h"

// The worker leaves the File the user picked in self.piuDisc.
EM_JS(double, disc_size, (), { return self.piuDisc.size; });
EM_JS(int, disc_read, (double offset, int count, void* out), {
  const bytes = new FileReaderSync().readAsArrayBuffer(self.piuDisc.slice(offset, offset + count));
  HEAPU8.set(new Uint8Array(bytes), out);
  return bytes.byteLength;
});

namespace {

class DiscFile : public piu::extract::ByteSource {
 public:
  std::uint64_t size() override { return static_cast<std::uint64_t>(disc_size()); }
  std::size_t read(std::uint64_t offset, std::uint8_t* out, std::size_t count) override {
    return static_cast<std::size_t>(disc_read(static_cast<double>(offset), static_cast<int>(count), out));
  }
};

std::optional<piu::extract::Extractor> extractor;
piu::extract::OutputFile current;
std::vector<piu::extract::Bytes> sounds;

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int ext_open() {
  extractor.emplace();
  return extractor->open(std::make_unique<DiscFile>()) ? 1 : 0;
}

// Produces the next file. 0 when finished or on error.
EMSCRIPTEN_KEEPALIVE int ext_next() { return extractor->next(current) ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE const char* ext_name() { return current.name.c_str(); }
EMSCRIPTEN_KEEPALIVE const void* ext_data() { return current.data.data(); }
EMSCRIPTEN_KEEPALIVE int ext_size() { return static_cast<int>(current.data.size()); }
EMSCRIPTEN_KEEPALIVE double ext_progress() { return extractor->progress(); }
EMSCRIPTEN_KEEPALIVE const char* ext_error() { return extractor->error().c_str(); }

EMSCRIPTEN_KEEPALIVE void* ext_alloc(int size) { return std::malloc(static_cast<std::size_t>(size)); }
// Decodes the sound effects; both buffers are released here. Returns how many there are.
EMSCRIPTEN_KEEPALIVE int ext_sounds_open(std::uint8_t* table, int table_size, std::uint8_t* rom, int rom_size) {
  const piu::extract::Bytes table_bytes(table, table + table_size);
  const piu::extract::Bytes rom_bytes(rom, rom + rom_size);
  std::free(table);
  std::free(rom);
  sounds = piu::extract::decode_sounds(table_bytes, rom_bytes);
  return static_cast<int>(sounds.size());
}
EMSCRIPTEN_KEEPALIVE const void* ext_sound_data(int index) { return sounds[static_cast<std::size_t>(index)].data(); }
EMSCRIPTEN_KEEPALIVE int ext_sound_size(int index) { return static_cast<int>(sounds[static_cast<std::size_t>(index)].size()); }

}
