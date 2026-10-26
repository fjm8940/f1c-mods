#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

static bool pid_matches(DWORD wanted_pid, const char* wanted_name) {
    wchar_t expected[MAX_PATH] = {};
    if (!MultiByteToWideChar(CP_ACP, 0, wanted_name, -1, expected, MAX_PATH)) return false;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
    bool matched = false;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (entry.th32ProcessID == wanted_pid &&
                _wcsicmp(entry.szExeFile, expected) == 0) {
                matched = true;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return matched;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: f1c_pid_injector.exe <dll> <pid> <expected-exe>\n");
        return 2;
    }
    char* end = nullptr;
    unsigned long parsed = std::strtoul(argv[2], &end, 10);
    if (!end || *end != '\0' || parsed == 0 || parsed > 0xFFFFFFFFUL) {
        std::fprintf(stderr, "invalid pid\n");
        return 2;
    }
    DWORD pid = static_cast<DWORD>(parsed);
    if (!pid_matches(pid, argv[3])) {
        std::fprintf(stderr, "pid does not identify the expected game process\n");
        return 2;
    }

    char dll[MAX_PATH] = {};
    if (!GetFullPathNameA(argv[1], MAX_PATH, dll, nullptr) ||
        GetFileAttributesA(dll) == INVALID_FILE_ATTRIBUTES) {
        std::fprintf(stderr, "invalid dll path\n");
        return 2;
    }
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                                 FALSE, pid);
    if (!process) {
        std::fprintf(stderr, "OpenProcess failed (%lu)\n", GetLastError());
        return 1;
    }
    SIZE_T length = std::strlen(dll) + 1;
    void* remote = VirtualAllocEx(process, nullptr, length, MEM_COMMIT, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(process, remote, dll, length, nullptr)) {
        std::fprintf(stderr, "remote path write failed (%lu)\n", GetLastError());
        if (remote) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    union {
        FARPROC raw;
        LPTHREAD_START_ROUTINE thread;
    } load_library = {};
    load_library.raw = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE thread = CreateRemoteThread(process, nullptr, 0, load_library.thread, remote, 0, nullptr);
    if (!thread) {
        std::fprintf(stderr, "CreateRemoteThread failed (%lu)\n", GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    WaitForSingleObject(thread, INFINITE);
    DWORD loaded = 0;
    GetExitCodeThread(thread, &loaded);
    CloseHandle(thread);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);
    if (!loaded) {
        std::fprintf(stderr, "LoadLibraryA failed in target process\n");
        return 1;
    }
    std::printf("injected into pid %lu\n", static_cast<unsigned long>(pid));
    return 0;
}
