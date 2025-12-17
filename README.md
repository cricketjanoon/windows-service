# Windows Service

A lightweight Windows service written in C++ that logs periodically to a file.

This software is for educational and authorized testing purposes only.

## Features

- Runs as a Windows service (starts automatically with Windows)
- Logs periodically to a file with timestamps
- Lightweight and minimal resource usage
- Simple installation and removal

## Prerequisites

- Windows 10/11 or Windows Server
- CMake 3.10 or higher
- C++ compiler (Visual Studio 2019 or later, or MinGW)
- Administrator privileges (for service installation)

## Building the Service

### Using Visual Studio

1. Open a Developer Command Prompt (or PowerShell with Visual Studio tools)
2. Navigate to the project directory:
   ```cmd
   cd windows-service
   ```
3. Create a build directory:
   ```cmd
   mkdir build
   cd build
   ```
4. Generate the Visual Studio solution:
   ```cmd
   cmake .. -G "Visual Studio 17 2022"
   ```
5. Build the project:
   ```cmd
   cmake --build . --config Release
   ```

The executable will be located at: `build/bin/Release/keylogger_service.exe`

## Installing the Service

### Method 1: Using the Installation Script (Recommended)

1. **Right-click** on `install_service.bat`
2. Select **"Run as administrator"**
3. The script will:
   - Check for administrator privileges
   - Verify the executable exists
   - Create and configure the service
   - Set it to start automatically

### Method 2: Manual Installation

1. Open Command Prompt or PowerShell **as Administrator**
2. Navigate to the build directory:
   ```cmd
   cd path\to\windows-service\build\bin\Release
   ```
3. Create the service:
   ```cmd
   sc create KeyLoggerService binPath= "C:\full\path\to\windows_service.exe" start= auto DisplayName= "TestService"
   ```
4. Start the service:
   ```cmd
   sc start TestService
   ```

## Verifying the Service

1. Open **Services** (services.msc)
2. Look for "TestService"
3. Check that its status is "Running"

## Viewing Logs

The service logs all keystrokes to:
```
C:\Windows\Temp\service_log.txt
```

You can view the log file with any text editor. Each entry includes:
- Timestamp
- Heartbeat

Example log entry:
```
2025-12-17 22:41:10 - === Service Started ===
2025-12-17 22:41:10 - Service worker thread started
2025-12-17 22:42:10 - Service is running (heartbeat)
```

## Removing the Service

### Method 1: Using the Uninstallation Script (Recommended)

1. **Right-click** on `uninstall_service.bat`
2. Select **"Run as administrator"**
3. The script will:
   - Stop the service if it's running
   - Delete the service from Windows

### Method 2: Manual Removal

1. Open Command Prompt or PowerShell **as Administrator**
2. Stop the service:
   ```cmd
   sc stop TestService
   ```
3. Delete the service:
   ```cmd
   sc delete TestService
   ```

## License

This software is provided as-is for educational purposes. Use at your own risk.

