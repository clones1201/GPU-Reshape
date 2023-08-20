// 
// The MIT License (MIT)
// 
// Copyright (c) 2023 Miguel Petersen
// Copyright (c) 2023 Advanced Micro Devices, Inc
// Copyright (c) 2023 Avalanche Studios Group
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy 
// of this software and associated documentation files (the "Software"), to deal 
// in the Software without restriction, including without limitation the rights 
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
// of the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE 
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, 
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 

// Bootstrapper
#include <Backends/DX12/RelFunTBL.h>
#include <Backends/DX12/Shared.h>

// Common
#include <Common/IPGlobalLock.h>
#include <Common/FileSystem.h>
#include <Common/GlobalUID.h>
#include <Common/String.h>

// Std
#include <Psapi.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <tlhelp32.h>

/// Use bootstrapper sessioning, useful for iteration
#define USE_BOOTSTRAP_SESSIONS 0

/// Hook all running processes
#define HOOK_ALL_RUNNING 1

/// Shared Win32 hook
HHOOK Hook;

/// Cached x86 function table
X86RelFunTBL X86Table{};

/// Graceful exit handler
BOOL CtrlHandler(DWORD event) {
    if (event == CTRL_CLOSE_EVENT) {
        // Graceful cleanup
        if (Hook) {
            UnhookWindowsHookEx(Hook);
        }

        // OK
        return TRUE;
    }

    // Ignore
    return FALSE;
}

/// Naive pump for hooks
void MessagePump() {
    // Pooled message
    MSG message;

    // Keep the message pump active for all hooked applications
    for (;;) {
        int32_t pumpResult = GetMessage(&message, NULL, 0, 0);
        switch (pumpResult) {
            default:
                TranslateMessage(&message);
                DispatchMessage(&message);

                // Erroneous
            case 0:
            case -1:
                return;
        }
    }
}

bool RemoteLoadBootstrapper(PTHREAD_START_ROUTINE loadLibraryAGPA, const std::string& sessionPathStrX64, const std::string& sessionPathStrX86, DWORD processId) {
    // Try to open process
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, false, processId);
    if (!process) {
        return false;
    }

    // Determine if process is wow64
    BOOL isWow64;
    if (!IsWow64Process(process, &isWow64)) {
        return false;
    }

    // Determine session path
    std::string_view sessionPathStr;
    if (isWow64) {
        std::cout << "[Wow64] ";
        sessionPathStr = sessionPathStrX86;
    } else {
        sessionPathStr = sessionPathStrX64;
    }
    
    // Try to allocate path memory
    void* remoteSessionPathA = VirtualAllocEx(process, nullptr, sessionPathStr.length() + 1u, MEM_COMMIT, PAGE_READWRITE);
    if (!remoteSessionPathA) {
        DWORD error = GetLastError();
        return false;
    }

    // Try to write path
    if (!WriteProcessMemory(process, remoteSessionPathA, sessionPathStr.data(), sessionPathStr.length() + 1u, nullptr)) {
        return false;
    }

    // Use X86 function table if SysWow64
    if (isWow64) {
        // Check the table is valid
        if (!X86Table.kernel32LoadLibraryA) {
            return false;
        }

        // Assume from table
        loadLibraryAGPA = reinterpret_cast<PTHREAD_START_ROUTINE>(static_cast<uint64_t>(X86Table.kernel32LoadLibraryA));
    }

    // Load bootstrapper
    return CreateRemoteThread(process, nullptr, 0, loadLibraryAGPA, remoteSessionPathA, 0, nullptr) != nullptr;
}

bool RemoteLoadBootstrapperGlobal(const std::string& sessionPathStrX64, const std::string& sessionPathStrX86) {
    // Create snapshot
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    // Entry information
    PROCESSENTRY32 entry{sizeof(PROCESSENTRY32)};

    // Get kernel32 module for LoadLibraryA
    HMODULE kernel32Handle = GetModuleHandleA("kernel32.dll");
    if (!kernel32Handle) {
        return false;
    }

    // Get the LoadLibraryA GPA
    auto loadLibraryWGPA = reinterpret_cast<PTHREAD_START_ROUTINE>(GetProcAddress(kernel32Handle, "LoadLibraryA"));
    if (!loadLibraryWGPA) {
        return false;
    }

    // Now walk the snapshot of processes
    for (bool result = Process32First(snapshot, &entry); result; result = Process32Next(snapshot, &entry)) {
        std::cout << "\t Hooking process '" << entry.szExeFile << "'... ";

        // Try to bootstrap
        if (RemoteLoadBootstrapper(loadLibraryWGPA, sessionPathStrX64, sessionPathStrX86, entry.th32ProcessID)) {
            std::cout << "OK\n";
        } else {
            std::cout << "Failed!\n";
        }
    }

    // Cleanup
    CloseHandle(snapshot);

    // OK
    return true;
}

bool DisplayHelp() {
    // Print help
    std::cout << "Help\n";
    std::cout << "\t<no arguments> - Run the bootstrapping service\n";
    std::cout << "\t       release - Release all bootstrapped processes\n";
    return true;
}

bool CacheRelFunTBL() {
    // Table generator ocmmand line
    std::string cmd = "GRS.Backends.DX12.Service.RelFunTBL.exe";

    // Startup information
    STARTUPINFO info{};
    info.cb = sizeof(info);
    
    // Launch table generator
    PROCESS_INFORMATION processInfo;
    if (!CreateProcess(
        nullptr,
        cmd.data(),
        nullptr,
        nullptr,
        false,
        0,
        nullptr,
        nullptr,
        &info,
        &processInfo
    )) {
        return false;
    }

    // Wait for process
    WaitForSingleObject(processInfo.hProcess, INFINITE);

    // Validate exit code
    DWORD exitCode;
    if (!GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
        exitCode = 1u;
    }

    // Cleanup
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    // Succeeded?
    if (exitCode != 0) {
        return false;
    }

    // Open table data
    std::ifstream stream(GetIntermediatePath("Interop") / "X86RelFunTbl.dat", std::ios_base::binary);
    if (!stream.good()) {
        return false;
    }

    // Stream in table
    stream.read(reinterpret_cast<char*>(&X86Table), sizeof(X86Table));
    stream.close();

    // Validate
    if (!X86Table.kernel32LoadLibraryA || !X86Table.kernel32FreeLibrary) {
        return false;
    }

    // OK
    return true;
}

void ReleaseBootstrappedProcess(PTHREAD_START_ROUTINE freeLibraryGPA, const char* processName, DWORD processId) {    
    // Try to open process
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, false, processId);
    if (!process) {
        return;
    }
    
    // Determine if process is wow64
    BOOL isWow64;
    if (!IsWow64Process(process, &isWow64)) {
        return;
    }
    
    // Use X86 function table if SysWow64
    if (isWow64) {
        // Check the table is valid
        if (!X86Table.kernel32FreeLibrary) {
            return;
        }

        // Assume from table
        freeLibraryGPA = reinterpret_cast<PTHREAD_START_ROUTINE>(static_cast<uint64_t>(X86Table.kernel32FreeLibrary));
    }

    // Determine needed byte count
    DWORD needed{0};
    if (!EnumProcessModules(process, nullptr, 0, &needed)) {
        return;
    }

    // Get all modules
    std::vector<HMODULE> modules(needed / sizeof(HMODULE), nullptr);
    if (!EnumProcessModules(process, modules.data(), static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &needed)) {
        return;
    }

    // Check all modules
    for (HMODULE module : modules) {
        // Try to get name
        char name[1024];
        if (!GetModuleFileNameEx(process, module, name, sizeof(name))) {
            DWORD error = GetLastError();
            continue;
        }

        // Not the bootstrapper?
        if (!std::ends_with(name, "GRS.Backends.DX12.BootstrapperX64.dll") && !std::ends_with(name, "GRS.Backends.DX12.BootstrapperX32.dll")) {
            continue;
        }

        // Diagnostic
        std::cout << "\t Releasing bootstrapped process '" << processName << "'... ";

        // Try to unload
        if (CreateRemoteThread(process, nullptr, 0, freeLibraryGPA, module, 0u, nullptr)) {
            std::cout << "OK\n";
        } else {
            std::cout << "Failed!\n";
        }
    }
}

bool ReleaseBootstrappers() {
    std::cout << "Releasing bootstrapped processes.\n";
    
    // Create snapshot
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to create snapshot!\n";
        return false;
    }

    // Entry information
    PROCESSENTRY32 entry{sizeof(PROCESSENTRY32)};

    // Get kernel32 module for LoadLibraryA
    HMODULE kernel32Handle = GetModuleHandleA("kernel32.dll");
    if (!kernel32Handle) {
        std::cout << "Failed to open kernel32!\n";
        return false;
    }

    // Get the LoadLibraryA GPA
    auto freeLibrary = reinterpret_cast<PTHREAD_START_ROUTINE>(GetProcAddress(kernel32Handle, "FreeLibrary"));
    if (!freeLibrary) {
        std::cout << "Failed to GPA FreeLibrary!\n";
        return false;
    }

    // Now walk the snapshot of processes
    for (bool result = Process32First(snapshot, &entry); result; result = Process32Next(snapshot, &entry)) {
        ReleaseBootstrappedProcess(freeLibrary, entry.szExeFile, entry.th32ProcessID);
    }

    // Cleanup
    CloseHandle(snapshot);

    // OK
    return true;
}

int main(int32_t argc, const char *const *argv) {
    std::cout << "GPUOpen DX12 Service\n" << std::endl;

    // Unsupported format
    if (argc > 3) {
        std::cout << "Unexpected command line format, see help." << std::endl;
        return 1u;
    }

    // Optional modes
    if (argc > 1) {
        if (!std::strcmp(argv[1], "help")) {
            return !DisplayHelp();
        } else if (!std::strcmp(argv[1], "release")) {
            return !ReleaseBootstrappers();
        } else {
            std::cout << "Unknown command, see help." << std::endl;
            return 1u;
        }
    }

    // Diagnostic
    std::cout << "Getting X86 function table... " << std::flush;

    // Try to cache the x86 table
    if (CacheRelFunTBL()) {
        std::cout << "OK" << std::endl;
    } else {
        std::cout << "Failed, SysWow64 processes will be skipped!" << std::endl;
    }

    // No special requests, run service
    std::cout << "Initializing global lock... " << std::flush;

    // Set handlers
    SetConsoleCtrlHandler(reinterpret_cast<PHANDLER_ROUTINE>(CtrlHandler), true);

    // Try to acquire lock
    IPGlobalLock globalLock;
    if (!globalLock.Acquire(kSharedD3D12ServiceMutexName, true)) {
        std::cerr << "Failed to open or create shared mutex '" << kSharedD3D12ServiceMutexName << "'" << std::endl;

#ifndef NDEBUG
        std::cin.ignore();
#endif
        return 1;
    }

    std::cout << "OK" << std::endl;

#if USE_BOOTSTRAP_SESSIONS
    // Host all sessions under intermediate
    std::filesystem::path sessionDir = GetIntermediatePath("Bootstrapper\\Sessions");

    // Clean up all old sessions
    for (std::filesystem::path file: std::filesystem::directory_iterator(sessionDir)) {
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
    }

    // Create unique name
    std::string sessionNameX64 = "GRS.Backends.DX12.BootstrapperX64 " + GlobalUID::New().ToString() + ".dll";
    std::string sessionNameX86 = "GRS.Backends.DX12.BootstrapperX86 " + GlobalUID::New().ToString() + ".dll";

    // Copy the bootstrapper to a new session, makes handling unique sessions somewhat bearable (certain programs refuse to let go of handle)
    std::filesystem::path sessionPathX64 = sessionDir / sessionNameX64;
    std::filesystem::path sessionPathX86 = sessionDir / sessionNameX86;

    // Copy current bootstrapper
    std::filesystem::copy(GetCurrentModuleDirectory() / "GRS.Backends.DX12.BootstrapperX64.dll", sessionPathX64);
    std::filesystem::copy(GetCurrentModuleDirectory() / "GRS.Backends.DX12.BootstrapperX32.dll", sessionPathX86);
#else // USE_BOOTSTRAP_SESSIONS
    // No sessions
    std::filesystem::path sessionPathX64 = GetCurrentModuleDirectory() / "GRS.Backends.DX12.BootstrapperX64.dll";
    std::filesystem::path sessionPathX86 = GetCurrentModuleDirectory() / "GRS.Backends.DX12.BootstrapperX32.dll";
#endif // USE_BOOTSTRAP_SESSIONS

    // To string
    std::string sessionPathStrX64 = sessionPathX64.string();
    std::string sessionPathStrX86 = sessionPathX86.string();

    // Hook all running?
#if HOOK_ALL_RUNNING
    std::cout << "Hooking all running..." << std::endl;

    // Bootstrap everything!
    if (!RemoteLoadBootstrapperGlobal(sessionPathStrX64, sessionPathStrX86)) {
        std::cout << "Failed to remote start bootstrappers!" << std::endl;
    }

    // Flush console
    std::cout.flush();
#endif // HOOK_ALL_RUNNING

    // Load the boostrapper
    HMODULE bootstrapperModule = LoadLibraryW(sessionPathX64.wstring().c_str());
    if (!bootstrapperModule) {
        std::cerr << "Failed to open bootstrapper" << std::endl;

#ifndef NDEBUG
        std::cin.ignore();
#endif
        return 1;
    }

    // Get hook proc
    auto gpa = reinterpret_cast<HOOKPROC>(GetProcAddress(bootstrapperModule, "WinHookAttach"));
    if (!gpa) {
        std::cerr << "Failed to get bootstrapper WinHookAttach address" << std::endl;

#ifndef NDEBUG
        std::cin.ignore();
#endif
        return 1;
    }

    // Attempt to attach global hook
    Hook = SetWindowsHookEx(WH_CBT, gpa, bootstrapperModule, 0);
    if (!Hook) {
        DWORD error = GetLastError();

        // Format the underlying error
        char buffer[1024];
        FormatMessage(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            buffer, sizeof(buffer), NULL
        );

        std::cerr << "Failed to attach global hook: " << buffer << std::endl;

#ifndef NDEBUG
        std::cin.ignore();
#endif
        return 1;
    }

    // Hold and start pump
    std::cout << "Holding hook..." << std::endl;
    MessagePump();

    // Cleanup
    UnhookWindowsHookEx(Hook);

    // OK
    std::cout << "DX12 service shutdown" << std::endl;
    return 0;
}
