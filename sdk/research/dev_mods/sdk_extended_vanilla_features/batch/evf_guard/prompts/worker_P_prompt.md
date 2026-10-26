# WORKER P — extended_vanilla_features / guard analysis

You are worker P of 16. Read the role file in full first:

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\tools\roles\WORKER_EVF.md

Everything in it applies — sourcing, tiers, `UNKNOWN` over guessing, PowerShell
(no `grep`/`head`; use `rg ... | Select-Object -First N`), no live memory, no
writes to shared files.

## Corpus root

    cd %USERPROFILE%\Desktop\f1c_ida_decompiled

## Your output file — this and nothing else

    %USERPROFILE%\Desktop\f1c_ida_decompiled\mods\extended_vanilla_features\batch\evf_guard\output\worker_P.jsonl

One JSON object per line, one per item. Write after each item.

## The question you are answering

Each item is a commentary clip whose **emitter exists and is reachable**, but
which is never heard in game. A previous validated pass established that much;
your job is the next step: **find the exact condition that stops it firing, and
say precisely how to make it fire.**

This is not a survey. The output is meant to be built from.

## Method

1. `python tools/ask.py findings <clip>` and on the emitter name. Prior art
   first — some of these have DISPROVEN attempts logged.
2. Open the emitter `.c`. Find the call that passes your eid. Walk **outward**
   from that call and write down every `if` gating it, innermost first — that is
   the `guard_chain`.
3. For each guard, resolve what it actually tests. A `*(this + 0x1234)` is a
   struct offset: `python tools/ask.py offset 0x1234`. A `dword_XXXXXX` is a
   global: `python tools/ask.py global 0xXXXXXX`. Name it or write `UNKNOWN`.
4. `python tools/ask.py func {emitter_addr}` for callers and guards. Then read
   the CALLER too — a guard in the caller can be the real blocker.
5. Identify the single condition most likely never satisfied in normal play, and
   say why it isn't.
6. Choose the fix and be specific:
   - `patch` — a static byte/constant flip. Give the **VA** and what changes
     (e.g. a `jz` to `jmp`, or a float constant). Check
     `persistence_map.jsonl` if a vehicle offset is involved.
   - `hook` — needs per-frame logic. Name the tick site and the state to poll.
   - `config` — a `.plr`/`.hdv` token already parses into it; name the token.
   - `inject` — nothing to fix; the event only needs queue injection.
   - `UNKNOWN` — you could not determine it. Acceptable.

A `patch` whose VA you did not verify by reading the disassembly or the `.c` is
worse than `UNKNOWN`, because someone will apply it. Do not register patches
(no `tools/patch.py add`) — you only describe.

## Output schema

```json
{"id":"CI-136","clip":"GREEN01","eid":126,"emitter":{"name":"PitSelectionStateMachine","addr":"0x0041b3c0"},"call_site":"functions/PitSelectionStateMachine_0041b3c0.c:58","guard_chain":[{"file_line":"functions/X.c:42","test":"*(this+0x40) == 2","means":"session phase == RACE","tier":"DERIVED"}],"blocking_condition":"one sentence: the guard that is never true, and why","callers":["0x004d2e50 PitTimer"],"fix":{"type":"patch","target":"0x0041b41c","detail":"exactly what to change and to what","risk":"what else this call site affects"},"confidence":"DERIVED","evidence":["file:line or command -> output, one per claim"],"open_questions":["what still needs a live test"]}
```

`fix.type` is one of `patch`, `hook`, `config`, `inject`, `UNKNOWN`.
`confidence` is `DERIVED` or `GUESS` — you cannot self-award CONFIRMED or FACT,
those require a live poke you are not permitted to do.

## Your 1 items


### 1. CI-146 — Scar03 (eid 424)

- emitter: `ConstructorStandingsParser` at `0x00420820`
- what the previous pass found: Emitter eid 424 exists at ConstructorStandingsParser line 245 but guard 'if ( a1 )' is never true; only caller ScoringClassificationDisplay always passes a1=0.

Print `WORKER P DONE — 1 items` when finished.
