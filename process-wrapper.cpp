// process_wrapper.cpp - Implementation of ProcessWrapper class

#include "process_wrapper.h"
#include <iostream>

ProcessWrapper::ProcessWrapper() 
    : m_hChildStd_IN_Rd(NULL),
      m_hChildStd_IN_Wr(NULL),
      m_hChildStd_OUT_Rd(NULL),
      m_hChildStd_OUT_Wr(NULL),
      m_hProcess(NULL),
      m_hThread(NULL),
      m_dwProcessId(0),
      m_dwThreadId(0),
      m_bRunning(false) {
}

ProcessWrapper::~ProcessWrapper() {
    if (m_bRunning) {
        Terminate();
    }
    ClosePipes();
}

void ProcessWrapper::CreatePipes() {
    SECURITY_ATTRIBUTES saAttr;
    
    // Set up security attributes for pipe handles
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create pipe for child's STDOUT
    if (!CreatePipe(&m_hChildStd_OUT_Rd, &m_hChildStd_OUT_Wr, &saAttr, 0)) {
        throw std::runtime_error("Failed to create stdout pipe");
    }
    
    // Ensure read handle is not inherited
    if (!SetHandleInformation(m_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(m_hChildStd_OUT_Rd);
        CloseHandle(m_hChildStd_OUT_Wr);
        throw std::runtime_error("Failed to set stdout pipe handle information");
    }

    // Create pipe for child's STDIN
    if (!CreatePipe(&m_hChildStd_IN_Rd, &m_hChildStd_IN_Wr, &saAttr, 0)) {
        CloseHandle(m_hChildStd_OUT_Rd);
        CloseHandle(m_hChildStd_OUT_Wr);
        throw std::runtime_error("Failed to create stdin pipe");
    }
    
    // Ensure write handle is not inherited
    if (!SetHandleInformation(m_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
        ClosePipes();
        throw std::runtime_error("Failed to set stdin pipe handle information");
    }
}

void ProcessWrapper::ClosePipes() {
    if (m_hChildStd_IN_Rd) { CloseHandle(m_hChildStd_IN_Rd); m_hChildStd_IN_Rd = NULL; }
    if (m_hChildStd_IN_Wr) { CloseHandle(m_hChildStd_IN_Wr); m_hChildStd_IN_Wr = NULL; }
    if (m_hChildStd_OUT_Rd) { CloseHandle(m_hChildStd_OUT_Rd); m_hChildStd_OUT_Rd = NULL; }
    if (m_hChildStd_OUT_Wr) { CloseHandle(m_hChildStd_OUT_Wr); m_hChildStd_OUT_Wr = NULL; }
    if (m_hProcess) { CloseHandle(m_hProcess); m_hProcess = NULL; }
    if (m_hThread) { CloseHandle(m_hThread); m_hThread = NULL; }
}

bool ProcessWrapper::Start(const std::string& commandLine, bool hideWindow) {
    if (m_bRunning) {
        return false; // Already running
    }

    try {
        CreatePipes();
    } catch (const std::exception& e) {
        std::cerr << "Error creating pipes: " << e.what() << std::endl;
        return false;
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOA siStartInfo;
    
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = m_hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = m_hChildStd_OUT_Wr;
    siStartInfo.hStdInput = m_hChildStd_IN_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    
    if (hideWindow) {
        siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;
        siStartInfo.wShowWindow = SW_HIDE;
    }

    // Make a mutable copy of command line
    char* cmdline = _strdup(commandLine.c_str());
    
    // Create the child process
    BOOL bSuccess = CreateProcessA(
        NULL,           // Application name
        cmdline,        // Command line
        NULL,           // Process security attributes
        NULL,           // Thread security attributes
        TRUE,           // Inherit handles
        hideWindow ? CREATE_NO_WINDOW : 0, // Creation flags
        NULL,           // Environment
        NULL,           // Current directory
        &siStartInfo,   // Startup info
        &piProcInfo     // Process info
    );

    free(cmdline);

    if (!bSuccess) {
        ClosePipes();
        return false;
    }

    // Store process information
    m_hProcess = piProcInfo.hProcess;
    m_hThread = piProcInfo.hThread;
    m_dwProcessId = piProcInfo.dwProcessId;
    m_dwThreadId = piProcInfo.dwThreadId;
    m_bRunning = true;

    // Close handles that child inherited
    CloseHandle(m_hChildStd_OUT_Wr);
    m_hChildStd_OUT_Wr = NULL;
    CloseHandle(m_hChildStd_IN_Rd);
    m_hChildStd_IN_Rd = NULL;

    return true;
}

bool ProcessWrapper::WriteToStdin(const std::string& data) {
    return WriteToStdin(data.c_str(), data.length());
}

bool ProcessWrapper::WriteToStdin(const char* data, size_t length) {
    if (!m_bRunning || !m_hChildStd_IN_Wr) {
        return false;
    }

    DWORD bytesWritten;
    BOOL bSuccess = WriteFile(m_hChildStd_IN_Wr, data, (DWORD)length, &bytesWritten, NULL);
    
    return bSuccess && (bytesWritten == length);
}

std::string ProcessWrapper::ReadFromStdout(size_t maxBytes) {
    if (!m_bRunning || !m_hChildStd_OUT_Rd) {
        return "";
    }

    DWORD bytesAvailable = 0;
    if (!PeekNamedPipe(m_hChildStd_OUT_Rd, NULL, 0, NULL, &bytesAvailable, NULL)) {
        return "";
    }

    if (bytesAvailable == 0) {
        return "";
    }

    // Limit read size
    DWORD bytesToRead = (bytesAvailable < maxBytes) ? bytesAvailable : (DWORD)maxBytes;
    
    char* buffer = new char[bytesToRead + 1];
    DWORD bytesRead = 0;
    
    BOOL bSuccess = ReadFile(m_hChildStd_OUT_Rd, buffer, bytesToRead, &bytesRead, NULL);
    
    std::string result;
    if (bSuccess && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        result = std::string(buffer, bytesRead);
    }
    
    delete[] buffer;
    return result;
}

bool ProcessWrapper::IsDataAvailable(DWORD& bytesAvailable) {
    if (!m_bRunning || !m_hChildStd_OUT_Rd) {
        bytesAvailable = 0;
        return false;
    }

    return PeekNamedPipe(m_hChildStd_OUT_Rd, NULL, 0, NULL, &bytesAvailable, NULL) != 0;
}

bool ProcessWrapper::IsRunning() const {
    if (!m_bRunning || !m_hProcess) {
        return false;
    }

    DWORD exitCode;
    if (GetExitCodeProcess(m_hProcess, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }
    
    return false;
}

bool ProcessWrapper::WaitForExit(DWORD timeout) {
    if (!m_bRunning || !m_hProcess) {
        return true;
    }

    DWORD result = WaitForSingleObject(m_hProcess, timeout);
    if (result == WAIT_OBJECT_0) {
        m_bRunning = false;
        return true;
    }
    
    return false;
}

bool ProcessWrapper::Terminate(DWORD exitCode) {
    if (!m_bRunning || !m_hProcess) {
        return false;
    }

    BOOL result = TerminateProcess(m_hProcess, exitCode);
    if (result) {
        WaitForSingleObject(m_hProcess, 5000); // Wait up to 5 seconds
        m_bRunning = false;
    }
    
    return result != 0;
}
