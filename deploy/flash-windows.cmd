@echo off
setlocal
PowerShell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0flash-windows.ps1" %*
set "CMT_EXIT=%ERRORLEVEL%"
if not "%CMT_EXIT%"=="0" pause
exit /b %CMT_EXIT%
