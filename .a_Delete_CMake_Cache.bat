@echo off
setlocal enabledelayedexpansion
title CosmoScout VR Build and Run Script



REM =====================================================
REM 1. Delete CMake-Cache
REM =====================================================
echo.
echo Deleting CMake cache...

rm C:\Repos\cosmoscout-vr\build\windows-Release\CMakeCache.txt

echo.
echo CMake cache deleted.
exit /b 0
