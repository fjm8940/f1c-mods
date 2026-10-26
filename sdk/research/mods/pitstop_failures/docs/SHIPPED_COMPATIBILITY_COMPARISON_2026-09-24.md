# Shipped compatibility comparison — 2026-09-24

Purpose: answer owner question about other mods versus the shipped Pit installer.
Product: Pit Stop Failures, observe; no binary, release or lifecycle change.

Verified by source inspection: Red Flag 1.1.3 and Safety Car 0.10.66 beta1
installer/GameCompatibility.cs scan EXEs by content, not a whole-file hash.
They require 32-bit PE, expected image base, no dynamic-base flag, seven required
section layouts/permissions and 19 32-byte code probes. Filename, timestamp,
checksum and whole-file identity are not matched by that checker. Other required
sections/code still must match; this is not support for arbitrary layouts.
Red Flag package/Install-Beta.ps1 invokes Check-Game.exe and separately checks
recognized loader/broker hashes, packaged-file integrity and game-closed state.
Its dependency/payload hashes must not be confused with an EXE allowlist.

ESC Pause Menu 1.0.2 publication record explicitly documents official Lightloader
host proof, no EXE filename or whole-file hash gate in the client, and retained
broker layout/code-byte/detour checks. This statement is release-record evidence;
no new runtime validation was performed here.

Pit installer r4/r8 still scans EXEs against two complete-file hashes. Its runtime
also retains PE identity/probe checks. Therefore it can reject differently patched
EXEs even when relevant pit code/layout is unchanged. That possibility is an
inference from the differing checks, not a verified customer-file diagnosis.

Correction needed: define pit-specific layout/code checks covering actual pit
reads and broker capabilities, align installer and runtime, test benign file
changes and altered critical code. Do not blindly reuse RF's probes as proof of
pit compatibility and do not simply delete runtime protection. Exact additional
customer EXEs and human/gameplay verification remain pending. No universal-support
claim; no binary or deployment was changed by this comparison.

Sources: development/mods/red_flag/releases/v1.1.3/installer/GameCompatibility.cs;
development/mods/full_safety_car/releases/safetycar-v0.10.66-beta1/installer/GameCompatibility.cs;
Red Flag v1.1.3/package/Install-Beta.ps1; operations/release_management/esc_pause_menu/PUBLICATION_1_0_2.md;
Pit release_candidates/pitstop_failures-v1.0.0-r8/installer/InstallEngine.cs and
r5/source/live_client.cpp already inspected in this task.

Indexed from component README; complete prior README and restoration map in
workspace_organization_audit/PIT_COMPAT_COMPARE_BEFORE_2026-09-24.
Wasted-effort: repeated short compatibility source comparison needed for exact
answer; no builds/tests. One unused rg command used a PowerShell-only flag and
failed, without obscuring the successful source read. No idle polling.
