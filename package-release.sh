#!/bin/sh
# Builds the macOS, Windows and web versions and packs them in the release folder. Needs a Mac.
set -e
cd "$(dirname "$0")"

fail() {
  echo "$1" >&2
  exit 1
}

# Emscripten needs Python 3.10 or newer, and macOS ships an older python3.
find_python() {
  for candidate in "${EMSDK_PYTHON:-}" python3 /opt/homebrew/bin/python3 /opt/homebrew/opt/python@3*/bin/python3; do
    if [ -n "$candidate" ] && "$candidate" -c 'import sys; sys.exit(sys.version_info < (3, 10))' 2>/dev/null; then
      command -v "$candidate"
      return 0
    fi
  done
  return 1
}

[ "$(uname)" = "Darwin" ] || fail "This script needs a Mac, because the macOS version can only be built there."
xcode-select -p >/dev/null 2>&1 || fail "Apple's command line tools are missing. Install them with: xcode-select --install"
command -v cmake >/dev/null 2>&1 || fail "CMake is missing. Install it with: brew install cmake"
command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1 || fail "The Windows compiler is missing. Install it with: brew install mingw-w64"
command -v emcmake >/dev/null 2>&1 || fail "Emscripten is missing. Install it with: brew install emscripten"
EMSDK_PYTHON=$(find_python) || fail "Python 3.10 or newer was not found. Install it with: brew install python"
export EMSDK_PYTHON

JOBS=$(sysctl -n hw.ncpu)
APP="build-mac/Open It UP.app"
WEB_FILES="piu.html piu.js piu.wasm piu_extract.js piu_extract.wasm extract_worker.js"

rm -rf release
mkdir release

echo "Building the macOS version..."
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DPIU_BUILD_DESKTOP=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build-mac --target open_it_up -j "$JOBS"
codesign --force --deep --sign - "$APP"

echo "Building the Windows version..."
cmake -S . -B build-windows -DCMAKE_BUILD_TYPE=Release -DPIU_BUILD_DESKTOP=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mingw-w64.cmake"
cmake --build build-windows --target open_it_up -j "$JOBS"

echo "Building the web version..."
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j "$JOBS"

# A published program must not reveal the folder or the account it was built in.
check_clean() {
  if grep -a -q "$HOME" "$1"; then
    rm -rf release
    fail "$1 contains the path of this account. Nothing was packed."
  fi
}
check_clean "$APP/Contents/MacOS/Open It UP"
check_clean "$APP/Contents/Info.plist"
check_clean "build-windows/Open It UP.exe"
for file in $WEB_FILES; do
  check_clean "build-web/$file"
done

ditto -c -k --norsrc --noextattr --noacl --keepParent "$APP" release/OpenItUP-macOS.zip
cp "build-windows/Open It UP.exe" release/OpenItUP-Windows.exe
(cd build-web && zip -q -X ../release/OpenItUP-Web.zip $WEB_FILES)

echo
echo "Done. Upload these files to the GitHub release:"
ls -lh release
