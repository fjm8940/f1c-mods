# Render-state optimization implementation review

Date: 2026-10-13. Owner asked to implement general FPS/stability work. Canonical
source/documentation: this sandbox, README/current-state, SDK findings and dated
checkpoint. No source sharing or production/game modification. The existing
workspace structure gate still fails on .claude and plans, left untouched.

## Scope and implementation

An isolated x86 Lightloader plugin was implemented to suppress repeated ordinary
render/texture-stage assignments. It included explicit host proof, SDK header and
code-pattern checks, named Direct3DCreate8 import discovery, per-object shadow
vtables, cached successful setters, recording bypass, state-block/reset/frame
invalidation, thread bypass and bounded contexts. No game simulation, native code
hooks, graphics quality, resource ownership or draw arguments were intentionally
changed. No new raw game addresses. Initial design was described in commentary;
this durable review was written after implementation, not a prior approved spec.

## Verification

Warnings-as-errors build and actual hook-path tests PASS: duplicate/unknown
states, failures, recording, failed EndStateBlock/Reset, thread bypass, factory
creation/error propagation, device lifetime/reuse, capacity, PE import bounds and
host refusal. An independent 50,000-operation backend oracle found identical
states/HRESULTs: RS backend 25,001/25,001; TSS 12,498/12,500. These varied synthetic
counts do not establish meaningful gameplay call reduction. The actual compiled
x86 DLL loads, exports its API and refuses absent/malformed hosts. A real valid
host/game activation was not tested. Default full sdk-test PASS; all canonical
SDK tests passed and seven regenerated artifacts were unchanged. Pre-existing
SDK git modifications were not reverted or attributed to this task. Some legacy
SDK suites wrote per-run evidence into their old workspace directories; retained.

## Real backend: failure, not an optimization claim

A separate harness loaded system D3D8 and created a hidden 256x256 windowed HAL
device. Adapter identification: NVIDIA GeForce test GPU. It never launched or
attached to F1C. Samples perform 600,000 repeated setters, three rounds rotating
mode order. These are CPU setter costs, not rendering or FPS benchmarks.

| Revision | Direct driver | Hook passthrough | Cached | Cache/direct |
|---|---:|---:|---:|---:|
| Initial measured | 1.256 ms | 2.533 ms | 1.563 ms | 1.244 |
| Revised cache path / Release restore | 1.581 ms | 3.408 ms | 2.403 ms | 1.520 |

Both measured cases cost more than direct calls. Variation prevents ranking the
revisions statistically. The initial harness exited 0xC0000005 before clean
teardown. Two Application event 1000 records identify the temporary benchmark
executables, unknown module and fault offset zero. Restoring the original vptr
before Release and removing whitelist work on cache hits did not solve the
experiment: the revised harness crashed after timings and before reporting its
hidden draw/present result. No game crash occurred. Exact instruction/root cause
is OPEN; internal dispatch beyond the public interface/RTTI/cooperating wrappers
were not modeled. Synthetic COM objects were insufficient native safety proof.

## Disposition

REJECTED. The protected entry now returns FALSE unconditionally before any host,
IAT, target or pinning operations. INI Enabled=0 is also the default, but cannot
reactivate it. The retained native harness is compiled, deliberately not rerun by
final build. This guard is not a passing native gate. Final focused build and
inert-DLL smoke PASS; SHA256
B04C12629D79B513AB22A6386194DCD6E364438273C5BD7393E2FF92CBC40792.

No general FPS increase has been demonstrated or delivered. Do not install or
distribute any prototype. Future investigation should target native geometry
preparation, buffer synchronization or texture handling rather than assume
another state-cache layer helps an already inexpensive driver path. Those
remaining costs are not yet proven bottlenecks. Gameplay, visuals, mods,
multiplayer and long-session acceptance remain pending for any future candidate.

## Preservation and restoration

Complete generated starter, index and SDK checkpoint before-states are under
workspace_organization_audit/RENDER_OPTIMIZER_BEFORE_2026-10-13. Rejected source
snapshots are under trials/rejected_shadow_v1_2026-10-13 and
trials/rejected_shadow_v2_2026-10-13. Rebuilt binary before-states remain in TEMP.
Source/records are retained; no hard deletion or structural move. No game restore
needed. Restore maintained index/checkpoint from the audit copy if required,
while retaining the new records as history. Product status/objective list unchanged.

Wasted-effort check: repeated scoped reads and wrong-directory/glob commands;
full SDK gate once; focused rebuilds followed changes/failures. Retries corrected
unavailable Get-FileHash, compiler warnings and atomic patch formatting. A rejected
README replacement patch did not mutate files; the preserved file was then edited
in place. No deliberate sleeps and minimal polling. Run a real-device smoke earlier
next time, before expanding synthetic COM tests.

Final documentation gate PASS: indexed/current-state review, valid checkpoint JSON/new findings, source hard-refusal before hook operations, final binary hash and UTF-8 checked. Final workspace validator repeats the existing .claude/plans errors, zero warnings. Native and human acceptance remain failed/pending as stated above.
