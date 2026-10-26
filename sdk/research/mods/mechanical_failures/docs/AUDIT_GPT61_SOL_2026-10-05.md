# Independent audit: mechanical_failures engine family, 2026-10-05

## 1. Verdict

**No: repair the hook installation/removal paths before continuing live injection/unload testing.**
The new hooks have reachable crash windows and can leave a jump into an unloaded DLL; the voice donors also collide with Pit Stop Failures.
All six baseline gates pass, but none exercises those runtime paths. No crash or gameplay acceptance was simulated.

## 2. Findings

Paths below are relative to `development/mods/mechanical_failures`, except explicitly named sibling/corpus paths. FACT means directly read code/static bytes; DERIVED means a failure follows from that code under the stated scenario; INFERENCE would require further evidence. Failure scenarios below are DERIVED, not claims that a crash was observed.

| id | severity | area | file:line | what is wrong | concrete failure scenario | suggested fix | tier |
|---|---|---|---|---|---|---|---|
| A01 | CRITICAL | A | `dll/engine_failure.c:8978-8983`, `:8817-8819`, `:9957-9958` | Hooks are patched from the worker with several unsynchronised instruction-byte stores. Lamp trampoline publication (`*tramp_out = tramp`) occurs **after** the E9 becomes reachable. | The game enters a lamp hook between E9 publication and trampoline publication and calls NULL; alternatively it executes a partly rewritten original prologue. Putting E9 last does not protect execution of the original while its operands are being overwritten. | Publish fully initialised state first and install/remove through a reviewed game-thread safe point or thread/IP-coordinated patch transaction. | DERIVED |
| A02 | CRITICAL | A | `dll/engine_failure.c:8943-8945`, `:8953-8955`, `:8827-8842`, `:8998-9011`, `:10242-10254`; `dll/mechanical_failures.c:107-118` | Removal blindly restores bytes, does not verify success/ownership, and ignores a nonzero counter after 200 ms. Lamp counters are decremented **before** the remaining DLL code executes. These removers never request the existing pin-on-failure mechanism. | VirtualProtect fails, so an E9 remains; the last family calls FreeLibraryAndExitThread. A later game call jumps into unmapped DLL code. A hook can also still be in its lamp tail after its counter reaches zero. Another mod's later detour is overwritten on removal. Retaining the trampoline cannot retain the DLL return address/body. | Cover the entire hook lifetime, prove quiescence and exact owned-byte restoration, and call `mf_exit_pin_required()` on any unresolved restore/drain failure. | DERIVED |
| A03 | HIGH | A, D | `dll/mf_voice.h:79-87`, `:124-128`; sibling `development/mods/pitstop_modding/live/pit_live_voice.h:12-22,43-47`, `live_client.cpp:578-579,1253-1256` | Six supposedly unused donor events are used by Pit Stop Failures: **50, 57, 62, 64, 67, 69**. Their global lookup entries are replaced irrespective of which mod queues the event. | Pit fuel recovery queues 57 and says "You've got a fuel leak." A successful front wheel recovery queues 62/64 and says the engine has gone/failed on a healthy engine. The current Pit startup-layout clash may prevent coexistence today; fixing that clash does not fix these collisions. | Replace these donors using a complete active-mod event inventory and reserve their score mappings centrally; add cross-mod collision tests. | DERIVED |
| A04 | HIGH | A | `dll/engine_failure.c:1559,1572-1576`; `dll/mf_voice.h:213-225` | Restore indexes the lookup using **current** `cur[i].score`, without validating it or comparing it to the saved score. The row helper does not include score in its ownership check. | Another owner changes a donor score: unload checks/restores the wrong lookup slot and leaves the originally patched slot behind. A negative/out-of-range score becomes an out-of-bounds read/write. The code writes `v` even on a restore-skip result. | Address the saved original score, validate bounds and ownership again, and skip every write whose ownership cannot be proven. | DERIVED |
| C01 | MEDIUM | B, C | `dll/mf_engine_model.h:2250-2253,2360-2386,2755-2760`; `dll/engine_failure.c:5098-5103` | A cooling fault freezes an incomplete baseline; with no baseline, `mf_water_base` returns the current temperature forever. The separate WATER02 voice condition bypasses the model's absolute temperature floor. | An arm before learning produces no temperature warning even at 125 C. An arm after learning 60 C during warm-up warns at a healthy 96 C. With a 60 C baseline, held water at 80 C satisfies the raw `> base+18` voice condition and can schedule "Water temp high, lift and coast" although the 95 C model instruction guard is false. | Track baseline validity/stability explicitly, handle cold arms with a tested fallback, and derive voice eligibility from the same guarded model output as HUD. | DERIVED; first two cases reproduced offline |
| B01 | MEDIUM | B, D | `dll/engine_failure.c:5046-5052,5067-5085`; `:1598-1604` | Both debris and leak call WATER01. Debris RAD01 is considered later and only if the single global pre-critical slot is unused. | Above the warning threshold, WATER TEMP holds 1-3 s then WATER01 holds 3-8 s: total 4-11 s. RAD01 requires 8-15 s plus 3 s: total 11-18 s. WATER01 can spend the slot before RAD01, so the documented debris diagnosis and its RAD02 instruction never occur. | Select the pre-critical diagnosis by fault kind before consuming its budget; test competing held deadlines. | DERIVED |
| D01 | MEDIUM | B, D | `dll/engine_failure.c:4981-5007,1648-1663`; `dll/mf_msg.h:250-266` | Timed instruction slots survive spontaneous/pit debris clearance and stabilisation. `voice_pump` checks terminal state but does not require the original fault/condition to remain active. Slots begin at request time, not confirmed playback. | WATER01/RAD01 schedules cooling advice; debris clears and RAD03 reports it clear; 30-60 s later the old advice still fires. Likewise "Problem's cleared, push now" can be followed by old restrictive advice. A dropped/delayed diagnosis still schedules its instruction. | Bind slots to a fault generation and current symptom, cancel on resolution, and start timing from successful playback/acceptance. | DERIVED |
| D02 | MEDIUM | D | `dll/engine_failure.c:1353-1367,1653-1655,472-473,515-550`; `dll/mf_msg.h:199-224` | The death fix blocks new requests and clears timed slots, but does not purge **already queued** non-death radio items at terminal transition. `radio_pump` passes only `wrecked_or_stopped()`. Death requests arrive 3-5 wall-clock seconds later. | The engine dies while coasting at speed with FinishState 0. Quiet remains false; a pre-existing instruction whose radio gap has elapsed can play before the delayed death request clears it. Direct queue reproduction returned eid 25 under this condition. | Treat terminal/dead-latch state as quiet at dispatch and purge non-death items immediately on death. | DERIVED; queue behaviour reproduced offline |
| F01 | HIGH | F | `tools/sdt_restore.py:65-87`; `tools/voice_uninstall.bat:6` | Restore authenticates the vanilla backup but overwrites **any** non-vanilla target without preserving it or checking game closure. The batch supplies `--yes` automatically. The overwrite is an ordinary copy, not an atomic replacement. | A different mod's appended SDT is erased on uninstall; the known vanilla backup does not recover that target. An interrupted copy can leave a truncated live SDT. Running while the DLL still maps new clip indices leaves runtime references to removed clips. | Refuse unknown target hashes, preserve the exact target before replacement, require closed game, and replace atomically with verified restoration metadata. | DERIVED |
| F02 | MEDIUM | F | `tools/voice_build.py:246-247,315-329,332-339` | `--verify` still creates directories, replaces changed source copies, and rewrites MANIFEST.csv; failures are returned **after** a partial manifest is written. Normal build overwrites outputs without before-state preservation. | A missing/bad WAV causes a failed verification to replace the 35-row manifest with only successful rows; changed originals can overwrite the previously retained mod source copies. Desktop originals are read-only, but project evidence is not. | Make verification strictly read-only; stage and validate all outputs before a preservation-safe committed build. | DERIVED |
| E01 | MEDIUM | E | `tools/engine_model_test.c:5245-5258,5537-5582,5604-5613`; `tools/voice_test.c:35-58,99-115`; `tools/lamps_test.c:1-7` | Warning calibration still tests gauge-only oil warnings and a **0.5 L** fuel-leak threshold. Voice tests manually simulate a guarded install and test isolated helpers; lamps tests only test an OR mapping. None calls the runtime hooks/voice pumps. | Removing the film-warning fallback or breaking the new 0.1 L threshold can leave the 67630 gate green. The "no partial install" test cannot detect writes before validation in `voice_install`, and the lamp tests cannot detect a broken AL ABI or unload race. | Add tests against shared runtime decision logic and a controlled hook/restore harness, including fault resolution, terminal dispatch and changed-score cases. | FACT |

### Scope A verification and remaining limits

Static file disassembly through the SDK front door matched the complete stolen instruction boundaries:

| site | actual prefix | stolen / resume | ABI evidence |
|---|---|---|---|
| 0x4EC1A0 | `83 EC 58 / 56 / 8B F1` | 6 / 0x4EC1A6 | Raw `functions/CollisionSparkParticleEffects_004ec1a0.c:1` declares char-return thiscall with two float pointers and one float; mod declaration at `engine_failure.c:8756-8757,8773-8774` matches. Full runtime stack preservation was not tested. |
| 0x49B9A0 | `A1 DC 54 7C 00` | 5 / 0x49B9A5 | Raw `functions/sub_49B9A0_0049b9a0.c:1` has no arguments. File disassembly at 0x49BA26 is `mov al,bl; pop ebx; ret`. `MfLampFn` is unsigned char at `engine_failure.c:8924`, so the original result is read from AL. |
| 0x49BA50 | `8B 0D 64 0B 7D 00` | 6 / 0x49BA56 | Raw `functions/FuelWeightComputation_0049ba50.c:1` has no arguments. File bytes show AL-only returns at 0x49BA82/0x49BA85; the original-call typedef correctly narrows the return. |

`g_lamp_engine/g_lamp_fuel` are aligned int-sized flags; the major race is patch/lifetime publication, not a proved torn flag read. Partial lamp install can enable one lamp while the other refuses; it is optional and independent, but successful installation still has A01/A02. Ordinary second-image activation encounters the mandatory fuelcut prologue refusal before the new hooks (`engine_failure.c:9943-9959`); this is not evidence that unload/reload is safe. Failure-safe removal and repeated-load native tests remain pending.

Voice validation rejects invalid score ranges and stitch values **before** the first planned write (`engine_failure.c:1499-1539`), but the transaction executes on the worker without excluding concurrent game/other-mod table users. It also accepts duplicate in-range scores (`mf_voice.h:186-193`; offline reproduction: all scores 42 -> accepted). This would alias clip mappings and corrupt restoration if a runtime layout supplies duplicates; baseline donor uniqueness is a documented static assumption, not a runtime gate. `voice_restore` is whole-row for the four parity fields, despite its per-field comment: changing one field keeps the other three parity edits. No stitch-table write was found.

The impact burst transfers several peak/pointer fields across threads and resets them after sampling (`engine_failure.c:8787-8791,8851-8871`). There is no coherent exchange of the entire sample, and the delayed `phys+0x3D84/+0x3D8C` read has no lifetime check. Lost contacts/stale-pointer dereference on a session teardown are unresolved runtime risks; no such occurrence was observed in this audit. A01/A02 alone already fail the live-safety gate.

## 3. Cause -> effect by fault

Manual arms use the virtual budget; automatic onsets use the lifetime/hazard model; impacts use their own bounded roll. There is one active fault with shared pre-critical radio state. OK below means the inspected source chain agrees for the named portion, **not** a live acceptance pass. Every row remains untested for native presentation in this audit.

| fault | trigger / model variable -> symptoms -> terminal | result |
|---|---|---|
| Water leak | Arm -> coolant loss and radiator scaling (`mf_engine_model.h:2692-2753`) -> at 1 L lost, COOLANT01 is scheduled through `voice_instr`, adding its default 30-60 s after a 1-3 s hold (`engine_failure.c:5053-5055`; `mf_voice.h:115-118`). At <1.6 L, COOLANT LOW/steam and PIT AND RETIRE; water HUD uses max(normal+12,90 C), cooling HUD max(normal+18,95 C). Gasket dry/thermal creep or native lifetime ends engine (`mf_engine_model.h:2774-2783`; `engine_failure.c:6156-6180,6204-6241`). | **WRONG** for baseline/voice guards and queued death advice (C01/D02); quantities and gasket chain otherwise source-consistent. |
| Radiator debris | Arm -> 70/85/95% blockage, reduced base/air cooling -> water-temperature threshold -> WATER TEMP then held RADIATOR BLOCKED/BOX - DEBRIS. WATER01 can win over RAD01. Random speed-weighted clearance or pit clears onset; gasket/native clock can end engine if still hot. (`mf_engine_model.h:1593-1595,2666-2672,2730-2734,2774-2783`; `engine_failure.c:4975-5103`). | **WRONG** diagnosis order and advice after clearing (B01/D01); cold-arm baseline untested by normal harness (C01). |
| Oil pressure | Pump efficiency falls -> gauge ratio <0.60 OR film <critical -> held OIL PRESSURE/OILP01 and timed OILP02/03; wear >3 s supports SHORT SHIFT; h_b<0.30 critical/BEAR01; h_b<=0 blows/seizes (`mf_engine_model.h:2595-2657`; `engine_failure.c:4173-4188`). | **OK** core chain; **UNTESTED** runtime film fallback, radio/lamp integration and dying-while-coasting handling. |
| Oil leak | Oil volume falls -> blue smoke above 2% loss -> marshals then OILLEAK01 then OILLEAK02; gauge/film warning, OIL TEMP only above optimum+5 AND learned oil+8; low sump/bearing critical; bearing seizure (`engine_failure.c:2585-2634`; `mf_engine_model.h:2613-2657`). | **OK** core chain; **UNTESTED** end-to-end warning order. Existing L2 gauge-only report falsely describes absence of the now-enabled film warning (E01). |
| Valve air | Return pressure decreases; <0.70 -> LOSING REVS/VALVE01. Lowered ceiling causes float/misfire; SHIFT NOW follows actual float, but VALVE02 is a fixed delayed pair, independent of current float. h_vt<0.30 critical, <=0 blow-up (`mf_engine_model.h:2532-2577`; `engine_failure.c:4053-4064`). | **WRONG** claimed condition for spoken float instruction; timed line can assert float after driver has stayed below ceiling (D01 mechanism). |
| Over-rev | HUD uses >1.04 of current band after lowering grace. Damage uses model car limit/health; abrupt death requires RPM above **car torque-zero**, >0.3 s dwell, not universally 1.15 (`engine_failure.c:4622-4661`; `mf_engine_model.h:2423-2454`). | **WRONG** universal 115% reference; **UNTESTED** pending-call removal at actual death (D02). |
| Exhaust | Arm -> crack growth, delayed actual pops -> EXHAUST after first pop; WATCH THE REAR after its diagnosis; EXH01 requires >=3 pops then 5-20 s, not just first pop/3-8 s. Rear heat can break suspension; not a dedicated engine-kill terminal (`engine_failure.c:4354-4409,4221`; `mf_engine_model.h:2808-2849`). | **WRONG** reference's voice trigger; **UNTESTED** native rear failure and symptom ordering. |
| Blow-by | Ring seal falls -> power loss/blue smoke. Index >0.3 held 45-90 s -> ENGINE PROBLEM; then BLOWBY01 requires smoke/running and another 5-20 s; EASE OFF. Seal<0.30 critical, <0.15 stop (`engine_failure.c:4536-4550`; `mf_engine_model.h:2789-2803`). | **OK** core chain; **UNTESTED** slow diagnosis finishing before critical/death on short budgets. |
| Fuel pressure | Delivery capacity <0.90 -> FUEL PRESSURE/FUELP01; delayed FUELP02, capacity <cut-full -> SHORT SHIFT; capacity floor under demand accumulates starvation dwell -> critical and stop (`engine_failure.c:5230-5239`; `mf_engine_model.h:2857-2914`). | **OK** core chain; **UNTESTED** native starvation/radio timing. Warning describes capacity, not measured delivery at low demand. |
| Fuel leak | Accumulated loss >=0.1 L then 20-40 s -> FUEL LEAK/FUELLEAK01; lap-derived shortage -> SAVE FUEL or BOX FOR FUEL/FUELLEAK02. Loss drains native fuel (empty can stop natively); hot-exhaust stochastic fire blows engine (`engine_failure.c:5381-5419`; `mf_engine_model.h:2930-2942`). | **WRONG** if reference is read as guaranteeing critical/fire before every end: empty fuel is a separate native end, and practice has no race-distance target. **UNTESTED** low-start-fuel case and 0.1 L warning gate (E01). |
| Impacts | Native player contact + vulnerability gate -> merged peak -> 0 below 2000, 25% at5000, 70% at8000, 85% at10000; rolled water/exhaust/oil fault; HIT02 spends diagnosis slot, otherwise HIT01 (`engine_failure.c:8781-8791,9831-9841`; `mf_engine_model.h:2100-2153`). | **WRONG** reference HIT01 restriction: source calls it for a detected no-fault 2000-5000 impulse too. **UNTESTED** cross-thread burst coherence, pointer lifetime and uninstall safety. |

## 4. Documentation versus code

| documentation | discrepancy |
|---|---|
| `docs/FAULT_MESSAGING_REFERENCE_2026-10-05.md:3` | Says dev A1B25511 / installed 0A7D7D4D. Audited dev hash is 73F30551; installed hash was not independently read. README:1 and current handoff identify 73F30551, while README:2-3 retain older installed/next claims. |
| Reference `:25,58-63` | Floor is 50 C, not 70 C; WATER TEMP minimum is 90 C, not ~89 C; hot-escape steam minimum is 88 C, not ~87 C. Low-coolant steam remains a distinct exception. |
| Reference `:26` | OIL TEMP needs optimum+5 **and** learned oil+8, not optimum+5 alone; unknown oil baseline bypasses the latter check. |
| Reference `:16` | Death gap is 5 s (`mf_msg.h:176-177`), not a universal 15 s. Gasket death deliberately queues GASKET01 and DEAD01/02, two death-priority lines (`engine_failure.c:6173-6177`). Coolant and oil-temperature instruction-only calls also bypass the global diagnosis budget. |
| Reference `:12,38,59,78` | Several HUD instructions start after 6-14 s, while paired speech is 30-60 s after its **request**, not playback. VALVE02/WATER02 are not continuously gated by their narrated conditions. |
| Reference `:57,135` | COOLANT01 is not immediate at 1 L loss: a held 1-3 s condition arms a default 30-60 s instruction slot. |
| Reference `:69-70,94-95` | Radiator diagnosis is displaced by earlier WATER01; EXH01 waits for three pops plus 5-20 s. |
| Reference `:88,126` | Hard over-rev is car torque-zero (fallback available), not universally 115%; no-fault HIT01 has no additional >=5000 gate. |
| `docs/ENGINEER_VOICE_INTEGRATION_2026-10-05.md:155-162,349-354`; `dll/mf_voice.h:4-5` | The "no other first-party mod" exclusion missed six Pit eids. Restore is whole-row for four parity values, not per-field. |
| `tools/voice_level.py:21-23,45-53` versus `docs/HANDOFF_2026-10-05.md` section 4 | Tool still searches up to +24 dB toward -9.5 dB, with **60 ms** release; the contemplated small +2 dB / 5 ms release is not implemented or approved. The known backup-directory refusal limits rerunning, but does not turn the tool into the proposed small boost. |

Tooling boundary: inspected default voice_build/sdt_append/voice_level input paths do not write Desktop originals. `sdt_append.py` copies its input to TEMP and checks clip preservation before output, but its unrestricted `--out` is writable anywhere (`:109-110,231-233`), including the input/game/original path if explicitly supplied; its "never writes game folder" statement is not enforced. It does not install by default. No audio build, restore, uninstall or level operation was executed.

## 5. Gates and evidence

Date: 2026-10-05, local time. Product: mechanical-failures, canonical development component; governance portfolio stage **investigate**, with local engine work in private verification/live-calibration. No lifecycle stage or protected artifact changed.

Dev DLL SHA-256: `73F3055172C2A94AC07C165BAA406C0676FE9C3129FEC0EF1882F856844EE50A`.

Each existing test was compiled directly, without running any build batch or overwriting the dev DLL. CWD was the workspace root. The reproducible command template was:

```powershell
& C:\msys64\mingw32\bin\gcc.exe -O2 -Wall -Wextra -Werror -I %USERPROFILE%\Desktop\F1\development\mods\mechanical_failures\dll %USERPROFILE%\Desktop\F1\development\mods\mechanical_failures\tools\<name>_test.c -o <TEMP>\<name>_test.exe -lm
& <TEMP>\<name>_test.exe
```

Actual TEMP directory: `%USERPROFILE%\AppData\Local\Temp\mf_audit_20261005_7jmoeud1`. Every compile and run exited 0.

| name | output |
|---|---|
| engine_model | `ALL PASS (67630 checks)` |
| msg | `MSG TEST: ALL PASS (817 checks)` |
| voice | `VOICE TEST: ALL PASS (330 checks)` |
| lamps | `lamps_test: 212 checks, 0 fail`; `ALL PASS` |
| ai_pulloff | `ai_pulloff_test: 81 checks, 0 fail` |
| ai_react | `ALL PASS (40 checks)` |

Additional edge test: compiled C from standard input with the same flags plus `-x c -`, output `<TEMP>\edge_test.exe`; no source file was created. It included the unmodified model/voice/message headers. The setup used `mf_car_derive(&c,1895,1784,40,398,2.93,380,1.20*1895,0.86*1895,0.05,0.20*1895,0.25*1895,2.639e-6,1,5.312e-3,1.022e-4)`, seed 7711, dt .01, life 5000, RPM 1895, oil 380 K, load 1, speed 90 m/s, fuel 60 L, ambient 25 C, on-track/running. After `mf_fault_arm(...,MF_FAULT_WATER_LEAK,MF_SEV_MEDIUM,300)` and one `mf_model_step`, results were:

```text
unknown baseline: water=125.0 C base=125.0 C warn=0 cool=0
partial warmup baseline: water=96.0 C base=60.0 C warn=1 cool=1
duplicate score validation=1
already queued info after terminal, coasting quiet=0: eid=25
```

The second thermal case seeded `e.water_base_k=333.15` before arming, representing an already learned but incomplete warm-up baseline. Duplicate validation filled every score with 42 and stitch with 0x40. Queue reproduction called `mfr_clear`, `mfr_req(...,25,MFR_P_INFO,1000)`, then `mfr_tick(...,2000,0)`. These demonstrate helper behaviour; they do not independently execute the runtime voice dispatcher or establish native symptom timing. Edge compile/run exited 0.

Read-only SDK commands, from `f1c_ida_decompiled`:

```text
python tools/f1c_mod.py start --json
  freshness.stale = false
python tools/f1c_mod.py evidence-query findings "engine" --json
  total 212; 207 reviewed / 5 RECORDED (RECORDED not treated as proof)
python tools/f1c_mod.py evidence-query findings "voice" --json
  total 9; no matching mechanical donor/stitch entry in the filtered results
python tools/f1c_mod.py inspect 0x4EC1A0
python tools/f1c_mod.py inspect 0x49B9A0
python tools/f1c_mod.py inspect 0x49BA50
python tools/f1c_mod.py exe-disassemble --exe "..\F1 Challenge V10\F1 Challenge Online.exe" --address <site> --size 16
  sites 0x4EC1A0, 0x49B9A0, 0x49BA50; prefixes listed above
python tools/f1c_mod.py exe-disassemble --exe "..\F1 Challenge V10\F1 Challenge Online.exe" --address 0x49BA20 --size 112
  confirms AL-only returns
```

A speculative 0x4ECBA0 disassembly window began inside an instruction and was **not used as ABI evidence**. No SDK tests, memory readers, debugger, game or injector was started. Static file disassembly does not prove that a running executable has these bytes.

Workspace validator command before and after report creation:

```text
powershell -NoProfile -ExecutionPolicy Bypass -File tools\workspace\check-workspace-structure.ps1
Errors: 1 / Warnings: 0
ERROR: Unexpected top-level directory: .claude
```

Pending validation: safe patch/unpatch/partial failure and repeated-load tests; active-mod donor reservation; cold-arm/repair/idle baseline scenarios; runtime HUD/radio ordering and cancellation; native fault presentation and audio; safe audio installation/uninstall; existing Pit compatibility blocker. Passing helper gates cannot substitute for any of these.

Record/preservation gate: this is the sole workspace file created or changed by the audit. The report is discoverable at the exact output path already named by `docs/AUDIT_PROMPT_GPT61_SOL_2026-10-05.txt` in the canonical component's docs folder. The supplied Claude scratch location was absent; that canonical prompt was used. Explicit one-report-only scope overrides the usual README/index/findings.jsonl append for this task. No source, governance, SDK findings, installed file, secret, or Desktop audio original was edited. Nothing requires restoration; test binaries remain in TEMP. Report encoding is UTF-8 without BOM; final read-back and unchanged dev hash checked.

Wasted-effort check: repeated large reads/output truncation caused avoidable overlap; test breadth was exactly the six requested gates plus one targeted helper edge test; failed commands included the absent supplied path, a Python console-encoding failure, and a Windows rg wildcard, followed by corrected bounded reads; an overly broad executable inventory and the misaligned disassembly window were unnecessary; no polling/idle loop occurred.

## 6. Questions for the owner

1. Which game editions and concurrent mods must the repaired candidate support?
2. Should every failure explicitly warn before engine death, including a fuel leak that empties a small starting fuel load?
3. After safe hook fixes, can the next human pass include arming a cooling fault before warm-up and unloading while moving?
