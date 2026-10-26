// Qualifying-only reviewed adapter. No general SDK target policy is relaxed.
#pragma once
#include "f1c_hook.h"
#include "f1c_qualifying_fingerprints.hpp"

namespace f1c_qualifying {
// Reader returns false for any partially unreadable range. Used unchanged by
// live memory and offline mapped-PE tests. Metadata unrelated to layout is ignored.
template<class Reader> bool compatible(uintptr_t base, Reader read)
{
    if(base != F1C_TARGET_IMAGE_BASE) return false;
    IMAGE_DOS_HEADER dos = {};
    if(!read(base,&dos,sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE
       || dos.e_lfanew < (LONG)sizeof(dos) || dos.e_lfanew > 0x100000) return false;
    IMAGE_NT_HEADERS32 nt = {};
    if(!read(base+(uintptr_t)dos.e_lfanew,&nt,sizeof(nt))
       || nt.Signature != IMAGE_NT_SIGNATURE
       || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386
       || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32)
       || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC
       || nt.OptionalHeader.ImageBase != base
       || nt.OptionalHeader.SizeOfImage < (uintptr_t)G_SessionClockSeconds+sizeof(double)-base
       || nt.OptionalHeader.SizeOfImage > 0x40000000u) return false;
    for(const auto &fingerprint : fingerprints) {
        uint8_t actual[2048];
        if(fingerprint.size > sizeof(actual)
           || fingerprint.address < base
           || fingerprint.address-base+fingerprint.size > nt.OptionalHeader.SizeOfImage
           || !read(fingerprint.address,actual,fingerprint.size)
           || memcmp(actual,fingerprint.bytes,fingerprint.size)) return false;
    }
    // All vehicle slots and each global consumed by the guard must be mapped.
    const uintptr_t globals[] = {(uintptr_t)G_SessionType,(uintptr_t)G_RacePhase,
        (uintptr_t)G_ChampionshipMode,(uintptr_t)G_SessionStartSeconds,
        (uintptr_t)G_SessionEndSeconds,(uintptr_t)G_VehicleCount};
    uint32_t value;
    for(auto address : globals) if(!read(address,&value,sizeof(value))) return false;
    double clock;
    if(!read((uintptr_t)G_SessionClockSeconds,&clock,sizeof(clock))) return false;
    uint8_t page[4096];
    for(size_t offset=0; offset<VEH_STRIDE*VEH_SLOTS; offset+=sizeof(page)) {
        const size_t remaining=VEH_STRIDE*VEH_SLOTS-offset;
        if(!read(VEH_BASE+offset,page,remaining<sizeof(page)?remaining:sizeof(page))) return false;
    }
    return read((uintptr_t)G_VehiclePtrArray,page,VEH_SLOTS*sizeof(uint32_t));
}

inline bool compatible_live()
{
    return compatible((uintptr_t)GetModuleHandleA(NULL),
        [](uintptr_t address,void *buffer,size_t size) {
            SIZE_T received=0;
            return ReadProcessMemory(GetCurrentProcess(),(void*)address,buffer,size,&received)
                && received==size;
        });
}

// Called only through the broker's game-thread dispatch; revalidate immediately
// before writing. One six-byte, position-independent instruction is stolen.
inline F1CDetourResult install(F1CDetour *hook,void *replacement)
{
    if(!compatible_live()) return F1C_DETOUR_WRONG_TARGET;
    return f1c_detour_install_prevalidated_layout(hook,
        (void*)fingerprints[0].address,replacement,fingerprints[0].bytes,6);
}
}
