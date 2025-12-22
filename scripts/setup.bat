@echo off
REM Medbox - New ESP32 Setup Script
REM Usage: setup.bat [PORT]
REM Example: setup.bat COM5

setlocal

set PORT=%1
if "%PORT%"=="" set PORT=COM5

echo.
echo ============================================
echo   Medbox ESP32 Security Setup
echo   Port: %PORT%
echo ============================================
echo.
echo WARNING: This will permanently configure security on the ESP32!
echo Only run this on a NEW/VIRGIN ESP32.
echo.

python scripts\setup_esp32.py %PORT%
