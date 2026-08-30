#!/usr/bin/env bash
# Run the game on Linux / macOS
set -euo pipefail
cd "$(dirname "$0")"

if [[ -x dist/rich_demo ]]; then
  cd dist
elif [[ -x build/dist/rich_demo ]]; then
  cd build/dist
else
  echo "ERROR: rich_demo not found. Run ./build.sh first."
  exit 1
fi

if [[ ! -f map.json ]]; then
  echo "ERROR: map.json missing. Rebuild with ./build.sh"
  exit 1
fi

echo "Running: $(pwd)/rich_demo"
exec ./rich_demo
