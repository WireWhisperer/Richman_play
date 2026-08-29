#!/usr/bin/env bash
# gcc-only build for Linux / macOS (no CMake required)
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v gcc >/dev/null 2>&1; then
  echo "ERROR: gcc not found. Install with: sudo apt install build-essential"
  exit 1
fi

echo "Using:"
gcc --version | head -n 1
echo

mkdir -p dist

echo "Compiling..."
gcc -std=c17 -O2 -Wall -Wextra \
  -Iinclude -Ithird_party/cJSON \
  -o dist/rich_demo \
  @sources.rsp

cp -f spec/map.json dist/map.json

echo
echo "Build OK: dist/rich_demo"
echo "Run:     ./dist/rich_demo"
echo "   or:   ./run-game.sh"
