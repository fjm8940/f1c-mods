# F1 Challenge GPU execution feasibility assessment

Date: 2026-10-13. Scope: owner's request to investigate running the game 100% on the GPU. Status: STATIC ASSESSMENT; no runtime profiling or implementation.

## Orientation and scope

Product: F1C SDK/research platform, canonical `f1c_ida_decompiled`. Registered product stage remains implement/candidate; this task follows orient -> define -> investigate. Documentation destination: this assessment, the companion-investigation index in `docs/README.md`, and curated `findings.jsonl`. No product stage, installed state, interface or checkpoint objective changed.

Acceptance: distinguish literal CPU-free execution, GPU-heavy graphics and GPU simulation; identify native evidence, feasibility, limitations and a bounded next investigation. Non-goals: patches, downloads, installation, gameplay automation or resuming the paused replacement engine.

Initial workspace validator failed with two existing unexpected top-level directories, `.claude` and `plans`. No structural changes or repairs were attempted. This prevents claiming the orientation structure gate passes, but does not require a structural move to perform this read-only investigation and retain its evidence.

## Answer

DERIVED: The existing Windows game cannot become a practical CPU-free application through a setting, graphics wrapper or small patch. GPU-heavy rendering is technically plausible; moving most simulation as well would require substantial subsystem replacement, different data layouts and extensive compatibility validation. This is an architectural rewrite, not an established optimization with a known speedup. The statement concerns this game/platform, not mathematical impossibility of emulating CPU instructions on a GPU.

GPU utilization of 100% is a separate metric. Increasing visual cost can saturate a GPU while making frame times worse; it does not prove that CPU work has moved.

## Evidence and prior art

Prior-art commands: `start --json` (freshness.stale=false), `evidence-query findings "GPU" --json` (zero results), and findings query for rendering. `inspect` and typed source searches checked the following evidence. Static interpretations below are DERIVED, not live measurements or newly reviewed callable ABIs.

| Evidence | Static result | Limit |
|---|---|---|
| `functions_typed_20260930/sub_53D750_0053d750.c`, lines 22-23; imported thunk `Direct3DCreate8_0059b250.c` | Renderer initialization calls Direct3DCreate8(220) and stores the interface. | Does not establish runtime wrapper, hardware/software vertex-processing flags, or per-frame GPU cost. |
| `functions_typed_20260930/MainGameTickUpdate_0042c690.c` | CPU code advances a time-derived loop in 0.0025 increments, uses modulo scheduling, iterates vehicles for AIMainUpdate, and calls physics, effects and camera work. | 400 Hz base schedule and AI every tenth base tick are static derivations; actual update frequency and time share were not measured. Not every subsystem uses the same frequency. |
| `functions_typed_20260930/AIMainUpdate_00424e70.c` | Vehicle-pointer access, session globals and conditional AI state updates are native CPU code. | This is not a complete dependency map or proof that all game work is single-threaded. |
| `functions_typed_20260930/VehiclePhysicsModeDispatcher_004e4930.c` | Mode-dependent native calls include VehiclePhysicsIntegrationStep and PhysicsIntegrationStateMachine. | Names and ABI metadata remain LEGACY; no function was called. |
| `functions_typed_20260930/VehicleDynamicsForceAccumulatorIntegration_00438130.c` | Numeric wheel/thermal/wear work uses double arithmetic, branches, mutable memory and native calls. | Performance importance and numerical equivalence of a hypothetical GPU port are unmeasured. |
| `docs/55-cpu-render-bottleneck-investigation-plan.md` | Prior work is explicitly PLAN ONLY; VBSTRATEGY parsing was independently located in ConfigIniParser at lines 383-385. | No verified mapping of values to buffer behavior, no proof software vertex processing is active, no demonstrated best setting. The plan's proposed findings filename 56 now collides with the voice reference; this assessment uses a distinct filename. |

## What could move

| Work | Feasibility assessment (DERIVED) |
|---|---|
| Graphics API/driver overhead | A compatibility wrapper may help particular bottlenecks, but translating graphics calls does not translate native AI or physics. Runtime benefit is OPEN. |
| Vertex transforms, animation, particles | Potential offload candidates if profiling proves substantial CPU work. Must first distinguish existing GPU work from CPU preparation and load-time conversion. |
| Visibility, batching, draw generation | A redesigned renderer could use GPU culling/indirect commands. Requires engine integration and visual regression checks. |
| Tyres, suspension, collisions | Mathematically parallel parts are conceivable compute workloads, but require explicit dependency boundaries, GPU-compatible layouts, stable numerical behavior and an economical synchronization strategy. |
| AI, rules, session management | Some pieces could be rewritten; pointer-heavy branches and shared state are poor candidates for a direct per-car transplant. A modest car count alone does not rule out useful finer-grained parallelism. |
| Input/FFB, files, network, audio delivery, OS/device setup | This Windows application still needs host/platform services. A GPU-heavy design retains CPU-side orchestration and interfaces. |

The GPU executes its own instruction model; the existing x86 routines cannot simply be dispatched as compute shaders. A port needs new kernels and integration, or an emulation layer that itself introduces cost. Host consumers include mods that expect native vehicle state; keeping simulation only in GPU memory would break those assumptions unless a compatible state bridge is designed. This compatibility risk is an inference, not an observed failure.

NVIDIA's best-practices guide explicitly warns that small transferred workloads may see little benefit, recommends substantial parallel work and retaining intermediate data on the device, and explains divergent control flow. These support risks, not a prediction that an F1C GPU port must be slower. GPU and CPU floating-point behavior would need validation, especially for replays, collisions and multiplayer. No bit-exact parity or hardware support is claimed.

Microsoft's indirect-drawing example demonstrates GPU-generated visibility/draw lists while the host application still dispatches and submits work. GPU-driven rendering is therefore a feasible architectural direction, not evidence of CPU-free execution.

## Next investigation and pending validation

1. Trace actual D3D8 CreateDevice flags and VBSTRATEGY readers statically, using real header method order; do not force hardware processing on assumption.
2. In a separately arranged guarded full-weekend test, measure frame-time distribution, CPU hotspots and GPU busy in a fixed track/camera/weather/grid scenario. Compare resolution and grid-size sensitivity; account for VSync, FPS caps and waits before diagnosing a CPU bottleneck. Low aggregate CPU/GPU utilization alone is insufficient.
3. Rank only measured costs: simulation, native render preparation, driver submission or GPU execution. Select one isolated improvement and compare identical scenarios, including transfer/wait overhead, visuals, mods and gameplay.
4. Abort a GPU experiment if total frame time or latency regresses, native state parity fails, required capabilities are absent, or the boundary cannot be made safe. Broader renderer/simulation replacement needs a separate specification; replacement-engine work remains paused.

OPEN: current PC bottleneck, active wrappers/device flags, exact VBSTRATEGY semantics, achievable speedup and development effort. No live capture, benchmarks, hardware inventory, downloads, new binaries, game writes or native acceptance were performed. Full SDK builds are not relevant verification for this documentation-only assessment.

## Changes, verification and restoration

Added this record and three curated findings; added an index link. Complete index before-state preserved under `workspace_organization_audit/GPU_ASSESSMENT_BEFORE_2026-10-13/`; existing finding bytes are preserved by append-only writing. Restoration of the index is possible from that copy; retain research/finding records as history rather than deleting them. No governance or operational document was edited. Documentation gate checks JSONL validity, original finding prefix preservation, index discoverability, UTF-8 and obvious mojibake. Final validator result is recorded below.

Wasted-effort check: no repeated large reads; no broad builds/tests; initial SDK command used workspace rather than SDK root and was corrected; early unbounded query/inspect output was truncated, then bounded reads were used; no polling/waiting/idle. Future runs should use explicit SDK working directory and bounded inspection from the start.

## External references (accessed 2026-10-13)

- [Microsoft: indirect drawing and GPU culling](https://learn.microsoft.com/en-us/windows/win32/direct3d12/indirect-drawing-and-gpu-culling-) — example of compute-generated draw commands.
- [Microsoft: CPU efficiency specification](https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html) — host command submission model.
- [NVIDIA: CUDA C++ Best Practices Guide](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html) — sections 3.2, 4 and control flow; parallelism, transfer overhead and profiling.

Final verification: workspace validator repeats the same two existing errors (.claude and plans), with zero warnings. Documentation gate PASS for report existence, index link, new finding JSON, preserved prior finding prefix and index before-state, and UTF-8 review. Whole-store JSON validation is BLOCKED by existing malformed nonempty lines [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15], left unchanged; the first validation attempt exposed this before any finding append. Correction to wasted-effort record: this validation failure required one bounded retry. No native/performance validation performed.
