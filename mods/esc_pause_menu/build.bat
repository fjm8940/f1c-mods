@echo off
REM ESC Pause Menu 1.1.4 public build. F1C_SDK_DIR defaults to the repo's sdk\tools\sdk.
setlocal
cd /d "%~dp0"
if not defined F1C_SDK_DIR set "F1C_SDK_DIR=%~dp0..\..\sdk\tools\sdk"
set "CC=C:\msys64\mingw32\bin\gcc.exe"
if not exist "%F1C_SDK_DIR%\f1c_runtime.h" exit /b 1
if not exist "%CC%" exit /b 1
if exist "build\esc_pause_menu.dll" exit /b 9
if not exist "build" mkdir "build"
"C:\msys64\mingw32\bin\windres.exe" -O coff -o "build\esc_pause_menu_meta.o" "esc_pause_menu_meta.rc"
if errorlevel 1 exit /b %errorlevel%
"%CC%" -shared -O2 -s -Wall -Wextra -Werror -Wno-unknown-pragmas -std=c11 -fvisibility=hidden -ffunction-sections -fdata-sections -I"%F1C_SDK_DIR%" -I"%F1C_SDK_DIR%\..\..\mods\sdk" -Wl,--gc-sections -Wl,--build-id=none -o "build\esc_pause_menu.dll" "esc_pause_menu.c" "%F1C_SDK_DIR%\f1c_broker.c" "build\esc_pause_menu_meta.o" -lkernel32
exit /b %errorlevel%
