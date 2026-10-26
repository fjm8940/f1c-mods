#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

static bool pid_matches(DWORD wanted_pid, const char *wanted_name)
{
    wchar_t expected[MAX_PATH] = {};
    PROCESSENTRY32W entry = {};
    HANDLE snapshot;
    bool matched = false;
    if (!MultiByteToWideChar(CP_ACP, 0, wanted_name, -1, expected, MAX_PATH))
        return false;
    entry.dwSize = sizeof(entry);
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;
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

int main(int argc, char **argv)
{
    char *end = nullptr;
    char dll[MAX_PATH] = {};
    unsigned long parsed;
    DWORD pid;
    HANDLE process;
    SIZE_T length;
    void *remote;
    HANDLE thread;
    DWORD loaded = 0;
    union {
        FARPROC raw;
        LPTHREAD_START_ROUTINE thread;
    } load_library = {};

    if (argc != 4) {
        std::fprintf(stderr,
                     "usage: f1c_pid_injector.exe <dll> <pid> <expected-exe>\n");
        return 2;
    }
    parsed = std::strtoul(argv[2], &end, 10);
    if (!end || *end != '\0' || parsed == 0 || parsed > 0xFFFFFFFFUL) {
        std::fprintf(stderr, "invalid pid\n");
        return 2;
    }
    pid = static_cast<DWORD>(parsed);
    if (!pid_matches(pid, argv[3])) {
        std::fprintf(stderr, "pid does not identify the expected game process\n");
        return 2;
    }
    if (!GetFullPathNameA(argv[1], MAX_PATH, dll, nullptr) ||
        GetFileAttributesA(dll) == INVALID_FILE_ATTRIBUTES) {
        std::fprintf(stderr, "invalid dll path\n");
        return 2;
    }
    process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                              PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                              PROCESS_VM_READ,
                          FALSE, pid);
    if (!process) {
        std::fprintf(stderr, "OpenProcess failed (%lu)\n", GetLastError());
        return 1;
    }
    length = std::strlen(dll) + 1;
    remote = VirtualAllocEx(process, nullptr, length, MEM_COMMIT, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(process, remote, dll, length, nullptr)) {
        std::fprintf(stderr, "remote path write failed (%lu)\n", GetLastError());
        if (remote) VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    load_library.raw =
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    thread = CreateRemoteThread(process, nullptr, 0, load_library.thread,
                                remote, 0, nullptr);
    if (!thread) {
        std::fprintf(stderr, "CreateRemoteThread failed (%lu)\n", GetLastError());
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        return 1;
    }
    WaitForSingleObject(thread, INFINITE);
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
