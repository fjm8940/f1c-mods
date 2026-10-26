#pragma once
#include "f1c_broker_penalty_hold.h"
typedef struct {
    uintptr_t vehicle;
    int32_t driver;
    uint32_t slot;
    int fault;
    F1CRestartPenalties queue;
} BrokerPenaltyEntry;
static BrokerPenaltyEntry g_penalty_entries[VEH_SLOTS];
static uint32_t g_penalty_count,g_penalty_generation;
static uint32_t g_penalty_live_generation;
static int g_penalty_restart_sealed,g_penalty_awaiting_restart;
static F1CBrokerToken g_penalty_owner;
static volatile LONG g_active_penalty_calls;
static int g_penalty_supported;
static F1CDetour g_penalty_issue_detour,g_penalty_service_detour,
    g_penalty_consume_detour,g_penalty_lap_detour;
typedef int (__attribute__((thiscall)) *BrokerPenaltyIssueFn)(void*,const void*);
typedef int (__attribute__((thiscall)) *BrokerPenaltyServiceFn)(void*);
typedef void (__attribute__((thiscall)) *BrokerPenaltyLapFn)(void*,float,float,int,int);
static BrokerPenaltyIssueFn g_penalty_issue_original;
static BrokerPenaltyServiceFn g_penalty_service_original,g_penalty_consume_original;
static BrokerPenaltyLapFn g_penalty_lap_original;
#define BROKER_PENALTY_SCORE_OFFSET (F1C_OFF_lapNumber-8u)
