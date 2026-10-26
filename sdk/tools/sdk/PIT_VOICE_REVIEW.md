# Pit commentary enqueue — 2026-09-21

## Player pit-retirement extension — 2026-09-22

EID117 (PWS_RETIRE02, "and that's the end of your race") is admitted for the
player terminal pit-failure path. The older player-retirement patch records it
as heard in game on 2026-07-28, and the original pit-failure mod uses the same
event immediately before its retire-to-garage sequence. It uses the existing
player subject `-1`, four-second lifetime and native queue gates. The pit-stop
adapter requests it once only after the broker accepts the terminal garage
handoff.

## Retirement extension (supersedes the earlier absence below)

EID237 is now admitted for caller-confirmed AI retirement only. Native raw
0041B250 uses 237 for generic AI retirement, vehicle+489C as named subject and
secondary context3 by default. The archived voice_browser.py human catalogue
labels AIRETIRE02 generic AI retirement; full_event_dump.json maps it to 237.
The live client waits for DNF/garage proof and attempts once. Retirement TTL20s
allows bounded waiting behind busy radio; other events retain TTL4s. This changes
neither global priorities/cooldowns nor settings. Return1 still means queued.
[Tests and native listening limits](../../../development/mods/pitstop_modding/verification/retirement-voice-2026-09-21/REPORT.md).

Narrow exact-target SDK helper f1c_pit_voice.h. Broker game-thread callers only;
requires existing validated target lifecycle. Calls native formatter and insert,
does not overwrite queue head or bypass settings/cooldowns. Return 1 means queue
acceptance, never audible playback. Event TTL is four simulation seconds and
delay zero so stale release cues are not deliberately kept indefinitely.

Raw evidence read through canonical inspect: StructInitWithSentinelValues
004144D0 initializes the eight-word record; DriverDataTableLookupAndTimeConversion
004144F0 fills eid, subject at word4, remaining context and timing. This function
is called CommentaryEventQueue in existing SDK macros, but is only a formatter.
InsertDriverTableEntry 00418560 inserts into controller 005EA618, returns char,
and respects ScoreThresholdGateCheck 00418B90. Native callers AIPitStrategyState,
AIOutOfTrackEvent, PitRaceStatusMessageDispatcher establish controller/ABI/gates.
AIOutOfTrackEvent passes dword_5F0F74[4656*slot] as the first subject parameter.
Subtracting VEH_BASE 005EC6D8 gives 489C, the existing VEHF_DriverID field.
Use that ID, not slot/rank/player identity. Player cues use subject -1.

The existing SDK globals G_RaceActiveFlag and G_TrackConditionState are used
only with the same integer/float gates as native commentary callers; their
historical names do not imply these are commentary-specific settings.

EID allowlist and product event mappings are inherited alpha source/catalogue
evidence, not newly verified listening: player 51/53/45/43 lane/brake/go/exit;
63/65/68/70 corner trouble, 62/64/67/69 corner clear, 61 wheelgun, 56 fuel,
54 stall, 66 jam. AI 183 generic, 184 stall, 185 fuel, 186..189 wheel issues.
Product fixture tests exercise mappings and subject forwarding. Native listening,
queue refusal/expiry behavior and simultaneous-driver playback remain to verify.
No retirement event is admitted: native retirement/garage provider is absent.

Product evidence: ../../../development/mods/pitstop_modding/verification/voice-2026-09-21/REPORT.md.
