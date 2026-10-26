# Read-only race-data observation — 2026-09-14

Consumer: [Race Data Collector](../../../development/mods/race_data_collector/README.md).
Interface: `f1c_race_data_observation.h`, a bounded pure decoder over an already
copied vehicle buffer. It does not inspect a process, install hooks, write game
state or call native functions. Caller must establish exact target identity,
current session/object handles and a broker callback on the native thread.

Existing fuel/efficiency, pit-timer, qualifying, ratings, tyre-choice, crossing,
engine and vehicle-path fields retain the evidence tiers in `f1c_sdk.h`.
FuelLevel remains DERIVED; no new runtime validation is claimed.

Supplemental field offsets use the archived F1CUI reader from exact JAR SHA256
`a7772c7fb4794f7edc04117cc9e3b36661317d4e132d480ca6b583e8d2601623`.
Its anchor is vehicle + 0x4714; subtraction constants and read types were
inspected directly in Java bytecode:

| Field | Subtraction | Buffer offset | Read type |
|---|---:|---:|---|
| last lap | 1736 | 0x404C | float |
| last I1 | 1732 | 0x4050 | float |
| last I2 | 1728 | 0x4054 | float |
| best I1 | 1652 | 0x40A0 | float |
| best I2 | 1648 | 0x40A4 | float |
| hit counter | 13408 | 0x12B4 | int32 |
| track-limit counter | 1552 | 0x4104 | int32 |
| track fraction | 1740 | 0x4048 | float |

Tier: OBSERVED community read contract, not new native proof. The collector
preserves raw timing values and distinguishes uncertain sector interpretation.
The native FinishState value 2/3 is DNF/DSQ; the Java reader's unaligned int
at vehicle+0x4063 exposes the corresponding 512/768 values. Do not copy those
shifted Java constants into a reader of the aligned SDK FinishState field.

The plugin samples existing reviewed `G_SessionClockSeconds` as double,
`G_SessionStartSeconds` as float, `G_AIWFilePathBuffer` as bounded bytes and
`G_ChampionshipMode` as int. Java raw racing mode reads module+3998736
(0x007D0410); 3=SINGLE, 4=CHAMP, 5=MP. Native round identity remains unknown
without a reviewed provider. Track wetness is a 0..1 path-water observation,
not precipitation intensity. Tyre puncture/condition fields are not tyre wear.

Validation: `test_race_data_observation.py` registered in canonical sdk-test;
actual consumer adds deterministic lifecycle/schema tests and real Season
Narrative parser/report integration. Those checks do not establish correct
values during live gameplay. Native exact-field comparison, callback budget,
session end, replay, ESC simulation and all-season checks remain required.
