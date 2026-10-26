# Qualifying Preservation 0.3.4

Standalone development candidate. [0.3.4 saves game-simulated qualifying](trials/2026-09-27-simulated-save/RESULT.md) (FINISH/NEXT before the end) and checks the race grid against it. [0.3.3 diagnostic timeline](trials/2026-09-27-timeline/RESULT.md): logs EXE hash, session timeline and every FINISH/NEXT press. [0.3.2 refusal diagnostics](trials/2026-09-27-diagnostic/RESULT.md): logs `NOT PRESERVED ...: <reason>` when the game is left to simulate. [0.3.1 static-runtime record](trials/2026-09-27-static-runtime/RESULT.md); [0.3.0 verification and restoration record](trials/2026-09-15-standalone/RESULT.md). Native gameplay validation pending; not promoted for distribution.

Companion: [Grid Restore](../grid_restore/README.md) applies or edits the saved qualifying order as the race starting grid (candidate, native validation pending).

## Behaviour

Save completed qualifying to a new timestamped TSV beside the DLL and prevent FINISH SESSION / NEXT SESSION from simulating extra laps. Native result/grid copying continues. If archive creation, writing or flushing fails, the order remains protected and the failure is logged. A failed disk operation does not mean a durable backup exists. Untimed entries remain untimed.

No runtime EXE filename, hash, PE metadata, EXE-type or broad code-fingerprint checks. No Lightloader type, SDK broker, broker version/capability or registration requirement. Any normal Windows loader that loads this x86 DLL can start it. MinGW runtime is linked statically: imports only KERNEL32.dll and msvcrt.dll, so no MSYS2 or libgcc_s_dw2-1.dll is needed on the player's PC.

The implementation still uses the existing F1C memory addresses and thiscall simulator ABI. The six-byte simulator entry must match the instruction copied into the trampoline; unavailable/conflicting entry bytes cannot be installed. This is an instruction boundary requirement, not an EXE identity restriction. Arbitrarily relocated or rewritten engines are not automatically supported.

Completed qualifying still means session 5, championship mode 3, phase 5/8, finite expired clock, complete unique positions, valid unique vehicle pointers, and every active car inactive and within 0.5 m/s of stationary. Other conditions use the original simulator. This preserves the established completed-session scope.

## Loading and lifetime

Load build/qualifying_preservation.dll at game startup. The module arms its own single-byte simulator entry trap, whose exception handler redirects execution on the calling game thread. It creates no tick hook. The original simulator trampoline remains available for pass-through.

The DLL is pinned until game exit. Cooperative hot-unload from older revisions is no longer supported. Exit the game before replacing the DLL. No INI setup is required. Look for `Standalone 0.3.4 active` in qualifying_preservation_log.txt.

## Build and verify

From the SDK repository root:

```powershell
python tools/f1c_mod.py build qualifying_preservation --exe "%USERPROFILE%\Documents\F1 Challenge V10\F1 Challenge Online.exe"
python tools/f1c_mod.py sdk-test --exe "%USERPROFILE%\Documents\F1 Challenge V10\F1 Challenge Online.exe"
```

The build reference validates generated bindings offline; it is not a runtime identity gate. Native build tests execute the actual trap/trampoline and preservation callback with synthetic vehicle storage. Historical layout tests and broker helper source are retained but are not runtime dependencies.

Pending: native full-weekend FINISH and NEXT transitions, exact before/after grid and time equality, repeated weekends and interaction with other mods. The current game was running during this task, so its installed DLL was not replaced.

## Earlier evidence

[2026-09-27 friend-machine confirmation: `Standalone 0.3.1 active` on friend PC; preservation event still pending](trials/2026-09-27-friend-confirmation/RESULT.md).

[2026-09-27 friend-machine load failure: DLL imports MinGW libgcc_s_dw2-1.dll](trials/2026-09-27-friend-load-failure/RESULT.md).

[September 14 compatibility candidate](trials/2026-09-14-portability/RESULT.md).
[Original mechanism and ABI evidence](../../../tools/sdk/QUALIFYING_PRESERVATION.md).
