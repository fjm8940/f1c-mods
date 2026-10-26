#ifndef SC_READ_SCOPE_H
#define SC_READ_SCOPE_H
#include <windows.h>
#include <stdint.h>
#include <string.h>
/* Game-thread-only, permission metadata only: never cache vehicle contents.
 * Scope must enclose a synchronous observation with no native simulation,
 * allocator, teardown, or hook calls. Never retain regions across scopes. */
static struct {
    unsigned depth,count;
    DWORD thread;
    struct {uintptr_t first,last;} region[16];
} sc_read_scope;
static void sc_read_scope_begin(void) {
    if(!sc_read_scope.depth){sc_read_scope.count=0;sc_read_scope.thread=GetCurrentThreadId();}
    ++sc_read_scope.depth;
}
static void sc_read_scope_end(void) {
    if(sc_read_scope.depth && !--sc_read_scope.depth){sc_read_scope.count=0;sc_read_scope.thread=0;}
}
static int sc_read_scope_copy(uintptr_t address,void*out,size_t bytes) {
    MEMORY_BASIC_INFORMATION mi;
    if(!address||!out||!bytes||address+bytes<address)return 0;
    int scoped=sc_read_scope.depth&&sc_read_scope.thread==GetCurrentThreadId();
    if(scoped)for(unsigned i=0;i<sc_read_scope.count;i++){
        if(address>=sc_read_scope.region[i].first && address+bytes<=sc_read_scope.region[i].last){
            memcpy(out,(const void*)address,bytes);return 1;
        }
    }
    if(!VirtualQuery((void*)address,&mi,sizeof(mi))||mi.State!=MEM_COMMIT||
       (mi.Protect&(PAGE_NOACCESS|PAGE_GUARD)) ||
       !(mi.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)) ||
       address+bytes>(uintptr_t)mi.BaseAddress+mi.RegionSize)return 0;
    if(scoped && sc_read_scope.count<16){
        unsigned i=sc_read_scope.count++;
        sc_read_scope.region[i].first=(uintptr_t)mi.BaseAddress;
        sc_read_scope.region[i].last=(uintptr_t)mi.BaseAddress+mi.RegionSize;
    }
    memcpy(out,(const void*)address,bytes);return 1;
}
#endif
