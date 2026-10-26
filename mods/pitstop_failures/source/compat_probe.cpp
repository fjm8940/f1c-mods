#include "pit_compat.h"
#include <cstdio>
// Test both representations using the production checker, without running game code.
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    if(pit_compat::module()) {std::puts("wrong runtime module accepted");return 2;}
    FILE* f=std::fopen(argv[1],"rb"); if(!f) return 2;
    std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);
    if(size<64 || size>64*1024*1024){std::fclose(f);return 2;}
    std::vector<unsigned char> bytes((size_t)size);
    bool loaded=std::fread(bytes.data(),1,bytes.size(),f)==bytes.size();std::fclose(f);if(!loaded)return 2;
    bool disk=pit_compat::check(bytes.data(),bytes.size());
    // Malformed input is rejected before synthetic mapping; memory mutants of a
    // valid file are tested separately via the optional mapped corruption marker.
    if(!disk){std::puts("file=refused");return 1;}
    unsigned pe=pit_compat::u32(bytes.data()+60), opt=pit_compat::u16(bytes.data()+pe+20);
    unsigned extent=pit_compat::u32(bytes.data()+pe+24+56);
    auto* mapped=(unsigned char*)VirtualAlloc(nullptr,extent,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    if(!mapped){std::puts("map unavailable");return 2;}
    unsigned headers=pit_compat::u32(bytes.data()+pe+24+60);std::memcpy(mapped,bytes.data(),headers);
    for(unsigned i=0;i<pit_compat::u16(bytes.data()+pe+6);i++) {
        const auto* s=bytes.data()+pe+24+opt+i*40;
        if(pit_compat::u32(s+16))std::memcpy(mapped+pit_compat::u32(s+12),bytes.data()+pit_compat::u32(s+20),pit_compat::u32(s+16));
    }
    bool memory=pit_compat::check(mapped,4096,mapped);
    bool mutations=true;
    for(const auto& probe:pit_code_probes) {
        mapped[probe.rva]^=1;
        mutations=mutations && !pit_compat::check(mapped,4096,mapped);
        mapped[probe.rva]^=1;
    }
    VirtualFree(mapped,0,MEM_RELEASE);
    std::printf("file=accepted memory=%s mapped_mutations=%s\n",memory?"accepted":"refused",mutations?"refused":"MISSED");
    return memory&&mutations?0:1;
}
