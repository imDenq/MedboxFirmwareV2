@echo off
REM ===================================================
REM Script de build pour Medbox Firmware
REM Utilise ESP-IDF v5.5.1
REM Double-cliquer pour builder le projet
REM ===================================================

echo.
echo ===== Medbox Firmware Build =====
echo.

REM Aller dans le dossier du script
cd /d "%~dp0"

REM Sourcer l'environnement ESP-IDF
call "%USERPROFILE%\esp\v5.5.1\esp-idf\export.bat"
if errorlevel 1 (
    echo ERREUR: Impossible de charger ESP-IDF
    pause
    exit /b 1
)

echo.
echo === Demarrage du build ===
echo.

REM Build du projet
idf.py build

if errorlevel 1 (
    echo.
    echo === BUILD ECHOUE ===
) else (
    echo.
    echo === BUILD REUSSI ===
    echo.
    echo Pour flasher sur COM5:
    echo   idf.py -p COM5 flash monitor
)

echo.
pause
