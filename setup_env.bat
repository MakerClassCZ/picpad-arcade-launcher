@echo off
rem Run this from a cmd prompt to set PICO_ROOT_PATH for the launcher build.
rem   Usage:  setup_env.bat
rem
rem Defaults to the bundled PicoLibSDK copy under .\picolibsdk\.
rem Override by setting PICO_ROOT_PATH before running this file.
rem
rem To use a specific ARM GCC install, set ARM_GCC_DIR before running, e.g.:
rem   set "ARM_GCC_DIR=C:\Program Files\ARM\bin"
rem   setup_env.bat

if "%PICO_ROOT_PATH%"=="" (
    set "PICO_ROOT_PATH=%~dp0picolibsdk"
)

if not exist "%PICO_ROOT_PATH%" (
    echo ERROR: PicoLibSDK not found at %PICO_ROOT_PATH%
    exit /b 1
)

echo PICO_ROOT_PATH = %PICO_ROOT_PATH%

set "PATH=%PICO_ROOT_PATH%\_tools;%PATH%"

if not "%ARM_GCC_DIR%"=="" (
    if exist "%ARM_GCC_DIR%\arm-none-eabi-gcc.exe" (
        set "PATH=%ARM_GCC_DIR%;%PATH%"
        echo ARM toolchain  = %ARM_GCC_DIR%
    ) else (
        echo WARNING: ARM_GCC_DIR set but arm-none-eabi-gcc.exe not found there.
    )
)

where arm-none-eabi-gcc >nul 2>&1
if errorlevel 1 (
    echo WARNING: arm-none-eabi-gcc not on PATH.
    echo   Install: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
    echo   Then add its bin\ directory to PATH or set ARM_GCC_DIR.
)
