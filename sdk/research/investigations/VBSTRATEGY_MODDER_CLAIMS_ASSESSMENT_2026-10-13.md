# VBSTRATEGY and modern-renderer claims assessment

Date: 2026-10-13. Status: STATIC FINDINGS; AMD/NVIDIA runtime comparisons pending.

## Objective, authority and scope

Owner asks to investigate VBSTRATEGY after a friend experienced lag with value 2 on AMD, and assess another modder's statements about CPU rendering, D3D9 conversion, helper threads, DDS, anisotropy and videos above 400 FPS. Owner explicitly says to treat these as leads, not authority. Preserve the broader objective: maximize FPS and stability on modern hardware without silently lowering visual or simulation fidelity.

Canonical product: F1C SDK/research platform, registered implement/candidate; this task is investigate. Destination: this assessment, docs companion index and curated findings. Startup freshness.stale=false. Existing root/scoped/SDK instructions apply; no new applicable component instruction found for these records. No stage, public interface, installed artifact or checkpoint objective changed. The workspace validator still fails only on existing unexpected root directories `.claude` and `plans`; neither was altered.

Scope: native static code and public primary-source documentation. No external modder binary, patch diff, video URL, source or benchmark configuration was supplied; no claim about the contents of that executable is verified. No software download/install, native game launch, live configuration edit, binary patch or sharing/publication was performed. Sibling-engine thread architecture was not re-investigated.

## Main result

DERIVED: VBSTRATEGY is a renderer policy affecting geometry-buffer pooling, dynamic vertex-buffer placement/usage, lock flags and device vertex-processing selection. It is not a global CPU/GPU workload switch. Values 1 and 2 share pooling and lock-hint behavior, but differ in dynamic-buffer software-processing usage and whether device creation is forced into mixed processing.

Therefore the friend's result is compatible with the code: value 2 is not established as universally fastest. Driver/backend/resource behavior can change the outcome. The actual AMD cause remains OPEN; no AMD-specific bug has been proven, and this is not a recommendation to force 1 on every AMD system.

## Native evidence map

Paths below are relative to the SDK repo. Addresses are navigation to the canonical corpus, not callable ABI approval.

| Function | Evidence and interpretation |
|---|---|
| `ConfigIniParser_00421cd0.c:383-385` | VBSTRATEGY token stored at config `this[57]`, byte offset 228. `GameMainEntry_0045b200.c:9` supplies base 0x5EC3D0; 0x5EC3D0 + 228 = 0x5EC4B4. |
| `VidMan_Enter_004fa000.c:60` | Passes dword_5EC4B4 to GraphicsConfigStateSetter before the following video setup call. |
| `GraphicsConfigStateSetter_005408f0.c` | 0 stores policy=0, pooling=0; 1 stores policy=1, pooling=1; 2 stores policy=2, pooling=1. Other values leave the existing fields unchanged. Renderer fields are `this[1272]` (+0x13E0) and `this[1271]` (+0x13DC). |
| `DirectXDeviceSurfaceInitialization_0053d4f0.c:63-64` | Renderer constructor initializes both fields to 1. This is a renderer default, not proof of the current config value. |
| `sub_540940_00540940.c:75-167, 236-254` | Pooling path suballocates geometry from shared vertex/index buffer lists; non-pooling path creates dedicated buffers and zeroes base offsets. Dynamic-like vertex descriptors use a separate pool list. |
| `sub_540DF0_00540df0.c:12-51` | Calls device vtable byte +92 with length, usage, FVF, pool and output pointer: CreateVertexBuffer. Internal descriptor bit 0x20 gates the strategy-dependent dynamic branch. |
| `BufferLockAllocate_00540f00.c:10-33` | Calls buffer vtable +44, Lock, with stride-scaled offset/length and strategy-dependent hints. |
| `sub_53E070_0053e070.c:378-415` | Calls IDirect3D8 vtable +60, CreateDevice, using 0x20/0x40/0x80 vertex-processing flags. VBSTRATEGY is read at renderer+5088; a separate option is read at +5076. |
| `sub_540FC0_00540fc0.c` and `SurfaceBlitLockWithFlagCheck_00541090.c` | CreateIndexBuffer at device+96; index Lock at buffer+44. Despite its name, the latter body locks an index buffer, not a texture surface. Index locks here do not select strategy-specific DISCARD/NOOVERWRITE. |
| `sub_5550E0_005550e0.c:309-336,588` | Real native renderer issues SetIndices (+340), DrawIndexedPrimitive (+284) and DrawPrimitive (+280). This contradicts a literal claim that drawing is entirely CPU rasterization. |

All files above are in `functions_typed_20260930/`. Relevant names often retain LEGACY metadata. Read bodies, not inferred names. Header method order was independently checked in Wine's published d3d8.h: CreateDevice slot 15; device CreateVertexBuffer 23, CreateIndexBuffer 24, DrawPrimitive 70, DrawIndexedPrimitive 71, SetIndices 85; buffer Lock 11. Multiply slots by four for this x86 ABI. Constants were checked in d3d8.h/d3d8types.h. This corroborates static API identification, not runtime performance or callable lifetime contracts.

## Strategy matrix

Applies to the examined geometry path and its descriptor bit 0x20. Other geometry/resource paths may have different flags.

| Value | Geometry pooling | Dynamic vertex-buffer policy | Dynamic vertex Lock policy | CreateDevice selection |
|---|---|---|---|---|
| 0 | Disabled in the examined allocator | SYSTEMMEM pool (2); DYNAMIC (0x200), plus common base flags | DISCARD (0x2000) | In the input flag-0x04 branch, forced MIXED (0x80) |
| 1 | Enabled | DEFAULT pool (0); DYNAMIC plus SOFTWAREPROCESSING (0x210), plus common base flags | Caller hint bit 0x40 selects DISCARD; otherwise NOOVERWRITE (0x1000) | In that branch, forced MIXED (0x80) |
| 2 | Enabled | DEFAULT pool (0); DYNAMIC (0x200), plus common base flags, without mode 1's forced SOFTWAREPROCESSING | Same as 1 | In that branch, HARDWARE (0x40) if renderer+5076 is zero; otherwise MIXED (0x80) |

Common caveats: if renderer vertex-processing state `this[1102]` is zero, the VB allocator adds SOFTWAREPROCESSING even outside mode 1. If device-selection input flag 0x04 is absent, CreateDevice requests SOFTWARE (0x20) regardless of strategy. The input flag is the engine's gate for the hardware/mixed path; its full upstream capability provenance was not exhaustively mapped. HARDWARE_VERTEXPROCESSING is distinct from D3DCREATE_PUREDEVICE (0x10), which is not the flag selected here.

The Lock helper always includes NOSYSLOCK (0x800) and optionally READONLY (0x10). Non-dynamic descriptors do not enter the strategy lock-hint branch. Mode 0's DISCARD request is recorded as code behavior, not a claim that the driver implements buffer renaming for SYSTEMMEM. The flag-0x40 mapping is specific to the helper's caller hint; it must not be confused with the CreateDevice hardware flag.

Useful consequence: changing 1 to 2 can alter both device processing and dynamic-buffer handling. A mode comparison alone cannot identify which difference caused a gain or regression. Modes 1/2 share DISCARD/NOOVERWRITE selection, so a claim that they differ *only* in lock flags is wrong.

Read-only configuration observation: `F1 Challenge V10/Config.ini:32` currently says VBSTRATEGY=2; SDK repo `Config.ini:32` says 1. No value was changed. On-disk settings are not proof of a running process's latched setting.

## CPU vertex work and threading

DERIVED: `sub_54CA00_0054ca00.c` contains CPU loops that pack colours and adjust/copy per-vertex values, under guards including an already-prepared flag; it ultimately sets prepared flags. The observed loops are not proof that this entire preparation repeats every rendered frame. `sub_55D720_0055d720.c` has guarded vertex-copy/update paths, including BufferLockAllocate on one branch. Together with the real D3D draw calls, this supports a CPU-prepared/GPU-rendered pipeline. It does not show that all transforms, lighting or rasterization happen on the CPU, or rank these routines by cost.

DERIVED: The canonical typed corpus contains a CreateThread call in `sub_585470_00585470.c:39`, called from `NetworkConnectionThread_0057fc80.c:67`, whose body initializes Winsock and sockets. This is concrete evidence for a network-related worker creation path. It corrects the older plan's shallow 'no CreateThread reference' lead. No beginthread textual match was found, but absence of a symbol match cannot establish absence of threads.

OPEN: actual live native thread count, active worker roles, and threads created by Miles/audio, graphics drivers, plugins or wrappers. 'Exactly two threads' is not established by static import counts. A dominant serial game/render thread remains plausible; compare CPU time per thread and call stacks, not total thread count. A helper that opens menus does not by itself explain steady-race FPS gains.

CONFIRMED EXTERNAL MECHANISM: DXVK D3D9 constructs `m_csThread`; DxvkCsThread constructs a real worker, processes queued command chunks and synchronizes when required. A graphics wrapper can therefore provide a worker without F1C source or a parallel simulation rewrite. Whether the modder's build contains this design, and whether it helps a given scenario, remain OPEN. D3DCREATE_MULTITHREADED means synchronization/thread safety, not automatically spawning a render worker; Microsoft warns that the synchronization can hurt performance.

## Assessment of the supplied claims

| Reported statement | Assessment |
|---|---|
| VBSTRATEGY=2 is best | Not a universal conclusion. Static code explains a different hardware/buffer path; owner's AMD regression is an uninstrumented counterexample report needing controlled reproduction. |
| Everything including scene drawing runs on CPU | Overstated literally: native code issues D3D draw calls and can create a hardware vertex-processing device. CPU traversal, submission and vertex updates can still dominate. No performance share measured. |
| F1C has one thread for everything plus multiplayer | Partly supported by a network-worker creation path; exact count and all other worker roles unverified. Driver/audio/plugin threads must be separated from engine workers. |
| A DXVK-like second thread was added | Technically plausible. DXVK has a command-stream worker. This is different from proving AI/physics/scene traversal became parallel. His implementation was not inspected. |
| Cannot add threads without source | Too absolute. A wrapper/plugin can create a worker; safe useful task extraction is difficult because state ownership, order and synchronization must be recovered. Source makes this easier, not categorically possible versus impossible. |
| GTR/NT2004 use more GPU work and helper threads and have no bottleneck | Unverified sibling-engine comparison. No-bottleneck is not an achievable architectural guarantee. Helpers and API versions alone do not establish critical-path improvements. |
| 150 FPS with GPU usage below 80% proves CPU bottleneck | Insufficient. Consider VSync/FPS caps, present/driver waits, memory/I/O stalls and GPU metric sampling. It can suggest CPU/driver limitation only in an otherwise controlled uncapped test. |
| More than 700 D3D8 calls each need translation | A count might mean static call sites or calls per frame, not distinct methods; no count/diff supplied. A compatibility interface can centralize translation for many sites. API changes genuinely include BaseVertexIndex, stream offsets, surface/resource creation and software-VP control. |
| D3D8-to-9 conversion and mirror allocation fixes | Plausible engineering work. Microsoft documents real surface and resource API changes. Specific mirror bug and fix need his code/diff or a reproducible API trace. Backend compatibility must include mirrors and reset, not just peak FPS. |
| DDS can replace textures without MTS changes | Stock corpus already includes DDS signature/format/mip parsing, including DXT block-size paths in sub_5026A4. This does not prove a BMP-named MTS reference automatically resolves a .dds file, or establish that he added DDS decoding. Filename lookup, renaming, MAS precedence and actual GPU format need separate verification. |
| ANISOTROPY added to both EXEs | Plausible setting/render-state feature; his binaries/config parser were not supplied. No ANISOTROP text match in the canonical typed function corpus, which is not proof that numeric anisotropic state support is absent. Primarily a visual-quality control, not automatic CPU modernization or a speedup. |
| Videos exceed 400 FPS | Owner-reported outcome, not inspected or reproduced. 400 FPS corresponds to 2.5 ms/frame, versus 6.67 ms at 150 FPS. Without matched CPU, track, grid, textures, mirrors, weather, camera, wrappers and frame-time distribution, causality and generality remain open. This does not dispute that his improvements may be substantial. |
| Only the CPU part of F1C / GPU part of gMotor2 source is needed | No clean source partition is established: scene state, geometry, resource lifetimes and rendering API work cross subsystem boundaries. Decompiled evidence and call maps can identify useful boundaries, but are not original source or a ready-to-build engine. No corpus/source was sent to anyone. |

## Ranked next work

1. Compare 1 and 2 on the owner's NVIDIA system and the friend's exact AMD configuration using matched, uncapped guarded-weekend scenarios. Include 0 as a diagnostic, not a default recommendation. Record executable hash, driver and wrapper modules/config, effective device flags, geometry/resource workload and p50/p95/p99 frame times. Repeat warm runs in alternating order; separate loading/first-use behavior. No game setting is changed by this report.
2. Trace actual CreateDevice, CreateVertexBuffer and Lock arguments and timing, draw counts and CPU stacks. Validate the branch table at runtime and identify which flags/resources dominate the lag. If possible compare native backend with one identified wrapper at a time; no third-party installation was performed here.
3. Prioritize conditional CPU vertex rewrites, buffer synchronization and resource churn only if hot. Static preparation routines may be load-time; instrument frequency before spending effort replacing them. Preserve ordering, DISCARD/NOOVERWRITE correctness, mirrors, reset and mods.
4. Continue the high-resolution-texture investigation independently: texture residency, conversion/upload costs, mips and address-space pressure. VBSTRATEGY addresses geometry policy; it is not a direct texture-memory cure.
5. If the modified EXE or patch/source becomes available, inspect its exact changes and benchmark in an isolated target. Do not infer ownership or request publication from the quotes. No new tool installation, source sharing or wider-engine rewrite is implied.

Abort implementation when call ABI, game/device thread, resource ownership or exact target is unproven. An FPS gain with corrupt geometry, worse long-frame tails or session-exit crashes fails the objective. Gameplay, visuals, multiplayer and long-session stability remain human/native gates.

## Verification, preservation and limitations

Static chain checked from parser storage to renderer setter, pool creation, device creation, lock and actual drawing; external headers and API docs corroborate method/constant interpretation. Findings remain DERIVED for native behavior; no runtime FPS/AMD result is promoted to FACT. No SDK interface/binary changes; full SDK builds would not validate these claims and were not run.

Added this report and append-only findings, updated research index and marked partial static progress in the pre-existing plan. Complete before-states of index and plan are under `workspace_organization_audit/VBSTRATEGY_BEFORE_2026-10-13/`. Existing finding prefix is preserved. Restore maintained docs from these copies if needed; retain new research/findings as history. No governance/product status edit, production change or structural move.

Wasted-effort check: some overlapping source reads; no unnecessary builds. A broad this[57] search produced unrelated classes; a front-door regex was mangled by argument handling and caused a slow full scan, then routed ripgrep excerpts resolved the field. One quoted glob was invalid on Windows. Three attempted nearby-address lookups were absent from the recovery index; exact names/known addresses were used afterwards. Several no-match searches are negative evidence, not implementation failures. No deliberate sleeps or prolonged polling; one process poll, then its completed output was collected. Future work should use known renderer offsets, bounded exact queries and shell-safe patterns.

## Primary external references (accessed 2026-10-13)

- [Wine d3d8.h](https://raw.githubusercontent.com/wine-mirror/wine/master/include/d3d8.h): interface order and device flags.
- [Wine d3d8types.h](https://raw.githubusercontent.com/wine-mirror/wine/master/include/d3d8types.h): pool, usage and lock constants.
- [Microsoft D3D9 performance guidance](https://learn.microsoft.com/en-us/windows/win32/direct3d9/performance-optimizations): dynamic-buffer synchronization and lock semantics; mechanism guidance, not an F1C benchmark.
- [Microsoft D3D8-to-9 migration](https://learn.microsoft.com/en-us/windows/win32/direct3d9/converting-to-directx-9): concrete API differences.
- [Microsoft threading differences](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-render-multi-thread-differences): multithreaded API synchronization costs.
- [DXVK D3D9 device](https://raw.githubusercontent.com/doitsujin/dxvk/master/src/d3d9/d3d9_device.cpp) and [command-stream implementation](https://raw.githubusercontent.com/doitsujin/dxvk/master/src/dxvk/dxvk_cs.cpp): actual worker construction and queue execution.
- [DXVK README/HUD](https://github.com/doitsujin/dxvk): worker, draw and memory counters; no deployment recommendation inferred.

Final gates: documentation PASS (report, indexed link, plan status link, eight valid appended findings, byte-preserved original finding prefix, complete index/plan before-states, UTF-8/mojibake review). Workspace validator FAIL with unchanged .claude/plans errors and zero warnings. Whole-store JSON validity is not claimed because previously recorded malformed first 15 lines remain untouched. Native effective flags, frame times, AMD regression cause, modder binary identity and human gameplay/presentation/stability validation remain pending.

## Owner-supplied screenshot addendum (2026-10-13)

OBSERVED IMAGE: Owner supplied codex-clipboard-d5031dcc-c3b0-4b6a-9e18-e6550f1cbef2.png from the desktop temporary directory. The visible overlay reads GPU 60 C / 55%; MEM 401 MB / 198 MB; CPU 58 C / 86% / 3592 MHz; RAM 14936 MB; D3D9 368 FPS. Overlay configuration, metric definitions, sample interval, hardware, executable hash and test conditions are unknown. Do not label the two MEM counters as used/total VRAM, CPU 86% as a particular core or whole-process utilization, or RAM as F1C-only usage without the overlay configuration.

DERIVED: 368 FPS corresponds to approximately 2.72 ms/frame if interpreted as the displayed rate. This is image evidence of a high reported rate in this scene, not an independently timed benchmark or proof of 400+ FPS. D3D9 label supports that the overlay detects a D3D9 presentation path, but does not distinguish a rewritten EXE from a D3D8-to-9 wrapper or prove Vulkan/DXVK is absent downstream. GPU usage 55% with a substantial CPU reading is consistent with CPU/submission limitation, subject to caps, waiting and measurement ambiguity; it does not prove a specific bottleneck or multithreaded engine rewrite. No p99/1%-low, long-session stability, high-resolution texture behavior or matched stock baseline is visible.

Scope: screenshot interpretation only. No native runtime checks or game changes. Appended this evidence to the existing indexed assessment and findings, preserving earlier content. Screenshot was not copied into the workspace; its temporary path may expire. Pending validation: metric definitions, hardware/configuration, matched baseline and frame-time capture. Wasted-effort check for this addendum: none observed.
