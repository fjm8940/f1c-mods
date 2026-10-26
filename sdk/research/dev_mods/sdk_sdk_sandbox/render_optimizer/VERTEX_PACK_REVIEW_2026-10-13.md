# Fused vertex packing: implementation and native-buffer verification

Date: 2026-10-13. Owner continued general FPS/stability work. Canonical source:
render_optimizer sandbox. Parent SDK product stage unchanged. This slice is
implement -> verify for a standalone packing kernel; game integration remains
investigate/specify. No game or production modification, native detour or source
sharing. SDK startup summary was stale; recent checkpoint correctly records the
rejected state-cache prototype, which remains disabled and was not repeated.

## Native evidence and bounded scope

Typed corpus sub_55D720 at 0x55D720 copies XYZ from 48-byte source records into
64-byte destination records, optionally the UV pair at +32/+36. The unlit/source-
colour branch of sub_55D900 at 0x55D900 subsequently copies colour +24 in a second
pass. This provides a bit-preserving fusion candidate. No float computation,
quantization, normal change, texture downgrade or simulation change is needed.

The branch is conditional: renderer+0x13D8 is zero; geometry group is unprepared
or dirty; owner+316 is zero; low group flags select sub_55D900 rather than the
other colour/lighting routines; material+156 has bit 0x20. Optional UV copying
is group flags+56 bit 0x800. A later effect at owner+20 bit 0x400000 must remain
in order. Do not apply the specialization to the lighting-computed, other
lighting dispatch, already-prepared or different-layout paths.

D3DMarkDirty_00556cd0 and sub_556B10 set dirty bit 0x400 and call sub_55D720;
sub_5550E0 also conditionally calls it during drawing. Static presence does not
establish frame frequency or cost. Some packing can be load-time/dirty-only.
Remaining call ABI, exact bytes, thread/lifetime and footprint review is needed
before native integration. Lock and Unlock at 0x540F00/0x540F90 remain LEGACY
callable metadata; no unreviewed function was called by this experiment.

## Implementation

vertex_pack.hpp implements ro_vertex_pack: one pass writes XYZ bytes +0..11,
colour +24..27, optionally UV bytes +32..39. All other destination bytes stay
untouched, source is immutable. It rejects nulls for nonzero count, short
capacities, multiplication/address overflow and overlapping source/destination
ranges before writing. Zero count is a successful no-op. No extra CPU ISA,
heap allocation or graphics-driver wrapping is required.

vertex_pack_benchmark.cpp contains a separate two-pass semantics oracle and a
real D3D8 test. The oracle is explicitly not the original executable's machine
code; it follows the examined loops, with vectorization disabled. Therefore the
benchmark does not prove a speedup over the exact compiled game routine.

## Verification and test GPU timings

2,000 randomized byte-exact comparisons across counts 0..257, both UV modes and
unaligned buffers PASS. Full destination comparison checks untouched fields;
source comparison checks immutability. Bounds/overlap/null/overflow refusal PASS.

The harness loaded system D3D8, identified NVIDIA GeForce test GPU, and used an
original hardware device and 64-byte-FVF vertex buffers. No object vtable or
IAT modification and no game attach. It tested static DEFAULT/WRITEONLY buffers
with NOSYSLOCK, dynamic DISCARD and dynamic NOOVERWRITE. Three rounds alternate
original/fused order; each sample performs 100 updates of 65,536 vertices.
Timings include Lock/copy/Unlock; no draw is issued between timed updates.
NOOVERWRITE does not overwrite data in GPU use in this experiment. It therefore
does not reproduce in-flight GPU contention or native pool suballocation.

| Buffer policy / UV | Two-pass mean | Fused mean | Fused/original |
|---|---:|---:|---:|
| Static, NOSYSLOCK / no UV | 69.662 ms | 34.728 ms | 0.499 |
| Static, NOSYSLOCK / UV | 68.321 ms | 34.926 ms | 0.511 |
| Dynamic, DISCARD / no UV | 66.899 ms | 34.647 ms | 0.518 |
| Dynamic, DISCARD / UV | 69.521 ms | 35.578 ms | 0.512 |
| Dynamic, NOOVERWRITE / no UV | 67.235 ms | 34.543 ms | 0.514 |
| Dynamic, NOOVERWRITE / UV | 68.435 ms | 33.952 ms | 0.496 |

An initial dynamic-DISCARD run also measured approximately 0.50/0.52 ratios.
These repeated results support roughly half the CPU update time in this bounded
microbenchmark, not twice the gameplay FPS or a statistically comprehensive
hardware comparison. Fewer passes over mapped memory are a plausible mechanism;
memory cache/write-combining attributes were not separately measured.

Actual buffer Lock/Unlock, FVF/stream binding, triangle draw, Present and clean
buffer/device release PASS. The draw used the last NOOVERWRITE buffer. No image
readback or human presentation check was done. The old shadow-interface crash
was not encountered on this original-interface path.

Front-door build PASS, including the prior synthetic hook/equivalence and actual
inert DLL refusal tests. Full default sdk-test PASS; all canonical tests passed,
seven regenerated artifacts unchanged. Existing legacy test warnings/output
locations retained. No need for a second full gate after documentation-only edits.

## Status and integration boundary

Standalone kernel verified; NOT yet integrated into F1C. The retained state-cache
DLL remains hard-disabled and byte-identical at SHA256
B04C12629D79B513AB22A6386194DCD6E364438273C5BD7393E2FF92CBC40792.
Do not interpret building that DLL as packaging the new kernel into an active
optimizer. No FPS increase has been delivered or measured in gameplay.

Next integration must preserve exact native guards, flags, renderer thread,
source/destination capacity/layout, alias rejection, lock/unlock ownership and
later effects. Unsupported branches must use the original path. Review complete
callee ABIs/fingerprints before calling them or installing any detour. Establish
hit/update counts and elapsed time in a guarded weekend: if this branch is
rare/load-only, its kernel speedup will not meaningfully improve FPS. Native
visual/mod/restart/device-reset/multiplayer/long-session acceptance is pending.
Abort on parity failure, unsupported identity, unsafe ownership or regressions.

## Preservation, documentation and wasted effort

Before-states of README/current-state/build/verification manifest, index and
checkpoint: workspace_organization_audit/VERTEX_PACK_BEFORE_2026-10-13. Added
kernel/harness here; output and prior binary copies in TEMP only. No structural
move, deletion, game change or restoration needed. Restore maintained docs from
before-states if needed while retaining these research records as history.
Findings/index/checkpoint and current-state distinguish kernel proof from native
integration. Workspace validator retains the existing .claude/plans errors.

Wasted-effort check: initial buffer prior-art output was broader than necessary;
no repeated large material; no failed commands; focused native test broadened
only to check static/DISCARD/NOOVERWRITE policies after the initial result;
full SDK gate once; two process result collections, no deliberate idle/sleeps.
