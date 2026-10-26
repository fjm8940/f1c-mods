# Mechanical Failures — orientation brief for an LLM (2026-09-26)

**Purpose:** tell you where everything is. This document is a map, not a task
list and not release authorization. Matias works with you step by step; wait for
his instruction on what to do next. Paths are relative to
`%USERPROFILE%\Desktop\F1` unless stated. Every path below was checked to
exist on 2026-09-26; statements marked *(unverified)* were not.

Goal context: Matias wants to ship the Mechanical Failures mod (player gearbox,
engine incl. oil leak/cutout, suspension failures) the way Pit Stop Failures
1.0.3 was shipped. A fuller phased draft plan exists:
[SHIP_PLAN_1_0_0_DRAFT_2026-09-26.md](SHIP_PLAN_1_0_0_DRAFT_2026-09-26.md) —
treat it as reference only; Matias narrowed scope to step-by-step work.

---

## 1. Rules to read before touching anything

| File | Why |
|---|---|
| `AGENTS.md` | Root rules: never delete, preserve before-states, pipeline, mandatory documentation, secrets |
| `governance\README.md`, `governance\PIPELINE.md` | Authority order, stage gates, wasted-effort check |
| `governance\PRODUCT_STATUS.json` | Product stages. **Mechanical Failures has no entry yet.** Pit is `pitstop-modding` |
| `development\AGENTS.md` | Development rules + **owner startup contract (2026-09-24)**: mods must verify Lightloader host protocol and required game code/layout before activating; no whole-EXE hash gate |
| `production\AGENTS.md` | Production is immutable; only exact verified hash is copied in |
| `f1c_ida_decompiled\AGENTS.md` | SDK/runtime rules |
| `operations\release_management\SHIPPING_PIPELINE.md` | Release gates for direct Lightloader plugins |
| `operations\vps\VPS_DEPLOYMENT.md` | Remote/VPS procedure. Outward-facing — only on Matias's explicit go |

Never read `api1.txt` or private keys. Workspace validator:
`powershell -NoProfile -ExecutionPolicy Bypass -File tools\workspace\check-workspace-structure.ps1`
(baseline 2026-09-26: 11 errors, 0 warnings — all pre-existing, unrelated).

---

## 2. The mod itself

### Development directory (canonical)
`development\mods\mechanical_failures\`

| Path | What |
|---|---|
| `README.md` | Main technical doc for all three failure families (mechanics, addresses, live proofs, traps, "Not done yet") |
| `dll\gear_failure.c` / `.dll` | Gearbox: stuck / lost gears / neutral. Detour on player SetGear `0x447E00` |
| `dll\engine_failure.c` / `.dll` | Engine: staged wear, stress, seizure, smoke/fire, cutout (`C`), oil leak (`O`), retire commentary. **`.c` modified Aug 8, `.dll` built Aug 7 — binary may not match source** |
| `dll\suspension_failure.c` / `.dll` | Suspension original (pre-retune, Aug 8 13:21) — **stale** |
| `dll\suspension_failure_v2..v4.dll` | Intermediate builds (no source) |
| `dll\suspension_failure_v5.c` / `.dll` | Newest suspension in this tree (Aug 14, auto-reset) |
| `dll\bluesmoke.c` / `.dll` | Standalone blue-smoke experiment; oil leak duplicates its emitter path |
| `dll\mintest_here.dll`, `mt_mech.dll` | Test/experimental binaries — not product |
| `dll\gear_failure_log.txt`, `dll\inject_out.txt` | Old run logs |
| `mechanical_failures.py` | **Deprecated** poll-based script, kept as record of disproved approach |
| `mechanical_failure_gui.bat` | Launches the manual trial GUI (elevates) |
| `workspace_organization_audit\` | Before-state copies (preserve here before editing README etc.) |

Current state: all triggering is **manual** (GUI shared-memory blocks
`F1C_GEARFAIL_CTL` / `F1C_ENGFAIL_CTL` / `F1C_SUSFAIL_CTL`, or numpad hotkeys).
Loaded by manual injection with admin, **not** through Lightloader. No host
proof, no layout gate, no INI, no installer, no tests. Player-only by
construction. `Num8` collides between engine and suspension DLLs.

### Evidence and docs (live in the IDA corpus, not the dev dir)
The README's relative links (`../../docs/...`, `findings.jsonl`, `tools/...`)
resolve against the legacy tree, not the dev dir:

| Path | What |
|---|---|
| `f1c_ida_decompiled\mods\extended_vanilla_features\docs\ENGINE_FAILURE.md` | Full engine map |
| `f1c_ida_decompiled\mods\extended_vanilla_features\docs\SUSPENSION_DAMAGE.md` | Suspension map (read-vs-write warning) |
| `f1c_ida_decompiled\mods\extended_vanilla_features\tools\mechanical_failure_gui.py` | Trial GUI (Gearbox/Engine/Suspension tabs) |
| `f1c_ida_decompiled\mods\extended_vanilla_features\data\progress.json` | Clip progress (11 of 14 gearbox clips WIP / not ear-checked) |
| `f1c_ida_decompiled\findings.jsonl` | Corpus findings (gearbox/engine/ignition/bluesmoke/suspension entries; ids quoted in README, e.g. `pws-mechanwarn0-eid130-confirmed-not-131`) |
| `f1c_ida_decompiled\docs\IGNITION_INVESTIGATION.md` | Superseded; ignition = `0x7C9081` |
| `f1c_ida_decompiled\docs\46-player-specific-mechanical-failures.md` | **Known wrong** on four points (bad offset base) — do not trust |
| `f1c_ida_decompiled\functions\PositionLapStateUpdate_00447e00.c` | Player SetGear (gearbox hook site) |
| `f1c_ida_decompiled\functions\VehicleMainPhysicsUpdate_004441f0.c` | Player per-frame physics (suspension hook site) |
| `f1c_ida_decompiled\functions\EngineStressSimulation_0043ef20.c` | Player engine life/stress |
| `f1c_ida_decompiled\functions\SteeringGeometryUpdate_00442060.c` | Game's own bend input (suspension calls this, game thread only) |
| `f1c_ida_decompiled\functions\FuelConsumptionAndWear_004ed960.c` | Vanilla AI-only failure system |

### Other copies (identical source/hashes — do not develop there)
- `f1c_ida_decompiled\mods\extended_vanilla_features\patches\mechanical_failures\` (legacy origin)
- `archive\workspaces\evf_shipping_hub\mods\mechanical_failures\` (archive — never develop)

Both have the same MD5 for every `.c` and for `engine_failure.dll` as the dev dir (checked 2026-09-26). Other DLLs not compared.

### SDK port of suspension (newer than dev dir)
`f1c_ida_decompiled\mods\sdk_sandbox\suspension_failure\` — `suspension_failure.c`
(Aug 14 19:34), `f1c_mod.json`, `build.bat`, `build\suspension_failure.dll`,
README. Adds auto-reset, removes HUD text. Its README calls it the canonical
suspension source going forward. Built with
`python tools/f1c_mod.py build mods/sdk_sandbox/suspension_failure` (from
`f1c_ida_decompiled`). Gearbox and engine have **no** SDK port found *(only a
filename search was done)*.

---

## 3. The reference: Pit Stop Failures 1.0.3 (shipped 2026-09-24)

Copy this pattern. Do not edit these files.

| Path | What |
|---|---|
| `development\mods\pitstop_modding\README.md`, `STATUS.md` | Component status and history |
| `development\mods\pitstop_modding\release_candidates\pitstop_failures-v1.0.3-rc.1\` | **Exact shipped candidate — the template** |
| …`\SPECIFICATION_1_0_3.md`, `REPORT.md`, `VERIFICATION_1_0_3.json` | Spec / report / verification format |
| …`\f1c_mod.json`, `build.bat` | SDK project + build (gcc/g++ mingw32, `-Werror`, stripped, static) |
| …`\source\pit_loader_host.h`, `protected_plugin_abi.h` | **Lightloader v2 host validation** |
| …`\source\live_client.cpp` (~line 1282–1413) | Inert `DllMain`, startup/initialized log markers, exported host entry `rfe_1d3b7a91(const LightLoaderProtectedHostV2*)`, `F1CModSdkVersion` |
| …`\source\live_client.cpp` ~line 399 | Pit also toggles ignition byte `0x7C9081` (engine stall) — conflict surface with mechanical engine module |
| …`\source\pit_compat.h`, `pit_compat_profile.h`, `compat_helper.cpp`, `compat_probe.cpp`, `compatibility-profile.json`, `verify_compatibility.py` | **Code/layout compatibility gate** (same checker at install and runtime) |
| …`\source\host_smoke.cpp`, `live_tests.cpp` | Host and runtime tests |
| …`\PitStopFailures.ini`, `source\pit_player_settings.h` | Player INI format + bounded parsing (missing = defaults, invalid = refuse + log) |
| …`\installer\InstallEngine.cs`, `SetupUI.cs`, `Version.cs` | C# installer (upgrade handling, settings preservation, backups) |
| …`\payload\` | Installed tree: `dinput8.dll` (Lightloader), `plugins\lightloader\` (mod DLL, `.mod.json`, broker, INI), audio |
| …`\manifest.json`, `assemble.py`, `verify_candidate.py`, `verify_upgrades.py` | Packaging manifest (SHA-256 per file) and assembly/verification scripts |
| …`\verification\` | `HOST_RESULTS.json`, `ACTUAL_INSTALLS.json`, logs, `binding\`, `scan\` |
| …`\assembly-before\` | Before-state of assembly |
| `development\mods\pitstop_modding\docs\STARTUP_CONTRACT_AUDIT_2026-09-24.md` | Why host proof ≠ finding broker/dinput8 |

Operations / production for Pit:

| Path | What |
|---|---|
| `operations\release_management\pitstop_failures\RELEASE_1_0_3.md` | Release record format |
| `…\PROMOTION_1_0_3.json` | Promotion record (authorization, hashes, verification, pending) |
| `…\DEPLOYMENT_1_0_3.txt`, `deploy_103_20260924.py`, `prepare_nginx.py`, `pitstop-failures-short-link.conf` | VPS deployment scripts/records |
| `…\public-verification-1.0.3\` | Public download verification |
| `production\mods\pitstop_failures\v1.0.3\` | Frozen artifacts: setup EXE, wrapper ZIP, README.txt |
| `production\RELEASE_REGISTRY.md` | Public release index |

Shared templates / infrastructure:

| Path | What |
|---|---|
| `operations\release_management\templates\DIRECT_PLUGIN_RELEASE_TEMPLATE.md`, `RELEASE_RECORD_TEMPLATE.md` | Release templates |
| `development\bug_report_site\config\public-downloads.json` | Bug-report download selector (add on promotion) |
| `production\lightloader\` (`v1.1.0` current public) | Lightloader releases |
| `development\lightloader\v1.1.0-protected-plugins-rc.1\README.md` | Protected plugin host protocol docs |

---

## 4. SDK

| Path | What |
|---|---|
| `f1c_ida_decompiled\tools\f1c_mod.py` | SDK front door. Start with `python tools/f1c_mod.py start --json` from `f1c_ida_decompiled`; use `route`, `search`, `inspect`, `build`, test commands |
| `f1c_ida_decompiled\tools\sdk\README.md`, `NEW_MOD_GUIDE.md`, `LLM_OPERATING_CONTRACT.md` | How to create an SDK mod |
| `f1c_ida_decompiled\tools\sdk\f1c_hook.h` | Detour helper (5-byte only; suspension site needs 6-byte push/ret — see sandbox port) |
| `f1c_ida_decompiled\tools\sdk\f1c_broker.c/.h`, `f1c_broker_abi.h` | Shared game-update broker |
| `f1c_ida_decompiled\tools\sdk\f1c_pit_voice.h`, `PIT_VOICE_REVIEW.md` | Commentary queue injection pattern |
| `f1c_ida_decompiled\tools\sdk\F1C_MEMORY_MCP.md` | Live memory diagnostics (read-only unless authorized) |

---

## 5. Key technical facts (from the mod README; verify before relying)

- Gearbox hook `0x447E00` (5-byte jmp, prologue `8B 44 24 04 56` checked). Player object `0x7C7428`; gear fields `0x7C9318/1C/20`. Filter branches must gate on `engaged`, not `enabled`. Stuck may only refuse a change.
- Engine: life `0x7C91DC` (double), stress `0x7C91D4` (double, = power loss, cap 0.6), smoke banks `0x7C920C/14/1C` (healthy = `0.0`), ignition `0x7C9081`.
- Suspension: detour `0x4441F0` (6-byte push/ret, 12 stolen bytes), call `SteeringGeometryUpdate(0x7C7428, wheel, sev)` **on game thread only** (worker-thread call crashed 2026-07-30). Never write bend fields directly.
- HUD strip: `*(u32*)0x7CD204`, 8 slots stride 0x5C (shared with Red Flag).
- Commentary: player gearbox eids 80–93 (cooldown unblock, 14 sites), suspension eid 75, oil warn eid 130, retire eid 119. **Traps:** eids 201–217 are AI third-person lines; 340–383 are TV channel.
- Vanilla `.PLR` mechanical failure rate is AI-only.

## 6. Unverified / open items worth checking early

- Does `engine_failure.dll` match `engine_failure.c`?
- Dev `suspension_failure_v5.c` vs sandbox `suspension_failure.c` differences.
- Hook overlap with `development\mods\eighth_gear\` (gear-related) — not checked.
- Ignition-byte ownership vs Pit 1.0.3.
- Owner decisions not yet made: one merged DLL vs three, automatic trigger model, session scope, installer vs ZIP, free/paid, protection, INI settings, required co-mods.

## 7. Where to record your work

- Component README status line at top of `development\mods\mechanical_failures\README.md` (preserve before-state first under `workspace_organization_audit\<PHASE>_BEFORE_<date>\`).
- New dated docs under `development\mods\mechanical_failures\docs\`.
- Release candidates under `development\mods\mechanical_failures\release_candidates\mechanical_failures-v<ver>-rc.<n>\` (mirror Pit layout).
- Operations records later under `operations\release_management\mechanical_failures\`.
