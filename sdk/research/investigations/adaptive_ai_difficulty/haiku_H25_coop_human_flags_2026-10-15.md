# H25 - CO-OP human car flags and AIDriverStructInit coverage (static)

Date: 2026-10-15. Status: STATIC READ ONLY. No game process touched, no mod source edited, no findings.jsonl appended (read-only scope; proposed rows in section 6). Tiers: FACT = code/decompile/finding cited; DERIVED = reasoned from FACTs; OPEN = needs live or wider code check; CONFLICT = sources disagree.

## 0. Answers

1. Remote human marking: veh+0x32C8 == 2 is the remote human copy in CO-OP (FACT, coop_mode.cpp:4858). Local human = 0, AI = 1, inactive = -1. Role 2 is not unique to humans (section 5).
2. AIDriverStructInit runs for remote humans and for the host at slot load (FACT, NetCommPlayerSlotSync line 104 -> SceneRenderUpdate -> AIDriverStructInit). Its strength call (lines 128-131) is not gated by role, so it runs for every car it is invoked on.
3. Hook coverage: human cars are covered on host and client, by role (!= 1) and by player pointer (DERIVED). Host-owned AI and client stream bots are role 1 and are covered only by the session guard.
4. Clients: the hook should not run there. The guard refuses state 3 (DERIVED). The worker tick leaves a window of up to about 100 ms (section 4).

## 1. Marking fields

| Field | Meaning in sources | Writer / reader evidence | Tier |
|---|---|---|---|
| veh+0x32C8 (13000) | Role: 0 local human, 1 AI, 2 remote human copy OR physics-off hold, -1 inactive | SessionStateTransition sub_4EF5A0 stores its 2nd arg into this[3247] (functions/SessionStateTransition_004ef5a0.c:9); its `this` is veh+0xC, so the field is veh+0x32C8. Slot load: local = 0 (NetCommPlayerSlotSync.c:85), remote = 2 (:90). Unload = -1 (:252). | FACT |
| AIMainUpdate gate | Role 0-1 run path tracking, >1 returns | functions/AIMainUpdate_00424e70.c:1 comment; findings native-sc-aimainupdate-self-gates-no-count-change (FACT) | FACT |
| coop car_is_human | role 0 or role 2 = human | coop_mode.cpp:3582-3585 | FACT (code) |
| veh+0x45F4 (17908) | SDK AIControlFlag int32 (f1c_sdk.h:243-244); callgraph label ActiveFlag; coop VEH_GO_FLAG (coop_mode.cpp:4462, qualifying go flag) | AIDriverStructInit writes byte 0 UNCONDITIONALLY at functions/AIDriverStructInit_00424790.c:80 (outside the LOBYTE(a2) block at :82). Finding f1c-aicontrol-zero-retires-a-moving-car. | FACT (write); CONFLICT (meaning) |
| veh+0xC4 (196) | SDK name isHumanInControl___isInDrsZone, read as float (f1c_sdk.h:31-32) | Only decompile use: sub_4A0250 loop reads it as a BYTE to gate StateHandlerDispatch (functions/sub_4A0250_004a0250.c, loop near line 50). Coop does not read it. | CONFLICT / OPEN - not a verified human marker |
| veh+0x4894 | Remote stream id for bots | coop_mode.cpp:5423, 5501 | DERIVED |
| GameModuleManager +0x3FC4 (via *0x7CD25C) | Network state: 0 offline, 1/2 host, 3 client, 4 transient | Finding ai-brain-mp-guard-session-state-3fc4-2026-10-03 (DERIVED; only offline value 0 measured live). Coop helper network_state() coop_mode.cpp:4128-4131 returns 0 when the manager pointer is null (fail-OPEN). | DERIVED / OPEN (host and client live reads pending) |

## 2. Where AIDriverStructInit is reached

- Callgraph (callgraph.json, entry 00424790): sole direct caller SceneRenderUpdate 0x4C6D90 (FACT). SceneRenderUpdate has 7 callers (FACT): AICommandMessageQueueDispatcher, PitStopDecisionTireCompound, DynMan_Restart, NetCommPlayerSlotSync, sub_4A00A0, sub_4A0250, ProcessDriverEntries.
- Strength call: AIDriverStructInit lines 128-131 run `AIDifficultyInterpolation()`, multiply by flt_5EA3E8 unless dword_7C56A4 is 3/4/5, then `return ScaleAudioPitch(v19)`. Role is not checked here (FACT).
- Remote humans and host at slot load: NetCommPlayerSlotSync line 82 compares the slot to IDLookupInArray; line 85 (local) or 90 (other) sets role; then ++dword_5C329C, then line 104 `SceneRenderUpdate((int*)v6, 1)`. So each slot loaded through this path reaches the strength call once with its role already set (FACT for the path; DERIVED that it runs for the host's own car too).
- Per frame (DERIVED): AICommandMessageQueueDispatcher case 15 calls SceneRenderUpdate(0) (AICommandMessageQueueDispatcher_00424cd0.c:34), reached from sub_426430 and others. Which cars hit that state is not traced.
- Player role change: sub_4DA840 line 47 sets the PLAYER pointer to role 1 when the player's FinishState (veh+0x4064) is nonzero under the stated conditions (FACT). RestartGate line 22 sets the player back to 0 (FACT). So role 1 can appear on the player car, and the pointer check is needed.
- AI slot load path (host bots and client stream bots): not traced (OPEN).

## 3. Hook check (development/mods/adaptive_ai_difficulty/dll/adaptive_ai_difficulty.c)

- ada_slot_of, lines 221-238: range check (221-226); `if (k == 0) return REF_SLOT0` (228); count check; ptr == dword_7C54F0[k] (231-232); `if (player == veh) return REF_PLAYER` (234); `if (mark != 1u) return REF_NOT_AI` (236). Every refusal returns -1 and the stub passes a2 through (FACT).
- Stub guard, line 255: `if (!g_session_ok) { refuse; return a2; }`. The factor is applied only after this check (FACT).
- g_session_ok defaults to 0 (line 105, fail-closed). read_session_state (203-210) returns -1 when unreadable, so the guard is fail-closed (FACT). Updated by ada_session_tick (533-540) from the worker loop (642-652) at a 100 ms wait (FACT).
- Spec match: HOOK_SPEC section 8.3 says veh+0x32C8 == 0 passes through; the code is stricter (any role != 1) (FACT). No conflict in effect.

Coverage table (DERIVED from sections 1-3):

| Car | Host | Client | Role | Hook result | What stops scaling |
|---|---|---|---|---|---|
| Local human | yes | yes | 0 | pass | role != 1, pointer == player |
| Remote human copy | yes | yes | 2 | pass | role != 1 |
| Player after FinishState set | yes | yes | 1 | pass | pointer == player (line 234) |
| Host-owned AI | yes | - | 1 | scaled only if guard == 0 | session guard |
| Client stream bot (host AI on client) | - | yes | 1 | scaled only if guard == 0 | session guard |
| Physics-off held AI | yes | yes | 2 | pass (not scaled) | role != 1 |
| Any car in slot 0 | - | - | any | pass | slot-0 rule (conservative; slot 0 need not be the player in CO-OP) |

Result: human coverage is complete on both host and client by role plus pointer. It does not depend on the session guard. AI coverage in CO-OP depends only on the guard.

## 4. Should the hook run on clients?

- No (DERIVED). Client stream bots are role 1 and are driven by the host's stream (coop_mode.cpp:5423-5442). A client-side scale changes only the client's simulation and diverges from the host. Spec R-3 already refuses in MP and CO-OP.
- Keep the refusal for states 1, 2, 3 and 4, fail-closed. Do not copy coop's network_state() helper (fail-open to 0) into the hook.
- Window risk (DERIVED): the guard is refreshed by the worker every 100 ms. The first stub call after a session starts can still see g_session_ok == 1. Mitigation: read 0x7CD25C+0x3FC4 inline in the stub (two reads) or check it in the broker task before each apply.
- Host: the same guard also blocks the host (states 1/2). Scaling host AI in CO-OP is an owner decision (OPEN).

## 5. Conflicts and open items

- CONFLICT 0xC4: SDK float name versus decompile byte use (sub_4A0250). Do not use as a human marker until verified live.
- CONFLICT 0x45F4: AIControl (SDK), ActiveFlag (callgraph), go flag (coop). AIDriverStructInit clears it on every call (FACT). Writing 0 to a moving car retires it (finding f1c-aicontrol-zero-retires-a-moving-car).
- CONFLICT role 2: remote human copy (coop_mode.cpp:4858) versus physics-off hold on AI cars (findings physics-off-hold-32c8-2 and ai-soft-tame-armed-32c8-2, both FACT). Coop car_is_human (3585) would count a held AI as human. The hook is safe (pass-through), but coop logic is not.
- OPEN: other role writers. The functions/ corpus found only two SessionStateTransition sites with roles 1-2 (sub_4DA840, NetComm) and no direct 13000 writes. Corpus coverage is 14 SessionStateTransition files, so this is not exhaustive.
- OPEN: AI slot load path, client stream bot creation, and live host/client reads of 0x3FC4.
- Doc gap: AIDriverStructInit's clear of 0x45F4 (FACT, line 80) is not listed in HOOK_SPEC section 1. Add it to ADDRESS_INVENTORY when the hook is built.

## 6. Proposed findings (NOT appended; read-only scope)

- FACT: SessionStateTransition writes its role argument to veh+0x32C8 (sub_4EF5A0; this = veh+0xC).
- FACT: AIDriverStructInit line 80 writes veh+0x45F4 = 0 unconditionally.
- FACT: NetCommPlayerSlotSync sets role 0 for the local slot and role 2 for other loaded slots before SceneRenderUpdate (lines 85, 90, 104).
- DERIVED: the adaptive hook covers all human cars on host and client by role and pointer; AI in CO-OP is covered only by the session guard.
- CONFLICT: 0xC4 and 0x45F4 naming, and role 2 meaning (section 5).

## 7. Documentation gate

- Durable record: this file. Not indexed in an INDEX (none found for this folder beyond the H-series reports; add a line to the adaptive_ai_difficulty README when the owner approves a README edit).
- Changed state: this report only. Nothing built, deployed, promoted, or appended to findings.jsonl.
- Pending validation: live reads of 0x3FC4 on host and client; live 0xC4 read; role-2 held versus remote-human discrimination; AI slot-load path.
- Restoration: delete this file to revert (no other file changed).
