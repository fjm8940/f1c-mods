# Legacy viewer function coverage

Native layer and p-code layer. Native survey: `SURVEY_f1_telemetry_2026-10-09.json` (IDB sha256 `3E66F6DC306FFC6B0086D784FA5F81169A4D7D15C333544957D153A0387E7985`).

## Native layer

- Native layer: 134/134 classified (50 runtime thunks, 78 p-code dispatch stubs, 6 evidence-backed glue items; 0 unclassified native functions). P-code layer: 195/195 RTMI methods classified with per-method evidence; 12 method slots have no RTMI (9 null, 3 inline data) and are listed individually.

## P-code layer (classified)

- Methods: 207; with RTMI: 195; without RTMI: 12
- Per-method index: `METHOD_INDEX_f1_telemetry_2026-10-09.json` (object, name, address, size, terminator, string references, runtime references, category, evidence).

| Category | Methods |
|---|---|
| metadata | 77 |
| axis_distance | 47 |
| lap_check | 44 |
| layout_io | 39 |
| upload | 35 |
| unknown | 33 |
| lap_management | 26 |
| file_io_spt_read | 24 |
| filter | 16 |
| ui_event_handler | 15 |
| render | 13 |
| shutdown | 9 |
| render_transform | 2 |

### Methods without an RTMI

| Object | Method | Target | Kind |
|---|---|---|---|
| General_utility_routines | #0  | 0xf6fcfe7c | other |
| General_utility_routines | #1  | 0x6404fd64 | other |
| General_Declarations | #0  | 0xff7404ff | other |
| General_WinAPI_Declarations | #0  | 0x0 | zero |
| General_WinAPI_Declarations | #1  | 0x0 | zero |
| General_WinAPI_Declarations | #2  | 0x0 | zero |
| General_WinAPI_Declarations | #3  | 0x0 | zero |
| General_WinAPI_Declarations | #4  | 0x0 | zero |
| General_WinAPI_Declarations | #5  | 0x0 | zero |
| General_WinAPI_Declarations | #6  | 0x0 | zero |
| General_WinAPI_Declarations | #7  | 0x0 | zero |
| General_WinAPI_Declarations | #8  | 0x0 | zero |

### Evidence limits

- String references come from a constant-pool operand scan; they prove the method uses the string, not the exact opcode.
- Runtime references (thunk/trampoline pool entries) are candidates under the same caveat.
- Stream sizes are terminator-trimmed; the two streams with >3 bytes of filler (148/255) are bounded by the next RTMI and flagged in the index.
