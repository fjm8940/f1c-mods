# Fable 5 Re-Evaluation — Findings on NEXT_LLM_PLAN.md

Date: 2026-07-23. Answers to section 5 challenges, with evidence. New artifacts:
`prelabel.py` / `prelabel.json`, `vehicle_slot_recover.py` / `vehicle_slot_globals.json`.

---

## Challenge 5 — 0x34F0 conflict: RESOLVED STATICALLY. It is yaw angle.

No poke needed. Static evidence (offset 0x34F0 = decimal 13552):

- `AIMainUpdate` (00424e70), lines 181-182:
  `*(float*)(this+17548) = -sin(*(float*)(this+13552));`
  `*(float*)(this+17556) = -cos(*(float*)(this+13552));`
  A position coordinate is never fed to sin/cos to produce a unit heading vector.
- `SteeringAngleController` (00425670):
  `v4 = atan2(-(target_x - this[0x34E0]), -(target_z - this[0x34E8])); v5 = v4 - this[0x34F0];`
  Classic heading-error: desired bearing minus current yaw.
- Corroboration: neighbors 0x34E0 / 0x34E8 are position X/Z — they are differenced
  against another object's coordinates and passed as a `float*` position to
  `BVHNearestNeighborQuery` (`AISteering` 004270a0, line 323).

Verdict: **0x34F0 = yaw (radians), CONFIRMED (multi-site static)**. The poke-doc
"RenderX" claim is wrong for this offset (likely anchored to a different base or a
transcription slip). Update `name_index.json` accordingly; keep the conflict note.

---

## Challenge 1 — Runtime-address gap: the premise is wrong. The array is static.

`BuildActiveVehiclePointerArray` (0042bc80) proves:

| Fact | Value | Evidence |
|---|---|---|
| Vehicle array base | **0x5EC6D8** (static, not runtime) | loop start `&unk_5EC6D8` |
| Stride | 0x48C0 | `v1 += 4656` on `_DWORD*` (=18624 bytes); `v3 += 18624` |
| Slot count | **104 exactly** | (0x7C54D8−0x5EC6D8)/0x48C0 = 104; loop bound `&byte_7C54D8` |
| Active-pointer array | `dword_7C54F0[104]` (0x7C54F0..0x7C5690) | fill loop |
| Slot-live test | field **0x32C8 != -1** | `v1[3250] != -1` (dword idx 3250 = 0x32C8) |
| 0x5F0DEC identity | **slot0 + 0x4714** — the F1CUI anchor | 0x5F0DEC−0x5EC6D8 = 0x4714 |

So the "driver-array base pointer 0x5F0DEC" is not a runtime pointer at all: it is
a fixed address inside slot 0, and slot 0 is the player (F1CUI reads player
telemetry there). The mapping `our_offset = 0x4714 − F1CUI_value` now has a
structural explanation, not just an empirical fit.

Consequences, already executed by `vehicle_slot_recover.py`:
- Every extracted global in [0x5EC6D8, 0x7C54D8) is really `vehicle[slot].field`:
  **93 refs in 69 functions remapped; 47 immediately gain a proven field name.**
- Field 0x32C8 gets meaning "slot active / driver state (−1 = empty)" — this is
  the same `this+13000` entry guard `AIMainUpdate` checks.

Remaining true runtime gap: functions that walk the array through a register
(extractor logs their fields as base `obj`, offsets intact). Not lost — see
challenge 3: it is an identity-binding problem, not an address-recovery problem.

---

## Challenge 3 — Cross-struct naming: fingerprint seeding works.

The extractor already keeps `base: this` vs `base: obj` separate, so
cross-contamination is structurally avoidable. Cheap identity scheme:

1. **Seeds (proven):** any pointer derived from 0x5EC6D8, `dword_7C54F0[i]`, or
   0x5F0DEC-region reads is vehicle-typed.
2. **Fingerprint:** a function whose `this`-offsets intersect the 58 proven
   vehicle offsets in ≥2 places is vehicle-`this`. Measured: **610 functions**
   qualify today (top: AIMainUpdate 15 matches, AIAbility 11, AIPushLap 8).
   Weight rare offsets (0x4064, 0x32C8) over generic ones (0x0, 0x4) to kill
   false positives.
3. **Propagate:** if f(this) passes `this` unchanged as callee's first arg, the
   callee inherits the struct identity. One pass over `callgraph.json` +
   argument-forwarding info from the ctree.
4. Apply the same scheme per struct for `knowledge_graph/structs.json` (158
   fields): each struct needs its own seed set; names only attach where the
   identity pass proves the base type. Never merge maps.

---

## Challenge 4 — Semantic scaling: 15% of the job deleted deterministically.

`prelabel.py` results on the 5,304 substantive set:

| Action | Count | Meaning |
|---|---:|---|
| template | 260 | getters/setters/ctor-inits/wrappers — prose fully determined by facts |
| cluster_copy | 535 | byte-identical structural signature to a representative; copy + "twin of X" |
| **llm** | **4,509** | remaining real job (was 5,304) |

Further cuts available: near-duplicate clustering (signature minus literals),
vtable-slot grouping, and the challenge-3 identity pass (a vehicle-typed getter
with a named field needs no LLM at all).

---

## Challenge 2 — Deeper indirection: do it, but bounded.

Recommended design for `skeleton_extract.py`: forward pass over the Hex-Rays
ctree per function, tracking a small abstract state: each local ∈ {this+k,
*(this+k), global G, G+k, unknown}. Assignments propagate; `+= stride` loops
mark the local as `array(base, stride)` and record accesses as
`(base, stride, field_off)` instead of dropping them. Cap chain depth at 3 and
mark anything past it unknown — over-reporting phantom fields is worse than a
blank. The tire sub-struct (stride 0xF8) then falls out as
`array(this+K, 0xF8)` records. This directly feeds the identity pass (chal. 3).

---

## Critique of the plan itself (short)

- **Section 3 routing is sound.** Keep validator-escalation. But run `prelabel.py`
  first and drop template/cluster_copy from the worker queue.
- **"~26% runtime gap" was overstated** — the biggest component was a
  misidentified static array. Re-run `coverage_report.py` after slot recovery.
- **Trust tiers:** template summaries are DERIVED (mechanical, but from DERIVED
  extraction); cluster copies must carry the twin note so a later fix to the
  representative propagates.
- **Step 4 pilot:** include at least one cluster representative and one monster
  in the 10, not just average functions.

## Suggested execution order

1. Patch `name_index.json`: 0x34F0 → yaw (CONFIRMED, static-multi-site);
   0x32C8 → slot-active/−1-empty (CONFIRMED).
2. Wire `render_skeleton.py` to consume `prelabel.json` (skip non-llm) and
   `vehicle_slot_globals.json` (render `VEH[slot].name` instead of raw global).
3. Implement identity pass (challenge 3 steps 1-3) — cheap, high yield.
4. Then the DeepSeek batch on the reduced 4,509 set, priority tick-path first.
5. Extractor indirection upgrade (challenge 2) as a second extraction round;
   re-diff facts, re-render only changed functions.
