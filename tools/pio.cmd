@echo off
setlocal
for %%I in ("%~dp0..") do set "CMT_ROOT=%%~sI"
set "PLATFORMIO_CORE_DIR=%CMT_ROOT%\.platformio"
"%CMT_ROOT%\.venv\Scripts\python.exe" -m platformio %*
exit /b %ERRORLEVEL%
