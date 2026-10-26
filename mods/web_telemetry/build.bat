@echo off
setlocal
cd /d "%~dp0"
if not defined F1C_SDK_DIR set "F1C_SDK_DIR=%~dp0..\..\sdk\tools\sdk"
if not defined TELEMETRY_BUILD_DIR set "TELEMETRY_BUILD_DIR=%TEMP%\f1c-web-telemetry-4.4.0"
python verify_payload.py "%TELEMETRY_BUILD_DIR%" --preflight
if errorlevel 1 exit /b 1
if not exist "%TELEMETRY_BUILD_DIR%" mkdir "%TELEMETRY_BUILD_DIR%"
"C:\msys64\mingw32\bin\gcc.exe" -shared -O2 -static -static-libgcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wl,--no-insert-timestamp -I"%F1C_SDK_DIR%" -I"%F1C_SDK_DIR%\..\..\mods\sdk" -o "%TELEMETRY_BUILD_DIR%\web_telemetry.dll" web_telemetry.c legacy_formats.c sha256.c "%F1C_SDK_DIR%\f1c_broker.c" -lws2_32 -lkernel32
if errorlevel 1 exit /b 1
copy /y workbench_ui.js "%TELEMETRY_BUILD_DIR%\workbench_ui.js" >nul
copy /y telemetry_viewer.js "%TELEMETRY_BUILD_DIR%\telemetry_viewer.js" >nul
python verify_sidecar.py "%TELEMETRY_BUILD_DIR%"
if errorlevel 1 exit /b 1
rem BACKEND 2 review fix 4: candidate payload staging lives in %TEMP%, never in the workspace.
if not exist "%TELEMETRY_BUILD_DIR%\payload" mkdir "%TELEMETRY_BUILD_DIR%\payload"
copy /y "%TELEMETRY_BUILD_DIR%\web_telemetry.dll" "%TELEMETRY_BUILD_DIR%\payload\web_telemetry.dll" >nul
copy /y "%TELEMETRY_BUILD_DIR%\workbench_ui.js" "%TELEMETRY_BUILD_DIR%\payload\workbench_ui.js" >nul
copy /y "%TELEMETRY_BUILD_DIR%\telemetry_viewer.js" "%TELEMETRY_BUILD_DIR%\payload\telemetry_viewer.js" >nul
copy /y web_telemetry.ini "%TELEMETRY_BUILD_DIR%\payload\web_telemetry.ini" >nul
copy /y web_telemetry.dll.mod.json "%TELEMETRY_BUILD_DIR%\payload\web_telemetry.dll.mod.json" >nul
python verify_payload.py "%TELEMETRY_BUILD_DIR%"
if errorlevel 1 exit /b 1
if "%TELEMETRY_TESTS%"=="1" (
  python audit_packaging_test.py "%TELEMETRY_BUILD_DIR%"
  if errorlevel 1 exit /b 1
  "C:\msys64\mingw32\bin\gcc.exe" -O1 -static -static-libgcc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -Wl,--no-insert-timestamp -Wl,--image-base,0x10000000 -Wl,--export-all-symbols -I"%F1C_SDK_DIR%" -I"%F1C_SDK_DIR%\..\..\mods\sdk" -o "%TELEMETRY_BUILD_DIR%\runtime_test.exe" runtime_test.c legacy_formats.c sha256.c "%F1C_SDK_DIR%\f1c_broker.c" -lws2_32 -lkernel32
  if errorlevel 1 exit /b 1
  python test_telemetry.py "%TELEMETRY_BUILD_DIR%"
  if errorlevel 1 exit /b 1
)
echo Built %TELEMETRY_BUILD_DIR%\web_telemetry.dll
