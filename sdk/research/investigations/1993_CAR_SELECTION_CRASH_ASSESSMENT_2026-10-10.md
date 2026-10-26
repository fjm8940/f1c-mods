# 1993 car-selection crash — 2026-10-10

## Scope and orientation

Owner request: investigate 1993 cars crashing `%USERPROFILE%\Documents\F1 Challenge V10`. Owner clarified: selecting a 1993 car crashes instantly. This is a read-only game-install investigation at **investigate -> verify** within the F1C SDK research platform (platform PRODUCT_STATUS remains implement/candidate). No release, installation, gameplay launch or memory write is authorized or performed by this record. Documentation destination: this assessment, `findings.jsonl`, and linked bounded evidence under `analysis/1993_selection_crash_2026-10-10/`.

Acceptance: identify the failure mechanism from existing evidence, separate verified observations from inference, and leave a reproducible next repair step. Non-goals: replace car assets, change game configuration, alter installed plugins, advance product stages or conduct unrelated crash repairs.

## Conclusion — CONFIRMED mechanism; native repair pending

The immediate crash is consistent with a missing season-history file triggering an unhandled Lua error in the installed OPH `CreateFile` callback. Selecting the first 1993 car causes the game to try opening `SAVE\Matias Driver\F1_1993.bch`. The installed callback executes `io.lines(_afile)` before the native file-open operation can handle or create a missing file. OPH invokes the callback with unprotected `lua_callk`, so the Lua error reaches abort/fail-fast. This is a season/profile history initialization defect; no evidence here establishes a defective 1993 mesh or texture.

## Verified evidence

- **Owner observation:** immediate crash while selecting a 1993 car. Individual team coverage was not supplied.
- **FACT, Windows event:** Application Error 1000 at 2026-10-10 names this Documents installation, `Lua.dll`, exception `0xc0000409`, offset `0x39435`, report ID `028864ac-5f3d-49cc-9f9c-ce078e730086`. Event 1001 follows at 18:39:31.
- **FACT, saved minidump:** `%LOCALAPPDATA%/CrashDumps/F1 Challenge Online.exe.10472.dmp`, SHA-256 `ed571761e1ed501f480e3c33ed387041bfd6b78e0d720b3e1c81d555ce48eb0f`. Exception stream contains `Lua.dll+0x39435`, parameter 0 = 7. Disassembly reaches `int 0x29` after loading ECX = 7; the code alone must not be described as proof of a stack buffer overflow.
- **FACT, dump contents:** faulting-thread stack contains `SAVE\Matias Driver\F1_1993.bch`, `SEASONDATA\F1_1993.gdb` and the installed `LuaScript/main.lua` path. Stack scanning finds candidate return addresses at Lua +0x4128 and +0x2AEA and OPH +0x2E98C/+0x2EB4B. These are scanned addresses, not a fully unwound stack trace.
- **CONFIRMED, installed binary:** OPH RVA `0x2E986` calls IAT `0x100691C4`, resolved by its PE imports to `Lua.dll!lua_callk`. It passes one argument and one result. The return address is OPH +0x2E98C, matching the dump. The nearby global lookup string identifies the `CreateFile` callback. A protected `lua_pcallk` import exists but is not used at this call site.
- **FACT, inspected files:** `F1_1993.bch` is absent from all three profiles (`Matias Driver`, `Matias`, `Matthias Steppe`). Each has history files for the other installed seasons, including 1994 and 1995. History files are player state, not car meshes.
- **CONFIRMED, installed script:** `LuaScript/main.lua`, function `CreateFile`, matches `.bch`, immediately iterates `io.lines(_afile)`, then rewrites `GAMEOPT_opponents`. There is no missing-file check or protected call. The next-start Lua log reports successful fixes for 1995 history, which exists. The original error text is not present in the saved dump; the error text below comes from the controlled reproduction.
- **FACT, offline reproduction:** a 32-bit C harness loads the exact installed Lua DLL, opens only base/string/io libraries, and runs the extracted callback under a protected call so the test itself can report the error. The startup section that rewrites Config.ini is excluded. OPH is mocked only for `noPlayer()`. All fixture I/O stays in a unique `%TEMP%` directory; the game is never launched.

| Isolated case | Result |
|---|---|
| Original callback, nonexistent .bch | Lua runtime error 2: cannot open file, No such file or directory |
| Original callback, existing .bch | Pass; one opponent becomes GAMEOPT_opponents=1 |
| Guarded callback, nonexistent .bch | Pass; missing file remains absent for native initialization |
| Guarded callback, existing .bch | Pass; opponent correction remains functional |

Evidence sources and exact file hashes: [manifest](../analysis/1993_selection_crash_2026-10-10/evidence_manifest.json). Test outcomes: [offline results](../analysis/1993_selection_crash_2026-10-10/offline_results.json). Harness: [source](../analysis/1993_selection_crash_2026-10-10/lua_callback_harness.c), compiled with the existing `C:\msys64\mingw32\bin\gcc.exe`. The four preserved Lua fixture scripts are alongside it; their paths point to disposable TEMP fixtures. [Run locations](../analysis/1993_selection_crash_2026-10-10/run_locations.json).

## Inference and limits

The combined dump, missing-file observation, binary call-site verification and exact-DLL reproduction strongly identify the failure chain. No debugger-unwound trace, captured original Lua error string or repaired native selection was obtained. Do not claim every 1993 team was tested or that every historical crash shares this cause. Earlier Windows events include different modules/offsets and are separate investigations. The WER ReportArchive directory denied read access; the user-local dump provided the necessary evidence without changing permissions.

## Reviewable repair and next validation

A complete [guarded main.lua candidate](../analysis/1993_selection_crash_2026-10-10/main.lua.missing_bch_guard.candidate) is prepared, **not installed**. It adds a read-only existence check at the beginning of `CreateFile`; when a requested file does not exist, the callback returns and allows the native open to proceed. The rest of the installed script is preserved. The extracted function with this exact guard passed the two guarded tests above. A more complete upstream OPH repair should use a protected callback and handle file-open failures, but requires third-party source/owner coordination.

Next action: close the game, preserve the installed script before-state and hash, install the reviewed candidate only under explicit repair scope, then select a 1993 car using the affected profile. Verify no crash, native creation/loading of `F1_1993.bch`, repeat selection and an existing season. Gameplay acceptance remains Matias's gate. Avoid fabricating or copying another season's .bch: it includes season-specific history/settings/opponents. No full SDK build is required for this read-only diagnostic and offline third-party callback test; no SDK runtime interface changed.

Restoration: the game installation is unchanged, so no game rollback is needed. Candidate installation, if later authorized, must preserve/restore the exact original `LuaScript/main.lua` recorded in the manifest. All new workspace artifacts are additive; original findings before-state is hash-preserved under `workspace_organization_audit/1993_CRASH_BEFORE_2026-10-10/`. No bulk output or dump was copied into the workspace. No secrets were read.

## Verification and documentation gate

Four controlled offline outcomes pass; candidate function matches the guarded fixture function; all manifest inputs are hash-recorded. Workspace validator before investigation: **2 existing errors, 0 warnings** (`.claude`, `plans` unexpected top-level directories). These unrelated structural issues were not changed. Final validator result is recorded below. Findings entries link this report through the SDK's normal evidence-query front door. Product status is unchanged because no product stage, path, maturity or release gate changed. Pending validation: repaired native 1993 selection/history initialization and human gameplay acceptance.

Wasted-effort check: initial combined/broad orientation output was truncated and required narrower reads; no broad builds/tests; two SDK inspect attempts used unresolved names/interior addresses and were replaced with exact symbol/binary evidence; WER archive access failed and user-local dump succeeded; no polling or idle wait. Future investigations should start with bounded event summaries and local CrashDumps.

Final workspace validator: 2 unchanged pre-existing errors (.claude, plans), 0 warnings. Final hash verification: all five evidence input files unchanged. Candidate extracted function equals tested guarded function except its comment. Documentation gate PASS: report and referenced artifacts exist, two findings are returned by evidence-query, tiers/limits and pending native validation are explicit.
