/* CO-OP sender trust rules for the UDP links (security fix F-19 / F-20, 2026-10-13).
 * Pure functions: no Windows or game access, so tests/test_coop_trust.py can
 * compile and check them headless. Included by coop_mode.cpp.
 *
 * Client: the only trusted sender of manifest text and MF datagrams is the host
 * address of this game process's own established TCP session to the game port
 * (game_session_host_ip). A datagram sender never becomes the host. With no
 * such session, nothing is trusted.
 *
 * Host: MF datagrams and MF relay registration are accepted only from addresses
 * that hold an established game TCP session to this host (game_session_peer_ips).
 *
 * Limit: these are IPv4 address checks. A LAN attacker who spoofs the host's IP
 * as UDP source is not stopped by them (manifest and MF datagrams are still
 * unauthenticated). The protocol-level fix is M7 (HMAC on UDP), not done here. */
#ifndef COOP_TRUST_H
#define COOP_TRUST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Client: 1 when a datagram from from_ip may be treated as coming from the host.
 * host_ip 0 means no game TCP session to a host, so nothing is trusted. */
static inline int coop_trust_client_sender_ok(uint32_t host_ip, uint32_t from_ip)
{
    return host_ip != 0 && from_ip == host_ip;
}

/* Host: 1 when from_ip is one of the peers with an established game TCP session. */
static inline int coop_trust_host_sender_ok(const uint32_t *peers, int count, uint32_t from_ip)
{
    if (!peers || count <= 0 || from_ip == 0) return 0;
    for (int i = 0; i < count; ++i)
        if (peers[i] == from_ip) return 1;
    return 0;
}

/* Rate limit for rejected-sender log lines: at most one line per interval.
 * The caller logs the reject count since the last line. */
typedef struct {
    uint32_t last_ms;    /* 0 = nothing logged yet */
    uint32_t suppressed; /* rejects skipped since the last logged line */
} CoopTrustLogState;

/* Returns 1 when a line should be written now. *suppressed receives the number of
 * rejects skipped since the previous line. Returns 0 (and counts) otherwise. */
static inline int coop_trust_should_log(CoopTrustLogState *state, uint32_t now_ms,
                                        uint32_t interval_ms, uint32_t *suppressed)
{
    if (state->last_ms != 0 && now_ms - state->last_ms < interval_ms) {
        ++state->suppressed;
        return 0;
    }
    if (suppressed) *suppressed = state->suppressed;
    state->suppressed = 0;
    state->last_ms = now_ms ? now_ms : 1;
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* COOP_TRUST_H */
