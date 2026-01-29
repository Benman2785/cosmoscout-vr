@echo off
setlocal enabledelayedexpansion
title CosmoScout VR Run Script

REM =====================================================
REM 1. In Bin-Verzeichnis wechseln
REM =====================================================
set "BIN_PATH=C:\Repos\cosmoscout-vr\install\windows-Release\bin"
if exist "%BIN_PATH%" (
    cd /d "%BIN_PATH%"
    echo Changing directory to "%BIN_PATH%"
) else (
    echo Error: Directory not found!
    pause
    exit /b 1
)

REM =====================================================
REM 2. Benutzer-Eingabe (Jede Taste startet VR)
REM =====================================================
echo.
echo Starting CosmoScout for Icaros Pro on the Mars surface
echo.

REM Hier wird auf Input gewartet, aber der Inhalt der Variable wird ignoriert
set /p DUMMY_VAR=[Start]

echo Starting Cosmoscout...
start "CosmoScout VR" cmd /c "cd /d "%BIN_PATH%" && hmd_icaros_mars-OHNE-FEATURES.bat"

echo.
echo Started CosmoScout.
exit /b 0