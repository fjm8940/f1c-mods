# 55 — CPU render bottleneck investigation plan (for DeepSeek V4.1 Flash)

**Status:** PARTIAL STATIC INVESTIGATION (2026-10-13) — VBSTRATEGY parser-to-device/buffer/lock map established; renderer draw calls and network-worker creation path identified. Runtime timing, AMD/NVIDIA comparisons and exhaustive render/thread mapping remain pending. See [the VBSTRATEGY and modder-claims assessment](../investigations/VBSTRATEGY_MODDER_CLAIMS_ASSESSMENT_2026-10-13.md). The starting leads below retain their original unverified wording as history.
**Created:** 2026-09-25 by Claude (Opus 5.5) at Matias's request.
**Executor:** DeepSeek V4.1 Flash (or any follow-up LLM).
**Pipeline stage:** define -> investigate. No implementation is authorized.

## 1. Origin and objective

A fellow F1C modder reports that F1C is CPU-bound: vertex rewriting, scene
drawing, and "everything" run on the processor on one thread. He says
`VBSTRATEGY=2` is the best setting but still mixes CPU and GPU work, while later
ISI engines moved the work to the GPU and use helper threads (GTR, NT2004). His
heuristic: "150 FPS with GPU usage under 80% means the CPU is the bottleneck."
He asked for "F1C source, the part that does things on the CPU."

**Objective:** Determine, with evidence, where F1C's frame CPU time goes, what
`VBSTRATEGY` actually changes, and which fixes are feasible, ranked by cost and
risk. The output is a findings document and a ranked fix list. It is not a patch.

**Non-goals:**
- No binary patching, DLL injection, or writes into any game installation.
- No attempt to multithread the engine.
- No sharing of decompiled corpus content with the external modder. Whether to
  share anything is Matias's decision; flag it and do not act on it.
- No third-party downloads (d3d8to9, DXVK, profilers) without Matias's explicit
  approval in chat.

## 2. Starting facts from the 2026-09-25 quick look (verify, don't trust)

| Fact | Source | Tier |
|---|---|---|
| Renderer is Direct3D 8 (`Direct3DCreate8` referenced; no DirectDraw) | `callgraph.json` grep | STATIC, shallow |
| No `CreateThread` reference in the call graph | `callgraph.json` grep | STATIC, shallow; the CRT or Miles (`Mss32.dll`) may still create threads |
| `VBSTRATEGY` is parsed in `ConfigIniParser_00421cd0.c` (around line 383, `sub_565760(aVbstrategy, &v31, ...)`) | function file | STATIC |
| Matias's `Config.ini` copy has `VBSTRATEGY=1` | `f1c_ida_decompiled/Config.ini` | FILE |
| The render path is barely named: `render.cpp` has only 3 string references; `analysis/subsystems/` holds only `pit.md` | `SOURCE_FILES.md`, directory listing | STATIC |
| Named D3D/CPU-side candidates exist: `D3DContextInit_005063a5`, `ColorVertexTransform_00507bc5`, `FindNearestVertexWeight_005086dd`, `D3DSurfaceCopyBlit_00500bc8`, `CopyTextureSurface_0050aeae`, `BitFlagSetterWithDXUpdate_0055d2f0`, colour/DXT converters at `0x507xxx`–`0x509xxx` | `functions/` | NAMES ONLY; the names are inferred, not proven |
| Corpus size: 7,143 function files in `functions/` | directory count | FILE |

## 3. Rules for the executor

1. Cold start per the root `AGENTS.md`: read `README.md`, the `governance\` files,
   `f1c_ida_decompiled\AGENTS.md`, and `tools\sdk\LLM_OPERATING_CONTRACT.md`, then
   run `tools\workspace\check-workspace-structure.ps1`.
2. Never read `api1.txt` or private keys. Never delete, reset, or clean anything.
3. Label every claim `STATIC`, `RUNTIME OBSERVED`, or `INFERENCE`. A function name
   in this corpus is a hypothesis until the decompiled body supports it.
4. Derive D3D8 vtable offsets from the real `d3d8.h`
   (`IDirect3DDevice8` / `IDirect3D8` method order x 4 bytes). Do not trust
   offsets from memory, including any written in this plan.
5. Budget: stop after each phase and write results into the findings document
   before starting the next phase. If a phase exceeds about 40 function reads
   without convergence, stop and record the blocker.
6. Runtime work (Phase 4) needs Matias at the machine. Prepare it; don't improvise it.

## 4. Phases

### Phase 1 — What `VBSTRATEGY` does (STATIC)

1. In `ConfigIniParser_00421cd0.c`, find where the parsed value (`v31` near
   line 383) is stored: the global address or struct offset.
2. Find every reader of that storage. Use `callgraph.json`,
   `docs/12-globals-reference.md`, and grep across `functions/` for the address.
3. For each reader, record the behavior per value (0/1/2/any others): vertex
   buffer creation flags, static vs dynamic buffers, lock and discard patterns,
   and UP (user-pointer) draw calls vs VB draw calls.

**Deliverable:** a table of `VBSTRATEGY` values -> code path -> CPU/GPU consequence.

### Phase 2 — D3D8 device and draw path map (STATIC)

1. Start at `D3DContextInit_005063a5`. Find `Direct3DCreate8` and
   `IDirect3D8::CreateDevice`, and record the **BehaviorFlags**:
   `D3DCREATE_SOFTWARE_VERTEXPROCESSING`, `HARDWARE_`, `MIXED_`, or `PUREDEVICE`.
   **This is the single most important finding.** Software vertex processing
   means the D3D runtime does transform and lighting on the CPU.
2. Locate the stored device pointer and enumerate indirect calls through its
   vtable for: `CreateVertexBuffer`/`CreateIndexBuffer` (with the Usage and Pool
   arguments), `DrawPrimitive`, `DrawIndexedPrimitive`, `DrawPrimitiveUP`,
   `DrawIndexedPrimitiveUP`, `ProcessVertices`, `SetStreamSource`,
   `SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING)`, and VB/IB
   `Lock`/`Unlock`.
3. Identify CPU-side per-vertex loops: `ColorVertexTransform_00507bc5`,
   `FindNearestVertexWeight_005086dd`, and any skinning, morphing, or
   matrix-per-vertex loops that callers reach from the draw path. Determine
   whether they run per frame or only at load time. Texture/DXT converters are
   probably load-time; confirm that before discarding them.
4. Map the main loop: the frame function, the order of simulation vs render, and
   `Present`.

**Deliverable:** a call map from frame -> scene traversal -> draw submit, with
per-frame CPU work marked.

### Phase 3 — Threading reality check (STATIC)

1. Check imports for `CreateThread`, `_beginthread(ex)`, and `WaitForSingleObject`,
   including in `callgraph.json` and the import table if available.
2. List the threads that do exist. Miles audio likely has its own; networking
   may have one.
3. Record which work, if any, could move off the main thread in isolation
   (asset/track loading, audio decode) and why simulation plus render cannot.

**Deliverable:** a short threading model section. Confirm or refute the modder's
"single thread" claim.

### Phase 4 — Runtime measurement protocol (PREPARE ONLY; Matias executes)

Write a step-by-step protocol, don't run it:

- Fixed scenario: the same track, car count, camera, and replay or AI-only
  session, run in `F1 Challenge V10_clean` (not the main install) unless Matias
  chooses otherwise.
- Matrix: `VBSTRATEGY` 0/1/2 (plus any other valid values from Phase 1) x
  (stock D3D8, d3d8to9, d3d8to9 + DXVK).
- Metrics: PresentMon frame time plus GPU busy; ETW/WPA CPU sampling on the
  main thread. Map hot addresses to `functions/*.c` by address.
- Interpretation rule: high time inside `d3d8.dll` or the driver means API/driver
  overhead, and a wrapper may fix it. High time inside `F1C.exe` render code
  means a game-side patch is needed. High time in simulation means rendering is
  not the issue.
- Every tool download is listed with its source URL for Matias's approval.

### Phase 5 — Synthesis

Produce a ranked fix list. Each entry includes: expected gain, evidence tier,
implementation cost, risk to mods and visuals, and the next gate. The expected
candidate order, to be confirmed or overturned by evidence:

1. A wrapper (d3d8to9, or d3d8to9 + DXVK), which needs zero reverse engineering.
2. A best `VBSTRATEGY` recommendation backed by Phase 1 evidence.
3. Forcing hardware vertex processing in `CreateDevice`, if it currently uses
   software processing (a one-flag patch candidate).
4. Converting hot dynamic/UP draw paths to static VBs, or batching them.
5. Isolated offload to helper threads (loading only).
6. Full multithreading: expected verdict "not feasible". State why.

## 5. Output and documentation

- Findings go in `f1c_ida_decompiled/docs/56-cpu-render-bottleneck-findings.md`,
  using `docs/TEMPLATE.md`, and are indexed in `docs/README.md`.
- Update this plan's status line when phases complete.
- Do not update `governance\PRODUCT_STATUS.json`. This is research, not a stage
  change.

## 6. Abort conditions

- Device creation or the vtable dispatch can't be located statically after a
  bounded search. Record it and propose a runtime API trace instead.
- Evidence contradicts the modder's premise (for example, the game is GPU-bound
  or simulation-bound). Report that plainly; don't force the narrative.
- Any step would need writing to a game installation or downloading software.
  Stop for Matias.
