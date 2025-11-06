// process_wrapper.h - C++ Wrapper for Process Management
// Task 4.6: C++ wrapper for process creation with pipe redirection

#ifndef PROCESS_WRAPPER_H
#define PROCESS_WRAPPER_H

#include <windows.h>
#include <string>
#include <stdexcept>

class ProcessWrapper {
private:
    HANDLE m_hChildStd_IN_Rd;
    HANDLE m_hChildStd_IN_Wr;
    HANDLE m_hChildStd_OUT_Rd;
    HANDLE m_hChildStd_OUT_Wr;
    HANDLE m_hProcess;
    HANDLE m_hThread;
    DWORD m_dwProcessId;
    DWORD m_dwThreadId;
    bool m_bRunning;

    void CreatePipes();
    void ClosePipes();

public:
    ProcessWrapper();
    ~ProcessWrapper();

    // Disable copy constructor and assignment operator
    ProcessWrapper(const ProcessWrapper&) = delete;
    ProcessWrapper& operator=(const ProcessWrapper&) = delete;

    // Start a process with redirected I/O
    bool Start(const std::string& commandLine, bool hideWindow = true);
    
    // Write data to child process stdin
    bool WriteToStdin(const std::string& data);
    bool WriteToStdin(const char* data, size_t length);
    
    // Read data from child process stdout (non-blocking)
    std::string ReadFromStdout(size_t maxBytes = 4096);
    
    // Check if data is available to read
    bool IsDataAvailable(DWORD& bytesAvailable);
    
    // Check if process is still running
    bool IsRunning() const;
    
    // Wait for process to exit
    bool WaitForExit(DWORD timeout = INFINITE);
    
    // Terminate process
    bool Terminate(DWORD exitCode = 0);
    
    // Get process ID
    DWORD GetProcessId() const { return m_dwProcessId; }
    
    // Get handles (for advanced usage)
    HANDLE GetStdinHandle() const { return m_hChildStd_IN_Wr; }
    HANDLE GetStdoutHandle() const { return m_hChildStd_OUT_Rd; }
    HANDLE GetProcessHandle() const { return m_hProcess; }
};

#endif // PROCESS_WRAPPER_H
