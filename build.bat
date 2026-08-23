@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

where gcc >nul 2>&1
if errorlevel 1 (
    echo [错误] 未找到 gcc。请先安装 MinGW 并将 gcc 加入 PATH。
    echo 下载地址: https://www.mingw-w64.org/
    pause
    exit /b 1
)

if not exist dist mkdir dist

set SRC=src\game.c src\game_mine.c src\game_jail.c src\game_toolShop.c ^
src\usr_action.c src\usr_judge.c src\file_utils.c src\path_utils.c ^
src\case_loader.c src\action_executor.c src\actual_writer.c ^
src\expected_checker.c src\test_runner.c src\manual_ui.c ^
src\player_setup.c src\console.c third_party\cJSON\cJSON.c src\main.c

gcc -std=c17 -Wall -Wextra -Iinclude -Ithird_party\cJSON -o dist\rich_demo.exe %SRC%
if errorlevel 1 (
    echo [错误] 编译失败。
    pause
    exit /b 1
)

copy /Y spec\map.json dist\map.json >nul
if errorlevel 1 (
    echo [错误] 复制地图文件失败，请确认 spec\map.json 存在。
    pause
    exit /b 1
)

echo.
echo [成功] 已生成 dist\rich_demo.exe
echo 双击 dist\rich_demo.exe 即可开始游戏。
echo.
pause
