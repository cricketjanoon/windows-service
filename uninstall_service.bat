@echo off
REM Script to uninstall the Windows service
REM This script must be run as Administrator

echo Uninstalling TestService...

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo ERROR: This script must be run as Administrator!
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

REM Stop the service if it's running
echo Stopping service...
sc stop TestService
timeout /t 2 /nobreak >nul

REM Delete the service
echo Deleting service...
sc delete TestService

if %errorLevel% equ 0 (
    echo Service uninstalled successfully!
    echo.
    echo Note: The log file at C:\Windows\Temp\service_log.txt was not deleted.
    echo You may delete it manually if desired.
) else (
    echo ERROR: Failed to delete service
    echo Error code: %errorLevel%
    echo The service may not be installed, or you may need to stop it first.
)

pause

