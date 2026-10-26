# patches/ — the extended vanilla features, as shippable source

**Start at [`INDEX.md`](INDEX.md).** One subfolder per restored feature, each
holding `source.c` (rebuildable by copy-paste), `FEATURE.md` (what it restores,
why it was silent, the exact writes, the evidence) and the built `.dll`.

Two layers live here, and they are not the same thing:

| | What it is | Who uses it |
|---|---|---|
| `manifest.json` | the **registry** of verified byte changes: `{va, orig, new}` | `tools/patch.py`, development only — it hardcodes one developer's exe path and writes to the .exe on disk |
| `<feature>/source.c` | the **shippable** form of those same changes, written into the running process at DLL attach | the mod pack. No game file is modified |

The manifest is the source; the feature folders are generated from it. After
registering a new patch, regenerate:

```bash
python tools/build_pack.py build
```

---

## The registry

`manifest.json` in **this** directory is the real manifest — `tools/patch.py`
resolves it as `mods/extended_vanilla_features/patches/manifest.json`. Register
and apply with the corpus root tool, never a standalone patcher:

```bash
python tools/patch.py status                       # every patch: clean / applied / FOREIGN
python tools/patch.py show player-damage-commentary
python tools/patch.py apply player-damage-commentary
python tools/patch.py revert player-damage-commentary
```

Convention in this folder: **accumulate verified patches, ship them together.**
Nothing here is applied per-session unless you ask.

## Registered 2026-07-27 — cooldown unblocks

The event table (`0x5BF9A0`, stride `0x14`) stores a float cooldown at `+0x0C`.
A value of `FLT_MAX` (`ff ff 7f 7f`) means the event is **deliberately disabled**
— 26 clips ship that way. Ten of them also have a real emitter in the decompiled
corpus, so for those ten the cooldown is the *only* thing keeping them silent.
Those ten are registered below.

New values are not invented — each mirrors the cooldown the game itself gives
the **AI counterpart** of that event class.

| Patch | Sites | Events | FLT_MAX → | Mirrors |
|---|--:|---|---|---|
| `player-damage-commentary` | 6 | 106,107,108,109,110,111 — Hit01-06, player wing/wheel loss | `15.0` | AIHit01/02/07/08 (eids 220-223) |
| `player-retire-commentary` | 3 | 117,118,119 — Retire01/02/04 | `6.0` | AIRetire02-05 (eids 235-238) |
| `player-dq-commentary` | 1 | 149 — RE_PD_DQ, failed to serve stop/go | `1.0` | RE_PD_SG_* (eids 151-153) |

Emitters (why these ten and not the other sixteen):

```
sub_41AFD0_0041afd0.c:13/16/19/22/25/28 -> eids 106/107/109/110/108/111
sub_41B250_0041b250.c:15/19/23          -> eids 119/118/117
sub_41A940_0041a940.c:15  (candidate)   -> eid 149
```

### What is NOT proven

Unblocking the cooldown is **necessary, not demonstrably sufficient**. The
emitter's own guards still apply — `sub_41B250` gates on `SessionType == 7`, so
its three retirement lines are race-only regardless of cooldown. Apply, then
verify live with:

```bash
python tools/watch_damage_commentary.py --hz 10 --secs 120
```

and damage the car in a race. A per-event timestamp moving off `-1.0` is the
decisive "it played" signal.

### Do not bulk-edit all 26 FLT_MAX events

The other 16 have **no emitter at all**. Flipping their cooldown does nothing;
they need emitter code first. See `findings.jsonl` and
`batch/evf_verify/merged.jsonl`.
