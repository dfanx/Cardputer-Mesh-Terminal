@echo off
setlocal

set "ROOT=%~dp0.."
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  echo Visual Studio Build Tools not found.
  exit /b 2
)

for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
rem Some installs do not register the VC.Tools.x86.x64 component id even though
rem MSVC is present. Fall back to the latest install and treat the presence of
rem vcvars64.bat as the real test.
if not defined VSROOT (
  for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%I"
)
if not defined VSROOT (
  echo Visual Studio installation not found.
  exit /b 2
)
if not exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" (
  echo MSVC C++ toolchain not found in "%VSROOT%".
  exit /b 2
)

call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

set "OUT=%ROOT%\.pio\build\msvc-native"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"

cl /nologo /std:c++17 /EHsc /W4 /D_CRT_SECURE_NO_WARNINGS ^
  /I"%ROOT%\include" ^
  /I"%ROOT%\.pio\libdeps\native\Unity\src" ^
  "%ROOT%\src\core\types.cpp" ^
  "%ROOT%\src\core\channel_plan.cpp" ^
  "%ROOT%\src\core\wire_protocol.cpp" ^
  "%ROOT%\src\core\secure_frame.cpp" ^
  "%ROOT%\src\core\fragmentation.cpp" ^
  "%ROOT%\src\core\mesh_router.cpp" ^
  "%ROOT%\src\core\sequence_tracker.cpp" ^
  "%ROOT%\src\core\geo.cpp" ^
  "%ROOT%\src\core\message_codec.cpp" ^
  "%ROOT%\test\test_core\test_main.cpp" ^
  "%ROOT%\.pio\libdeps\native\Unity\src\unity.c" ^
  /Fe:test_core.exe
if errorlevel 1 (
  popd
  exit /b 1
)

test_core.exe
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%
