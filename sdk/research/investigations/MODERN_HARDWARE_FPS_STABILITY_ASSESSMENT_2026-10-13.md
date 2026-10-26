# Modern hardware: FPS and stability investigation

Date: 2026-10-13. Status: preliminary static assessment; runtime baseline pending.

## Owner objective and orientation

Owner clarified the preceding GPU question: optimize the existing engine for modern hardware, maximize FPS and stability, and address severe lag with high-resolution textures despite an owner-reported test GPU. This supersedes CPU-free execution as the objective. The reply `y` to the symptom-category question did not identify a symptom, track or pack; no sustained-FPS versus stutter diagnosis is inferred.

Product: F1C SDK platform, registered implement/candidate; task stage investigate. Documentation: this internal assessment, docs/README.md companion index, findings.jsonl. No stage transition, paused replacement-engine restart or production change. Earlier cold-start documents remain applicable; SDK start refreshed, freshness.stale=false. Workspace structure gate remains blocked by existing `.claude` and `plans` directories.

Acceptance direction: improve average FPS and frame-time consistency without introducing crashes, memory growth, visual defects, physics changes or mod regressions. No numerical performance promise is supportable without a baseline. All systems have a limiting resource; practical success means removing avoidable stalls and scaling better on modern hardware.

## Verified observations and static interpretations

- OBSERVED FILE HEADER: Read-only PE header inspection of the live-install directory finds `F1 Challenge Online.exe` and `F1 (1).exe` machine 0x14c (x86), with IMAGE_FILE_LARGE_ADDRESS_AWARE set. `modified f1 challenge 99-02.exe` is x86 with the flag absent. Running process identity was not queried; these observations do not identify the owner's current executable. No listed local `d3d8.dll`, `d3d9.dll`, `dxgi.dll` or `ddraw.dll` was found in the install root. This does not exclude globally installed wrappers or other injection paths.
- CONFIRMED PLATFORM LIMIT: On 64-bit Windows an x86 process has up to 4 GB of user virtual address space with LAA, normally 2 GB without it. This is distinct from VRAM and total installed RAM. LAA does not guarantee an available contiguous allocation, fix leaks or increase CPU throughput. Thus a blanket 4 GB patch recommendation is redundant for the observed Online executable, not a verified fix for the lag.
- DERIVED: `DirectXSurfaceFormatNegotiation_00502a15.c` dispatches several image decoding paths, frees temporary state between failed attempts, and enumerates linked image levels. `DDSTextureSurfaceCreation_005026a4.c` checks DDS magic (542327876 = 0x20534444), parses dimensions/format fields and reads a level count. This establishes relevant parser paths, not which formats remain compressed on the GPU or how often decoding happens during driving.
- DERIVED CORRECTION: `TextureCreationWrapper_00462250.c` dispatches NetworkSendDispatch and is referenced by network command paths. Its name is not evidence of a graphics texture creation API. `TextureAllocationWithFormatSizeFlags_00558d10.c` calls a buffer-lock helper with element counts, and its callers include particle/vertex-like writes. `DirectXTextureCreationWithMipmapChain_00507496.c` is a constructor with a palette conversion loop; its body does not by itself establish D3D texture allocation or mipmap generation. Do not build hooks around these inferred names without identifying their actual role.
- DERIVED: `D3DResourceListCleanup_00554e10.c` traverses resource/material state and calls cleanup. Existing OPEN finding `S-retire-crash-d3d8-destroy-2026-10-11` describes an intermittent resource-destroy crash. That is a reason to preserve lifetime semantics in any renderer work, not evidence that this crash causes texture lag.
- OBSERVED SOURCE: `mods/sdk_sandbox/engine_overhaul_lab/engine_overhaul_lab.c` is a passive starter: target validation, logging and unload-event wait. It contains no demonstrated optimization. Its existence does not mean an engine overhaul has been implemented.

All native inspection used the SDK front door and typed corpus, with no calls to unreviewed game functions. LEGACY metadata is not a callable ABI.

## Why large textures can matter despite GPU capacity

INFERENCE, not a diagnosis: possible limits include 32-bit address-space pressure/fragmentation, redundant CPU image copies, repeated format conversion, synchronous uploads, resource churn, driver submission costs, inefficient sampling from missing mip levels, or a genuinely GPU-limited scene. High-resolution packs may also change geometry and material counts, so comparing different packs alone cannot isolate textures.

Arithmetic illustration: one 4096 x 4096 RGBA8 texture is 64 MiB at the top level, about 85.3 MiB with a complete mip chain. One 1024 x 1024 equivalent is 4 MiB (about 5.3 MiB with mips). Doubling each dimension quadruples texels. These are uncompressed theoretical sizes, not measured game allocations; file compression, GPU format, copies, sharing and alignment change actual totals. A powerful GPU does not remove host allocation or synchronization limits.

Microsoft's Direct3D 9 resource guide explains system-memory/device-memory management and update strategies. It supplies mechanism candidates, not proof that this Direct3D 8 game uses a particular resource pool or that a wrapper will fix it. Existing VBSTRATEGY plan remains PLAN ONLY.

## Bounded modernization roadmap

1. **Establish the exact baseline.** Identify the loaded executable and graphics modules, CPU/RAM/driver configuration, graphics settings and an owner-reproducible high-resolution scenario. Use a guarded full weekend. Hold track, grid, geometry, camera, weather and mods fixed; compare matched texture variants only if already available. Measure warmed repeated runs separately from loading/first-use behavior.
2. **Measure relevant costs.** Average FPS plus p50/p95/p99 frame times, long-frame counts, CPU hotspot/thread time, GPU busy, process virtual-space usage and largest available regions, working set/private commit, dedicated/shared GPU memory and disk activity. Total CPU utilization alone can hide one saturated thread; GPU utilization alone cannot distinguish caps and waits.
3. **Map graphics/resource boundaries.** Trace actual D3D8 creation, binding, locking, upload and destruction calls with real header method order. Count live textures and bytes by dimensions/format/mips, upload volume and per-frame creation churn. Correlate spikes with frame times before changing code. Instrumentation must be bounded and benchmarked for overhead; no per-draw disk logging.
4. **Choose the smallest measured fix.** If uploads/conversions dominate, investigate cache reuse, format preservation, mip handling and staged loading. If CPU draw submission dominates, investigate state reduction, batching and buffer reuse. If memory grows or cleanup is unsafe, repair ownership before performance experimentation. Wrapper comparisons are conditional candidates, not guaranteed gains. Preserve appearance as the primary comparison; any quality/performance option must be explicit.
5. **Scale CPU work only where independent.** Asset reads/decoding may be suitable for workers after proving reentrancy and dependencies. Keep native device/game-state calls on their required threads. Arbitrary simulation multithreading, forcing vertex-processing flags and timer/physics-tick changes are not acceptable shortcuts.
6. **Escalate only with evidence.** A new rendering backend or x64 engine port is a much larger compatibility project; neither follows automatically from this objective. Start with the existing engine and measured hotspots.

## Verification gates and limits

Each candidate needs a reversible development-only specification, exact target/build identity, before-state and restore mapping, one-change A/B evidence, improvement beyond run variance and no meaningful p99 regression. Stability checks: load/leave/reload, camera changes, restart, alt-tab/device reset where applicable, a long-session memory trend, representative large grids, visual checks, existing mods and multiplayer where affected. No lower simulation accuracy or silent asset downgrade may be counted as equivalent engine optimization. Abort on corruption, crashes, leaks or unproven thread/lifetime ownership.

Pending: exact problematic content, symptom class, running EXE/modules, runtime measurements, API resource map, implementation specification and human validation. No performance patch, native benchmark, download, build, install, process mutation or game-file change was made. FPS and stability improvement are not yet demonstrated.

## Record changes and restoration

Added this assessment, a companion index link and append-only findings. Preserve complete docs index before-state under `workspace_organization_audit/MODERN_HARDWARE_ASSESSMENT_BEFORE_2026-10-13/`. No governance or operational edits. Existing finding prefix must remain byte-identical; do not repair the pre-existing malformed first 15 JSONL lines in this task. Restore the index from its before-state if needed; preserve the new research as historical evidence. Product status and checkpoint objectives remain unchanged.

Wasted-effort check: no repeated large reads or unnecessary builds; one redundant bounded BufferLockAllocate inspection; no failed commands, polling or idle time. Some name-led searches reached misleading legacy labels; corrections above prevent patching the wrong subsystem.

## References

- [Previous GPU feasibility assessment](GPU_EXECUTION_FEASIBILITY_ASSESSMENT_2026-10-13.md).
- [Existing render investigation plan](../docs/55-cpu-render-bottleneck-investigation-plan.md).
- [Microsoft: memory limits](https://learn.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases).
- [Microsoft: resource management best practices](https://learn.microsoft.com/en-us/windows/win32/dxtecharts/resource-management-best-practices).
- [Microsoft: mipmap filtering](https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/texture-filtering-with-mipmaps).

External references accessed 2026-10-13. Local sources listed by repository-relative path above.

Final documentation gate: PASS for report/index discoverability, valid new JSON findings, original finding prefix and complete index before-state preservation, and UTF-8 review. Final workspace validator: FAIL with the same existing unexpected .claude and plans directories, zero warnings. No structural change. Native FPS, stability and presentation validation remain pending.
