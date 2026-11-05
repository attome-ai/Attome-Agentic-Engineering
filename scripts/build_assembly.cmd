@echo off
rem Build the assembly planner (assembly\) into assembly\build, separately from the engine.
setlocal
if "%MAMBA_ROOT_PREFIX%"=="" set MAMBA_ROOT_PREFIX=%USERPROFILE%\micromamba\root
set PREFIX=%MAMBA_ROOT_PREFIX%\envs\attome-ae\Library
if "%VCVARS%"=="" set VCVARS=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat
call "%VCVARS%" >nul 2>nul
set PATH=%PREFIX%\bin;%PATH%
cmake -S "%~dp0..\assembly" -B "%~dp0..\assembly\build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%PREFIX%" -DCMAKE_MAKE_PROGRAM="%PREFIX%\bin\ninja.exe" || exit /b 1
cmake --build "%~dp0..\assembly\build" || exit /b 1
