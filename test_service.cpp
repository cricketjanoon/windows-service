#include <windows.h>
#include <fstream>
#include <string>
#include <ctime>
#include <iomanip>

// Service name and display name
#define SERVICE_NAME L"TestService"
#define SERVICE_DISPLAY_NAME L"Test Service"

// Global variables
SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
HANDLE g_WorkerThread = NULL;
std::ofstream g_LogFile;

// Forward declarations
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
void WriteLog(const std::string& message);

// Main entry point
int main(int /*argc*/, char*[] /*argv*/)
{
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcher(ServiceTable) == FALSE)
    {
        return GetLastError();
    }

    return 0;
}

// Service main function
VOID WINAPI ServiceMain(DWORD /*argc*/, LPTSTR* /*argv*/)
{
    DWORD Status = E_FAIL;

    // Register service control handler
    g_StatusHandle = RegisterServiceCtrlHandler(SERVICE_NAME, ServiceCtrlHandler);
    if (g_StatusHandle == NULL)
    {
        return;
    }

    // Tell the service controller we are starting
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(L"TestService: SetServiceStatus returned error (START_PENDING)");
    }

    // Create stop event
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (g_ServiceStopEvent == NULL)
    {
        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        g_ServiceStatus.dwCheckPoint = 1;

        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // Open log file
    std::wstring logPath = L"C:\\Windows\\Temp\\service_log.txt";
    g_LogFile.open(logPath, std::ios::app);
    if (g_LogFile.is_open())
    {
        WriteLog("=== Service Started ===");
    }

    // Start worker thread that logs every minute
    g_WorkerThread = CreateThread(
        NULL,
        0,
        ServiceWorkerThread,
        NULL,
        0,
        NULL);

    if (g_WorkerThread == NULL)
    {
        WriteLog("Failed to create worker thread");
    }

    // Tell the service controller we are running
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(L"TestService: SetServiceStatus returned error (RUNNING)");
    }

    // Wait until service is stopped
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    // Wait for worker thread to finish
    if (g_WorkerThread != NULL)
    {
        WaitForSingleObject(g_WorkerThread, INFINITE);
        CloseHandle(g_WorkerThread);
        g_WorkerThread = NULL;
    }

    // Cleanup
    if (g_LogFile.is_open())
    {
        WriteLog("=== Service Stopped ===");
        g_LogFile.close();
    }

    if (g_ServiceStopEvent != NULL && g_ServiceStopEvent != INVALID_HANDLE_VALUE)
    {
        CloseHandle(g_ServiceStopEvent);
        g_ServiceStopEvent = INVALID_HANDLE_VALUE;
    }

    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 3;

    if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
    {
        OutputDebugString(L"TestService: SetServiceStatus returned error (STOPPED)");
    }
}

// Service control handler
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode)
{
    switch (CtrlCode)
    {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        g_ServiceStatus.dwControlsAccepted = 0;
        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        g_ServiceStatus.dwWin32ExitCode = 0;
        g_ServiceStatus.dwCheckPoint = 4;

        if (SetServiceStatus(g_StatusHandle, &g_ServiceStatus) == FALSE)
        {
            OutputDebugString(L"TestService: SetServiceStatus returned error (STOP_PENDING)");
        }

        // Signal the worker thread to stop
        SetEvent(g_ServiceStopEvent);
        break;

    default:
        break;
    }
}

// Worker thread that logs a heartbeat every minute
DWORD WINAPI ServiceWorkerThread(LPVOID /*lpParam*/)
{
    // Log once immediately
    WriteLog("Service worker thread started");

    // Loop until stop event is signaled; wake up every 60 seconds
    while (WaitForSingleObject(g_ServiceStopEvent, 60 * 1000) == WAIT_TIMEOUT)
    {
        WriteLog("Service is running (heartbeat)");
    }

    WriteLog("Service worker thread stopping");
    return 0;
}

// Write log entry with timestamp
void WriteLog(const std::string& message)
{
    if (!g_LogFile.is_open())
        return;

    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);

    g_LogFile << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " - " << message << std::endl;
    g_LogFile.flush();
}


