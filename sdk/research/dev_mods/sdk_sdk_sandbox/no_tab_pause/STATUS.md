**2026-10-02:** merged into Lightloader 2.1.0 as an optional built-in setting (port in
`development/lightloader/v2.0.0-hub-dev.1/lightloader_no_tab_pause.c`). The built-in option stands down when this
stand-alone module is loaded. This sandbox source is unchanged and stays the reference.
[2.1.0 manifest](../../../../development/lightloader/v2.0.0-hub-dev.1/release_2.1.0_2026-10-02/MANIFEST.md).

# No Tab Pause status — 2026-09-02

Pipeline stage: verify.

The supported sandbox candidate builds successfully as a PE32 DLL and is
staged at `testing\no_tab_pause\2026-09-02-candidate`.

- Source SHA-256:
  `DD78161E670D40B4DD6401617EA9F7A6A629B17BDDAAB8F29F69A5698B07F1A3`
- Candidate SHA-256:
  `96FF703329C1290AA7619BFA3E0E4F80C0F0B66B849A20E0506CB7830244FA64`
- Target executable SHA-256:
  `1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003`

Implementation fixes the shutdown path by retaining the original WndProc in a
local callback variable, restoring the real window procedure before close,
and forwarding the initiating close message to that original procedure.
Cooperative unload restores first, waits for active callbacks, and refuses to
unload if either safety condition fails.

Automated state:

- exact-target doctor: PASS;
- sandbox manifest validation: PASS;
- 32-bit canonical build: PASS;
- staged hash equality: PASS;
- full SDK gate: BLOCKED by the stale expected Desktop game path; the active
  verified game is under Documents;
- live install: BLOCKED while PID 524 is running;
- in-game alt-tab and instant-close validation: PENDING PC.

No legacy source, archived DLL, active ASI, executable, or production artifact
was modified.
