// process_wrapper_example.cpp - Example usage of ProcessWrapper class

#include "process_wrapper.h"
#include <iostream>
#include <windows.h>

int main() {
    try {
        ProcessWrapper proc;
        
        std::cout << "Starting cmd.exe process..." << std::endl;
        
        // Start cmd.exe with hidden window
        if (!proc.Start("cmd.exe", true)) {
            std::cerr << "Failed to start process" << std::endl;
            return 1;
        }
        
        std::cout << "Process started with PID: " << proc.GetProcessId() << std::endl;
        
        // Give cmd.exe time to initialize
        Sleep(500);
        
        // Read initial output
        std::string output = proc.ReadFromStdout();
        if (!output.empty()) {
            std::cout << "Initial output:\n" << output << std::endl;
        }
        
        // Send a command
        std::cout << "\nSending command: dir\n" << std::endl;
        proc.WriteToStdin("dir\r\n");
        
        // Wait and read response
        Sleep(1000);
        output = proc.ReadFromStdout();
        if (!output.empty()) {
            std::cout << "Command output:\n" << output << std::endl;
        }
        
        // Send another command
        std::cout << "\nSending command: echo Hello from C++ Wrapper!\n" << std::endl;
        proc.WriteToStdin("echo Hello from C++ Wrapper!\r\n");
        
        Sleep(500);
        output = proc.ReadFromStdout();
        if (!output.empty()) {
            std::cout << "Command output:\n" << output << std::endl;
        }
        
        // Check if process is still running
        if (proc.IsRunning()) {
            std::cout << "\nProcess is still running. Terminating..." << std::endl;
            proc.Terminate();
        }
        
        std::cout << "Example completed successfully." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
