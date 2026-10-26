# CO-OP mode (`coop_mode`)

**Latest verified release: Multiplayer Overhaul 1.0.3 (2026-10-17), shipped / observe.** Plain-language settings page: readable names and descriptions, inert `[Net]` keys removed, internal keys hidden; the DLL upgrades an older INI on first start (values kept, old file kept as `.before-1.0.3`). Game behaviour unchanged. Catalog 192657AF, ZIP E56CCBCF, DLL 58DB3FFD served and hash-verified; 10/10 test scripts pass. Not run in game. [Release record](releases/v1.0.3/RELEASE_RECORD.md) (includes the full INI audit).

**Staged 1.0.4 (2026-10-18), NOT deployed, NOT run in game:** content sync leaves out Camera Editor files (`camera_editor.profile.cfg`, `<driver>.camera_editor.cfg`, their .tmp/.bak copies): host does not list or serve them, client ignores them even from an older host. DLL 355BC295, ZIP FEC7989D, catalog 1FA92659 (base live BC522F9E); 10/10 test scripts pass; catalog VALID on Lightloader 2.4.2 and 2.3.26. Owner deploy commands in the [release record](releases/v1.0.4/RELEASE_RECORD.md) · [change record](docs/CAMERA_EDITOR_SYNC_EXCLUSION_2026-10-18.md).

**Previous: Multiplayer Overhaul 1.0.2 (2026-10-15).** Modern net mode removed (client AI lag/spikes); native netcode, players use LAN. Catalog B029A4FF, ZIP 60923C8C, DLL 446876FB served and hash-verified; 9/9 test scripts pass. Not run in game. [Release record](releases/v1.0.2/RELEASE_RECORD.md) · [Why Modern was removed](docs/MODERN_DISABLED_2026-10-15.md). Earlier status entries below are historical.

**Modern disabled 2026-10-15 (shipped in 1.0.2, not live-tested):** owner report: Modern = AI lag/spikes on clients, LAN clean. Cause: Modern sends every car at 60 Hz with no byte budget (~3-4x LAN upload), home uplink bufferbloat queues it. Not fixable small; Modern forced off (INI, host lines, menu step). DLL 89F42ABF. [Record](docs/MODERN_DISABLED_2026-10-15.md).

**Security fix (dev, NOT shipped, NOT live-tested) 2026-10-13:** F-19/F-20 host-trust fix. The client takes the manifest and MF datagrams only from the host of its own game TCP session (never from a UDP sender); the host relays MF only for game peers. Dev build hash and tests: [SECURITY_FIX_HOSTIP_2026-10-13.md](docs/SECURITY_FIX_HOSTIP_2026-10-13.md). UDP spoofing of the host IP remains open (needs M7 HMAC, a protocol change). Version not bumped (candidate label only, owner decides).

**Shipped 0.9.6 (2026-10-12):** catalog FF1305DE, DLL 938D9910; crash fixes, mid-session qualifying times, partial-grid guard, garage-box fix, BotStream off by default, crash capture. [Release record](../../lightloader/v2.0.0-hub-dev.1/release_coop_0.9.6_2026-10-12/README.md). Exact DLL not run in game; 3-player race with mid-session join pending.

Status: **shipped 0.9.3 WIP / observe** (2026-10-10): every AI car timed in qualifying, no exits after the flag, dev2-dev10 AI fixes; SlowLaps/BlueFlags/MfSync shipped OFF. [Release record](../../lightloader/v2.0.0-hub-dev.1/release_coop_0.9.3_2026-10-10/README.md). Earlier: **shipped 0.9.1 WIP / observe** (2026-10-10): client countdown sync + opt-in usage summaries (`dll/coop_usage.h`, `tests/test_usage.py`). [Release record](../../lightloader/v2.0.0-hub-dev.1/release_2.3.1_coop_2026-10-10/README.md). 0.9.1 not yet run in game (host staged). 0.9.0 record: [2.3.0 release](../../lightloader/v2.0.0-hub-dev.1/release_2.3.0_coop_2026-10-10/README.md).

**Crash fix (dev, NOT shipped) 2026-10-11:** client bot unload now moves the viewed-car pointer off the car first and follows the native unload order; bot builds run the native per-slot netcode resets; native message copy 0x4B3A90 capped at 432 bytes; crash capture (f1c_usage.h). Release candidate `1501749D` in `%TEMP%\coop_release_candidate` (current dev incl. garage-box fix; BotStream default 0; all 6 tests PASS; version still 0.9.5, bump pending). Not run in game. Record: [COOP_095_CRASH_REPORTS_2026-10-11.md](../../../operations/vps/COOP_095_CRASH_REPORTS_2026-10-11.md).

**Live 0.9.5 crash reports 2026-10-11:** mid-race client crashes (ntdll / IME / no-module AVs, MF-reported) on 2 clients of one 3-human host; Modern not established (owner: no Modern issues; hooks ABI-clean). Cause unattributed; next = crash capture (minidump + stack): [COOP_095_CRASH_REPORTS_2026-10-11.md](../../../operations/vps/COOP_095_CRASH_REPORTS_2026-10-11.md).

**Dev 2026-10-11:** 0.9.5-dev1 waypoint-tree fix live-OK (AI avoids remote human). 0.9.5-dev3 adds `[Net] Mode=Modern` (uncapped car sync, 60 Hz; dev2 crashed on join, ECX fix in dev3) — installed on host, live test pending: [NETCODE_AUDIT_2026-10-11.md](docs/NETCODE_AUDIT_2026-10-11.md) section 6.

Previous status: **implement → verify** (2026-10-09). Dev build f96f1ec1: host-alone features verified live; race-start hold and laptop join pending.

## Objective
A native main-menu mode **CO-OP** next to TEST DAY / RACE / CHAMPIONSHIP / MULTIPLAYER: friends race
together against a full AI grid in one race weekend.

## Owner decisions (2026-10-09)
- Native menu entry, not a Lightloader button.
- Friends vs full AI grid; each human picks any free car.
- v1 = single weekend; CO-OP championship later.
- Disconnect: car disappears (vanilla).
- Host sets allowed driver aids (native MP ALLOWED/DISALLOWED).
- Delivery: Claude investigates, specifies, reviews and tests; DeepSeek implements mechanical batches.

## Design
CO-OP = native multiplayer session (mode 5) + host AI injection (baseline
`f1c_ida_decompiled/mods/sdk_sandbox/multiplayer_ai_host`, 22-bot live trial 2026-09-20).
Plain MULTIPLAYER stays vanilla; the mod arms only when CO-OP was selected.
Offline-race variant rejected: `f1c_ida_decompiled/mods/sdk_sandbox/multiplayer_ai_host/COOP_OFFLINE_RACE_FEASIBILITY_2026-10-09.md`.

## Non-goals (v1)
Championship, AI takeover on disconnect, new gizmo types, exe patching on disk.

## Acceptance (v1)
1. Main menu spinner cycles ... MULTIPLAYER > CO-OP > TEST DAY; MULTIPLAYER unchanged.
2. CO-OP host gets a full AI grid; a client sees the same field.
3. Without CO-OP selected the mod is inert.
4. Mod standard R1–R11 met or exceptions recorded.

## Records (newest first)
- [1.0.3 release 2026-10-17](releases/v1.0.3/RELEASE_RECORD.md) — INI audit (used / hidden / inert keys), plain-language layout, INI upgrade (`dll/coop_ini_layout.h`), tests, deploy, pending in-game check.
- [Modern disabled 2026-10-15](docs/MODERN_DISABLED_2026-10-15.md) — why Modern lags clients, change, build hash, pending live test.
- [Latest release check 2026-10-14](docs/CHECK_LATEST_2026-10-14.md) — live 1.0.1 integrity, automated checks, stale usage test and pending gameplay validation.
- `docs/HANDOFF_CLAUDE_2026-10-13.md` — CURRENT HANDOFF: Multiplayer Overhaul 1.0.0 rename, plain-MP Modern, require-mod kick, Lightloader per-session replay naming; live tests pending.
- `docs/SECURITY_FIX_HOSTIP_2026-10-13.md` — F-19/F-20 host-trust fix (dev, not live-tested): changes with line numbers, tests (9 new, existing suites), build hash, open items O1-O9, live test steps.
- `docs/SPEC_REQUIRE_MOD_KICK_2026-10-13.md` — refuse/kick vanilla joiners: host chat warning + native booted packet after NET1 grace. Section 14 (2026-10-13): CO-OP kick ON by default (`RequireModCoop=1`), bot guard, log wording fixed; live 2-PC round 2 pending.
- `docs/SPEC_MODERN_NET_PLAIN_MP_2026-10-13.md` — **0.9.7-dev1 implemented (not live-tested):** Modern for plain multiplayer (CO-OP off) through the NET1 check on UDP 34500, Vanilla fallback for a silent or modem-link peer, `[Net] PlainMP=1` kill switch. Changes, build hash, test results, connection-type and replay findings, pending 2-PC tests are in its section 12. Pending owner decisions Q1-Q8.
- `docs/INVESTIGATION_REPLAY_RECORDING_2026-10-13.md` — replay recording in CO-OP/MP: static says all cars (incl. bots 32+, remote mode 2) recorded; live check pending.
- `docs/IMPLEMENTATION_N2_2026-10-11.md` — N2 stream, review fixes and Fix rounds 1–3; CB02, stall hold/recovery, upload-rate controls, build/test evidence and pending live validation.
- `docs/INCIDENT_HEIDFELD_TESTER_A_2026-10-11.md` — lap-0 Heidfeld/TesterA contact: not crash-related (crash = host BSOD 30 min later); AI bug unproven.
- `docs/GARAGE_BOX_TESTER_A_BURTI_2026-10-11.md` — 2002 [Bots] ini list inert under BotSource=season; garage box FIXED live 17:05 (picker detour + own-team-box rule).
- `docs/AI_JITTER_CLIENT_2026-10-11.md` — historical N1b smoothing investigation; its 0x500010/0x500560 hooks were removed in Fix round 3 after the client logs showed they did not run.
- `docs/NETCODE_OPTIONS_SYNC_AI_2026-10-11.md` — AI on clients in sync: not reliable (non-deterministic); recommended N2 own UDP bot stream with interpolation.
- `docs/HANDOFF_CLAUDE_2026-10-11e.md` — CURRENT HANDOFF: N2 bot stream live (stage 2 works), network stalls host->client, AI capture, next = no-reader race + Codex fix3.
- `docs/HANDOFF_CLAUDE_2026-10-11d.md` — previous handoff: garage box fixed, ini cleaned, client smoothing 150 ms test pending, N2 decision.
- `../../lightloader/v2.0.0-hub-dev.1/release_coop_0.9.5_2026-10-11/README.md` — **0.9.5 shipped 2026-10-11** (catalog 77670226, zip C5EAECE3, production `production/mods/coop_mode/v0.9.5`).
- `docs/HANDOFF_CLAUDE_2026-10-10.md` — cold start for the next session (current state: 0.9.0-wip candidate).
- `docs/HANDOFF_CLAUDE_2026-10-11b.md` — **cold start for the next session** (0.9.5-dev3 Modern netcode, work queue).
- `docs/NET_SETTINGS_UI_2026-10-11.md` — [Net] Mode/SendHz in Lightloader settings (dev, 176/176 self-test) + 0.9.5-dev4 in-game Modern entry on the Connection type spinner, host sends mode to clients (built, 5/5 tests, not live-tested).
- `docs/NETCODE_AUDIT_2026-10-11.md` — vanilla netcode vs modern practice: budgets, rates, dead reckoning, ranked improvements (C0/N1/N1b/N2).
- `docs/USER_GUIDE.md` — player guide.
- `releases/v0.9.0-wip/README.md` — release candidate, gates and open decisions.
- `docs/IMPLEMENTATION_LOG_2026-10-10.md` — evidence log 2026-10-10.
- `docs/HANDOFF_CLAUDE_2026-10-09b.md` — evening handoff 2026-10-09 (history).
- `docs/HANDOFF_CLAUDE_2026-10-09.md` — morning handoff (history).
- `docs/ISSUES_2026-10-09.md` — owner live-test issue register (I0–7).
- `docs/IMPLEMENTATION_LOG_2026-10-09.md` — what was built, hashes, limitations, live checklist L1–L5.
- `docs/DEEPSEEK_HANDOFF_COOP_MODE_2026-10-09.md` — DS1–DS4 work order with STOPs.
- `docs/INVESTIGATION_2026-10-09.md` — mode enum, spinner vtable 0x5A2400, .osc menu data, open items O1–O5.
- Definition draft: `f1c_ida_decompiled/mods/sdk_sandbox/multiplayer_ai_host/COOP_MODE_DEFINITION_2026-10-09.md`.

## N2 network setup

The host must allow inbound UDP 34501 for the bot stream. Clients send the initial request and need no inbound firewall rule. The development INI defaults to `BotStream=2`; verify both PCs have the intended build before a live race.

## Pending validation
- 2026-10-10 host crash 0x004038E1 on garage exit (waypoint query, RemoteCarTracking prime suspect): [docs/CRASH_4038E1_2026-10-10.md](docs/CRASH_4038E1_2026-10-10.md).
- 2026-10-10 Mechanical Failures sync (`MFEV1` on ManifestPort, `[CoOp] MfSync`, `dll/coop_mf_bridge.h`): built + offline tests only - [record](../mechanical_failures/docs/COOP_SYNC_INVESTIGATION_AND_SPEC_2026-10-10.md).
Live checks L1–L5 in the implementation log.
- [AI fixes plan 2026-10-10](docs/PLAN_AI_FIXES_2026-10-10.md) — pit-exit scheduler + remote speed; test plan
- [Edit starting grid with AI — investigation 2026-10-10](docs/INVESTIGATION_EDIT_GRID_AI_2026-10-10.md)

**2026-10-12 friend report:** [BR-20261012-594CE0](../../../operations/vps/BR_20261012_594CE0_INVESTIGATION.md): exact 0.9.6 installed, invalid name-pointer read during vehicle setup with CO-OP hook on raw stack; origin unproven. Friend retained BotStream=2; off/disabled comparison and dump analysis pending.
