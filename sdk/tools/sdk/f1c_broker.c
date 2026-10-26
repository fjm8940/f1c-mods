#include "f1c_broker.h"

#include <tlhelp32.h>
#include <string.h>

static FARPROC required_export(HMODULE module, const char *name)
{
    return GetProcAddress(module, name);
}

static uint64_t capabilities_for_abi(uint64_t advertised, uint32_t abi)
{
    if (abi < 6u)
        advertised &= ~(F1C_BROKER_CAP_VEHICLE_SNAPSHOTS |
                        F1C_BROKER_CAP_POLL_EVENTS);
    if (abi < 7u)
        advertised &= ~F1C_BROKER_CAP_SESSION_SNAPSHOT;
    if (abi < 8u)
        advertised &= ~F1C_BROKER_CAP_PACE_CONTROL;
    if (abi < 10u)
        advertised &= ~F1C_BROKER_CAP_PAUSE_MENU;
    if (abi < 12u)
        advertised &= ~F1C_BROKER_CAP_HIGH_GEAR_RATIO;
    if (abi < 13u)
        advertised &= ~F1C_BROKER_CAP_RESTART_GUARD;
    if (abi < 14u)
        advertised &= ~F1C_BROKER_CAP_PRELOAD;
    if (abi < 15u)
        advertised &= ~F1C_BROKER_CAP_GARAGE_RESULTS;
    return advertised;
}

F1CBrokerDiscoveryResult f1c_broker_discover(
    uint32_t client_min_abi, uint32_t client_max_abi,
    uint64_t required_capabilities, F1CBrokerApi *api_out)
{
    HANDLE snapshot;
    MODULEENTRY32 module_entry;
    HMODULE candidate = NULL;
    F1CBrokerGetInfoFn get_info = NULL;
    F1CBrokerInfo candidate_info;
    F1CBrokerApi selected_api;
    uint32_t api_capacity;
    uint32_t candidate_count = 0;
    F1CBrokerDiscoveryResult rejected = F1C_BROKER_DISCOVERY_NOT_FOUND;

    if (!api_out || api_out->structure_size < F1C_BROKER_API_ABI6_SIZE ||
        !client_min_abi || client_min_abi > client_max_abi)
        return F1C_BROKER_DISCOVERY_BAD_ARGUMENT;
    api_capacity = api_out->structure_size < sizeof(*api_out)
        ? api_out->structure_size : (uint32_t)sizeof(*api_out);
    memset((unsigned char *)api_out + sizeof(api_out->structure_size), 0,
           api_capacity - sizeof(api_out->structure_size));
    if (api_capacity < sizeof(*api_out)) {
        if (api_capacity < F1C_BROKER_API_ABI7_SIZE) {
            if (client_max_abi > 6u) client_max_abi = 6u;
        } else if (api_capacity < F1C_BROKER_API_ABI9_SIZE) {
            if (client_max_abi > 7u) client_max_abi = 7u;
        } else if (api_capacity < F1C_BROKER_API_ABI11_SIZE) {
            if (client_max_abi > 9u) client_max_abi = 9u;
        } else if (api_capacity < F1C_BROKER_API_ABI12_SIZE) {
            if (client_max_abi > 11u) client_max_abi = 11u;
        } else if (api_capacity < F1C_BROKER_API_ABI13_SIZE) {
            if (client_max_abi > 12u) client_max_abi = 12u;
        } else if (client_max_abi > 13u) {
            client_max_abi = 13u;
        }
    }

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE)
        return F1C_BROKER_DISCOVERY_QUERY_FAILED;

    memset(&module_entry, 0, sizeof(module_entry));
    module_entry.dwSize = sizeof(module_entry);
    if (Module32First(snapshot, &module_entry)) {
        do {
            F1CBrokerGetInfoFn probe = (F1CBrokerGetInfoFn)(uintptr_t)
                required_export(module_entry.hModule, "F1CBrokerGetInfo");
            HMODULE probe_module = NULL;
            F1CBrokerInfo info;
            uint32_t selected;
            uint64_t negotiated_capabilities;
            if (!probe) continue;
            ++candidate_count;
            /* A Toolhelp snapshot does not pin modules. Hold a reference before
               invoking broker code so concurrent unload cannot stale probe. */
            if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                    (LPCSTR)(uintptr_t)probe, &probe_module)) {
                rejected = F1C_BROKER_DISCOVERY_QUERY_FAILED;
                continue;
            }
            memset(&info, 0, sizeof(info));
            info.structure_size = sizeof(info);
            if (probe(&info) != F1C_BROKER_OK ||
                info.structure_size < F1C_BROKER_INFO_ABI6_SIZE ||
                !info.abi_version ||
                !info.min_compatible_abi ||
                info.min_compatible_abi > info.abi_version) {
                rejected = F1C_BROKER_DISCOVERY_QUERY_FAILED;
                FreeLibrary(probe_module);
                continue;
            }
            selected = client_max_abi < info.abi_version
                ? client_max_abi : info.abi_version;
            if (selected < client_min_abi || selected < info.min_compatible_abi) {
                rejected = F1C_BROKER_DISCOVERY_INCOMPATIBLE_ABI;
                FreeLibrary(probe_module);
                continue;
            }
            negotiated_capabilities = capabilities_for_abi(info.capabilities,
                                                           selected);
            if ((negotiated_capabilities & required_capabilities) !=
                required_capabilities) {
                rejected = F1C_BROKER_DISCOVERY_MISSING_CAPABILITY;
                FreeLibrary(probe_module);
                continue;
            }
            if (!candidate) {
                candidate = probe_module;
                get_info = probe;
                candidate_info = info;
                candidate_info.capabilities = negotiated_capabilities;
            } else {
                FreeLibrary(probe_module);
            }
        } while (Module32Next(snapshot, &module_entry));
    }
    CloseHandle(snapshot);

    /* More than one broker-shaped module is always ambiguous, even when only
       one happens to satisfy this client's requested version or capabilities. */
    if (candidate_count > 1) {
        if (candidate) FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_DUPLICATE;
    }
    if (!candidate) return rejected;

    memset(&selected_api, 0, sizeof(selected_api));
    selected_api.structure_size = api_capacity;
#define BIND_REQUIRED(field, symbol, type) do { \
    selected_api.field = (type)(uintptr_t)required_export(candidate, symbol); \
    if (!selected_api.field) { \
        FreeLibrary(candidate); \
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS; \
    } \
} while (0)
    BIND_REQUIRED(register_client, "F1CBrokerRegister", F1CBrokerRegisterFn);
    BIND_REQUIRED(unregister_client, "F1CBrokerUnregister", F1CBrokerUnregisterFn);
    BIND_REQUIRED(post, "F1CBrokerPost", F1CBrokerPostFn);
    BIND_REQUIRED(get_stats, "F1CBrokerGetStats", F1CBrokerGetStatsFn);
    selected_api.get_info = get_info;
    selected_api.set_call_to_box = (F1CBrokerSetCallToBoxFn)(uintptr_t)
        required_export(candidate, "F1CBrokerSetCallToBox");
    selected_api.pulse_call_to_box = (F1CBrokerPulseCallToBoxFn)(uintptr_t)
        required_export(candidate, "F1CBrokerPulseCallToBox");
    selected_api.request_mini_restart = (F1CBrokerRequestMiniRestartFn)(uintptr_t)
        required_export(candidate, "F1CBrokerRequestMiniRestart");
    if (candidate_info.capabilities & F1C_BROKER_CAP_VEHICLE_SNAPSHOTS) {
        selected_api.get_vehicle_handles = (F1CBrokerGetVehicleHandlesFn)(uintptr_t)
            required_export(candidate, "F1CBrokerGetVehicleHandles");
        selected_api.get_vehicle_snapshot = (F1CBrokerGetVehicleSnapshotFn)(uintptr_t)
            required_export(candidate, "F1CBrokerGetVehicleSnapshot");
    }
    if (candidate_info.capabilities & F1C_BROKER_CAP_SESSION_SNAPSHOT)
        selected_api.get_session_snapshot = (F1CBrokerGetSessionSnapshotFn)(uintptr_t)
            required_export(candidate, "F1CBrokerGetSessionSnapshot");
    if (candidate_info.capabilities & F1C_BROKER_CAP_POLL_EVENTS)
        selected_api.poll_events = (F1CBrokerPollEventsFn)(uintptr_t)
            required_export(candidate, "F1CBrokerPollEvents");
    if (candidate_info.capabilities & F1C_BROKER_CAP_PACE_CONTROL)
        selected_api.set_pace_cap = (F1CBrokerSetPaceCapFn)(uintptr_t)
            required_export(candidate, "F1CBrokerSetPaceCap");
    if (candidate_info.capabilities & F1C_BROKER_CAP_PAUSE_MENU) {
        selected_api.pause_menu_arm = (F1CBrokerPauseMenuArmFn)(uintptr_t)
            required_export(candidate, "F1CBrokerPauseMenuArm");
        selected_api.pause_menu_command = (F1CBrokerPauseMenuCommandFn)(uintptr_t)
            required_export(candidate, "F1CBrokerPauseMenuCommand");
    }
    if (candidate_info.capabilities & F1C_BROKER_CAP_HIGH_GEAR_RATIO)
        selected_api.set_high_gear_ratio = (F1CBrokerSetHighGearRatioFn)(uintptr_t)
            required_export(candidate, "F1CBrokerSetHighGearRatio");
    if (candidate_info.capabilities & F1C_BROKER_CAP_RESTART_GUARD)
        selected_api.set_restart_guard = (F1CBrokerSetRestartGuardFn)(uintptr_t)
            required_export(candidate, "F1CBrokerSetRestartGuard");
    if (candidate_info.capabilities & F1C_BROKER_CAP_PRELOAD)
        selected_api.set_preload = (F1CBrokerSetPreloadFn)(uintptr_t)
            required_export(candidate, "F1CBrokerSetPreload");
#undef BIND_REQUIRED

    if ((candidate_info.capabilities & F1C_BROKER_CAP_VEHICLE_SNAPSHOTS) &&
        (!selected_api.get_vehicle_handles || !selected_api.get_vehicle_snapshot)) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }
    if ((candidate_info.capabilities & F1C_BROKER_CAP_POLL_EVENTS) &&
        !selected_api.poll_events) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }
    if ((candidate_info.capabilities & F1C_BROKER_CAP_SESSION_SNAPSHOT) &&
        !selected_api.get_session_snapshot) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }
    if ((candidate_info.capabilities & F1C_BROKER_CAP_PACE_CONTROL) &&
        !selected_api.set_pace_cap) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }
    if ((candidate_info.capabilities & F1C_BROKER_CAP_PAUSE_MENU) &&
        (!selected_api.pause_menu_arm || !selected_api.pause_menu_command)) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }
    if ((candidate_info.capabilities & F1C_BROKER_CAP_HIGH_GEAR_RATIO) &&
        !selected_api.set_high_gear_ratio) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }
    if ((candidate_info.capabilities & F1C_BROKER_CAP_RESTART_GUARD) &&
        !selected_api.set_restart_guard) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }

    if ((candidate_info.capabilities & F1C_BROKER_CAP_PRELOAD) &&
        !selected_api.set_preload) {
        FreeLibrary(candidate);
        return F1C_BROKER_DISCOVERY_INCOMPLETE_EXPORTS;
    }
    selected_api.module = candidate;
    selected_api.negotiated_abi = client_max_abi < candidate_info.abi_version
        ? client_max_abi : candidate_info.abi_version;
    selected_api.capabilities = candidate_info.capabilities;
    memcpy(api_out, &selected_api, api_capacity);
    return F1C_BROKER_DISCOVERY_OK;
}

void f1c_broker_release(F1CBrokerApi *api)
{
    HMODULE module;
    uint32_t api_capacity;
    if (!api) return;
    api_capacity = api->structure_size;
    if (api_capacity < F1C_BROKER_API_ABI6_SIZE) return;
    if (api_capacity > sizeof(*api)) api_capacity = sizeof(*api);
    module = api->module;
    memset(api, 0, api_capacity);
    if (module) FreeLibrary(module);
}
