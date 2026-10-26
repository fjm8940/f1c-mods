# VB6 p-code structure parse: f1_telemetry.exe

Source: `%USERPROFILE%\Documents\F1 Challenge V10\Telemetry\f1_telemetry.exe`
SHA256: `0AA34C51C30C3113BF6B6C2F1B59A8B3346AD1322F6AB519E60757BAF8573BEB`
Compilation mode: **pcode** (aNativeCode = 0x0)

## VBHeader

- VA 0x408f04; runtime build 0x1ff0; LCID 0x409
- Forms: 7; thunks: 233; external count field: 0

## ObjectTable and objects

- ObjectTable VA 0x40ebc4; 13 objects; project name `F1_Challenge_Telemetry`

| # | Object | Type | Methods | ObjectInfo | Method-name table |
|---|---|---|---|---|---|
| 0 | General_utility_routines | 0x18001 | 50 | 0x40a6e8 | - |
| 1 | Data_management | 0x18001 | 27 | 0x40a1d8 | - |
| 2 | home | 0x18083 | 60 | 0x40d008 | 0x40ee88 |
| 3 | General_Declarations | 0x18001 | 1 | 0x408e60 | - |
| 4 | General_WinAPI_Declarations | 0x18001 | 14 | 0x408fc0 | - |
| 5 | FileReading | 0x18001 | 15 | 0x409da4 | - |
| 6 | frmAbout | 0x18083 | 2 | 0x4099ec | 0x40ef78 |
| 7 | LapBrowser | 0x18083 | 18 | 0x40b72c | 0x40ef80 |
| 8 | LapOptions | 0x18083 | 6 | 0x40ac28 | 0x40efc8 |
| 9 | LapInfo | 0x18083 | 7 | 0x40c0f8 | 0x40efe0 |
| 10 | General_Translation | 0x18001 | 1 | 0x4090b0 | - |
| 11 | frmPasswordBox | 0x18083 | 3 | 0x40b17c | 0x40effc |
| 12 | frmMsgBox | 0x18083 | 3 | 0x40969c | 0x40f008 |

## Method dispatch and RTMI records

- RTMI entries: 195; distinct RTMI addresses: 195
- RTMI lpObjectInfo back-pointer matches a known ObjectInfo for 195/195 entries
- Streams whose last 4 bytes contain an exit-like opcode: 159/195

## External (Declare) table

- Declarations parsed: 9
- DLLs: kernel32, shell32.dll, user32
- Functions: GetWindowTextA, GetWindowTextLengthA, GetParent, GetWindow, GetCurrentThread, SetThreadPriority, ShellExecuteA, GetSystemMetrics, EnableWindow

## P-code streams

Each RTMI record is 0x30 bytes; the p-code stream follows it and ends with an exit-like opcode before the next RTMI (empirically verified on this binary).

| RTMI | Stream VA | Length | Owners | Decoded | First unknown |
|---|---|---|---|---|---|
| 0x42d568 | 0x42d598 | 8 | LapBrowser #13 | 2 | 0x0 @ +0x6 |
| 0x42d5a0 | 0x42d5d0 | 8 | home #9 | 0 | 0x10 @ +0x0 |
| 0x42d5d8 | 0x42d608 | 8 | home #46 | 2 | 0x0 @ +0x6 |
| 0x42d610 | 0x42d640 | 8 | LapInfo #2 | 0 | 0x10 @ +0x0 |
| 0x42d648 | 0x42d678 | 12 | LapBrowser #14 | 0 | 0x10 @ +0x0 |
| 0x42d684 | 0x42d6b4 | 12 | LapBrowser #4 | 0 | 0x28 @ +0x0 |
| 0x42d6c0 | 0x42d6f0 | 336 | General_utility_routines #25 | 0 | 0x0 @ +0x0 |
| 0x42d840 | 0x42d870 | 16 | home #50 | 0 | 0xf4 @ +0x0 |
| 0x42d880 | 0x42d8b0 | 16 | home #26 | 0 | 0xf4 @ +0x0 |
| 0x42d8c0 | 0x42d8f0 | 144 | home #25 | 0 | 0x0 @ +0x0 |
| 0x42d980 | 0x42d9b0 | 20 | home #51 | 1 | 0x28 @ +0x5 |
| 0x42d9c4 | 0x42d9f4 | 96 | home #59 | 0 | 0x0 @ +0x0 |
| 0x42da54 | 0x42da84 | 24 | LapBrowser #9 | 0 | 0x5e @ +0x0 |
| 0x42da9c | 0x42dacc | 24 | General_WinAPI_Declarations #9 | 0 | 0x5e @ +0x0 |
| 0x42dae4 | 0x42db14 | 20 | General_WinAPI_Declarations #10 | 0 | 0xfd @ +0x0 |
| 0x42db28 | 0x42db58 | 32 | Data_management #2 | 0 | 0x0 @ +0x0 |
| 0x42db78 | 0x42dba8 | 24 | home #3 | 0 | 0x5e @ +0x0 |
| 0x42dbc0 | 0x42dbf0 | 28 | General_WinAPI_Declarations #11 | 0 | 0x0 @ +0x0 |
| 0x42dc0c | 0x42dc3c | 28 | home #11 | 0 | 0x5c @ +0x0 |
| 0x42dc58 | 0x42dc88 | 28 | home #36 | 0 | 0x0 @ +0x0 |
| 0x42dca4 | 0x42dcd4 | 28 | LapBrowser #7 | 0 | 0x5c @ +0x0 |
| 0x42dcf0 | 0x42dd20 | 28 | home #32 | 0 | 0x78 @ +0x0 |
| 0x42dd3c | 0x42dd6c | 36 | frmAbout #0 | 0 | 0x78 @ +0x0 |
| 0x42dd90 | 0x42ddc0 | 32 | LapBrowser #12 | 1 | 0x80 @ +0x5 |
| 0x42dde0 | 0x42de10 | 32 | Data_management #12 | 0 | 0xf4 @ +0x0 |
| 0x42de30 | 0x42de60 | 32 | home #17 | 0 | 0xfd @ +0x0 |
| 0x42de80 | 0x42deb0 | 32 | FileReading #5 | 0 | 0xfd @ +0x0 |
| 0x42ded0 | 0x42df00 | 32 | General_utility_routines #37 | 0 | 0xfd @ +0x0 |
| 0x42df20 | 0x42df50 | 32 | General_utility_routines #36 | 0 | 0xfd @ +0x0 |
| 0x42df70 | 0x42dfa0 | 32 | FileReading #4 | 0 | 0x5 @ +0x0 |
| 0x42dfc0 | 0x42dff0 | 36 | LapInfo #0 | 0 | 0x78 @ +0x0 |
| 0x42e014 | 0x42e044 | 36 | frmMsgBox #1 | 0 | 0x68 @ +0x0 |
| 0x42e068 | 0x42e098 | 36 | frmMsgBox #0 | 0 | 0x68 @ +0x0 |
| 0x42e0bc | 0x42e0ec | 36 | frmPasswordBox #0 | 0 | 0x68 @ +0x0 |
| 0x42e110 | 0x42e140 | 36 | home #16 | 0 | 0x78 @ +0x0 |
| 0x42e164 | 0x42e194 | 36 | General_utility_routines #10 | 0 | 0x0 @ +0x0 |
| 0x42e1b8 | 0x42e1e8 | 36 | home #5 | 0 | 0x78 @ +0x0 |
| 0x42e20c | 0x42e23c | 44 | General_utility_routines #9 | 0 | 0x0 @ +0x0 |
| 0x42e268 | 0x42e298 | 36 | FileReading #3 | 0 | 0xf4 @ +0x0 |
| 0x42e2bc | 0x42e2ec | 44 | home #7 | 0 | 0x78 @ +0x0 |
| 0x42e318 | 0x42e348 | 48 | LapOptions #2 | 1 | 0x0 @ +0x3 |
| 0x42e378 | 0x42e3a8 | 44 | home #41 | 1 | 0x8 @ +0x3 |
| 0x42e3d4 | 0x42e404 | 48 | LapBrowser #0 | 0 | 0x78 @ +0x0 |
| 0x42e434 | 0x42e464 | 52 | General_utility_routines #6 | 0 | 0x0 @ +0x0 |
| 0x42e498 | 0x42e4c8 | 52 | General_utility_routines #32 | 0 | 0x0 @ +0x0 |
| 0x42e4fc | 0x42e52c | 52 | Data_management #21 | 0 | 0x0 @ +0x0 |
| 0x42e560 | 0x42e590 | 52 | General_utility_routines #33 | 0 | 0x0 @ +0x0 |
| 0x42e5c4 | 0x42e5f4 | 52 | General_utility_routines #5 | 0 | 0x0 @ +0x0 |
| 0x42e628 | 0x42e658 | 60 | Data_management #20 | 0 | 0x0 @ +0x0 |
| 0x42e694 | 0x42e6c4 | 60 | General_utility_routines #47 | 0 | 0x0 @ +0x0 |
| 0x42e700 | 0x42e730 | 60 | LapOptions #1 | 0 | 0xf4 @ +0x0 |
| 0x42e76c | 0x42e79c | 56 | home #4 | 0 | 0xfd @ +0x0 |
| 0x42e7d4 | 0x42e804 | 64 | FileReading #6 | 0 | 0x0 @ +0x0 |
| 0x42e844 | 0x42e874 | 80 | General_utility_routines #16 | 0 | 0x0 @ +0x0 |
| 0x42e8c4 | 0x42e8f4 | 80 | General_utility_routines #35 | 0 | 0x0 @ +0x0 |
| 0x42e944 | 0x42e974 | 64 | home #29 | 0 | 0x78 @ +0x0 |
| 0x42e9b4 | 0x42e9e4 | 220 | General_utility_routines #42 | 0 | 0x0 @ +0x0 |
| 0x42eac0 | 0x42eaf0 | 76 | Data_management #1 | 0 | 0x0 @ +0x0 |
| 0x42eb3c | 0x42eb6c | 96 | General_WinAPI_Declarations #12 | 0 | 0x0 @ +0x0 |
| 0x42ebcc | 0x42ebfc | 88 | home #8 | 0 | 0x60 @ +0x0 |

(First 60 of 195 streams; full data in the JSON.)

## Opcode census (evidence-backed subset only)

Linear decode (stops at the first unknown opcode):
- ImpAdCallFPR4: 4
- ILdRf: 3
- ExitProcHresult: 2
- LitStr: 1

Greedy non-overlapping scan (no instruction-boundary claim):
- VarRef: 7474
- FLdRfVar: 5706
- ILdRf: 1570
- LitStr: 989
- LitI4: 669
- ImpAdCallFPR4: 532
- ExitProcHresult: 207

## Unresolved / open

- RTMI `field_*` values are recorded raw; only `lp_object_info` is proven.
- The opcode decoder is a proven subset; unknown opcodes stop the linear walk (their offsets are recorded per stream).
- GUI table and PrivateObjectDescriptor internals are not fully parsed.
- Semantic mapping of each method to its stream is proven only through the dispatch-table -> RTMI -> stream chain, not by executing the p-code.
