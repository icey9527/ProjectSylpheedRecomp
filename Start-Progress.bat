@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
  py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3, 11) else 1)" >nul 2>nul
  if not errorlevel 1 (
    py -3 "%~dp0scripts\progress_server.py" --open
    goto finish
  )
)
where python >nul 2>nul
if not errorlevel 1 (
  python -c "import sys; sys.exit(0 if sys.version_info >= (3, 11) else 1)" >nul 2>nul
  if not errorlevel 1 (
    python "%~dp0scripts\progress_server.py" --open
    goto finish
  )
)
echo Python 3.11+ was not found. Install Python and enable its launcher or PATH.
pause
exit /b 1
:finish
if errorlevel 1 (
  echo Dashboard stopped with an error. See the message above.
  pause
  exit /b 1
)
endlocal
