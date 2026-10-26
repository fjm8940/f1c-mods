# Vertex packing repeat test — 2026-10-13

Owner request: test the proposed optimization. Product: SDK sandbox Render Optimizer; stage: verify standalone kernel, investigate native integration. No gameplay candidate exists yet.

## Observed evidence

Canonical `python tools/f1c_mod.py build render_optimizer` passed on the test GPU. All 2,000 byte-exact and invalid-input cases passed. Real D3D8 buffer Lock/Unlock, draw/present and clean release passed.

| Buffer policy | UV | Baseline ms | Fused ms | Ratio |
|---|---|---|---|---|
| Static | No | 69.993 | 34.049 | 0.486 |
| Static | Yes | 66.506 | 33.720 | 0.507 |
| DISCARD | No | 68.481 | 34.777 | 0.508 |
| DISCARD | Yes | 67.129 | 34.078 | 0.508 |
| NOOVERWRITE | No | 66.476 | 33.686 | 0.507 |
| NOOVERWRITE | Yes | 67.910 | 35.349 | 0.521 |

Each sample contains 100 updates of 65,536 vertices. Baseline is the source-level two-pass oracle, not the original EXE machine code. No intervening GPU draws in timed updates. These numbers do not establish gameplay FPS, GPU contention behavior or visual acceptance.

## Static native boundary evidence

Canonical IDA disassembly query of 0x55D720 (180 instructions) confirms ECX group input, preserved EBX/ESI/EDI and `retn 8`. Renderer input is the address of global object 0x8B78A0. Native zero-renderer-flag branch clears dirty bit 0x400 and sets prepared bit 0x100 before owner+316 check; optional locking precedes writes; group low bits choose lighting routines; owner+20 bit 0x400000 invokes an additional effect before conditional unlock. This is static evidence, not a reviewed callable runtime interface. Source IDB SHA256: 7D88668955ED735AC7DC40E70A4ADBDD44B9A904AE1500A33CED0F44A415186E.

## Limits and next work

No game was launched, patched or installed. The rejected state-cache DLL remains inert, SHA256 B04C12629D79B513AB22A6386194DCD6E364438273C5BD7393E2FF92CBC40792. Native integration, fingerprinted SDK adapter, safe fallback, branch-frequency measurements and guarded full-weekend A/B FPS/frame-time/visual/stability validation remain pending. Full SDK suite was passed in the preceding kernel task; not rerun here because source behavior was unchanged.

Workspace validator before testing: existing unexpected top-level `.claude` and `plans`, zero warnings. No structural changes. Build output is in TEMP. Restoration: no game state changed; README before-state is in workspace_organization_audit/VERTEX_PACK_RETEST_BEFORE_2026-10-13. Wasted-effort check: repeat benchmark independently reproduced the speedup; future effort should target native integration rather than repeating this microbenchmark.
