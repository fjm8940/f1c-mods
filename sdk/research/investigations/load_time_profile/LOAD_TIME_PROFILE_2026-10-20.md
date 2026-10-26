# Session load-time profile (2026-10-20)

Product: none yet (pre-definition investigation for a possible load-speed
mod). Stage: investigate. Owner goal (Matias): faster loads on modded seasons
with high-res textures. Tool: `development/load_profiler/` (read-only
profiler DLL + suspended launcher). Findings id:
`load-time-profile-tooling-2026-10-20`.

## Status
- DONE: profiler built (DLL 3998813f..., launcher fffd06ef..., selftest
  5c198370..., sha256 prefixes, `%TEMP%\f1c_loadprof`), compiles with no
  warnings. Hook prologue bytes verified identical in the V10 and Senna
  Edition exes (`Game Versions\`), so the VAs are stable across these builds.
- Self-test PASS 2026-10-20 after fix (fixed-address VirtualAlloc failed, err 487; now image-spanning). summarize.ps1 added (no Python on owner PC). Target load: 1995 Adelaide, 26 cars.
- PENDING (owner): 4 live runs (vanilla / heavy modded, cold and
  warm cache). The game was running while this was built, so no runtime test
  was started (no-runtime-while-driving rule). The ms table below is empty
  until those runs exist. **No timing number in this record is measured yet.**

2026-10-20 burst fix: owner race run (pid 6092) wrote no tables. Most likely cause (INFERENCE): hooked calls never stopped for 2 s once the race was running, so the idle-only burst detector never fired, and there was no dump at exit. Fix: a burst also closes after 2 s of rendering at 20 fps or more; bursts under 200 ms hooked are counted, not written; a final dump runs at DLL_PROCESS_DETACH (TryEnterCriticalSection). Self-test PASS in both modes (manual dump, and exit-only dump).

2026-10-20 round 2 hooks: added hdv_parse 0x4F4270, scene_render_upd 0x4C6D90, physics_cfg 0x4E45B0, tire_init 0x43E050, line_input 0x5351D0, vehgfx_scene 0x4DFCE0, vehgen_scn 0x4C6C40, tex_search 0x5427D0, tex_file_read 0x543130, tex_decode 0x502A15, mas_inflate_1shot 0x59B260. Prologue bytes read from the V10 exe and checked by script (21/21 match); the thunk does not depend on calling convention. New log lines: inflate split by direct caller (gzread / mas 1-shot / other) and a histogram of tex_decode calls per tex_load. Self-test PASS.

2026-10-20 round 3 hooks: D3DX texture path 0x5063A5 (dispatcher, histogram of last helper called inside), 0x504508 0x504640 0x504731 0x504933 0x504B37 0x504F5A 0x506559 0x50099C 0x53FED0 0x53FD40 0x53FC30 0x5429B0 0x50B8A9; scene 0x5453A0 and 0x54BC60 attributed per vehgfx_scene call (car 1..n) with outer-only ms for recursion. CreateTexture histograms (max dim, D3DFORMAT, mean mips). 37/37 prologues match the V10 exe. Five D3DX helpers start with mov eax,imm + call __EH_prolog (C++ EH): only the mov is stolen; an exception through them would desync timing. 0x50099C device-copy vs lock split not separated beyond its hooked children. Self-test PASS (exercises the thunk and IAT only; the new per-car and histogram code is compile-checked, not run).

2026-10-20 round 4 hooks (under vehgfx_scene): 0x55B6B0 scn_file_load (first stack arg = path, logged per car with on-disk size, -1 when not a loose file), 0x55B200, 0x55A060, 0x55A790, 0x55A500, 0x4E2D00, 0x543FA0, 0x557240, 0x4E3360 (mov eax,imm + _chkstk, only mov stolen), 0x4DDAD0, 0x4CEF30. New per-car table: calls/outer-ms of each plus tex_load, inflate, ReadFile under that car. 48/48 prologues match the V10 exe. Self-test PASS (thunk/IAT only).

## Hook points (verified bytes, exe non-ASLR)
| category | VA | role | evidence |
|---|---|---|---|
| file open | 0x587DB0 | static-CRT fopen | F1CM.Probe (proven hook) |
| file I/O | exe IAT kernel32 CreateFileA/ReadFile/CloseHandle | time + bytes by extension | exe imports |
| inflate | 0x5357E0 | zlib inflate core | finding f1c-cubemas-entries-are-zlib |
| inflate | 0x534F90 | zlib gzread (16 KB fread + inflate + crc) | decompile |
| texture | 0x550080 | texture loader (name, size, mip clamp) | decompile, name DERIVED |
| texture | d3d8 IDirect3DDevice8::CreateTexture (vt 20) | GPU object create | D3D8 ABI |
| mesh | 0x550AD0 | MAS mesh/VB/IB loader | decompile, calls inflate wrapper |
| .veh | 0x4C3200 / 0x4C3D80 | VehFileParser / boot roster scan | finding ai-field-selection-... |
| track | 0x44C0E0 | SceneTrackFileLoader | decompile, name DERIVED |
| envelope | 0x4B87C0 / 0x4B8940 | LoadingPhase1/2 | decompile, name DERIVED |

## Results table (to fill from `summarize.py`, exclusive ms)
| run | burst | span ms | file read | inflate | texture | mesh | veh/hdv | track | other | hooked |
|---|---|---|---|---|---|---|---|---|---|---|
| vanilla cold boot | | | | | | | | | | |
| vanilla cold race load | | | | | | | | | | |
| heavy cold boot | | | | | | | | | | |
| heavy cold race load | | | | | | | | | | |
| heavy warm race load | | | | | | | | | | |

## Prior evidence (not from this profiler)
- Sibling finding `v10-1995-hockenheim-26car-crash-missing-footwork-bitmap-2026-10-20`
  (1 s process sampling, V10, one 1995 load): ~18 s load, ~370 MB read,
  ~0.85 core CPU, working set 176 -> 772 MB. Tier: observed once, coarse.
  INFERENCE: near one full core busy for the whole load means the load is
  CPU-bound on one thread, not waiting on the disk (warm cache assumed).
- Boot roster scan reads 271 small text .veh files once
  (`ai-field-selection-filters-in-memory-roster-2026-10-20`).

## Provisional recommendation (INFERENCE, to confirm with the table)
1. **(a) boot season pre-filter: probably not worth it.** It only touches boot,
   271 small text files; expect well under the session-load cost. Build only
   if `roster_scan` excl > ~1 s on the heavy install.
2. **(b) parallel prefetch DLL: only if `file read` is a large share on a
   COLD run.** With ~0.85 core CPU during load, warm loads are CPU-bound, so
   prefetch only helps first loads after boot/reboot. Cheapest variant: a
   worker that reads the picked cars' and track's MAS files sequentially into
   the OS cache (no game-thread work, MOD_STANDARD R2-safe).
3. **(c) parallel/cached inflate: the likely real win if `inflate` + `texture`
   dominate**, but it is the hardest: the game consumes MAS entries serially
   on the game thread. A cache of pre-inflated entries keyed by MAS hash, or
   a worker that inflates ahead into a buffer the hook returns, both need the
   MAS entry API mapped first.
4. Content-side alternative to note for the owner: if `texture` excl
   dominates (decode/resample/mip build of big TGA/BMP), shipping high-res
   textures as pre-mipped DDS in the season may beat any code change.

Decision rule once data exists: the category with the largest exclusive ms on
the heavy race load decides; (b) needs cold-run file read share > ~30 %.

## Restoration / footprint
No game files changed. Binaries and logs only in `%TEMP%\f1c_loadprof`
(disposable per-run output, retention policy section 4). Source under
`development/load_profiler/`.
