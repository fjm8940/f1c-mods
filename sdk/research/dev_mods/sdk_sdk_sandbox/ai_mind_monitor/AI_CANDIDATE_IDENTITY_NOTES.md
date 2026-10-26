# AI steering candidate identity map

Status: read-only sandbox evidence, captured from the active race.

`AISteering_004270a0` stores five nearby-car entries beginning at `+0x4174`,
with a 0x44-byte stride. The first dword of each entry was live-read and
matched the relocated vehicle array exactly:

```text
candidate vehicle slot = (entryPointer - vehicleArrayBase) / vehicleStride
```

The live snapshot had zero unreadable reads and resolved pointers to known
live vehicle slots. This establishes that the entries represent nearby vehicle
candidates, not abstract lane coordinates.

Each entry also contains an index at `entry + 0x40` (`+0x41B4`, `+0x41F8`,
`+0x423C`, `+0x4280`, or `+0x42C4`). It maps that candidate to a mode array
at `+0x42D4 + 0x1C * index`. Values `0..4` are valid mode-array indexes;
`0xFFFFFFFF` is the no-candidate sentinel.

This yields the following read-only relation for the GUI:

```text
AI driver -> candidate-entry pointer -> nearby live driver
          -> candidate mode index -> lane/conflict mode
          -> candidate plan index -> racing-line speed-profile slot
```

The mode index and plan index are separate values. The former selects one of
five traffic-mode slots; the latter selects one of the 20 racing-line profile
values used while judging the candidate. Do not display lane-mode slot 0 as
the candidate's mode unless that candidate's own mode index is 0.

Static code shows modes `1`, `2`, `7`, and `8` reach the `Overtaking` call.
The monitor may show these modes next to an identified nearby driver, but it
must still label them as **candidate traffic modes**, not a completed pass:
the candidate-target pointer remains unobserved in the high-frequency capture,
and race-progress confirmation is a separate requirement.

## Event-correlated examples

The 60-second, 20 Hz capture in
`runtime_memory/observations/ai_named_candidate_side_60/correlation.json`
recorded 100 lateral-side transitions. It joined every transition with the
same source driver's candidate pointers, indexes, and lane modes.

Examples from the capture:

- Martin Brundle's `-3 -> 0` side transition coincided with Bertrand Gachot
  in candidate mode `2`.
- A later Brundle `0 -> -3` transition coincided with Gachot in mode `7`.
- A backmarker slot's `0 -> -3` transition coincided with Mika Salo in mode
  `7`.

Modes `2` and `7` are among the static AISteering modes that call
`Overtaking_0042af70`. This is the first event-correlated evidence connecting
a named nearby car to a traffic-side decision. It still does not prove pass
completion or reveal a durable target pointer.

## Mode-transition sequence

The higher-frequency mode-0 capture in
`runtime_memory/observations/ai_lane_mode0_context_90/correlation.json`
recorded 47 natural transitions and resolved the mode-0 owner back to its
named candidate.

One representative chain was:

```text
Pierluigi Martini (slot 17) evaluates Gianni Morbidelli (slot 13)
mode 5 -> 7, then 7 -> 2
lateral side already/then remains -3
```

Both `7` and `2` are modes that statically reach `Overtaking`. The same source
and candidate pairing also changed `2 -> 5`, consistent with leaving the
active maneuver state. This is meaningful evidence for a state-machine-like
traffic decision rather than a single one-frame random side offset. The exact
semantic names of modes 5 and 16 remain unmapped, so they are intentionally
shown as numeric modes only.
