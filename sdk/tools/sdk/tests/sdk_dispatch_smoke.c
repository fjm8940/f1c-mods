#include <windows.h>
#include "f1c_runtime.h"

typedef struct DispatchTestTask {
    int id;
} DispatchTestTask;

static F1CGameDispatch queue;
static DispatchTestTask tasks[70];
static int observed[70];
static int observed_count;

static void record_task(void *context)
{
    DispatchTestTask *task = (DispatchTestTask *)context;
    observed[observed_count++] = task->id;
}

static DWORD WINAPI wrong_thread_drain(void *unused)
{
    (void)unused;
    return f1c_dispatch_drain_bounded(&queue, 1);
}

int main(void)
{
    HANDLE thread;
    DWORD thread_result = 1;
    int index;

    f1c_dispatch_init(&queue);
    for (index = 0; index < F1C_DISPATCH_CAPACITY - 1; ++index) {
        tasks[index].id = index;
        if (!f1c_dispatch_post(&queue, record_task, &tasks[index])) return 1;
    }
    tasks[63].id = 63;
    if (f1c_dispatch_post(&queue, record_task, &tasks[63])) return 2;
    if (f1c_dispatch_drain_bounded(&queue, 7) != 7) return 3;
    if (observed_count != 7) return 4;

    thread = CreateThread(NULL, 0, wrong_thread_drain, NULL, 0, NULL);
    if (!thread) return 5;
    WaitForSingleObject(thread, INFINITE);
    GetExitCodeThread(thread, &thread_result);
    CloseHandle(thread);
    if (thread_result != 0 || observed_count != 7) return 6;

    for (index = 63; index < 70; ++index) {
        tasks[index].id = index;
        if (!f1c_dispatch_post(&queue, record_task, &tasks[index])) return 7;
    }
    if (f1c_dispatch_drain(&queue) != F1C_DISPATCH_CAPACITY - 1) return 8;
    if (observed_count != 70) return 9;
    for (index = 0; index < 70; ++index)
        if (observed[index] != index) return 10;
    if (f1c_dispatch_drain_bounded(&queue, 1) != 0) return 11;
    return 0;
}
