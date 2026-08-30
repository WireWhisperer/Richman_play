@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

set "REPO_URL=https://github.com/WireWhisperer/Richman_play.git"
set "BRANCH=FINAL+TEST"
set "DIR=Richman_play"

echo ============================================
echo  One-click setup: clone %BRANCH% and build
echo ============================================

where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] git not found. Install Git first and add it to PATH.
    pause
    exit /b 1
)

if exist CMakeLists.txt (
    echo [1/3] Already inside the repo, skip clone/update.
) else (
    if exist "%DIR%\.git" (
        echo [1/3] Existing repo found, updating to latest...
        pushd "%DIR%"
        git checkout %BRANCH% >nul 2>&1
        git pull --ff-only >nul 2>&1
        if errorlevel 1 echo [WARN] git pull failed - local changes? - building current code.
        popd
    ) else (
        echo [1/3] Cloning branch %BRANCH% ...
        git clone -b %BRANCH% %REPO_URL% %DIR%
        if errorlevel 1 (
            echo [INFO] Default clone failed, retrying with alternate SSL settings...
            rmdir /s /q "%DIR%" 2>nul
            git -c http.sslbackend=openssl -c http.sslVerify=false clone -b %BRANCH% %REPO_URL% %DIR%
            if errorlevel 1 (
                echo [ERROR] Clone failed. Check your network or proxy settings.
                pause
                exit /b 1
            )
        )
    )
    cd /d "%DIR%"
)

echo [2/3] Detecting compiler...
where gcc >nul 2>&1
if errorlevel 1 goto msvc_build

echo       Using gcc ...
if not exist dist mkdir dist
gcc -std=c17 -O2 -Wall -Wextra -Iinclude -Ithird_party/cJSON -o dist\rich_demo.exe @sources.rsp
if errorlevel 1 goto gcc_fail
copy /Y spec\map.json dist\map.json >nul
set "EXE=dist\rich_demo.exe"
goto build_ok

:msvc_build
echo       gcc not found, trying Visual Studio + CMake ...
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_toolchain
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto no_toolchain
set "CMAKE=%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" goto no_toolchain
echo       Using MSVC via CMake ...
"%CMAKE%" -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=OFF
if errorlevel 1 goto build_fail
"%CMAKE%" --build build --config Release
if errorlevel 1 goto build_fail
set "EXE=build\dist\Release\rich_demo.exe"
goto build_ok

:gcc_fail
echo [ERROR] gcc build failed.
pause
exit /b 1

:build_fail
echo [ERROR] CMake configure or build failed.
pause
exit /b 1

:no_toolchain
echo [ERROR] No compiler found. Install MinGW-w64 or Visual Studio.
pause
exit /b 1

:build_ok
echo [3/3] Build OK: %EXE%
if /i "%1"=="test" (
    echo Running tests...
    "%EXE%" test testcases
    echo Exit code: !ERRORLEVEL!
)
echo.
echo Run game : %EXE%
echo Run tests: %EXE% test testcases
echo ============================================
pause
endlocal
