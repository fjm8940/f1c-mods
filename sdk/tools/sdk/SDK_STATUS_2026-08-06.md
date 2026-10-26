# Canonical mod SDK status — 2026-08-06

The canonical SDK is operational at public version `1.0.0` for one exact
32-bit executable target. The complete `python tools/f1c_mod.py sdk-test` gate passes.
It is now the active project priority; full replacement-engine recreation is
paused in favor of broad source-like control over the existing game.

## Current coverage

- 101 vehicle fields; 93 are `FACT` or `CONFIRMED`, with 60 padded layout gaps.
- 88 trusted scalar vehicle fields in the allocation-free trace schema.
- 7,886 named globals; 48 are `FACT` or `CONFIRMED`.
- 6,364 function catalogue entries: 22 reviewed callable ABIs and 6,342
  `LEGACY` analysis leads.
- 181 vtable leads and 165 legacy structure leads.
- Recovery index coverage: 7,132 functions and 15,170 call edges.

## Working capabilities

The SDK generates C/C++ layouts and reviewed wrappers, compiles x86 mods,
validates the exact executable, installs reversible expected-byte hooks, queues
worker requests to the game thread, traces trusted state without allocation,
creates isolated sandbox mods, refuses unsafe overwrites, guards deployment and
owned updates, resolves supported binary variants fail-closed, and orchestrates
large multi-subsystem investigation/build plans.

## Remaining expansion

Infrastructure is mature; reviewed semantic breadth is not. Only 22 of 6,364
catalogued functions are approved even at the ABI level, and ABI review alone
does not prove that invoking an internal transition is lifecycle-safe. Expansion
therefore proceeds subsystem-by-subsystem: establish ownership and thread
requirements, validate natural behavior, add reviewed wrappers/commands, build
under `mods/sdk_sandbox`, and promote manually. The SDK is already useful for
safe observation and narrowly reviewed mods, but it is not yet a general
source-level API over every engine system.

The first new breadth batch dispatches 48 bounded DeepSeek workers over 16
high-leverage entry points with three independent candidates per function.
Results remain evidence candidates until deterministic validation,
reconciliation, instruction-level ABI review and runtime/lifecycle review.

## Latest full-gate result

The 2026-08-06 run regenerated the catalogue, vehicle headers, 88-field trace
schema, 22-wrapper reviewed API, and 7,132-function recovery index; passed C and
C++ smoke builds; created and built a temporary x86 DLL; proved create/overwrite
and guarded deployment/update behavior; and passed the mod orchestrator,
variant resolver, runtime validator, shadow-source, architecture, and
reconciliation gates.
