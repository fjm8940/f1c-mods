@echo off
REM NOTE: the sibling inject.bat points at mods\red-flag-dll\inject.exe, which
REM does not exist in this corpus. The working injector is in ai_pit_retire.
echo. | "%~dp0..\ai_pit_retire\inject.exe" "%~dp0gr31.dll"
