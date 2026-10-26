# PE inspection: f1_telemetry.exe

- Path: `%USERPROFILE%\Documents\F1 Challenge V10\Telemetry\f1_telemetry.exe`
- SHA256: `0AA34C51C30C3113BF6B6C2F1B59A8B3346AD1322F6AB519E60757BAF8573BEB`
- Size: 331776 bytes
- Machine: IMAGE_FILE_MACHINE_I386 (0x14c)
- Linker: 6.0; subsystem: IMAGE_SUBSYSTEM_WINDOWS_GUI
- Build timestamp: 2003-04-03 (raw 1049400226)
- Entry point: 0x4011fc; image base: 0x400000

## Sections

| Name | VA | Virtual | Raw | Raw size | Entropy | Flags |
|---|---|---|---|---|---|---|
| .text | 0x1000 | 317928 | 0x1000 | 319488 | 4.967 | code, executable, readable |
| .data | 0x4f000 | 16408 | 0x0 | 0 | 0.0 | initialized-data, readable, writable |
| .rsrc | 0x54000 | 4360 | 0x4f000 | 8192 | 3.064 | initialized-data, readable |

## Imports

### MSVBVM60.DLL (50 functions)

- `ordinal 690` @ 0x401000
- `ordinal 588` @ 0x401004
- `MethCallEngine` @ 0x401008
- `ordinal 516` @ 0x40100c
- `ordinal 518` @ 0x401010
- `ordinal 519` @ 0x401014
- `ordinal 660` @ 0x401018
- `ordinal 593` @ 0x40101c
- `ordinal 594` @ 0x401020
- `ordinal 595` @ 0x401024
- `ordinal 596` @ 0x401028
- `ordinal 597` @ 0x40102c
- `ordinal 598` @ 0x401030
- `ordinal 520` @ 0x401034
- `ordinal 631` @ 0x401038
- `ordinal 525` @ 0x40103c
- `ordinal 632` @ 0x401040
- `ordinal 526` @ 0x401044
- `EVENT_SINK_AddRef` @ 0x401048
- `ordinal 529` @ 0x40104c
- `DllFunctionCall` @ 0x401050
- `ordinal 670` @ 0x401054
- `EVENT_SINK_Release` @ 0x401058
- `ordinal 600` @ 0x40105c
- `EVENT_SINK_QueryInterface` @ 0x401060
- `__vbaExceptHandler` @ 0x401064
- `ordinal 530` @ 0x401068
- `ordinal 533` @ 0x40106c
- `ProcCallEngine` @ 0x401070
- `ordinal 535` @ 0x401074
- `ordinal 536` @ 0x401078
- `ordinal 537` @ 0x40107c
- `ordinal 645` @ 0x401080
- `ordinal 648` @ 0x401084
- `ordinal 571` @ 0x401088
- `ordinal 576` @ 0x40108c
- `ordinal 578` @ 0x401090
- `ordinal 100` @ 0x401094
- `ordinal 689` @ 0x401098
- `ordinal 610` @ 0x40109c
- `ordinal 612` @ 0x4010a0
- `ordinal 614` @ 0x4010a4
- `ordinal 616` @ 0x4010a8
- `ordinal 617` @ 0x4010ac
- `ordinal 618` @ 0x4010b0
- `ordinal 540` @ 0x4010b4
- `ordinal 541` @ 0x4010b8
- `ordinal 650` @ 0x4010bc
- `ordinal 546` @ 0x4010c0
- `ordinal 581` @ 0x4010c4

## Exports

- none

## Resources

Type counts: {"icon": 1, "group-icon": 1, "version": 1}

## Version info

- Comments: F1 Challenge 99-02 Telemetry
- CompanyName: Electronic Arts
- LegalCopyright: (c) 2003 Electronic Arts Ltd - All rights reserved
- ProductName: F1 Challenge 99-02 Telemetry
- FileVersion: 2003.14.0373
- ProductVersion: 2003.14.0373
- InternalName: f1_telemetry
- OriginalFilename: f1_telemetry.exe

## VB6 boundary markers

- VB5! at file offset 0x8f04
- MSVBVM60 at file offset 0x238
- MSVBVM60 at file offset 0x4e948
- VB6 at file offset 0x10032
