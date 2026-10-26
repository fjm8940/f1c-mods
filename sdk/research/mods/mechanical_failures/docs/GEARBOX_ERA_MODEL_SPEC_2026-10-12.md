# G7 - Era-general gearbox model (spec, 2026-10-12)

Owner request (2026-10-12): "implement a general gearbox model suitable for all F1 periods, should fit in to players
and AI and not break the lifetime value determined value - random failures might occur tho, just as with engines."
Builds on G1-G4 ([GEARBOX_SIM_SPEC_2026-10-12.md](GEARBOX_SIM_SPEC_2026-10-12.md)). Mod standard:
`development/mods/MOD_STANDARD.md`. Builder: Haiku agent, two batches with a Claude review STOP after each.
Dev only until owner live test + G6 GO.

## 1. Objective, non-goals, acceptance

Objective: the gearbox model knows the TECHNOLOGY of the car it runs on (H-pattern manual, early electro-hydraulic
semi-automatic, mature semi-automatic, seamless shift, 8-speed hybrid era) and books wear and picks faults the way that
technology fails. Same model for player and AI.

Non-goals: clutch, differential, oil leak chain (separate later batches); changing the hazard (eta/beta); changing any
game data file; CO-OP.

Acceptance:
- E1 **Life invariant (owner rule).** For EVERY era, the reference pace (`REF` pace in the test) books 1.0 s/s
  within 3 %. The native counter `veh+0x3D78` stays the only budget; the era never scales it.
- E2 Random failures unchanged: same Weibull hazard (beta 5, eta from INI) for every era.
- E3 Fault mix at the reference pace matches the era's table (section 3) within 5 % (4 % for shares <= 15 %).
- E4 Era 4 (SEMI_MATURE) reproduces the current G1 model exactly: all 39 existing tests pass unchanged.
- E5 Era comes from the car's own `.veh` year; unknown year -> SEMI_MATURE (the game's native era); INI override.
- E6 Player and every AI car get the era of THEIR car (mixed-era grids work).

## 2. Era detection (FACT, installed data 2026-10-12)

Every `.veh` has `Classes="FormulaOne, OpenWheel, <year>"` (installed: 1993-2005, plus `2005RH` and the safety car
`"Safety, DSGermany, 1999, 2000, 2001, 2002, NotAI"`). The live vehicle struct carries its `.veh` path as an inline
ASCII string at `veh+0x4714` (FACT, engine family E9-H5 / finding veh-4678-car-4714-veh-path-live-2026-10-01; panel
reads it the same way).

Rule: read the `.veh` (cap 64 KB), find the line whose key is `Classes` (case-insensitive, before `=`), take the FIRST
run of exactly 4 digits in its value with 1946 <= year <= 2040. If none: take a leading `^(\d{4})_` of the `.veh` file
name. If none: year 0 -> era SEMI_MATURE.

| Era id | Name | Years | Technology |
|---|---|---|---|
| 1 | MANUAL_EARLY | <= 1969 | 4-5 speed H-pattern, dog/crash box, no shift aids |
| 2 | MANUAL_LATE | 1970-1988 | 5-6 speed H-pattern, turbo torque from 1977 |
| 3 | SEMI_EARLY | 1989-1997 | first electro-hydraulic paddle boxes (Ferrari 640 1989) |
| 4 | SEMI_MATURE | 1998-2003 | mature semi-automatic 6-7 speed (current G1 constants) |
| 5 | SEAMLESS | 2004-2013 | seamless shift, 7 speed |
| 6 | HYBRID | >= 2014 | 8 speed, ECU-protected shifts |

INI `[Gearbox] Era=0` (0 = auto, 1-6 = force every car). Invalid -> 0.

## 3. Era profile (the only new numbers)

All values are DESIGN CHOICES (real-world direction, not measured). The reference shares (mesh + bearing + shift)
always sum to 1.00 so E1 holds by construction. Shift cost per shift = `shift_share / MF_GBX_SHIFT_REF_PS` (0.5/s).

| Field | E1 MAN_EARLY | E2 MAN_LATE | E3 SEMI_EARLY | E4 SEMI_MATURE | E5 SEAMLESS | E6 HYBRID |
|---|---|---|---|---|---|---|
| k_mesh (s/s) | 0.55 | 0.62 | 0.58 | **0.60** | 0.66 | 0.66 |
| k_brg (s/s) | 0.20 | 0.15 | 0.15 | **0.15** | 0.16 | 0.17 |
| shift_share (s/s) | 0.25 | 0.23 | 0.27 | **0.25** | 0.18 | 0.17 |
| split_sel / act / dog | 0.55/0.15/0.30 | 0.55/0.15/0.30 | 0.40/0.40/0.20 | **0.5/0.3/0.2** | 0.45/0.35/0.20 | 0.45/0.35/0.20 |
| mix lost/stuck/act/nodrive % | 50/25/10/15 | 50/25/10/15 | 35/25/30/10 | **45/30/15/10** | 45/25/20/10 | 40/25/25/10 |
| lost_neutral_p | 0.60 | 0.50 | 0.35 | **0.30** | 0.20 | 0.15 |
| rapid_s (s) | 0.60 | 0.50 | 0.25 | **0.25** | 0.15 | 0.15 |
| harsh_over | 0.03 | 0.04 | 0.05 | **0.05** | 0.06 | 0.08 |
| shock_over | 0.10 | 0.12 | 0.15 | **0.15** | 0.18 | 0.20 |
| shock_p_max | 0.60 | 0.55 | 0.50 | **0.50** | 0.40 | 0.30 |

Bold = the current G1 constants (E4 must be bit-for-bit today's model).

Reasons: manual boxes miss shifts and jump out of gear (high lost_neutral, low harsh threshold, slower "rapid"
window, bearings/crownwheel more exposed); early hydraulics were the weak point (actuator 30 %); seamless/hybrid
shifts cost the dogs little and the ECU refuses the worst downshifts (high harsh/shock thresholds, low shock chance).

Fault weights: the model computes them from the profile, never hard-coded per era:
`ref_gear = k_mesh + shift_share * split_dog`, `ref_sel = shift_share * split_sel`,
`ref_act = shift_share * split_act`, `ref_brg = k_brg`; `W_f = mix_f / ref_f`. For E4 this equals today's
`MF_GBX_W_*` (0.45/0.65, 0.30/0.125, 0.15/0.075, 0.10/0.15).

Fault names: in eras 1-2 the ACTUATOR fault is the "gear linkage" (same mechanics: refusals + false neutrals,
terminal jam). `mf_gbx_fault_name_era(fault, era)` returns "gear linkage" there; everything else unchanged.

## 4. Batch H1 - model + tests (pure C, no game addresses)

File `dll/mf_gearbox_model.h`:
1. Add `#define MF_GBX_ERA_MANUAL_EARLY 1` ... `MF_GBX_ERA_HYBRID 6`, `MF_GBX_ERA_DEFAULT 4`.
2. Add `typedef struct { double k_mesh, k_brg, shift_share, split_sel, split_act, split_dog, mix[MF_GBX_F_COUNT],
   lost_neutral_p, rapid_s, harsh_over, shock_over, shock_p_max; } MfGbxEra;` and a `static const MfGbxEra
   g_mf_gbx_eras[7]` table (index 0 unused = copy of 4) with section 3 values (mix as fractions, mix[0] = 0).
3. `MF_GBX_FN int mf_gbx_era_for_year(int year)` per section 2 table (0 or out of range -> 4).
4. `MF_GBX_FN const char *mf_gbx_era_name(int era)`; `MF_GBX_FN const char *mf_gbx_fault_name_era(int f, int era)`.
5. `MfGbx` gets `int era; MfGbxEra p; double w[MF_GBX_F_COUNT];`. `mf_gbx_init` sets era 4 (calls the setter).
   New `MF_GBX_FN void mf_gbx_set_era(MfGbx *g, int era)`: invalid -> 4; copies the profile into `g->p`; computes
   `g->w[]` per section 3.
6. Replace every use of `MF_GBX_K_MESH, MF_GBX_K_BRG, MF_GBX_SHIFT_S, MF_GBX_SPLIT_*, MF_GBX_W_*,
   MF_GBX_LOST_NEUTRAL_P, MF_GBX_RAPID_S, MF_GBX_HARSH_OVER, MF_GBX_SHOCK_OVER, MF_GBX_SHOCK_P_MAX` inside functions
   with the `g->p` / `g->w` values. Shift cost = `g->p.shift_share / MF_GBX_SHIFT_REF_PS`. Keep the old #defines
   (other files / tests may read them). No other behaviour change.

File `tools/gearbox_model_test.c` (keep all existing tests; add):
- `t_era_map`: 1950->1, 1969->1, 1970->2, 1988->2, 1989->3, 1997->3, 1998->4, 2003->4, 2004->5, 2013->5, 2014->6,
  2030->6, 0->4, 1800->4.
- `t_era_rates`: for each era 1-6, `mf_gbx_set_era` then the REF pace as `t_rates` does: rate 0.97..1.03 (E1).
- `t_era_mix`: for each era, the `t_mix` procedure; shares within 0.05 (0.04 for targets <= 0.15) of the table (E3).
- `t_era_default`: a fresh `mf_gbx_init` model has era 4 and `w[]` equal to the old `MF_GBX_W_*` within 1e-12.
- `t_era_names`: era 1 actuator name "gear linkage", era 4 "shift actuator".
Gate: `gcc -O2 -Wall -Wextra -Werror -I dll tools\gearbox_model_test.c` -> ALL PASS (39 old + new). STOP for review.

## 5. Batch H2 - integration (after H1 review)

New `dll/gbx_era.h`, included by gear_failure.c right after `gbx_sim_core.h` (before `gbx_sim_pump.h`):
- `#define GBX_OFF_VEHPATH 0x4714u`; `static int gbx_veh_year(uintptr_t veh)`: copy up to 259 bytes of the inline
  path (stop at NUL; `IsBadReadPtr` guard; must end in `.veh`, case-insensitive, else 0); read the file with
  `CreateFileA`/`ReadFile` capped at 64 KB into a static buffer; parse per section 2. Worker thread only. Any failure
  returns 0. No allocation, no CRT file API.
- `static int g_gbx_era_ini = 0;` loaded in `gbx_ini_load` (`[Gearbox] Era`, 0..6 else 0).
- `static int gbx_era_pick(uintptr_t veh, int *year_out)`: INI override if 1..6 (year still read for the log); else
  `mf_gbx_era_for_year(year)`.

`dll/gbx_sim_pump.h`:
- Player: in `gbx_session_reset`, after `mf_gbx_init(&g_gbx_pl, ...)`: `mf_gbx_set_era(&g_gbx_pl,
  gbx_era_pick(VEH_BASE, &g_gbx_pl_year))` + log `GBX: player year %d era %d (%s)`. New static `int g_gbx_pl_year`.
- AI: `GbxAi` gets `int year;`. In `gbx_ai_slot` where `a->gen` is re-initialised (after `mf_gbx_init`):
  `mf_gbx_set_era(&a->m, gbx_era_pick(veh, &a->year))`; log once with the name (after `gbx_ai_name`):
  `GBX AI: slot %d %s year %d era %d (%s)`.
- Every log/panel use of `mf_gbx_fault_name(x)` for a model fault -> `mf_gbx_fault_name_era(x, m->era)`.

`dll/gbx_sim_panel.h` (ABI 2):
- `GBXS_ABI 2`; `GbxsCar` gets `int32_t era, year;` inserted right BEFORE `char name[24]`; static asserts become
  53 x 4 and `(33 + 53 * GBX_AI_MAX) * 4`. `gbxs_fill`/`gbxs_pump` write `era` (`m->era`) and `year` (player
  `g_gbx_pl_year`, AI `a->year`).

`tools/mech_failure_test_gui.py`: `GBXS_ABI = 2`; `GBXS_CAR` gets `"era", "year"` before `"name"`; size assert
`(33 + 53 * 24) * 4`; `assert len(GBXS_CAR) == 53`; add `GBXS_ERAS = {1: "MANUAL EARLY", 2: "MANUAL LATE",
3: "SEMI-AUTO EARLY", 4: "SEMI-AUTO", 5: "SEAMLESS", 6: "HYBRID 8-SPEED"}`.
`tools/mech_test_panel.py`: `GbxPageMixin.build_live` adds `self.v_era = Value(box, "Gearbox era")`; `fill_live`
sets `f"{core.GBXS_ERAS.get(era, '?')}  ({year or 'year ?'})"`; `Gbxs.fault_text` names fault 3 "GEAR LINKAGE"
when era <= 2.

Gates: model test ALL PASS; gear TU compiles dev and non-dev with `-Werror`; full `build_merged.bat` (NOT install) OK;
`python -B tools/mech_test_panel.py --smoke-test` PASS; `%TEMP%\gbx_pages\fake_test.py` adapted to ABI 2 passes.
STOP for review. Install only by Claude after review, game closed.

## 6. Risks / abort

- A wrong `veh+0x4714` read is only a log + default era (no game write). Abort H2 if the path string is not inline on
  the first live read (log shows year 0 for every car) - then fall back to the panel AiReader method.
- E4 drift (old tests fail) = H1 abort, fix before H2.
- Restoration: before-states `*.BEFORE_20261012_g7` of every edited file.

## 7. Live test (owner, after install)

1. Log shows `GBX: player year ... era ...` and one line per AI car with the right year.
2. Panel pages 3/4 show "Gearbox era".
3. `[Gearbox] Era=1` in the INI: a forced fault 3 reads GEAR LINKAGE; lost gear spits to N more often.

## 8. Results

- H1 (Haiku, reviewed by Claude 2026-10-12): `mf_gearbox_model.h` + `tools/gearbox_model_test.c` per section 4.
  Diff reviewed line by line: only the listed constants moved to `g->p`/`g->w`, table = section 3. Gate re-run by
  Claude: 105 passed, 0 failed (39 old + 66 new); reference rate 0.995 s/s in every era (E1); E4 unchanged (old
  t_mix/t_rates numbers identical); gear TU compiles dev + non-dev `-Werror`. Builder notes: out-of-range year -> era 4;
  `mf_gbx_era_name` invalid -> "unknown"; test prints `ALL PASS` now. Before-states `*.BEFORE_20261012_g7`.
- H2 (Haiku, reviewed by Claude 2026-10-12): new `dll/gbx_era.h`; `gbx_sim_pump.h` (INI Era, player + AI era
  per car with year log, era-aware fault names), `gbx_sim_panel.h` ABI 2 (era, year before name, 53 fields),
  one include in `gear_failure.c`; panel field map ABI 2 + "Gearbox era" row + GEAR LINKAGE name. Builder gates: model
  ALL PASS, TU dev/non-dev OK, %TEMP% merged build OK, panel smoke PASS, fake block test ABI 2 shows "SEAMLESS (2005)".
  Claude review: diff matches section 5; extra guards accepted (printable path, >= 5 chars, name-year range); FIX by
  Claude: the file-name year fallback now also runs when the .veh cannot be opened/read. Re-checked TU dev + non-dev,
  smoke PASS. Merged build `3281c08ef6327bbe...` in dll/ - NOT installed (game running). Live test pending (section 7);
  unverified: veh+0x4714 path readable by CreateFileA (relative vs absolute path) - the log will show year 0 if not.

Installed 2026-10-12 (game closed): G7 build, hash equal after copy (see sha above in session: 3281c08e...). Live test pending (section 7).
- Live 2026-10-12 (owner race): every car logged `year 0 era 4`; panel "SEMI-AUTO (year ?)". The path itself is
  readable (engine GRADE SRC reads `SEASONDATA\VEHICLES\...\2002_SAUBER08.VEH` the same session): INFERENCE - the
  inline path at veh+0x4714 is still empty when the gearbox slot/session initialises. FIX (Claude): bounded retry each
  pump (GBX_YEAR_TRIES 400 = ~12 s) for player and AI until a year reads; era switched only while no fault runs.
  Installed `6e0df3e65ac1b05f...` (gbx_sim_pump.h `.BEFORE_20261012_yearretry`). Live re-check pending.

- 2026-10-12: owner pressed Lightloader UPDATE for Mechanical Failures -> installed DLL replaced by release `536e0044...` (INI untouched). Dev build reinstalled by Claude: `6e0df3e6...`. Note: a Lightloader update overwrites the dev build.
- Live 2026-10-12 (owner): forced AI faults OK, but the retry build still logged year 0 for every car. ROOT CAUSE
  (FACT, offline test against the real 2002_SAUBER08.VEH): Haiku's extension check in `gbx_read_veh_path` compared
  the last char with 'e' (".vee"), rejecting every path; Claude's H2 review missed it. Fixed ('h'); offline test
  now reads year 2002. Build `1fb03b7c4245d96d...` in dll/ - NOT installed (game running). The section 9 retry stays (harmless).
