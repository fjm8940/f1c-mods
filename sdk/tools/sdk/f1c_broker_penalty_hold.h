#pragma once
#include "f1c_broker_abi.h"
#include "f1c_restart_penalties.h"
/* Optional ABI15 capability, exact native function bodies validated by broker.
 * Begin runs only on its game thread and atomically captures all named racers.
 * Read runs on the game thread, checks identity/token/generation and copies
 * the event ledger; End disables the hold only
 * when no native penalty operation is in flight. No client installs a hook.
 * During hold: issue events accumulate, service cannot discharge, and native
 * lap processing cannot expire the captured remaining-lap allowances.
 */
#define F1C_BROKER_CAP_PENALTY_HOLD (1ull << 13)
typedef F1CBrokerResult (__cdecl *F1CBrokerPenaltyBeginFn)(F1CBrokerToken,
    const uintptr_t*,uint32_t,F1CRestartPenalties*,uint32_t*);
typedef F1CBrokerResult (__cdecl *F1CBrokerPenaltyReadFn)(F1CBrokerToken,
    uintptr_t,uint32_t,F1CRestartPenalties*);
typedef F1CBrokerResult (__cdecl *F1CBrokerPenaltyEndFn)(F1CBrokerToken);
/* Game-thread seal immediately before the owned constructor: authorizes one
 * native restart boundary. The ledger survives, never a copied native object;
 * each later event/read revalidates fixed slot, driver and scoring self-pointer.
 * Exit/other session transitions discard the ledger. */
typedef F1CBrokerResult (__cdecl *F1CBrokerPenaltySealFn)(F1CBrokerToken);
/* Mirrors native four-entry append, including full-queue accumulation. */
static inline int f1c_penalty_hold_issue(F1CRestartPenalties *s,int seconds) {
    if(!f1c_restart_penalties_valid(s) || seconds<0 || seconds>127)return 0;
    if(s->count==4) {
        if(s->pair[3][1]>3600-seconds)return 0;
        s->pair[3][1]+=seconds;
    } else {
        s->pair[s->count][0]=3;s->pair[s->count][1]=seconds;s->count++;
    }
    return 1;
}
