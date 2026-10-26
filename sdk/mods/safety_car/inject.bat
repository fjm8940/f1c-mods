@echo off
REM Current build = sc2.dll. sc1.dll is the superseded first build (it lacks the
REM arming delay, so the safety car is released into a field still at racing
REM pace and gets wiped out at the pit exit).
REM RESTART THE GAME before injecting a new build - injecting a second DLL while
REM an older one is still loaded leaves two tick threads fighting over the car.
echo. | "%~dp0..\ai_pit_retire\inject.exe" "%~dp0sc2.dll"
