#!/bin/bash
cd "$(dirname "$0")"
if [! -f "dist/rich_demo"];then
    bash build.sh
fi
if [ ! -f "dist/rich_demo" ]; then
    echo "ERROR: build failed"
    read -p "Press [Enter] key to continue..."
    exit 1
fi
echo "Running automated tests..."
dist/rich_demo test testcases
ERR=$?
echo ""
echo "Exit code: $ERR"
read -p "Press [Enter] key to continue..."
exit $ERR
