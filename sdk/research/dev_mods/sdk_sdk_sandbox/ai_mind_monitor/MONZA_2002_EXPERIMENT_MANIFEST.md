# Monza 2002 live AI experiment manifest

Date: 2026-08-18

This is a reproducibility record for the elevated, read-only Monza captures.
No capture wrote game memory, injected code, or sent game input.

## Active-session setup

- Season: 2002.
- Track: Monza.
- Race length: 100%.
- AI aggression: 90% for the two `aggression90` captures.
- Active RCD directory: `%USERPROFILE%\Desktop\F1\F1 Challenge V10\SeasonData\Drivers\2002\`.
- Before the RCD edit, all 22 active files exactly matched the existing
  `.f1c-randomizer-backup\drivers\SeasonData\Drivers\2002\` copies.
- `RaceAbility` values were multiplied by the common factor
  `6.2 / 17.13 = 0.36193812025685934`, preserving each original ratio. The
  resulting range is `0.651489` through `6.200000`.

| Driver file | Before | After |
| --- | ---: | ---: |
| AMcNish.rcd | 9.39 | 3.398599 |
| AYoong.rcd | 17.13 | 6.200000 |
| DCoulthard.rcd | 5.73 | 2.073905 |
| EBernoldi.rcd | 12.59 | 4.556801 |
| EIrvine.rcd | 2.92 | 1.056859 |
| FMassa.rcd | 7.14 | 2.584238 |
| GFisichella.rcd | 5.35 | 1.936369 |
| HFrentzen.rcd | 7.54 | 2.729013 |
| JButton.rcd | 9.58 | 3.467367 |
| JMontoya.rcd | 7.18 | 2.598716 |
| JTrulli.rcd | 7.38 | 2.671103 |
| JVilleneuve.rcd | 7.02 | 2.540806 |
| KRaikkonen.rcd | 7.63 | 2.761588 |
| MSalo.rcd | 7.49 | 2.710917 |
| MSchumacher.rcd | 1.80 | 0.651489 |
| MWebber.rcd | 12.94 | 4.683479 |
| NHeidfeld.rcd | 9.32 | 3.373263 |
| OPanis.rcd | 10.56 | 3.822067 |
| PDeLaRosa.rcd | 14.70 | 5.320490 |
| RBarrichello.rcd | 1.80 | 0.651489 |
| RSchumacher.rcd | 3.27 | 1.183538 |
| TSato.rcd | 7.92 | 2.866550 |

## Preserved raw capture inventory

Every capture completed at 600 seconds and has zero unreadable reads.

| Capture tag | Samples | Transitions | Key addition |
| --- | ---: | ---: | --- |
| `monza_2002_pack_pass_join_20260818a` | 47,929 | 257 | Initial packed-field window |
| `monza_2002_race_pass_join_20260818a` | 47,921 | 204 | Race-phase pair snapshots |
| `monza_2002_villeneuve_coulthard_fisichella_20260818a` | 48,084 | 463 | Focused traffic window |
| `monza_2002_aggression90_pass_join_20260818a` | 47,928 | 278 | 90% aggression standing-swap inventory |
| `monza_2002_aggression90_named_pass_join_20260818a` | 47,418 | 135 | Name-resolved target/crossover join |

The aggregate is 239,280 samples and 1,337 transitions. Raw JSON evidence is
kept under `runtime_memory/observations/`; no capture is discarded because it
is incomplete, negative, or superseded.

## Current conclusion and boundary

The named capture joins Räikkönen's transient Coulthard candidate and `-3.0`
lateral value to Coulthard's later position gain. This proves a live,
same-pair candidate/interaction-to-crossover relationship, while falsifying
the stronger claim that the candidate writer must be the eventual passer.

The following are deliberately still open:

1. Exact source-to-target pass direction and the completed-pass success branch.
2. Separation of the Overtaking and proximity/lapping writers to `+0x45FC`.
3. AI-aggression persistence/storage and all consuming code paths.
4. The complete numeric pass-score model and its thresholds.

See `AI_OVERTAKE_CORRELATION_NOTES.md` for the static call-path evidence and
the named Monza join interpretation.
