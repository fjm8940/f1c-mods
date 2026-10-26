#pragma once
#include "f1c_broker.h"
typedef F1CBrokerResult (__cdecl *F1CBrokerSetRecoveryFn)(F1CBrokerToken, F1CBrokerTaskFn, void *);
typedef F1CBrokerResult (__cdecl *F1CBrokerNativeBudgetFn)(F1CBrokerToken);
/* ABI14 optional capability extension. Recovery runs on the broker game thread,
   outside its lock, before retirement; callback code remains in flight. Recovery
   must only release owned controls, never rebuild native resources or repost.
   Budget applies only to the currently executing callback of this token. */
typedef struct F1CBrokerRecoveryApi {
    F1CBrokerSetRecoveryFn set_recovery;
    F1CBrokerNativeBudgetFn native_budget;
} F1CBrokerRecoveryApi;
static inline int f1c_broker_recovery_bind(const F1CBrokerApi *api,F1CBrokerRecoveryApi *out){
    if(!api || !out || !(api->capabilities & F1C_BROKER_CAP_CALLBACK_RECOVERY))return 0;
    out->set_recovery=(F1CBrokerSetRecoveryFn)(uintptr_t)GetProcAddress(api->module,"F1CBrokerSetRecovery");
    out->native_budget=(F1CBrokerNativeBudgetFn)(uintptr_t)GetProcAddress(api->module,"F1CBrokerNativeBudget");
    return out->set_recovery && out->native_budget;
}
