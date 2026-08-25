@echo off
setlocal
cd /d "%~dp0"

if exist "D:\Download\MinGW\bin\gcc.exe" set "PATH=D:\Download\MinGW\bin;%PATH%"

where gcc >nul 2>&1
if errorlevel 1 (
    echo ERROR: gcc not found.
    echo Add MinGW to PATH, e.g. D:\Download\MinGW\bin
    pause
    exit /b 1
)

if not exist dist mkdir dist

gcc -std=c17 -Wall -Wextra -Iinclude -Ithird_party\cJSON -o dist\rich_demo.exe ^
src\game.c src\game_mine.c src\game_jail.c src\game_toolShop.c ^
src\game_property.c src\game_giftShop.c ^
src\usr_action.c src\usr_judge.c src\file_utils.c src\path_utils.c ^
src\case_loader.c src\action_executor.c src\actual_writer.c ^
src\expected_checker.c src\test_runner.c src\manual_ui.c ^
src\player_setup.c src\console.c third_party\cJSON\cJSON.c src\main.c

if errorlevel 1 (
    echo ERROR: build failed.
    pause
    exit /b 1
)

copy /Y spec\map.json dist\map.json >nul
if errorlevel 1 (
    echo ERROR: copy map.json failed.
    pause
    exit /b 1
)

echo.
echo Build OK: dist\rich_demo.exe
echo.
pause
