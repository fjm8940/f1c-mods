@echo off


setlocal
if not defined PIT_OUT set "PIT_OUT=%TEMP%\f1c-pitstop-1.1.4-final"


cd /d "%~dp0"


if exist "%PIT_OUT%\pitstop_failures.dll" exit /b 9
if not exist "%PIT_OUT%" mkdir "%PIT_OUT%"


set "PATH=C:\msys64\mingw32\bin;%PATH%"


set "PIT_BUILD=%PIT_OUT%"
set "CFLAGS=-O2 -Wall -Wextra -Werror -std=c11"
windres -I. "pitstop_failures_meta.rc" -O coff -o "%PIT_BUILD%\pitstop_failures_meta.o"
if errorlevel 1 exit /b 1


gcc %CFLAGS% -I"%F1C_SDK_DIR%" -I"src" -I"%F1C_SDK_DIR%\..\.." -c "source\policy_dev.c" -o "%PIT_BUILD%\policy.o"


if errorlevel 1 exit /b 1


gcc %CFLAGS% -I"%F1C_SDK_DIR%" -I"src" -I"%F1C_SDK_DIR%\..\.." -c "source\session_dev.c" -o "%PIT_BUILD%\session.o"


if errorlevel 1 exit /b 1


gcc %CFLAGS% -I"%F1C_SDK_DIR%" -I"src" -I"%F1C_SDK_DIR%\..\.." -c "src\pit_failure_policy_v3.c" -o "%PIT_BUILD%\policy_v3.o"


if errorlevel 1 exit /b 1


gcc %CFLAGS% -I"%F1C_SDK_DIR%" -I"src" -I"%F1C_SDK_DIR%\..\.." -c "src\pit_failure_session_v3.c" -o "%PIT_BUILD%\session_v3.o"


if errorlevel 1 exit /b 1


gcc %CFLAGS% -I"%F1C_SDK_DIR%" -I"src" -I"%F1C_SDK_DIR%\..\.." -c "%F1C_SDK_DIR%\f1c_broker.c" -o "%PIT_BUILD%\broker.o"


if errorlevel 1 exit /b 1


g++ -shared -s -DPIT_LIVE_PUBLIC_BUILD=1 -O2 -Wall -Wextra -Werror -std=c++17 -static -I"%F1C_SDK_DIR%" -I"%F1C_SDK_DIR%\..\..\mods\sdk" -I"src" -I"%F1C_SDK_DIR%\..\.." -o "%PIT_BUILD%\pitstop_failures.dll" source\live_client.cpp "%PIT_BUILD%\policy.o" "%PIT_BUILD%\session.o" "%PIT_BUILD%\policy_v3.o" "%PIT_BUILD%\session_v3.o" "%PIT_BUILD%\broker.o" "%PIT_BUILD%\pitstop_failures_meta.o" -lws2_32 -lwinmm


if errorlevel 1 exit /b 1


g++ -shared -s -O2 -Wall -Wextra -Werror -std=c++17 -static -I"%F1C_SDK_DIR%" -I"src" -I"%F1C_SDK_DIR%\..\.." -o "%PIT_BUILD%\pitstop_compat.dll" source\compat_helper.cpp


if errorlevel 1 exit /b 1


g++ -O2 -Wall -Wextra -Werror -std=c++17 -static -I"%F1C_SDK_DIR%" -I"src" -I"%F1C_SDK_DIR%\..\.." -Wl,--image-base,0x10000000,--disable-dynamicbase -o "%PIT_BUILD%\compat_probe.exe" source\compat_probe.cpp


if errorlevel 1 exit /b 1


g++ -DPIT_LIVE_PUBLIC_BUILD=1 -O2 -Wall -Wextra -Werror -std=c++17 -static -I"%F1C_SDK_DIR%" -I"%F1C_SDK_DIR%\..\..\mods\sdk" -I"src" -I"%F1C_SDK_DIR%\..\.." -o "%PIT_BUILD%\live_tests.exe" source\live_tests.cpp "%PIT_BUILD%\policy.o" "%PIT_BUILD%\session.o" "%PIT_BUILD%\policy_v3.o" "%PIT_BUILD%\session_v3.o" "%PIT_BUILD%\broker.o" -lws2_32 -lwinmm


if errorlevel 1 exit /b 1


"%PIT_BUILD%\live_tests.exe" > "%PIT_BUILD%\live_tests.txt"


if errorlevel 1 exit /b 1



g++ -O2 -Wall -Wextra -Werror -std=c++17 -static -o "%PIT_BUILD%\host_smoke.exe" source\host_smoke.cpp
if errorlevel 1 exit /b 1
