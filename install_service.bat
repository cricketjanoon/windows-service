@echo off
REM Script to install the Windows service
REM This script must be run as Administrator

echo Installing TestService...

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator!
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Get the directory where the executable is located
set SERVICE_PATH=D:\MEGA\Repos\Cpp\windows-service\build\bin\Release\test_service.exe

REM Check if the executable exists
if not exist "%SERVICE_PATH%" (
    echo ERROR: test_service.exe not found at %SERVICE_PATH%
    echo Please build the project first using CMake
    pause
    exit /b 1
)

REM Create the service
sc create TestService binPath="%SERVICE_PATH%" start=auto DisplayName="TestService"

if %errorLevel% equ 0 (
    echo Service created successfully!
    echo.
    echo To start the service, run:
    echo   sc start TestService
    echo.
    echo The service will log to: C:\Windows\Temp\service_log.txt
) else (
    echo ERROR: Failed to create service
    echo Error code: %errorLevel%
)

pause

