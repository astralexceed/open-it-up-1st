# Building Open It UP

You only need this if you want to modify the game or prefer to compile it yourself. To just play, see the [README](README.md).

Download this repository and open a terminal in its folder. Then run the script for your platform. On macOS and Windows, the result is saved in a folder on your **Desktop**, and the script tells you where it went.

| Version | Command | Output |
|---|---|---|
| Web | `sh run-web.sh` | `build-web/`, served on `localhost:8000` |
| macOS | `sh run-mac.sh` | `Desktop/Open It UP/` |
| Windows, from a PC | `run-windows.bat` | `Desktop\Open It UP\` |
| Windows, from macOS or Linux | `sh run-windows.sh` | `Desktop/Open It UP (Windows)/` |

Building downloads [libchdr](https://github.com/rtissera/libchdr) (BSD-3) and, on macOS and Windows, [SDL2](https://github.com/libsdl-org/SDL) (zlib), both pinned to an exact commit. The first build takes a few minutes.

Rebuilding only replaces the program, so the `Data` folder next to it is kept.

## Web

**Requirements:** [Emscripten](https://emscripten.org), CMake 3.20 or newer, Git and Python 3.10 or newer. On macOS: `brew install emscripten cmake git python`. On Windows, use WSL.

```sh
sh run-web.sh
```

The script builds the game and starts a local server. Leave it running and open <http://localhost:8000/piu.html>. Press `Ctrl+C` to stop it.

To distribute it, you need these six files from `build-web/`:

- `piu.html`, `piu.js`, `piu.wasm`
- `piu_extract.js`, `piu_extract.wasm`, `extract_worker.js`

## macOS

**Requirements:** macOS 11 or newer, Apple's command line tools (`xcode-select --install`), CMake 3.20 or newer and Git. With Homebrew: `brew install cmake git`.

```sh
sh run-mac.sh
```

The app is saved in `Desktop/Open It UP/` and opens by itself. macOS may ask for permission to use the Desktop: accept it. Build details are written to `build-mac.log`.

## Windows

The Windows version is new. It has been built with mingw-w64 and tested under Wine, and it still needs more testing on real machines. The Visual Studio build has not been tested yet.

**From a Windows PC.** Requirements: Windows 10 or newer, [CMake](https://cmake.org/download/) 3.20 or newer, [Git](https://git-scm.com/download/win) and Visual Studio with the **Desktop development with C++** workload. Double-click `run-windows.bat` or run it in a terminal.

```bat
run-windows.bat
```

The game is saved in `Desktop\Open It UP\` and opens by itself. Build details are written to `build-windows.log`.

**From macOS or Linux.** Requirements: CMake, Git and mingw-w64. On macOS: `brew install cmake git mingw-w64`.

```sh
sh run-windows.sh
```

The game is saved in `Desktop/Open It UP (Windows)/`. Copy that folder to a Windows PC and open `Open It UP.exe`. Nothing else needs to be installed.

## Packaging a release

On a Mac with all the requirements above installed, run:

```sh
sh package-release.sh
```

It builds the three versions and leaves these files in `release/`, ready to upload to a GitHub release:

- `OpenItUP-Windows.exe`
- `OpenItUP-macOS.zip`, with the `.app`
- `OpenItUP-Web.zip`, with the six web files

The script stops without packing anything if a program contains the path of the account that built it.

## Tests

The tests do not need any game files.

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

## The 60 FPS limit

The game is limited to 60 frames per second because that is how it was made, and some things depend on the frames drawn. To see what happens without the limit, uncomment `#define PIU_UNLOCK_FPS` in `src/core/frame_limiter.h`. Do it at your own risk.

## Project structure

| Folder | Contents |
|---|---|
| `src/core` | Game rules, screens and clock, independent of the platform |
| `src/render` | OpenGL drawing |
| `src/extractor` | CHD reading and conversion of the game's formats |
| `src/platform` | Web and desktop adapters |
| `assets/icons` | Game icon |
| `cmake` | Building for Windows from macOS or Linux |
| `web` | Launcher and game flow in the browser |
| `tests` | Engine tests |
