@echo off
rem Sets up Cura for the Kobra Neo Evolution firmware. Close Cura first.
where py >nul 2>nul && (py -3 "%~dp0install_cura.py" %*) || (python "%~dp0install_cura.py" %*)
echo.
pause
