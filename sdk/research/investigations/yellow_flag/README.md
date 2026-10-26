# Yellow flags / track flag-zone state — SOLVED 2026-07-25

**Supersedes a "confirmed absent" claim.** `mods/f1cui_web_remote/README.md`
(Known Limitations) stated that no offset, struct field or function anywhere in
the corpus tracks zone-level flag state, and that `ShortComputation_bad0` was
"an opaque black-box boolean, not traced to memory". It is neither opaque nor
absent — it is a table lookup on the **Steward** object, and it took one
disassembly of a 12-instruction function to see it.

Found while a yellow flag was actually flying in-game, so every value below is
against a labelled positive sample.

## Terminology — these are NOT the three timing sectors

A lap has three **timing sectors** (S1/S2/S3), which the game already exposes per
car (`+0x4050`, `+0x4054`, and best/last variants). The table documented here is a
different thing: **29 marshalling flag zones** of ~199 m each, covering the lap.
Real circuits work the same way — a handful of timing sectors, dozens of marshal
posts. Call them zones; calling them "sectors" collides with the sector times
shown right next to them in any timing display.

## The chain

```
HUDFlagRendererMain (0x004D82D0)      draws flag byte 62 = yellow
  └─ ShortComputation_bad0 (0x0049BAD0)
       v1 = DistanceProjectionCalc(dword_7C54E0 + 12)   ; player's track distance
       return RingBufferAccessWithWrap(v1) == 2         ; segment idx-1, wrapping
           || IndexedFloatArrayLookup(v1)  == 2         ; segment idx, clamped
```

The decompiler drops the `this` pointer on those two `__thiscall`s. The
disassembly does not:

```
0x49bad1  mov eax, dword ptr [0x7c54e0]   ; player vehicle
0x49bad7  lea ecx, [eax + 0xc]
0x49bada  call 0x4f0050                   ; DistanceProjectionCalc -> distance
0x49bae8  mov ecx, 0x8acee0               ; <<< the Steward
0x49baed  call 0x4d1cf0                   ; state at segment (idx - 1)
0x49baf2  cmp eax, 2                      ; 2 == YELLOW
0x49baf8  mov ecx, 0x8acee0
0x49bafd  call 0x4d1d40                   ; state at segment (idx)
0x49bb02  cmp eax, 2
```

`0x8ACEE0` is the object every `Steward_Init` / `Steward_Setup` / `Steward_Post`
/ `Steward_Restart` takes. **`docs/26` mislabels it `ForceFeedbackBase`** — and that name is not community
runtime data at all: docs/26's name column is explicitly "already documented in
docs/12", and docs/12 is Ghidra+LLM naming inherited from the deprecated
`F1C_ghidra_project`. Treat every name sourced that way as GUESS tier.

## Layout (Steward @ 0x008ACEE0)

| Offset | Type | Meaning | Live value |
|---|---|---|---|
| `+0x108` | float | scale; `distance * scale` = segment index | 0.0050162 |
| `+0x10C` | int32 | segment count | 29 |
| `+0x110` | ptr | array of `{ int32 state; float end_distance }`, stride 8 | — |
| `+0x114` | ptr | array of int32 — the game's OWN previous-state copy, which it diffs to fire flag events | — |

`1 / 0.0050162 = 199.35` units per segment; `29 * 199.35 = 5781.3` = track
length. `end_distance[i] == (i+1) * 199.35`, so the second dword is the
segment's far edge, not a separate flag.

Sample taken while yellow was flying, one segment lit:

```
cur : 00002000000000000000000000000    <- segment 4 == 2
prev: 00002000000000000000000000000
```

## State values

| Value | Meaning | Trust |
|---|---|---|
| 0 | green / clear | OBSERVED |
| 2 | **yellow** | OBSERVED + static (`cmp eax, 2` in the yellow branch) |

The field is **6 bits wide**, not a bool: the change-notify path encodes each
event as `state | (segment << 6)`, and `ParseMidiBitstream` (0x004D1A50) decodes
the same word as `segment = w >> 6`, `state = w & 0x3F`. Values other than 0 and
2 have not been seen; anything else is reported raw rather than guessed at.

## Who writes it — `sub_4D1730`, every frame

Called from the scene update (`SceneRenderUpdate` / `sub_4C6E80`) with the
Steward. Per call it:

1. **Clears every segment to 0.**
2. For each car with `FinishState (+0x4064) != 2`, computes a minimum-speed
   threshold from the car's physics object (`veh+0x3AA8`, fields `+140`/`+300`,
   with a race-start relaxation gated on `SessionType == 7 && RacePhase == 5`),
   and if the car's forward velocity is below it, sets **that car's segment
   (index − 1, wrapped) to 2**.
3. Diffs the result against the `+0x114` copy and, on any change, calls
   `PitSelectionStateMachine` and encodes the `state | (segment << 6)` events.

So a yellow means **"a car is going too slowly in this sector"** — the real
rule, not a scripted event. And because the table is rebuilt from scratch every
frame, **poking a segment cannot survive a single frame**: this is a read-only
observable, not a mod surface. Anyone wanting to *force* a yellow must hook
`sub_4D1730` or make a car genuinely slow.

## Consumed by

`mods/f1cui_web_remote` (`web_telemetry.c`, `flags_read` / `flags_apply_to_car`)
publishes:

- `sg` — one digit per segment, start line first; `sgn`, `sgl`, `trkl`, `yc`
- per car `sgi` (flag-zone index, from `track_pos / segment_length`) and `yf`,
  which applies the game's own display rule (this segment **or** the one behind)
  to every car rather than only to the player
- Race Control bulletins on the 0↔2 edges, and a flag strip in the session pane

`track_pos` (`veh+0x06CC`) is confirmed to be **distance along the track in the
same units** — the player at 1261 lands in segment 6 (`1261 * 0.0050162 = 6.3`),
consistent with the cars' visible positions.

## Still open

- The other 61 possible state values. Blue/black/white flags are drawn by
  `VehicleFlagCheck` / `DisqualificationCheck` / `PitStateFlagCheck`, which are
  separate per-car predicates and do **not** come from this table.
- The exact speed threshold constants in `sub_4D1730` (`veh+0x3AA8 +140`, `+300`,
  and `Steward+212` / `+176`) are unmapped; only their role is established.
