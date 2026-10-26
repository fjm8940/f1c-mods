# "Plays right now" audit — worker brief (owner request 2026-10-18)

Owner question: **which race-engineer lines does the game play RIGHT NOW — if I manage to cause the
situation, the line is heard?** Answer per eid, with code evidence. Earlier tables guessed this from a
blog and from "an event exists"; the owner found them wrong. Your job is to prove each verdict.

Read `docs/HAIKU_BRIEF.md` first (paths, tools, hard rules 1-6 all apply: read only, no live game, cite
every number, label tiers, UTF-8). This file overrides its "Output" section.

## Scope

Your eid range is in your task. PWS (race engineer) eids only. Source list of eids: EVF
`data/full_event_dump.json` (clip, score, cooldown, blocked per eid). Clip text: doc 56.

## The owner's current game (judge "via mod" against THIS)

`%USERPROFILE%\Documents\F1 Challenge 99-02 (V10)\plugins\lightloader\` — loaded: extended_damage_commentary,
mechanical_failures, pitstop_failures, multiplayer_overhaul, esc_pause_menu, camera_editor, grid_restore,
qualifying_preservation, race_data_collector, web_telemetry, render_vertex_optimizer, ai_brain_lab.
DISABLED (LightloaderDisabled.ini): redflag, safetycar, adaptive_ai_difficulty. **The exe is NOT stock (coordinator check 2026-10-18, file sha256 1a70e5ed...):** event-table cooldowns on disk read eids 106-111 = 15.0 and 117-119 = 6.0 (EVF unblock patches baked into the exe), eid 80 = FLT_MAX. Read a cooldown from the owner's exe, not from a clean exe: table `0x5BF9A0 + eid*0x14 + 0x0C`. Also: clips can be reached as SUB-CLIPS (see dossier `subclip_resolvers`): FontRenderingPrep 0x411840 -> SoundEventDispatcher 0x411E90 builds each line's clip list by clip type; e.g. EnumValueTranslation 0x414010 (types 0x11/0x34) appends SDT clip 113+(enum-1) (113..134). A clip with no eid of its own may still play this way — check it.
Census of mod lines: `ERE/data/modded_lines.json`. **Installed Pit Stop Failures = 1.1.4-rc.1 (hash-checked); its source is `development/mods/pitstop_modding/release_candidates/pitstop_failures-v1.1.4-rc.1/source/` (`pit_live_voice.h`, `live_client.cpp`) — judge pit lines against THAT tree, not `live/` or `dll/`. A quiet log does not prove a line is not played.** **Installed Mechanical Failures = release 1.0.7 (hash-checked), ENGINE family only: its gearbox (eids 80-93), suspension (75) and electronics (76) lines are DEV-ONLY and do NOT play in the owner's game → MOD_ONLY with mod_active_in_owner_game=false.** **mechanism `token_only` = the mod borrows the eid for
its OWN audio; the vanilla clip is NOT played — never count that as playing.**

## Method per eid

1. Who emits it: `data/emitter_index.json`, then grep `f1c_ida_decompiled/functions/*.c` for the literal
   eid in `DriverDataTableLookupAndTimeConversion(..., EID, ...)` calls, and for table/dynamic sources
   (DriverNameIDLookup etc.). Note low/high twins.
2. Is the emitter reached? Follow callers (`python tools/f1c_mod.py inspect FUNC` from f1c_ida_decompiled)
   up to a per-frame/session function. A function with no caller, or only a caller that cannot run in
   single player, = unreachable.
3. Gates on the path: session type (5 qual / 7 race), race phase `dword_7C56A8`, player-only checks
   (`dword_7C54E8`), commentary flags `dword_7D0308` / `flt_7CF3C8`, thresholds. Quote the line.
   A gate that can never be true in single player = BLOCKED_GATE.
4. Cooldown: FLT_MAX (`blocked:true`) = BLOCKED_COOLDOWN unless an ACTIVE mod unblocks it.
5. Score → clip: the eid's score must map to the clip the label says (doc 56 / dump).
6. Prior evidence of being heard: EVF `docs/VOICE_LINES.md`, `docs/TRIAGE_NOTES.txt` (owner's notes,
   e.g. "never heard"), findings (`evidence-query findings "<clip>"`), mod verification records. Owner
   "never heard" notes do not override code, but must be cited.


## EXHAUSTIVE PER-LINE METHOD (owner 2026-10-18: "each line analyzed individually and exhaustively throughout the decompiled corpus") — overrides "Method per eid"

Unit of work = ONE SDT clip. Do every step for every clip; never copy a verdict from a neighbour.

0. Open its dossier `ERE/data/line_trace/<sdt>_<clip>.json` (made by `tools/line_trace.py`). It lists: all
   event rows mapping to the clip (twins, shared score), each PWS eid's literal hits inside the 40 game
   functions that queue commentary, eid-lookup tables, other functions containing the number,
   emitters with NON-literal eids, callers up to 4 levels, findings, EVF docs, mod sources.
1. For EVERY emitter hit: open the `.c` file in `f1c_ida_decompiled/functions/` and read the whole
   function. Confirm the number really is the eid argument (not a coincidence). Write the branch
   conditions that lead to that emit.
2. For EVERY function in `dynamic_emitters_to_check_by_hand`: read how its eid variable is built
   (tables, `a4 ? X : Y`, lookups, arithmetic) and decide whether THIS eid can come out. Record yes/no
   + line for each one. This is mandatory — dynamic emitters are where earlier tables went wrong.
3. Check the eid-lookup tables (DriverNameIDLookup, sub_417640, TrackNameDisplayIDLookup, ...) hits:
   do they feed an emitter with this eid?
4. Reachability: walk callers up to a per-frame/session root (`inspect` from f1c_ida_decompiled). Note
   any caller that only runs in multiplayer/replay/menus.
5. Gates: quote every condition between the root and the emit. Decide if each can be true in single
   player. Cooldown from the dump (FLT_MAX = blocked).
6. Shared-score alternates: if the clip is only the second clip at a score, say so (not reachable by
   its own eid; whether the game picks it at random is UNKNOWN unless code proves it).
7. Mods: dossier `mod_sources` + `ERE/data/modded_lines.json` (token_only never counts).
8. Prior evidence: findings, EVF docs, owner notes (TRIAGE_NOTES.txt). Cite; code wins over notes.

Every row MUST include `checked`: a list of {function, result, line} for EVERY function from steps
1-3 (result = "emits this eid under <condition>" / "cannot produce this eid" / "number is not an eid").
A row without a complete `checked` list is rejected.


## PLAY PLAN per line (owner 2026-10-18) — required for every NOT_ACTIVATED row, every row with suppressed_trigger_exists=true, and every MOD_ONLY row whose mod is disabled

Owner: "draft a plan how to get a voiceline (which exists in .SDT but is not playing) playing accordingly,
for example PWS_2001_14, how should it be played? each individually". Add `play_plan` to the row:

```json
"play_plan": {
  "meaning": "what the engineer is telling the driver, in real F1 terms (e.g. 2001_14: in qualifying, the car just ahead on track is on a flying lap — don't block, hold back)",
  "when_it_should_play": "exact real situation, session type, who (player / named AI driver)",
  "trigger": "precise predicate with every state read: broker snapshot field (ERE prefers F1CBrokerVehicleSnapshot / SessionSnapshot fields, see f1c_ida_decompiled/tools/sdk/f1c_broker_abi.h) or offset/global with tier + source",
  "subject": "player (-1) | AI driver id veh+0x489C | team",
  "technique": "lift_suppression (cite the gate/cooldown to change) | own_emitter (ERE queues the PWS eid) | hook_native (call/keep the game's emitter) | carrier_score",
  "latch_cooldown": "once per lap / per session / N s",
  "must_not_play_when": "false-positive cases to exclude",
  "conflicts": "lines/mods that could clash (MF tokens DD-07, SC, pit mod)",
  "calibration": "constant to measure live, or null",
  "live_test": "step by step how the owner makes it happen in game",
  "confidence": "DERIVED / GUESS + what would raise it"
}
```
Coordinator fact: RacePhase `dword_7C56A8 == 5` = green-flag racing (FACT, findings: whole-field governor gated on ==5 held 21 cars ~143 s in a running race). Coordinator rules for plans: (a) "ahead/behind on track" = lap distance `veh+0x4044` (+ lap number, wrap) or world position + heading, NEVER race_position in qualifying; (b) MF 1.0.7 never rewrites event rows — conflict with MF tokens is only "don't queue while an MF line is pending on that eid" (DD-07); (c) out-lap = first lap after a pit_status 4->0 edge, in-lap = pit_status 1 or call_to_box, flying = neither (DERIVED).
Start from the batch designs in `ERE/data/design/H*.json` when they cover the clip, but re-check every
claim for this one clip. Respect owner decisions in `ERE/docs/DESIGN_DECISIONS.md` (DD-09 SLOWCAR01 =
player approaching a slower car; DD-10 no AI penalty lines; DD-11 game-missing infractions excluded —
write play_plan "EXCLUDED by DD-11"; DD-14 Porsche lines not used).

## SUB-CLIP BANK FACT (coordinator, 2026-10-18 late) — overrides every sub-clip note above

Sub-clips NEVER come from `PC_RE_MAIN.SDT`. Proof (static, f1c_ida_decompiled/functions):
- A queued line's MAIN clip: FontCharacterRenderDispatcher 0x411DB0 writes entry+12 =
  GlobalFieldSelectorByIndex 0x414160(ch) = `dword_5BF8C8` (ch 0) / `dword_5BF8D0` (ch 1).
- Every SUB-CLIP builder (sub_412490, sub_412E50, sub_413820, EnumValueTranslation 0x414010,
  MapCollisionShapeToBoneIndex 0x412C30, CollisionShapeSizeClassify 0x4139C0, SeasonSpecificTeamIDLookup
  0x413B60, and the name tables filled via 0x4141A0) writes entry+12 = sub_414180 / sub_4141A0(ch) =
  `dword_5BF8CC` (ch 0) / `dword_5BF8D4` (ch 1).
- AudioCommentaryChannelConfiguration 0x4117B0 pairs ch 0: C8 with path pair off_5BF8D8
  ("Audio\Commentary\Pws\" + "pc_re_main.sdt") and CC with off_5BF8E0 ("Audio\Commentary\Pws\" +
  "pc_re_stitch.sdt"); ch 1 = pc_tv_main / pc_tv_stitch (strings_index.json 0x5BF8D8-0x5BF8F4).
- Cross-check: sub_412490 (ch 0) returns indices up to 387; PC_RE_MAIN has 316 clips, PC_RE_STITCH 494.
  Stitch names at those indices are the words the code needs: 113-134 PWS_ISRUN*, 153-188 PWS_LAPS_*,
  215-250 PWS_NUMB*, 275-292 PWS_POINTS_*, 293-336 PWS_POS*, 337+ PWS_POSX*.
- PWS eids 1-290 are channel 0 (ShortComputation_19d0 0x4119D0); TV eids 293-535 channel 1.

Rule: a `subclip_resolvers` hit or "sub-clip value table" index N is an index into PC_RE_STITCH, NOT
PC_RE_MAIN clip N. It is never evidence that a PC_RE_MAIN line plays. A PC_RE_MAIN clip plays only as the
main clip of its own score row (CharacterCodeMapper 0x411B50: unk_5BEE08 / unk_5BF4C0 tables).
Earlier "plays as sub-clip of X" verdicts and "subclip_route_open" UNKNOWNs must be re-judged without it.

## Verdicts (owner's three buckets, 2026-10-18 — pick exactly one)

- `NATIVE_ACTIVE` — the game itself has a working activation and the line can be played (example:
  PWS_AILAPPIN02-04). Give the situation in plain words.
- `NOT_ACTIVATED` — the clip exists in the SDT but it is PROVEN the game never activates/plays it
  (examples: PWS_AILAPPIN01, PWS_YELLOW04). Give the reason: `no_emitter`, `unreachable_emitter`,
  `blocked_cooldown` (FLT_MAX), `blocked_gate` (quote the line), `orphan_clip`.
- `MOD_ONLY` — not activated by the game, but a first-party mod activates the real clip (all Safety Car
  lines, Mechanical Failures lines, Pit Stop Failures lines, etc.). Count disabled mods too; set
  `mod_active_in_owner_game` true/false. token_only never counts.
- `UNKNOWN` — say exactly what is missing to decide.

Extra flag on every row: `suppressed_trigger_exists` (true/false) + `suppressed_note`. Set true when the
game was written NOT to play the line (FLT_MAX cooldown, a gate that is always false, a dead branch,
a return before the emit) BUT the activation trigger itself exists in code — so in theory it should
play if that suppression were lifted. Cite the trigger and the suppression lines. Example:
PWS_YELLOW04 — clear-flag trigger exists in sub_41B3C0, suppressed by `dword_7C56A8 == 5` while racing.

## Output (only these files)

`ERE/data/audit/<batch>.json` (one row per CLIP, key sdt_index; eid may be null; add "checked":[...]):
```json
{"schema":"ere.audit.v1","range":"A1_1-50","rows":[{"eid":49,"clip":"PWS_PIT01","says":"Pit on this lap",
"score":49,"cooldown":15.0,"verdict":"NOT_ACTIVATED","reason":"no_emitter","situation":"","mod_active_in_owner_game":null,"suppressed_trigger_exists":false,"suppressed_note":"","emitters":["sub_XXXX (file:line)"],
"call_path":"MainGameTickUpdate -> ... -> sub_XXXX","gates":["dword_7C56A4 == 7 (file:line)"],
"mod":"","heard_evidence":"TRIAGE_NOTES.txt:NN 'never heard'","tier":"DERIVED","notes":""}]}
```
`ERE/docs/audit/<range>.md`: one table row per eid (eid, clip, says, verdict, situation or blocker),
then a short list of contradictions with earlier tables (doc 56 status, progress.json, voice_index).

Final reply max 12 lines: count per verdict, top surprises.
