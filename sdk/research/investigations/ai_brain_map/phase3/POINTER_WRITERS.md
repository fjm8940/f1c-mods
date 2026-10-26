# POINTER_WRITERS — D10: store scan for 0x4408 / 0x44B4 (BATCH B item 8)

Date: 2026-10-02. Author: DeepSeek V4.1 (read-only). Script: `phase3/analysis/d10_writers.py`
whole `.text` (545 258 instructions), every store (`mov`, `fstp`, `fst`, `movss`) whose memory
displacement is 0x4408, 0x43FC (=0x4408-0xC), 0x44B4 or 0x44A8 (=0x44B4-0xC), any base register;
per hit, backward trace (<=20 insns) of the base register. Closes doc 54 section 7 item 2.

## 1. Result: 15 hits, all vehicle-based plain stores — no pointer-based (veh+0xC) writer

| address | function | instruction | field | base trace |
|---|---|---|---|---|
| 0x423ED9 | `RaceStructInit` (0x423xxx) | `mov dword [esi+0x43FC], ebx` | 0x43FC braking target | init zeroing block (ebx=0) |
| 0x423EE5 | `RaceStructInit` | `mov dword [esi+0x4408], ebx` | **0x4408** | init zeroing block |
| 0x423FE2 | `RaceStructInit` | `mov dword [esi+0x44A8], ebx` | 0x44A8 command_queue_array | init zeroing block |
| 0x42484B | `AIDriverStructInit` | `fstp dword [esi+0x44B4]` | **0x44B4** | `fld [esi+0x396C]` at 0x424838, stored directly |
| 0x42493F/0x42496A | `AIDriverStructInit` | `mov eax,[esi+0x44A8]` / `mov [esi+0x44A8], eax` | 0x44A8 | queue pointer init |
| 0x425E1C | `AIRacingLine` | `mov dword [esi+0x43FC], 0x461C3C00` | 0x43FC = 9999.0 sentinel reset | esi = vehicle |
| 0x425FA3 | `AIRacingLine` | `mov dword [esi+0x43FC], edx` | 0x43FC = limiting target | esi = vehicle |
| 0x4293FE | `AISteering` | `mov dword [esi+0x4408], edx` | **0x4408 = 0x440C** | esi = vehicle (epilogue) |
| 0x42940C | `AISteering` | `mov dword [esi+0x4408], eax` | **0x4408 = 0x4420** if `0x4548 != 0` | esi = vehicle |
| 0x42942B | `AISteering` | `mov dword [esi+0x4408], ecx` | **0x4408 = 0x4424** (forced override wins) | esi = vehicle |
| 0x424013/0x424DA3/0x424DF3/0x424E3D/0x42493F | `VehicleSubsystemCleanup`, `AICommandMessageQueueDispatcher`, `PairArrayLinearSearch`, `AIDriverStructInit` | `mov eax/edx/ecx/edi, [base+0x44A8]` | 0x44A8 readers (not stores) | - |

- **0x4408 writers (per tick): `AISteering` epilogue 0x4293F0-0x42942B** — exactly doc 54 section 6a:
  copy 0x440C, then the request 0x4420 when `override_enable 0x4548 != 0`, then the forced 0x4424 if
  non-zero (last wins). Plus the init zero in `RaceStructInit`.
- **0x44B4 writer: `AIDriverStructInit` only** — `fstp [esi+0x44B4]` storing the `[esi+0x396C]` value
  loaded at init; the constant 0.4014 observed live is that value. No per-tick or pointer-based
  writer exists in `.text`.
- The `veh+0xC`-relative displacements (0x43FC, 0x44A8) hit only the known `braking_target_speed`
  and `command_queue_array` fields; no instruction stores 0x4408/0x44B4 through a `veh+0xC` base.
- Why the item looked open: Hex-Rays renders these as `this[4354]` / `this[4397]` (scaled indices),
  so a literal `0x4408`/`0x44B4` text search in the `.c` corpus missed them.

## 2. Verdict

**Doc 54 section 7 item 2 is closed**: the writers of `published_lateral_target 0x4408` and
`steering_lock_divisor 0x44B4` are plain vehicle-relative stores, not pointer-based. The steering
law's 0x44B4 is an init-time constant from `veh+0x396C`; the only per-tick 0x4408 writer is the
AISteering epilogue (copy/request/forced chain), so pre-stage-6 0x4548/0x4420 and 0x4424 writes are
the supported override surface (unchanged from doc 54 section 6).

## Limitations

- The scan sees only stores whose displacement is literally one of the four values. A store through
  a computed address (`lea reg,[veh+0x4408]; mov [reg],v`) would still surface as a store with a
  different displacement only if the addend is folded elsewhere; the 20-instruction base trace found
  none in the hit list, and store density near the fields is low (15 hits total).
- `movss`/SSE stores do not occur for these fields in this 32-bit x87 build.
