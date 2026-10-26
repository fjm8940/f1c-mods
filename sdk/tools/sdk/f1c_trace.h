/* Allocation-free snapshots and typed field-change callbacks. */
#pragma once
#include <stdint.h>
#include <string.h>
#include "f1c_trace_schema.h"

typedef struct F1CTraceSnapshot {
    uint64_t raw[F1C_TRACE_FIELD_COUNT];
} F1CTraceSnapshot;

typedef void (*F1CTraceChangeFn)(const F1CTraceFieldDesc *field,
                                 uint64_t before, uint64_t after,
                                 void *context);

#if defined(_MSC_VER)
#define F1C_TRACE_INLINE static __inline
#else
#define F1C_TRACE_INLINE static inline
#endif

F1C_TRACE_INLINE uint64_t f1c_trace_read_raw(
    const void *object, const F1CTraceFieldDesc *field)
{
    uint64_t value = 0;
    memcpy(&value, (const uint8_t*)object + field->offset, field->size);
    return value;
}

F1C_TRACE_INLINE void f1c_trace_snapshot_vehicle(
    const void *vehicle, F1CTraceSnapshot *snapshot)
{
    unsigned index;
    for (index = 0; index < F1C_TRACE_FIELD_COUNT; ++index)
        snapshot->raw[index] = f1c_trace_read_raw(
            vehicle, &F1C_TRACE_VEHICLE_FIELDS[index]);
}

F1C_TRACE_INLINE unsigned f1c_trace_diff_vehicle(
    const F1CTraceSnapshot *before, const F1CTraceSnapshot *after,
    F1CTraceChangeFn callback, void *context)
{
    unsigned index;
    unsigned changes = 0;
    for (index = 0; index < F1C_TRACE_FIELD_COUNT; ++index) {
        if (before->raw[index] == after->raw[index]) continue;
        ++changes;
        if (callback) callback(&F1C_TRACE_VEHICLE_FIELDS[index],
                               before->raw[index], after->raw[index], context);
    }
    return changes;
}

F1C_TRACE_INLINE float f1c_trace_raw_float(uint64_t raw)
{
    uint32_t bits = (uint32_t)raw;
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

#undef F1C_TRACE_INLINE

