# fast_loading (DEV)

Objective: faster race loads using all CPU cores (owner 2026-10-20: "use the maximum
performance of modern systems"). Non-goals now: release packaging, Lightloader host,
texture/MAS work (later layers). Acceptance: byte-identical car maps (Verify PASS on
the test grid), measurable load-time cut, no crash, damage/collision unchanged in game.

Spec: f1c_ida_decompiled/investigations/fast_loading/SPEC_PARALLEL_4DE610_2026-10-20.md
Evidence trail: f1c_ida_decompiled/investigations/fast_loading/ (DEFINITION, HANDOFF).

## Status (2026-10-20)
- Stage: implement (layer 2 prototype = worker threads). Built, NOT yet run in game.
- dll/f1c_fastload.c: hooks 0x4DE610 (queues the ORIGINAL game build to a worker pool),
  joins in all six readers of car+0xBA8 (0x4E3890, 0x4E38D0, 0x4E3940, 0x4E39D0,
  0x4E3A10, 0x4E3A50; full objdump scan of the V10 exe found no other reader).
  Copies the game thread x87 control word to workers. Verify=1 compares with a
  vanilla synchronous build per call.
- tools/run_fastload.ps1 (owner, elevated): builds to %TEMP%\f1c_fastload, launches.
  Flags: -Verify, -Profile (also loads the load profiler), -Threads N.
- Build check: gcc -Wall -Wextra clean (mingw32), thiscall hook ends in ret 8 (objdump).

## Dev exceptions to MOD_STANDARD (must be closed before any release)
R1 own launcher, not the Lightloader host; hooks in DllMain (process suspended).
R2 worker threads write into car objects that the game does not read until the join.
R4-R11, usage reports, crash capture, metadata: not done (prototype).
Worker crash = game crash (no SEH in workers yet). Verify scratch arrays are leaked.

## Pending validation
1. Owner run with -Verify: expect "VERIFY: 78 PASS, 0 FAIL" (Adelaide 26 cars).
2. Owner run with -Profile (no verify): load time vs 7.05 s baseline.
3. Owner: race drives normally, damage/collision look the same.

## Run log
- 2026-10-20 run 1 (-Verify, pid 23868): CRASH loading Adelaide, ntdll c0000005 +0x936B1.
  Log: pool 19 threads, all 78 jobs finished (worker CPU sum 2.67 s), first 13 compared
  with no FAIL, then crash. Cause (high confidence): copy_arr returned the GAME's pointer
  when the count was 0 (malloc(0) gives a real block), and the join then HeapFree'd that
  game CRT block. Fixed: count 0 -> NULL. The threaded build itself was not the cause.
- 2026-10-20 run 2 (-Verify, pid 23616): no crash. VERIFY: 78 PASS, 0 FAIL (all worker
  builds byte-identical to the vanilla build, pointers compared as part/offset). 19 threads,
  worker CPU sum 2.69 s, game thread never waited at the join. FACT for this grid/PC.
- 2026-10-20 run 3 (-Profile, no verify, pid 22852): race load burst 4.64 s vs 7.66 s in
  round 5 with the same profiler (-3.0 s, -39%). x_4E3360 2887 ms -> 220 ms incl.
  Workers finished the 78 builds 3.0 s after the first queue; game thread waited 0.
  Owner: drove, hit something, damage OK. FACT (single run, this PC, 20 cores).

## 2026-10-20 additions (built, not yet run in game)
- vehgen.scn content cache (INI ScnCache=1). Measured offline on this PC: a read open of a
  just-written file costs ~4.5 ms (Defender scan; 26 opens = 117-188 ms; FILE_ATTRIBUTE_TEMPORARY,
  writer-left-open and sequential-scan flags do not help), an unchanged file opens in ~0.01 ms.
  Exe IAT hooks (CreateFileA, WriteFile, CloseHandle, SetFilePointer, SetEndOfFile) capture the
  bytes the game writes, store one copy per content hash in %TEMP%\f1c_fastload\scn_cache and
  redirect the next read open to that unchanged copy. Gain expected from the 2nd load of the
  same cars on. Any seek/truncate/error = vanilla path.
- fast gzgets (INI FastGets=1): 0x5351D0 (zlib 1.1.x gzgets, one gzread(1) per byte) replaced
  for uncompressed streams: refills inbuf with fread(16384) instead of 1-byte freads. Exact by
  reasoning (gzread/gzseek/gzrewind/gztell all use avail_in/total_out); gz_stream offsets
  static-asserted. Compressed streams call the original. Counts logged on detach.
- Not done (not confident yet): texture threading/cache, .HDV parse reuse, vertex buffers.
- Waiting on owner permission: libdeflate (msys2 package mingw-w64-i686-libdeflate 1.26) to
  replace the MAS one-shot inflate 0x59B260 (1.02 s over 2650 calls) with a faster, output-
  identical decoder.

## 2026-10-20 run 4/5 (-Profile, two loads, 22-car grid this time, pids 6652 then 8588)
- Fast gzgets FACT: gzread calls 3.45 M -> 206 k; hdv_parse 450 -> 153 ms incl; 146k gzgets
  served fast, 0 fell back. Load burst 3.90 s then 3.63 s (22 cars, not directly comparable
  with the 26-car 4.64 s). Owner: both loads OK.
- vehgen.scn cache: little gain. The file content contains the grid slot (INSTANCE=SLOTnnn)
  and slot-specific lines, so a car only hits the cache in the same slot as before
  (3 of 22 on the second load). Stays on (harmless), but it is not a real fix; real fix
  would be in-memory parsing (not confident, CRT FILE emulation).
## libdeflate (owner approved download 2026-10-20)
- Installed msys2 mingw-w64-i686-libdeflate 1.26 (pacman, official repo), linked statically.
- 0x59B260 replaced (INI FastInflate=1); any libdeflate failure falls back to zlib. Verify=1
  compares every call against zlib. Built, not yet run.
## 2026-10-20 run 6/7 (libdeflate) FACT
- -Verify (pid 9208): libdeflate 4400 calls, verify 4400 PASS 0 FAIL, zlib fallback 0;
  mesh maps 66/66 PASS. No visual problems reported by owner.
- -Profile (pid 22284): race load burst 3.05 s (22-car grid; 3.63 s before libdeflate,
  same grid size). Session total from 7.66 s/26 cars baseline: all layers together.
- Remaining big items: texture convert 0x504640 0.61 s + decode 0.26 s, vb_alloc 0.48 s excl,
  CreateFileA 0.23 s. Next candidates need new investigation (not started).

## 2026-10-20 startup work
- Sampler run pid 25088: sub_55F0D0 (from RenderUISystemGlobalInit) busy-waits 0.25 s on QPC to
  time RDTSC (CPU MHz). MHz is only compared with 400/1200 (feature flags). New patch
  (INI FastCpuCheck=1): fcomp operand at 0x55F1D0 -> 0.05 s, fmul operand at 0x55F263 -> 20.0
  (shared constants 0x59EB50/0x59E7D8 untouched). Saves ~0.2 s. Built, not yet run.
- 2026-10-20 run pid 21612 (FastCpuCheck on): log "CPU speed check: 0.25 s -> 0.05 s"; startup
  burst 1 1.90 s (2.07-2.11 s in the two runs before). Owner: menu normal. FACT, single run.
- Startup attribution (sampler 6c, main thread, ~2.5 ms/sample, stacks are noisy candidates):
  0-0.8 s exe init + imagehlp/bcrypt/CRYPT32/apphelp/AcLayers (Windows compatibility shim layer
  is active for the exe, likely the run-as-admin setting; not changed, owner's system setting);
  0.4-0.8 s D3D8 device creation (win32u); 0.8-1.6 s DirectInput/HID device enumeration
  (~0.13 s, HID.DLL NtDeviceIoControlFile) and mechanical_failures.dll startup work (~0.08 s);
  1.2-1.9 s icon texture conversion (D3DX float path); 2.7-3.2 s menu car DXT1 compression.
  Next confident-candidate: none yet; texture conversion caching needs D3DX investigation.

## 2026-10-20 texture cache (built, not yet run)
dll/texcache.inc, INI TexCache=1. Hooks 0x50099C = statically linked D3DXLoadSurfaceFromSurface
(stdcall 8, ret 0x20; called by 0x53FD40 with (dest, NULL, NULL, src, NULL, NULL, 4, colorkey)).
Key = 128-bit hash of source pixel rows + both surface descs + filter + colorkey. Miss: original,
then read the dest back and append to %TEMP%\f1c_fastload\texcache\data.bin + index.bin. Hit: copy
stored bytes into the dest. Only full-surface calls into lockable non-DEFAULT-pool dests with known
formats; everything else = original. Verify=1 runs the original on hits and compares bytes.
Not covered yet: mip generation 0x500BC8 (D3DXFilterTexture), image decode.
- 2026-10-20 runs pids 24404 (fill), 24736 (-Verify), 9748 (speed). Only startup + menu were
  exercised (no race). Fill: 1084 stored, misses cost 379 ms. Hit run: 1231 hits in 91 ms;
  startup burst 1 1.88 s and the menu-car burst disappeared. VERIFY: 1226 PASS, 5 FAIL, all
  A8R8G8B8 -> DXT1 (one 1024x1024, three 256x256, one 256x512). Cache NOT trusted until the
  cause is known. Added a diagnostic (second D3DX run + x87 control word in the log).
- 2026-10-20 run pid 24148 (-Verify, menu + race): same 5 DXT1 menu-car textures FAIL (31-1571
  bytes differ vs cache) but two D3DX runs in the same session give identical bytes; x87 cw at
  that time 0x007F (24-bit precision). Race: 746 new textures stored, mesh maps 66/66 PASS.
  Inference: D3DX DXT output depends on FPU state, which differed in the fill session.
  Fix: x87 control word + MXCSR added to the cache key; new cache folder texcache_v2 (old
  %TEMP%\f1c_fastload\texcache is dead dev data, disposable).
- 2026-10-20 runs pid 19172 (fill v2) and 18600 (-Verify, menu + race): texcache verify 2032 PASS,
  0 FAIL (34 new misses stored); libdeflate 4360/4360 PASS; mesh maps 66/66 PASS. FACT.
  Fill run: 1815 misses cost 1.05 s in D3DX; hits average ~0.14 ms vs ~0.58 ms per miss.
- 2026-10-20 run pid 14348 (speed, texcache v2 warm): texcache 1982 hits (295 ms total, ~0.15 ms
  each), 6 misses. Race: tex_53FD40 332 -> 182 ms; d3dx_504640 still 313 ms (mip generation via
  0x500BC8, not cached). Race burst 3.21 s vs 3.05 s earlier: within run-to-run noise, no clear
  overall gain yet. Startup burst 1 1.886 s (unchanged).
- Added mip cache (INI MipCache=1): 0x500BC8 = D3DXFilterTexture (stdcall 4, ret 0x10), called
  (tex, NULL, D3DX_DEFAULT, 5) after the top level load. Key = level-0 bytes + desc + level count
  + filter + FPU state; value = levels 1..n. Built, not yet run.
- 2026-10-20 runs pids 14348/11816/5568 (mip cache fill, -Verify, speed): mipcache verify 45/45
  PASS but only 45 calls (~20 ms total): negligible. texcache verify 2056/2056 PASS. Race burst
  2.90 s and 2.94 s (3.21 s and 3.05 s in earlier runs). Owner: textures look normal.
- Added decode cache (INI FileCache=1): 0x5008E4 = D3DXLoadSurfaceFromFileInMemory (stdcall 9,
  ret 0x24), called from 0x53FED0 (surf, 0, 0, data, size, 0, filter 1, ck, info). Key = file
  bytes + dest desc + filter + colorkey + FPU state; value = 28-byte image info + surface bytes.
  Skips image decode + conversion on a hit. Built, not yet run.
- 2026-10-20 runs pids 19872 (fill, two race loads), 19576 (-Verify), 6540 (speed, game still open
  when read): decode cache verify 1799/1799 PASS, texcache 2065/2065 PASS. Speed run: startup
  burst 1 1.62 s (1.89 s before); race load split in two bursts 1.20 + 1.40 s = ~2.6 s (2.9 s
  before); the game thread now waited 97 ms for the mesh-map workers at the join (loading
  outran them). Single runs.
- PROBLEM: cache size. texcache_v2\data.bin = 948 MB after these runs (decoded surfaces are stored
  uncompressed; every new screen/car adds entries). Needs a size cap + eviction, or compressed
  storage, before any release. Dev only for now.

## 2026-10-20 release candidate 1.0.0-rc.1 (packaging, NOT tested in game)

Stage: release prep (verify gates partly passed). Not deployed, not copied into any game folder,
game not run, production/ not touched. Owner test of the exact hash is still pending.

### What was built
- Release source: `releases/v1.0.0-rc.1/` (copied from `dll/` then changed there). The dev `dll/`
  source is unchanged and still drives the dev launcher.
- Lightloader host entry: export `rfe_1d3b7a91` (same name and contract as esc_pause_menu 1.0.5).
  It validates the protected host (magic, process id, dinput8 module, product id, HUB_2 caps, seal,
  proof) and only then installs the hooks. DllMain does nothing but DisableThreadLibraryCalls.
- Meta: `fast_loading.meta.json` embedded through `fast_loading_meta.rc` (`F1C_MOD_META`).
  id fast_loading, name "Fast Loading", version 1.0.0, author Matias, lightloader ">=2.0.0"
  (same minimum as esc 1.0.5; the header needs HUB_2 caps, so 2.0.0 hub is the floor).
- Settings: `fast_loading.ini` [fastload] with the keys and defaults from the task. Missing or
  invalid value = default (flags accept 0/1 only; Threads 0..32; CacheMaxMB 1..3072).
  Verify is read only in the dev build; the release ignores it.
- Caches moved from %TEMP% to `%LOCALAPPDATA%\Lightloader\fast_loading\` (`texcache`, `scn_cache`).
- Cap (FL-3, see decision conflict below): startup over cap = move data.bin/index.bin to
  data.old.bin/index.old.bin (previous .old pair deleted first, same folder, same names only),
  start empty. During a session, no new entries once the cap would be exceeded (logged once).
- Log: `fast_loading_log.txt` beside the DLL, max 512 KB per session, previous session kept as
  `fast_loading_log_prev.txt`.
- Dev-only verify code (compare, scratch builds, per-call zlib/D3DX comparisons, per-job log lines)
  is behind `#ifdef FASTLOAD_DEV_TOOLS`. The release DLL has no VERIFY/Verify/verify strings.
- Address inventory: `docs/ADDRESS_INVENTORY.md` (15 fixed addresses with expected bytes).
  `tools/check_inventory.sh` passes on the release source; a planted unlisted address makes it fail.
- Build: `releases/v1.0.0-rc.1/build.bat` (gcc -Wall -Wextra -Werror, -Wl,-Bstatic -ldeflate,
  -static-libgcc). Output: `%TEMP%\fastload_rc1\fast_loading.dll`.
- Install notes: `releases/v1.0.0-rc.1/install.txt` (player wording).

### Files
- Created: `releases/v1.0.0-rc.1/{build.bat, fast_loading.c, texcache.inc, fast_loading_meta.rc,
  fast_loading.meta.json, fast_loading.ini, install.txt, protected_plugin_abi.h, f1c_mod_meta.h}`,
  `docs/ADDRESS_INVENTORY.md`, `tools/check_inventory.sh`.
- BEFORE copies (state before this batch): `releases/v1.0.0-rc.1/fast_loading.c.BEFORE_20261020_release`,
  `releases/v1.0.0-rc.1/texcache.inc.BEFORE_20261020_release`, `README.md.BEFORE_20261020_release`.
- Stray duplicate: `releases/README.md.BEFORE_20261020_release` (copy of README made by mistake; the
  correct copy is the one above). Owner to remove or keep; not deleted by the agent.
- `protected_plugin_abi.h` is byte-identical to lightloader `v2.0.0-hub-dev.1/release_2.3.20_wheel_startup_2026-10-13/loader_source/`
  and esc 1.0.5. `f1c_mod_meta.h` is from esc 1.0.5.

### Hashes (build of this session)
- `fast_loading.dll` (99,840 bytes) SHA-256 `2E61566D89F1C902D6BA08E73B156C5BF6EB44C80B321AE6BDB742C67F538404`
  (in `%TEMP%\fastload_rc1\`). Built once; reproducibility not tested.

### Verification done (facts)
- Build: PASS with -Wall -Wextra -Werror (after fixing format-truncation warnings with a bounded
  path helper; no warning suppression flags).
- Exports: `F1CModId`, `rfe_1d3b7a91`. Imports: KERNEL32, msvcrt only (libdeflate is static).
- Dev strings: no VERIFY/Verify/verify/DEV/f1c_fastload/TEMP strings in the release DLL.
- Refusal path (`host_refusal_test.c`, scratchpad, plain process, no game): NULL host, zero magic,
  foreign process id are all refused, and no log file is written. PASS.
- Inventory gate: PASS (15 addresses); negative test FAIL as designed.

### Not verified / not done
- Acceptance path (real Lightloader host calling rfe with a valid proof): NOT tested. Needs the
  owner's in-game test of the exact hash.
- No game run. The 2026-10-20 dev runs measured the speed-ups; this release only re-packages them.
- Startup timing: the log line `init done in N us` is written; the game's own startup time is not.
- Usage reports (f1c_usage.h), crash capture, manager/telemetry/privacy onboarding: NOT DONE.
- Multiplayer gating (R7): NOT done. The speed-ups run in every mode.
- Session reset / 3-race-load survival (R9): NOT tested.
- Worker threads still run game build code that writes car objects before the join (R2 exception
  still open; the mod README exceptions above still apply).
- Log lines are written from the join path on the game thread, not the worker (MOD_STANDARD section 4
  says the worker writes). Open deviation.
- Hook timing (R1, see below) and the exception to R5: each feature checks its own sites, but
  features are independent (one failing feature does not stop the others). All sites of one
  feature are checked before any byte is written.

### Lightloader timing (answer to task 3)
- `rfe_*` is called by Lightloader's plug-in pass (`LoadLightloaderPlugins`) on the `CoreWorker`
  thread. That thread is started from the dinput8 bootstrap DllMain (`LightLoaderStartCoreAsync`), so
  the call is asynchronous with game start: it can run before, during or after the game's own init.
  Source: `development/lightloader/v2.0.0-hub-dev.1/release_2.3.20_wheel_startup_2026-10-13/loader_source/lightloader_core.c`
  (LoadLightloaderPlugins, CoreWorker, LightLoaderDinputBootstrapWorker).
- Consequence (FACT from code, not measured): the CPU speed check (sub_55F0D0, called from
  RenderUISystemGlobalInit) may already have run; startup texture conversion may already have run.
  The first race-load build (0x4DE610) is later and should still be hooked. Patching code while the
  main thread runs can race with an instruction at the patched site. The dev launcher avoided this
  by patching a suspended process. Needs an owner log check: the `init done in` line and whether
  `CPU speed check` and `texture` lines appear.
- Open question for the loader: is there a pre-game hook point? Not decided by the agent.

### Decision conflicts found (reported, not improvised)
- FL-3 (DESIGN_DECISIONS.md) says "when full the cache is cleared". The task says "stop storing new
  entries once the cap would be exceeded during a session". Implemented both: startup clears (moves
  aside) an over-cap cache, and a session stops storing at the cap. Owner to confirm.
- FL-2 says texture caches are "an optional setting in the menu". Implemented as INI keys only; no
  in-game menu. Open.
- MOD_STANDARD section 5 says author `Matias`. The owner rule (memory) says author is always
  Matias; the task says Matias. Used Matias.
- The vehgen.scn cache (ScnCache) is not under the CacheMaxMB cap (the task only caps data.bin).
  Its files are small and keyed by content. Open.

### Next actions (for owner)
1. Owner in-game test of `fast_loading.dll` from `%TEMP%\fastload_rc1` (copy to plugins\lightloader
   only by owner, with the INI). Check the log: `armed`, `init done in`, join lines, cache lines.
2. Decide: pre-game hook point (R1 timing), R2 worker/game-thread rule, R7 multiplayer gate,
   FL-3 wording.
3. Usage/crash capture and manager/telemetry onboarding, if the release goes out.

## 2026-10-20 Opus review of v1.0.0-rc.1 (Haiku build 2E61566D superseded)
- Found: Lightloader calls rfe_* from its worker thread while the game runs, so direct code
  writes could race the game thread. Fixed: all code patches are queued and written by
  apply_patches(): every other thread suspended, write only when no thread EIP is in a patch
  site (CPU check: whole loop 0x55F1C7-0x55F269 incl. return addresses on the top 256 stack
  slots), no heap/CRT/log calls while suspended, up to 200 tries, else nothing is written.
  IAT pointer writes stay direct (atomic 4-byte). Before-state fast_loading.c.BEFORE_20261020_safepatch.
- Inventory: 4 new addresses added; check_inventory PASS (19/19).
- ini: CacheMaxMB comment = emptied at next start when full (FL-3); dev Verify key removed.
  meta lightloader ">=2.0.0" -> ">=2.2.3" (same as esc_pause_menu, protected host V2).
- Build (build.bat, -Werror): fast_loading.dll 8283550C92E4FD2235BB91C009AB3483FA66CF7A95F9209742A57A32595933A9
  (104960 B) in %TEMP%\fastload_rc1. Superseded: 2E61566D (Haiku), 7347F854 (before meta fix).
- Pending owner test with Lightloader 2.4.5 manager DA2B01D0: hooks timing (log), 3 race loads,
  What's new, Career absent.
- 2026-10-20 OWNER TEST PASS (pid 13772, V10, Lightloader manager 2.4.5 rc2 EE09F180, loader 2.4.4):
  fast_loading.dll 8283550C installed in plugins\lightloader. Log: all features armed, "code patches
  written: 11 (attempt 1, other threads paused while writing)", init 27.9 ms, 3 race loads (66 jobs
  each, game thread waited 0 / 11.8 / 0 ms), libdeflate 11145 calls 0 fallback, gzgets 0 fallback,
  texture caches off (default). Owner: What's new box shown, mod listed in Mods, everything loads.
  Owner: "ship".

## Release packaging 2026-10-20 (Lightloader 2.4.5 release prep, NOT deployed)

- Release folder: `development/lightloader/v2.0.0-hub-dev.1/release_2.4.5_2026-10-20/` (record `RELEASE_RECORD.md`).
- Package (esc_pause_menu layout): `fast_loading-v1.0.0-lightloader.zip` = `fast_loading.dll` (8283550C...),
  `fast_loading.ini`, `install.txt`. Zip SHA-256 DB22112224B9A1C698485A2A4D83EFB811CB3676DBF8863017971D645E26C5D0, 52197 bytes.
- Catalog row prepared (`make_catalog.pl`): id `fast_loading`, version 1.0.0, `paid` false, changelog from
  `operations/release_management/changelogs/fast_loading.json`. Its `url` page (`https://mods.palloset.com/fast_loading`)
  does not exist yet; decision in `RELEASE_RECORD.md` section 6.
- Open before publish: validate all managers against the new catalog; production order (`operations/AGENTS.md`);
  usage reports and crash capture (not done); Multiplayer gate (R7, not done).
- Not deployed. Pre-edit copy of this file: `README.md.BEFORE_20261020_ll245`.
- 2026-10-20 DEPLOYED as Fast Loading 1.0.0 with Lightloader 2.4.5 (catalog 4EE39836, backup
  /var/backups/lightloader-245-20261020T151234Z). Record: development/lightloader/v2.0.0-hub-dev.1/release_2.4.5_2026-10-20/DEPLOY_RESULT.md.
