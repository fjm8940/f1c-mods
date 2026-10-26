@echo off
REM NOTE: the original inject.bat points at mods\red-flag-dll\inject.exe, which
REM does not exist in this corpus. The working injector is in ai_pit_retire.
echo. | "%~dp0..\ai_pit_retire\inject.exe" "%~dp0gr37.dll"
