@echo off
REM AI Live Form GUI - double-click to run. Game must be running, in a session.
REM Memory access needs admin: this relaunches itself elevated (one UAC prompt).
cd /d "%~dp0\.."
powershell -NoProfile -Command "Start-Process pythonw -ArgumentList 'tools\ai_live_form_gui.py' -Verb RunAs -WorkingDirectory '%CD%'"
