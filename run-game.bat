@echo off
cd /d "%~dp0"
if exist dist\rich_demo.exe goto run_dist
if exist build\dist\rich_demo.exe goto run_cmake
echo ERROR: rich_demo.exe not found. Run build.bat first.
pause
exit /b 1

:run_dist
cd dist
goto start

:run_cmake
cd build\dist
goto start

:start
if not exist map.json (
  echo ERROR: map.json missing. Rebuild with build.bat
  pause
  exit /b 1
)
echo Running: %CD%\rich_demo.exe
rich_demo.exe
echo.
pause