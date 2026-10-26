# Spec: parallel car mesh-map build (sub_4DE610) - 2026-10-20

Stage: specify. Product: fast-load DLL (new, dev only, MOD_STANDARD.md applies).

## Owner decision (Matias, 2026-10-20, chat)
"we want to use the maximum performance of modern systems for things like these"
-> Multicore is the target, not just the minimum safe fix. Applies to loading in general.

## Evidence (tier marked)
- FACT (loadprof_6176): sub_4DE610 = 2.54 s exclusive of a 7.05 s load, 78 calls
  (3 per car), 34-203 ms per car, identical cost for teammates with the same model.
- INFERRED (static, functions/*.c): sub_4DE610 only READS its inputs and writes into
  its own object (`this`) plus fresh heap blocks:
  - callees SceneGraphChildCount 0x54C210, LinkedListIndexedAccessor 0x54C230,
    PackedArrayTotalSize 0x54E100, PackedArrayIndexLookup 0x54E120, 0x4DE5D0,
    TriangleNormalComputation 0x4DD9F0, comparator 0x4DE590 are pure reads.
  - qsort 0x5890AA sorts only its own new array.
  - malloc path 0x587C03 -> 0x58A947 -> 0x58A973 is the MT CRT: small-block heap under
    CriticalSectionLazyInitEnter(9), else HeapAlloc. Thread-safe.
  - No globals written. Results are read later by 0x4E3940, 0x4E3890, 0x4E3A50.
- INFERRED: the inner loop recomputes the same triangle normal for every vertex in the
  z band (TriangleNormalComputation per band vertex and per stored triangle).

## Design (three layers, each exact, each switchable in the INI)
1. Exact speed-up of the function itself (single thread): reimplement 0x4DE610
   bit-exact, but compute the current triangle normal once and cache per-triangle
   normals. Same x87 operation order so the output is byte-identical.
2. Cross-car parallelism: hook sub_4E3360's sub_4DE610 calls. Deep-copy the small
   input `a2` (counts + 20 B and 24 B arrays, it lives on 4E3360's stack), set the
   object header fields synchronously, queue the build to a worker pool
   (threads = logical cores - 1). Join all jobs before the first reader runs:
   at the end of load phase 2 (0x4B8940) and, as a safety net, at the entry of
   0x4E3940 / 0x4E3890 / 0x4E3A50.
3. Per-model reuse: key = (mesh node pointer content hash, restriction data hash).
   A teammate's identical job copies the finished result instead of rebuilding.

## Risks and abort conditions
- R1 the car mesh vertices could change after 4E3360 returns and before the job
  runs (later load steps). Mitigation: verification mode below; if any mismatch,
  fall back to layer 1 only (synchronous).
- R2 an exception or OOM in a worker. The job catches it and marks the car for a
  synchronous rebuild at the join point.
- R3 float order must match exactly for layer 1. Mitigation: verify mode.
- Abort: any byte mismatch in verify mode on the test grid, or a crash.

## Validation (agent then owner)
- Verify mode (INI `Verify=1`): for every call also run the original 0x4DE610 into a
  scratch object after the join and memcmp all produced arrays. Log PASS/FAIL per car.
- Profiler run (Adelaide 26 cars, compression off): target x_4DE610 wall time
  under 0.4 s on the owner PC (from 2.54 s).
- Owner: race start, damage/collision looks and behaves as before, teammates included.
- Restoration: remove the DLL / set Enabled=0, game runs vanilla.

## Next loading targets for the same worker pool (after this lands)
MAS one-shot inflate 0x59B260 (1.0 s), texture decode + convert (~1 s).
