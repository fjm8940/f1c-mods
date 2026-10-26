#pragma once
#include "protected_plugin_abi.h"

// Published Lightloader exclusive-host contract, as used by ESC Pause Menu.
// Copy through ReadProcessMemory before dereferencing an untrusted host record.
static bool pit_validate_loader_host(const LightLoaderProtectedHostV2* input) {
    LightLoaderProtectedHostV2 host{};
    SIZE_T copied=0;
    uint32_t size=0;
    if(!input || !ReadProcessMemory(GetCurrentProcess(),input,&size,sizeof size,&copied) ||
       copied!=sizeof size || size<sizeof host) return false;
    if(!ReadProcessMemory(GetCurrentProcess(),input,&host,sizeof host,&copied) || copied!=sizeof host)
        return false;
    if(host.magic!=LLP_ABI_MAGIC || host.abi_version<LLP_ABI_VERSION ||
       host.process_id!=GetCurrentProcessId() || !host.loader_module ||
       host.loader_module!=GetModuleHandleW(L"dinput8.dll") ||
       host.product_id!=LLP_PRODUCT_ID ||
       (host.capabilities&(LLP_CAP_EXCLUSIVE_HOST|LLP_CAP_HUB_2))!=
           (LLP_CAP_EXCLUSIVE_HOST|LLP_CAP_HUB_2) ||
       host.compatibility_generation<LLP_HUB_2_GENERATION ||
       host.seal!=LightLoaderHostSeal(&host)) return false;
    auto proof=reinterpret_cast<LightLoaderProofProc>(reinterpret_cast<uintptr_t>(
        GetProcAddress(host.loader_module,LLP_PROOF_NAME)));
    return proof && proof(LightLoaderProofChallenge(&host))==host.proof;
}
