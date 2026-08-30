@echo off
setlocal
cd /d "%~dp0"

if exist "D:\Download\MinGW\bin\gcc.exe" set "PATH=D:\Download\MinGW\bin;%PATH%"
if exist "C:\Program Files\CMake\bin\cmake.exe" set "PATH=C:\Program Files\CMake\bin;%PATH%"

where gcc >nul 2>&1
if errorlevel 1 (
    echo ERROR: gcc not found.
    echo Add MinGW to PATH, e.g. D:\Download\MinGW\bin
    pause
    exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: cmake not found.
    echo Install CMake and add it to PATH.
    pause
    exit /b 1
)

cmake --preset windows-mingw
if errorlevel 1 (
    cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    if errorlevel 1 (
        pause
        exit /b 1
    )
)

cmake --build build
if errorlevel 1 (
    pause
    exit /b 1
)

echo.
echo Build OK: build\dist\rich_demo.exe
echo Run: run-game.bat
echo.
pause
