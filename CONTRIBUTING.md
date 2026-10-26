# Contributing

Bug fixes, new features, new mods and new research are all welcome.

## How changes ship

1. Fork this repository and open a pull request.
2. Matias reviews it and may ask for changes.
3. Once merged, Matias builds the mod and releases it through Lightloader.
   Players only get changes through a Lightloader release.

## What goes in a pull request

- **The code change.** One fix or feature per pull request. Follow
  `sdk/docs/MOD_STANDARD.md`.
- **Your research, always.** Anything you found while working goes into the
  same pull request, so the next person (or AI agent) starts where you
  stopped:
  - every new result, including things that turned out false, as a line in
    `sdk/findings.jsonl` (tier, claim, evidence, source);
  - write-ups of investigations in `sdk/research/` (one dated Markdown file
    per investigation is fine);
  - new offsets or structures in the relevant `sdk/docs/` file.
  Large captures or decompiled output go to the corpus repository in a
  matching pull request.
- **How you tested it.** What you ran, and what you saw in game.

## Never commit

Game files, decompiled code (corpus repository only), binaries or build
output, keys, tokens, passwords, IP addresses, personal paths or anyone's
personal details.

By contributing you agree that your work is licensed under GPL-3.0.

## Reporting bugs

Report bugs and questions on Discord to **mpsteppe**, not as GitHub issues.
Include the mod name and version, what happened, what you expected, and steps
to reproduce it. Pull requests are welcome here on GitHub.
