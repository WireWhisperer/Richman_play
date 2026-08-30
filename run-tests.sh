#!/usr/bin/env bash
# Run automated tests on Linux / macOS
set -euo pipefail
cd "$(dirname "$0")"

BIN=""
if [[ -x dist/rich_demo ]]; then
  BIN=dist/rich_demo
elif [[ -x build/dist/rich_demo ]]; then
  BIN=build/dist/rich_demo
else
  echo "rich_demo not found, building..."
  ./build.sh
  if [[ -x dist/rich_demo ]]; then
    BIN=dist/rich_demo
  else
    echo "ERROR: build failed"
    exit 1
  fi
fi

CASE_DIR="${1:-testcases}"
if [[ ! -d "$CASE_DIR" ]]; then
  echo "ERROR: test case directory not found: $CASE_DIR"
  exit 1
fi

echo "Running automated tests..."
echo "Binary:  $BIN"
echo "Cases:   $CASE_DIR"
echo

"$BIN" test "$CASE_DIR"
ERR=$?

echo
echo "Exit code: $ERR"
exit "$ERR"
