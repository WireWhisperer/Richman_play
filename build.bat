@echo off
REM gcc-only build. No CMake. ASCII only for Windows CMD.
cd /d "%~dp0"

if exist "%~dp0tools\mingw64\bin\gcc.exe" set "PATH=%~dp0tools\mingw64\bin;%PATH%"
if exist "C:\mingw64\bin\gcc.exe" set "PATH=C:\mingw64\bin;%PATH%"
if exist "C:\MinGW\bin\gcc.exe" set "PATH=C:\MinGW\bin;%PATH%"
if exist "D:\mingw64\bin\gcc.exe" set "PATH=D:\mingw64\bin;%PATH%"
if exist "D:\Download\MinGW\bin\gcc.exe" set "PATH=D:\Download\MinGW\bin;%PATH%"
if exist "C:\msys64\mingw64\bin\gcc.exe" set "PATH=C:\msys64\mingw64\bin;%PATH%"

where gcc >nul 2>&1
if errorlevel 1 goto no_gcc

echo Using:
gcc --version
echo.

if not exist dist mkdir dist

echo Compiling...
gcc -std=c17 -O2 -Wall -Wextra -Iinclude -Ithird_party/cJSON -o dist/rich_demo.exe @sources.rsp
if errorlevel 1 goto build_fail

copy /Y spec\map.json dist\map.json >nul
if errorlevel 1 goto copy_fail

echo.
echo Build OK: dist\rich_demo.exe
echo Run: run-game.bat
echo.
pause
exit /b 0

:no_gcc
echo ERROR: gcc not found.
echo Add MinGW bin to PATH, e.g. D:\Download\MinGW\bin
echo Then reopen CMD and run build.bat again.
pause
exit /b 1

:build_fail
echo ERROR: compile failed.
pause
exit /b 1

:copy_fail
echo ERROR: copy map.json failed.
pause
exit /b 1