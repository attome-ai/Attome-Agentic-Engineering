@echo off
rem Build the engine into engine\build-dev (used while engine\build\structura.exe is busy, e.g. during a benchmark).
setlocal
if "%MAMBA_ROOT_PREFIX%"=="" set MAMBA_ROOT_PREFIX=%USERPROFILE%\micromamba\root
set PREFIX=%MAMBA_ROOT_PREFIX%\envs\attome-ae\Library
if "%VCVARS%"=="" set VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
call "%VCVARS%" >nul 2>nul
set PATH=%PREFIX%\bin;%PATH%
cmake -S "%~dp0..\engine" -B "%~dp0..\engine\build-dev" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%PREFIX%" -DCMAKE_MAKE_PROGRAM="%PREFIX%\bin\ninja.exe" || exit /b 1
cmake --build "%~dp0..\engine\build-dev" || exit /b 1
