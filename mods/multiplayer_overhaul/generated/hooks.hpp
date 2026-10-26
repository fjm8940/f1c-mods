/* Generated guarded hook declarations. Do not edit. */
#pragma once
#include "f1c_hook.h"
#include "f1c_reviewed_api.hpp"

#if defined(_MSC_VER)
#define F1C_GEN_THISCALL __thiscall
#define F1C_GEN_STDCALL __stdcall
#elif defined(__GNUC__) && defined(__i386__)
#define F1C_GEN_THISCALL __attribute__((thiscall))
#define F1C_GEN_STDCALL __attribute__((stdcall))
#else
#define F1C_GEN_THISCALL
#define F1C_GEN_STDCALL
#endif

namespace multiplayer_ai_host { namespace hooks {

char* F1C_GEN_THISCALL process_driver_entries_hook(void* entries);
inline F1CDetour ProcessDriverEntries_detour = {};
inline const uint8_t ProcessDriverEntries_expected[] = {0x81, 0xEC, 0x8C, 0x00, 0x00, 0x00};
inline F1CDetourResult install_ProcessDriverEntries(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &ProcessDriverEntries_detour,
        reinterpret_cast<void*>(0x004B8DA0u),
        reinterpret_cast<void*>(&process_driver_entries_hook), ProcessDriverEntries_expected, sizeof(ProcessDriverEntries_expected));
}
inline f1c::reviewed::game_thread::ProcessDriverEntriesFn original_ProcessDriverEntries()
{
    return reinterpret_cast<f1c::reviewed::game_thread::ProcessDriverEntriesFn>(ProcessDriverEntries_detour.trampoline);
}

uint8_t F1C_GEN_THISCALL multiplayer_qualification_grid_hook(void* session_manager);
inline F1CDetour MultiplayerQualificationGridAssignment_detour = {};
inline const uint8_t MultiplayerQualificationGridAssignment_expected[] = {0x83, 0xEC, 0x10, 0x55, 0x56};
inline F1CDetourResult install_MultiplayerQualificationGridAssignment(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &MultiplayerQualificationGridAssignment_detour,
        reinterpret_cast<void*>(0x00462E20u),
        reinterpret_cast<void*>(&multiplayer_qualification_grid_hook), MultiplayerQualificationGridAssignment_expected, sizeof(MultiplayerQualificationGridAssignment_expected));
}
inline f1c::reviewed::game_thread::MultiplayerQualificationGridAssignmentFn original_MultiplayerQualificationGridAssignment()
{
    return reinterpret_cast<f1c::reviewed::game_thread::MultiplayerQualificationGridAssignmentFn>(MultiplayerQualificationGridAssignment_detour.trampoline);
}

uint8_t F1C_GEN_THISCALL netcomm_slot_sync_hook(void* netcomm);
inline F1CDetour NetCommPlayerSlotSync_detour = {};
inline const uint8_t NetCommPlayerSlotSync_expected[] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8};
inline F1CDetourResult install_NetCommPlayerSlotSync(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &NetCommPlayerSlotSync_detour,
        reinterpret_cast<void*>(0x0045FC50u),
        reinterpret_cast<void*>(&netcomm_slot_sync_hook), NetCommPlayerSlotSync_expected, sizeof(NetCommPlayerSlotSync_expected));
}
inline f1c::reviewed::game_thread::NetCommPlayerSlotSyncFn original_NetCommPlayerSlotSync()
{
    return reinterpret_cast<f1c::reviewed::game_thread::NetCommPlayerSlotSyncFn>(NetCommPlayerSlotSync_detour.trampoline);
}

void F1C_GEN_THISCALL scene_render_update_hook(::F1CVehicle* vehicle, uint8_t full_setup);
inline F1CDetour SceneRenderUpdate_detour = {};
inline const uint8_t SceneRenderUpdate_expected[] = {0x83, 0xEC, 0x18, 0x53, 0x8B, 0x5C, 0x24, 0x20};
inline F1CDetourResult install_SceneRenderUpdate(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &SceneRenderUpdate_detour,
        reinterpret_cast<void*>(0x004C6D90u),
        reinterpret_cast<void*>(&scene_render_update_hook), SceneRenderUpdate_expected, sizeof(SceneRenderUpdate_expected));
}
inline f1c::reviewed::game_thread::SceneRenderUpdateFn original_SceneRenderUpdate()
{
    return reinterpret_cast<f1c::reviewed::game_thread::SceneRenderUpdateFn>(SceneRenderUpdate_detour.trampoline);
}

uint8_t F1C_GEN_THISCALL suspension_force_hook(void* vehicle_dynamics);
inline F1CDetour SuspensionForceComputationWithPerCornerVirtualDevices_detour = {};
inline const uint8_t SuspensionForceComputationWithPerCornerVirtualDevices_expected[] = {0x83, 0xEC, 0x24, 0x56, 0x8B, 0xF1};
inline F1CDetourResult install_SuspensionForceComputationWithPerCornerVirtualDevices(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &SuspensionForceComputationWithPerCornerVirtualDevices_detour,
        reinterpret_cast<void*>(0x004F8440u),
        reinterpret_cast<void*>(&suspension_force_hook), SuspensionForceComputationWithPerCornerVirtualDevices_expected, sizeof(SuspensionForceComputationWithPerCornerVirtualDevices_expected));
}
inline f1c::reviewed::game_thread::SuspensionForceComputationWithPerCornerVirtualDevicesFn original_SuspensionForceComputationWithPerCornerVirtualDevices()
{
    return reinterpret_cast<f1c::reviewed::game_thread::SuspensionForceComputationWithPerCornerVirtualDevicesFn>(SuspensionForceComputationWithPerCornerVirtualDevices_detour.trampoline);
}

int32_t F1C_GEN_STDCALL driver_name_lookup_hook(int32_t* entry_field, int32_t record_index, uint32_t context, int8_t variant_a, int8_t variant_b);
inline F1CDetour DriverNameIDLookup_detour = {};
inline const uint8_t DriverNameIDLookup_expected[] = {0x53, 0x8B, 0x5C, 0x24, 0x10};
inline F1CDetourResult install_DriverNameIDLookup(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &DriverNameIDLookup_detour,
        reinterpret_cast<void*>(0x00415D90u),
        reinterpret_cast<void*>(&driver_name_lookup_hook), DriverNameIDLookup_expected, sizeof(DriverNameIDLookup_expected));
}
inline f1c::reviewed::game_thread::DriverNameIDLookupFn original_DriverNameIDLookup()
{
    return reinterpret_cast<f1c::reviewed::game_thread::DriverNameIDLookupFn>(DriverNameIDLookup_detour.trampoline);
}

int32_t F1C_GEN_THISCALL commentary_entry_hook(void* entry);
inline F1CDetour CommentaryDriverEntryInit_detour = {};
inline const uint8_t CommentaryDriverEntryInit_expected[] = {0x81, 0xEC, 0x00, 0x01, 0x00, 0x00};
inline F1CDetourResult install_CommentaryDriverEntryInit(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &CommentaryDriverEntryInit_detour,
        reinterpret_cast<void*>(0x00414600u),
        reinterpret_cast<void*>(&commentary_entry_hook), CommentaryDriverEntryInit_expected, sizeof(CommentaryDriverEntryInit_expected));
}
inline f1c::reviewed::game_thread::CommentaryDriverEntryInitFn original_CommentaryDriverEntryInit()
{
    return reinterpret_cast<f1c::reviewed::game_thread::CommentaryDriverEntryInitFn>(CommentaryDriverEntryInit_detour.trampoline);
}

void F1C_GEN_THISCALL commentary_event_hook(void* entry, int32_t event_id, int32_t driver1, int32_t driver2, int32_t arg5, int32_t arg6, float arg7, float arg8);
inline F1CDetour CommentaryEventQueue_detour = {};
inline const uint8_t CommentaryEventQueue_expected[] = {0x8B, 0x44, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x0C};
inline F1CDetourResult install_CommentaryEventQueue(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &CommentaryEventQueue_detour,
        reinterpret_cast<void*>(0x004144F0u),
        reinterpret_cast<void*>(&commentary_event_hook), CommentaryEventQueue_expected, sizeof(CommentaryEventQueue_expected));
}
inline f1c::reviewed::game_thread::CommentaryEventQueueFn original_CommentaryEventQueue()
{
    return reinterpret_cast<f1c::reviewed::game_thread::CommentaryEventQueueFn>(CommentaryEventQueue_detour.trampoline);
}

int32_t F1C_GEN_THISCALL multiplayer_vehicle_placement_hook(void* session, int32_t session_type);
inline F1CDetour MultiplayerVehiclePlacement_detour = {};
inline const uint8_t MultiplayerVehiclePlacement_expected[] = {0x53, 0x55, 0x56, 0x8B, 0xE9};
inline F1CDetourResult install_MultiplayerVehiclePlacement(F1CHookRegistry *registry)
{
    return f1c_hook_registry_install(registry, &MultiplayerVehiclePlacement_detour,
        reinterpret_cast<void*>(0x0042CF10u),
        reinterpret_cast<void*>(&multiplayer_vehicle_placement_hook), MultiplayerVehiclePlacement_expected, sizeof(MultiplayerVehiclePlacement_expected));
}
inline f1c::reviewed::game_thread::MultiplayerVehiclePlacementFn original_MultiplayerVehiclePlacement()
{
    return reinterpret_cast<f1c::reviewed::game_thread::MultiplayerVehiclePlacementFn>(MultiplayerVehiclePlacement_detour.trampoline);
}

}} // namespace

#undef F1C_GEN_THISCALL
#undef F1C_GEN_STDCALL
