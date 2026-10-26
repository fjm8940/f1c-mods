/* GENERATED VARIANT — do not edit by hand.
 * Produced by tools/build_v3_variant.py from the accepted v2 session
 * (hash-guarded). The accepted source is unchanged.
 */
/*
 * pit_failure_session.c — service identity, bounded lifecycle storage,
 * shared-clock validation and exactly-once command delivery.
 *
 * Pure C11; no game/OS APIs. See pit_failure_policy.h for the public contract.
 *
 * Storage model (R1):
 *   - `active` holds services currently being serviced (planned/resolving).
 *   - `history` retains completed/cancelled identities for the current epoch
 *     so duplicate notifications return the stored plan and cancelled tokens
 *     stay rejected. History is bounded (PF_HISTORY_MAX); when full, commit
 *     fails explicitly with PF_SESSION_ERR_CAPACITY and never evicts silently.
 *   - `new_epoch` is the only reset; stale epochs are refused.
 *
 * Delivery model (R2/R4):
 *   - Due events are appended once to `queue`; `advance` drains them. Events
 *     are never dropped on capacity shortfall; the caller sees the written
 *     count and can query the pending count.
 *   - `sequence` increments only when an event is accepted into the queue, so
 *     it is the stable per-service event index under any polling schedule.
 */
#include "pit_failure_policy.h"
#include "pit_failure_policy_v3.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    int used;
    uint64_t epoch;
    uint64_t vehicle_id;
    uint64_t service_token;
    pf_session_state_t state;
    double committed_at;               /* paused-adjusted clock at commit */
    pf_context_t context;
    pf_eligibility_t eligibility;
    pf3_plan_t plan;
    pf_timing_result_t timing;
    int timing_bound;
    int delay_known;
    double added_stop_time;
    int attempt_count;
    unsigned int emitted_mask;
    int notice_emitted;
    int final_emitted;
    int sequence;
} pf3_active_t;

typedef struct {
    uint64_t epoch;
    uint64_t vehicle_id;
    uint64_t service_token;
    uint8_t state;                     /* resolved / terminal-requested / cancelled */
    uint8_t has_plan;                  /* 0 for cancelled entries */
    pf3_plan_t plan;
} pf3_history_t;

struct pf3_session {
    pf3_config_t config;
    uint64_t seed;
    uint64_t epoch;
    int have_epoch;
    double last_time;
    int have_time;
    int invalid;
    int paused;
    double paused_at;
    double paused_total;
    pf_session_error_t last_error;

    pf3_active_t *active;
    size_t active_count;
    size_t active_cap;

    pf3_history_t *history;
    size_t history_count;
    size_t history_cap;
    size_t history_reserved;           /* accepted services not yet retained */

    uint32_t *index;                   /* open addressing -> history index + 1 */
    size_t index_cap;

    pf_command_t *queue;
    size_t queue_count;
    size_t queue_cap;
};

/* ------------------------------------------------------------- helpers -- */

static int identity_matches(uint64_t epoch, uint64_t vehicle, uint64_t token,
                            const pf_identity_t *id)
{
    return epoch == id->epoch && vehicle == id->vehicle_id &&
           token == id->service_token;
}

static void clear_error_ok(pf3_session_t *session)
{
    session->last_error = PF_SESSION_OK;
}

static pf3_active_t *active_find(pf3_session_t *session, const pf_identity_t *id)
{
    size_t i;
    if (!id) {
        return NULL;
    }
    for (i = 0; i < session->active_count; i++) {
        pf3_active_t *slot = &session->active[i];
        if (identity_matches(slot->epoch, slot->vehicle_id, slot->service_token,
                             id)) {
            return slot;
        }
    }
    return NULL;
}

static uint64_t identity_hash(uint64_t epoch, uint64_t vehicle, uint64_t token)
{
    uint64_t h = pf_seed_mix(epoch ^ UINT64_C(0x243F6A8885A308D3));
    h = pf_seed_mix(h ^ vehicle);
    h = pf_seed_mix(h ^ token);
    return h;
}

/* Grows index/history storage so `needed` retained entries fit. Reservations
 * are taken at commit and are not yet counted in history_count. */
static int history_ensure(pf3_session_t *session, size_t needed)
{
    if (needed > PF_HISTORY_MAX) {
        return 0;
    }
    if (needed > session->history_cap) {
        size_t new_cap = session->history_cap ? session->history_cap : 256;
        pf3_history_t *grown;
        while (new_cap < needed) {
            if (new_cap > PF_HISTORY_MAX / 2) {
                new_cap = PF_HISTORY_MAX;
                break;
            }
            new_cap *= 2;
        }
        if (new_cap < needed) {
            return 0;
        }
        grown = (pf3_history_t *)realloc(session->history,
                                        new_cap * sizeof(*grown));
        if (!grown) {
            return 0;
        }
        session->history = grown;
        session->history_cap = new_cap;
    }
    {
        size_t required = needed * 10 / 7 + 1;
        if (session->index_cap < required) {
            size_t new_cap = session->index_cap ? session->index_cap : 1024;
            uint32_t *table;
            size_t i;
            while (new_cap < required) {
                if (new_cap > ((size_t)1 << 30)) {
                    return 0;
                }
                new_cap *= 2;
            }
            table = (uint32_t *)calloc(new_cap, sizeof(*table));
            if (!table) {
                return 0;
            }
            for (i = 0; i < session->history_count; i++) {
                const pf3_history_t *entry = &session->history[i];
                size_t slot =
                    (size_t)(identity_hash(entry->epoch, entry->vehicle_id,
                                           entry->service_token) &
                             (new_cap - 1));
                while (table[slot] != 0) {
                    slot = (slot + 1) & (new_cap - 1);
                }
                table[slot] = (uint32_t)(i + 1);
            }
            free(session->index);
            session->index = table;
            session->index_cap = new_cap;
        }
    }
    return 1;
}

/* Reserves one retention slot for a newly accepted service (R9). */
static int history_reserve(pf3_session_t *session)
{
    if (session->history_count + session->history_reserved >= PF_HISTORY_MAX) {
        return 0;
    }
    if (!history_ensure(session,
                        session->history_count + session->history_reserved + 1)) {
        return 0;
    }
    session->history_reserved++;
    return 1;
}

static pf3_history_t *history_find(pf3_session_t *session,
                                  const pf_identity_t *id)
{
    size_t probe;
    size_t start;
    if (!id || !session->index || session->index_cap == 0) {
        return NULL;
    }
    start = (size_t)(identity_hash(id->epoch, id->vehicle_id, id->service_token) &
                     (session->index_cap - 1));
    probe = start;
    for (;;) {
        uint32_t slot = session->index[probe];
        if (slot == 0) {
            return NULL;
        }
        {
            pf3_history_t *entry = &session->history[slot - 1];
            if (identity_matches(entry->epoch, entry->vehicle_id,
                                 entry->service_token, id)) {
                return entry;
            }
        }
        probe = (probe + 1) & (session->index_cap - 1);
        if (probe == start) {
            return NULL;
        }
    }
}

/* Retains identity + state (and plan when has_plan). Returns 1 on success.
 * Retention space for accepted services is reserved at commit time, so this
 * cannot fail for a service that was accepted (except after an explicit
 * reservation release, which no path performs). */
static int history_add(pf3_session_t *session, const pf3_active_t *slot,
                       pf_session_state_t state, int keep_plan)
{
    pf3_history_t *entry;
    size_t probe;

    if (session->history_count >= PF_HISTORY_MAX) {
        return 0;
    }
    if (!history_ensure(session, session->history_count + 1)) {
        return 0;
    }

    entry = &session->history[session->history_count];
    memset(entry, 0, sizeof(*entry));
    entry->epoch = slot->epoch;
    entry->vehicle_id = slot->vehicle_id;
    entry->service_token = slot->service_token;
    entry->state = (uint8_t)state;
    entry->has_plan = keep_plan ? 1 : 0;
    if (keep_plan) {
        entry->plan = slot->plan;
    }
    session->history_count++;
    if (session->history_reserved > 0) {
        session->history_reserved--;
    }

    probe = (size_t)(identity_hash(entry->epoch, entry->vehicle_id,
                                   entry->service_token) &
                     (session->index_cap - 1));
    while (session->index[probe] != 0) {
        probe = (probe + 1) & (session->index_cap - 1);
    }
    session->index[probe] = (uint32_t)session->history_count;
    return 1;
}

static int active_add(pf3_session_t *session, const pf3_active_t *slot)
{
    if (session->active_count >= PF_ACTIVE_MAX) {
        return 0;
    }
    if (session->active_count == session->active_cap) {
        size_t new_cap = session->active_cap ? session->active_cap * 2 : 32;
        pf3_active_t *grown;
        if (new_cap > PF_ACTIVE_MAX) {
            new_cap = PF_ACTIVE_MAX;
        }
        grown = (pf3_active_t *)realloc(session->active,
                                       new_cap * sizeof(*grown));
        if (!grown) {
            return 0;
        }
        session->active = grown;
        session->active_cap = new_cap;
    }
    session->active[session->active_count++] = *slot;
    return 1;
}

/* Removes active[index], preserving order. */
static void active_remove(pf3_session_t *session, size_t index)
{
    if (index + 1 < session->active_count) {
        memmove(&session->active[index], &session->active[index + 1],
                (session->active_count - index - 1) * sizeof(pf3_active_t));
    }
    session->active_count--;
}

static void queue_clear(pf3_session_t *session)
{
    session->queue_count = 0;
}

static int queue_push(pf3_session_t *session, const pf_command_t *cmd)
{
    if (session->queue_count >= PF_QUEUE_MAX) {
        return 0;
    }
    if (session->queue_count == session->queue_cap) {
        size_t new_cap = session->queue_cap ? session->queue_cap * 2 : 64;
        pf_command_t *grown;
        if (new_cap > PF_QUEUE_MAX) {
            new_cap = PF_QUEUE_MAX;
        }
        grown = (pf_command_t *)realloc(session->queue,
                                        new_cap * sizeof(*grown));
        if (!grown) {
            return 0;
        }
        session->queue = grown;
        session->queue_cap = new_cap;
    }
    session->queue[session->queue_count++] = *cmd;
    return 1;
}

/* Removes every undelivered command for one exact identity, preserving the
 * order of the remaining events. Delivered events are already gone and cannot
 * be recalled. */
static void queue_purge_identity(pf3_session_t *session, const pf_identity_t *id)
{
    size_t read_i;
    size_t write_i = 0;

    for (read_i = 0; read_i < session->queue_count; read_i++) {
        pf_command_t *cmd = &session->queue[read_i];
        if (identity_matches(cmd->identity_epoch, cmd->identity_vehicle,
                             cmd->identity_token, id)) {
            continue;
        }
        if (write_i != read_i) {
            session->queue[write_i] = *cmd;
        }
        write_i++;
    }
    session->queue_count = write_i;
}

static double effective_time(const pf3_session_t *session, double now)
{
    double adjusted = now - session->paused_total;
    if (session->paused) {
        double frozen = session->paused_at - session->paused_total;
        if (frozen < adjusted) {
            adjusted = frozen;
        }
    }
    return adjusted;
}

/* Invalidates the epoch: cancels every live service, discards undelivered
 * events and requires an explicit new_epoch before any further effects. */
static void invalidate_time(pf3_session_t *session)
{
    size_t i;
    for (i = 0; i < session->active_count; i++) {
        pf3_active_t *slot = &session->active[i];
        slot->state = PF_SESSION_CANCELLED;
        slot->final_emitted = 1;
        slot->notice_emitted = 1;
        slot->emitted_mask = ~0u;
        if (!history_add(session, slot, PF_SESSION_CANCELLED, 0)) {
            session->last_error = PF_SESSION_ERR_CAPACITY;
        }
    }
    session->active_count = 0;
    queue_clear(session);
    session->invalid = 1;
}

/* Shared clock validation. Returns 1 when `now` is accepted, 0 when the time
 * is a discontinuity (session invalidated) and -1 when the session already
 * requires a reset. */
static int touch_time(pf3_session_t *session, double now)
{
    if (session->invalid) {
        session->last_error = PF_SESSION_ERR_INVALID_STATE;
        return -1;
    }
    if (!isfinite(now)) {
        invalidate_time(session);
        session->last_error = PF_SESSION_ERR_INVALID_TIME;
        return 0;
    }
    if (session->have_time && now < session->last_time - 1e-9) {
        invalidate_time(session);
        session->last_error = PF_SESSION_ERR_INVALID_TIME;
        return 0;
    }
    session->last_time = now;
    session->have_time = 1;
    return 1;
}

/* --------------------------------------------------------------- public -- */

pf3_session_t *pf3_session_create(const pf3_config_t *cfg, uint64_t seed)
{
    pf3_session_t *session;
    char err[1];

    if (!cfg) {
        return NULL;
    }
    if (pf3_config_validate((pf3_config_t *)cfg, err, 0) != PF_CONFIG_OK) {
        return NULL;
    }
    session = (pf3_session_t *)calloc(1, sizeof(*session));
    if (!session) {
        return NULL;
    }
    session->config = *cfg;
    session->seed = seed;
    return session;
}

void pf3_session_destroy(pf3_session_t *session)
{
    if (!session) {
        return;
    }
    free(session->active);
    free(session->history);
    free(session->index);
    free(session->queue);
    free(session);
}

uint64_t pf3_session_seed(const pf3_session_t *session)
{
    return session ? session->seed : 0;
}

static void reset_state(pf3_session_t *session)
{
    session->active_count = 0;
    session->history_count = 0;
    session->history_reserved = 0;
    if (session->index && session->index_cap) {
        memset(session->index, 0,
               session->index_cap * sizeof(session->index[0]));
    }
    queue_clear(session);
    session->invalid = 0;
    session->have_time = 0;
    session->last_time = 0.0;
    session->paused = 0;
    session->paused_at = 0.0;
    session->paused_total = 0.0;
}

int pf3_session_new_epoch(pf3_session_t *session, uint64_t epoch)
{
    if (!session) {
        return -1;
    }
    reset_state(session);
    session->epoch = epoch;
    session->have_epoch = 1;
    clear_error_ok(session);
    return 0;
}

int pf3_session_pause(pf3_session_t *session, double now)
{
    int touched;
    if (!session) {
        return -1;
    }
    if (session->paused) {
        session->last_error = PF_SESSION_ERR_INVALID_STATE;
        return -1;
    }
    touched = touch_time(session, now);
    if (touched != 1) {
        return -1;
    }
    session->paused = 1;
    session->paused_at = now;
    clear_error_ok(session);
    return 0;
}

int pf3_session_resume(pf3_session_t *session, double now)
{
    int touched;
    if (!session || !session->paused) {
        if (session) {
            session->last_error = PF_SESSION_ERR_INVALID_STATE;
        }
        return -1;
    }
    touched = touch_time(session, now);
    if (touched != 1) {
        return -1;
    }
    session->paused_total += now - session->paused_at;
    session->paused = 0;
    clear_error_ok(session);
    return 0;
}

static int session_commit_impl(pf3_session_t *session, const pf_identity_t *id,
                               const pf_context_t *ctx,
                               const pf_eligibility_t *eligibility, double now,
                               const pf3_forcing_t *forcing,
                               const pf3_plan_t *given, pf3_plan_t *out_plan,
                               int *out_committed)
{
    pf3_active_t slot;
    pf3_history_t *done;
    pf_context_t frozen;
    int touched;

    if (out_plan) {
        memset(out_plan, 0, sizeof(*out_plan));
    }
    if (out_committed) {
        *out_committed = 0;
    }
    if (!session || !id || !eligibility) {
        if (session) {
            session->last_error = PF_SESSION_ERR_REFUSED;
        }
        return -1;
    }
    if (!session->have_epoch || id->epoch != session->epoch) {
        session->last_error = PF_SESSION_ERR_REFUSED;
        return -1;
    }
    touched = touch_time(session, now);
    if (touched != 1) {
        return -1;
    }

    {
        pf3_active_t *live = active_find(session, id);
        if (live) {
            if (out_plan) {
                *out_plan = live->plan;
            }
            session->last_error = PF_SESSION_ERR_DUPLICATE;
            return 0;
        }
    }
    done = history_find(session, id);
    if (done) {
        if (done->state == PF_SESSION_CANCELLED) {
            session->last_error = PF_SESSION_ERR_TOKEN_CANCELLED;
            return -1;
        }
        if (done->has_plan) {
            if (out_plan) {
                *out_plan = done->plan;
            }
            session->last_error = PF_SESSION_ERR_DUPLICATE;
            return 0;
        }
        session->last_error = PF_SESSION_ERR_REFUSED;
        return -1;
    }

    memset(&frozen, 0, sizeof(frozen));
    if (ctx) {
        frozen = *ctx;
    }
    frozen.eligibility = *eligibility;
    /* R2: existing identities were resolved above; validate NEW selections
     * against ONE frozen context (authoritative eligibility argument) BEFORE
     * acceptance/reservation. An invalid forced selection never consumes an
     * identity as a clean incident. */
    if (!given && forcing &&
        !pf3_forcing_valid(&session->config, &frozen, forcing, NULL, 0)) {
        session->last_error = PF_SESSION_ERR_REFUSED;
        if (out_plan) {
            memset(out_plan, 0, sizeof(*out_plan));
        }
        return -1;
    }
    /* Reserve retention space before accepting so completion can never fail
     * for an accepted service and unrelated queued output is never trapped. */
    if (!history_reserve(session)) {
        session->last_error = PF_SESSION_ERR_CAPACITY;
        return -1;
    }

    memset(&slot, 0, sizeof(slot));
    slot.used = 1;
    slot.epoch = id->epoch;
    slot.vehicle_id = id->vehicle_id;
    slot.service_token = id->service_token;
    slot.state = PF_SESSION_SERVICE_PLANNED;
    slot.committed_at = effective_time(session, now);
    slot.context = frozen;
    slot.eligibility = *eligibility;
    if (given)
        slot.plan = *given;
    else
        pf3_plan_build_full(&session->config, id, &frozen, session->seed,
                            forcing, &slot.plan);
    slot.attempt_count = slot.plan.attempts > 0 ? slot.plan.attempts : 1;
    if (!active_add(session, &slot)) {
        session->history_reserved--;
        session->last_error = PF_SESSION_ERR_CAPACITY;
        return -1;
    }
    if (out_plan) {
        *out_plan = slot.plan;
    }
    if (out_committed) {
        *out_committed = 1;
    }
    clear_error_ok(session);
    return 0;
}

int pf3_session_commit(pf3_session_t *session, const pf_identity_t *id,
                       const pf_context_t *ctx,
                       const pf_eligibility_t *eligibility, double now,
                       const pf3_forcing_t *forcing, pf3_plan_t *out_plan,
                       int *out_committed)
{
    return session_commit_impl(session, id, ctx, eligibility, now, forcing,
                               NULL, out_plan, out_committed);
}

int pf3_session_commit_given(pf3_session_t *session, const pf_identity_t *id,
                             const pf_context_t *ctx,
                             const pf_eligibility_t *eligibility, double now,
                             const pf3_plan_t *given, pf3_plan_t *out_plan,
                             int *out_committed)
{
    if (!given) {
        if (session) session->last_error = PF_SESSION_ERR_REFUSED;
        return -1;
    }
    return session_commit_impl(session, id, ctx, eligibility, now, NULL,
                               given, out_plan, out_committed);
}

int pf3_session_bind_timing(pf3_session_t *session, const pf_identity_t *id,
                           const pf_job_t *jobs, size_t n)
{
    pf3_active_t *slot;
    if (!session || !id) {
        return -1;
    }
    slot = active_find(session, id);
    if (!slot) {
        session->last_error = PF_SESSION_ERR_REFUSED;
        return -1;
    }
    slot->timing_bound = 1;
    if (pf3_plan_apply_timing(&slot->plan, jobs, n, &slot->timing)) {
        slot->delay_known = 1;
        slot->added_stop_time = slot->timing.added_stop_time;
        clear_error_ok(session);
        return 1;
    }
    slot->delay_known = 0;
    slot->added_stop_time = 0.0;
    clear_error_ok(session);
    return 0;
}

static double event_offset(const pf3_active_t *slot, int index)
{
    double total;
    int attempts;

    if (slot->plan.severity == PF_TERMINAL) {
        return slot->plan.terminal_presentation_seconds;
    }
    total = slot->plan.extra_job_seconds;
    if (!(total > 0.0) || !isfinite(total)) {
        total = 0.0;
    }
    attempts = slot->attempt_count > 0 ? slot->attempt_count : 1;
    if (index >= attempts || attempts <= 1) {
        return total;
    }
    return total * (double)index / (double)attempts;
}

/* Queues one event if due and not yet emitted. Returns 1 when queued,
 * 0 when already emitted/not due, -1 on queue capacity exhaustion. */
static int queue_event(pf3_session_t *session, pf3_active_t *slot, int index)
{
    pf_command_t cmd;
    int attempts;
    int final_index;

    attempts = slot->attempt_count > 0 ? slot->attempt_count : 1;
    final_index = attempts;
    memset(&cmd, 0, sizeof(cmd));
    cmd.identity_epoch = slot->epoch;
    cmd.identity_vehicle = slot->vehicle_id;
    cmd.identity_token = slot->service_token;
    cmd.outcome = slot->plan.outcome;
    cmd.severity = slot->plan.severity;
    cmd.delay_known = slot->delay_known;
    cmd.added_stop_time = slot->added_stop_time;
    cmd.terminal = slot->plan.terminal;

    if (index == 0) {
        if (slot->notice_emitted) {
            return 0;
        }
        cmd.kind = PF_CMD_FAULT_NOTICE;
        cmd.attempt = 0;
        cmd.due_seconds = 0.0;
        cmd.delay_known = 0;
        cmd.added_stop_time = 0.0;
    } else if (index >= final_index) {
        if (slot->final_emitted) {
            return 0;
        }
        cmd.kind = slot->plan.terminal ? PF_CMD_TERMINAL_REQUEST
                                       : PF_CMD_RECOVERED;
        cmd.attempt = slot->plan.terminal ? 0 : attempts;
        cmd.due_seconds = event_offset(slot, index);
    } else {
        if (slot->emitted_mask & (1u << (unsigned)index)) {
            return 0;
        }
        cmd.kind = PF_CMD_RECOVERY_ATTEMPT;
        cmd.attempt = index;
        cmd.due_seconds = event_offset(slot, index);
        cmd.delay_known = 0;
        cmd.added_stop_time = 0.0;
    }

    /* The sequence is the per-service event index; it advances only when the
     * event is accepted into the delivery queue. */
    cmd.sequence = slot->sequence + 1;
    if (!queue_push(session, &cmd)) {
        return -1;
    }
    slot->sequence = cmd.sequence;

    if (index == 0) {
        slot->notice_emitted = 1;
        slot->state = PF_SESSION_RESOLVING;
    } else if (index >= final_index) {
        slot->final_emitted = 1;
        slot->state = slot->plan.terminal ? PF_SESSION_TERMINAL_REQUESTED
                                          : PF_SESSION_RESOLVED;
    } else {
        slot->emitted_mask |= 1u << (unsigned)index;
    }
    return 1;
}

int pf3_session_advance(pf3_session_t *session, double now, pf_command_t *cmds,
                       int cmd_capacity)
{
    double adjusted;
    int touched;
    size_t i;
    int query;
    int blocked = 0;

    if (!session) {
        return -1;
    }
    touched = touch_time(session, now);
    if (touched != 1) {
        return -1;
    }
    adjusted = effective_time(session, now);
    query = (cmds == NULL || cmd_capacity <= 0);

    /* Producer phase: queue due events. When the delivery queue is full (or a
     * retention step is blocked), stop producing, keep the affected service
     * live so its unqueued events are retried later, and continue to the drain
     * phase: a saturated queue must never block draining (R9). */
    i = 0;
    while (i < session->active_count) {
        pf3_active_t *slot = &session->active[i];
        double elapsed;
        int attempts;
        int index;
        int resolved = 0;

        if (slot->plan.severity == PF_CLEAN) {
            if (!history_add(session, slot, PF_SESSION_RESOLVED, 1)) {
                blocked = 1;
                break;
            }
            slot->state = PF_SESSION_RESOLVED;
            active_remove(session, i);
            continue;
        }
        elapsed = adjusted - slot->committed_at;
        if (elapsed < 0.0) {
            elapsed = 0.0;
        }
        attempts = slot->attempt_count > 0 ? slot->attempt_count : 1;
        for (index = 0; index <= attempts; index++) {
            double due = index == 0 ? 0.0 : event_offset(slot, index);
            int queued;
            if (elapsed + 1e-9 < due) {
                break;
            }
            queued = queue_event(session, slot, index);
            if (queued < 0) {
                blocked = 1;
                break;
            }
            if (slot->final_emitted) {
                resolved = 1;
                break;
            }
        }
        if (blocked) {
            break;
        }
        if (resolved) {
            if (!history_add(session, slot, slot->state, 1)) {
                blocked = 1;
                break;
            }
            active_remove(session, i);
            continue;
        }
        i++;
    }
    if (blocked) {
        session->last_error = PF_SESSION_ERR_CAPACITY;
    }

    if (query) {
        if (!blocked) {
            clear_error_ok(session);
        }
        return (int)session->queue_count;
    }
    {
        int written = 0;
        int capacity = cmd_capacity > 0 ? cmd_capacity : 0;
        while (written < capacity && session->queue_count > 0) {
            cmds[written++] = session->queue[0];
            if (session->queue_count > 1) {
                memmove(&session->queue[0], &session->queue[1],
                        (session->queue_count - 1) * sizeof(pf_command_t));
            }
            session->queue_count--;
        }
        if (!blocked) {
            clear_error_ok(session);
        }
        return written;
    }
}

int pf3_session_pending(const pf3_session_t *session)
{
    return session ? (int)session->queue_count : 0;
}

pf_session_error_t pf3_session_last_error(const pf3_session_t *session)
{
    return session ? session->last_error : PF_SESSION_ERR_INVALID_STATE;
}

int pf3_session_cancel(pf3_session_t *session, const pf_identity_t *id, double now)
{
    pf3_active_t *slot;
    pf3_history_t *done;
    int touched;
    if (!session || !id) {
        return -1;
    }
    touched = touch_time(session, now);
    if (touched != 1) {
        return -1;
    }
    slot = active_find(session, id);
    if (slot) {
        /* Retain the tombstone before mutating so a capacity failure leaves
         * the live service untouched and cancellable later. */
        if (!history_add(session, slot, PF_SESSION_CANCELLED, 0)) {
            session->last_error = PF_SESSION_ERR_CAPACITY;
            return -1;
        }
        slot->state = PF_SESSION_CANCELLED;
        slot->final_emitted = 1;
        slot->notice_emitted = 1;
        slot->emitted_mask = ~0u;
        {
            size_t i;
            for (i = 0; i < session->active_count; i++) {
                if (&session->active[i] == slot) {
                    active_remove(session, i);
                    break;
                }
            }
        }
        queue_purge_identity(session, id);
        clear_error_ok(session);
        return 0;
    }

    /* The service may already have completed into history with its final
     * event queued but undrained: cancellation must still purge delivery. */
    done = history_find(session, id);
    if (!done) {
        session->last_error = PF_SESSION_ERR_REFUSED;
        return -1;
    }
    if (done->state == PF_SESSION_CANCELLED) {
        session->last_error = PF_SESSION_ERR_REFUSED;
        return -1;
    }
    done->state = PF_SESSION_CANCELLED;
    done->has_plan = 0;
    queue_purge_identity(session, id);
    clear_error_ok(session);
    return 0;
}

int pf3_session_state(const pf3_session_t *session, const pf_identity_t *id)
{
    size_t i;
    if (!session || !id) {
        return -1;
    }
    for (i = 0; i < session->active_count; i++) {
        if (identity_matches(session->active[i].epoch,
                             session->active[i].vehicle_id,
                             session->active[i].service_token, id)) {
            return (int)session->active[i].state;
        }
    }
    {
        pf3_history_t *entry = history_find((pf3_session_t *)session, id);
        if (entry) {
            return (int)entry->state;
        }
    }
    return -1;
}

int pf3_session_plan(const pf3_session_t *session, const pf_identity_t *id,
                    pf3_plan_t *out)
{
    size_t i;
    if (!session || !id || !out) {
        return -1;
    }
    for (i = 0; i < session->active_count; i++) {
        if (identity_matches(session->active[i].epoch,
                             session->active[i].vehicle_id,
                             session->active[i].service_token, id)) {
            *out = session->active[i].plan;
            return 0;
        }
    }
    {
        pf3_history_t *entry = history_find((pf3_session_t *)session, id);
        if (entry && entry->has_plan) {
            *out = entry->plan;
            return 0;
        }
    }
    return -1;
}

int pf3_session_count(const pf3_session_t *session)
{
    return session ? (int)session->active_count : 0;
}

uint64_t pf3_session_epoch_get(const pf3_session_t *session)
{
    if (!session) {
        return UINT64_MAX;
    }
    return session->epoch;
}
