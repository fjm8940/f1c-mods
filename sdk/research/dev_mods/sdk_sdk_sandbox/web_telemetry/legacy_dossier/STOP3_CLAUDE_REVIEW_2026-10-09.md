# STOP 3 review (Claude, 2026-10-09)

Verdict: NOT ACCEPTED - research strong, workbench design incomplete. Revise sections 4-7 before STOP 4.

Verified (fact): format tests pass (56/56 claimed; suite passes); spt writer chain and growth check (2,312 vs 2,315 B/s) internally consistent; RPM cited not re-derived. Good.

Required:
1. Section 6 designs only legacy import. The owner's core requirement (assignment "Modern integrated product behavior") is LIVE graphs for player AND AI. Design must cover: A/B car selection (player or any AI) with colors/labels; default Chassis Overview (track/driving line, speed vs distance, G-G scatter, RPM vs distance); Inputs view (throttle, brake, gear, steering only where proven); live partial-lap drawing as cars move; LIVE / partial / completed / frozen-reference states; freeze/resume without stopping capture; lap crossing keeps history and reference; mid-lap selection; linked cursor across charts + track; zoom/pan/reset; missing-data reasons. Map each to existing endpoints/UI (Engineering tab, /trace/*, /track, SSE) and name what is new.
2. Ledger error: positions are exposed natively for ALL cars (f1c-memory f1c_drivers returns world_x/world_z per slot, live 2026-10-09; web_telemetry already serves /track). Correct "not exposed" for position x/z (player + AI), cite source offset, keep y/yaw status explicit.
3. Budgets cover only import decode. Add live budgets: which cars are captured (2 selected vs all 22), capture Hz vs transmit Hz vs redraw Hz, per-car ring size and history laps, memory per car/lap, SSE payload bytes/s, client cache bounds, behaviour on pause/reconnect (no fake samples, no lines across gaps). Measured where possible, otherwise labeled design budget.
4. Port design (section 7) conflicts with parallel task A (DEEPSEEK_PARALLEL_A_PORT_2026-10-09.md: ini `Port=`, fallback 8081-8085, /health port). Port work belongs to task A; drop it from STOP 4 scope and reference task A instead. Do not edit web_telemetry.c until told task A has landed.
5. Player steering candidate P+0x000/P+0x040: keep OPEN; list the exact live read-only check needed (next owner race).

## STOP 3R re-review (Claude, 2026-10-09)

Verdict: ACCEPTED. Sections 6.1-6.6 cover every required live item, mapped to existing surfaces; ledger positions corrected; live budgets given. STOP 4 conditions:
- Existing live RPM field `c.r` is rad/s: convert x 9.549296585513721 before any "RPM" label (finding engine-rpm-live-scale-20261009); test it.
- SSE ~310 KB/s per client at 22 cars: acceptable on LAN; record the Tailscale/remote cost and keep A/B trace payloads bounded.
- Do not edit web_telemetry.c until parallel task A has landed and been reviewed.

## Parallel B review (Claude, 2026-10-09)

Verdict: ACCEPTED. Verified: recipe EngineAngularSpeed float32 0x39A0 present; test_runtime_validator.py passes. Rev-limit candidate veh+0x39A4 EngineMaxRPM (rad/s) noted; per-car value plus player limiter band still to be cross-checked live (with steering check, next race).
