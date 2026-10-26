# F1 Challenge 99-02 mods and SDK

Every public mod for F1 Challenge 99-02 by Matias, its full source, and the
SDK and research used to build it. Licensed under GPL-3.0 (see `LICENSE`).

```
mods/   one folder per mod, the exact source of the version in Lightloader
sdk/    SDK tools and headers, docs, findings and all research
```

## Mods

| Folder | Mod |
| --- | --- |
| `mods/safety_car` | Safety Car |
| `mods/esc_pause_menu` | ESC Pause Menu |
| `mods/extended_damage_commentary` | Extended Damage Commentary |
| `mods/mechanical_failures` | Mechanical Failures |
| `mods/pitstop_failures` | Pit Stop Failures |
| `mods/red_flag` | Red Flag |
| `mods/multiplayer_overhaul` | Multiplayer Overhaul |
| `mods/fast_loading` | Fast Loading |
| `mods/web_telemetry` | Telemetry (source 1.2.5, newer than the released 1.2.4) |

Install mods through **Lightloader**, the official distribution channel. This
repository is for reading, fixing and improving them. Lightloader itself is
not open source.

## SDK and research

- `sdk/tools/f1c_mod.py` - the SDK front door (run from `sdk/`).
- `sdk/tools/sdk/` - headers and the broker ABI mods build against.
- `sdk/docs/` - offsets, subsystem maps and `MOD_STANDARD.md`.
- `sdk/findings.jsonl` - every proven, disproven and open finding.
- `sdk/research/` - investigations, trial write-ups, research for mods not
  yet released, and reverse-engineering notes.

Decompiled game code and other large analysis data live in the companion
**corpus** repository. Link it with `python sdk/tools/setup_corpus.py PATH`.

## Using an AI coding agent

Point Claude Code, Codex or another agent at this repository. It starts from
`AGENTS.md` and `LLM_GUIDE.md`, which explain everything needed to build and
change mods.

## Contributing

See `CONTRIBUTING.md`.

## Bugs and questions

Send bug reports and questions on Discord to **mpsteppe**. GitHub issues are
not monitored.
