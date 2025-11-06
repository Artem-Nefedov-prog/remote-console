// my.c - Remote Console Application for Windows 11
// Implements client-server architecture for remote command execution

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tchar.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#define BUFSIZE 4096
#define DEFAULT_PORT 9999

// Global variables
HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;
HANDLE g_StopEvent = NULL;

SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_ServiceStatusHandle = NULL;

// Forward declarations
void RunServer(BOOL asService);
void RunClient(const char* serverIP);
BOOL CreateChildProcessWithPipes(void);
DWORD WINAPI PipeToSocketThread(LPVOID lpParam);
DWORD WINAPI SocketToPipeThread(LPVOID lpParam);
void InstallService(void);
void UninstallService(void);
void StartMyService(void);
void StopMyService(void);
VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
DWORD WINAPI ServiceCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext);
void SetServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);
void ErrorExit(const char* msg);

// Structure to pass socket handle to threads
typedef struct {
    SOCKET sock;
} ThreadParams;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  Server mode:              my.exe -s\n");
        printf("  Server as service:        my.exe -s -service\n");
        printf("  Install service:          my.exe -install\n");
        printf("  Uninstall service:        my.exe -uninstall\n");
        printf("  Start service:            my.exe -start\n");
        printf("  Stop service:             my.exe -stop\n");
        printf("  Client mode:              my.exe -c [server_ip]\n");
        printf("                            (default: 127.0.0.1)\n");
        return 1;
    }

    if (strcmp(argv[1], "-s") == 0) {
        if (argc > 2 && strcmp(argv[2], "-service") == 0) {
            // Run as service
            SERVICE_TABLE_ENTRY ServiceTable[] = {
                {TEXT("RemoteConsoleService"), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
                {NULL, NULL}
            };
            if (!StartServiceCtrlDispatcher(ServiceTable)) {
                printf("StartServiceCtrlDispatcher failed (%d)\n", GetLastError());
                return 1;
            }
        } else {
            // Run as console application
            RunServer(FALSE);
        }
    }
    else if (strcmp(argv[1], "-c") == 0) {
        const char* serverIP = (argc > 2) ? argv[2] : "127.0.0.1";
        RunClient(serverIP);
    }
    else if (strcmp(argv[1], "-install") == 0) {
        InstallService();
    }
    else if (strcmp(argv[1], "-uninstall") == 0) {
        UninstallService();
    }
    else if (strcmp(argv[1], "-start") == 0) {
        StartMyService();
    }
    else if (strcmp(argv[1], "-stop") == 0) {
        StopMyService();
    }
    else {
        printf("Unknown option: %s\n", argv[1]);
        return 1;
    }

    return 0;
}

// Create child process (cmd.exe) with redirected pipes
BOOL CreateChildProcessWithPipes(void) {
    SECURITY_ATTRIBUTES saAttr;
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    BOOL bSuccess = FALSE;

    // Set up security attributes for pipe handles
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create pipe for child's STDOUT
    if (!CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0))
        return FALSE;
    
    // Ensure read handle is not inherited
    if (!SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0))
        return FALSE;

    // Create pipe for child's STDIN
    if (!CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
        return FALSE;
    
    // Ensure write handle is not inherited
    if (!SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0))
        return FALSE;

    // Set up process info
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = g_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = g_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;

    // Create cmd.exe process
    char cmdline[] = "cmd.exe";
    bSuccess = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 
                              CREATE_NO_WINDOW, NULL, NULL, 
                              &siStartInfo, &piProcInfo);

    if (!bSuccess) {
        printf("CreateProcess failed (%d)\n", GetLastError());
        return FALSE;
    }

    // Close handles not needed by parent
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(g_hChildStd_OUT_Wr);
    CloseHandle(g_hChildStd_IN_Rd);

    return TRUE;
}

// Thread: Read from pipe (cmd stdout) and send to socket
DWORD WINAPI PipeToSocketThread(LPVOID lpParam) {
    ThreadParams* params = (ThreadParams*)lpParam;
    SOCKET clientSocket = params->sock;
    char buffer[BUFSIZE];
    DWORD bytesRead;
    DWORD bytesAvail;
    
    while (WaitForSingleObject(g_StopEvent, 0) != WAIT_OBJECT_0) {
        // Check if data is available without blocking
        if (PeekNamedPipe(g_hChildStd_OUT_Rd, NULL, 0, NULL, &bytesAvail, NULL) && bytesAvail > 0) {
            if (ReadFile(g_hChildStd_OUT_Rd, buffer, BUFSIZE - 1, &bytesRead, NULL) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                if (send(clientSocket, buffer, bytesRead, 0) == SOCKET_ERROR) {
                    printf("Send failed: %d\n", WSAGetLastError());
                    break;
                }
            }
        } else {
            Sleep(10); // Small delay to avoid busy-wait
        }
    }
    
    return 0;
}

// Thread: Receive from socket and write to pipe (cmd stdin)
DWORD WINAPI SocketToPipeThread(LPVOID lpParam) {
    ThreadParams* params = (ThreadParams*)lpParam;
    SOCKET clientSocket = params->sock;
    char buffer[BUFSIZE];
    int bytesRecv;
    DWORD bytesWritten;
    
    // Set socket to non-blocking mode
    u_long mode = 1;
    ioctlsocket(clientSocket, FIONBIO, &mode);
    
    while (WaitForSingleObject(g_StopEvent, 0) != WAIT_OBJECT_0) {
        bytesRecv = recv(clientSocket, buffer, BUFSIZE - 1, 0);
        
        if (bytesRecv > 0) {
            buffer[bytesRecv] = '\0';
            if (!WriteFile(g_hChildStd_IN_Wr, buffer, bytesRecv, &bytesWritten, NULL)) {
                printf("WriteFile failed: %d\n", GetLastError());
                break;
            }
        } else if (bytesRecv == 0) {
            // Connection closed
            break;
        } else {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                printf("Recv failed: %d\n", error);
                break;
            }
            Sleep(10); // Small delay
        }
    }
    
    return 0;
}

void RunServer(BOOL asService) {
    WSADATA wsaData;
    SOCKET listenSocket = INVALID_SOCKET;
    SOCKET clientSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    int result;
    
    if (!asService)
        printf("Starting server on port %d...\n", DEFAULT_PORT);

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        if (!asService)
            printf("WSAStartup failed: %d\n", result);
        return;
    }

    // Create socket
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        if (!asService)
            printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    // Setup server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(DEFAULT_PORT);

    // Bind socket
    result = bind(listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        if (!asService)
            printf("Bind failed: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    // Listen
    result = listen(listenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        if (!asService)
            printf("Listen failed: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    if (!asService)
        printf("Server listening on port %d. Waiting for client...\n", DEFAULT_PORT);

    // Accept client connection
    clientSocket = accept(listenSocket, NULL, NULL);
    if (clientSocket == INVALID_SOCKET) {
        if (!asService)
            printf("Accept failed: %d\n", WSAGetLastError());
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    if (!asService)
        printf("Client connected!\n");

    // Create child process with redirected pipes
    if (!CreateChildProcessWithPipes()) {
        if (!asService)
            printf("Failed to create child process\n");
        closesocket(clientSocket);
        closesocket(listenSocket);
        WSACleanup();
        return;
    }

    // Create stop event
    g_StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    // Create threads for bidirectional communication
    ThreadParams params;
    params.sock = clientSocket;
    
    HANDLE hThreads[2];
    hThreads[0] = CreateThread(NULL, 0, PipeToSocketThread, &params, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, SocketToPipeThread, &params, 0, NULL);

    // Wait for threads to complete
    WaitForMultipleObjects(2, hThreads, TRUE, INFINITE);

    // Cleanup
    CloseHandle(hThreads[0]);
    CloseHandle(hThreads[1]);
    CloseHandle(g_StopEvent);
    CloseHandle(g_hChildStd_IN_Wr);
    CloseHandle(g_hChildStd_OUT_Rd);
    
    closesocket(clientSocket);
    closesocket(listenSocket);
    WSACleanup();

    if (!asService)
        printf("Server stopped.\n");
}

void RunClient(const char* serverIP) {
    WSADATA wsaData;
    SOCKET connectSocket = INVALID_SOCKET;
    struct sockaddr_in serverAddr;
    int result;
    char sendBuffer[BUFSIZE];
    char recvBuffer[BUFSIZE];
    
    printf("Connecting to server %s:%d...\n", serverIP, DEFAULT_PORT);

    // Initialize Winsock
    result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        printf("WSAStartup failed: %d\n", result);
        return;
    }

    // Create socket
    connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        printf("Socket creation failed: %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }

    // Setup server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, serverIP, &serverAddr.sin_addr);

    // Connect to server
    result = connect(connectSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        printf("Connection failed: %d\n", WSAGetLastError());
        closesocket(connectSocket);
        WSACleanup();
        return;
    }

    printf("Connected to server!\n");
    printf("Enter commands (type 'exit' to quit):\n\n");

    // Set socket to non-blocking
    u_long mode = 1;
    ioctlsocket(connectSocket, FIONBIO, &mode);

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD fdwMode = ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT;
    SetConsoleMode(hStdin, fdwMode);

    BOOL running = TRUE;
    while (running) {
        // Check for keyboard input
        DWORD eventsAvailable = 0;
        if (GetNumberOfConsoleInputEvents(hStdin, &eventsAvailable) && eventsAvailable > 1) {
            DWORD bytesRead;
            if (ReadConsole(hStdin, sendBuffer, BUFSIZE - 1, &bytesRead, NULL) && bytesRead > 0) {
                sendBuffer[bytesRead] = '\0';
                
                // Check for exit command
                if (strncmp(sendBuffer, "exit", 4) == 0) {
                    send(connectSocket, "exit\r\n", 6, 0);
                    running = FALSE;
                    break;
                }
                
                // Send to server
                if (send(connectSocket, sendBuffer, bytesRead, 0) == SOCKET_ERROR) {
                    printf("Send failed: %d\n", WSAGetLastError());
                    break;
                }
            }
        }

        // Check for server response
        result = recv(connectSocket, recvBuffer, BUFSIZE - 1, 0);
        if (result > 0) {
            recvBuffer[result] = '\0';
            printf("%s", recvBuffer);
        } else if (result == 0) {
            printf("\nConnection closed by server.\n");
            break;
        } else {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                printf("Recv failed: %d\n", error);
                break;
            }
        }

        Sleep(10); // Small delay
    }

    // Cleanup
    closesocket(connectSocket);
    WSACleanup();
    printf("Client disconnected.\n");
}

// Service Management Functions
void InstallService(void) {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];

    if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Append service argument
    _tcscat_s(szPath, MAX_PATH, TEXT(" -s -service"));

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    schService = CreateService(
        schSCManager,
        TEXT("RemoteConsoleService"),
        TEXT("Remote Console Service"),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START,
        SERVICE_ERROR_NORMAL,
        szPath,
        NULL, NULL, NULL, NULL, NULL);

    if (schService == NULL) {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    printf("Service installed successfully.\n");
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

void UninstallService(void) {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    schService = OpenService(schSCManager, TEXT("RemoteConsoleService"), DELETE);
    if (schService == NULL) {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    if (!DeleteService(schService)) {
        printf("DeleteService failed (%d)\n", GetLastError());
    } else {
        printf("Service uninstalled successfully.\n");
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

void StartMyService(void) {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    schService = OpenService(schSCManager, TEXT("RemoteConsoleService"), SERVICE_START);
    if (schService == NULL) {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    if (!StartService(schService, 0, NULL)) {
        printf("StartService failed (%d)\n", GetLastError());
    } else {
        printf("Service started successfully.\n");
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

void StopMyService(void) {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    SERVICE_STATUS serviceStatus;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (NULL == schSCManager) {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }

    schService = OpenService(schSCManager, TEXT("RemoteConsoleService"), SERVICE_STOP);
    if (schService == NULL) {
        printf("OpenService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }

    if (!ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) {
        printf("ControlService failed (%d)\n", GetLastError());
    } else {
        printf("Service stopped successfully.\n");
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    g_ServiceStatusHandle = RegisterServiceCtrlHandlerEx(
        TEXT("RemoteConsoleService"), 
        ServiceCtrlHandler, 
        NULL);

    if (!g_ServiceStatusHandle) {
        return;
    }

    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Start the service
    SetServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);
    
    RunServer(TRUE);

    SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

DWORD WINAPI ServiceCtrlHandler(DWORD dwControl, DWORD dwEventType, 
                                 LPVOID lpEventData, LPVOID lpContext) {
    switch (dwControl) {
        case SERVICE_CONTROL_STOP:
            SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            if (g_StopEvent) {
                SetEvent(g_StopEvent);
            }
            SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
            return NO_ERROR;

        case SERVICE_CONTROL_SHUTDOWN:
            SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
            if (g_StopEvent) {
                SetEvent(g_StopEvent);
            }
            SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
            return NO_ERROR;

        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void SetServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;

    g_ServiceStatus.dwCurrentState = dwCurrentState;
    g_ServiceStatus.dwWin32ExitCode = dwWin32ExitCode;
    g_ServiceStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING) {
        g_ServiceStatus.dwControlsAccepted = 0;
    } else {
        g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    if ((dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED)) {
        g_ServiceStatus.dwCheckPoint = 0;
    } else {
        g_ServiceStatus.dwCheckPoint = dwCheckPoint++;
    }

    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
}

void ErrorExit(const char* msg) {
    printf("Error: %s\n", msg);
    ExitProcess(1);
}
