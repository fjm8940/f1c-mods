/* f1c_crash_capture_test.c - headless check of the 2026-10-11 crash capture in
 * f1c_usage.h. Build 32-bit into an EMPTY temp folder and run there; it faults
 * inside ntdll (RtlFillMemory on NULL), which the old exe/own-image filter
 * never reported. f1c_crash_capture_check.py then validates the outputs. */
#include <windows.h>
#include <stdio.h>
#include "f1c_usage.h"

static int g_cb;
static void cb(void) { char l[F1C_USAGE_LINE]; f1c_usage_exc_line(l, sizeof l); g_cb++; printf("CB %s\n", l); fflush(stdout); }
static LONG WINAPI quiet(EXCEPTION_POINTERS *ep) { (void)ep; printf("FILTER reached\n"); fflush(stdout); ExitProcess(0); return EXCEPTION_EXECUTE_HANDLER; }

typedef void (WINAPI *FillFn)(void *, SIZE_T, BYTE);
int main(void)
{
    FillFn fill = (FillFn)(void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlFillMemory");
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    SetUnhandledExceptionFilter(quiet);            /* the "game's" filter, chained by ours */
    f1c_usage_crash_identity("coop_mode", "9.9.9");
    f1c_usage_veh_install(cb);
    IsBadReadPtr((void *)16, 4);                    /* kernelbase probe: must NOT report */
    printf("probe done cb=%d\n", g_cb); fflush(stdout);
    fill(NULL, 64, 0x41);                           /* AV inside ntdll */
    printf("NOT REACHED\n");
    return 1;
}
