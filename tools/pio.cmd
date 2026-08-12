@echo off
setlocal
for %%I in ("%~dp0..") do set "CMT_ROOT=%%~sI"
rem Windows caps a process command line at 32767 characters. The ESP32 Arduino
rem framework contributes ~200 -I paths, so a repo-local core directory sitting
rem under a long checkout path pushes the compiler invocation over that limit
rem and gcc fails with "CreateProcess: No such file or directory". Use the
rem repo-local directory only when it already exists, and let an explicit
rem PLATFORMIO_CORE_DIR win; otherwise fall back to PlatformIO's own short
rem default at %%USERPROFILE%%\.platformio.
if not defined PLATFORMIO_CORE_DIR if exist "%CMT_ROOT%\.platformio" set "PLATFORMIO_CORE_DIR=%CMT_ROOT%\.platformio"
rem PlatformIO looks for platformio.ini in the *current* directory, not next to
rem this script. Launching via "Run as administrator" (or any shell started
rem elsewhere) leaves the working directory at C:\Windows\System32 and pio then
rem fails with NotPlatformIOProjectError. Pin the working directory to the repo
rem root; setlocal restores the caller's directory when this script exits.
cd /d "%CMT_ROOT%"
if errorlevel 1 (
  echo Cannot enter repo root "%CMT_ROOT%".
  exit /b 1
)
"%CMT_ROOT%\.venv\Scripts\python.exe" -m platformio %*
exit /b %ERRORLEVEL%
