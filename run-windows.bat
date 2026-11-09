@echo off
rem Builds the Windows version on Windows and saves it in a folder on the Desktop.
rem Needs CMake, Git and the C++ tools of Visual Studio.
setlocal
cd /d "%~dp0"

where cmake >nul 2>nul || (echo CMake is missing: https://cmake.org/download/ & pause & exit /b 1)
where git >nul 2>nul || (echo Git is missing: https://git-scm.com/download/win & pause & exit /b 1)

set "LOG=%CD%\build-windows.log"
echo Building. The first build takes a few minutes...
cmake -S . -B build-windows -DCMAKE_BUILD_TYPE=Release -DPIU_BUILD_DESKTOP=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 > "%LOG%" 2>&1 || goto failed
cmake --build build-windows --config Release --target open_it_up --parallel >> "%LOG%" 2>&1 || goto failed

set "EXE=build-windows\Release\Open It UP.exe"
if not exist "%EXE%" set "EXE=build-windows\Open It UP.exe"
if not exist "%EXE%" goto failed

for /f "usebackq delims=" %%d in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('Desktop')"`) do set "DESKTOP=%%d"
set "DEST=%DESKTOP%\Open It UP"
if not exist "%DEST%" mkdir "%DEST%"
copy /y "%EXE%" "%DEST%\Open It UP.exe" >nul || goto failed

echo.
echo Done. The game was saved in:
echo   %DEST%
echo To play, open "Open It UP.exe". Its extracted files will be kept next to it, in the Data folder.
start "" "%DEST%\Open It UP.exe"
pause
exit /b 0

:failed
echo.
echo The build failed. See build-windows.log for details.
pause
exit /b 1
