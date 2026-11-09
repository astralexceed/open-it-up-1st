#!/bin/sh
# Builds the web version and serves it on http://localhost:8000/piu.html
set -e
cd "$(dirname "$0")"

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

EMSDK_PYTHON=$(find_python) || {
  echo "Python 3.10 or newer was not found. Install it with: brew install python" >&2
  exit 1
}
export EMSDK_PYTHON

emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
echo
echo "Done. Open http://localhost:8000/piu.html"
echo "To stop it: Ctrl+C"
"$EMSDK_PYTHON" -m http.server 8000 --bind 127.0.0.1 -d build-web
