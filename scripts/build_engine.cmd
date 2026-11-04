@echo off
rem Build the C++ engine with MSVC (Visual Studio 2022) against the attome-ae environment.
rem Usage: scripts\build_engine.cmd [Release|Debug]
rem Set VCVARS to your vcvars64.bat if Visual Studio is installed elsewhere.
setlocal
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Release
if "%MAMBA_ROOT_PREFIX%"=="" set MAMBA_ROOT_PREFIX=%USERPROFILE%\micromamba\root
set PREFIX=%MAMBA_ROOT_PREFIX%\envs\attome-ae\Library
if "%VCVARS%"=="" set VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
call "%VCVARS%" >nul 2>nul
where cl >nul 2>nul || (echo MSVC not found; set VCVARS & exit /b 1)
set PATH=%PREFIX%\bin;%PATH%
cmake -S "%~dp0..\engine" -B "%~dp0..\engine\build" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_PREFIX_PATH="%PREFIX%" -DCMAKE_MAKE_PROGRAM="%PREFIX%\bin\ninja.exe" || exit /b 1
cmake --build "%~dp0..\engine\build" || exit /b 1
