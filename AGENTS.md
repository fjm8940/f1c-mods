# Agent instructions

Read `LLM_GUIDE.md` first. It explains the game, the SDK, the evidence
system and how mods are built and tested.

## Before changing anything

1. Read `LLM_GUIDE.md`, `CONTRIBUTING.md` and `sdk/docs/MOD_STANDARD.md`.
2. Read the files of the mod you are changing in `mods/<mod>/`.
3. From `sdk/`, run `python tools/f1c_mod.py start --json`, then
   `python tools/f1c_mod.py evidence-query findings TEXT --json` for your
   topic, and search `sdk/research/`. Use `inspect` for any function or
   address.

## Rules

- Verified evidence beats confidence. Mark every claim as verified or
  inferred.
- Research is part of every pull request: append new results (including
  negative ones) to `sdk/findings.jsonl` and put investigation write-ups in
  `sdk/research/`. A finding that only lives in chat is lost.
- Build output, logs and test files go to `%TEMP%`, never into the repo.
- Never commit game files, decompiled code, binaries, secrets, IP addresses
  or personal details.
- Never run fuzzers or test runners while the game is running.
- Talk to other mods only through the broker ABI in `sdk/tools/sdk`.
- Text files are UTF-8 without BOM. In Windows PowerShell 5.1, always pass
  `-Encoding UTF8`.
- Keep changes small and say in the pull request what you tested in game.
