@echo off
REM Mechanical Failures 1.0.7 release build (engine family only).
REM Sources: .\src (snapshot of dll\ at rc time, incl. the Lightloader broker
REM for game-thread commentary posts). Output: .\build\mechanical_failures.dll
REM No MF_DEV_TOOLS: the developer AI override/gear detour is compiled out.
setlocal
cd /d "%~dp0"
set "GCC=C:\msys64\mingw32\bin\gcc.exe"
set "WINDRES=C:\msys64\mingw32\bin\windres.exe"
set "CFLAGS=-O2 -Wall -Wextra -Werror -DMF_MERGED=1 -DMF_ENGINE_ONLY=1 -Isrc -I"%F1C_SDK_DIR%\..\..\mods\sdk" -I"%F1C_SDK_DIR%""
if not exist build mkdir build
"%WINDRES%" -O coff -Isrc -o build\meta.o mechanical_failures_meta.rc || exit /b 1
"%GCC%" -c %CFLAGS% -DDllMain=DllMain_engine -o build\engine.o src\engine_failure.c || exit /b 1
"%GCC%" -c %CFLAGS% -o build\broker.o src\f1c_broker.c || exit /b 1
"%GCC%" -c %CFLAGS% -o build\host.o src\mechanical_failures.c || exit /b 1
"%GCC%" -shared -s -Wl,--no-insert-timestamp -o build\mechanical_failures.dll build\host.o build\engine.o build\broker.o build\meta.o -lkernel32 -lwinmm || exit /b 1
echo Built %CD%\build\mechanical_failures.dll
