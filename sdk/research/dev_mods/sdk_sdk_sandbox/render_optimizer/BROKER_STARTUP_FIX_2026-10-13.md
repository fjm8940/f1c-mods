# Vertex trial broker startup correction — 2026-10-13

Owner reported enabled game running. Read-only observation at 09:57:52.811305 UTC (12:, PID 18008, canonical Online EXE hash, sim_time 18.27415657, game_state 2/session_type 1/race_phase 0 and null player pointer: current sample was not an in-car race. The enabled prototype log reported REFUSED: compatible shared broker unavailable, zero counters. Consequently this attempted ON session did not exercise optimization and cannot support any FPS comparison.

Proven source defect: static F1CBrokerApi broker was zero-initialized and vertex_plugin.cpp never set structure_size before f1c_broker_discover. Canonical discovery rejects capacity below its API prefix as BAD_ARGUMENT before checking modules. This was a prototype initialization bug, not proof that the installed broker was absent or incompatible. Fixed through discover_broker(), which sets sizeof(broker) before discovery. Native packing code and guards unchanged.

New vertex_startup_test.cpp includes the actual plugin source and invokes the actual discovery wrapper against the real discovery implementation. It verifies a correctly sized table and NOT_FOUND in its broker-free process, rather than BAD_ARGUMENT, with no hook installed. Build runs this regression before the existing 1,600 captured-native differential cases, 2,000 kernel cases, test GPU D3D8 buffers/draw/present/release and DLL refusal smoke. All passed. Full canonical sdk-test passed; seven regenerated files unchanged. A successful real broker handshake/active hook is still a separate native acceptance gate.

Corrected private 0.2.0-dev.1 artifact SHA256: D8FD10737BE3456A40D5685283D92BDCB0D6BDA2E357772E11BC41B0DB970BA5. Previous installed artifact 83223FDF4C1A42B5B3BB58F246F1CA487C058FFD0C11C9F0A60D759F2C02D286 preserved in TEMP/f1c-render-vertex-0.2.0-dev1/installed-before-broker-fix-20261013.dll. Revision is distinguished by exact hash; no public release/version promotion.

After observing no F1 processes running, canonical stage --replace-sdk-owned replaced only render_vertex_optimizer.dll and its owned render_optimizer.ini/markers. Installed exact corrected hash verified; Enabled=1. Original configuration and ownership map plus source/documents before-state retained under workspace_organization_audit/VERTEX_BROKER_FIX_BEFORE_2026-10-13. No hot injection, process termination, EXE/broker/loader/other-mod changes. No fresh game launch after replacement; current log still refers to the failed old build until next startup. Restoration is Enabled=0 and full restart; no binary rollback to the defective candidate needed.

Next owner run: guarded full-weekend scenario, never QuickRace. Confirm new startup ACTIVE and increasing fused_calls; then gather matched OFF/ON FPS/frame-time/visual/stability evidence. Merely selecting Enabled=1 is insufficient. No FPS gain claimed.

Documentation gate: record linked in README/current state; exact source/installed hash and pending activation in verification manifest/checkpoint/findings. Root validator retains pre-existing .claude/plans errors, zero warnings. Wasted-effort check: repeated large reads none material; broad tests only required full SDK; failed commands included an rg invocation with a PowerShell-only flag and guessed broker source paths, corrected using rg --files; waiting bounded to SDK completion. Earlier smoke tests covered host refusal but omitted actual discovery-wrapper initialization; the new regression closes that gap.


## Handoff resumption check — 2026-10-13

Objective: resume the supplied TEMP performance handoff by checking whether the corrected private trial has a fresh native activation result. Canonical product is Render Optimizer under the SDK sandbox; pipeline remains verify / pending human validation. Documentation destination is this existing README-indexed broker correction record. SDK startup summary is stale; its newer render-optimizer checkpoint entries and this component state take precedence over unrelated telemetry objectives.

FACT (read-only host/file observation): no process named F1 was returned. Installed DLL SHA256 remains D8FD10737BE3456A40D5685283D92BDCB0D6BDA2E357772E11BC41B0DB970BA5; INI Enabled=1. The 209-byte log last-write time is 2026-10-13 and still reports broker refusal and zero calls/fused calls. This is the historical failed-start log, not evidence that the corrected DLL failed. No fresh activation, gameplay, FPS or frame-time evidence was obtained. Process absence is a point-in-time observation, not proof that the owner never launched between checks.

No game, configuration, binary, SDK source, checkpoint or product-stage change. No rebuild or broad SDK test justified for this read-only check. Pending: owner starts the guarded full-weekend game with the corrected installed candidate; capture new startup identity, ACTIVE and increasing fused_calls before matched OFF/ON/OFF measurement. No public promotion authorized. Existing rollback remains Enabled=0 with game closed and full restart.

Preservation: complete prior report retained at workspace_organization_audit/VERTEX_RESUME_BEFORE_2026-10-13/f1c_ida_decompiled/mods/sdk_sandbox/render_optimizer/BROKER_STARTUP_FIX_2026-10-13.md. Findings appended for discoverability. Root validator before check: two existing unexpected directories (.claude, plans), zero warnings; neither changed. Documentation gate: this record is already linked from component README; facts and pending native validation distinguished.

Wasted-effort check: initial combined root-document output was truncated, requiring a bounded reread of governance README/PIPELINE; no broad builds/tests, failed/retried commands or polling waits.


## Fresh launch observation — 2026-10-13

FACT: Win32_Process identifies PID 7496, F1 Challenge Online.exe, created 2026-10-13 (11:57:38 UTC). Fresh optimizer log has matching 11:57:38 UTC last-write time and reports WAITING: native hook queued for broker game-thread dispatch; calls=0, fused_calls=0, fused_vertices=0, thread_bypass=0. Two bounded log reads returned the same status. The earlier Get-Process -Name F1 check missed the actual executable process name; it must not be used to infer game absence for this installation.

Interpretation: corrected prototype initialization progressed past the earlier broker refusal and queued installation. ACTIVE/hook installation and optimized work are not yet established. Pending: enter the guarded full-weekend rendering scenario and recheck ACTIVE/increasing fused_calls; if WAITING persists there, investigate dispatch. No game/configuration/binary changes, memory writes or build/tests. Existing README links this record; findings appended. Complete prior report preserved under workspace_organization_audit/VERTEX_WAITING_BEFORE_2026-10-13_1158 with its repository-relative path. Wasted-effort: exact-name process query missed Online executable; corrected with bounded CIM name search. No broad tests, large repeated reads or polling waits.


## Native hook active, no fused work — 2026-10-13

Owner reports on track. FACT: optimizer log reports ACTIVE: native post-lock vertex hook installed. Two log samples with last-write times 11:59:15 and 11:59:21 UTC show calls increasing from 8169 to 9152, while fused_calls=0, fused_vertices=0 and thread_bypass=0 in both. This establishes installed hook execution in this launch, closing the previous queued-installation observation; it does not establish eligible optimized work or any FPS improvement. Process identity was not returned by the bounded CIM query during this check; no stronger process-lifetime attribution claimed.

Pending: investigate why observed groups fall back / do not satisfy eligibility, then demonstrate increasing fused_calls before performance A/B. No game, configuration, binary or source changes; no memory writes, builds or tests. Parent SDK stage unchanged. Before-state preserved under workspace_organization_audit/VERTEX_ACTIVE_BEFORE_2026-10-13_1159 with repository-relative path. Record remains indexed by component README; tiered finding appended; UTF-8 reread performed. Wasted-effort check: none observed (two bounded samples establish increasing calls).
