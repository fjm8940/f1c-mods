**2026-09-22 reader v13:** fixed read-only native vehicle count, replay mode and104-pointer roster; explicit invalid-slot raw diagnostics; bounded scene traversal only for separately checked, explicit role3 identity, and existing SC status slot/hidden/count fields. [Live SC/replay findings, tests, bounds and restoration](../../../development/mods/red_flag/trials/2026-09-22/massa-stuck-r4/SC_LOCATION_INVESTIGATION.md). 13 reader tests and full SDK gate pass; older pipe-version notes below are historical.

**Session context extension (2026-09-15):** all successful live observations include native game/session/race state, pause flag, player-out and player pointer. Fixed exact-target read-only fields, current pipev9. [Widths, tests, captures and historical inference limits](../../../development/mods/red_flag/trials/2026-09-15/sc53-replay-abort/README.md).

**Object graph extension (2026-09-15):** CLI `memory-mcp positions --slot 13`, MCP `f1c_positions(slot=13)`. Optional slot0..127 selects full active/retired target bytes and bounded scene/part ownership. All spatial cells include all three lists. Current pipe v8; use documented start-reader for a fresh helper. [Exact scope, Sato captures, tests, restoration and unresolved semantics](../../../development/mods/red_flag/trials/2026-09-15/retired-object-graph/README.md). Complete flags describe bounded reads, not exhaustive collider ownership. Earlier pipe versions below are historical.

**Position extension (2026-09-15):** CLI `memory-mcp positions`, MCP `f1c_positions`. Includes named valid excluded slots, garage comparison, full terminal vehicle bytes. Fixed read-only operation, no address parameters; v2 pipe launches a fresh reader through start-reader. [Scope, exact bounds, captures, tests, restoration and unresolved collision coverage](../../../development/mods/red_flag/trials/2026-09-15/retired-coordinate-audit/README.md). Six tools now; earlier five-tool counts below are historical.

**Orientation extension (2026-09-14):** corrected vehicle+0x720, validated up-vector, state and bounded observed inversion duration. Current reader must restart before exposing this change. Continuous broker telemetry is authoritative for sampled dwell; MCP observations do not prove continuous history or permanent incapacitation. [Shared review](ORIENTATION_REVIEW_2026-09-14.md).

**Restart service diagnostic extension (2026-09-21):** current reader pipe v10.
An explicit `memory-mcp positions --slot 0` (or another bounded slot) adds
`restart_service`: active vehicle fuel/capacity/compound, penalty queues and
candidate wheel condition; separate player physics fuel/compound and candidate
wheel condition/temperature, with a fixed 5216-byte raw snapshot and hash.
All reads share the existing equal-simulation-time capture window. This is a
fixed exact-target read, not an arbitrary-address interface. Candidate tyre
fields do not establish a complete tyre, puncture or damage restoration contract.
Eleven reader tests pass; a paused native race capture succeeded at
2026-09-21, simulation time 12.5434721. That single sample does not
establish advancing gameplay or restart preservation. Use documented
`start-reader` to select the current helper; older version notes remain historical.
[Implementation, evidence and pending validation](../../../development/mods/red_flag/trials/2026-09-21/policy-v89/completion-work/penalty-hold/README.md).

# F1C read-only memory MCP — 2026-09-14

Product: canonical SDK, implement/candidate. Safety Car remains verify/candidate.
Owner approved a local MCP connection to a hidden elevated session reader.
This is the specification and discoverable task record; validation below is updated after execution.

## Scope and contract

Canonical entry: `python tools/f1c_mod.py memory-mcp ACTION`.
Actions: `start-reader`, `health`, `drivers`, `safety_car`, `serve`, `reader`, `test`, `verify`.
`start-reader` uses Windows elevation once and pythonw, with a hidden window.
Reader expires after eight hours; there is no persistent service, task or network listener.
MCP tools are health, all drivers, Safety Car, bounded trace, and snapshot comparison.
No arbitrary process, address, command, write, injection, breakpoint or debugger endpoint.
Dependencies: existing Python 3.12, mcp 1.28.1 (v1 API), pywin32 and psutil 7.2.2.

Named pipe is local-only, first-instance exclusive, ACL limited to current user and SYSTEM,
with medium integrity label so the ordinary Codex process can connect. Server identifies
the client SID before dispatch; client grants identification-level impersonation only.
JSON allowlist and two-MiB message bounds; no pickle. Requests have read deadlines;
trace accepts 2–10 samples, separated by 0.25–1 seconds. The elevated code remains
user-owned source: this trusts this user's local account and workspace, not hostile code
running as the same user. Do not expose the pipe through a network proxy.

Process handles request only QUERY_INFORMATION | VM_READ. Each observation reconnects,
validates the exact reviewed EXE SHA256 and image base, and includes PID/creation time.
No game match, ambiguity, denied access, changed executable, incomplete reads and repeated
cross-tick reads fail closed. One contiguous vehicle-array read is bounded by equal
simulation-time markers (three attempts); external memory reads are not fully atomic.
Published SC mapping has no verified owner PID; attribution is explicitly unverified.
SC vehicle candidates use names; authoritative SC identity is pending.

Freshness includes UTC capture time, simulation time, heartbeat and advancing/unchanged/
unavailable state. Unchanged does not prove pause. PID reuse and simulation reset clear
comparison continuity. PassingNorm, target and lateral fields do not establish an AI's
mental intention. Existing reviewed fields originate in runtime recipes and SDK lifecycle/
AI interaction headers. No new gameplay field or native command is introduced.

## Acceptance, restoration and abort

Required: deterministic failure/freshness tests, full SDK gate, actual ordinary-client to
elevated-reader IPC, MCP initialization/list/calls, Codex registration, documentation gate.
Native restart/advancing race validation is separate from simulated restart tests. No
gameplay or game installation edits. Stop on unsupported target; do not relax the hash.

Existing CLI, SDK README/checkpoint/contracts/catalogue and gate before-states are under
`../../../workspace_organization_audit/MCP_SETUP_BEFORE_2026-09-14/` with hashes and
RESTORATION.json. Restore exact preserved files if withdrawing this implementation;
retain newly added files as evidence. Codex config preservation recorded after setup.

## Evidence and verification

Initial evidence: earlier hidden elevated reader successfully read all 25 vehicle slots;
ordinary shell access was unreliable. Earlier samples had identical simulation time and
heartbeat, so advancing gameplay was not established. See the indexed trial record
`../../../development/mods/full_safety_car/trials/elevated_read_20260914/MCP_RECOMMENDATION.md`.

Implementation and acceptance results:

- Five deterministic tests pass: operation/parameter bounds, exact target and ambiguity
  rejection, advancing/unchanged/reset/PID-reuse handling, failures clearing history, and
  refusal to compare different identities. These use synthetic states, not native restarts.
- Full `sdk-test --exe` gate passes against the Documents installation's reviewed hash.
  Evidence: `MCP_SDK_GATE_FINAL_2026-09-14.txt`.
- Actual ordinary-client → elevated-reader → game reads pass. Reader PID24956;
  game PID22256; all 25 active slots read. Latest verification sampled simulation
  495.0173237 and heartbeat6262 unchanged. This is fresh reading of unchanged state,
  not evidence that gameplay is advancing.
- Actual MCP initialization, five-tool discovery/read-only annotations and calls pass.
  An out-of-bounds trace is refused; an unsupported operation sent directly over the
  wire is rejected and the next valid connection succeeds. Evidence:
  `MCP_VERIFY_BOUNDARY_2026-09-14.json` (earlier verification files retained).
- Bernd Maylander name candidate: observed speed169.728km/h, cap201.6km/h. The name
  heuristic and mapping owner caveats remain exposed to callers.
- Codex global server `f1c-memory` is enabled with STDIO, the existing Python executable,
  and canonical `tools/f1c_mod.py memory-mcp serve` arguments. `codex mcp get f1c-memory`
  verified configuration. Existing tools in the config were retained. Before-state is
  `%USERPROFILE%\.codex\config.before-f1c-memory-20260914.toml`, byte-verified;
  this private config backup is outside project documentation. For withdrawal, preserve
  current config then disable just this server; use the backup to review the exact delta,
  avoiding replacement of unrelated subsequent settings.
- Workspace validator before/after: same 10 pre-existing errors, zero warnings (missing
  root game paths/randomizer path, .pytest_cache and four production Python files).
  This task does not repair or claim to pass those unrelated structural checks.
- Changed SDK files: backend, front-door command, five focused tests, gate registration,
  README index, tool contract/catalogue, checkpoint and this record/evidence. No game
  installation, gameplay, broker, RCD, production or global product-stage change.

Native advancing-game, actual game exit/relaunch, other-user ACL rejection, and eight-hour
expiry are pending validation. SID access control is configured and current-user access
works; no second-account experiment was performed. No debugger was installed or attached.
The running helper uses the verified reader implementation; later edits only extracted
the client exchange function and extended the verification routine.

## Claude Code registration — 2026-09-30

Claude Code sessions get the same read-only server through project-scoped
`.mcp.json` files in the workspace root and in this repository root. Both launch
through `cmd.exe /d /c "cd /d <SDK root> && python tools1c_mod.py memory-mcp serve"`,
because the front door refuses to run outside the SDK repository root. The first
registration launched from the workspace root and failed with CONNECTION_CLOSED;
this was fixed on 2026-09-30. Approve
the `f1c-memory` server when Claude Code first asks. A local MCP handshake
(`initialize` and `tools/list`) was verified on 2026-09-30; live reads still
need the elevated session reader (`memory-mcp start-reader`).

## Everyday use

Reload Codex's MCP connection or open a new task so it discovers `f1c-memory`.
The reader is running now. Ask for Safety Car status, all drivers or a short trace.
After a reboot/eight-hour session expiry, run the canonical `memory-mcp start-reader`
action (Codex can run it); accept Windows elevation if prompted. Repeated starts check
for an existing healthy reader. No need to run the whole Codex app as administrator.
Reader error logs are local under `%LOCALAPPDATA%\F1CReadOnly\reader-PID.log`.

## Wasted-effort check

- Repeated large reads: none material; reviewed targeted interfaces and existing evidence.
- Unnecessarily broad builds/tests: none; SDK changes require the full gate, rerun after
  implementation/registration edits. Focused checks cover subsequent client-only changes.
- Avoidable failures/retries: first pipe connection failed because the impersonation API
  lives in win32security, not win32pipe. Hidden-reader logs identified it; corrected and
  replaced only our helper processes. One verification summary assumed structuredContent
  rather than text content; corrected parsing. Earlier failed outputs are retained.
- Polling/waiting/idle: bounded pipe waits and test completion waits only; no game polling loop.

Reference contracts: [Codex MCP](https://learn.chatgpt.com/docs/extend/mcp?surface=cli),
[Windows process rights](https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights),
[official Python MCP SDK](https://github.com/modelcontextprotocol/python-sdk).

## Documentation coverage audit — 2026-09-14

Owner asked whether the integration was mentioned at all relevant entry points.
Verified existing coverage: SDK README, this guide, tool contract, tool catalogue,
automation checkpoint and global Codex registration. Missing coverage was corrected
in workspace README/AGENTS, repository README/AGENTS, SDK LLM operating contract,
and Safety Car README/CURRENT_STATUS. The active Safety Car README incorrectly
said the server was not installed; it now links here and labels the proposal historical.

Editor bootstraps (GEMINI.md, .github/copilot-instructions.md, .cursorrules and
.cursor/rules/f1c-sdk.mdc) already require repository AGENTS.md, which now routes
to this guide. They need no duplicate setup instructions. Historical trials,
archives and production docs remain historical; governance PRODUCT_STATUS stays
unchanged because no stage, maturity, path, next gate or blocker changed.

All eight edited documents were byte-preserved before editing under
`../../../workspace_organization_audit/MCP_DOC_COVERAGE_BEFORE_2026-09-14/`
with RESTORATION.json and SHA256 hashes. Restore the listed before-states to undo
this documentation-only correction. No executable, config or gameplay edits.

Verification: new local links resolve; required coverage markers and editor
bootstrap routing checked; documentation gate passed. Workspace validator still
reports the same 10 pre-existing errors and zero warnings. Prior SDK/runtime test
results remain applicable; no build was needed for prose-only changes. Native
advancing-game, actual restart and other pending validation above remain pending.

Wasted-effort check: repeated large reads — none beyond mandatory orientation;
unnecessarily broad builds/tests — none; avoidable failed/retried commands — none;
polling/waiting/idle — none.

Audit verification correction: explicit UTF-8 was required after a Windows-default decoding failure; the coverage check also caught a missing literal server name in the Safety Car pointer, which was added. Recheck passed for all eight documents and four editor bootstrap routes. These are the avoidable retries for this documentation task.
