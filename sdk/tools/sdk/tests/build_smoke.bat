@echo off
setlocal
set VC=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207
set SDK=C:\Program Files (x86)\Windows Kits\10
set SDKV=10.0.26100.0
set PATH=%VC%\bin\Hostx64\x86;%SDK%\bin\%SDKV%\x64;%PATH%
set INCLUDE=%VC%\include;%SDK%\Include\%SDKV%\ucrt;%SDK%\Include\%SDKV%\um;%SDK%\Include\%SDKV%\shared
set LIB=%VC%\lib\x86;%SDK%\Lib\%SDKV%\ucrt\x86;%SDK%\Lib\%SDKV%\um\x86

cl /nologo /c /W4 /WX /I "%~dp0.." "%~dp0sdk_smoke.c" /Fo"%TEMP%\f1c_sdk_smoke_c.obj" || exit /b 1
cl /nologo /c /W4 /WX /EHsc /I "%~dp0.." "%~dp0sdk_smoke.cpp" /Fo"%TEMP%\f1c_sdk_smoke_cpp.obj" || exit /b 1
cl /nologo /W4 /WX /I "%~dp0.." "%~dp0sdk_dispatch_smoke.c" /Fe"%TEMP%\f1c_sdk_dispatch_smoke.exe" || exit /b 1
"%TEMP%\f1c_sdk_dispatch_smoke.exe" || exit /b 1
echo SDK C/C++ builds and bounded dispatch smoke passed.
