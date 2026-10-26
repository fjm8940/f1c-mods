#include "protected_plugin_abi.h"
#include <cstdio>
#include <cassert>
using Init=BOOL (__cdecl *)(const LightLoaderProtectedHostV2*);
int main(int argc,char**) {
    if(argc>1) {
        assert(LoadLibraryW(L"dinput8.dll")); Sleep(2000);
        std::puts("Official loader scan complete; inspect loader and Pit logs."); return 0;
    }
    HMODULE plugin=LoadLibraryW(L"pitstop_failures.dll");assert(plugin);
    assert(LoadLibraryW(L"pitstop_failures.dll")); // Keep test export callable after worker exits.
    auto init=reinterpret_cast<Init>(reinterpret_cast<uintptr_t>(GetProcAddress(plugin,LLP_ENTRY_NAME)));assert(init);
    Sleep(100);
    assert(GetFileAttributesW(L"pitstop_failures.log")==INVALID_FILE_ATTRIBUTES);
    assert(!init(nullptr));assert(!init((LightLoaderProtectedHostV2*)1));
    LightLoaderProtectedHostV2 host{};host.size=sizeof host;
    assert(!init(&host));
    HMODULE loader=LoadLibraryW(L"dinput8.dll");assert(loader);
    Sleep(100);
    assert(GetFileAttributesW(L"pitstop_failures.log")==INVALID_FILE_ATTRIBUTES);
    auto proof=reinterpret_cast<LightLoaderProofProc>(reinterpret_cast<uintptr_t>(GetProcAddress(loader,LLP_PROOF_NAME)));assert(proof);
    host.magic=LLP_ABI_MAGIC;host.abi_version=LLP_ABI_VERSION;
    host.process_id=GetCurrentProcessId();host.loader_module=loader;
    host.nonce=1234567;host.product_id=LLP_PRODUCT_ID;
    host.capabilities=LLP_CAP_EXCLUSIVE_HOST|LLP_CAP_HUB_2;
    host.compatibility_generation=LLP_HUB_2_GENERATION;
    auto seal=[&](){host.seal=LightLoaderHostSeal(&host);host.proof=proof(LightLoaderProofChallenge(&host));};
    seal();auto valid=host;
    host.size=4;assert(!init(&host));host=valid;
    host.magic^=1;seal();assert(!init(&host));host=valid;
    host.process_id++;seal();assert(!init(&host));host=valid;
    host.loader_module=plugin;seal();assert(!init(&host));host=valid;
    host.product_id^=1;seal();assert(!init(&host));host=valid;
    host.abi_version=0x10000;seal();assert(!init(&host));host=valid;
    host.capabilities=LLP_CAP_EXCLUSIVE_HOST;seal();assert(!init(&host));host=valid;
    host.compatibility_generation=LLP_EXCLUSIVE_GENERATION;seal();assert(!init(&host));host=valid;
    host.seal^=1;assert(!init(&host));host=valid;
    host.proof^=1;assert(!init(&host));host=valid;
    assert(init(&host));assert(!init(&host));Sleep(250);
    std::puts("PASS hub-2 accepted; old capability/generation, direct load, null/unreadable/truncated/forged hosts and duplicate init refused.");
    return 0;
}
