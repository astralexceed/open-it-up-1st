#!/bin/sh
# Builds the macOS application, saves it in a folder on the Desktop and opens it.
cd "$(dirname "$0")" || exit 1

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Apple's command line tools are missing. Install them with: xcode-select --install" >&2
  exit 1
fi

LOG="$PWD/build-mac.log"
STATUS="$PWD/.build-mac.status"
{
  cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DPIU_BUILD_DESKTOP=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 &&
    cmake --build build-mac --target open_it_up -j "$(sysctl -n hw.ncpu)"
  echo $? > "$STATUS"
} 2>&1 | tee "$LOG"

if [ "$(cat "$STATUS")" != "0" ]; then
  rm -f "$STATUS"
  echo
  echo "The build failed. See build-mac.log for details." >&2
  exit 1
fi
rm -f "$STATUS"

# The game and its extracted files live together in this folder.
DEST="$HOME/Desktop/Open It UP"
APP="$DEST/Open It UP.app"
mkdir -p "$DEST"
rm -rf "$APP"
ditto "build-mac/Open It UP.app" "$APP"
codesign --force --deep --sign - "$APP"
echo
echo "Done. The game was saved in:"
echo "  $DEST"
echo "To play, open \"Open It UP.app\". Its extracted files will be kept next to it, in the Data folder."
open "$APP"
