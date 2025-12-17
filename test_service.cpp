// Define WIN32_LEAN_AND_MEAN to prevent windows.h from including winsock.h
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <fstream>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

// Service name and display name
#define SERVICE_NAME L"TestService"
#define SERVICE_DISPLAY_NAME L"Test Service"

// Namespaces for Boost.Asio / Beast
namespace net = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;

// Global variables
SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = NULL;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;
HANDLE g_WorkerThread = NULL;
std::ofstream g_LogFile;

// Single io_context used by the WebSocket server
net::io_context g_Ioc;

// Forward declarations
VOID WINAPI ServiceMain(DWORD argc, LPWSTR *argv);
VOID WINAPI ServiceCtrlHandler(DWORD CtrlCode);
DWORD WINAPI ServiceWorkerThread(LPVOID lpParam);
void WriteLog(const std::string& message);

// Simple WebSocket session that sends a heartbeat every minute
class WebsocketSession : public std::enable_shared_from_this<WebsocketSession>
{
public:
    explicit WebsocketSession(tcp::socket socket)
        : ws_(std::move(socket)),
          timer_(ws_.get_executor()),
          closed_(false)
    {}

    void start()
    {
        // Accept the WebSocket handshake asynchronously
        ws_.async_accept(
            [self = shared_from_this()](beast::error_code ec)
            {
                self->on_accept(ec);
            });
    }

private:
    websocket::stream<tcp::socket> ws_;
    net::steady_timer timer_;
    bool closed_;

    void on_accept(beast::error_code ec)
    {
        if (ec)
        {
            WriteLog(std::string("WebSocket accept failed: ") + ec.message());
            return;
        }

        // Log client connection
        beast::error_code ep_ec;
        auto ep = ws_.next_layer().remote_endpoint(ep_ec);
        if (!ep_ec)
        {
            std::ostringstream oss;
            oss << "Client connected: " << ep.address().to_string() << ":" << ep.port();
            WriteLog(oss.str());
        }
        else
        {
            WriteLog("Client connected (endpoint unknown)");
        }

        schedule_heartbeat();
    }

    void schedule_heartbeat()
    {
        if (closed_)
            return;

        // Wait for one minute before sending the next heartbeat
        timer_.expires_after(std::chrono::minutes(1));
        timer_.async_wait(
            [self = shared_from_this()](beast::error_code ec)
            {
                self->on_heartbeat_timer(ec);
            });
    }

    void on_heartbeat_timer(beast::error_code ec)
    {
        if (closed_)
            return;

        if (ec == net::error::operation_aborted)
        {
            // Timer cancelled (likely because we're shutting down)
            return;
        }

        if (ec)
        {
            WriteLog(std::string("Heartbeat timer error: ") + ec.message());
            return;
        }

        // Send heartbeat message to the client
        auto msg = std::make_shared<std::string>("heartbeat");
        ws_.async_write(
            net::buffer(*msg),
            [self = shared_from_this(), msg](beast::error_code ec, std::size_t /*bytes_transferred*/)
            {
                self->on_heartbeat_sent(ec);
            });
    }

    void on_heartbeat_sent(beast::error_code ec)
    {
        if (closed_)
            return;

        if (ec)
        {
            if (ec == websocket::error::closed)
            {
                log_disconnected();
            }
            else if (ec != net::error::operation_aborted)
            {
                WriteLog(std::string("Heartbeat send error: ") + ec.message());
                log_disconnected();
            }
            return;
        }

        // Schedule the next heartbeat
        schedule_heartbeat();
    }

    void log_disconnected()
    {
        if (closed_)
            return;

        closed_ = true;

        beast::error_code ep_ec;
        auto ep = ws_.next_layer().remote_endpoint(ep_ec);
        if (!ep_ec)
        {
            std::ostringstream oss;
            oss << "Client disconnected: " << ep.address().to_string() << ":" << ep.port();
            WriteLog(oss.str());
        }
        else
        {
            WriteLog("Client disconnected");
        }

        timer_.cancel();
    }
};

// WebSocket server that accepts incoming connections
class WebsocketServer : public std::enable_shared_from_this<WebsocketServer>
{
public:
    WebsocketServer(net::io_context& ioc, const tcp::endpoint& endpoint)
        : acceptor_(ioc),
          socket_(ioc)
    {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
        {
            WriteLog(std::string("Failed to open acceptor: ") + ec.message());
            return;
        }

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
        {
            WriteLog(std::string("Failed to set reuse_address: ") + ec.message());
            return;
        }

        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            WriteLog(std::string("Failed to bind: ") + ec.message());
            return;
        }

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            WriteLog(std::string("Failed to listen: ") + ec.message());
            return;
        }

        std::ostringstream oss;
        oss << "WebSocket server listening on " << endpoint.address().to_string()
            << ":" << endpoint.port();
        WriteLog(oss.str());
    }

    bool is_open() const
    {
        return acceptor_.is_open();
    }

    void run()
    {
        if (!acceptor_.is_open())
            return;

        do_accept();
    }

private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;

    void do_accept()
    {
        acceptor_.async_accept(
            socket_,
            [self = shared_from_this()](beast::error_code ec)
            {
                self->on_accept(ec);
            });
    }

    void on_accept(beast::error_code ec)
    {
        if (ec)
        {
            if (ec != net::error::operation_aborted)
            {
                WriteLog(std::string("Accept error: ") + ec.message());
            }
        }
        else
        {
            // Create a session for the new client
            std::make_shared<WebsocketSession>(std::move(socket_))->start();
        }

        // Accept another connection if we're still open
        if (acceptor_.is_open())
        {
            do_accept();
        }
    }
};

// Main entry point
int main(int /*argc*/, char*[] /*argv*/)
{
    SERVICE_TABLE_ENTRYW ServiceTable[] =
    {
        {SERVICE_NAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain},
        {NULL, NULL}
    };

    if (StartServiceCtrlDispatcherW(ServiceTable) == FALSE)
    {
        return GetLastError();
    }

    return 0;
}

// Service main function
VOID WINAPI ServiceMain(DWORD /*argc*/, LPWSTR* /*argv*/)
{
    // Register service control handler
    g_StatusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
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
        OutputDebugStringW(L"TestService: SetServiceStatus returned error (START_PENDING)");
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

    // Start worker thread that runs the WebSocket server
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
        OutputDebugStringW(L"TestService: SetServiceStatus returned error (RUNNING)");
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
        OutputDebugStringW(L"TestService: SetServiceStatus returned error (STOPPED)");
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
            OutputDebugStringW(L"TestService: SetServiceStatus returned error (STOP_PENDING)");
        }

        // Signal the worker thread to stop and stop the io_context
        SetEvent(g_ServiceStopEvent);
        g_Ioc.stop();
        break;

    default:
        break;
    }
}

// Worker thread that runs the WebSocket server
DWORD WINAPI ServiceWorkerThread(LPVOID /*lpParam*/)
{
    try
    {
        g_Ioc.restart();

        // Listen on all interfaces, port 9002
        auto address = net::ip::make_address("0.0.0.0");
        unsigned short port = 9002;
        tcp::endpoint endpoint{address, port};

        auto server = std::make_shared<WebsocketServer>(g_Ioc, endpoint);
        if (!server->is_open())
        {
            WriteLog("WebSocket server failed to start");
            return 1;
        }

        server->run();

        // Run the I/O context on this thread until stopped
        g_Ioc.run();
    }
    catch (const std::exception& ex)
    {
        WriteLog(std::string("Exception in worker thread: ") + ex.what());
    }
    catch (...)
    {
        WriteLog("Unknown exception in worker thread");
    }

    return 0;
}

// Write log entry with timestamp
void WriteLog(const std::string& message)
{
    if (!g_LogFile.is_open())
        return;

    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);

    g_LogFile << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " - " << message << std::endl;
    g_LogFile.flush();
}


