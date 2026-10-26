#pragma once
#include "f1c_broker.h"
/* Optional ABI15 capability. Runs synchronously after RestartGate returns 6,
 * before Game_Post/vehicle destruction, on its caller thread. No veto.
 * RESULTS_FINALIZE additionally invokes it BEFORE sub_46BF10 simulates the
 * remaining session on ESC; this is earlier than the final results snapshot.
 * Consumers must be idempotent: accelerated finish can precede full leave.
 * Callback lifetime is reserved under the broker lock, invocation outside it.
 * Not a pause, mini-restart, process-kill, or unloaded-field callback. */
typedef F1CBrokerResult (__cdecl *F1CBrokerSetSessionEndFn)(F1CBrokerToken,F1CBrokerTaskFn,void*);
static inline F1CBrokerSetSessionEndFn f1c_broker_session_end_bind(const F1CBrokerApi *api){
 if(!api || !(api->capabilities & F1C_BROKER_CAP_SESSION_END))return NULL;
 return (F1CBrokerSetSessionEndFn)(uintptr_t)GetProcAddress(api->module,"F1CBrokerSetSessionEnd");
}
