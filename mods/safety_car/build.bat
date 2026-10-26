@echo off
REM Safety Car 0.10.95: no licence/activation, no usage reports.
REM SC_NO_USAGE stubs out every scu_* function (no usage file, no report path).
REM Public free build (GPL-3.0). Released through Lightloader.
setlocal
cd /d "%~dp0"
if not defined F1C_SDK_DIR set "F1C_SDK_DIR=%~dp0..\..\sdk\tools\sdk"
if not exist "%F1C_SDK_DIR%\f1c_runtime.h" exit /b 8
if exist "build\safetycar.dll" exit /b 9
if not exist build mkdir build
"C:\msys64\mingw32\bin\windres.exe" -O coff -I. -o "build\sc_meta.o" safetycar_meta.rc
if errorlevel 1 exit /b 1
"C:\msys64\mingw32\bin\gcc.exe" -shared -Wl,--exclude-all-symbols -O2 -static-libgcc -D_WIN32_WINNT=0x0601 -DSC_RELEASE_SEQUENCE=1 -DSC_NO_USAGE -Wall -Wextra -Wno-unused-function -msse2 -std=c11 -I"%F1C_SDK_DIR%\..\..\mods\sdk" -I"%F1C_SDK_DIR%" -o "build\safetycar.dll" "safety_car_guardrails.c" "target_check.c" "build\sc_meta.o" "%F1C_SDK_DIR%\f1c_broker.c" -lkernel32 -luser32 -ladvapi32
if errorlevel 1 exit /b 1
echo Safety Car 0.10.95 compiled: build\safetycar.dll.
