# rc.4 configurable HTTP port, 2026-10-09

Development record. Pipeline: verify, then owner
native/human validation. rc.2 remains installed; rc.3 was built but never
installed; rc.4 carries the rc.3 crossing fix plus the port change and is
built, not installed. Assignment: `DEEPSEEK_PARALLEL_A_PORT_2026-10-09.md`.

## Problem

Observed 2026-10-09 (legacy dossier STOP 3 / CLAUDE_FORMAT_SPOTCHECK):
`HTTP_PORT 8080` was compile-time hard-coded (web_telemetry.c:608). While
`llama-server.exe` held TCP 8080, the log showed
`bind port 8080 failed: 10048` and the dashboard served nothing for the whole
race while capture kept running silently.

## Implementation (rc.4)

- `web_telemetry.ini` `[Mod] Port` beside the DLL: range 1024-65535, default
  8080 (the compiled `HTTP_PORT`). Read once on the init path
  (`rfe_1d3b7a91`) before any thread starts. Missing file/key or invalid value
  falls back to the default and logs the reason (`settings: no
  web_telemetry.ini ...`, `settings: invalid [Mod] Port ...`, `settings:
  [Mod] Port missing ...`).
- Listener: tries the configured port, then the next up-to-5 ports
  (8081-8085 at the default). Each failure logs
  `bind port <n> failed: <winsock error>`; success logs the bound port. If
  none of the six binds: `HTTP disabled: no port available`, socket cleanup,
  HTTP thread exits - capture, race log and worker keep running (no crash, no
  game-thread work; the whole path runs on the HTTP thread only).
- `/health` `port` and the page footer now report the actual bound port (the
  page copy is built once per successful bind).
- A test-build-only `TELEMETRY_INI` override exists for the synthetic
  harness; the shipped DLL reads the ini beside the module only.

Facts vs inference: the parse/fallback/health/footer behavior above is the
implemented and synthetically tested code (FACT for this candidate). Native
in-game behavior (guarded weekend while llama-server holds 8080, or an
ini-set port) is NOT yet observed - owner gate remains.

## Verification

- Canonical build `python tools/f1c_mod.py build mods/sdk_sandbox/web_telemetry`
  with `TELEMETRY_TESTS=1`: **41,106 runtime assertions PASS** (26 new:
  valid/invalid/missing/mixed-section/one-shot `[Mod] Port` parse; occupied
  primary -> +1 fallback; all-six-occupied -> disabled 0). HTTP and Node
  gates unchanged PASS; system-only x86 imports PASS.
- New end-to-end HTTP gates (test_telemetry.py): 18080 held -> binds 18081
  with `/health` and footer agreeing; 18080-18085 held -> no listener and the
  process stays alive; ini `Port=18083` via the test-only override ->
  `/health` 18083 plus footer; invalid `Port=99` -> compiled default; the
  main run asserts the footer shows `port 18080`.
- Synthetic capture+read p50 214 us, p95 229 us, max 248 us (native cost
  unmeasured). This is not native FPS/overhead acceptance.
- Reproducible: two consecutive builds produced DLL SHA256
  **22062C6DFC3FE22F3A46A597ED037655A28BAE51BAD441EA54DB771897D583B6** at
  `%TEMP%\f1c-web-telemetry-4.2.0-rc.4\web_telemetry.dll`.
- Full SDK gate `python tools/f1c_mod.py sdk-test`: **PASS**, all canonical
  tests, regenerated artifacts unchanged (7 files).
- Workspace validator: same 2 pre-existing errors (`.claude`, `plans`),
  0 warnings.

## Preservation and changed files

Before-states: `workspace_organization_audit/WEB_TELEMETRY_PORT_BEFORE_2026-10-09/`
(12 files plus RESTORATION.json). No file was deleted or moved. No install,
no loader/broker/game/settings-store change; the installed rc.2 DLL and
sidecar are untouched.

Changed: `web_telemetry.c` (port setting, bind fallback, disabled path,
`/health`, footer copy, rc.4), `compatibility_runtime.inc` (one-shot load at
init, activation log port), `runtime_test.c`, `test_telemetry.py`,
`web_telemetry.ini` (Port=8080 documented), `build.bat`, `f1c_mod.json`,
`mod.json`, `web_telemetry.dll.mod.json` (rc.4 version/build dir),
`README.md`, `PRIVATE_TEST_README.md`, `VERIFICATION_MANIFEST.json`; this
record, findings.jsonl and the SDK checkpoint. Version bumped rc.3 -> rc.4
because the bytes changed (MOD_STANDARD section 5); rc.3 was never installed.

Documentation gate: this record is indexed from README and the manifest;
exact DLL/source hashes are in the manifest; implemented/synthetic facts are
separated from the pending native gate; UTF-8 edited lines were reread.

PARALLEL A READY - check DS

## Claude review (2026-10-09)

Verdict: ACCEPTED. Verified: candidate %TEMP%\f1c-web-telemetry-4.2.0-rc.4\web_telemetry.dll SHA256 22062C6DFC3FE22F3A46A597ED037655A28BAE51BAD441EA54DB771897D583B6; installed DLL still rc.2 BA87BC2E...; bind loop runs on the HTTP thread, SO_EXCLUSIVEADDRUSE, +5 fallback, "HTTP disabled: no port available" path keeps capture alive. Native acceptance pending (owner: game closed, install via guarded route, full-weekend race with llama-server on 8080 to see fallback 8081 + crossing fix).
