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

# 优先 C17，编译器不支持则回退 C11（gcc 7.x 及更早没有 -std=c17）。
STD="-std=c17"
PROBE_DIR="$(mktemp -d)"
printf 'int main(void){return 0;}\n' > "$PROBE_DIR/probe.c"
if ! gcc "$STD" -o "$PROBE_DIR/probe" "$PROBE_DIR/probe.c" >/dev/null 2>&1; then
  echo "Note: this compiler has no -std=c17, falling back to -std=c11."
  STD="-std=c11"
fi
rm -rf "$PROBE_DIR"

mkdir -p dist

echo "Compiling..."
gcc "$STD" -O2 -Wall -Wextra \
  -Iinclude -Ithird_party/cJSON \
  -o dist/rich_demo \
  @sources.rsp

cp -f spec/map.json dist/map.json

echo
echo "Build OK: dist/rich_demo"
echo "Run:     ./dist/rich_demo"
echo "   or:   ./run-game.sh"
