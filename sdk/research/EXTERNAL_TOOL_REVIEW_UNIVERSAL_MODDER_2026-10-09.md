# External tool review: rehan-remade/universal-modder (2026-10-09)

Source: https://github.com/rehan-remade/universal-modder (MIT). Shallow clone inspected in %TEMP%; nothing installed or copied into the workspace.

## What it is (verified by reading repo)
- Agent skills (mod-any-game, game-recon, reverse-engineering, game-automation, showcase-video, publish-mod, fal-assets, share-field-notes) plus a Python `um` CLI.
- `um win`: gfxcapture window screenshots/recording with game-only audio (ProcLoopback), WinDrive input (refuses input when game not foreground), idle check.
- `um video`: EDL to titled, beat-cut showcase video.
- `um publish check`: blocks shipping game files, decompiled code, leaked keys.
- `knowledge/`: per-game field notes. No F1 Challenge / ISI gMotor notes (checked game list and grep for isi/gmotor/rfactor).

## Fit for F1 (inference)
- Low overlap gain: workspace already has screenshot.py, input.py, gamekey.py, GAME_CONTROL.md, and a deeper KB (findings.jsonl, SDK front door).
- Candidate borrows: (1) game-only audio + gfxcapture recording for showcase videos; (2) publish-check style leak scan in release pipeline; (3) "never send input while owner active" idle rule for live tests.
- Not useful: fal asset generation, engine playbooks for other engines, mashup skills.

## Status
No adoption decision. Pending owner choice on which borrow, if any.
