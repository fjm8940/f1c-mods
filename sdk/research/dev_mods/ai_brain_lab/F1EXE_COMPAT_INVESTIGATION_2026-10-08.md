# Driver AI Toolkit vs `Desktop\F1\F1.exe` - investigation (2026-10-08)

Stage: investigate (no code changed). Product: Driver AI Toolkit 1.0.3 / ai_brain_lab.

## Target
- `%USERPROFILE%\Desktop\F1\F1.exe`: SHA-256 `D8FB5722210F5A406272360B8D045BFDC057D1FBA1AF6B37014BDB8D7EE2074A`,
  2,772,992 bytes, timestamp 0x3E9C24F6, checksum 0x002A70CC.
- Canonical `F1 Challenge Online.exe`: SHA-256 `1A70E5ED...`, 3,141,632 bytes, checksum 0x00306F7B.

## Findings (VERIFIED = static byte comparison of both files, script in %TEMP%)
1. Same build, same section layout. Differences: .rsrc size, 7 small .text patches, `IsDebuggerPresent` import
   string, a 0x5C01F0 config clamp table (F1.exe max = FLT_MAX where canonical clamps to 15.0 / 6.0 / 1.0), and
   car cap at 0x474A6A (F1.exe 0x17 = 23, canonical 0x68 = 104). Canonical has extra code caves at 0x59DB5B.
2. Every Toolkit DLL dependency is byte-identical in F1.exe: 7 call sites + 4 callee heads of `compat_check()`,
   all 28 rule sites of `abl_rules.h`, and the viewer `LAYOUT_SIGNATURE` (0x4270A0, 0x426CF0). DLL would activate.
3. Lightloader v2.2.6 `LayoutVerdict` gives F1.exe verdict `variant` (checksum/size differ, probe matches) and
   still loads plug-ins (`lightloader_core.c` CoreWorker has no verdict gate). Broker refuses `high_gear_ratio` on
   variants (seen for `F1.exe` in VPS BUG_REPORT_CHECK_2026-10-04); Toolkit does not need it.
4. **Root cause (VERIFIED in source):** viewer `brain_core.py` `Game.__init__` finds the game by process name and
   accepts only names starting with `f1 challenge` or `f1c`. `f1.exe` matches neither, so it raises
   `Game not running.` and `app/main.py try_viewer` retries every 2 s forever. Left live-diagnostics pane stays
   empty; the shared-memory Lab drawer (levers) is name-independent.
   Canonical source: `f1c_ida_decompiled/mods/sdk_sandbox/ai_mind_monitor/brain_core.py` (copied into
   releases by `tools/build_toolkit.py`). Same filter in every release 1.0.0..1.0.3-r2.
5. Secondary (INFERENCE, not tested): F1.exe has no `dinput8.dll` beside it in `Desktop\F1`, so run from there
   no Lightloader/Toolkit loads at all. It must be placed in a game folder. Local LightLoader logs show no run of
   it (only canonical verdicts).

## Proposed fix (not applied, owner decision)
Drop the name filter: pick any process whose AISteering/AISteeringCorrection bytes match `LAYOUT_SIGNATURE`
(the existing layout gate already refuses wrong builds), or at minimum also accept `f1.exe`.
Similar name-only filters: mechanical_failures tools (`IMAGENAME eq F1 Challenge Online.exe`).

## Pending validation
Owner live test: run F1.exe from the game folder, confirm viewer stays "Game not running", then after fix attaches.

## Update: user screenshots (2026-10-08, two runs, pid 1064 / 4692)
OBSERVED: viewer attached (passed name filter + code LAYOUT_SIGNATURE) but every value is garbage: sim 0.00
NOT ADVANCING, session 7340144 / -1882600549, wetness 7.2e-42 / 9.2e+21, random driver names in slots up to
103, "Plugin not loaded: watching only", "no race running".
INFERENCE: since `f1.exe` cannot pass the name filter, the viewer attached to a different process (whose name
starts `f1 challenge` / `f1c`) or the user's exe is not our local `F1.exe` copy. With identical code and
.data size, our `F1.exe` should place SIM_T 0x8AFEA0 / VEH_BASE 0x5EC6D8 identically, so static analysis alone
does not explain the garbage. Plugin-not-loaded also means Lightloader/ai_brain_lab is not in that process.
Needed from the user: process list (name + path), Toolkit VERSION, LightLoader.log, ai_brain_lab_log.txt,
SHA-256 of their exe. Suggested viewer change: show attached exe name/path/hash; require ai_brain_lab shared
memory or SIM_T sanity before showing data.

## Update: fix applied (2026-10-08, Claude, owner: "make sure the mod works with the F1.exe ... from desktop")

Target re-confirmed: `%USERPROFILE%\Desktop\F1 (1).exe` = same file as `Desktop\F1\F1.exe`
(SHA-256 `D8FB5722210F5A40...`).

VERIFIED (static, script `%TEMP%\f1exe_compat\compare.py`, pefile, canonical `F1 Challenge V10\F1 Challenge Online.exe`
`1A70E5ED...`): every hex address in the current plug-in source (`ai_brain_lab.c`, `abl_rules.h`, `abl_levers.h`,
`abl_precedence.h`, `abl_boot.h`, `abl_logic.h`, `levers.json`, `rules.json`) and viewer (`brain_core.py`,
`brain_explain.py`): 71 addresses, 64 byte-identical (16 bytes each), 7 runtime-only (.bss in both), 0 different.
No differing byte range of the two EXEs lies within 64 bytes of any of them. Differences are elsewhere: 0x4227D5/D9,
car cap 0x474A6C/70 (F1.exe 23 cars), 0x4A1362, 0x4A13D4, 0x4AD369, 0x4D597F, code caves 0x59DB5B.., 0x5BD120
(IsDebuggerPresent string), 0x5C01F4.. clamp table, 0x8D8092, resources. Plug-in `6C60B666...` unchanged.

VERIFIED (source): Lightloader v2.0.0-hub-dev.1 runtime has no exe name / hash gate; the name+hash verifiers in
`package_signature_verifier.c` are called only by `package_signature_smoke.c`. F1.exe gets layout verdict `variant`
(diagnostics only). Toolkit setup (`app/toolkit_setup.py`) finds the game by folder (`plugins\lightloader`), not exe.

FIX: `ai_mind_monitor/brain_core.py` `Game.__init__` now finds the game by code fingerprint: F1-looking process
names first, then every other .exe; first process whose `LAYOUT_SIGNATURE` bytes match is used. Other builds are
still refused (message names the exe); elevation error names the exe. Viewer label shows the attached exe name
(`LIVE  F1.exe  pid n`). Before-state: `brain_core.py.BEFORE_20261008_f1exe`.

Tests: `%TEMP%\f1exe_compat\test_attach.py` (mocked process list + memory) 9/9 PASS (F1.exe, F1 (1).exe, canonical,
any renamed exe, other build refused, elevated refused, Lightloader ignored, none, right build wins over wrong).
Real run (game running elevated, shell not): correct refusal "OpenProcess failed: run at the same elevation as the
game (F1 Challenge Online.exe)"; scan 0.43 s. `test_abl_ui.py` 0 failures; viewer replay selftest OK.
Package candidate `releases/driver_ai_toolkit/1.0.5-r2/driver_ai_toolkit-1.0.5-r2.zip`
`2C57DDC9C73B100E506FF1B75712355675DD1B52187674042640116E33227D9A` (includes the 1.0.5 UI redesign).

Still required (INFERENCE + owner test): F1.exe must sit in the game folder next to Lightloader's `dinput8.dll`
(on the Desktop alone nothing loads). NOT live-tested: run F1.exe from the game folder as administrator, open the
toolkit, confirm viewer label `LIVE  F1.exe`, plug-in "DLL ACTIVE", one lever applied. The earlier user garbage-value
screenshots remain unexplained (needs their logs); the new label shows which exe the viewer attached to.
