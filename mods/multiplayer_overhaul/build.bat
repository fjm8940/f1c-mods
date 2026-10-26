@echo off
REM multiplayer_overhaul build (folder name coop_mode kept). Output goes to %TEMP%\multiplayer_overhaul_build unless OUT is set.
setlocal
cd /d "%~dp0"
if not defined F1C_SDK_DIR set "F1C_SDK_DIR=%~dp0..\..\sdk\tools\sdk"
if not exist "%F1C_SDK_DIR%\f1c_runtime.h" (
  echo F1C SDK not found at "%F1C_SDK_DIR%".
  exit /b 1
)
set "CXX=C:\msys64\mingw32\bin\g++.exe"
if not exist "%CXX%" (
  echo 32-bit MinGW G++ not found at "%CXX%".
  exit /b 1
)
if not defined OUT set "OUT=%TEMP%\multiplayer_overhaul_build"
set "DEVFLAG="
if "%COOP_DEV_TOOLS%"=="1" set "DEVFLAG=-DCOOP_DEV_TOOLS"
if not exist "%OUT%" mkdir "%OUT%"
REM 0.9.0: embed F1C_MOD_META (Lightloader manager metadata and readme)
"C:\msys64\mingw32\bin\windres.exe" -O coff -o "%OUT%\coop_mode_meta.o" "coop_mode_meta.rc"
if errorlevel 1 exit /b 1
"%CXX%" -shared -O2 -Wall -Wextra -Werror -std=c++17 -static -static-libgcc -static-libstdc++ ^
  %DEVFLAG% -I"%F1C_SDK_DIR%" -I"%F1C_SDK_DIR%\..\..\mods\sdk" -I"%CD%" -o "%OUT%\multiplayer_overhaul.dll" "coop_mode.cpp" "coop_menu_logic.c" "%OUT%\coop_mode_meta.o" ^
  -ladvapi32 -lws2_32 -liphlpapi -lcomctl32 -lgdi32 -luser32 -lkernel32
if errorlevel 1 exit /b 1
"%CXX%" -O2 -Wall -Wextra -Werror -std=c++17 -static -static-libgcc -static-libstdc++ ^
  -o "%OUT%\f1c_pid_injector.exe" "f1c_pid_injector.cpp" -lkernel32
if errorlevel 1 exit /b 1
"%CXX%" -shared -O2 -Wall -Wextra -Werror -std=c++17 -static -static-libgcc -static-libstdc++ -Wl,--kill-at ^
  -o "%OUT%\dinput8.dll" "coop_dinput8_proxy.cpp" -lkernel32
if errorlevel 1 exit /b 1
echo Built %OUT%\multiplayer_overhaul.dll
