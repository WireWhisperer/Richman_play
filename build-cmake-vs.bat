@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 cmake。请安装 CMake 或从 Visual Studio 安装器勾选 CMake 组件。
    pause
    exit /b 1
)

set VS_GENERATOR=Visual Studio 17 2022
cmake -G "%VS_GENERATOR%" -A x64 >nul 2>&1
if errorlevel 1 (
    set VS_GENERATOR=Visual Studio 16 2019
)

if exist build\CMakeCache.txt (
    echo [提示] 清理旧的 build 目录...
    rmdir /s /q build
)

echo 使用生成器: %VS_GENERATOR%
cmake -S . -B build -G "%VS_GENERATOR%" -A x64 -DBUILD_TESTING=ON
if errorlevel 1 (
    echo.
    echo [失败] 请确认已安装 Visual Studio 的「使用 C++ 的桌面开发」工作负载。
    pause
    exit /b 1
)

cmake --build build --config Debug
if errorlevel 1 (
    pause
    exit /b 1
)

ctest --test-dir build -C Debug --output-on-failure
if errorlevel 1 (
    pause
    exit /b 1
)

echo.
echo [成功] 游戏程序: build\dist\Debug\rich_demo.exe
echo 地图文件:     build\dist\Debug\map.json
echo 双击 rich_demo.exe 即可运行。
echo.
pause
