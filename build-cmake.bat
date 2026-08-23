@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

where gcc >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 gcc。请安装 MinGW 并将 bin 目录加入 PATH。
    echo 示例: set PATH=D:\Download\MinGW\bin;%%PATH%%
    pause
    exit /b 1
)

where cmake >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 cmake。请安装 CMake 并加入 PATH。
    pause
    exit /b 1
)

if exist build\CMakeCache.txt (
    for /f "tokens=2 delims==" %%G in ('findstr /B "CMAKE_GENERATOR:INTERNAL" build\CMakeCache.txt') do set OLD_GEN=%%G
    echo !OLD_GEN! | findstr /C:"MinGW Makefiles" >nul
    if errorlevel 1 (
        echo [提示] 检测到旧的 CMake 缓存，正在清理 build 目录...
        rmdir /s /q build
    )
)

cmake --preset windows-mingw
if errorlevel 1 (
    echo.
    echo [备选] 手动命令:
    echo   cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
    pause
    exit /b 1
)

cmake --build build
if errorlevel 1 (
    pause
    exit /b 1
)

ctest --test-dir build --output-on-failure
if errorlevel 1 (
    pause
    exit /b 1
)

echo.
echo [成功] 游戏程序: build\dist\rich_demo.exe
echo 双击即可运行。
echo.
pause
