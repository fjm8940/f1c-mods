/* Reviewed-boundary x86 detour helper. The caller must supply whole-instruction
 * stolen bytes and their exact expected contents; mismatch fails closed. */
#pragma once
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "f1c_runtime.h"

#define F1C_DETOUR_MAX_STOLEN 32
#define F1C_HOOK_REGISTRY_CAPACITY 32

typedef enum F1CDetourResult {
    F1C_DETOUR_OK = 0,
    F1C_DETOUR_BAD_ARGUMENT,
    F1C_DETOUR_WRONG_TARGET,
    F1C_DETOUR_BYTES_MISMATCH,
    F1C_DETOUR_ALLOC_FAILED,
    F1C_DETOUR_RANGE_ERROR,
    F1C_DETOUR_PROTECT_FAILED,
    F1C_DETOUR_ALREADY_INSTALLED,
    F1C_DETOUR_NOT_INSTALLED
} F1CDetourResult;

typedef struct F1CDetour {
    uint8_t *target;
    void *replacement;
    uint8_t original[F1C_DETOUR_MAX_STOLEN];
    size_t stolen_length;
    void *trampoline;
    int installed;
} F1CDetour;

typedef struct F1CHookRegistry {
    F1CDetour *hooks[F1C_HOOK_REGISTRY_CAPACITY];
    size_t count;
} F1CHookRegistry;

#if defined(_MSC_VER)
#define F1C_HOOK_INLINE static __inline
#else
#define F1C_HOOK_INLINE static inline
#endif

F1C_HOOK_INLINE int f1c_rel32(const void *from_after_jump,
                              const void *destination, int32_t *result)
{
    intptr_t delta = (const uint8_t*)destination -
                     (const uint8_t*)from_after_jump;
    if (delta < INT32_MIN || delta > INT32_MAX) return 0;
    *result = (int32_t)delta;
    return 1;
}

F1C_HOOK_INLINE F1CDetourResult f1c_detour_install_internal(
    F1CDetour *hook, void *target, void *replacement,
    const void *expected, size_t stolen_length, int layout_prevalidated)
{
    DWORD old_protection;
    DWORD ignored;
    int32_t to_replacement;
    int32_t to_return;
    uint8_t *trampoline;
    F1CTargetResult target_result;
    if (!hook || !target || !replacement || !expected || stolen_length < 5 ||
        stolen_length > F1C_DETOUR_MAX_STOLEN)
        return F1C_DETOUR_BAD_ARGUMENT;
    if (hook->installed) return F1C_DETOUR_ALREADY_INSTALLED;
    if (!layout_prevalidated) {
        target_result = f1c_target_validate_headers();
        if (target_result != F1C_TARGET_OK) return F1C_DETOUR_WRONG_TARGET;
    }
    if (memcmp(target, expected, stolen_length) != 0)
        return F1C_DETOUR_BYTES_MISMATCH;

    trampoline = (uint8_t*)VirtualAlloc(NULL, stolen_length + 5,
                                        MEM_COMMIT | MEM_RESERVE,
                                        PAGE_EXECUTE_READWRITE);
    if (!trampoline) return F1C_DETOUR_ALLOC_FAILED;
    if (!f1c_rel32((uint8_t*)target + 5, replacement, &to_replacement) ||
        !f1c_rel32(trampoline + stolen_length + 5,
                   (uint8_t*)target + stolen_length, &to_return)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return F1C_DETOUR_RANGE_ERROR;
    }
    memcpy(trampoline, target, stolen_length);
    trampoline[stolen_length] = 0xE9;
    memcpy(trampoline + stolen_length + 1, &to_return, sizeof(to_return));

    if (!VirtualProtect(target, stolen_length, PAGE_EXECUTE_READWRITE,
                        &old_protection)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return F1C_DETOUR_PROTECT_FAILED;
    }
    memcpy(hook->original, target, stolen_length);
    memset(target, 0x90, stolen_length);
    ((uint8_t*)target)[0] = 0xE9;
    memcpy((uint8_t*)target + 1, &to_replacement, sizeof(to_replacement));
    FlushInstructionCache(GetCurrentProcess(), target, stolen_length);
    VirtualProtect(target, stolen_length, old_protection, &ignored);

    hook->target = (uint8_t*)target;
    hook->replacement = replacement;
    hook->stolen_length = stolen_length;
    hook->trampoline = trampoline;
    hook->installed = 1;
    return F1C_DETOUR_OK;
}

/* Default SDK callers retain the canonical target-metadata gate. */
F1C_HOOK_INLINE F1CDetourResult f1c_detour_install(
    F1CDetour *hook, void *target, void *replacement,
    const void *expected, size_t stolen_length)
{
    return f1c_detour_install_internal(hook, target, replacement, expected,
                                       stolen_length, 0);
}

/* Reviewed broker/SDK-adapter portability path. The caller must first prove the executable's
 * fixed image layout and independent code fingerprint. This helper then
 * validates the exact bytes at the requested hook site before writing. EXE
 * filename, timestamp, checksum, entry metadata and whole-file hash are not
 * compatibility gates. */
F1C_HOOK_INLINE F1CDetourResult f1c_detour_install_prevalidated_layout(
    F1CDetour *hook, void *target, void *replacement,
    const void *expected, size_t stolen_length)
{
    return f1c_detour_install_internal(hook, target, replacement, expected,
                                       stolen_length, 1);
}

F1C_HOOK_INLINE F1CDetourResult f1c_detour_remove(F1CDetour *hook)
{
    DWORD old_protection;
    DWORD ignored;
    int32_t expected_jump;
    size_t index;
    if (!hook || !hook->installed) return F1C_DETOUR_NOT_INSTALLED;
    if (hook->target[0] != 0xE9 ||
        !f1c_rel32(hook->target + 5, hook->replacement, &expected_jump) ||
        memcmp(hook->target + 1, &expected_jump, sizeof(expected_jump)) != 0)
        return F1C_DETOUR_BYTES_MISMATCH;
    for (index = 5; index < hook->stolen_length; ++index)
        if (hook->target[index] != 0x90) return F1C_DETOUR_BYTES_MISMATCH;
    if (!VirtualProtect(hook->target, hook->stolen_length,
                        PAGE_EXECUTE_READWRITE, &old_protection))
        return F1C_DETOUR_PROTECT_FAILED;
    memcpy(hook->target, hook->original, hook->stolen_length);
    FlushInstructionCache(GetCurrentProcess(), hook->target, hook->stolen_length);
    VirtualProtect(hook->target, hook->stolen_length, old_protection, &ignored);
    VirtualFree(hook->trampoline, 0, MEM_RELEASE);
    memset(hook, 0, sizeof(*hook));
    return F1C_DETOUR_OK;
}

F1C_HOOK_INLINE void f1c_hook_registry_init(F1CHookRegistry *registry)
{
    memset(registry, 0, sizeof(*registry));
}

F1C_HOOK_INLINE F1CDetourResult f1c_hook_registry_install(
    F1CHookRegistry *registry, F1CDetour *hook,
    void *target, void *replacement, const void *expected, size_t stolen_length)
{
    F1CDetourResult result;
    if (!registry || registry->count >= F1C_HOOK_REGISTRY_CAPACITY)
        return F1C_DETOUR_BAD_ARGUMENT;
    result = f1c_detour_install(hook, target, replacement, expected, stolen_length);
    if (result == F1C_DETOUR_OK) registry->hooks[registry->count++] = hook;
    return result;
}

/* Reverse-order cleanup. Conflicted hooks are deliberately left installed
 * rather than overwriting bytes now owned by somebody else. */
F1C_HOOK_INLINE unsigned f1c_hook_registry_remove_all(F1CHookRegistry *registry)
{
    unsigned failures = 0;
    size_t remaining = 0;
    size_t index;
    if (!registry) return 1;
    index = registry->count;
    while (index) {
        F1CDetour *hook = registry->hooks[--index];
        if (f1c_detour_remove(hook) != F1C_DETOUR_OK) {
            registry->hooks[remaining++] = hook;
            ++failures;
        }
    }
    for (index = remaining; index < registry->count; ++index)
        registry->hooks[index] = NULL;
    registry->count = remaining;
    return failures;
}

#undef F1C_HOOK_INLINE
