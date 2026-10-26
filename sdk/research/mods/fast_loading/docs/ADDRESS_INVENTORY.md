# fast_loading 1.0.0 address inventory (MOD_STANDARD R6)

Target: F1 Challenge 99-02 game executable (the Lightloader-supported build that the
expected bytes below were taken from). Every fixed address the release source touches is
listed here with the bytes it must contain before the mod patches it. The hooks check
every site before writing anything (all-or-nothing); a mismatch = that feature is
skipped and logged, and the game loads the normal way. The whole-file EXE hash is not
used (owner standard).

Test: `tools/check_inventory.sh` fails when an address written in the release source
(`0x00[45]xxxxx` form) is missing from this file. Keep the two in step.

Patch method: a 5-byte `E9` jump to the hook, remaining stolen bytes set to `90`. The
original bytes run from a trampoline in a mod-owned executable page.

| Address | Role | Expected bytes (before) | Bytes stolen | Patch | Feature |
|---|---|---|---|---|---|
| 0x004DE610 | per-car mesh map build (queued to workers) | 83 EC 58 8B 44 24 60 | 7 | jump to hook_build | always (Threads > 0) |
| 0x004E3890 | reader of car+0xBA8 (joins workers first) | 53 8B D9 8B 83 A8 0B 00 00 | 9 | jump to join stub | always |
| 0x004E38D0 | reader of car+0xBA8 | 56 8B F1 8B 86 A8 0B 00 00 | 9 | jump to join stub | always |
| 0x004E3940 | reader of car+0xBA8 | 83 EC 18 8B 44 24 20 | 7 | jump to join stub | always |
| 0x004E39D0 | reader of car+0xBA8 | 53 8B D9 8B 83 A8 0B 00 00 | 9 | jump to join stub | always |
| 0x004E3A10 | reader of car+0xBA8 | 53 8B D9 8B 83 A8 0B 00 00 | 9 | jump to join stub | always |
| 0x004E3A50 | reader of car+0xBA8 | 53 8B D9 8B 83 A8 0B 00 00 | 9 | jump to join stub | always |
| 0x005351D0 | zlib gzgets (fast path for uncompressed streams) | 53 55 56 8B F2 | 5 | jump to fl_gzgets | FastGets=1 |
| 0x0058803F | game CRT fread (called by fl_gzgets) | 55 8B EC 56 FF 75 14 | 0 | read only (checked) | FastGets=1 |
| 0x0059B260 | zlib uncompress (one-shot inflate) | 83 EC 38 8B 44 24 3C | 7 | jump to fl_uncompress | FastInflate=1 |
| 0x0050099C | D3DXLoadSurfaceFromSurface (texture convert) | 55 8D 6C 24 A8 | 5 | jump to fl_lsfs | TexCache=1 |
| 0x00500BC8 | D3DXFilterTexture (mip levels) | 55 8B EC 83 EC 64 | 6 | jump to fl_filter | MipCache=1 |
| 0x005008E4 | D3DXLoadSurfaceFromFileInMemory (decode + convert) | 55 8B EC 83 EC 64 | 6 | jump to fl_lsffm | FileCache=1 |
| 0x0055F1D0 | CPU speed check: `fcomp qword [0x59EB50]` (0.25 s) | DC 1D 50 EB 59 00 | 6 | 4-byte operand at +2 set to 0.05 s constant | FastCpuCheck=1 |
| 0x0055F263 | CPU speed check: `fmul qword [0x59E7D8]` (4.0) | DC 0D D8 E7 59 00 | 6 | 4-byte operand at +2 set to 20.0 constant | FastCpuCheck=1 |
| 0x0055F1D2 | operand of the fcomp at 0x55F1D0 (written: 4-byte address) | 50 EB 59 00 | CPU speed check (queued patch) |
| 0x0055F265 | operand of the fmul at 0x55F263 (written: 4-byte address) | D8 E7 59 00 | CPU speed check (queued patch) |
| 0x0055F1C7 | start of the busy range (timing loop head), not written | - | apply_patches busy check |
| 0x0055F269 | end of the busy range (after the fmul), not written | - | apply_patches busy check |

## Indirect references (not patched, read or named only)

| Address | Use | Note |
|---|---|---|
| 0x0059EB50 | double 0.25 used by 0x0055F1D0 | shared constant, never written; not in a hex-literal form in the source (appears only inside byte arrays) |
| 0x0059E7D8 | double 4.0 used by 0x0055F263 | shared constant, never written; same note |
| 0x0053FD40 | caller of D3DXLoadSurfaceFromSurface (texture build) | comment reference only |
| 0x0053FED0 | caller of D3DXLoadSurfaceFromFileInMemory | comment reference only |
| 0x00504640 | texture convert function (cost measure) | comment reference only |

## Not fixed addresses

- Import table entries patched at run time (kernel32: CreateFileA, WriteFile, CloseHandle,
  SetFilePointer, SetEndOfFile) for the vehgen.scn cache. Found by name, not by address.
- Lightloader host export `rfe_1d3b7a91` and `llv_6a20c4e7` (proof) are resolved by name
  from dinput8.dll; they are not patched code.

## Expected results

- Startup log line per feature: `armed` or `skipped`. A skip means the byte sequence above
  did not match, so the game code is not the expected build.
- Verified on the dev build (not this release file): every listed site matched in the
  2026-10-20 runs. That is a dev-run fact, not a release test. Pending: owner in-game test
  of this exact hash.
