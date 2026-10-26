# Extended Race Engineer — "plays right now" audit: owner report (2026-10-19)

## Next action for you

- Open the voice browser (`tools\browse_voices.bat`) and use the new **Audit verdict** chips
  (Native active / Mod only / Not activated / Unknown / Suppressed). Click a row: the right pane shows verdict,
  reason, suppressed note and the play plan.
- Then decide: start Phase B (core DLL spec), or do the listening checks below first. They can wait for the first
  big live test.

## Result (all 351 clips: 316 in PC_RE_MAIN.SDT + 35 MF clips)

| Verdict | Clips | Meaning |
|---|---:|---|
| NATIVE_ACTIVE | 94 | The game itself plays it when the situation happens |
| MOD_ONLY | 101 | Only a first-party mod plays it: 73 with the mod on in your game, 28 with the mod off or dev-only |
| NOT_ACTIVATED | 130 | Proven never played: 104 no emitter, 18 orphan clips, 3 blocked gates, rest cooldown or unreachable |
| UNKNOWN | 26 | One fact missing (listed below) |
| Suppressed trigger | 23 | Flag across buckets: the game has the trigger but was written not to play the line |

- Every row has a `checked` list (functions read). 186 rows have a play plan (how ERE would play the line).
- Stitch re-check changed 41 verdicts. Rule now proven: sub-clips come from `pc_re_stitch.sdt`, never from
  `pc_re_main.sdt`. So no main clip plays "as a piece" of another line.
- Evidence tier: static code reading by Haiku workers, Claude-reviewed with spot checks. Not live-proven per line.

## The old "Vanilla" column is stale (fact from the merge)

The browser's Vanilla column still comes from the 2026-10-07 EVF data. The audit disagrees for 40 clips:

- 17 marked vanilla but NOT_ACTIVATED: SDT 32, 115, 116, 123, 124, 149, 151, 154, 189, 191, 211, 237, 253, 277,
  290, 304, 305.
- 23 marked not vanilla but NATIVE_ACTIVE: SDT 0, 93-100 (corner blister lines), 122, 158 (RE_PD_DQ), 163, 177,
  221, 224-230 (AI mechanical lines), 270, 275.
- Trust the Verdict column. Vanilla stays as history; nothing was overwritten.

## Suppressed trigger exists (23)

| SDT | Clip | Verdict | What blocks it |
|---|---|---|---|
| 33 | RE_30_MINS_LEFT | UNKNOWN | PitTimer calls the time-left chain only in sessions 1-6, not the race |
| 34 | RE_45_MINS_LEFT | UNKNOWN | same as 33 |
| 105-110 | PWS_HIT03/04/01/05/06/02 | MOD_ONLY | FLT_MAX cooldown in a stock exe; Extended Damage Commentary plays them (your exe also has 15 s cooldowns baked in) |
| 112 | RE_PD_STALLED_T | NOT_ACTIVATED | FLT_MAX cooldown on eid 113 |
| 118 | PWS_RETIRE02 | MOD_ONLY | FLT_MAX cooldown + unreachable arm; Pit Stop Failures plays it |
| 119 | PWS_RETIRE03 | UNKNOWN | second clip of eid 117's score; picker unproven |
| 120 | PWS_RETIRE01 | NOT_ACTIVATED | sub_4F1FE0 returns first; the game's own retirement closes the 10 s dwell before it can play |
| 121 | PWS_RETIRE04 | MOD_ONLY | same arm as 120; mod plays it |
| 130 | PWS_GREEN01 | UNKNOWN | flag-clear gate off while racing (phase 5); phases 6/7 open |
| 135 | PWS_RED01 | MOD_ONLY | zone state 3 has no native writer; Red Flag mod (disabled in your game) |
| 142, 144, 146 | PWS_YELLOW04/05/06 | NOT_ACTIVATED | `sub_41B3C0:36` drops flag-clear lines while racing |
| 259 | PWS_AIPENALTY22 | UNKNOWN | AI stop/go counter producer unproven (DD-10 excludes AI penalty lines anyway) |
| 268, 269 | PWS_AIPENALTY04/13 | UNKNOWN | AI branch only gets the player tracker (DD-10 excludes) |
| 272 | PWS_AIPENALTY01 | UNKNOWN | speeding check runs on slot 0 only (DD-10 excludes) |
| 279 | PWS_AIFASTLAP01 | UNKNOWN | emit gated to slot 0 |

## UNKNOWN (26) — what is missing

- SDT 1, 2, 3, 8, 31 (qualifying best-time lines, eids 3/4/5/10/33): argument order at `LapCompletion:53`.
  Needs an IDA re-check or one live isolation test.
- SDT 20 (2001_21): emitter proven, reachability not.
- SDT 33, 34: time-left lines, see the suppressed table.
- SDT 117 (OFFROAD02), 119 (RETIRE03): second clips at a shared score; roll tests.
- SDT 130 (GREEN01): race phases 6/7.
- SDT 192, 193 (AIPIT08/09): live input count unproven.
- SDT 238, 239 (AIINCID10/11): threshold unresolved.
- SDT 251 (AIRETIRE03): AI source of ReasonCode 9.
- SDT 259, 268, 269, 272, 279: AI/slot-0 gates (see table above).
- SDT 293-297 (2001_155..159): qualifying globals and countdown.

## Live and listening checks (for the first big test)

1. **SDT 302 drizzle — resolved, no in-game test needed.** "Owner heard" in voice_index came from your
   browser preview (H8 listening, 2026-10-18), not from a race. No game route plays it; its base eid 283
   has no emitter either. ERE must give it its own emitter (rain start, light).
2. **Win lines SDT 161-177:** the code picks the win line by track name; for about 7 clips the code's track
   does not match the doc 56 label (batches B18, B19). Listen to each in the browser and say which track it
   names. SDT 177 ("You won, great drive, well done") alternates with 176 in round robin.
3. **LapCompletion:53 argument order** (eids 3/4/5/10/33): in qualifying, set a personal best sector / lap and
   note which line plays.
4. **The 6 roll tests** in `docs/design/H8_LISTEN_CHECKLIST.md` (OFFROAD02, RETIRE03, 2001_110, AIINCID02,
   AIINCID05, 2001_219). Note: 2001_219 and AIINCID02/05 cannot play natively (their base eids have no
   emitter), so only OFFROAD02, RETIRE03 and 2001_110 are worth testing.
5. **RETIRE03 via Pit Stop Failures:** the pit mod queues eid 117 (score 115). If round robin applies, a second
   pit retirement may play RETIRE03 instead of RETIRE02. Listen across two retirements (inference, not proven).
6. **Eid 237** (AI retire generic) is queued by the game and by Pit Stop Failures: listen for a double line.
7. **SDT 2001_21** (SDT 20) reachability, and flag-clear lines during a non-racing phase (GREEN01).

## Owner decisions still open (from the batches)

- B16: MISC05, MISC47, MISC02, MISC03 are not covered by DD-11 — play or skip?
- B21: eid 397 (shared generic) maps to PWS_QUAL32 — keep?

## Where everything is

- Verdicts per clip: `data/audit/B*.json` (full rows), merged summary in `data/voice_index.json` field `audit`.
- Browser: `tools/browse_voices.bat`. Generated table: `docs/VOICE_INDEX.md` (new Verdict / Reason / Play plan
  columns + suppressed table).
- Findings added to `f1c_ida_decompiled/findings.jsonl`: 11 rows `ere-*` dated 2026-10-19 (backup `findings.jsonl.BEFORE_20261019_ere_audit`).
