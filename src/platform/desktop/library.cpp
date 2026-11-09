// Data folder, extraction to disk, file index and settings file.

#include "platform/desktop/library.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <system_error>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include "core/json.h"
#include "extractor/extractor.h"

namespace fs = std::filesystem;

namespace piu::desktop {
namespace {

constexpr int kLibraryVersion = 2;
constexpr const char* kFolderName = "Data";
constexpr const char* kMarker = "library.json";
constexpr const char* kSoundsMarker = "sounds.json";

fs::path executable_path() {
#if defined(_WIN32)
  wchar_t path[4096];
  return GetModuleFileNameW(nullptr, path, 4096) != 0 ? fs::path(path) : fs::path();
#elif defined(__APPLE__)
  std::uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string path(size, '\0');
  if (_NSGetExecutablePath(path.data(), &size) != 0) return {};
  path.resize(path.find('\0'));
  return fs::path(path);
#else
  std::error_code error;
  return fs::read_symlink("/proc/self/exe", error);
#endif
}

fs::path application_folder() {
  fs::path folder = executable_path().parent_path();
#if defined(__APPLE__)
  // Open It UP.app/Contents/MacOS/<program>: the game's folder is the one holding the .app.
  folder = folder.parent_path().parent_path().parent_path();
#endif
  return folder;
}

fs::path fallback_folder() {
#if defined(_WIN32)
  const wchar_t* local = _wgetenv(L"LOCALAPPDATA");
  return (local != nullptr ? fs::path(local) : fs::temp_directory_path()) / "Open It UP";
#else
  const char* home = std::getenv("HOME");
  const fs::path base = home != nullptr ? fs::path(home) : fs::temp_directory_path();
#if defined(__APPLE__)
  return base / "Library" / "Application Support" / "Open It UP";
#else
  return base / ".local" / "share" / "open-it-up";
#endif
#endif
}

bool usable(const fs::path& folder) {
  std::error_code error;
  fs::create_directories(folder, error);
  if (error) return false;
  const fs::path probe = folder / ".write-test";
  {
    std::ofstream file(probe);
    if (!file) return false;
  }
  fs::remove(probe, error);
  return true;
}

fs::path library_folder() { return data_folder() / "library"; }
fs::path sounds_folder() { return data_folder() / "sfx"; }
fs::path settings_path() { return data_folder() / "settings.json"; }

std::string lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  return text;
}

std::string quoted(const std::string& text) {
  std::string out = "\"";
  for (char c : text) {
    if (c == '"' || c == '\\') out += '\\';
    if (static_cast<unsigned char>(c) >= 0x20) out += c;
  }
  return out + "\"";
}

bool write_file(const fs::path& path, const char* data, std::size_t size) {
  std::error_code error;
  fs::create_directories(path.parent_path(), error);
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  file.write(data, static_cast<std::streamsize>(size));
  file.flush();
  return file.good();
}

std::optional<Json> read_json(const fs::path& path) {
  const std::optional<std::string> text = read_file(path);
  return text ? Json::parse(*text) : std::nullopt;
}

class DiscFile : public extract::ByteSource {
 public:
  explicit DiscFile(const fs::path& path) : file_(open_file(path)) {
    std::error_code error;
    size_ = fs::file_size(path, error);
    if (error) size_ = 0;
  }
  ~DiscFile() override {
    if (file_ != nullptr) std::fclose(file_);
  }
  std::uint64_t size() override { return size_; }
  std::size_t read(std::uint64_t offset, std::uint8_t* out, std::size_t count) override {
    if (file_ == nullptr || !seek(offset)) return 0;
    return std::fread(out, 1, count, file_);
  }

 private:
  static std::FILE* open_file(const fs::path& path) {
#if defined(_WIN32)
    return _wfopen(path.c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
  }

  bool seek(std::uint64_t offset) {
#if defined(_WIN32)
    return _fseeki64(file_, static_cast<long long>(offset), SEEK_SET) == 0;
#else
    return fseeko(file_, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
  }

  std::FILE* file_;
  std::uint64_t size_ = 0;
};

}  // namespace

fs::path data_folder() {
  static const fs::path folder = [] {
    const fs::path beside = application_folder() / kFolderName;
    return usable(beside) ? beside : fallback_folder();
  }();
  return folder;
}

std::optional<std::string> read_file(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::nullopt;
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

Settings load_settings() {
  Settings settings;
  const std::optional<Json> root = read_json(settings_path());
  if (!root) return settings;
  if (const Json* language = root->find("language"); language != nullptr && language->string() == "es") {
    settings.language = "es";
  }
  if (const Json* wide = root->find("wide"); wide != nullptr) settings.wide = wide->number() != 0.0;
  if (const Json* fullscreen = root->find("fullscreen"); fullscreen != nullptr) {
    settings.fullscreen = fullscreen->number() != 0.0;
  }
  if (const Json* keys = root->find("keys"); keys != nullptr) settings.keys = keys->string();
  if (const Json* offset = root->find("audioOffset"); offset != nullptr) settings.audio_offset_ms = static_cast<int>(offset->number());
  if (const Json* machine = root->find("machine"); machine != nullptr && machine->items().size() == 6) {
    std::array<int, 6> values{};
    for (std::size_t i = 0; i < values.size(); ++i) values[i] = static_cast<int>(machine->items()[i].number());
    settings.machine = values;
  }
  return settings;
}

bool save_settings(const Settings& settings) {
  std::ostringstream text;
  text << "{\"language\":" << quoted(settings.language) << ",\"wide\":" << (settings.wide ? 1 : 0)
       << ",\"fullscreen\":" << (settings.fullscreen ? 1 : 0);
  if (!settings.keys.empty()) text << ",\"keys\":" << quoted(settings.keys);
  if (settings.audio_offset_ms != 0) text << ",\"audioOffset\":" << settings.audio_offset_ms;
  if (settings.machine) {
    text << ",\"machine\":[";
    for (std::size_t i = 0; i < settings.machine->size(); ++i) text << (i == 0 ? "" : ",") << (*settings.machine)[i];
    text << "]";
  }
  text << "}\n";
  const std::string body = text.str();
  return write_file(settings_path(), body.data(), body.size());
}

LibraryState read_library_state() {
  LibraryState state;
  if (const std::optional<Json> marker = read_json(library_folder() / kMarker)) {
    const Json* version = marker->find("version");
    const Json* source = marker->find("source");
    if (version != nullptr && static_cast<int>(version->number()) == kLibraryVersion) {
      state.ready = true;
      if (source != nullptr) state.source = source->string();
    }
  }
  std::error_code error;
  state.sounds_ready = fs::exists(sounds_folder() / kSoundsMarker, error);
  return state;
}

std::string extract_game(const fs::path& chd, const Progress& progress) {
  extract::Extractor extractor;
  // A wrong file stops here, before the library that already works is touched.
  if (!extractor.open(std::make_unique<DiscFile>(chd))) return extractor.error();

  const fs::path library = library_folder();
  std::error_code error;
  fs::remove_all(library, error);

  // The marker goes in last, so a half-finished library never counts as ready.
  extract::OutputFile file;
  int count = 0;
  int announced = -1;
  while (extractor.next(file)) {
    if (!write_file(library / file.name, reinterpret_cast<const char*>(file.data.data()), file.data.size())) {
      return "no-space";
    }
    ++count;
    const int percent = static_cast<int>(extractor.progress() * 100.0);
    if (percent != announced) {
      announced = percent;
      if (progress) progress(percent);
    }
  }
  if (!extractor.error().empty()) return extractor.error();

  std::ostringstream marker;
  marker << "{\"version\":" << kLibraryVersion << ",\"source\":" << quoted(chd.filename().u8string())
         << ",\"files\":" << count << "}\n";
  const std::string body = marker.str();
  return write_file(library / kMarker, body.data(), body.size()) ? "" : "no-space";
}

std::string extract_sounds(const fs::path& rom, const Progress& progress) {
  const std::optional<std::string> table = read_file(library_folder() / "piu.bin");
  const std::optional<std::string> sound_rom = read_file(rom);
  if (!table || !sound_rom) return "wrong-sound-rom";
  const auto bytes = [](const std::string& text) { return extract::Bytes(text.begin(), text.end()); };
  const std::vector<extract::Bytes> sounds = extract::decode_sounds(bytes(*table), bytes(*sound_rom));
  if (sounds.empty()) return "wrong-sound-rom";

  const fs::path folder = sounds_folder();
  std::error_code error;
  fs::remove_all(folder, error);
  for (std::size_t i = 0; i < sounds.size(); ++i) {
    const fs::path path = folder / (std::to_string(i) + ".wav");
    if (!write_file(path, reinterpret_cast<const char*>(sounds[i].data()), sounds[i].size())) return "no-space";
    if (progress) progress(static_cast<int>((i + 1) * 100 / sounds.size()));
  }
  const std::string marker = "{\"count\":" + std::to_string(sounds.size()) + "}\n";
  return write_file(folder / kSoundsMarker, marker.data(), marker.size()) ? "" : "no-space";
}

bool Library::open() {
  files_.clear();
  charts_.clear();
  const fs::path root = library_folder();
  const std::pair<const char*, const char*> folders[] = {
      {"png", "png:"}, {"spr", "spr:"}, {"wav", "wav:"}, {"bga", "bga:"}, {"model", "scene:"}};
  std::error_code error;
  for (const auto& [folder, prefix] : folders) {
    for (const auto& entry : fs::directory_iterator(root / folder, error)) {
      files_[prefix + lower(entry.path().stem().string())] = entry.path();
    }
  }
  for (const auto& entry : fs::directory_iterator(root / "charts", error)) {
    charts_[lower(entry.path().stem().string())] = entry.path();
  }
  if (fs::exists(root / "stage.cfg", error)) files_["cfg:stage"] = root / "stage.cfg";
  for (const auto& entry : fs::directory_iterator(sounds_folder(), error)) {
    if (entry.path().extension() == ".wav") files_["sfx:" + entry.path().stem().string()] = entry.path();
  }
  return !files_.empty();
}

const fs::path* Library::find(const std::string& key) const {
  const auto found = files_.find(key);
  return found == files_.end() ? nullptr : &found->second;
}

const fs::path* Library::find_chart(const std::string& stem) const {
  const auto found = charts_.find(stem);
  return found == charts_.end() ? nullptr : &found->second;
}

}  // namespace piu::desktop
