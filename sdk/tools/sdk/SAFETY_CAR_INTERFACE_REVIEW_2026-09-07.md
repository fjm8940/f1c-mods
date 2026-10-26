# Safety Car interfaces — bounded static review

This review supports the current Safety Car audit-fix test build. It does not qualify runtime gameplay or authorize arbitrary state transitions. Product evidence and disassembly are in `../development/mods/full_safety_car/trials/v0.9.1_auditfix/evidence/restart_abi_2026-09-07` relative to the repository root's parent relationship (full workspace path: `development/mods/full_safety_car/...`).

## DynMan_Restart ABI

Exact owner-supplied executable: `%USERPROFILE%\Documents\F1 Challenge V10\F1 Challenge Online.exe`, SHA256 `1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003`. Canonical `doctor --exe ... --require-exe` passes. Bounded `variant-profile --address ... --length <=64` reads were concatenated over the CSV function boundaries and decoded with Capstone; raw profiles and full decoded instructions are preserved in the product evidence.

- Callee span `0x0042C3B0..0x0042C439`, 138 bytes. First instruction `68 6F 02 00 00`, push immediate 0x26f, is exactly five bytes and has no relative-address relocation.
- It reads neither incoming ECX/EDX nor caller stack arguments. ECX is assigned to globals/vehicle pointers before any use. ESI is pushed/popped. EAX returns the selected vehicle pointer. Final byte is C3, plain return.
- Caller Game_Restart span `0x0042D7A0..0x0042D892`. It calls at `0x0042D831` with no pushed arguments. Its preceding ECX assignment is unused by the callee; the ABI does not require a receiver.
- Independent raw sources `functions/DynMan_Restart_0042c3b0.c` and `functions/Game_Restart_0042d7a0.c` agree on no arguments and EAX return.

Reviewed override: `int32_t (__cdecl *)(void)`, game-thread-only. Safety Car hooks the existing load/restart pass; review is not approval to call this function at arbitrary runtime times. Preserve prologue verification, owned-patch rollback and loader lifecycle constraints.

## Latched AI configuration inputs

PassingNorm +0x44CC: raw `sub_424080_00424080.c` lines 110–114 parses Passing, lines 160–163 scale to 0.9 + Passing*0.001 and clamp at 1.0. `AIDriverStructInit_00424790.c:58` seeds 1.0. `AIRacingLine_00425920.c:264` consumes `this+17612` in its line/gap calculation. Existing name_index evidence also records per-driver live matches and initialization overwrites. This is a baked input, not a racing result. Label `latched_control`; finite bounded values, game-thread ownership, capture/restore and session reset are still necessary.

TrackAggressionEffective +0x44F8: parser line 96 reads TrackAggression; lines 144–147 multiply by AI Realism and floor at 0.01. Initializer line 57 supplies a default. `AISteering_004270a0.c:455,500–505,608–622` reads index 4414 for follow gap, side-by-side spacing and pressure accumulation. Existing name_index records separate live validation. Label `latched_control`; this is not the no-passing switch and arbitrary values are not qualified.

lapNumber remains scoring state. Its cosmetic override is removed from the candidate and the manifest does not request it. Tests explicitly verify that putting the write back is refused. Native lap count/classification is retained; no false SDK semantic classification is introduced.

## Verification and CLI target selection

`test_safety_car_interfaces.py` checks reviewed ABI shape, both input labels, retained scoring-output refusal, and hash-checked SDK test-target propagation/refusal. It is part of the canonical `sdk-test` suite. `sdk-test --exe PATH` supplies a read-only target to tests which previously hard-coded the workspace-root installation; missing or mismatched SHA256 is rejected before spawning tests, and an inherited environment override is discarded. The fixed-address target identity remains unchanged.

Prior catalogue/header states, tests and active guidance were preserved under `workspace_organization_audit/SAFETY_CAR_LINKED_BUILD_BEFORE_2026-09-07`. Final command outcomes and hashes belong to the product linked-build report and checkpoint. Static ABI/input review is verified evidence; in-game timing, coexistence and presentation remain human/runtime gates.
