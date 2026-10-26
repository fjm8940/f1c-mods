/* mf_coop_engine.h - CO-OP sync for the engine family (2026-10-10, Claude).
 *
 * Included ONCE inside engine_failure.c (needs its AI slot table, effect
 * emitters and audio). Spec: docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md
 *
 * Rule: the PC that simulates a car owns its failures. Host = bots (marker
 * 1) + its own car; a client = its own car. The owner runs the normal MF
 * model and mirrors every visible effect as an event over the CO-OP link
 * (coop_mf_bridge.h). Other PCs never model that car; they replay the
 * effect at their copy of it (marker 2): particle bursts, smoke banks, the
 * blow-up fire, positional sounds and the retirement.
 *
 * Single-player: g_mfc_latched stays 0, every hook returns at once. */

#define MFC_RQ_MAX        32
#define MFC_DEDUP         128
#define MFC_BANK_KEEP_MS  3000u
#define MFC_FIRE_MAX_MS   20000.0

typedef struct { int id, n, vslot; float rate, spread, up; unsigned gen; } MfcBurst;
static MfcBurst g_mfc_rq[MFC_RQ_MAX];
static CRITICAL_SECTION g_mfc_rq_cs;
static volatile LONG g_mfc_rq_cs_ok = 0;
static uint32_t g_mfc_seq = 0;
static uint32_t g_mfc_seen[MFC_DEDUP];
static int g_mfc_seen_i = 0;
/* owner side: per vehicle slot last sent banks / retire state */
static float g_mfc_bank_sent[MFC_VEH_SLOTS][3];
static DWORD g_mfc_bank_ms[MFC_VEH_SLOTS];
static int   g_mfc_retire_sent[MFC_VEH_SLOTS];
/* receiver side: remote fire owners (g_blow_fx_start[MF_AI_MAX + k]) */
static int   g_mfc_fire_vslot[MFC_REMOTE_FX];
static LONG  g_mfc_stat_tx = 0, g_mfc_stat_rx = 0;
static DWORD g_mfc_stat_ms = 0;

/* ---------------------------------------------------------------- helpers */
static void mfc_car_name(int vslot, char *out, int cap)
{
    const char *p = (const char*)(mfc_veh(vslot) + AI_OFF_NAME);
    int i = 0;
    out[0] = '\0';
    if (IsBadReadPtr(p, 24)) return;
    while (i < cap - 1 && p[i]) { out[i] = p[i]; i++; }
    out[i] = '\0';
}

/* the PC-local car an event names: same slot + name, else any remote copy
 * with that name. Only marker 2 (another PC's car) is ever touched. */
static int mfc_find_remote(const CoopMfEvent *e)
{
    char nm[24];
    int s;
    if (!e->name[0]) return -1;
    if (e->vslot >= 0 && e->vslot < MFC_VEH_SLOTS && mfc_marker(e->vslot) == 2) {
        mfc_car_name(e->vslot, nm, (int)sizeof nm);
        if (strncmp(nm, e->name, sizeof nm) == 0) return e->vslot;
    }
    for (s = 0; s < MFC_VEH_SLOTS; s++) {
        if (mfc_marker(s) != 2) continue;
        mfc_car_name(s, nm, (int)sizeof nm);
        if (strncmp(nm, e->name, sizeof nm) == 0) return s;
    }
    return -1;
}

/* owner index (0 player, 1.. MF AI index) -> local vslot, -1 = not ours */
static int mfc_owned_vslot(int owner)
{
    int v;
    if (owner == 0) return mfc_player_vslot();
    if (owner < 1 || owner >= MF_AI_MAX) return -1;
    v = (int)g_mfc_map[owner];
    if (v <= 0 || mfc_marker(v) != 1) return -1;
    return v;
}

static void mfc_send(uint16_t type, int vslot, const int *iv, const float *fv,
                     const char *text)
{
    CoopMfBridge *b;
    CoopMfEvent e;
    if (!g_mfc_sync || vslot < 0) return;
    b = mfc_bridge();
    if (!b || !b->active) return;
    memset(&e, 0, sizeof e);
    e.type = type;
    e.vslot = (int16_t)vslot;
    e.id = ((GetCurrentProcessId() & 0xFFFu) << 20) | (++g_mfc_seq & 0xFFFFFu);
    mfc_car_name(vslot, e.name, (int)sizeof e.name);
    if (iv) { e.i[0] = iv[0]; e.i[1] = iv[1]; }
    if (fv) { e.f[0] = fv[0]; e.f[1] = fv[1]; e.f[2] = fv[2]; }
    if (text) lstrcpynA(e.text, text, (int)sizeof e.text);
    if (coop_mf_push_out(b, &e)) g_mfc_stat_tx++;
}

/* ------------------------------------------------------- owner-side hooks */
static void mfc_on_burst(int id, int n, float rate, float spread, float up, int slot)
{
    int iv[2];
    float fv[3];
    int v = mfc_owned_vslot(slot);
    if (v < 0) return;
    iv[0] = id; iv[1] = n;
    fv[0] = rate; fv[1] = spread; fv[2] = up;
    mfc_send(COOP_MF_EV_BURST, v, iv, fv, NULL);
}

static void mfc_on_blow(int owner, int on)
{
    int iv[2];
    int v = mfc_owned_vslot(owner);
    if (v < 0) return;
    iv[0] = on ? 1 : 0; iv[1] = 0;
    mfc_send(COOP_MF_EV_BLOW | COOP_MF_RELIABLE, v, iv, NULL, NULL);
}

static void mfc_on_sound(int owner, int kind, const char *path, float gain)
{
    int iv[2];
    float fv[3];
    const char *f;
    int v = mfc_owned_vslot(owner);
    if (v < 0 || !path) return;
    f = strrchr(path, '\\');
    f = f ? f + 1 : path;
    iv[0] = kind; iv[1] = 0;
    fv[0] = gain; fv[1] = fv[2] = 0.0f;
    mfc_send(COOP_MF_EV_SOUND, v, iv, fv, f);
}

/* smoke banks + retirement of every car this PC owns (worker) */
static void mfc_owner_scan(void)
{
    int idx;
    DWORD now = GetTickCount();
    for (idx = 0; idx < MF_AI_MAX; idx++) {
        int v = mfc_owned_vslot(idx);
        float bk[3];
        int fin, rsn, k, changed = 0, lit = 0;
        if (v < 0) continue;
        if (idx == 0) {                       /* player: physics-object banks */
            bk[0] = (float)ENG_BANK1; bk[1] = (float)ENG_BANK2; bk[2] = (float)ENG_BANK3;
        } else {
            bk[0] = *(volatile float*)(mfc_veh(v) + AI_OFF_BANK1);
            bk[1] = *(volatile float*)(mfc_veh(v) + AI_OFF_BANK2);
            bk[2] = *(volatile float*)(mfc_veh(v) + AI_OFF_BANK3);
        }
        for (k = 0; k < 3; k++) {
            if (!(bk[k] == bk[k])) bk[k] = 0.0f;
            if (fabsf(bk[k] - g_mfc_bank_sent[v][k]) > 1e-3f) changed = 1;
            if (fabsf(bk[k]) > 1e-3f) lit = 1;
        }
        if (changed || (lit && now - g_mfc_bank_ms[v] >= MFC_BANK_KEEP_MS)) {
            mfc_send(COOP_MF_EV_BANKS, v, NULL, bk, NULL);
            for (k = 0; k < 3; k++) g_mfc_bank_sent[v][k] = bk[k];
            g_mfc_bank_ms[v] = now;
        }
        fin = *(volatile int*)(mfc_veh(v) + AI_OFF_FINISH);
        rsn = *(volatile int*)(mfc_veh(v) + AI_OFF_REASON);
        /* FinishState 2 = retired (1 = took the flag, 3 = DQ: not ours) */
        if (fin == 2 && !g_mfc_retire_sent[v]) {
            int iv[2];
            const char *nm = "";
            g_mfc_retire_sent[v] = 1;
            if (idx > 0 && g_ai[idx].onset_logged)
                nm = dmg_fault_name_(g_ai[idx].eng.fault);
            else if (idx == 0 && g_ctl && g_ctl->dead_latch)
                nm = (const char*)g_ctl->dead_fault_name;
            iv[0] = rsn; iv[1] = fin;
            mfc_send(COOP_MF_EV_RETIRE | COOP_MF_RELIABLE, v, iv, NULL, nm);
            logf_("COOP: sent retire slot %d (ReasonCode %d, FinishState %d, %s)",
                  v, rsn, fin, nm[0] ? nm : "-");
        } else if (fin == 0) {
            g_mfc_retire_sent[v] = 0;
        }
    }
}

/* ------------------------------------------------------- receiver side */
static void mfc_rq_push(int id, int n, float rate, float spread, float up, int vslot)
{
    if (!g_mfc_rq_cs_ok) {
        if (InterlockedExchange(&g_mfc_rq_cs_ok, 2) == 0) InitializeCriticalSection(&g_mfc_rq_cs);
        else while (g_mfc_rq_cs_ok != 2) Sleep(0);
    }
    EnterCriticalSection(&g_mfc_rq_cs);
    if (g_mfc_rq_n < MFC_RQ_MAX) {
        MfcBurst *q = &g_mfc_rq[g_mfc_rq_n];
        q->id = id; q->n = n; q->vslot = vslot;
        q->rate = rate; q->spread = spread; q->up = up;
        q->gen = g_session_gen;
        g_mfc_rq_n++;
    }
    LeaveCriticalSection(&g_mfc_rq_cs);
}

/* game thread (fuelcut hook): spawn the queued effects at the remote car,
 * with the same emitter shapes as the local ones */
static void mfc_rq_drain(unsigned gen)
{
    MfcBurst q[MFC_RQ_MAX];
    int k, cnt;
    if (g_mfc_rq_cs_ok != 2 || !TryEnterCriticalSection(&g_mfc_rq_cs)) return;
    cnt = (int)g_mfc_rq_n;
    memcpy(q, g_mfc_rq, sizeof(MfcBurst) * (size_t)cnt);
    g_mfc_rq_n = 0;
    LeaveCriticalSection(&g_mfc_rq_cs);
    for (k = 0; k < cnt; k++) {
        void *d;
        float pos[3], vel[3], bx, by, bz, simTime = (float)SESSION_TIME;
        int i, blue = !(q[k].id & 0x300);
        uintptr_t base = mfc_veh(q[k].vslot);
        if (q[k].gen != gen || mfc_marker(q[k].vslot) != 2) continue;
        d = blue ? find_bluesmoke_descriptor() : find_sfx_descriptor(q[k].id & 0xFF);
        if (!d) continue;
        bx = *(volatile float*)(base + AI_OFF_POS_X);
        by = *(volatile float*)(base + AI_OFF_POS_Y);
        bz = *(volatile float*)(base + AI_OFF_POS_Z);
        if (!(bx == bx) || !(by == by) || !(bz == bz)) continue;
        for (i = 0; i < q[k].n && i < 40; i++) {
            if (blue) {
                pos[0] = bx + jitterf(0.6f);
                pos[1] = by + jitterf(0.15f);
                pos[2] = bz + jitterf(0.6f);
                vel[0] = jitterf(0.02f);
                vel[1] = 0.02f + jitterf(0.02f);
                vel[2] = jitterf(0.02f);
            } else {
                float sp = q[k].spread, up = q[k].up;
                pos[0] = bx + jitterf(sp);
                pos[1] = by + 0.3f + jitterf(sp * 0.4f);
                pos[2] = bz + jitterf(sp);
                vel[0] = jitterf(up * 0.8f);
                vel[1] = up * (0.6f + (float)(rand() % 100) / 100.0f);
                vel[2] = jitterf(up * 0.8f);
            }
            ParticleTrailEmitter(d, NULL, simTime, pos, vel, q[k].rate, 0, 0, -1);
        }
    }
}

static void mfc_fire(int vslot, int on)
{
    int k, free_k = -1;
    for (k = 0; k < MFC_REMOTE_FX; k++) {
        if (g_blow_fx_start[MF_AI_MAX + k] > 0.0 && g_mfc_fire_vslot[k] == vslot) {
            if (!on) {
                g_blow_fx_start[MF_AI_MAX + k] = 0.0;
                blow_fx_update();
            }
            return;
        }
        if (free_k < 0 && !(g_blow_fx_start[MF_AI_MAX + k] > 0.0)) free_k = k;
    }
    if (on && free_k >= 0) {
        g_mfc_fire_vslot[free_k] = vslot;
        g_blow_fx_start[MF_AI_MAX + free_k] = (double)game_ms();
        blow_fx_update();
    }
}

static int mfc_seen(uint32_t id)
{
    int k;
    for (k = 0; k < MFC_DEDUP; k++) if (g_mfc_seen[k] == id) return 1;
    g_mfc_seen[g_mfc_seen_i] = id;
    g_mfc_seen_i = (g_mfc_seen_i + 1) % MFC_DEDUP;
    return 0;
}

/* 2026-10-11: other players' failures are announced on this PC's HUD (their
 * engineer radio stays theirs). CO-OP bots live in slots 32+ and, like local
 * AI, get no HUD line - only human cars (slots below 32) do. */
#define MFC_HUMAN_VSLOT(v) ((v) >= 0 && (v) < 32)

static void mfc_apply(const CoopMfEvent *e)
{
    int type = e->type & 0x7FFF;
    int v;
    if ((e->type & COOP_MF_RELIABLE) && mfc_seen(e->id)) return;
    v = mfc_find_remote(e);
    if (v < 0) return;
    if (SESSION_OVER() || !session_live()) return;
    switch (type) {
    case COOP_MF_EV_BURST:
        if (e->i[1] > 0) mfc_rq_push(e->i[0], e->i[1], e->f[0], e->f[1], e->f[2], v);
        break;
    case COOP_MF_EV_BANKS: {
        int k;
        for (k = 0; k < 3; k++)
            if (e->f[k] == e->f[k])
                *(volatile float*)(mfc_veh(v) + AI_OFF_BANK1 + 4u * (DWORD)k) = e->f[k];
        break;
    }
    case COOP_MF_EV_BLOW:
        mfc_fire(v, e->i[0]);
        if (e->i[0] && MFC_HUMAN_VSLOT(v))       /* 2026-10-11: other players see it */
            hud_show("%s: ENGINE BLOW-UP", e->name);
        logf_("COOP: %s fire %s at slot %d", e->name, e->i[0] ? "on" : "off", v);
        break;
    case COOP_MF_EV_SOUND: {
        char path[MAX_PATH], file[24];
        float x, y, z;
        int n;
        lstrcpynA(file, e->text, (int)sizeof file);
        if (!file[0] || strchr(file, '\\') || strchr(file, '/') || strstr(file, "..")) break;
        if (e->i[0] == 1) {
            game_dir(path, sizeof path);
            if (!path[0]) break;
            n = (int)strlen(path);
            _snprintf(path + n, MAX_PATH - n, "\\Audio\\Vehicle\\%s", file);
            path[MAX_PATH - 1] = '\0';
            if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) break;
        } else if (!mf_sound_path(path, file)) {
            break;
        }
        x = *(volatile float*)(mfc_veh(v) + AI_OFF_POS_X);
        y = *(volatile float*)(mfc_veh(v) + AI_OFF_POS_Y);
        z = *(volatile float*)(mfc_veh(v) + AI_OFF_POS_Z);
        if (!(x == x) || !(y == y) || !(z == z)) break;
        mf_audio_play_pos(path, e->f[0] > 0.0f && e->f[0] < 4.0f ? e->f[0] : 1.0f, x, y, z);
        break;
    }
    case COOP_MF_EV_RETIRE: {
        volatile int *fin = (volatile int*)(mfc_veh(v) + AI_OFF_FINISH);
        volatile int *rsn = (volatile int*)(mfc_veh(v) + AI_OFF_REASON);
        logf_("COOP: %s retired on its owner PC (ReasonCode %d, FinishState %d, %s) - "
              "local slot %d FinishState %d%s", e->name, e->i[0], e->i[1],
              e->text[0] ? e->text : "-", v, *fin,
              g_mfc_sync_retire && *fin == 0 ? ", marking" : "");
        if (e->i[1] == 2 && MFC_HUMAN_VSLOT(v))   /* 2026-10-11 notice (reliable: once) */
            hud_show("%s: OUT - %s", e->name, e->text[0] ? e->text : "car failure");
        if (g_mfc_sync_retire && *fin == 0 && e->i[1] == 2) {
            if (e->i[0] != 0) *rsn = e->i[0];
            *fin = e->i[1];
        }
        break;
    }
    default:
        break;
    }
}

/* ------------------------------------------------------- map + session */
static void mfc_unmap(int idx)
{
    int v = (int)g_mfc_map[idx];
    ai_table_restore(&g_ai[idx], idx);
    ai_levers_restore(&g_ai[idx], idx);
    if (v > 0 && v < MFC_VEH_SLOTS) InterlockedExchange(&g_mfc_rev[v], 0);
    InterlockedExchange(&g_mfc_map[idx], 0);
    ai_slot_reset(&g_ai[idx], idx);
    g_ai_slot_logged[idx] = 0;
}

static void mfc_session_reset(void)
{
    int i;
    if (!g_mfc_latched) return;
    for (i = 0; i < MF_AI_MAX; i++) InterlockedExchange(&g_mfc_map[i], 0);
    for (i = 0; i < MFC_VEH_SLOTS; i++) InterlockedExchange(&g_mfc_rev[i], 0);
    memset(g_mfc_bank_sent, 0, sizeof g_mfc_bank_sent);
    memset(g_mfc_bank_ms, 0, sizeof g_mfc_bank_ms);
    memset(g_mfc_retire_sent, 0, sizeof g_mfc_retire_sent);
    memset(g_mfc_fire_vslot, 0, sizeof g_mfc_fire_vslot);
    if (!mfc_active()) {
        InterlockedExchange(&g_mfc_latched, 0);
        logf_("COOP: session reset outside CO-OP - single-player addressing");
    }
}

static void mfc_map_refresh(void)
{
    int v, i;
    if (!g_mfc_latched) {
        if (!mfc_active()) return;
        /* single-player addressing -> CO-OP map: give back anything owned
         * under the identity map first (normally nothing: CO-OP bots sit in
         * slots 32+), then draw a new basis so the slots are rebuilt */
        for (i = 1; i < MF_AI_MAX; i++) {
            ai_table_restore(&g_ai[i], i);
            ai_levers_restore(&g_ai[i], i);
        }
        for (i = 0; i < MF_AI_MAX; i++) InterlockedExchange(&g_mfc_map[i], 0);
        for (i = 0; i < MFC_VEH_SLOTS; i++) InterlockedExchange(&g_mfc_rev[i], 0);
        InterlockedExchange(&g_mfc_latched, 1);
        ai_reseed();
        logf_("COOP: CO-OP session (%s) - player = vehicle slot %d, AI map on",
              mfc_is_host() ? "host" : "client", mfc_player_vslot());
    }
    /* a mapped car that left (marker -1, bot released) frees its index */
    for (i = 1; i < MF_AI_MAX; i++) {
        v = (int)g_mfc_map[i];
        if (v > 0 && mfc_marker(v) == -1) {
            logf_("COOP: AI index %d (vehicle slot %d) left - unmapped", i, v);
            mfc_unmap(i);
        }
    }
    /* every local AI car (marker 1) gets an index */
    for (v = 1; v < MFC_VEH_SLOTS; v++) {
        if (mfc_marker(v) != 1 || g_mfc_rev[v] > 0) continue;
        for (i = 1; i < MF_AI_MAX; i++) if (g_mfc_map[i] == 0) break;
        if (i >= MF_AI_MAX) {
            static unsigned full_gen = 0;
            if (full_gen != g_session_gen) {
                full_gen = g_session_gen;
                logf_("COOP: AI index table full - vehicle slot %d not modelled", v);
            }
            break;
        }
        ai_slot_reset(&g_ai[i], i);
        g_ai_slot_logged[i] = 0;
        InterlockedExchange(&g_mfc_rev[v], i);
        InterlockedExchange(&g_mfc_map[i], v);
        logf_("COOP: AI index %d = vehicle slot %d", i, v);
    }
}

/* worker, every model pump */
static void mfc_engine_pump(void)
{
    CoopMfBridge *b;
    CoopMfEvent e;
    int guard = 0;
    if (!g_mfc_latched && !mfc_active()) return;   /* single-player */
    mfc_map_refresh();
    if (!g_mfc_latched) return;
    b = mfc_bridge();
    if (!b) return;
    if (b->active) mfc_owner_scan();
    while (guard++ < COOP_MF_RING && coop_mf_pop(b->in, &b->in_r, &b->in_w, &e)) {
        g_mfc_stat_rx++;
        if (g_mfc_sync) mfc_apply(&e);
    }
    {   /* remote fire safety release */
        int k;
        double now = (double)game_ms();
        for (k = 0; k < MFC_REMOTE_FX; k++) {
            if (g_blow_fx_start[MF_AI_MAX + k] > 0.0
                && now - g_blow_fx_start[MF_AI_MAX + k] > MFC_FIRE_MAX_MS) {
                g_blow_fx_start[MF_AI_MAX + k] = 0.0;
                blow_fx_update();
            }
        }
    }
    if (GetTickCount() - g_mfc_stat_ms >= 60000u) {
        g_mfc_stat_ms = GetTickCount();
        logf_("COOP: events tx %ld rx %ld (link sent %ld recv %ld dropped %ld)",
              g_mfc_stat_tx, g_mfc_stat_rx, b->sent, b->recv, b->dropped);
    }
}
