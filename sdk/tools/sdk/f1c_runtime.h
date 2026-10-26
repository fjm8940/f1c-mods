/* Safe runtime boundary for native F1C mods. Windows/x86 only. */
#pragma once
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "f1c_sdk_version.h"
#include "f1c_target.h"
#include "f1c_sdk.h"

#if defined(_MSC_VER)
#define F1C_RT_INLINE static __inline
#else
#define F1C_RT_INLINE static inline
#endif

typedef enum F1CTargetResult {
    F1C_TARGET_OK = 0,
    F1C_TARGET_NO_MODULE,
    F1C_TARGET_WRONG_BASE,
    F1C_TARGET_BAD_DOS,
    F1C_TARGET_BAD_PE,
    F1C_TARGET_WRONG_MACHINE,
    F1C_TARGET_WRONG_TIMESTAMP,
    F1C_TARGET_WRONG_ENTRY,
    F1C_TARGET_WRONG_IMAGE_SIZE,
    F1C_TARGET_WRONG_CHECKSUM,
    F1C_TARGET_CODE_PATCHED
} F1CTargetResult;

F1C_RT_INLINE const char *f1c_target_result_string(F1CTargetResult result)
{
    static const char *const names[] = {
        "compatible", "main module unavailable", "unexpected image base",
        "invalid DOS header", "invalid PE header", "wrong machine",
        "wrong timestamp", "wrong entry point", "wrong image size",
        "wrong PE checksum", "fingerprint bytes already patched"
    };
    return (unsigned)result < sizeof(names) / sizeof(names[0])
        ? names[(unsigned)result] : "unknown target error";
}

F1C_RT_INLINE F1CTargetResult f1c_target_validate_headers(void)
{
    HMODULE module = GetModuleHandleA(NULL);
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    if (!module) return F1C_TARGET_NO_MODULE;
    if ((uintptr_t)module != F1C_TARGET_IMAGE_BASE) return F1C_TARGET_WRONG_BASE;
    dos = (const IMAGE_DOS_HEADER*)module;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return F1C_TARGET_BAD_DOS;
    nt = (const IMAGE_NT_HEADERS32*)((const uint8_t*)module + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return F1C_TARGET_BAD_PE;
    if (nt->FileHeader.Machine != F1C_TARGET_MACHINE) return F1C_TARGET_WRONG_MACHINE;
    if (nt->FileHeader.TimeDateStamp != F1C_TARGET_TIMESTAMP) return F1C_TARGET_WRONG_TIMESTAMP;
    if (nt->OptionalHeader.AddressOfEntryPoint != F1C_TARGET_ENTRY_RVA)
        return F1C_TARGET_WRONG_ENTRY;
    if (nt->OptionalHeader.SizeOfImage != F1C_TARGET_IMAGE_SIZE)
        return F1C_TARGET_WRONG_IMAGE_SIZE;
    /* A sandbox may name one reviewed layout-compatible checksum at compile
     * time. It must independently verify the complete executable identity
     * before installing hooks; this header allowance is not variant proof. */
    if (nt->OptionalHeader.CheckSum != F1C_TARGET_CHECKSUM
#if defined(F1C_TARGET_ADDITIONAL_CHECKSUM)
        && nt->OptionalHeader.CheckSum != F1C_TARGET_ADDITIONAL_CHECKSUM
#endif
        )
        return F1C_TARGET_WRONG_CHECKSUM;
    return F1C_TARGET_OK;
}

F1C_RT_INLINE F1CTargetResult f1c_target_validate_strict(void)
{
    F1CTargetResult result = f1c_target_validate_headers();
    if (result != F1C_TARGET_OK) return result;
    if (memcmp((const void*)(uintptr_t)F1C_TARGET_PROBE_ADDRESS,
               F1C_TARGET_PROBE_BYTES, F1C_TARGET_PROBE_LENGTH) != 0)
        return F1C_TARGET_CODE_PATCHED;
    return F1C_TARGET_OK;
}

F1C_RT_INLINE void *f1c_vehicle_slot(int slot)
{
    if (slot < 0 || slot >= VEH_SLOTS) return NULL;
    return (void*)VEH(slot);
}

/* SessionStateTransition owns the controller subobject at F1CVehicle+0x0C,
 * not the vehicle base. Recovered callers construct this exact interior
 * pointer before the thiscall. */
F1C_RT_INLINE void *f1c_vehicle_controller(void *vehicle)
{
    return vehicle ? (void *)((uint8_t *)vehicle + 0x0Cu) : NULL;
}

F1C_RT_INLINE int f1c_vehicle_slot_live(int slot)
{
    return f1c_vehicle_slot(slot) != NULL && VEH_ACTIVE(slot);
}

/* Reviewed target globals used only by game-thread field construction. These
 * accessors keep target-specific addresses inside the canonical SDK boundary. */
F1C_RT_INLINE int32_t f1c_vehicle_count_get(void)
{
    return *(volatile int32_t *)(uintptr_t)0x005C329Cu;
}

/* Owner view used by the stock grid-position transition functions. The
 * plannedPositionAtStart field is grid_state+0x24. */
F1C_RT_INLINE void *f1c_vehicle_grid_state(void *vehicle)
{
    return vehicle ? (void *)((uint8_t *)vehicle + 0x4034u) : NULL;
}

F1C_RT_INLINE void f1c_vehicle_count_set(int32_t count)
{
    *(volatile int32_t *)(uintptr_t)0x005C329Cu = count;
}

F1C_RT_INLINE int32_t f1c_vehicle_lookup_index_get(void)
{
    return *(volatile int32_t *)(uintptr_t)0x008AC38Cu;
}

F1C_RT_INLINE void f1c_vehicle_lookup_index_set(int32_t index)
{
    *(volatile int32_t *)(uintptr_t)0x008AC38Cu = index;
}

/* Single-producer/single-consumer queue. The producer may be a worker thread.
 * f1c_dispatch_drain MUST be called by the SDK-owned game-update hook broker;
 * individual mods must not install competing detours at the internal anchor. */
typedef void (*F1CGameTaskFn)(void *context);
typedef struct F1CGameTask {
    F1CGameTaskFn function;
    void *context;
} F1CGameTask;

#define F1C_DISPATCH_CAPACITY 64
typedef struct F1CGameDispatch {
    volatile LONG write_index;
    volatile LONG read_index;
    DWORD game_thread_id;
    F1CGameTask tasks[F1C_DISPATCH_CAPACITY];
} F1CGameDispatch;

F1C_RT_INLINE void f1c_dispatch_init(F1CGameDispatch *queue)
{
    if (!queue) return;
    memset(queue, 0, sizeof(*queue));
}

F1C_RT_INLINE int f1c_dispatch_post(
    F1CGameDispatch *queue, F1CGameTaskFn function, void *context)
{
    if (!queue) return 0;
    LONG write_index = queue->write_index;
    LONG next = (write_index + 1) % F1C_DISPATCH_CAPACITY;
    if (!function || next == queue->read_index) return 0;
    queue->tasks[write_index].function = function;
    queue->tasks[write_index].context = context;
    MemoryBarrier();
    InterlockedExchange(&queue->write_index, next);
    return 1;
}

F1C_RT_INLINE unsigned f1c_dispatch_drain_bounded(
    F1CGameDispatch *queue, unsigned max_tasks)
{
    unsigned count = 0;
    LONG read_index;
    if (!queue || !max_tasks) return 0;
    if (!queue->game_thread_id) queue->game_thread_id = GetCurrentThreadId();
    if (queue->game_thread_id != GetCurrentThreadId()) return 0;
    while (count < max_tasks &&
           (read_index = queue->read_index) != queue->write_index) {
        F1CGameTask task = queue->tasks[read_index];
        InterlockedExchange(&queue->read_index,
                            (read_index + 1) % F1C_DISPATCH_CAPACITY);
        task.function(task.context);
        ++count;
    }
    return count;
}

/* Compatibility helper with a strict upper bound. A broker that needs a
 * smaller per-update budget should call f1c_dispatch_drain_bounded directly. */
F1C_RT_INLINE unsigned f1c_dispatch_drain(F1CGameDispatch *queue)
{
    return f1c_dispatch_drain_bounded(queue, F1C_DISPATCH_CAPACITY - 1u);
}

#undef F1C_RT_INLINE
