@echo off
REM Script pour configurer le projet via menuconfig

cd /d "%~dp0"
call "%USERPROFILE%\esp\v5.5.1\esp-idf\export.bat"

echo.
echo === Configuration Medbox ===
echo.
echo Va dans: Medbox Configuration / WiFi Configuration
echo   - WiFi SSID: ton reseau
echo   - WiFi Password: ton mot de passe
echo.
echo Puis: Escape / S pour sauvegarder / Escape / Q pour quitter
echo.

idf.py menuconfig

pause
