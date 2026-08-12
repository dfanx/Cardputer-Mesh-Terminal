@echo off
setlocal

set "ROOT=%~dp0.."
set "VSWHERE=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :wsl_fallback

for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
rem Some installs do not register the VC.Tools.x86.x64 component id even though
rem MSVC is present. Fall back to the latest install and treat the presence of
rem the vcvars scripts as the real test.
if not defined VSROOT (
  for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -property installationPath`) do set "VSROOT=%%I"
)
if not defined VSROOT goto :wsl_fallback
if not exist "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" goto :wsl_fallback
rem vcvars64.bat delegates to vcvarsall.bat; a VS install that is missing the
rem C++ workload keeps the former and drops the latter along with the STL
rem headers, so check for it explicitly instead of failing inside vcvars.
if not exist "%VSROOT%\VC\Auxiliary\Build\vcvarsall.bat" goto :wsl_fallback

call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

set "OUT=%ROOT%\.pio\build\msvc-native"
if not exist "%OUT%" mkdir "%OUT%"
pushd "%OUT%"

cl /nologo /std:c++17 /EHsc /W4 /utf-8 /D_CRT_SECURE_NO_WARNINGS ^
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

:wsl_fallback
rem No usable MSVC. The core sources are plain C++17, so the default WSL distro
rem can run the same suite as long as g++ is installed there.
where wsl >nul 2>&1
if errorlevel 1 (
  echo No usable MSVC C++ toolchain and no WSL. Install the "Desktop development
  echo with C++" workload in Visual Studio Installer, or install WSL plus g++.
  exit /b 2
)
wsl -- test -x /usr/bin/g++ >nul 2>&1
if errorlevel 1 (
  echo No usable MSVC C++ toolchain, and g++ is missing in the default WSL distro.
  echo Install it with: wsl -- sudo apt-get install -y g++
  exit /b 2
)
echo MSVC C++ toolchain unavailable; running the suite in WSL instead.
for /f "usebackq tokens=*" %%I in (`wsl -- wslpath -a "%~dp0test-native-wsl.sh"`) do set "WSLSCRIPT=%%I"
if not defined WSLSCRIPT (
  echo Could not resolve the WSL path to tools\test-native-wsl.sh.
  exit /b 2
)
wsl -- bash "%WSLSCRIPT%"
exit /b %ERRORLEVEL%
