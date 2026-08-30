#!/usr/bin/env bash
# Run automated tests on Linux / macOS
set -euo pipefail
cd "$(dirname "$0")"

# 始终重新编译，避免拉取代码后仍用旧二进制（曾导致 Linux 上看不见 PASS）
echo "Building before tests..."
./build.sh

BIN=""
if [[ -x dist/rich_demo ]]; then
  BIN=dist/rich_demo
elif [[ -x build/dist/rich_demo ]]; then
  BIN=build/dist/rich_demo
else
  echo "ERROR: build failed"
  exit 1
fi

CASE_DIR="${1:-testcases}"
if [[ ! -d "$CASE_DIR" ]]; then
  echo "ERROR: test case directory not found: $CASE_DIR"
  exit 1
fi

CASE_COUNT=$(find "$CASE_DIR" -maxdepth 1 -type f -name '*.json' ! -name 'map.json' | wc -l | tr -d ' ')

echo "Running automated tests..."
echo "Binary:  $BIN"
if command -v git >/dev/null 2>&1 && git rev-parse --short HEAD >/dev/null 2>&1; then
  echo "Commit:  $(git rev-parse --short HEAD)"
fi
echo "Cases:   $CASE_DIR ($CASE_COUNT suite files)"
echo

"$BIN" test "$CASE_DIR"
ERR=$?

echo
echo "Exit code: $ERR"
exit "$ERR"
