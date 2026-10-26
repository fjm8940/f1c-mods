# Internal tool implementation archive

> **Do not invoke entries in this catalogue directly.** The files and historical
> examples are preserved, but the sole supported interface is
> `python tools/f1c_mod.py`. Use `python tools/f1c_mod.py route tools/README.md`
> and the LLM operating contract for current workflows.

# tools/ — the historical F1C IDA-corpus toolchain (preserved)

**Use these tools. Do NOT reach into `%USERPROFILE%\Desktop\F1C_ghidra_project\tools`
anymore** — every tool needed for inspection, live memory, patching, and the
full mod-investigation loop now lives here, repointed at this corpus
(`functions/`, `name_index.json`, `skeleton_facts_all.json`, `tools/sdk/`).
The ghidra copies are the deprecated originals (hardcoded ghidra paths).

Python: `%USERPROFILE%\AppData\Local\Python\bin\python3.exe`
Live-memory tools need the game running; **writes hit the running process** —
save replays/setups first.

## Canonical paths and friction notes (2026-08-14 — do not regress)

- Repo root = `%USERPROFILE%\Desktop\F1\f1c_ida_decompiled`; the game install
  = `%USERPROFILE%\Desktop\F1\F1 Challenge V10` (the `F1\` segment is real —
  older docs/tools hardcode paths without it and fail at runtime). Tools derive
  both from `__file__`; never paste a literal absolute path.
- The mechanical-failure GUI is **not** `tools/mechanical_failure_gui.py`. It is
  `mods/extended_vanilla_features/tools/mechanical_failure_gui.py` (Gearbox /
  Engine / Suspension tabs; the Suspension tab drives `suspension_failure.dll`
  through shared memory `F1C_SUSFAIL_CTL`, unload event `F1C_SUSFAIL_UNLOAD`).
- **Rebuilding a DLL that is still injected** fails with `Permission denied`.
  Unload CLIENT DLLs before the broker (it refuses to unload while clients are
  registered), via `python tools/f1c_mod.py unload <event>`:
  `F1C_ESC_PAUSE_MENU_UNLOAD`, `F1C_SUSFAIL_UNLOAD`, `F1C_PACEPROBE_UNLOAD`,
  `F1C_FUEL_FIRE_LAB_UNLOAD`, then `F1C_GAME_UPDATE_BROKER_UNLOAD`. A missing
  event means that DLL is already out. Never `FreeLibrary` from outside — these
  DLLs self-eject only (worker-thread crash class, see the legacy READMEs).
  `f1c_mod.py build` now pre-checks the lock and prints this order.
- `f1c_mod.json` manifest schema (enforced by `validate`, see
  `tools/sdk/tests/test_mod_orchestrator.py`):
  ```json
  "hooks":  [{"function": "<reviewed ABI name>", "handler": "<C ident>",
              "stolen_length": 5, "expected_bytes": "AA BB CC DD EE"}],
  "writes": [{"object": "F1CVehicle", "field": "<vehicle field name>"}]
  ```
  The 6-byte push-imm/ret stub detour used by `suspension_failure` does NOT fit
  (f1c_hook.h emits only 5-byte `E9 rel32`) — keep its `hooks` empty and document
  the stub in the `.c` header + README. Wrong keys report the first bad key via
  `python tools/f1c_mod.py validate mods/sdk_sandbox/<id> --json`.

## Live memory / runtime (game must be running)
| Script | What | Example |
|---|---|---|
| `ai_difficulty_calculator.py` | Recommends AI Driver Strength (slider) + AI Realism (aggression) from player lap time, using the runtime-verified GDS lerp from the track AIW (docs/42). `calibrate` fits the per-track laptime exponent from two observed points. | `python tools/ai_difficulty_calculator.py calc --aiw <track>.AIW --player-lap 85.0 --ref-lap 84.2 --ref-slider 92` |
| `ai_form_jitter.py` | AI result variation: jitters RaceAbility + QualifyingAbility in rcd files between races (EMA form momentum + rare brilliant/awful weekends), so the same driver doesn't always win. Byte-exact backup on first run, `restore` undoes everything. Applies to NEXT session load (game can stay open). | `python tools/ai_form_jitter.py jitter --drivers-dir <install>\SeasonData\Drivers\2001 --dry-run` |
| `ai_live_form.py` | **WRITES.** Mid-session AI pace knob: rewrites veh+0x44F4 (baseline pace scalar — read every frame, never rewritten, POKE-PROVEN to persist) so a driver's pace walks to a new value at the game's own ±0.01/frame. Instant `set`, multi-minute `fade`, `reset` restores captured originals. Player refused, factor clamp [0.85,1.08]. See docs/43 sec3.8. GUI: `ai_live_form_gui.bat` (double-click, self-elevates). | `python tools/ai_live_form.py fade Hakkinen 0.98 --minutes 8` |
| `session_setup.py` | **EDITS test.PLR (byte-exact backup first).** Chooses the track WITHOUT menus or mouse coordinates: rewrites the `[ SCENE ]` keys (`Scene File`/`Scene Description`/`AI Database File`) that the menu itself writes. Resolution-independent. Menu selection CANNOT be poked — the globals it moves (`CurrentTrackPath`/`SettingsFilePath`/`CurrentSceneIndex`) are records, not inputs (proven). Pair with `lab.py autolaunch quickrace`. | `python tools/session_setup.py set 2001_Monza --dry-run` |
| `screenshot.py` | **READ-ONLY.** Captures the game window ONLY (not the desktop) via `PrintWindow`+`PW_RENDERFULLCONTENT` — plain BitBlt is black for a DirectX window. No focus change, no input. Output is client pixels, so a target's position can be read off the image and fed to `gmouse.py`. | `python tools/screenshot.py tools/shot.png` |
| `tour.py` | **READ-ONLY.** Screenshots the game window every N seconds with an audible beep per shot, so a human can drive the menus while a record is kept of exactly when each capture happened. Used to map the menu flow in `tools/GAME_CONTROL.md` sec 4c. | `python tools/tour.py --interval 2` |
| `gkey.py` | **WRITES. Background in-race keypress, no focus stolen.** Writes the game's own pending-key queue (`word_7CB21C[]` + count `dword_7CB218`), drained every frame by 5 consumers. Camera cycle, F1–F10 driving aids (F6=Invulnerability), ESC→MainMenuTransition (guarded by `--confirm`). Reliable as a trigger, NOT precisely countable — count is not self-clearing. See `tools/GAME_CONTROL.md`. | `python tools/gkey.py press numpadenter` |
| `gmouse.py` | **WRITES. Background menu/UI clicks, no focus stolen.** UI screens are window-message driven, so `PostMessage` of WM_MOUSEMOVE/LBUTTONDOWN/UP presses buttons without the game being foreground. Client-pixel or `--norm` coords. **Refuses the bottom-left icon strip (TELEMETRY — never click) unless `--force`.** | `python tools/gmouse.py click --norm 0.96 0.97` |
| `gamekey.py` | **WRITES. FALLBACK ONLY — steals desktop focus.** Interception kernel-driver keypress (real hardware-level, works where SendInput is invisible to the game). Forces the game foreground via AttachThreadInput and HARD-ABORTS rather than sending a key to the wrong window. Prefer `gkey.py`/`gmouse.py`. | `python tools/gamekey.py add --dry-run` |
| `poke.py` | Named memory console: read/write/watch/veh/snapshot/diff by NAME (resolves via `tools/sdk/f1c_sdk.json`) | `python tools/poke.py watch TireWearMultiplier` |
| `snapshot.py` | NUMPAD0 → dump player-vehicle state (RPM/speed/gear/tire/pos) to console + `snapshot_out.txt` | `python tools/snapshot.py` |
| `standings.py` | One-shot race standings (position fields, finish state, speed) for all cars | `python tools/standings.py` |
| `race_monitor.py` | Periodic full vehicle-struct dump + change tracking to `monitor_output/`; labels from `name_index.json` vehicle_fields (needs `pip install pymem`) | `python tools/race_monitor.py --interval 5` |
| `garage_exit_trace.py` | **READ-ONLY.** Traces the game's own garage->track transition. Samples the whole `0x48C0` block for every garaged car and separates *discrete state flips* from *physics jitter* (a parked car still jitters ~406 words/sample), then intersects across cars. Collapsed 406 offsets to 17 and found `0x4510 driverStatus 5->3`. | `python tools/garage_exit_trace.py trace 150` |
| `deretire_probe.py` | **WRITES.** Brings a retired/garaged car back: flags + `driverStatus=3` + **engine crank** (`0xA0`/`0x39A0`) — the crank is the actual gate, a garaged car's engine is OFF. Revival proven in-race; the car then has no path out of the garage. See `investigations/garage_exit_ignition/README.md`. | `python tools/deretire_probe.py deretire 1 45` |
| `runtime_extract.py` | **RUNTIME corpus extractor** — dissect a struct-array across all live instances + follow its heap children into `runtime_facts.json` (OBSERVED tier, offset-path keyed, NO absolute addrs) + `runtime_structs/<name>.md`. Runtime analog of `skeleton_extract.py`; sees the heap child objects the static pipeline cannot. | `python tools/runtime_extract.py --struct vehicle` |
| `runtime_delta.py` | Behavior-delta capture/diff: snapshot all live cars (flat + heap-child bytes) -> cause behavior -> snapshot -> diff. Tags moved offset-paths into `runtime_facts.json`. Same-session only (heap realloc noise across restarts). | `python tools/runtime_delta.py capture before; ...; diff before after --label X --slot N` |
| `semantic_memory_linker.py` via `f1c_mod.py memory-*` | **READ-ONLY.** Joins static field layouts, exact raw-C store widths, reviewed object identities, config tokens, persistence and runtime flat/child captures. Provides layout matching and control-subtracted event ranking without auto-promoting guesses. | `python tools/f1c_mod.py memory-front-wing` |
| `process_memory_snapshot.py` via `f1c_mod.py memory-snapshot/memory-diff` | **READ-ONLY.** Captures readable module or process pages into a compressed SHA-addressed store, preserves the memory map, and diffs module pages by RVA across base relocation. | `python tools/f1c_mod.py memory-diff inactive active --transition 0:1` |
| `variant_resolver.py` via `f1c_mod.py variant-profile/resolve-symbols` | **READ-ONLY.** Resolves moved functions, globals and structure offsets using masked anchor quorum, normalized instruction hashes, local operand contexts and cross-symbol invariants. Ambiguity produces a refused profile. | `python tools/f1c_mod.py resolve-symbols modified.exe` |
| `match_values.py` | **READ-ONLY. Names offsets with no LLM and no debugger.** Every live car is a known driver with a shipped `.rcd` file, so an offset's value vector across the grid must agree with the vector of whichever `.rcd` key it holds. Robust affine fit recovers the game's own transform (`0x44C0 = 1 - QualifyingAbility/22`); **outliers are reported, not tolerated — an outlier is usually a second writer** (found `AIDriverStructInit` overriding `0x44CC`). Self-validated by rederiving the known `0x44F4` pace formula. | `python tools/match_values.py --tol 1e-6 --out value_matches.json` |
| `trace_retire.py` | Live event tracer: polls every car's key flags (callToBox/pitting/FinishState/Retired/Active/PitState/pos), prints timestamped changes. Captures ordered sequences (e.g. wing-loss -> pit -> retire). | `python tools/trace_retire.py 90` |
| **`MCP_TOOLS.md`** | **Reference for the live-debugging MCP servers (Frida / x64dbg / Cheat Engine)** — which to use, proven recipes, and the hard gotchas (Frida globals don't persist between calls; x64dbg BPs do; condition syntax). READ before doing live BP/hook work. | — |
| `part_detach_watch.js` | **Frida agent: the part-detach chain at the source.** Hooks `RepairServiceStateDispatcher_004ed250` + `sub_41AFD0_0041afd0` to log the raw `partIdx` (0-3 wheel, 4 front wing, 5 rear wing, 6-13 bodywork), and `DriverDataTableLookupAndTimeConversion_004144f0` to log EVERY commentary event id at emission — before the 1-deep queue head can lose it. Read-only. | `python tools/mod_pipeline/watch_run.py tools/part_detach_watch.js --out detach.csv` |
| `watch_damage_commentary.py` | Polls damage fields + the commentary controller's per-event timestamp array (`0x5EA618+0xAF8`). Complements the hook above: the hook sees **emission**, this sees **playback** — they differ (AI lines can queue behind a player-contact line and be dropped). | `python tools/watch_damage_commentary.py --hz 30 --slot 3` |
| `wing_record_probe.py` | Reads the two wing damage records (`0xCD4`/`0xD14` front, `0xD18`/`0xD58` rear) across the field, flags the retire term `*(veh+0xD18) && !*(byte*)(veh+0xD58)`. Only meaningful BETWEEN detach and pit box — the game resets the flag once the retirement completes. Read-only. | `python tools/wing_record_probe.py` |
| `force_wing_retire.py` | **Retire a healthy AI car via the GAME'S OWN path**: arms `veh+0xD58=0` + `callToBox=1` and hands off — the car drives in and the game retires it, garage teleport included. Unlike forging the end-state (`mods/ai_pit_retire`), which strands cars at the pit box. WRITES to the running game; reversible. | `python tools/force_wing_retire.py arm-all` / `status` / `revert-all` |
| `teleport_car.py` | Frida: list cars / teleport by name / teleport retired cars | `python tools/teleport_car.py list` |
| `move_car_ctrl.py` | Frida controller demo for `frida_move_car.js` (nudge player position) | `python tools/move_car_ctrl.py` |
| `frida_move_car.js`, `watch_tire_wear.js` | Frida agents (loaded by the controllers / `mod_pipeline/watch_run.py`) | — |

## Static patching / scaffolding (no game needed)
| Script | What | Example |
|---|---|---|
| `patch.py` | Byte-patch manager: verify/apply/revert/stack patches by manifest (default `mods/extended_vanilla_features/patches/manifest.json`, override with `$F1C_PATCH_MANIFEST`; `status` prints which one is live); patch a COPY with `--out` | `python tools/patch.py status` |
| `gen_hook.py` | Scaffold a compilable DLL mod (`tick` polling or `detour` inline-hook). Resolves func names/addrs from `functions/`; SDK header from `tools/sdk/` | `python tools/gen_hook.py detour my-penalty DisqualificationHandler` |

## Unattended experiments — `lab.py`
| Command | What |
|---|---|
| `lab.py probe` | Is the game up, which vehicle slots are live |
| `lab.py sweep` | **Read-only** baseline pass: which offsets move on their own |
| `lab.py sweep --go` | The real classifier: per offset, `LATCHED` (a poke holds) / `RECALCULATED` (must hook the writer) / `VOLATILE` / `SKIPPED_*`. Appends `persistence_map.jsonl`, checkpointed per offset, resumes after a crash |
| `lab.py autolaunch quickrace --launchtime 15` | **Edits `config.ini`** (byte-exact backup first) to boot straight into a race unattended. `[AUTOLAUNCH] MODE` enum at `off_5BE6C4`: Off/QuickRace/Replay/MultiServer/MultiClient |
| `lab.py autolaunch off` | Restore `config.ini` from the backup, hash-verified |

Answers "can I poke this or must I hook it?" mechanically — the question that
killed safety_car v3. Writes a minimal perturbation (int+1, float×1.01), never
garbage, and restores the original immediately. Pointer-looking dwords, offsets
that move on their own, and a protected set (liveness marker, RetiredFlag,
ActiveFlag, FinishState, callToBox, RearWingIntactFlag) are never written.
Results are **OBSERVED** tier: one session, one car state.

## Ask first — `ask.py` (no game needed)
| Command | What |
|---|---|
| `ask.py func <addr\|name>` | Full dossier: source module (from the MSVC `__FILE__` strings), priority score, callers, callees, globals touched with name+tier, struct fields with name+tier, guards, strings referenced, fill + source paths, matching findings |
| `ask.py offset <hex>` | Vehicle struct +N: name_index name/tier/note, runtime OBSERVED sample, behavior deltas it moved in, WRITERS and readers ranked with a vehicle-likeness flag, disproven-offset check, matching findings |
| `ask.py global <hex\|name>` | Name + tier + sources + name conflicts, writers/readers. Bare text searches names |
| `ask.py token <text>` | String literals + who references them, plus config-schema token -> offsets -> setter |
| `ask.py findings <text>` | Prior art: FACT / CONFIRMED / DERIVED / OPEN / **DISPROVEN**, with repro command |
| `ask.py stats` | Corpus inventory incl. the trust-tier split |

Read-only, joins eight stores that use three different key formats. Backed by
`findings.jsonl` — append to it whenever a result lands, especially a negative one.

## String / config-schema extraction (no game needed)
| Script | What | Rerun when |
|---|---|---|
| `../extract_strings.py` | **Headless IDA.** Every string literal + code/data xrefs + IDA symbol → `strings_index.json`. Segment + printability filters are mandatory: without them 37k x86 instruction bytes come back as "strings" (IDA's `ignore_instructions` does not exclude them). | IDB changes |
| `strings_report.py` | `strings_index.json` → `strings_modules.json` (original 2002 `.cpp` filenames → the functions compiled from them) + `strings_tokens.json` + `STRINGS_REPORT.md` | after `extract_strings.py` |
| `extract_config_schema.py` | Recovers config-file schemas from the parsers themselves. Two dialects: token+offset in one call (`assoc=inline`, strong) and token lookup followed by a typed-index store like `*((float *)this + 4403)` (`assoc=lookahead`, proximity-matched — colliding rows are flagged `suspect`, read the `.c` before use). 771 fields, 36 parsers → `config_schema.json` / `CONFIG_SCHEMA.md` | after `extract_strings.py` |
| `index_data_files.py` | Indexes the **shipped** data files in the game install (1,587 .hdv/.aiw/.rcd/.tbc/.gdb/.plr/.veh): per key, file counts, value ranges, garage-adjustable flag, and the parser offset when known. Values only — no asset copied. Also ranks the most-used keys with **no parser yet** (the next parsers to recover) → `data_index.json` / `DATA_INDEX.md` | after `extract_config_schema.py`, or when the install changes |

Run order:
```
idat.exe -A -L"str.log" -S"extract_strings.py" "<path>\F1 Challenge Online.exe.i64"
python tools/strings_report.py
python tools/extract_config_schema.py
```

## SDK / index generation
| Script | What | Rerun when |
|---|---|---|
| `gen_sdk_from_index.py` | `name_index.json` (+ proven vehicle array) → `tools/sdk/f1c_sdk.json` + `.h` | name index changes |
| `gen_vehicle_struct_header.py` | `name_index.json` → `tools/sdk/f1c_vehicle_struct.h` (stdint, for mods) + `f1c_vehicle_struct_ida.h` (IDA-native, for `parse_decls`). Gap-fills to VEH_STRIDE | name index changes |
| `apply_vehicle_struct_idb.py` | Imports `struct F1CVehicle` into the IDB Local Types so Hex-Rays shows `v->LivePaceFactor`, not `*(float*)(a1+0x453C)`. **Headless only** | after `gen_vehicle_struct_header.py` |
| `regen_knowledge_graph.py` | `skeleton_facts_all.json` + fills → `knowledge_graph/*.json` | corpus/index changes |
| `import_crossbinary_names.py` | Propose names from CONFIRMED F1C↔NASCAR match table | new cross-binary matches |

### Applying the vehicle struct to the IDB
Close the IDA GUI first (DB lock), then:
```
python tools/gen_vehicle_struct_header.py
idat.exe -A -L"idat_struct.log" -S"apply_vehicle_struct_idb.py" "<path>\F1 Challenge Online.exe.i64"
```
Success = `sizeof=0x48c0` in the log; that equals VEH_STRIDE and is the check
proving no offset drifted. Then retype a function's `this`/arg1 to `F1CVehicle *`
(Y in the decompiler) **per function, as you land in it** — do NOT mass-retype,
a wrong `this` guess corrupts readable decompilation across the corpus.

Field names carry their trust tier as a line comment. A DERIVED or GUESS field
looks exactly as authoritative as a FACT one once Hex-Rays renders it — read the
comment before building on it.

Two headless gotchas (both cost a session once):
- the script **must** end in `ida_pro.qexit()`, else idat hangs forever in the
  batch loop looking exactly like slow autoanalysis (`.i64` mtime never updates);
- `parse_decls` cannot resolve `#include <stdint.h>` — that is why the `_ida.h`
  flavour exists. `-L` is required to capture `print()`; the log APPENDS, so
  check line numbers before trusting an error is from the current run.

## Mod-investigation pipeline — `tools/mod_pipeline/`
Read `tools/mod_pipeline/MODDING_PLAYBOOK.md` first, then:
| Command | What | Example |
|---|---|---|
| `mod_pipeline.py map <seed>` | Static dossier: every consumer of a behavior's anchor globals, split player-vs-AI path, inline FACT source from `functions/` → `investigations/<name>/` | `python tools/mod_pipeline/mod_pipeline.py map tools/mod_pipeline/seeds/ai_drying.json` |
| `mod_pipeline.py watch <seed>` | Generate a Frida CSV-logging agent for the anchors | `... watch tools/mod_pipeline/seeds/ai_drying.json` |
| `mod_pipeline.py patch --addr ... --type ... --value ...` | Patch record + Frida live-override + rollback bytes | `... patch --addr 0x0059e450 --type float --value 0.06 --name wetness` |
| `asm_tools.py resolve <addr>` | One-shot function dossier: identity, ret-imm/stack, callers, strings, raw `.c` | `python tools/mod_pipeline/asm_tools.py resolve 0x0042a180 --source` |
| `asm_tools.py redirect <caller> <from> <to>` | Find E8 calls, emit retarget bytes, CHECK thiscall stack balance | `... redirect 0x004bc4e0 0x004bc0e0 0x004bd000` |
| `watch_run.py <agent.js>` | Attach Frida, stream a watch agent's CSV; `--interactive` for live poke/dump | `python tools/mod_pipeline/watch_run.py investigations/ai_retire/ai_retire_watch.js` |

## Local data these tools depend on (checked in, deterministic)
`ida_data_xrefs.csv` (func↔global R/W edges), `cross_binary_final.csv`
(NASCAR/ISI identity), `ida_functions.csv` (addr/size/end), `functions/`
(raw Hex-Rays `.c`), `name_index.json`, `knowledge_graph/callers.json`.

## The verification loop (DERIVED → FACT)
1. `poke.py list <term>` — find candidate (DERIVED = unverified lead)
2. `poke.py watch <name>` while causing the behavior in-game — confirm meaning
3. Promote the entry in `name_index.json` to FACT (note evidence)
4. `gen_sdk_from_index.py` + `gen_vehicle_struct_header.py` + `regen_knowledge_graph.py` — propagate
   (re-run `apply_vehicle_struct_idb.py` too if the IDB should see the new field)
