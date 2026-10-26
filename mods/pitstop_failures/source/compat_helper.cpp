#include "pit_compat.h"
extern "C" __declspec(dllexport) int __cdecl PitCheckFile(const wchar_t* path) {
    try { return path && pit_compat::file(path)?1:0; } catch(...) { return 0; }
}
