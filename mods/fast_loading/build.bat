@echo off
REM fast_loading 1.0.0 release build (Lightloader plug-in).
REM Output goes to %TEMP%\fastload_rc1 (never into a game folder).
REM Refuses to overwrite an existing output DLL; move the old one aside by hand first.
setlocal
set "PATH=C:\msys64\mingw32\bin;C:\msys64\usr\bin;%PATH%"
cd /d "%~dp0"
set "CC=C:\msys64\mingw32\bin\gcc.exe"
set "RC=C:\msys64\mingw32\bin\windres.exe"
if not defined OUT set "OUT=%TEMP%\fastload_rc1"
if not exist "%CC%" exit /b 1
if not exist "%RC%" exit /b 1
if not exist "C:\msys64\mingw32\lib\libdeflate.a" exit /b 1
if not exist "%OUT%" mkdir "%OUT%"
if exist "%OUT%\fast_loading.dll" exit /b 9
"%RC%" -I "%~dp0." -O coff -o "%OUT%\fast_loading_meta.o" "fast_loading_meta.rc"
if errorlevel 1 exit /b %errorlevel%
"%CC%" -shared -O2 -s -Wall -Wextra -Werror -std=gnu11 -static-libgcc -fvisibility=hidden -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--build-id=none -o "%OUT%\fast_loading.dll" "fast_loading.c" "%OUT%\fast_loading_meta.o" -Wl,-Bstatic -ldeflate -Wl,-Bdynamic -lkernel32 -luser32 -ladvapi32
exit /b %errorlevel%
