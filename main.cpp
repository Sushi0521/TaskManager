#include <windows.h>    // For Windows API functions
#include <psapi.h>      // For process and memory information functions
#include <iostream>     // For standard input and output
#include <vector>       // For using the vector container
#include <string>       // For using the wstring class
#include <map>          // For using the map container

// Structure to hold information about each process
struct ProcessInfo {
    DWORD processID;            // Process ID
    std::wstring processName;   // Name of the process
    SIZE_T memoryUsageKB;       // Memory usage in kilobytes
    std::wstring windowClassName; // Window class name associated with the process
};

// Function to retrieve the name of a process given its handle
std::wstring GetProcessName(HANDLE hProcess) {
    WCHAR processName[MAX_PATH] = L"<unknown>"; // Default name if retrieval fails
    HMODULE hMod;
    DWORD cbNeeded;

    // Get the first module in the process (typically the executable)
    if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
        // Retrieve the base name of the module (process name)
        GetModuleBaseName(hProcess, hMod, processName, sizeof(processName) / sizeof(WCHAR));
    }
    return std::wstring(processName);
}

// Function to retrieve the memory usage of a process
SIZE_T GetProcessMemoryUsage(HANDLE hProcess) {
    PROCESS_MEMORY_COUNTERS pmc;
    // Get the memory usage information for the process
    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / 1024; // Convert bytes to kilobytes
    }
    return 0;
}

// Function to retrieve the window class names associated with each process
std::map<DWORD, std::wstring> GetProcessWindowClassNames() {
    std::map<DWORD, std::wstring> processWindowClasses;

    // Lambda function to be called for each top-level window
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        DWORD processID;
        // Get the process ID associated with the window
        GetWindowThreadProcessId(hwnd, &processID);

        WCHAR className[256];
        // Get the class name of the window
        if (GetClassName(hwnd, className, sizeof(className) / sizeof(WCHAR))) {
            auto& processWindowClasses = *reinterpret_cast<std::map<DWORD, std::wstring>*>(lParam);
            // Map the process ID to its window class name
            processWindowClasses[processID] = className;
        }
        return TRUE; // Continue enumeration
        }, reinterpret_cast<LPARAM>(&processWindowClasses));

    return processWindowClasses;
}

int main() {
    std::vector<ProcessInfo> processes; // Vector to store information about processes
    DWORD processIDs[1024], cbNeeded, processCount;

    // Retrieve the list of process identifiers
    if (!EnumProcesses(processIDs, sizeof(processIDs), &cbNeeded)) {
        std::wcerr << L"Failed to enumerate processes." << std::endl;
        return 1;
    }

    processCount = cbNeeded / sizeof(DWORD); // Calculate the number of processes

    // Retrieve window class names for all processes
    auto processWindowClasses = GetProcessWindowClassNames();

    // Iterate through each process ID
    for (unsigned int i = 0; i < processCount; i++) {
        if (processIDs[i] != 0) {
            // Open the process with necessary access rights
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processIDs[i]);
            if (hProcess) {
                // Get the memory usage of the process
                SIZE_T memoryUsageKB = GetProcessMemoryUsage(hProcess);
                // Check if memory usage exceeds 200 MB
                if (memoryUsageKB > 200 * 1024) {
                    ProcessInfo pInfo;
                    pInfo.processID = processIDs[i];
                    // Get the process name
                    pInfo.processName = GetProcessName(hProcess);
                    pInfo.memoryUsageKB = memoryUsageKB;

                    // Retrieve the window class name if available
                    auto it = processWindowClasses.find(processIDs[i]);
                    if (it != processWindowClasses.end()) {
                        pInfo.windowClassName = it->second;
                    }
                    else {
                        pInfo.windowClassName = L"<no window>";
                    }

                    // Add the process information to the vector
                    processes.push_back(pInfo);
                }
                // Close the process handle to prevent memory leaks
                CloseHandle(hProcess);
            }
        }
    }

    // Display the processes using more than 200 MB of RAM
    std::wcout << L"Processes using more than 200 MB of RAM:" << std::endl;
    for (const auto& p : processes) {
        std::wcout << L"PID: " << p.processID
            << L"\tMemory Usage: " << p.memoryUsageKB << L" KB"
            << L"\tName: " << p.processName
            << L"\tWindow Class: " << p.windowClassName << std::endl;
    }

    return 0;
}
