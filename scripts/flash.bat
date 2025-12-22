@echo off
REM Medbox - Quick Encrypted Flash Script
REM Usage: flash.bat [PORT] [--full]
REM Example: flash.bat COM5
REM         flash.bat COM5 --full  (includes bootloader - first flash only)

setlocal

set PORT=%1
if "%PORT%"=="" set PORT=COM5

set SKIP_BOOTLOADER=--skip-bootloader
if "%2"=="--full" set SKIP_BOOTLOADER=

echo.
echo ============================================
echo   Medbox Encrypted Flash
echo   Port: %PORT%
if "%SKIP_BOOTLOADER%"=="--skip-bootloader" (
    echo   Mode: App only (bootloader protected)
) else (
    echo   Mode: Full flash (including bootloader)
)
echo ============================================
echo.

REM Build first
echo [1/2] Building firmware...
call idf.py build
if errorlevel 1 (
    echo [!] Build failed!
    exit /b 1
)

REM Flash encrypted
echo.
echo [2/2] Flashing encrypted firmware...
python scripts\encrypted_flash.py %PORT% %SKIP_BOOTLOADER%
if errorlevel 1 (
    echo [!] Flash failed!
    exit /b 1
)

echo.
echo Done! Run: idf.py -p %PORT% monitor
