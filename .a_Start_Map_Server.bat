@echo off
title MapServer Cosmoscout

REM =====================================================
REM Pfad zum Docker-Projekt in WSL
REM =====================================================
set "PROJECT_PATH=/mnt/c/Repos/docker-mapserver"

REM =====================================================
REM Starte WSL und führe Befehle aus
REM =====================================================
echo Starte Ubuntu WSL und Docker MapServer...
REM wsl bash -c "cd %PROJECT_PATH% && sudo docker run -ti --rm -p 8080:80 -v ../docker-mapserver/mapserver-datasets:/mapserver-datasets ghcr.io/cosmoscout/mapserver-example"
wsl bash -c "cd %PROJECT_PATH% && sudo docker run -ti --rm -p 8080:80 mapserver:mars_earth"

echo.
echo MapServer beendet.
pause
exit /b 0