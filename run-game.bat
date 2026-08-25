@echo off
cd /d "%~dp0"

set "GAME_EXE="
set "GAME_DIR="

if exist "build\dist\rich_demo.exe" (
    set "GAME_EXE=build\dist\rich_demo.exe"
    set "GAME_DIR=build\dist"
)

if not defined GAME_EXE if exist "dist\rich_demo.exe" (
    set "GAME_EXE=dist\rich_demo.exe"
    set "GAME_DIR=dist"
)

if not defined GAME_EXE (
    echo ERROR: rich_demo.exe not found.
    echo Please run build-cmake.bat first.
    pause
    exit /b 1
)

echo Running: %CD%\%GAME_EXE%
cd /d "%~dp0%GAME_DIR%"

if not exist map.json (
    echo ERROR: map.json not found in %CD%
    pause
    exit /b 1
)

rich_demo.exe
pause
