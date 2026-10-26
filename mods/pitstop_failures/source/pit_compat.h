#pragma once
#include <windows.h>
#include <stdint.h>
#include <cstring>
#include <vector>
#include "pit_compat_profile.h"

// One policy for file-backed installer checks and the actual mapped game.
// No file identity, timestamp, checksum, entry-point or resource fingerprint.
namespace pit_compat {
struct Section { uint32_t va, extent, raw, length, flags; };
inline uint32_t u32(const unsigned char* p) { uint32_t v; std::memcpy(&v,p,4); return v; }
inline uint16_t u16(const unsigned char* p) { uint16_t v; std::memcpy(&v,p,2); return v; }
inline bool range(size_t start,size_t length,size_t size) { return start<=size && length<=size-start; }
inline bool readable(const void* address,void* out,size_t size) {
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),address,out,size,&copied) && copied==size;
}
inline bool check(const unsigned char* data,size_t length,const unsigned char* mapped=nullptr) {
    if(!data || length<64 || u16(data)!=0x5a4d) return false;
    uint32_t pe=u32(data+60);
    if(!range(pe,24,length) || u32(data+pe)!=0x4550 || u16(data+pe+4)!=0x14c) return false;
    unsigned count=u16(data+pe+6), optional=u16(data+pe+20);
    if(count==0 || count>96 || optional<224 || !range(pe+24,optional,length)) return false;
    const unsigned char* op=data+pe+24;
    uint32_t image_size=u32(op+56), header_size=u32(op+60);
    if(u16(op)!=0x10b || u32(op+28)!=0x400000 || (u16(op+70)&0x40) ||
       image_size<0x4d5a70 || image_size>128u*1024*1024 || header_size>image_size) return false;
    size_t table=(size_t)pe+24+optional;
    if(!range(table,count*40,length) || table+count*40>header_size || header_size>4096) return false;
    // module() verifies the actual load address; this parser also accepts
    // isolated mapped-byte fixtures which never execute game instructions.
    std::vector<Section> sections;
    for(unsigned i=0;i<count;i++) {
        const unsigned char* s=data+table+i*40;
        Section sec{u32(s+12),u32(s+8),u32(s+20),u32(s+16),u32(s+36)};
        uint32_t extent=sec.extent>sec.length?sec.extent:sec.length;
        if(!extent || sec.va<header_size || !range(sec.va,extent,image_size) ||
           (!range(sec.raw,sec.length,64u*1024*1024)) || (sec.length && sec.raw<header_size) || (!mapped && !range(sec.raw,sec.length,length))) return false;
        for(const auto& prior:sections) {
            uint32_t prior_extent=prior.extent>prior.length?prior.extent:prior.length;
            if(sec.va<prior.va+prior_extent && prior.va<sec.va+extent) return false;
            // Shipped EXEs legitimately alias raw backing in extra sections.
            // Virtual ranges must remain disjoint; every raw range is bounded.
        }
        sections.push_back(sec);
    }
    static const uint32_t required[][3]={{0x1000,0x19c5e6,0x60000000},
        {0x19e000,0x1f0a1,0x40000000},{0x1be000,0x317a70,0xc0000000}};
    for(const auto& expected:required) {
        unsigned matches=0;
        for(const auto& s:sections) if(s.va==expected[0] && s.extent==expected[1] &&
                                      (s.flags&0xe0000000)==expected[2]) matches++;
        if(matches!=1) return false;
    }
    for(const auto& probe:pit_code_probes) {
        unsigned matches=0;
        for(const auto& s:sections) {
            if(probe.rva<s.va || !range(probe.rva-s.va,probe.size,s.length)) continue;
            if(!(s.flags&0x20000000) || ++matches!=1) return false;
            if(mapped) {
                std::vector<unsigned char> bytes(probe.size);
                if(!readable(mapped+probe.rva,bytes.data(),bytes.size()) ||
                   std::memcmp(bytes.data(),probe.bytes,probe.size)) return false;
            } else {
                size_t offset=(size_t)s.raw+probe.rva-s.va;
                if(!range(offset,probe.size,length) || std::memcmp(data+offset,probe.bytes,probe.size)) return false;
            }
        }
        if(matches!=1) return false;
    }
    return true;
}
inline bool file(const wchar_t* path) {
    HANDLE handle=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(handle,&size) || size.QuadPart<64 || size.QuadPart>64*1024*1024) {CloseHandle(handle);return false;}
    std::vector<unsigned char> data((size_t)size.QuadPart);
    DWORD count=0; bool ok=ReadFile(handle,data.data(),(DWORD)data.size(),&count,nullptr)!=0;
    CloseHandle(handle);
    return ok && count==data.size() && check(data.data(),data.size());
}
inline bool module() {
    const auto* base=(const unsigned char*)GetModuleHandleW(nullptr);
    if((uintptr_t)base!=0x400000) return false;
    unsigned char headers[4096];
    return readable(base,headers,sizeof headers) && check(headers,sizeof headers,base);
}
}
