@echo off
setlocal
title ICAROS COM-Port Konfiguration

REM =====================================================
REM Pfade
REM =====================================================
set ROOT_DIR=%~dp0
set DRIVER_DIR=%ROOT_DIR%externals\vista\VistaCoreLibs\VistaDeviceDrivers\VistaIcarosDriver
set DRIVER_FILE=%DRIVER_DIR%\VistaIcarosDriver.cpp

REM =====================================================
REM Ordnerprüfung
REM =====================================================
if not exist "%DRIVER_DIR%" (
    echo.
    echo FEHLER:
    echo Der Ordner "VistaIcarosDriver" existiert nicht.
    echo %DRIVER_DIR%
    echo.
    pause
    exit /b 1
)

REM =====================================================
REM Dateiprüfung
REM =====================================================
if not exist "%DRIVER_FILE%" (
    echo.
    echo FEHLER:
    echo Die Datei "VistaIcarosDriver.cpp" wurde nicht gefunden.
    echo %DRIVER_FILE%
    echo.
    pause
    exit /b 1
)

REM =====================================================
REM Benutzerinfo
REM =====================================================
echo.
echo =====================================================
echo ICAROS COM-Port Konfiguration
echo =====================================================
echo.
echo Comport-Zahl eingeben, unter welcher der Icaros Controller
echo angeschlossen ist.
echo.
echo Windows Geraetemanager:
echo Ansicht ^> Ausgeblendete Geraete anzeigen
echo Anschluesse (COM ^& LPT)
echo.

set /p NEW_COMPORT=Bitte COM-Port Nummer eingeben (z.B. 7): 

if "%NEW_COMPORT%"=="" (
    echo.
    echo FEHLER: Keine Zahl eingegeben.
    pause
    exit /b 1
)

REM =====================================================
REM Ersetzung via PowerShell
REM =====================================================
echo.
echo Aktualisiere COMPORT in VistaIcarosDriver.cpp ...

powershell -NoProfile -Command ^
  "(Get-Content '%DRIVER_FILE%') -replace 'int\s+COMPORT\s*=\s*\d+\s*;', 'int COMPORT = %NEW_COMPORT%;' | Set-Content '%DRIVER_FILE%'"

if errorlevel 1 (
    echo.
    echo FEHLER beim Aktualisieren der Datei.
    pause
    exit /b 1
)

REM =====================================================
REM Abschluss
REM =====================================================
echo.
echo =====================================================
echo Erfolgreich abgeschlossen.
echo COMPORT wurde auf %NEW_COMPORT% gesetzt.
echo Datei:
echo %DRIVER_FILE%
echo =====================================================
echo.
pause
endlocal
