/* coop_node_policy.h - 1.0.0 RequireMod node decision (spec section 12), 1.0.0 warn stage (section 13).
 * Pure C-compatible logic: no Win32, no game memory. The caller supplies the native node
 * list, the live claim ids and the clock, then acts on the returned events (boot, log).
 * Tested headless by tests/test_node_policy.cpp (run tests/test_node_policy.py). */
#ifndef COOP_NODE_POLICY_H
#define COOP_NODE_POLICY_H

#include <stdint.h>
#include <string.h>

#define NODE_POLICY_WATCH_MAX 32
#define NODE_POLICY_WARNED_MAX 64
#define NODE_POLICY_EVENT_MAX 64
#define NODE_WARN_DELAY_MS 3000 /* real boot: BOOT follows the WARN by this long (section 13) */

#define NODE_STATE_WATCHING 0
#define NODE_STATE_CLAIMED 1
#define NODE_STATE_BOOTED 2

#define NODE_EV_CLAIMED 1      /* node claims the mod: never booted */
#define NODE_EV_BOOT 2         /* caller must boot this node now */
#define NODE_EV_RACE_DEFER 3   /* grace expired during a race: boot deferred (logged once) */
#define NODE_EV_OVERFLOW 4     /* native list larger than the watch limit: no action */
#define NODE_EV_WARN 5         /* real boot due: send the notice now; BOOT follows NODE_WARN_DELAY_MS later */

typedef struct {
    int32_t node;
    uint32_t first_ms;
    int32_t state;
    int warn_pending;   /* WARN sent, BOOT waits for NODE_WARN_DELAY_MS */
    uint32_t warn_ms;
} NodePolicyWatch;

typedef struct {
    int32_t kind;
    int32_t node;
} NodePolicyEvent;

typedef struct {
    NodePolicyWatch watch[NODE_POLICY_WATCH_MAX];
    uint32_t watch_count;
    int race_logged;
    int warn_stage;                          /* 1 = real boot mode (node_policy_boot_mode) */
    int32_t warned[NODE_POLICY_WARNED_MAX];  /* nodes that got a WARN this session */
    uint32_t warned_count;
} NodePolicyState;

static inline int node_policy_contains(const int32_t *ids, uint32_t count, int32_t node)
{
    for (uint32_t i = 0; i < count; ++i)
        if (ids[i] == node) return 1;
    return 0;
}

static inline void node_policy_emit(NodePolicyEvent *events, uint32_t event_max, uint32_t *event_count,
                                    int32_t kind, int32_t node)
{
    if (*event_count < event_max) {
        events[*event_count].kind = kind;
        events[*event_count].node = node;
        ++*event_count;
    }
}

/* One decision step. Returns the number of BOOT events. A node is booted at most once
 * (state BOOTED is kept while it stays in the list). A claimed node that stops claiming
 * gets a fresh grace period. Non-host callers get no events and no boots.
 * Warn stage (warn_stage = 1, real boot mode only): the first due boot sends NODE_EV_WARN once
 * per node per session, then BOOT NODE_WARN_DELAY_MS later. A claim cancels the wait; a race
 * start defers it (as before). Dry-run never warns (node_policy_boot_mode). warn_stage = 0 keeps
 * the immediate BOOT. A full warned list (64 nodes) sends no further WARN, boots still follow. */
static inline uint32_t node_policy_step(NodePolicyState *st, int is_host, int32_t own,
                                        const int32_t *list, uint32_t list_count,
                                        const int32_t *claims, uint32_t claim_count,
                                        uint32_t now_ms, uint32_t grace_ms, int in_race,
                                        NodePolicyEvent *events, uint32_t event_max,
                                        uint32_t *event_count)
{
    uint32_t boots = 0;
    NodePolicyWatch next[NODE_POLICY_WATCH_MAX];
    uint32_t next_count = 0;
    *event_count = 0;
    if (!is_host) return 0;
    if (list_count > NODE_POLICY_WATCH_MAX) {
        node_policy_emit(events, event_max, event_count, NODE_EV_OVERFLOW, (int32_t)list_count);
        return 0;
    }
    for (uint32_t i = 0; i < list_count; ++i) {
        int32_t node = list[i];
        NodePolicyWatch entry;
        if (node == own) continue;
        entry.node = node;
        entry.first_ms = now_ms;
        entry.state = NODE_STATE_WATCHING;
        entry.warn_pending = 0;
        entry.warn_ms = 0;
        for (uint32_t j = 0; j < st->watch_count; ++j) {
            if (st->watch[j].node == node) {
                entry = st->watch[j];
                break;
            }
        }
        next[next_count++] = entry;
    }
    memcpy(st->watch, next, sizeof(NodePolicyWatch) * next_count);
    st->watch_count = next_count;

    for (uint32_t i = 0; i < next_count; ++i) {
        NodePolicyWatch *w = &st->watch[i];
        if (w->state == NODE_STATE_BOOTED) continue;
        if (node_policy_contains(claims, claim_count, w->node)) {
            if (w->state != NODE_STATE_CLAIMED)
                node_policy_emit(events, event_max, event_count, NODE_EV_CLAIMED, w->node);
            w->state = NODE_STATE_CLAIMED;
            w->warn_pending = 0;
            continue;
        }
        if (w->state == NODE_STATE_CLAIMED) {
            w->state = NODE_STATE_WATCHING;
            w->first_ms = now_ms;
        }
        if (now_ms - w->first_ms <= grace_ms) continue;
        if (in_race) {
            w->warn_pending = 0;
            if (!st->race_logged) {
                st->race_logged = 1;
                node_policy_emit(events, event_max, event_count, NODE_EV_RACE_DEFER, w->node);
            }
            continue;
        }
        if (st->warn_stage) {
            if (!w->warn_pending) {
                w->warn_pending = 1;
                w->warn_ms = now_ms;
                if (!node_policy_contains(st->warned, st->warned_count, w->node) &&
                    st->warned_count < NODE_POLICY_WARNED_MAX) {
                    st->warned[st->warned_count++] = w->node;
                    node_policy_emit(events, event_max, event_count, NODE_EV_WARN, w->node);
                }
                continue;
            }
            if (now_ms - w->warn_ms < NODE_WARN_DELAY_MS) continue;
        }
        node_policy_emit(events, event_max, event_count, NODE_EV_BOOT, w->node);
        w->state = NODE_STATE_BOOTED;
        ++boots;
    }
    return boots;
}

/* Boot mode for this tick: real boot (warn stage on) unless dry-run. Dry-run keeps the
 * immediate decision and never warns. */
static inline void node_policy_boot_mode(NodePolicyState *st, int dry_run)
{
    st->warn_stage = dry_run ? 0 : 1;
}

/* Caller-side gate: a BOOT event becomes a native boot only when not in dry-run.
 * Dry-run (CO-OP default, [Net] RequireModCoop=0) logs the decision and sends nothing. */
static inline int node_policy_should_boot(int32_t kind, int dry_run)
{
    return kind == NODE_EV_BOOT && !dry_run;
}

/* Bot guard (spec section 14, 2026-10-13): keep only the native list ids whose index is not
 * flagged ineligible[i] != 0 (AI bot slot, bot car, or no loaded entry name). Order is kept.
 * Only eligible ids reach node_policy_step, so a flagged node can never get WARN or BOOT.
 * Returns the number of ids written to out (out must hold list_count entries). */
static inline uint32_t node_policy_filter_eligible(const int32_t *list, const uint8_t *ineligible,
                                                   uint32_t list_count, int32_t *out)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < list_count; ++i)
        if (!ineligible[i]) out[n++] = list[i];
    return n;
}

/* Token parser: p must point at the space before the number. Accepts an optional '-'
 * and digits, followed by end of string, space, CR or LF. */
static inline int node_policy_parse_token(const char *p, int32_t *node)
{
    int negative = 0;
    int32_t value = 0;
    int digits = 0;
    if (*p != ' ') return 0;
    ++p;
    if (*p == '-') { negative = 1; ++p; }
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        ++digits;
        ++p;
        if (digits > 9) return 0;
    }
    if (digits == 0) return 0;
    if (*p != '\0' && *p != ' ' && *p != '\r' && *p != '\n') return 0;
    *node = negative ? -value : value;
    return 1;
}

/* "COOP1? <node>" -> 1 and node. The bare legacy "COOP1?" -> 0 (unclaimed). */
static inline int node_policy_parse_coop1(const char *text, int32_t *node)
{
    if (strncmp(text, "COOP1?", 6) != 0) return 0;
    return node_policy_parse_token(text + 6, node);
}

/* "NET1? <link> <rtt> <node>" -> 1 and node. The two-token legacy form -> 0 (unclaimed). */
static inline int node_policy_parse_net1(const char *text, int32_t *node)
{
    const char *p = text;
    if (strncmp(p, "NET1?", 5) != 0) return 0;
    p += 5;
    for (int field = 0; field < 2; ++field) {
        if (*p != ' ') return 0;
        ++p;
        while (*p != '\0' && *p != ' ' && *p != '\r' && *p != '\n') ++p;
    }
    return node_policy_parse_token(p, node);
}

#endif /* COOP_NODE_POLICY_H */
