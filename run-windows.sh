#!/bin/sh
# Builds the Windows version from macOS or Linux and saves it in a folder on the Desktop.
cd "$(dirname "$0")" || exit 1

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
  echo "The Windows compiler is missing. On macOS, install it with: brew install mingw-w64" >&2
  exit 1
fi

LOG="$PWD/build-windows.log"
STATUS="$PWD/.build-windows.status"
{
  cmake -S . -B build-windows -DCMAKE_BUILD_TYPE=Release -DPIU_BUILD_DESKTOP=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mingw-w64.cmake" &&
    cmake --build build-windows --target open_it_up -j "$(getconf _NPROCESSORS_ONLN)"
  echo $? > "$STATUS"
} 2>&1 | tee "$LOG"

if [ "$(cat "$STATUS")" != "0" ]; then
  rm -f "$STATUS"
  echo
  echo "The build failed. See build-windows.log for details." >&2
  exit 1
fi
rm -f "$STATUS"

DEST="$HOME/Desktop/Open It UP (Windows)"
mkdir -p "$DEST"
cp "build-windows/Open It UP.exe" "$DEST/Open It UP.exe"
echo
echo "Done. The Windows version was saved in:"
echo "  $DEST"
echo "Copy that folder to a Windows PC and open \"Open It UP.exe\"."
