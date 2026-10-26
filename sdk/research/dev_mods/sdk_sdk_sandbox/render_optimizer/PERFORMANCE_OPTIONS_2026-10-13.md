# Further performance options — 2026-10-13

Owner requested options for higher FPS, stability and better graphics-driver/hardware use. Product: Render Optimizer private SDK trial, investigate/specify future work; corrected installed trial still awaits native ACTIVE/hit evidence. No additional implementation/installation/settings change in this task.

Verified local context: native D3D8; VBSTRATEGY 2 allows hardware vertex processing in a specific device-creation branch but does not guarantee it, nor move all rendering preparation to GPU. Modes 0/1/2 differ in pooling/creation flags. Actual effective device flags remain unmeasured. Baseline 22-car GPU utilization was low but VSync and camera confound it; no gameplay FPS/frame-time data. Earlier generic state-cache experiment regressed/crashed and remains rejected. Corrected vertex trial improves a narrow CPU packing loop, not GPU offload.

Priority options (proposals, benefits unproven):

1. Measure native hardware/software vertex-processing selection, draw-call and Lock/Unlock timing, resource updates and CPU/GPU frame time. Establish uncapped throughput separately from capped smoothness; same native full-weekend scene and fixed camera. Do not infer a bottleneck from total CPU/GPU utilization alone.
2. Compare alternative API backends one at a time with stock baseline: d3d8to9 translates D3D8 calls/shader bytecode to D3D9; DXVK supports D3D8 via Vulkan and may change driver/submission overhead. No guaranteed speedup and no automatic move of game CPU logic to GPU. DXVK officially does not support Windows, so local Nvidia/AMD, mirrors, transparency, UI, fullscreen/reset and mod tests are necessary. No wrapper installed by this task.
3. Audit high-resolution texture costs: decoded versus compressed upload, complete mip chains, duplicate allocations/reuploads, main-thread conversion and 32-bit address-space pressure. Correcting confirmed asset/upload defects may address hitches without reducing selected texture resolution. Do not blindly recompress normal/alpha textures or assume available VRAM removes CPU/address-space costs. Existing Online EXE is already LAA; redundant LAA patch not proposed.
4. Reduce repeated geometry updates and CPU/GPU synchronization: retain immutable geometry in persistent buffers, batch appropriate uploads and preserve DISCARD/NOOVERWRITE lifetime rules. Current game already uses both flags in reviewed branches; do not blindly change them. Profile before patching.
5. Reduce redundant scene submission through valid batching, culling and LOD; preserve transparent ordering, mirrors and state semantics. Expect substantially more reverse engineering than a wrapper trial. Do not revive rejected shadow-vtable state cache.
6. Explicit GPU vertex shaders for confirmed CPU transformations/lighting/deformation can move meaningful rendering work onto GPU, but require identifying exact algorithms/formats and maintaining visuals/fallback. This is a larger renderer project; API translation alone does not accomplish it.
7. Background asset I/O/decompression/preparation can improve loading/hitches if ownership is proved; native device calls and game objects stay on reviewed owner threads. Arbitrarily moving AI/physics/render calls to threads is not a safe optimization.

Recommended order: get reliable frame-time measurements and validate current trial; compare one modern backend; investigate texture/upload path; target measured hot geometry/synchronization work; reserve broad shader/thread restructuring for supported evidence. No percent FPS improvement promised. GPU utilization itself is not an acceptance target; FPS, frame times, visuals and stability are.

Primary sources consulted 2026-10-13:
- https://github.com/crosire/d3d8to9 (API/shader translation scope).
- https://github.com/doitsujin/dxvk/wiki/ (D3D8 support).
- https://github.com/doitsujin/dxvk/wiki/Windows (Windows unsupported and platform-specific issues).
- https://learn.microsoft.com/en-us/windows/win32/direct3d9/performance-optimizations (batching, static/dynamic buffers and synchronization guidance; general D3D9 guidance, not proof of F1C performance).

Evidence: canonical VBSTRATEGY findings and linked existing trial/baseline reviews. New choices remain hypotheses until matched native tests. Documentation gate: component README link and OPEN findings. No binaries/settings or product-stage change; no tests needed for options prose. Validator retains existing .claude/plans errors and zero warnings. Before-state README retained under workspace_organization_audit/PERFORMANCE_OPTIONS_BEFORE_2026-10-13. Wasted-effort check: repeated large reads none; broad builds/tests none; failures/retries none material; polling/idle none.
