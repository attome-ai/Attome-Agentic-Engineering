@echo off
rem J1: one-command install on Windows. Creates the environment, builds the engine, checks the machine.
rem Needs Visual Studio 2022 with the C++ tools (for the engine), and internet for the first run only.
rem Usage: scripts\install.cmd
setlocal
if "%MAMBA_ROOT_PREFIX%"=="" set MAMBA_ROOT_PREFIX=%USERPROFILE%\micromamba\root
set MM=%USERPROFILE%\micromamba\micromamba.exe
if not exist "%MM%" (
  echo Downloading micromamba to %MM%
  mkdir "%USERPROFILE%\micromamba" 2>nul
  curl -L -f -o "%MM%" https://github.com/mamba-org/micromamba-releases/releases/latest/download/micromamba-win-64 || (echo could not download micromamba & exit /b 1)
)
if exist "%MAMBA_ROOT_PREFIX%\envs\attome-ae\python.exe" (
  echo Environment attome-ae already exists; updating it from environment.yml
  "%MM%" update -y -n attome-ae -f "%~dp0..\environment.yml" || exit /b 1
) else (
  "%MM%" create -y -f "%~dp0..\environment.yml" || exit /b 1
)
call "%~dp0build_engine.cmd" || exit /b 1
"%MM%" run -n attome-ae python "%~dp0demo.py" preflight || exit /b 1
echo.
echo Installed. Try: micromamba run -n attome-ae python -m structura.app
