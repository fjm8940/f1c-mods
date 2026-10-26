# H8 orphans: design summary

Batch: `H8_orphans` (46 clips in `data/clip_assignments.json`). Data: `data/design/H8_orphans.json`.
Method: static only. SDT header and frame walk, static read of `word_5BEBB0` from the exe `.data`, `full_event_dump.json`, decompiled code, owner notes. No live game work.

## Main finding: the 'orphans' are mostly second clips of a shared score

`word_5BEBB0[score]` is the base SDT index. Its count is `word_5BEBB0[score+1] - word_5BEBB0[score]`. Static read of `modified f1 challenge 99-02.exe` (file offset 0x1BEBB0) agrees with all 480 PWS event pairs in `full_event_dump.json`. Tier CONFIRMED (static file bytes + dump).

Eight of the eleven real orphans are the **second** clip of a two-clip score. The dump only records the base, so they look orphaned. Playing them needs the random roll from `RandomRangeTableInitializer` (`functions/RandomRangeTableInitializer_00411ab0.c`). Its reader is not located, so it is UNKNOWN whether the game ever plays the second clip. The EVF DUMP_RESULTS note "variant B orphaned, blog assumed both play" points the same way.

## Per-clip table (real orphans; MF rows in the JSON)

| Clip | SDT | Says | Technique | Trigger (one line) | Blocker |
|---|---|---|---|---|---|
| PWS_OFFROAD02 | 117 | UNKNOWN - owner must listen (label: Player went off track (2)) | carrier_score (roll-only: 2nd of score 114) | player leaves asphalt | no off-track predicate; roll reader |
| PWS_RETIRE03 | 119 | UNKNOWN - owner must listen (label: Player retired, generic 2) | carrier_score (roll-only: 2nd of score 115) | owner: terminal accident damage | terminal-damage read; roll reader; retire-emitter conflict |
| PWS_SCAR04 | 141 | "safety car is in" (pace_probe.c:559; owner isolation test CLAUDE.md:248) | owned_by_other_mod (full_safety_car pace_probe.c:4875) | safety car enters pit lane | none; the "inherited from SDT 137" note is wrong |
| PWS_2001_110 | 177 | UNKNOWN - owner must listen (label: Player wins race) | carrier_score (roll-only: 2nd of score 170) | player finishes P1 | end-of-race predicate; roll reader; may already play |
| PWS_AIINCID02 | 242 | UNKNOWN - owner must listen (owner: AI off, sector 1/2/3) | carrier_score (roll-only: 2nd of score 231) | AI leaves track | no AI off-track detector; AI subject id |
| PWS_AIINCID05 | 247 | UNKNOWN - owner must listen (owner: has spun) | carrier_score (roll-only: 2nd of score 235) | AI spins | no spin detector (owner's 5 km/h idea unverified) |
| PWS_2001_219 | 302 | owner quote, unverified: "its starting to rain but I don't think it's gonna turn any worse" (TRIAGE_NOTES:278) | carrier_score (roll-only: 2nd of score 286) | rain starts, light | weather predicate (see H6); roll reader |
| RE_PORSCHE_23 | 312 | UNKNOWN - owner must listen (label: Porsche Cup line 23) | carrier_score (base of score 294, count 3) | UNKNOWN | no trigger or context in corpus |
| RE_PORSCHE_24 | 313 | UNKNOWN - owner must listen | carrier_score (roll-only: score 294) | UNKNOWN | as above; roll reader |
| RE_PORSCHE_25 | 314 | UNKNOWN - owner must listen | carrier_score (roll-only: score 294) | UNKNOWN | as above; roll reader |
| RE_PORSCHE_03 | 315 | UNKNOWN - owner must listen | carrier_score (exact: score 295, count 1) | UNKNOWN | as above |

**MF rows.** `PWS_MF01`-`MF35` (SDT 316-350) are `owned_by_other_mod` (mechanical_failures). Their text is the MANIFEST column 6 (`development/mods/mechanical_failures/audio/engineer/MANIFEST.csv:2-36`). Not in the vanilla SDT (count 316). Present in the mechanical_failures build (`audio/engineer/build/PC_RE_MAIN.SDT`, count 351). Excluded from the listening checklist.

**Counts.** carrier_score 10 (of which 2 deterministic: RE_PORSCHE_23 base and RE_PORSCHE_03; 8 roll-dependent). owned_by_other_mod 36 (SCAR04 + 35 MF). cooldown_unblock 0, hook_native 0, skip 0. Nothing is designed to play yet.

## Score reachability (static table)

- SCAR04 = score 135 exactly. Proven by full_safety_car, owner-tested.
- RE_PORSCHE_03 = score 295 exactly. RE_PORSCHE_23 = base of score 294.
- All others are second clips of a two-clip score, so they need the roll reader.
- Durations (DERIVED, MPEG frame walk, MPEG-2 Layer III 22050 Hz 48 kbps mono): 117 2.35 s, 119 1.20 s, 141 1.25 s, 177 2.27 s, 242 1.49 s, 247 0.86 s, 302 2.77 s, 312 1.46 s, 313 1.91 s, 314 0.99 s, 315 1.33 s. Not measured by listening.
- Note: the `.mp2` extension in the SDT names does not match the content (Layer III).

## Shared state/hooks this batch needs

1. The roll reader: find what reads the per-eid (count, offset) pairs written by `sub_411AB0` (`functions/RandomRangeTableInitializer_00411ab0.c`). Until it is found, do not design any second-clip carrier.
2. Carrier eids: a dead PWS eid (1..290) with its score rewritten. Not chosen. Must not be eid 132 (full_safety_car owns it).
3. Triggers with no detector in the corpus: AI off-track (+ sector), AI spin, player off-asphalt, terminal damage, end-of-race win, rain start. Owner-proposed predicates are not verified.

## Contradictions with prior art

- clip_assignments / PROGRESS / VOICE_INDEX: "SCAR04 inherited from SDT 137". SDT 137 is PWS_SCAR05. Fix the note.
- ENGINEER_VOICE_INTEGRATION_2026-10-05.md:141-143: scores 294..299 are "free". The table holds RE_PORSCHE at 294 and 295. "Free" only in the native sense.
- OBSERVER_TABLE.md:181: retire emitter unreachable. Dump eid 117 (RETIRE02) is not blocked.
- TRIAGE_NOTES.txt:164: "check if loaded by the game already" for 2001_110. Answer (DERIVED): it is the second clip of score 170 (base WIN19).

## Open questions

1. Who reads the roll offset? Is the second clip ever played? A live test answers it (see checklist).
2. What is the Porsche Cup context, and who speaks the RE_PORSCHE lines?
3. Which weather predicate covers 2001_219? (H6 batch.)
4. Pending validation: all listening items; all "says" fields except SCAR04 are unverified.
