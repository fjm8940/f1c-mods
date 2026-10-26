# Main tick passive probe

This isolated SDK experiment observes `MainGameTickUpdate` at `0x0042C690`.
It records call thread, receiver, cadence, session phase, and the persistent
tick counters immediately before and after the original function. It performs
no semantic game-state writes.

It also posts 256 persistent no-op tasks from its worker thread and drains them
inside the hook. The log reports posted, executed, duplicate, wrong-thread,
hook-thread, and queue-drain-thread counts. This is a bounded dispatch proof,
not a general callback API.

The detour is guarded by the canonical executable identity and the exact
whole-instruction entry bytes. A mismatch fails closed. Samples remain in a
fixed in-memory buffer and are written only after the detour is removed.

Build with `python tools/f1c_mod.py build main_tick_probe`, stage with the
canonical `stage` command, then signal clean teardown with
`python tools/f1c_mod.py unload F1C_MAIN_TICK_PROBE_UNLOAD`. Results are written beside the DLL as
`main_tick_probe_samples.csv` and `main_tick_probe_log.txt`.

Use the launcher's staged `f1cm_guard.asi` menu-skip feature for unattended
weekend startup. Do not use the `[AUTOLAUNCH] MODE=QuickRace` path.
