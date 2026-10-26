# LLM guide: everything you need to build F1 Challenge 99-02 mods

This file is written for coding agents (Claude Code, Codex, Gemini and others)
and for the humans driving them. Read it fully before changing anything. It
describes the same knowledge, tools and workflow the original author uses, so
anyone can build mods to the same standard.

## 1. What this project is

F1 Challenge 99-02 (EA Sports / Image Space Inc., 2003) is a 32-bit Windows
game (`F1.exe`, x86, Direct3D 8). Mods here are native DLLs that the game
loads at startup through a `dinput8.dll` proxy loader. Mods read and write
game memory, hook game functions, and talk to each other through a shared
**broker** with a versioned ABI.

Players install mods through **Lightloader**, the official manager. Lightloader
is closed source, but everything a mod needs to talk to it is public:
`sdk/tools/sdk/f1c_broker_abi.h`, `f1c_broker.h`, `f1c_broker.c` and the
`F1C_MOD_META` resource format (`mods/*/f1c_mod_meta.h`).

## 2. Repositories

| Repo | Contents | License |
| --- | --- | --- |
| this repo | `mods/` (shipped source of every mod) and `sdk/` (tools, headers, docs, findings, all research) | GPL-3.0 |
| corpus repo | decompiled game functions, skeletons, shadow source, analysis indexes, call graph, strings | reference material, see its README |

The corpus is kept apart so the mods repo stays clean. You need it for
`inspect` and any reverse engineering.

## 3. Setup

1. Own a copy of F1 Challenge 99-02. Nothing from the game is in these repos.
2. Install 32-bit MinGW through MSYS2: `pacman -S mingw-w64-i686-gcc`, so
   that `C:\msys64\mingw32\bin\g++.exe` exists. Put that folder on `PATH`.
3. Install Python 3.10 or newer.
4. Clone both repos, then link the corpus into the SDK:
   `python sdk/tools/setup_corpus.py PATH_TO_CORPUS_CLONE`
5. Build a mod by running `build.bat` (or `build.ps1`) in `mods/<mod>/`.
   Set `F1C_SDK_DIR` to the absolute path of `sdk/tools/sdk` first; some
   scripts default to it. Output goes to `build/` or `%TEMP%`.
6. Run the SDK front door from inside `sdk/`:
   `python tools/f1c_mod.py start --json`

## 4. The SDK front door (`sdk/tools/f1c_mod.py`)

Always ask the SDK before guessing about game behavior, offsets or functions.

- `start --json`: current state, priorities and freshness. If
  `freshness.stale` is true, trust `recent_checkpoint_entries` over the
  summary fields.
- `evidence-query findings TEXT --json`: prior art. Search here before
  investigating anything; it is very likely someone already mapped it.
- `inspect ADDRESS_OR_NAME`: decompiled code (needs the corpus), reviewed
  ABI, callers, callees and SDK helper links for one function.
- `python tools/ask.py findings|func|offset|global|token VALUE`: quick
  lookups.

Full rules: `sdk/tools/sdk/LLM_OPERATING_CONTRACT.md`. Tool list:
`sdk/tools/sdk/F1C_TOOL_CATALOG.json`.

## 5. Evidence and findings

`sdk/findings.jsonl` is the shared memory of the project: one JSON claim per
line, with a tier (proven, disproven, open, or RECORDED for unreviewed
checkpoint copies), the evidence, and its source.

- Every new result, including negative ones, gets appended there. A report
  file alone is not discoverable by the next agent.
- Separate what you verified (live memory read, test, trace) from what you
  inferred (static reading, naming). Never promote inference to proven.
- Offsets: `sdk/docs/38-confirmed-offsets.md`, `12-globals-reference.md`.
  Subsystem maps: the numbered files in `sdk/docs/` (AI brain, pit stops,
  cameras, caution flags, voice lines, career, AIW navigation and more).
- Wider research: `sdk/research/` (investigations, trial write-ups per mod,
  research for unreleased mods in `sdk/research/dev_mods/`, history of each
  public mod in `sdk/research/mods/`).

## 6. How a mod is built

Every mod follows `sdk/docs/MOD_STANDARD.md` (lifecycle, folder layout, required
runtime pieces, consent-gated logging, metadata, verification gates, release
and records). New mods start from `sdk/tools/sdk/NEW_MOD_GUIDE.md`.

All 8 mods build from a clean clone, for example
`mods/multiplayer_overhaul/build.bat`. It shows the pattern: `windres` embeds the
`F1C_MOD_META` resource, then `g++`/`gcc` builds a 32-bit DLL against
`F1C_SDK_DIR` and `sdk/mods/sdk`.

Rules that keep mods safe in a 2003 game:

- 32-bit only, static runtime (`-static -static-libgcc`), no exceptions
  crossing into game code.
- Hook through `f1c_hook.h`; resolve addresses through the SDK, not magic
  numbers copied from a forum.
- Talk to other mods only through the broker ABI. Check the ABI version and
  fail closed on mismatch.
- Never block the game thread. Never write memory you have not proven.
- Logging and usage reports only with player consent (MOD_STANDARD section 4).
- Vanilla first: when a mod's feature is off, the game must behave exactly
  as unmodded.

## 7. Testing

- Build output and test files go to `%TEMP%`, never into the repo.
- Static and unit checks first (each mod's `tests/`), then a live test in a
  real game install.
- Live memory reads: confirm the simulation is advancing (sim time or tick
  changes between samples). Identical samples usually mean a paused frame.
- Do not run fuzzers or heavy test runners while the game is running.
- Describe exactly what you tested in game in the pull request.

## 8. Contributing a change

1. Read the mod's files, `sdk/docs/MOD_STANDARD.md` and the related findings.
2. Make the smallest change that fixes the problem.
3. Add or update tests, and put everything you learned in the same pull
   request: new lines in `sdk/findings.jsonl` and a write-up in
   `sdk/research/`. Research is never left out of a pull request.
4. Open a pull request (see `CONTRIBUTING.md`). The maintainer reviews it,
   builds it and ships it through Lightloader.

## 9. Never commit

Game files, decompiled code (it belongs in the corpus repo), binaries, build
output, keys, tokens, passwords, IP addresses, personal paths, or files from
someone else's project without a compatible license.

## 10. Known gaps (help wanted)

- Pit Stop Failures' `live_tests.exe` (a test program) trips one
  `-Werror=maybe-uninitialized` warning on newer GCC.
