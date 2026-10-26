# Memory Snapshots and Variant Resolution

This layer discovers addresses for executable variants without changing the
canonical fixed-address SDK. Every generated profile is separate and must
reach `status: RESOLVED`; ambiguous or contradictory evidence is refused.

## Memory-map-aware snapshots

With the game running:

```powershell
python tools\f1c_mod.py memory-snapshot inactive_a
python tools\f1c_mod.py memory-snapshot active_a
python tools\f1c_mod.py memory-diff inactive_a active_a --transition 0:1 --width 1
```

The snapshot process must run at the same or a higher Windows integrity level
as the game. If the game is elevated and the terminal is not, the tool exits
before creating a snapshot directory and reports the access mismatch.

The default `module` scope captures readable pages belonging to the main game
image, including its static data. `--scope process` captures every readable,
committed 32-bit process page and can be much larger.

Pages are compressed and stored by SHA-256 under
`runtime_memory/_page_store`. Identical pages across experiments are written
only once. Each snapshot manifest preserves the memory map, protection/type,
module hash, absolute snapshot address and module RVA. Diffs match module pages
by module name plus RVA, so an ASLR base change does not make every page appear
different.

An isolated `0→1` result is only a candidate. Repeat at least two complete
cycles and capture a same-duration control state before attaching semantics:

```text
inactive_a → active_a
inactive_b → active_b
ordinary_control_a → ordinary_control_b
new-session inactive_c → active_c
```

Heap addresses are snapshot-local. Persistent evidence should use an object
root and offset path.

## Executable profiles

```powershell
python tools\f1c_mod.py variant-profile "C:\path\modified.exe"
```

The profile includes SHA-256, PE timestamp, preferred image base, ASLR flag,
entry RVA, image size and per-section hashes. Profiling is read-only.

The canonical F1C target reports `dynamic_base: false`; its absolute module
addresses therefore remain stable across normal launches. Heap allocations do
not gain that stability.

## Symbol recipes

Canonical recipes are generated explicitly:

```powershell
python tools\f1c_mod.py symbol-recipes-build
```

`analysis/symbol_recipes.json` currently covers:

- `BuildActiveVehiclePointerArray`;
- `PitStopDecisionTireCompound`;
- `VehicleArray`;
- `VehicleStride`;
- `VehicleLivenessOffset`;
- `FrontWingRecordPtr`;
- `FrontWingIntactFlag`.

Each function has three masked anchors and requires a two-anchor quorum. Image
addresses, relative branches and explicitly relocatable operands are masked.
GNU objdump supplies a normalized instruction hash as an independent verifier.

Each derived operand has its own local context fingerprints. All occurrences
must agree. This allows the containing function, global address and structure
displacements to move independently.

## Resolve a modified executable

```powershell
python tools\f1c_mod.py resolve-symbols "C:\path\modified.exe" \
  --output analysis\modified_resolved_symbols.json
```

Resolution requires:

1. Function-anchor quorum with one unique candidate.
2. Local operand-context quorum.
3. Agreement between duplicate operand references.
4. Vehicle-array address inside the target image.
5. Plausible stride and field ranges.
6. `FrontWingIntactFlag - FrontWingRecordPtr == 0x40`.

The result includes per-anchor/context evidence and confidence. A failed
invariant makes the entire profile `REFUSED`; such a profile must never provide
addresses to a mod.

## What the tests prove

The synthetic variant test copies both pilot functions to different executable
RVAs, removes their originals, changes the vehicle global, stride, liveness
offset and both front-wing offsets, then resolves every new value. A second
variant deliberately breaks the wing-record relationship and must be refused.

Snapshot tests also relocate the module base between two manifests and verify
that a single byte `0→1` transition is still identified by module RVA.

This proves relocation mechanics. It does not prove semantic equivalence for an
arbitrary rebuild. Substantial logic changes may remove or duplicate a symbol;
the correct result in that case is refusal and a new reviewed recipe.
