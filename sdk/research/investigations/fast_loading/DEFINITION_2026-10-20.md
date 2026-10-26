# Fast loading - definition and first evidence (2026-10-20)

Stage: define -> investigate. Nothing implemented yet.

## Owner intent (Matias, 2026-10-20)

1. Multicore loading: move decode work off the single game thread.
2. Prefetch: read the files a session will need before the game asks.
3. Season-aware roster: the game should look only for cars of the chosen
   season instead of scanning every season, with NO game restart needed
   when the season changes.

## Evidence so far

FACT (live, 1 s process sampling, V10 install, 1995 mod season, 26 cars):
- Adelaide race load 00-00-00-00-00-00, about 13 s, 293 MB read
  (about 23 MB/s), one core pinned at 100% the whole time, working set
  176 -> 955 MB. Conclusion: CPU-bound on one thread, not disk-bound.
- Hockenheim load crashed at 0x55D852 from a missing texture
  (finding v10-1995-hockenheim-26car-crash-missing-footwork-bitmap-2026-10-20).
- Second (warm cache) Adelaide load not yet measured.

FACT (static):
- Roster builder VehicleFileLoader 0x4C3D80 is called once, from
  GameFileLoader 0x42D260 at Game::Enter. It frees the old roster
  (dword_8AC394), recursively lists *.veh under SeasonData\Vehicles
  (dword_5EC3D8 + dword_5EC428), runs VehFileParser 0x4C3200 on each and
  counts successes in dword_8AC390, then calls VehicleDataLoader.
  Because it frees and rebuilds, it is structurally re-callable.
- Per-session AI pick only filters this in-memory list
  (finding ai-field-selection-filters-in-memory-roster-2026-10-20).

## Design direction (inference, to verify)

- Season-aware roster without restart: keep a class index cache
  (path + mtime -> Classes) so boot parses only matching .veh files, and
  re-run 0x4C3D80 for the new season when the season changes in the menu.
  RISK: anything holding a roster index (player car, menu lists, PLR
  Vehicle File lookup dword_8AC38C) goes stale after a rebuild. Must map
  every reader of dword_8AC394/8AC38C before building.
- Season folders are by team (Arrows\1995_FA16); the year prefix is a
  naming habit, not guaranteed for mods, so filter on parsed Classes.
- Multicore: candidates are the zlib inflate sub_5357E0 and texture
  decode; D3D8 surface creation must stay on the game thread.
- Prefetch: low value on its own given the CPU-bound profile.

## Pending

- Warm-cache second load timing.
- Per-step breakdown from session "Profile F1C session load time".
- Boot share of the roster scan (is it worth it at all).
- Reader map of dword_8AC394 / dword_8AC38C for rebuild safety.

## Step 1a result: roster index holders (Haiku static map, Claude-reviewed, 2026-10-20)

Tier: DERIVED (grep + read of decompiled C, not IDB-verified).
- Only writer of count dword_8AC390 and base dword_8AC394: VehicleFileLoader 0x4C3D80 (boot).
- Selected index dword_8AC38C written by VehicleLookupByName 0x4C3F80,
  VehicleNameEnumeration 0x4C3FE0 (sets 0 on miss) and TrackSceneConfigLoader 0x477680.
- Per-car persistent copies: veh+0x4898 index written by NationalityDriverInfoLoader
  0x4C78F0; veh+0x489C cached by VehicleSetupDriverNumber 0x4C7B50; driver
  records in the dword_7D2E48 block by sub_4C0850. Readers in race:
  0x4E2A60, 0x4E3360, VehicleGraphicsSceneLoader 0x4DFCE0, 0x4E45B0, 0x4D4A80.
- Candidate list dword_8AC36C (6-dword object) reset by sub_4C2FE0.
- Review note (Claude): entry-pointer caches listed in function locals live
  only for one call, so they do not matter for a menu-time rebuild. The
  per-car fields are re-written at each session's entrant build, so a
  rebuild is expected safe ONLY while no session is loaded (menu), followed
  by re-running VehicleLookupByName on the PLR Vehicle File and sub_4C2FE0.
  Still open: menu UI lists that may cache roster indices (sub_4159B0
  NameListBuilder, sub_415780 VehicleListBuilder, sub_47F230).

## Step 1b/1c results (Haiku static maps, Claude-reviewed, 2026-10-20)

Tier: DERIVED unless noted.
- VehicleDataLoader 0x4C4240 (end of the roster build) sorts roster entries in
  place, so roster indices are build-specific: after any rebuild every stored
  index must be re-resolved by name/path.
- sub_4C2FC0 (teardown) frees the roster without zeroing dword_8AC394/8AC390;
  a DLL rebuild must go through 0x4C3D80, never 0x4C2FC0 alone.
- Index-holding list objects: sub_4C4D50, VehicleFilteringSelection 0x4C4990,
  VehicleGridSelection 0x4C4AA0 (rebuilt per call by SceneListCleanup), and
  VehicleListBuilderFromGlobalArray 0x415780 (176-byte nodes, persistence
  UNVERIFIED - check before rebuild).
- Season/class filter string: dword_7CE64C (GameFilterDataPath, char*), read
  by GameFilterDataLoader 0x4C0DB0, AIDriverCountSetup 0x4BFD90 and
  SeasonSceneInitialization 0x477720 (strcmp match). NO writer found in the
  decompiled corpus; next step is a live watch of dword_7CE64C while the
  owner changes season in the menu.
- Candidate hooks (INFERRED): GameFilterDataLoader 0x4C0DB0 entry, or
  SeasonSceneInitialization 0x477720.
- Profiler (development/load_profiler) selftest failed on owner PC
  2026-10-20: "cannot map test range (00000000)"; sent back to its session.

## Live season-change watch (2026-10-20, FACT) - corrects step 1c

dword_7CE64C is the player's selected .VEH path, not a season filter. Each
season change rewrote it in place to that season's car (1995..1999 Benetton)
and moved dword_8AC38C; roster count stayed 242. Plan consequence: a DLL can
detect a season change from the year in dword_7CE64C (or a dword_8AC38C
write) and rebuild then. Active-season storage and the writer remain open.

## Profiler boot capture (2026-10-20, FACT, %TEMP%\f1c_loadprof\loadprof_22960.txt)

Boot to main menu only (no race loaded). Window 3.2 s, hooked exclusive 838 ms.
- roster_scan (0x4C3D80) 65 ms total incl. 242 veh_parse 56 ms.
- tex_load 67 calls 434 ms (largest), gzread 153 ms, inflate 137 ms,
  CreateFileA 81 ms, fopen 103 ms incl. Disk ReadFile only 30 ms for 31 MB.
Conclusion: the full-roster scan costs ~65 ms, so a season-only roster saves
under 0.1 s of boot. Season-aware roster is NOT worth its rebuild risk for
speed; recommend dropping it unless wanted for another reason. Race load
breakdown still needed.

## Race-load profile (2026-10-20, FACT, %TEMP%\f1c_loadprof\loadprof_22948.txt)

1995CK_Adelaide, 26 cars, V10, fixed profiler. Race load burst: 11.3 s span,
6.83 s inside hooked code:
- tex_load 450 calls, 4.89 s exclusive (72% of hooked time). D3D
  CreateTexture itself only 23 ms, so the cost is CPU texture decode/convert.
- inflate (MAS zlib) 208,791 calls, 1.08 s.
- CreateFileA 0.42 s, of which LOG\vehgen.scn 244 ms (53 opens: the per-car
  scene file the game writes then re-reads, see VehgenScnFileLoader 0x4C6C40).
- gzread 0.25 s, ReadFile (real disk) 0.10 s for 274 MB.
- ~4.5 s of the span is outside hooked code (not yet attributed).
Pre-race track map burst: tex_load 1.35 s. Boot: roster_scan 74 ms.

Conclusions (DERIVED):
1. Texture decode is the target: decode on worker threads, keep only
   CreateTexture/lock+copy on the game thread. Upper bound ~4-5 s saved.
2. Parallel inflate: ~1 s.
3. vehgen.scn write+reread per car: ~0.25 s, an in-memory path would remove it.
4. Disk prefetch: <=0.1 s, not worth building.
5. Season-only roster: <=0.07 s, drop.
Next: find what tex_load hooks (which function, which formats are slow,
e.g. BMP/JPG convert, mip generation) and attribute the unhooked 4.5 s.

## Step 2 static maps (3 Haikus, Claude-reviewed, 2026-10-20). Tier DERIVED/INFERRED, not timed.

Textures (0x550080):
- Path: name normalise 0x589901 (critical section) -> search loop 0x5427D0 over
  dword_8B8E30 dirs (fopen+full read per dir, 0x543130) or MAS fallback 0x55FE70
  -> builder 0x5429B0 -> 0x542EC0/0x53FED0 -> 0x5008E4 -> decoder dispatch 0x502A15
  -> create 0x53F720 + lock/copy 0x506559/0x50070D on device dword_8B78A0.
- Possible duplicate decodes (#1 in 0x5427D0 path, #2 in RenderingCompositionSetup,
  #3 real in 0x5008E4). Claude review: the 0x500000-0x50B000 block looks like
  statically linked D3DX8; "decode #1/#2" may be header-only image-info probes,
  not full decodes. MUST be counted at runtime before acting.
- Thread hazards: shared error buffer byte_8B6AC8 (0x53C590), dword_5D71F8
  (0x5026A4), heap thread-safety unverified.
- Split point: after 0x502A15 returns >= 0 (decoded buffer in ctx+4/+8);
  D3D work starts in 0x5008E4 at DirectXSurfaceFormatNegotiation.
MAS / inflate:
- MAS entries inflate whole-entry, one call each (0x55FE70 -> 0x59B260 ->
  0x5357E0, z_stream on stack). The 208,791 inflate calls therefore come mostly
  from gzread 0x534F90 used by LineInputReader 0x5351D0 one byte at a time
  (text files: .hdv/.ini/.gen). Hazards: shared archive handle seek/read race
  (0x560230), mesh scratch globals dword_8B8F30/34/3C.
Unattributed 4.5 s:
- Prime suspect: .hdv parser TheSvmHdvVehicleFileParser 0x4F4270, called ~4x
  per car (LargeConfigFunctionWithPhysicsConstants 0x4E45B0 x2, TireModelInit
  0x43E050 x2), driven per car by SceneRenderUpdate 0x4C6D90 from DynMan_Restart.
  Reads text byte-by-byte via LineInputReader -> gzread.
Next: add timing hooks 0x4F4270, 0x4C6D90, 0x4E45B0, 0x43E050, 0x5351D0,
0x4DFCE0, 0x4C6C40, decoder 0x502A15 (count per tex_load), 0x5427D0, 0x543130.

## Round-2 race profile (2026-10-20, FACT, %TEMP%\f1c_loadprof\loadprof_4164.txt)

Race burst 11.05 s span, 10.42 s now inside hooked code (exclusive):
- tex_load body 4.54 s (416 textures). tex_decode 0x502A15 runs exactly 3x per
  texture (416/416) but costs only 315 ms total, so the duplicate decodes are
  CHEAP (header/probe passes). The 4.5 s sits in un-hooked code inside
  0x550080's subtree: suspected D3DX pixel conversion / resample / mip
  generation (0x504508, 0x504933, 0x504F5A, 0x504B37) or lock-copy. NEXT: hook those.
- vehgfx_scene 0x4DFCE0 body 3.17 s exclusive over 26 cars (~122 ms per car),
  NOT textures/meshes. Unknown CPU work (scene node lookups?). NEXT: map.
- MAS one-shot inflate 1.03 s (2648 calls). CORRECTION to Haiku step 2: the
  208k inflate calls are gzread-driven but cost only 41 ms; real inflate cost is MAS.
- hdv_parse 0.14 s excl (0.41 incl), line_input 0.2 s, CreateFileA 0.44 s
  (vehgen.scn 275 ms), disk ReadFile 0.09 s.
Revised targets: (1) texture conversion/mips 4.5 s - an offline-converted
cache (DDS with mips) may beat threading; (2) per-car scene setup 3.2 s;
(3) MAS inflate 1 s on worker threads; (4) vehgen.scn 0.3 s.

## Step 3 static maps (2 Haikus, Claude-reviewed, 2026-10-20). INFERRED until timed.

Per-car scene setup (0x4DFCE0, 3.17 s):
- Prime suspect: EvaluateSceneNodeProperty 0x5453A0 -> RecursiveTreeStringSearch
  0x54BC60. ~33 lookups per car; the node argument is not used as search root
  and the recursion has no early exit, so every lookup walks the whole scene
  tree, which grows with each loaded car => ~O(cars^2). Fix candidate: a
  name->node cache in a DLL hook. Test: per-car time rising with car index.
- Others: sub_4CEF30 + sub_548850 O(count^2) index walk; sub_548890; sub_4E3360.
Texture body (4.54 s):
- Dispatcher D3DContextInit 0x5063A5 tries helpers in order; SurfacePixelCopy
  0x504508 (same-format memcpy) first, then 0x504640, 0x504731, resample
  0x504933 (point), float filter 0x504B37, 2:1 box mip filter 0x504F5A, ...
- A cache of textures already in the target format, size and mip count should
  hit the 0x504508 fast path and skip filtering/resampling; decode and copy remain.
- Next timing hooks: 0x5063A5 (which helper wins), 0x504508/0x504640/0x504731/
  0x504933/0x504B37/0x504F5A, 0x506559, 0x50099C, 0x53FED0, 0x53FD40, 0x53FC30,
  0x5429B0, 0x50B8A9; scene: 0x5453A0, 0x54BC60 (per-car calls and nodes visited).

## Round-3 race profile (2026-10-20, FACT, %TEMP%\f1c_loadprof\loadprof_6108.txt)

Race burst 10.68 s, 10.33 s hooked.
TEXTURES - cause found:
- D3DContextInit 0x5063A5 dispatcher: helper 0x504640 wins 847/862 times and
  costs 4.50 s exclusive (all of the texture cost). 0x504508 same-format copy
  won once; 0x504B37 14 times.
- CreateTexture formats: D3DFMT 827611204 = 'DXT1' for 498 textures (67.4 Mtexels,
  avg 1.2 mips); fmt 21 (A8R8G8B8) 98; fmt 25 (A1R5G5B5) 12. Sizes mostly 256-512.
- DERIVED: the game converts uncompressed sources to DXT1 on the CPU at every
  load (0x504640 = D3DX conversion into the compressed surface). A cache of
  pre-compressed DXT1 DDS files matching size/mips should take the 0x504508
  memcpy path and remove ~4.5 s. Per-texture conversion is also independent
  CPU work, so worker-thread compression is the fallback.
SCENE - O(cars^2) hypothesis DISPROVEN:
- EvaluateSceneNodeProperty 29 calls/car, RecursiveTreeStringSearch 848 visits/car,
  0.2 ms/car, flat across cars 1..26.
- vehgfx_scene exclusive per car varies 47-215 ms with no growth trend
  (per-model cost). The 3.1 s is in un-hooked callees of 0x4DFCE0
  (candidates 0x543FA0 VertexBufferResourceAllocation, 0x4E2D00
  VertexClusteringLODGrouping, 0x4CEF30, 0x55B6B0, 0x4E3360, 0x4E2A60).

## Step 4 static maps + config check (2026-10-20)

- 0x504508 same-format copy fails for DXT1 destinations because source FourCC
  != DXT1 (BMP/TGA/JPG sources). 0x504640 is a per-level read-source/write-
  destination conversion loop (vtable +4 dst, +8 src on DXT1 class off_5ADDCC);
  that it is the DXT compressor is DERIVED, not proven.
- FACT: the PLR has Compressed Textures="1" and MIP Mapping="1"
  (SAVE\Matias Driver\Matias Driver.PLR; UI string "Compressed Textures" at
  0x5D0A68). Zero-code test: set Compressed Textures off and re-profile. If
  0x504640 cost disappears, runtime DXT compression is proven and the option
  alone removes ~4.5 s (cost: ~4-8x texture VRAM, fine on modern GPUs).
- Vehgfx: no O(n^2) found; prime suspect is SceneFileTopLevelParser 0x55B200
  via 0x55B6B0 (per-car .scn parse, no cache) and its handlers 0x55A060,
  0x55A790, 0x55A500; also 0x4E2D00, 0x543FA0, 0x4E3360. Needs timing hooks.
