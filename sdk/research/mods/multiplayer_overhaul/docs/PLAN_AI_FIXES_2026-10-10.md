# CO-OP AI fixes plan — 2026-10-10 (define/specify; nothing implemented)

Evidence: IMPLEMENTATION_LOG_2026-10-10.md (checks 09:11-10:45 UTC), finding ai-garage-exit-scheduler-slot-indexed-20261010.

## Fix A — bots leave the garage (practice/qualifying/warmup)
Cause (DERIVED): pit-exit scheduler sub_4A4110 scans its slot-indexed "cars out" array 0x7CE2F0 only for i < active count; bots are slots 32-54.
Change: host-only detour 0x4A4110 when CO-OP selected: copy array, pack non-null entries 0..103 into 0..n-1, keep [104] (next-exit time), call original on the copy, write back [104] and clear entries the original cleared. Offline/plain MP untouched.
Accept: log line per bot exit; host 60-min practice -> first bot out within ~5 min, >=12 bots out by mid-session; 20-min quali -> every bot with runs left goes out before the 75% rush; no two exits < 18 s apart; no pit-exit contact.

## Fix B — AI treats remote human as stationary (race)
Cause (DERIVED, live speed=0 FACT): host copy of a remote car has veh+0x4440 Speed = 0 while moving; AI reads it (AISteering 0x4270A0).
Step 0 (read-only): probe velocity vector of the remote copy; locate where net apply 0x4C73F0 writes velocity.
Change: host (and client for bots) each frame after native net apply: Speed = |velocity| (or position delta / dt) for mode-2 cars. Optional later: extrapolate by measured latency for AI view; collision channel.
Accept: probe shows remote speed within 5% of position-derived speed; AI nearest-table still lists the human; owner+Tester A overtake test: AI holds line / brakes, no turn-in contact in 5 attempts.

## Test plan (one session, owner + Tester A)
1. Build both fixes behind INI toggles ([CoOp] PitExitFix=1, RemoteSpeedFix=1); unit tests; record hashes.
2. Host alone, Practice 20 min, PitExitFix=0 then 1: count bots out at 5/10/15 min (memory-mcp drivers). Baseline vs fixed.
3. With Tester A: Practice 10 min (fix A on), Qualifying 20 min: all bots run, no garage overlap issues.
4. Race: ai_notice_probe + speed probe running; Tester A overtakes and follows bots for 5 laps; report contacts. Then RemoteSpeedFix=0 for 2 laps as control.
5. Offline quick race sanity (mod inert outside CO-OP).
Pass = all accept lines met; failures logged in IMPLEMENTATION_LOG with probe excerpt.

## Status 2026-10-10: A + B implemented in 0.9.3-dev (dll b13f7247), live test pending — see IMPLEMENTATION_LOG_2026-10-10.md.
