# Semantic Memory Linker

The semantic memory linker joins deterministic static object layouts to
offset-path-keyed runtime captures. It is read-only. It never writes to the
game, promotes names, edits fact stores or treats an absolute heap address as
stable.

## Build the index

```powershell
python tools\f1c_mod.py memory-build
```

The generated `analysis/semantic_memory_index.json` contains:

- per-function field groups separated by their actual base expression;
- ordered offsets, read/write direction, constants and layout fingerprints;
- all 775 configuration-token/offset associations;
- exact `_BYTE`, `_WORD` and `_DWORD` write widths recovered from raw C;
- runtime kind, persistence and trusted name joins for confirmed vehicle bases;
- reviewed object identities and explicitly DERIVED identity proposals.

Identity propagation is intentionally one-hop and forward-only. A reviewed
owner method may propose its type for a callee when it passes unchanged `this`.
Reverse and multi-hop propagation are forbidden because generic helpers such as
free/copy routines accept many unrelated object types.

Rebuilding an existing generated index requires `--force`. Other commands can
reuse it with `--index analysis\semantic_memory_index.json`.

## Inspect an object layout

```powershell
python tools\f1c_mod.py memory-fingerprint VehicleDamageModel
python tools\f1c_mod.py memory-fingerprint AABBCollisionBodyInitializer --json
```

The first command exposes the named `damage.ini` parser layout. The second also
shows exact constructor-store widths suitable for a runtime signature.

## Match a runtime capture

```powershell
python tools\f1c_mod.py memory-match AABBCollisionBodyInitializer before
```

The matcher scans the flat struct and every captured child at four-byte-aligned
candidate bases. Automatic signatures use only literal stores whose width is
explicit in raw Hex-Rays C. Functions without at least two such constraints are
refused rather than guessed.

A reviewed manual signature can be supplied when values come from shipped data
or a controlled runtime observation:

```json
{
  "name": "damage model test signature",
  "trust": "OBSERVED",
  "constraints": [
    {"offset": "0x1C", "type": "float", "value": 123456.0},
    {"offset": "0x4C", "type": "float", "value": 654321.0}
  ]
}
```

```powershell
python tools\f1c_mod.py memory-match VehicleDamageModel before --signature signature.json
```

At least two constraints and one nonzero constraint are mandatory. Matches are
labeled `OBSERVED_CANDIDATE`; they are not automatically recorded as facts.

## Correlate an event

```powershell
python tools\f1c_mod.py memory-correlate wing_loss --control ordinary_driving
```

Changed flat and child paths are joined to trusted names, persistence verdicts
and statically linked accessors. Paths also moving in a control capture receive
a dominant penalty. The ranking prioritizes investigation; it does not name an
unknown field automatically.

Capture inputs through the canonical read-only workflow:

```powershell
python tools/f1c_mod.py memory-snapshot wing_before
# cause one natural event
python tools/f1c_mod.py memory-snapshot wing_after
python tools/f1c_mod.py memory-diff wing_before wing_after
python tools/f1c_mod.py memory-correlate wing_loss --control ordinary_driving
```

Use a same-duration ordinary-driving capture as the control so normal physics
jitter is removed from the shortlist.

## Front-wing pilot

```powershell
python tools\f1c_mod.py memory-front-wing
```

The generated `analysis/semantic_memory_front_wing.md` currently joins:

- the `FrontWingDetach` configuration threshold at damage-model `+0x1C`;
- confirmed `veh+0xCD4` `FrontWingRecordPtr`;
- confirmed `veh+0xD14` `FrontWingIntactFlag`;
- the observed front-wing child-object shape;
- all extracted static readers and writers;
- existing runtime deltas involving those paths.

The current evidence contains readers but no extracted direct writer. The next
useful experiment is therefore a tightly timed natural-loss/control capture,
followed by hooks on the shortlisted game-owned accessors. Directly forging the
intact flag is not the objective.
