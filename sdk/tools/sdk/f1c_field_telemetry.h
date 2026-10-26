#pragma once

/* Shared, read-only whole-field telemetry contract for Red Flag, Safety Car,
   field viewers, and future SDK/LightLoader scripts. The provider owns all
   broker interaction; consumers only copy coherent session snapshots. */
#include <windows.h>
#include <stdint.h>
#include <string.h>

#define F1C_FIELD_TELEMETRY_MAPPING "Local\\F1C_FIELD_TELEMETRY_V2"
#define F1C_FIELD_TELEMETRY_MAGIC 0x32544646u
#define F1C_FIELD_TELEMETRY_ABI 2u

typedef struct F1CFieldTelemetryHeader {
    uint32_t magic, abi_version, header_size, row_size, mapping_size;
    volatile int32_t sequence;
    uint32_t session_generation, session_active;
    uint64_t snapshot_sequence;
    uint32_t row_count, max_rows;
    int32_t session_type, race_phase, race_active, total_race_laps;
    float race_time_threshold, track_condition_state;
    float online_wetness, offline_wetness, ambient_temp_c, track_temp_c;
    int32_t spectated_vehicle_slot;
    uint32_t reserved[8];
} F1CFieldTelemetryHeader;

typedef struct F1CFieldTelemetryRow {
    uint32_t session_generation, object_generation, slot, reserved0;
    uint64_t valid_fields;
    int32_t controller_role, driver_id, retired, finish_state, reason_code;
    int32_t race_position, qualifying_position, lap_number, pit_status;
    int32_t call_to_box, ai_control, off_track, driver_model_visible;
    int32_t front_wing_record_present, front_wing_intact;
    int32_t rear_wing_record_present, rear_wing_intact, car_conditions;
    uint32_t team_id;
    float world_x, world_y, world_z, speed, best_lap_time, elapsed_time;
    float engine_life_remaining, brakes_life_remaining;
    float gearbox_life_remaining, suspension_life_remaining;
    float tire_puncture_rr, tire_puncture_rl, tire_puncture_fr, tire_puncture_fl;
    char driver_name[64];
    char car_name[156];
    float orientation_up_dot, inverted_seconds;
    int32_t orientation_state, suspected_incapacitated;
    int32_t sc_inversion_reason,rf_inversion_reason;
} F1CFieldTelemetryRow;

typedef struct F1CFieldTelemetryClient {
    HANDLE mapping;
    const F1CFieldTelemetryHeader *view;
} F1CFieldTelemetryClient;

static __inline int f1c_field_telemetry_open(F1CFieldTelemetryClient *client)
{
    if (!client) return 0;
    memset(client, 0, sizeof(*client));
    client->mapping = OpenFileMappingA(FILE_MAP_READ, FALSE,
                                       F1C_FIELD_TELEMETRY_MAPPING);
    if (!client->mapping) return 0;
    client->view = (const F1CFieldTelemetryHeader *)MapViewOfFile(
        client->mapping, FILE_MAP_READ, 0, 0, 0);
    if (!client->view || client->view->magic != F1C_FIELD_TELEMETRY_MAGIC ||
        client->view->abi_version != F1C_FIELD_TELEMETRY_ABI ||
        client->view->header_size != sizeof(F1CFieldTelemetryHeader) ||
        client->view->row_size != sizeof(F1CFieldTelemetryRow)) {
        if (client->view) UnmapViewOfFile(client->view);
        CloseHandle(client->mapping);
        memset(client, 0, sizeof(*client));
        return 0;
    }
    return 1;
}

static __inline void f1c_field_telemetry_close(F1CFieldTelemetryClient *client)
{
    if (!client) return;
    if (client->view) UnmapViewOfFile(client->view);
    if (client->mapping) CloseHandle(client->mapping);
    memset(client, 0, sizeof(*client));
}

/* Returns 1 only for a coherent active-session snapshot. Any absence,
   boundary, size mismatch, or writer collision returns 0; consumers must
   take no gameplay action on 0. */
static __inline int f1c_field_telemetry_read(
    const F1CFieldTelemetryClient *client, F1CFieldTelemetryHeader *header_out,
    F1CFieldTelemetryRow *rows_out, uint32_t rows_capacity)
{
    const F1CFieldTelemetryHeader *source;
    const F1CFieldTelemetryRow *source_rows;
    LONG before, after;
    uint32_t count;
    if (!client || !client->view || !header_out) return 0;
    source = client->view;
    before = InterlockedCompareExchange((volatile LONG *)&source->sequence, 0, 0);
    if (before & 1) return 0;
    MemoryBarrier();
    if (source->magic != F1C_FIELD_TELEMETRY_MAGIC ||
        source->abi_version != F1C_FIELD_TELEMETRY_ABI ||
        source->header_size != sizeof(*source) ||
        source->row_size != sizeof(F1CFieldTelemetryRow) ||
        source->mapping_size < sizeof(*source) ||
        source->max_rows > (source->mapping_size - sizeof(*source)) /
            sizeof(F1CFieldTelemetryRow)) return 0;
    count = source->row_count;
    if (!source->session_active || count > source->max_rows ||
        count > rows_capacity || (count && !rows_out)) return 0;
    memcpy(header_out, source, sizeof(*header_out));
    source_rows = (const F1CFieldTelemetryRow *)
        ((const unsigned char *)source + sizeof(*source));
    if (count) memcpy(rows_out, source_rows, count * sizeof(*rows_out));
    MemoryBarrier();
    after = InterlockedCompareExchange((volatile LONG *)&source->sequence, 0, 0);
    return before == after && !(after & 1) &&
        header_out->session_generation == source->session_generation;
}
