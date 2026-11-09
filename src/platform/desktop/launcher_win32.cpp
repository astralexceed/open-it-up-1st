// The Windows launcher window: file selection, settings and launching the game.

#include <windows.h>

#include <commctrl.h>
#include <commdlg.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "platform/desktop/game_app.h"
#include "platform/desktop/library.h"

namespace piu::desktop {
namespace {

constexpr int kColumnWidth = 400;
constexpr int kColumnGap = 48;
constexpr int kContentWidth = kColumnWidth * 2 + kColumnGap;
constexpr UINT kProgressMessage = WM_APP + 1;
constexpr UINT kDoneMessage = WM_APP + 2;
constexpr COLORREF kPanel = RGB(0x17, 0x15, 0x28);
constexpr COLORREF kLine = RGB(0x35, 0x2f, 0x59);
constexpr COLORREF kInk = RGB(0xec, 0xe9, 0xf7);
constexpr COLORREF kSoft = RGB(0xa7, 0xa1, 0xc4);
constexpr COLORREF kSky = RGB(0x87, 0xce, 0xeb);
constexpr COLORREF kWarn = RGB(0xff, 0x8a, 0x5c);

enum Control { kChooseChd = 101, kAddSounds, kLaunch, kAspect, kLanguage, kDisplay, kMapKeys, kCalibrate };

struct Text {
  const char* key;
  const char* en;
  const char* es;
};

const Text kTexts[] = {
    {"instructions", "Instructions", "Instrucciones"},
    {"loadOriginals", "Load the original files from your own copy of the game:", "Carga los archivos originales de tu propia copia del juego:"},
    {"chdUse", "19990930.chd: BGAs, Steps and Sprites.", "19990930.chd: BGAs, Steps y Sprites."},
    {"romUse", "piu10.u9: Additional sound effects (Grades, Coins, Menus, etc.).", "piu10.u9: Efectos de sonido adicionales (Grades, Monedas, Menús, etc.)."},
    {"fileSelection", "File selection", "Selección de archivos"},
    {"addSounds", "Add sound effects (piu10.u9)", "Agregar efectos de sonido (piu10.u9)"},
    {"changeSounds", "Change piu10.u9 file", "Cambiar archivo piu10.u9"},
    {"extractNote", "Files are extracted only once and a path will be generated inside the game folder.", "Los archivos se extraen una sola vez y se generará un path dentro de la carpeta del juego."},
    {"legalNote", "The files used by this engine must be obtained through a legal backup of your own copy of the game.", "Los archivos utilizados para este motor, deben ser obtenidos a través de un backup legal de tu propia copia del juego."},
    {"additionalSettings", "Additional settings", "Configuraciones adicionales"},
    {"aspectRatio", "Aspect ratio", "Relación de aspecto"},
    {"aspectOriginal", "Original (4:3)", "Original (4:3)"},
    {"aspectWide", "Widescreen (16:9)", "Panorámica (16:9)"},
    {"menuLanguage", "Menu language", "Idioma del menú"},
    {"languageEnglish", "English", "Inglés"},
    {"languageSpanish", "Spanish", "Español"},
    {"displayMode", "Display mode", "Modo de pantalla"},
    {"displayWindowed", "Windowed", "Ventana"},
    {"displayFullscreen", "Fullscreen", "Pantalla completa"},
    {"mapControls", "Map controls", "Mapear controles"},
    {"calibrateAudio", "Calibrate audio", "Calibrar audio"},
    {"launch", "Launch game", "Lanzar juego"},
    {"chooseChd", "Choose CHD file", "Elegir archivo CHD"},
    {"changeChd", "Change CHD file", "Cambiar archivo CHD"},
    {"noGame", "No game loaded yet.", "Todavía no hay juego cargado."},
    {"ready", "Ready: %", "Listo: %"},
    {"soundsReady", "Sound effects ready.", "Efectos de sonido listos."},
    {"noSounds", "No sound effects (optional).", "Sin efectos de sonido (opcional)."},
    {"opening", "Opening the CHD…", "Abriendo el CHD…"},
    {"extracting", "Extracting… %", "Extrayendo… %"},
    {"readingSounds", "Reading the sound effects…", "Leyendo los efectos de sonido…"},
    {"no-space", "There is not enough space on this disk.", "No hay espacio suficiente en este disco."},
    {"not-chd", "That file could not be opened as a CHD.", "No pude abrir ese archivo como CHD."},
    {"not-cd", "That CHD is not a CD image.", "Ese CHD no es de un CD."},
    {"wrong-disc", "That CHD is not this game's disc.", "Ese CHD no es el disco de este juego."},
    {"damaged", "The game's files are damaged on that CHD.", "Los archivos del juego vienen dañados en ese CHD."},
    {"read-failed", "Part of that CHD could not be read.", "No pude leer una parte de ese CHD."},
    {"wrong-sound-rom", "That file is not the game's sound ROM (piu10.u9).", "Ese archivo no es la ROM de sonidos del juego (piu10.u9)."},
};

std::wstring wide(const std::string& text) {
  if (text.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<std::size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
  out.resize(static_cast<std::size_t>(size) - 1);
  return out;
}

struct Item {
  enum class Kind { kTitle, kLabel, kSeparator, kButton, kCombo, kProgress };
  HWND hwnd;
  Kind kind;
  HFONT font;
  // 0 or 1 for a column; -1 across the whole window.
  int column;
};

struct Job {
  HWND window;
  bool sounds;
  std::wstring path;
};

DWORD WINAPI run_job(void* data) {
  const Job* job = static_cast<const Job*>(data);
  const HWND window = job->window;
  const std::filesystem::path file(job->path);
  const Progress progress = [window](int percent) { PostMessageW(window, kProgressMessage, static_cast<WPARAM>(percent), 0); };
  const std::string error = job->sounds ? extract_sounds(file, progress) : extract_game(file, progress);
  PostMessageW(window, kDoneMessage, 0, reinterpret_cast<LPARAM>(new std::string(error)));
  delete job;
  return 0;
}

class Launcher {
 public:
  explicit Launcher(HINSTANCE instance) : instance_(instance) {}
  int run();
  LRESULT handle(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

 private:
  int scaled(int value) const { return MulDiv(value, dpi_, 96); }
  std::string text(const char* key) const;
  HFONT make_font(int points, bool bold) const;
  HWND add(const wchar_t* type, DWORD style, Item::Kind kind, HFONT font, int id = 0);
  HWND add_label(const char* key, HFONT font, bool soft = false);
  void add_separator();
  HWND add_combo(const char* label, int id);
  void build();
  void layout();
  void render_texts();
  void render_controls();
  void render_status();
  void set_status(const std::string& key, const std::string& argument, bool warn);
  void show_library();
  void fill_combo(HWND combo, const char* first, const char* second, int selected);
  void store_choices();
  void start_job(bool sounds, const wchar_t* filter);
  void leave_for(Start start);
  LRESULT color_static(HDC dc, HWND control);
  void draw_title(const DRAWITEMSTRUCT& item);

  HINSTANCE instance_;
  HWND window_ = nullptr;
  int dpi_ = 96;
  HBRUSH panel_ = nullptr;
  HBRUSH line_ = nullptr;
  HFONT title_font_ = nullptr;
  HFONT body_ = nullptr;
  HFONT heading_ = nullptr;
  HFONT small_ = nullptr;
  std::vector<Item> items_;
  int column_ = -1;
  std::vector<std::pair<HWND, const char*>> translated_;
  std::vector<HWND> soft_;
  HWND status_ = nullptr;
  HWND sound_status_ = nullptr;
  HWND progress_ = nullptr;
  HWND choose_chd_ = nullptr;
  HWND add_sounds_ = nullptr;
  HWND launch_ = nullptr;
  HWND map_keys_ = nullptr;
  HWND calibrate_ = nullptr;
  HWND aspect_ = nullptr;
  HWND language_ = nullptr;
  HWND display_ = nullptr;
  Settings settings_;
  LibraryState state_;
  bool busy_ = false;
  std::string status_key_;
  std::string status_argument_;
  bool status_warn_ = false;
};

std::string Launcher::text(const char* key) const {
  for (const Text& entry : kTexts) {
    if (std::strcmp(entry.key, key) == 0) return settings_.language == "es" ? entry.es : entry.en;
  }
  return key;
}

HFONT Launcher::make_font(int points, bool bold) const {
  return CreateFontW(-MulDiv(points, dpi_, 72), 0, 0, 0, bold ? FW_BOLD : FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

HWND Launcher::add(const wchar_t* type, DWORD style, Item::Kind kind, HFONT font, int id) {
  const HWND control = CreateWindowExW(0, type, L"", WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, window_,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
  SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  items_.push_back({control, kind, font, column_});
  return control;
}

HWND Launcher::add_label(const char* key, HFONT font, bool soft) {
  const HWND label = add(L"STATIC", SS_NOPREFIX, Item::Kind::kLabel, font);
  if (key != nullptr) translated_.emplace_back(label, key);
  if (soft) soft_.push_back(label);
  return label;
}

void Launcher::add_separator() {
  const HWND line = add(L"STATIC", 0, Item::Kind::kSeparator, body_);
  SetWindowLongPtrW(line, GWLP_USERDATA, 1);
}

HWND Launcher::add_combo(const char* label, int id) {
  add_label(label, small_, true);
  return add(L"COMBOBOX", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, Item::Kind::kCombo, body_, id);
}

void Launcher::build() {
  add(L"STATIC", SS_OWNERDRAW, Item::Kind::kTitle, title_font_);
  add_separator();
  column_ = 0;
  add_label("instructions", heading_);
  add_label("loadOriginals", body_);
  add_label("chdUse", body_);
  add_label("romUse", body_);
  add_separator();
  add_label("fileSelection", heading_);
  status_ = add_label(nullptr, body_);
  progress_ = add(PROGRESS_CLASSW, 0, Item::Kind::kProgress, body_);
  SendMessageW(progress_, PBM_SETRANGE32, 0, 100);
  choose_chd_ = add(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, Item::Kind::kButton, body_, kChooseChd);
  add_sounds_ = add(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, Item::Kind::kButton, body_, kAddSounds);
  sound_status_ = add_label(nullptr, small_, true);
  add_label("extractNote", small_, true);
  add_label("legalNote", small_, true);
  column_ = 1;
  add_label("additionalSettings", heading_);
  aspect_ = add_combo("aspectRatio", kAspect);
  language_ = add_combo("menuLanguage", kLanguage);
  display_ = add_combo("displayMode", kDisplay);
  map_keys_ = add(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, Item::Kind::kButton, body_, kMapKeys);
  calibrate_ = add(L"BUTTON", BS_PUSHBUTTON | WS_TABSTOP, Item::Kind::kButton, body_, kCalibrate);
  add_separator();
  launch_ = add(L"BUTTON", BS_DEFPUSHBUTTON | WS_TABSTOP, Item::Kind::kButton, body_, kLaunch);
}

// Places the controls top to bottom, and the window follows its content.
void Launcher::layout() {
  const int total_width = scaled(kContentWidth);
  const int column_width = scaled(kColumnWidth);
  const int gap = scaled(12);
  const int left = scaled(36);
  int top = scaled(32);
  int column_y[2] = {top, top};
  const HDC dc = GetDC(window_);
  for (const Item& item : items_) {
    if (item.kind == Item::Kind::kProgress && (GetWindowLongW(item.hwnd, GWL_STYLE) & WS_VISIBLE) == 0) continue;
    const bool across = item.column < 0;
    const int width = across ? total_width : column_width;
    const int x = left + (item.column == 1 ? column_width + scaled(kColumnGap) : 0);
    int& y = across ? top : column_y[item.column];
    int height = scaled(36);
    int control_height = 0;
    switch (item.kind) {
      case Item::Kind::kTitle: height = scaled(44); break;
      case Item::Kind::kSeparator: height = 1; break;
      case Item::Kind::kCombo:
        height = scaled(26);
        control_height = scaled(160);
        break;
      case Item::Kind::kProgress: height = scaled(12); break;
      case Item::Kind::kButton: break;
      case Item::Kind::kLabel: {
        wchar_t buffer[512];
        GetWindowTextW(item.hwnd, buffer, 512);
        const HGDIOBJ previous = SelectObject(dc, item.font);
        RECT box = {0, 0, width, 0};
        DrawTextW(dc, buffer, -1, &box, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, previous);
        height = box.bottom;
        break;
      }
    }
    SetWindowPos(item.hwnd, nullptr, x, y, width, control_height != 0 ? control_height : height, SWP_NOZORDER | SWP_NOACTIVATE);
    y += height + gap;
    if (across) column_y[0] = column_y[1] = top;
  }
  ReleaseDC(window_, dc);

  const DWORD style = static_cast<DWORD>(GetWindowLongW(window_, GWL_STYLE));
  RECT frame = {0, 0, total_width + left * 2, std::max(column_y[0], column_y[1]) + scaled(24)};
  AdjustWindowRect(&frame, style, FALSE);
  SetWindowPos(window_, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Launcher::fill_combo(HWND combo, const char* first, const char* second, int selected) {
  SendMessageW(combo, CB_RESETCONTENT, 0, 0);
  SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(text(first)).c_str()));
  SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide(text(second)).c_str()));
  SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected), 0);
}

void Launcher::render_status() {
  std::string message;
  if (!status_key_.empty()) {
    message = text(status_key_.c_str());
    const std::size_t mark = message.find('%');
    if (mark != std::string::npos) {
      const bool percent = status_key_ == "extracting";
      message.replace(mark, 1, status_argument_ + (percent ? "%" : ""));
    }
  }
  SetWindowTextW(status_, wide(message).c_str());
  InvalidateRect(status_, nullptr, TRUE);
}

void Launcher::set_status(const std::string& key, const std::string& argument, bool warn) {
  status_key_ = key;
  status_argument_ = argument;
  status_warn_ = warn;
  render_status();
}

void Launcher::render_controls() {
  SetWindowTextW(choose_chd_, wide(text(state_.ready ? "changeChd" : "chooseChd")).c_str());
  SetWindowTextW(add_sounds_, wide(text(state_.sounds_ready ? "changeSounds" : "addSounds")).c_str());
  SetWindowTextW(launch_, wide(text("launch")).c_str());
  SetWindowTextW(map_keys_, wide(text("mapControls")).c_str());
  EnableWindow(map_keys_, !busy_);
  SetWindowTextW(calibrate_, wide(text("calibrateAudio")).c_str());
  EnableWindow(calibrate_, !busy_);
  SetWindowTextW(sound_status_, wide(text(state_.sounds_ready ? "soundsReady" : "noSounds")).c_str());
  EnableWindow(choose_chd_, !busy_);
  EnableWindow(add_sounds_, !busy_ && state_.ready);
  EnableWindow(launch_, !busy_ && state_.ready);
  ShowWindow(progress_, busy_ ? SW_SHOW : SW_HIDE);
}

void Launcher::render_texts() {
  for (const auto& [label, key] : translated_) SetWindowTextW(label, wide(text(key)).c_str());
  fill_combo(aspect_, "aspectOriginal", "aspectWide", settings_.wide ? 1 : 0);
  fill_combo(language_, "languageEnglish", "languageSpanish", settings_.language == "es" ? 1 : 0);
  fill_combo(display_, "displayWindowed", "displayFullscreen", settings_.fullscreen ? 1 : 0);
  render_controls();
  render_status();
}

void Launcher::show_library() {
  state_ = read_library_state();
  if (state_.ready) set_status("ready", state_.source, false);
  else set_status("noGame", "", false);
  render_controls();
  layout();
}

void Launcher::store_choices() {
  settings_.wide = SendMessageW(aspect_, CB_GETCURSEL, 0, 0) == 1;
  settings_.fullscreen = SendMessageW(display_, CB_GETCURSEL, 0, 0) == 1;
  save_settings(settings_);
}

void Launcher::start_job(bool sounds, const wchar_t* filter) {
  wchar_t path[4096] = L"";
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = window_;
  dialog.lpstrFilter = filter;
  dialog.lpstrFile = path;
  dialog.nMaxFile = 4096;
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
  if (!GetOpenFileNameW(&dialog)) return;

  busy_ = true;
  SendMessageW(progress_, PBM_SETPOS, 0, 0);
  set_status(sounds ? "readingSounds" : "opening", "", false);
  render_controls();
  layout();
  const HANDLE thread = CreateThread(nullptr, 0, run_job, new Job{window_, sounds, path}, 0, nullptr);
  if (thread != nullptr) CloseHandle(thread);
}

void Launcher::leave_for(Start start) {
  store_choices();
  ShowWindow(window_, SW_HIDE);
  open_application(start);
  DestroyWindow(window_);
}

LRESULT Launcher::color_static(HDC dc, HWND control) {
  if (GetWindowLongPtrW(control, GWLP_USERDATA) == 1) return reinterpret_cast<LRESULT>(line_);
  bool soft = control == status_;
  for (HWND label : soft_) soft = soft || label == control;
  SetTextColor(dc, control == status_ && status_warn_ ? kWarn : soft ? kSoft : kInk);
  SetBkColor(dc, kPanel);
  return reinterpret_cast<LRESULT>(panel_);
}

void Launcher::draw_title(const DRAWITEMSTRUCT& item) {
  FillRect(item.hDC, &item.rcItem, panel_);
  const HGDIOBJ previous = SelectObject(item.hDC, title_font_);
  SetBkMode(item.hDC, TRANSPARENT);
  const std::pair<const wchar_t*, COLORREF> runs[] = {{L"Open ", kInk}, {L"It UP", kSky}, {L": 1st Preservation", kInk}};
  int x = item.rcItem.left;
  for (const auto& [run, color] : runs) {
    const int length = static_cast<int>(wcslen(run));
    SetTextColor(item.hDC, color);
    TextOutW(item.hDC, x, item.rcItem.top, run, length);
    SIZE size{};
    GetTextExtentPoint32W(item.hDC, run, length, &size);
    x += size.cx;
  }
  SelectObject(item.hDC, previous);
}

LRESULT Launcher::handle(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  switch (message) {
    case WM_CTLCOLORSTATIC:
      return color_static(reinterpret_cast<HDC>(wparam), reinterpret_cast<HWND>(lparam));
    case WM_DRAWITEM:
      draw_title(*reinterpret_cast<const DRAWITEMSTRUCT*>(lparam));
      return TRUE;
    case WM_COMMAND: {
      const int id = LOWORD(wparam);
      if (HIWORD(wparam) == CBN_SELCHANGE) {
        if (id == kLanguage) {
          settings_.language = SendMessageW(language_, CB_GETCURSEL, 0, 0) == 1 ? "es" : "en";
          store_choices();
          render_texts();
          layout();
        } else {
          store_choices();
        }
      } else if (id == kChooseChd) {
        start_job(false, L"CHD (*.chd)\0*.chd\0");
      } else if (id == kAddSounds) {
        start_job(true, L"piu10.u9 (*.u9)\0*.u9\0");
      } else if (id == kMapKeys) {
        leave_for(Start::kKeyMap);
      } else if (id == kCalibrate) {
        leave_for(Start::kCalibration);
      } else if ((id == kLaunch || id == IDOK) && IsWindowEnabled(launch_)) {
        leave_for(Start::kGame);
      }
      return 0;
    }
    case kProgressMessage:
      SendMessageW(progress_, PBM_SETPOS, wparam, 0);
      set_status("extracting", std::to_string(wparam), false);
      layout();
      return 0;
    case kDoneMessage: {
      const std::string* error = reinterpret_cast<const std::string*>(lparam);
      busy_ = false;
      show_library();
      if (!error->empty()) {
        set_status(*error, "", true);
        layout();
      }
      delete error;
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProcW(window, message, wparam, lparam);
  }
}

LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* launcher = reinterpret_cast<Launcher*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (launcher == nullptr || message == WM_NCCREATE) return DefWindowProcW(window, message, wparam, lparam);
  return launcher->handle(window, message, wparam, lparam);
}

int Launcher::run() {
  settings_ = load_settings();
  const HDC screen = GetDC(nullptr);
  dpi_ = GetDeviceCaps(screen, LOGPIXELSY);
  ReleaseDC(nullptr, screen);

  INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
  InitCommonControlsEx(&controls);

  panel_ = CreateSolidBrush(kPanel);
  line_ = CreateSolidBrush(kLine);
  title_font_ = make_font(20, true);
  body_ = make_font(10, false);
  heading_ = make_font(10, true);
  small_ = make_font(9, false);

  WNDCLASSEXW type{};
  type.cbSize = sizeof(type);
  type.lpfnWndProc = window_procedure;
  type.hInstance = instance_;
  type.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(1));
  type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  type.hbrBackground = panel_;
  type.lpszClassName = L"OpenItUpLauncher";
  RegisterClassExW(&type);

  window_ = CreateWindowExW(0, L"OpenItUpLauncher", L"Open It UP: 1st Preservation", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 500, 640, nullptr, nullptr, instance_, this);
  if (window_ == nullptr) return 1;

  build();
  render_texts();
  show_library();

  RECT area{};
  SystemParametersInfoW(SPI_GETWORKAREA, 0, &area, 0);
  RECT frame{};
  GetWindowRect(window_, &frame);
  const int width = frame.right - frame.left;
  const int height = frame.bottom - frame.top;
  SetWindowPos(window_, nullptr, area.left + (area.right - area.left - width) / 2, std::max<int>(area.top, area.top + (area.bottom - area.top - height) / 2),
               0, 0, SWP_NOSIZE | SWP_NOZORDER);
  ShowWindow(window_, SW_SHOW);

  MSG message;
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (IsDialogMessageW(window_, &message)) continue;
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return 0;
}

}  // namespace

void open_application(Start start) {
  wchar_t path[4096];
  if (GetModuleFileNameW(nullptr, path, 4096) == 0) return;
  std::wstring command = L"\"" + std::wstring(path) + L"\"";
  if (start == Start::kGame) command += L" --play";
  if (start == Start::kKeyMap) command += L" --keys";
  if (start == Start::kCalibration) command += L" --calibrate";
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  // Lets the new window come to the front when this one closes.
  AllowSetForegroundWindow(ASFW_ANY);
  if (CreateProcessW(path, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
  }
}

void bring_application_to_front() {}

int run_launcher(int, char**) {
  Launcher launcher(GetModuleHandleW(nullptr));
  return launcher.run();
}

}  // namespace piu::desktop
