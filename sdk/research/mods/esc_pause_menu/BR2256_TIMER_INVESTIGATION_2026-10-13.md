# BR2256 timer investigation - 13 October 2026

Scope: investigate BR-20261013-2256E4; ESC Pause Menu shipped/observe -> investigate. Supplements operations/vps/BR_20261013_2256E4_REVIEW_2026-10-13.md. Remote queries read-only; no repair, install or deployment.

## Correction: telemetry version is stale

Report context lists installed ESC 1.1.2. The preserved 1.1.2 release source esc_usage.h:20 defines ESCU_MOD_VERSION as 1.1.1. Therefore the previous review's suggestion of an old ESC DLL from usage version is unsupported. Context is still not an independently verified binary hash. Running core 2.3.22 is independently stated by loader logs; no causal link to its version established.

## Confirmed source defect

Preserved releases/v1.1.2-rc.1/esc_pause_menu.c initializes DWORD next_arm_refresh=0 (line 325), explicitly disarms the broker at startup, then gates first/repeated arm attempts with (LONG)(now-next_arm_refresh)>=0 (line 415), where now is GetTickCount(). At Windows tick values 0x80000000 through 0xFFFFFFFF, the first difference is negative. The deadline stays zero forever until the 32-bit tick wraps. This suppresses the entire arm block and its activation log. The DLL remains loaded and registered, session-end summaries still work, but native Esc passes through because the menu was never armed. Current source has the same pattern. A signed wrap-aware deadline comparison is appropriate only after a deadline has been initialized within its half-range, not against a zero sentinel at arbitrary uptime.

Reproduced the exact 32-bit signed predicate with Python ctypes.c_int32: 0, 1000, 2147483647 => runs; 2147483648, 3000000000, 4294967295 => skipped. Failure interval approximately 24.855 to 49.710 days after boot, repeating each 32-bit wrap. This proves arithmetic/source behavior, not the player's Windows uptime or native runtime execution.

## Correlation and confidence

All three matching ESC summaries have precisely four initial lines: loaded-through-Lightloader, discovery-completed, broker-discovered, pause-menu-registered (sanitized token patterns match the source). No fifth arm-state line appears and refusal/crash counts are zero. The timer defect predicts this exact absence: even multiplayer/unknown-state handling would log its first successful disarmed state because menu_armed starts at -1. This makes the timer defect the strongest source-backed explanation for this incident. Not fully confirmed on the player's machine: system info and uploaded logs provide no boot uptime/GetTickCount, and raw ESC/broker TXT logs are absent.

MenuWithoutPause=0 remains relevant if multiplayer, but cannot alone explain absence of the initial arm-state message. Matching same-install telemetry shows ordinary race sessions with 22 cars/player slot 0 and transition to phase 5; those fields do not establish multiplayer/offline state. No unsupported multiplayer conclusion made.

## Repair specification and pending validation

Initialize the first deadline from GetTickCount() immediately before entering the worker loop, or explicitly perform one first arm refresh before applying the signed deadline comparison. Preserve the periodic 250 ms wrap-safe scheduling. Verify first activation at 0x7fffffff, 0x80000000, 0xffffffff and across wrap, plus offline/multiplayer/unknown-state refusal. Separately correct telemetry version reporting. Exact-game validation and owner release approval remain pending; no source edits or shipped artifact changes in this investigation. A Windows Restart resetting uptime is a diagnostic workaround, not proof or a deployed fix.

SDK prior-art query uptime pause returned zero; prior startup stale objectives unrelated to this owner task. Findings appended for source defect and open incident attribution. Documentation restoration: README before-state in workspace_organization_audit/ESC_BR2256_TIMER_BEFORE_2026-10-13_191522; new report retained as evidence. Indexed from product README. UTF-8 readback checked; facts and inference separated. Validator retains existing .claude/plans errors, no new structural change.

Effort check: a few Windows wildcard and wrong-directory searches failed, corrected with explicit paths. Broad correlated usage output truncated; relevant ESC rows and version source were read explicitly. No builds, polling or idle waits. Narrow arithmetic check was sufficient for the source defect; no game reproduction claimed.
