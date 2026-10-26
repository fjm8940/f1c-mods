#pragma once
#include "f1c_hook.h"
#include "f1c_vertex_pack_bytes.h"

/* Private trial adapter, x86 GCC. The hook is INSIDE the renderer-zero branch,
 * after native locking and dirty/prepared handling. Six stolen bytes are whole
 * non-relative instructions: mov edx,[esi+38h]; mov eax,[esi+20h].
 * Success rejoins BEFORE the native optional effect and unlock. Fallback
 * executes the exact stolen instructions and the original packing/lighting.
 * No native function is invoked through a guessed C ABI. Callback must use
 * integer-only operations; pushad/pushfd preserve all entry CPU state.
 * Install from broker game-thread dispatch only; keep module pinned to exit.
 */
typedef int (__cdecl *F1CVertexTry)(uint32_t *);
static F1CDetour f1c_vertex_hook;
static F1CVertexTry f1c_vertex_callback;
extern "C" {
static void *f1c_vertex_resume __asm__("_f1c_vertex_resume") __attribute__((used));
static void *f1c_vertex_after __asm__("_f1c_vertex_after") __attribute__((used));
static int __attribute__((noinline,used,force_align_arg_pointer))
f1c_vertex_dispatch(uint32_t *group) { return f1c_vertex_callback(group); }
static void __attribute__((naked,used)) f1c_vertex_entry(void) {
    __asm__ __volatile__(
        "pushfl\n\tcmpl $128,8(%esi)\n\tjl 2f\n\ttestb $3,(%esi)\n\tjnz 2f\n\t"
        "pushal\n\tpushl %esi\n\tcall _f1c_vertex_dispatch\n\t"
        "addl $4,%esp\n\ttestl %eax,%eax\n\tjz 1f\n\t"
        "popal\n\tpopfl\n\tjmp *_f1c_vertex_after\n\t"
        "1: popal\n\t2: popfl\n\tmovl 0x38(%esi),%edx\n\t"
        "movl 0x20(%esi),%eax\n\tjmp *_f1c_vertex_resume\n\t");
}
}
static bool f1c_vertex_identity(HMODULE image) {
    if ((uintptr_t)image!=0x400000u) return false;
    const IMAGE_DOS_HEADER *dos=(const IMAGE_DOS_HEADER *)image;
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>4096) return false;
    const IMAGE_NT_HEADERS32 *nt=(const IMAGE_NT_HEADERS32 *)((const BYTE *)image+dos->e_lfanew);
    if (nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.ImageBase!=0x400000u || nt->OptionalHeader.SizeOfImage<0x15da45u) return false;
    return !memcmp((const BYTE *)image+0x15d720u,f1c_vertex_native_code,sizeof(f1c_vertex_native_code));
}
static int f1c_vertex_install(F1CVertexTry callback,DWORD game_thread) {
    if (!callback || !game_thread || GetCurrentThreadId()!=game_thread || f1c_vertex_hook.installed ||
        !f1c_vertex_identity(GetModuleHandleA(NULL))) return 0;
    f1c_vertex_callback=callback;
    f1c_vertex_resume=(void *)0x55d83au;
    f1c_vertex_after=(void *)0x55d8c7u;
    return f1c_detour_install_prevalidated_layout(&f1c_vertex_hook,(void *)0x55d834u,
        (void *)f1c_vertex_entry,f1c_vertex_native_code+0x114u,6)==F1C_DETOUR_OK;
}
