**r5 installed:** native ESC clears UI-active and frame counter; retain the prior UI context on POST RACE MONITOR as well as state5. This corrects r4 resuming driving. SHA C1FA9203; exact native UI-setter regression/full SDK pass; one-click gameplay pending. See latest r5 section in canonical pit report.

**r4 installed:** r3 reached an empty results page. Hold GamePostMainLoop on POST RACE MONITOR so Game_Post/SeasonScoringReset cannot precede native countdown; ACCEPT owns exit. SHA1D22A793; full SDK gate pass; gameplay pending. See latest r4 section in canonical pit investigation.

**r3 supersedes the grid-scheduling implementation below:** GUI release queues vanilla ESC leaf00404F30 for after RestartGate. Normal/maximized monitors covered. Installed D7E50F67; actual-hook/native-transition fixture and full SDK pass; gameplay pending. Canonical evidence: development/mods/pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md at workspace root.

# Leave Circuit icon: native results route

The X is an on-screen button. Physical-X interception is withdrawn. The
exact-target factory maps APP_Button_BackToMain to kind 56 and release
`00475050`. That handler resets scoring, navigates MAIN MENU and returns -1,
suppressing the OSC action. The owner rejected the OSC trial; original OSC restored.

The broker now owns that native UI-thread release detour. Only kind 56 on
MONITORPAGE, offline race type 7, phase >4 and a terminal player are eligible.
It repeats `sub_4A0250`'s native grid sequence: get object 33; build with
`0046BA20(grid,0)`; arm with `0046C030(grid)`; show POST RACE MONITOR. On the
second grid update, `0046C040` invokes `0046BF10(grid,1)` to finalize the race.
Existing pre-finalization cleanup and native ACCEPT `0046FC30` remain active.
Missing resources consume the click without resetting scoring. Duplicate
pending clicks cannot rearm. The hook requires canonical metadata and eight
native code fingerprints; it has guarded detach and in-flight draining.

Canonical interface:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/game_update_broker
python tools/f1c_mod.py sdk-test --suite garage-results
python tools/f1c_mod.py sdk-test
```

For missing corpus functions, `exe-disassemble --format u32` reviews bounded
pointer tables; `--format pointer-matches` finds literal pointer bytes, not
automatically proven code xrefs. Wrong hash, invalid target/size and >256
matches are refused. The fixture executes copied native release/countdown
code with external services stubbed: original reset reproduced, countdown
calls completion exactly once on its second tick; ABI and refusal checks pass.
Full SDK gate passed. Gameplay classifications and presentation remain unverified.

Installed candidate SHA-256:
`76B0F99AF5166C018BA1D4BB9E93E8F31EA56B0E203EAB8E132ED93E6A023E46`.
. Owner test: ESC Retire to Pits → garage → click Leave
Circuit icon → finalized populated Race Results → ACCEPT → main menu.

[Detailed evidence and restoration](../../../../development/mods/pitstop_modding/verification/leave-icon-native-2026-09-23/REPORT.md).
