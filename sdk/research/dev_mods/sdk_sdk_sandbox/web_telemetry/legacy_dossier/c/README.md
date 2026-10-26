# legacy_dossier/c — C decoder port (STOP 4 preparation record)

**Moved in STOP 4 (2026-10-09):** the decoder sources now live at the
component root — `legacy_formats.h`, `legacy_formats.c`,
`legacy_formats_test.c` — and are compiled into the DLL and test harness by
`build.bat`. This README is retained as the preparation record; the build and
parity notes below still apply, with the paths above replacing
`legacy_dossier/c/…` (the `.lap`/`.spt` memory entry points
`lf_decode_lap_mem`/`lf_decode_spt_mem` were added for the import route).

Standalone C port of the proven Python decoders
(`legacy_dossier/tools/legacy_formats.py`). During STOP 4 preparation it was
compiled only by its offline test harness; STOP 4 integration wired it into
the product (`POST /trace/import/legacy`).

## Files

| File | Purpose |
|---|---|
| `legacy_formats.h` | Public API, import bounds (64 MB file, 512 channels, 2,000,000 samples, 16M sample floats, 131,072 spt player records), the RPM factor `9.549296585513721` and `lf_rad_s_to_rpm()`. |
| `legacy_formats.c` | `.lap` decoder (checksum, byte-5 B1/B2 blocks, field block, name table, float32 samples, byte-7 trailer) and `.spt` decoder (header, descriptors, session words, XOR .VEH validation, typed record stream, player payload rows). Fail-closed on every malformed/truncated/oversized input. |

## Build and run

```powershell
$dir = "$env:TEMP\f1c-legacy-dossier-c"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
& "C:\msys64\mingw32\bin\gcc.exe" -O1 -static -static-libgcc -std=c11 `
    -Wall -Wextra -Werror -Wno-unused-function -o "$dir\legacy_formats_test.exe" `
    legacy_formats.c legacy_formats_test.c -lm
& "$dir\legacy_formats_test.exe"
```

Expected: `70 checks, 0 failures`. Real files are the vendor tree at
`%USERPROFILE%\Documents\F1 Challenge V10\Telemetry` (override with
`F1C_LEGACY_TREE`), read-only in place, never copied:

| Fixture | SHA256 |
|---|---|
| `telemetry.spt` (the crash-truncated live sample; torn-tail case) | `AA04E86E3C64311AC0D4AF93003E6AB82320162AAA5AB0B9F7FF6FE823B308E2` |
| `Reference/reference_2002_hockenheim.lap` | `6AA795B5FC05E3ADB8BAB5F4FE8350824AA095E4F3FA0A8434CC48E2CADE27AF` |

Synthetic fixtures (generated, never committed): `synthetic_good.lap`,
`synthetic_cut.lap`, `no_checksum.lap`, `short_block.lap`,
`zero_channels.lap`, `synthetic_good.spt`, `zeros.spt`, `bad_type.spt`,
`no_players.spt`.

## Parity notes

The C harness asserts the same facts as `tools/test_legacy_formats.py`
(56 checks, real files): spt 57 channels, descriptor end 0x4E0, driver
identity, embedded `[GENERAL]` block, 229/25/5 record sizes, 0.1 s dominant
cadence, 2,200–2,500 B/s growth; lap checksum 246076184220, B1/B2 pairs,
LAPTIME cross-check, 86 channels, 1,385 × 344 B samples, byte-7 trailer. The
C port adds: player payload rows (57 floats per 229 B record, row 0 first
float equals the elapsed value), a stricter per-name cap (64 chars) and
bounded pair parsing documented as a bounded equivalent of the Python regex.

Units: `.lap` sample floats are raw file values under the viewer's channel
names; the `.lap` `engine rpm` channel and the `.spt` `Engine RPM` payload are
already in RPM (the game writer applied the factor). Live `/telemetry` `c.r`
is rad/s — convert with `lf_rad_s_to_rpm()` before any RPM label; never apply
the factor twice to file data.

Endianness: the reader assumes the little-endian x86 target (same as the
product). Everything else (bounds, refusals, torn-tail behavior) mirrors the
Python decoder; a torn `.spt` decodes up to its last complete record and
reports the partial tail, never guessing.
