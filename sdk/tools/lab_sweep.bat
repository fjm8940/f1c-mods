@echo off
REM lab_sweep.bat - double-click to run the persistence sweep.
REM Game must be running and IN A SESSION (green flag, not paused).
REM Reading/writing another process's memory needs admin, so this relaunches
REM itself elevated (one UAC prompt). Output is teed to tools\lab_sweep.log so
REM the run can be inspected afterwards.
REM
REM   lab_sweep.bat          read-only baseline pass (no writes at all)
REM   lab_sweep.bat --go     the real classifier (writes sentinels, restores each)
cd /d "%~dp0\.."
set PY=%USERPROFILE%\AppData\Local\Python\bin\python3.exe
powershell -NoProfile -Command ^
  "Start-Process -FilePath 'cmd.exe' -ArgumentList '/k','\"%PY%\" tools\lab.py sweep %* 2>&1 ^| tee tools\lab_sweep.log' -Verb RunAs -WorkingDirectory '%CD%'"
