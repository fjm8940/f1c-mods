/* Reviewed layout of the pit controller embedded at F1CVehicle+0x32CC. */
#pragma once
#include <stdint.h>

struct F1CPitMachine {
    void *vehicle;                     /* 0x000 owning F1CVehicle* */
    int32_t pit_team_index;            /* 0x004 */
    uint8_t gap_008[0x178];
    uint8_t pit_entry_latch;           /* 0x180 / vehicle+0x344C */
    uint8_t gap_181[3];
    int32_t status;                    /* 0x184 / vehicle+0x3450 */
    float state_event_time;            /* 0x188, copied from transition event */
    float state_enter_time;            /* 0x18C, latched on entering 1/2 */
    uint8_t service_ready;             /* 0x190 / vehicle+0x345C */
    uint8_t gap_191[0xF];
    float stop_start_time;             /* 0x1A0 / vehicle+0x346C */
    float stop_end_time;               /* 0x1A4 / vehicle+0x3470 */
    uint32_t operation_flags;          /* 0x1A8 */
    uint8_t operation_sound_latch;     /* 0x1AC */
    uint8_t gap_1AD[3];
    float fuel_start;                  /* 0x1B0 */
    float fuel_start_time;             /* 0x1B4 */
    float fuel_target;                 /* 0x1B8 */
    float refuel_progress;              /* 0x1BC */
    uint32_t refuel_active;             /* 0x1C0 */
    uint32_t tire_change_active;        /* 0x1C4 */
    uint32_t wing_adjustment_active;    /* 0x1C8 */
    uint32_t repair_active;             /* 0x1CC */
    float tire_service_start_time;     /* 0x1D0 */
    float tire_service_end_time;       /* 0x1D4 */
};
