# Vertex rejection diagnostics review — 2026-10-13

Product: private Render Optimizer trial, verify / human validation pending. Objective: distinguish why active native hook calls produce no fused work. No release authorization; no eligibility broadening or native-address change.

## Evidence and decision

FACT: installed D8FD10737BE3456A40D5685283D92BDCB0D6BDA2E357772E11BC41B0DB970BA5 reported ACTIVE with 24,985 calls and zero fused calls/vertices/thread bypass. Adapter assembly only calls C after signed count >=128 and selector low bits zero. Therefore those two assembly filters cannot explain the C-dispatch count with zero successes. Remaining refusal points: null material, missing material+156 flag 0x20, count overflow or invalid/overlapping buffers. Existing counters cannot distinguish these. Material mode is a hypothesis, not an observed cause.

Static evidence: canonical inspect 0x55D720 and 0x55D900 plus IDA decompile 0x55D720. The native colour function copies source colour only with material flag 0x20; otherwise it computes and packs colour through native context calls. Removing this guard would change output. No new guessed ABI or raw address introduced. inspect of midpoint 0x55D834 failed because recovery index resolves function starts; corrected to enclosing 0x55D720.

## Diagnostic implementation

vertex_trial.hpp optionally returns a rejection reason while retaining predicates, order and packing behavior. vertex_plugin.cpp logs bounded cumulative reject_selector, reject_no_material, reject_material_mode, reject_count and reject_buffer counters; calls explicitly counts C dispatch, excluding assembly size/lighting bypasses. Existing stats export layout unchanged. New counters add diagnostic overhead; no FPS conclusion from diagnostic-build timing. No hot modification of live game.

Actual-plugin vertex_startup_test.cpp exercises all five refusal reasons and checks no destination changes on refusal; successful 128-vertex copy checks exact XYZ/colour bytes and untouched fields. Canonical build PASS: wrapper startup regression, new callback tests, 1,600 captured-x86 differential cases, 2,000 kernel cases, test GPU D3D8 buffer/draw/present/release and protected-DLL host refusal. Candidate SHA256 2BEC473A30961340EFD30B9ED38861B19BBAFF9595471A5D6CC41FBDE424B896, same private version 0.2.0-dev.1 distinguished by hash. Prior candidate retained by build script in TEMP; installed build unchanged at observation time.

At 12:03:22.187069 UTC canonical memory-mcp health reports Game not found, freshness unavailable, elevated reader healthy. A later log sample reverted to WAITING/zero counters; this is not evidence of failure of the diagnostic candidate, which was not yet installed. No new process identity established.

## Preservation, validation and pending work

Before-states of source, startup test, README, current state, verification manifest and checkpoint are retained under workspace_organization_audit/VERTEX_DIAGNOSTICS_BEFORE_2026-10-13 using workspace-relative paths. Diagnostic installation follows existing VERTEX_TRIAL_INSTALL_SPEC_2026-10-13.md: game closed, canonical stage --replace-sdk-owned, only SDK-owned vertex DLL/INI/markers, exact hash check; abort if game restarts, collision or checks fail. Rollback: close game, Enabled=0, full restart. Original EXE/loader/broker/other mods/VBSTRATEGY untouched.

Pending: full SDK gate completion, installation result, then fresh guarded full-weekend reason counters to establish actual rejection cause. ACTIVE and increasing fused_calls still required before matched FPS/frame-time comparison. Human visual/mod/reset/stability acceptance remains pending.

Wasted-effort check: one help command mistakenly used workspace root instead of SDK root; corrected working directory. Midpoint inspect failure corrected to enclosing function. No unnecessary broad tests beyond required full SDK; bounded gate completion waits. Existing full SDK tests generated some evidence directories inside the workspace despite TEMP policy; preserved, not deleted. No cleanup scope added.

## Verification closeout and installation blocker

Full canonical sdk-test completed with exit 0: all tests passed, seven regenerated artifacts unchanged. At 12:06:06.789989 UTC fresh canonical reader identified live PID 30544 (creation epoch 1791893089.5830967), sim_time 39.8112889, vehicle_count 22, replay_mode 1, freshness advancing, age 0.007641 s. Game restarted during verification. Installation aborted before stage invocation because game is running. Diagnostic candidate remains uninstalled; installed hash remains D8FD10737BE3456A40D5685283D92BDCB0D6BDA2E357772E11BC41B0DB970BA5. Installed DLL/INI/ownership markers also preserved in the before-state directory under installed/. Next action requires game closed, then canonical stage and hash verification, then fresh full-weekend reason capture. No failed native diagnostic run exists.

Documentation gate: README links this report, current state/checkpoint/verification manifest distinguish installed versus built candidate, tiered findings record observation and open cause. UTF-8 and links verified. Workspace validator remains two pre-existing .claude/plans errors and zero warnings.
