# F1C Modding Playbook

Read this before attempting any behavioral mod. It encodes the exact process a
prior session used to ship the first working mod (cut track = instant DQ), plus
the guardrails that stop you corrupting the game. Follow it top to bottom.

Python: `%USERPROFILE%\AppData\Local\Python\bin\python3.exe`
Exe:    `%USERPROFILE%\Desktop\F1\F1 Challenge V10\F1 Challenge Online.exe`

---

## Iron rules (violate these and you break the game)

1. **Trust only FACT tier.** Raw `.c` in `analysis_pipeline/analyzed/` and
   `decompiled_all/` is ground truth. Global labels in `docs/12`, offsets in
   `docs/13`, and call chains in `docs/31` are GUESS/LEAD — they contain
   DS-worker hallucinations (a docs/31 chain literally mislabeled a network
   function as "track grip"). Verify every label against raw source or with
   `asm_tools.py resolve`.

2. **Never modify the original exe.** Always patch a COPY. Keep original bytes
   for rollback. The `apply_*.py` scripts do this; do the same.

3. **Verify original bytes before writing any patch.** If the bytes don't match
   what you expect, STOP — wrong version or already patched.

4. **Check stack balance on any call redirect.** If the old and new call targets
   clean different numbers of stack bytes (`ret imm`), you MUST add/remove pushes
   or you corrupt the stack. `asm_tools.py redirect` computes this for you.

5. **Test with Frida first, bake to exe second.** The Frida memory-patch version
   is reversible and survives on-disk integrity checks. Confirm behavior live,
   then produce the static exe copy.

---

## The process

### 1. Name the behavior, find the anchor
What do you want to change? Find the global/function that represents it.
- Config-word behavior: `grep` the field name in `tools/ida_strings.csv` and `docs/12`.
- A penalty/event: grep `docs/` for the player-facing string ("Cut Track", "Stop/Go").
- A physics/AI value: pick the runtime global; see `mod_pipeline.py map`.

### 2. Map who touches it
```
python mod_pipeline.py map seeds/<name>.json
```
Reads the dossier. The player-path vs AI-path split shows where behavior diverges.
(Seed spec format: see README.md.)

### 3. Resolve the exact functions (replaces reading 5 .c files by hand)
```
python asm_tools.py resolve <func_addr>
python asm_tools.py resolve <func_addr> --source   # dump full raw .c
```
Gives in one shot: confirmed ISI identity (NASCAR match), size, string anchors,
callers, and the CRITICAL `ret imm` (calling convention / stack cleanup / arg count).
Use this to identify handlers — e.g. the DQ handler is the one whose strings
include "Disqualified:".

### 4. Plan the change
Most behavior mods are one of:
- **Redirect a call** — make decision X call handler B instead of A (see below).
- **Flip a constant** — change a threshold/multiplier (`mod_pipeline.py patch`).
- **Force a branch** — NOP a conditional jump so one path always taken.

### 5. Redirect a call (the common case — replaces all hand-disassembly)
```
python asm_tools.py redirect <caller_func> <old_target> <new_target> [--at <call_va>]
```
It finds the E8 call sites, computes the retarget bytes, and — crucially —
reports the **stack delta**. If delta != 0 it pinpoints the exact `push` to NOP.
- If multiple call sites exist, it warns and lists them. Use `--at` to pick one
  (so you don't accidentally redirect unrelated penalties).
- Emit the reported edits + any NOP as patch bytes.

### 6. Verify + apply to a copy
Model a small `apply_<mod>.py` on `mods/cut_track_dq/apply_cut_track_dq.py`:
- List edits as `(VA, expected_original_hex, new_hex, note)`.
- Verify each original before writing. Abort on mismatch.
- Write a renamed COPY. Never touch the original.

Also emit a Frida version (`mods/cut_track_dq/cut_track_dq.js` template): same
byte edits applied to memory at runtime, with a `restore()`.

### 7. Test in-game
Predict the exact observable ("cut a corner -> instant DQ"). Have the user test.
A failed prediction is valuable — it usually means a doc oversimplification;
go back to the raw source and correct it.

---

## Worked example (fully reproducible with the tools)

Goal: cutting the track = instant DQ.

```
# identify the DQ handler by its string
python asm_tools.py resolve 0x004bc0e0        # -> "Disqualified:", ret imm 4

# redirect the two cut-track penalty calls in the steward routine
python asm_tools.py redirect 0x004bccc0 0x004bbc60 0x004bc0e0 --at 0x004bd0c1  # warning->DQ, delta 0
python asm_tools.py redirect 0x004bccc0 0x004bbd60 0x004bc0e0 --at 0x004bd0b3  # stopgo->DQ, delta 4 -> NOP push at 0x004bd0aa
```

Resulting edits (what `mods/cut_track_dq/apply_cut_track_dq.py` writes):
```
0x004bd0aa  6a0a       -> 9090        NOP the stopgo `push 0xA`
0x004bd0b3  e8a8ecffff -> e828f0ffff  stopgo -> DQ
0x004bd0c1  e89aebffff -> e81af0ffff  warning -> DQ
```
In-game confirmed: any registered cut now disqualifies.

---

## Gotchas learned the hard way

- **thiscall**: `this` is in ECX, not on the stack. Frida `args[0]` = first STACK
  arg (the message), `this.context.ecx` = the object. NativeFunction abi `'thiscall'`.
- **Shared handlers**: warning/stopgo/DQ funcs are called for MANY penalty types.
  Redirecting the FUNCTION affects all of them; redirect the CALL SITE (`--at`) to
  scope to one behavior.
- **Stack cleanup**: F1C is MSVC 6.0 — thiscall/stdcall callee cleans args via
  `ret N`. Redirecting between handlers with different arg counts needs push fixup.
- **Copy protection / AV**: a patched on-disk exe may be flagged or fail an
  integrity check. If so, use the Frida memory patch (post-load).
- **Penalty severity is not just count**: cut-track uses a severity threshold
  `_DAT_005acb30` that can trigger stop/go on the first offense. Don't assume
  "first = lenient" from a doc summary — read the branch.
