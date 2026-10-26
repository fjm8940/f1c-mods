# Native F1C chat sender investigation (2026-10-13)

Scope: static, read-only. No game launched, no runtime or telemetry test, no source edits.
Evidence: `f1c_ida_decompiled/functions/*.c` (Hex-Rays) and `exe-disassemble` of
`F1 Challenge Online.exe`. Findings appended to `f1c_ida_decompiled/findings.jsonl`
(ids `chat-*`, `boot-0x462940-is-broadcast-2026-10-13`). Supersedes the OPEN entry
`boot-chat-class4-subtype6-sender-unproven-2026-10-13`, which is kept unchanged.

## Answer

The native chat sender is **class 4, opcode 6**. Two native senders exist:

| Sender | Role | Evidence |
|---|---|---|
| `sub_470490` (SessionMessageUICommandHandler) | Text-entry widget commit, state `this[193]==435`, text at `this[188]` | `functions/SessionMessageUICommandHandler_00470490.c`; asm 0x470490..0x470561 |
| `sub_45F030` (MouseInputHandler) | Preset quick message from linked list `this+28128` (count `this+28131`) | `functions/MouseInputHandler_0045f030.c`; asm 0x45F179, 0x45F1B4 |

Both call the same encoder and send wrapper.

## PROVEN (seen in decompile and disassembly)

1. **Encoder** `ProgressBarEventEncode` = `0x4B33B0`, `__thiscall`, `this` = packet buffer P,
   args `(a2=class, a3=opcode, a4=payload_len, a5=byte0, a6=float, a7=flag)`.
   - dword0 = `a5 | a4<<8 | a3<<18 | (dword_7C56A4 + 8*(a7+4*a2))<<24`
   - dword1 = `a6`; dword107 = -1. Callee cleans the stack (`ret 0x18`).
   - Chat call: `class=4, opcode=6, len=min(strlen,159), byte0=0, a6=(float)dbl_8AFEA0, a7=1`.
   - Sender call-site asm: `lea ecx,[esp+0x168]; call 0x4B33B0` at 0x470523..0x47052A.
2. **Packet**: header at P (8 bytes), NUL-terminated text at P+8, text length in
   dword0 bits 8-17. Wire size = len + 8. Confirmed by the receiver (`case 6u`).
3. **Send wrapper** `TextureCreationWrapper` = `0x462250`, `__thiscall`, `this` = `&netcomm_global`
   (chat passes literal `0x7CD25C`, the same convention as the boot sender and the lobby Kick path).
   - Send object = `COMVTableCallVia0x60(*this + 16312)`.
   - `target == -1` sets `obj[4]=1` (broadcast). Otherwise `obj->vtbl+12(target)`.
   - `mode 0` -> NetworkSendDispatch mode 0. `mode 2` -> mode 2 with flag 1.
   - Calls `NetworkSendDispatch(P, len+8, obj, bit27(P), mode, a5, flag)`.
   - Chat call-site asm: `push -1; push -1; push 0; push P; mov ecx,0x7CD25C; call 0x462250` at 0x470541..0x47055C.
4. **Boot sender** `0x462940` also sets `obj[4]=1`, i.e. boot is a **broadcast**. The target node
   id is in the payload (dword2). Every peer receives opcode 0x20. Only the named node shows
   "You have been booted". Others show "<name> was removed from the game".
5. **Receiver** `NetCommSessionMessageDispatcher` `0x45D890`, `case 6u`. This is native code,
   so vanilla clients run it without any mod:
   - Opcode from header bits 18-23. Length from bits 8-17. Text = packet+8.
   - Sender name: `DriverDataCopyByID(netcomm, byte0)`.
   - If `this[4081] != 1`: optional on-screen line only if `!byte_7CE254 && byte_7D1044`. The
     line is always appended to list `this+28124` and purged by threshold.
6. Chat sender gate (`sub_470490`): `SessionStateCheckWrapper(0x7CD25C) != 0` (netcomm session
   active) and `FieldGetter_79d0(this[16]) == this` (widget focus).
7. Quick-message sender (`sub_45F030`) gates: `this[225] >= 0.667`, `!byte_7CE254`.
   Its trigger codes 4098..4107 are pulled from the DIK action queue `word_7CB21C` (see
   `keyboard-action-queue-background-input`). They are not chat text.

## INFERRED (not proven)

- **Directed (single-peer) chat**: passing `target = node id` to `0x462250` should call
  `obj->vtbl+12(target)` and skip the broadcast flag. The code supports it, but no live test was run.
- **Receiver state 1 is the host**: follows from the boot finding (boot sender is host-only, states 1 or 2).
- **Mod recipe below will show the text to the booted player** only if the receiver gates pass.
  That is untested.

## OPEN (must be resolved before the mod relies on it)

a. **Sender name byte.** Both native senders put `byte0 = 0`. The receiver looks the name up from
   byte0 (`DriverDataCopyByID`). No stamping of the own node id was found in the
   `NetworkSendDispatch` -> `sub_586800` / `SingleFunctionCallWrapper_6a00` (memcpy only) path.
   Whether vanilla shows the wrong name, or the transport stamps it, is unknown.
b. **`byte_7CE254`.** Set by `sub_4A0D30` (GlobalStateSetter), which no decompiled function calls
   directly. It gates chat display and quick messages. Meaning unknown. If it means "in race",
   vanilla may not display chat in race at all. This would need a live check.
c. **`byte_7D1044`.** Gates display. Probably the server option `AllowChatInCar`, registered by
   `sub_4B1C20` in `PlayerOptionsInitializer` (line 1263). The binding to the global was not read.
d. **Lobby vs in-race availability** of the text widget, and what sets `this[193]=435` (Enter key).
   Not found. `EngineTemperatureDebugDisplay` also reads state 435, so the state is shared.
e. Sender `this` difference: `sub_470490` uses the literal `0x7CD25C`. `sub_45F030` uses `[ebx]`.
   The mod should copy the `sub_470490` form.

## Proposed call recipe for the mod (NOT implemented)

Run on the game thread only (per `f1c_ida_decompiled/AGENTS.md`). Requires netcomm state
check first. Text max 159 bytes.

1. Zeroed buffer P of 432 bytes (108 dwords). Stack-allocate in the mod stub.
2. Call `0x4B33B0` with `ecx = P`, stack args (push right to left):
   `a7=1`, `a6=(float)dbl_8AFEA0`, `a5=0` (or own node id, see open item a), `a4=len`, `a3=6`, `a2=4`.
   Callee cleans the stack.
3. `memcpy(P+8, text, len)`; `P[8+len] = 0`.
4. Broadcast: `ecx = 0x7CD25C`; push `-1, -1, 0, P`; call `0x462250`. Callee cleans the stack.
   Directed (unproven): replace the second push with the target node id.
5. Precondition: `SessionStateCheck(0x7CD25C) != 0`. Send before the boot packet so the text is
   queued first.
6. Verify in a 2-PC test: vanilla peer shows the line (open items a, b, c), and name appears.

## Ruled out / not found

- No chat string constant exists in `functions/` (grep for "Chat:" and "aChat" found nothing).
  Only option labels (`AllowChatInCar`, `Autochat01..10`) were found.
- `0x462940` is boot, not chat (it encodes class 4 opcode 32 and sends to a target).
- The `case 0x24` receiver re-broadcasts car-event text as chat (`ProgressBarEventEncode(v88,4,6,...)`,
  line 476). It is a relay, not a user sender.

## Restoration / safety

Only two new artifacts: this document and appended `findings.jsonl` entries (single `open('a')`
write). No prior text was rewritten. `findings.jsonl` contains 15 pre-existing `//` header lines,
which are not JSON; the 1276 JSON lines parse.
