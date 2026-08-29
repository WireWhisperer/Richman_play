@echo off
cd /d "%~dp0"
if not exist dist\rich_demo.exe call build.bat
if not exist dist\rich_demo.exe (
  echo ERROR: build failed
  pause
  exit /b 1
)
echo Running automated tests...
dist\rich_demo.exe test testcases
set ERR=%ERRORLEVEL%
echo.
echo Exit code: %ERR%
pause
exit /b %ERR%