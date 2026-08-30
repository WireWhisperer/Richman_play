@echo off
REM One-click gcc build. No CMake, no manual toolchain install.
REM ASCII only, so it stays readable under any Windows code page.
REM
REM Compiler lookup order:
REM   1. tools\w64devkit\bin\gcc.exe   portable toolchain, preferred
REM   2. gcc found in PATH
REM   3. auto-download w64devkit into tools\  (77 MB, once, needs network)
REM
REM Every candidate is checked by actually compiling AND linking a tiny
REM program, so a broken toolchain never slips through.
REM
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "PROBE_OK=0"
set "GCC_BIN="
set "GCC_DIR="

REM ---- 1) portable toolchain under tools\ ----
if exist "%~dp0tools\w64devkit\bin\gcc.exe" call :probe "%~dp0tools\w64devkit\bin\gcc.exe"

REM ---- 2) gcc already on PATH ----
if "%PROBE_OK%"=="0" (
    where gcc >nul 2>&1
    if not errorlevel 1 (
        for /f "delims=" %%G in ('where gcc') do (
            if "!PROBE_OK!"=="0" call :probe "%%G"
        )
    )
)

REM ---- 3) bootstrap a portable toolchain ----
if "%PROBE_OK%"=="0" (
    call :bootstrap
)

if "%PROBE_OK%"=="0" goto :no_gcc

set "PATH=%GCC_DIR%;%PATH%"

echo Using:
"%GCC_BIN%" --version
echo.

REM ---- pick the newest C standard this compiler accepts ----
set "STD=-std=c17"
> "%TEMP%\richman_stdprobe.c" echo int main(void){return 0;}
"%GCC_BIN%" -std=c17 -o "%TEMP%\richman_stdprobe.exe" "%TEMP%\richman_stdprobe.c" >nul 2>&1
if errorlevel 1 (
    echo Note: no -std=c17 support here, falling back to -std=c11.
    set "STD=-std=c11"
)
del /Q "%TEMP%\richman_stdprobe.c" "%TEMP%\richman_stdprobe.exe" >nul 2>&1

if not exist dist mkdir dist

echo Compiling...
"%GCC_BIN%" %STD% -O2 -Wall -Wextra -Iinclude -Ithird_party/cJSON -o dist/rich_demo.exe @sources.rsp
if errorlevel 1 goto :build_fail

copy /Y spec\map.json dist\map.json >nul
if errorlevel 1 goto :copy_fail

echo.
echo Build OK: dist\rich_demo.exe
echo Run: run-game.bat
echo.
call :pause_if_double_clicked
exit /b 0

REM ==================== subroutines ====================

:probe
set "CAND=%~1"
echo Trying: %CAND%
"%CAND%" -dumpversion >nul 2>&1
if errorlevel 1 exit /b 1
> "%TEMP%\richman_probe.c" echo int main(void){return 0;}
"%CAND%" -o "%TEMP%\richman_probe.exe" "%TEMP%\richman_probe.c" >nul 2>&1
if errorlevel 1 (
    del /Q "%TEMP%\richman_probe.c" "%TEMP%\richman_probe.exe" >nul 2>&1
    exit /b 1
)
del /Q "%TEMP%\richman_probe.c" "%TEMP%\richman_probe.exe" >nul 2>&1
set "GCC_BIN=%CAND%"
set "GCC_DIR=%~dp1"
set "PROBE_OK=1"
exit /b 0

:bootstrap
echo.
echo No working C compiler found on this machine.
echo Preparing a portable toolchain (w64devkit, about 77 MB) into:
echo   %~dp0tools
echo This runs once and needs an internet connection.
echo.
if not exist "%~dp0tools" mkdir "%~dp0tools"
set "W64ZIP=%~dp0tools\w64devkit-1.23.0.zip"
if not exist "%W64ZIP%" (
    echo Downloading w64devkit 1.23.0 ...
    curl -L -f --progress-bar -o "%W64ZIP%" "https://github.com/skeeto/w64devkit/releases/download/v1.23.0/w64devkit-1.23.0.zip"
    if errorlevel 1 (
        echo Download failed.
        if exist "%W64ZIP%" del /Q "%W64ZIP%"
        exit /b 1
    )
) else (
    echo Reusing existing download: %W64ZIP%
)
echo Extracting ...
tar -xf "%W64ZIP%" -C "%~dp0tools"
if errorlevel 1 (
    echo Extraction failed.
    exit /b 1
)
call :probe "%~dp0tools\w64devkit\bin\gcc.exe"
exit /b 0

:pause_if_double_clicked
echo %CMDCMDLINE% | findstr /i /l "%~nx0" >nul
if not errorlevel 1 pause
exit /b 0

:no_gcc
echo.
echo ERROR: no working C compiler, and the automatic setup did not finish.
echo.
echo Pick one of these, then re-run build.bat:
echo   1. winget install -e --id BrechtSanders.WinLibs.POSIX.UCRT
echo   2. Install MinGW-w64 by hand and put its bin directory on PATH
echo   3. Download the file below into tools\ and re-run build.bat:
echo      https://github.com/skeeto/w64devkit/releases/download/v1.23.0/w64devkit-1.23.0.zip
echo.
call :pause_if_double_clicked
exit /b 1

:build_fail
echo.
echo ERROR: compile failed.
echo.
call :pause_if_double_clicked
exit /b 1

:copy_fail
echo.
echo ERROR: copy map.json failed.
echo.
call :pause_if_double_clicked
exit /b 1
