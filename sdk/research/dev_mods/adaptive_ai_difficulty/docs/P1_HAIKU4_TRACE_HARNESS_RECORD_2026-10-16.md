# P1 Haiku 4 record: trace module and replay harness — 2026-10-16

. Development only. Nothing was run against the game. No live validation.
Spec: `P1_RATING_PORT_SPEC_2026-10-16.md` sections 4 (trace) and 5 (harness). Headers `dll/aad_trace.h`
and `dll/aad_rating.h`/`aad_store.h`/`aad_json.h` were NOT changed.

## Files (new)
- `dll/aad_trace.c` — trace writer and reader (section 4).
- `dll/test/aad_replay_harness.c` — console harness, modes replay / store / numfmt / round3 / mt (section 5).
- `dll/test/build_harness.bat` — builds `%TEMP%\aad_replay_harness\aad_replay_harness.exe`; CRLF; writes nothing into the workspace.
- `dll/test/` directory created (did not exist).

## Verification (fact)
- `aad_trace.c` and `test/aad_replay_harness.c` compile warning-free with `-O2 -Wall -Wextra -Werror -std=gnu11
  -msse2 -mfpmath=sse -ffp-contract=off -I.` using `-c` into `%TEMP%`.
- Trace round trip (throwaway driver in `%TEMP%\aad_trace_roundtrip`, links `aad_trace.c` only): 2 samples, 2 cars
  incl. NaN, -0.0, subnormal, 1e308, non-ASCII car name, pf present/absent, slider N and 93, empty utc, track set
  and reset to "". All bits equal. 7 malformed inputs rejected (ncars 65, unknown line, short S, bad track hex,
  blank line, bad header, stray C line).
- NOT done: `build_harness.bat` and `numfmt` run. `aad_json.c` and `aad_rating.c` do not exist yet
  (only `aad_store.c` exists). The bat must be run once all four .c files exist.

## Decisions and assumptions (inference unless marked)
- Reader `aad_trace_open` returns 1 ok, -1 bad header (spec only defines `aad_trace_next` codes).
- Reader rejects blank lines and unknown line types; lines over 4096 chars are format errors.
- Writer writes an empty utc as `-`; utc must not contain spaces (not stated in spec).
- Writer clamps ncars to 0..64 so the S count matches the C lines written.
- `T` line value is stored in the reader; each sample gets it (`s->track`). Harness creates the engine with the
  first sample's track (= first T line value).
- Harness error exit code 2 for usage, format, and I/O errors.

## Open issues for Claude review
1. `aad_store_lookup` takes `int slider` with no has-slider flag, so a `lookup` op with `"slider": null` cannot be
   represented. The harness currently fails that op with a message. Needs a header decision (has_slider flag, as
   in `aad_store_overall`) or a confirmed sentinel. Python `lookup` accepts None.
2. `aad_store_record` takes `track` as NULL-able; the harness passes NULL when the op has no track or a non-string.
3. `aad_summary_from_json` is given an empty object when the op has no summary object (assumed safe for missing keys).
4. `numfmt`/`round3` treat blank lines as errors (spec does not say).

## Follow-up
- Haiku 1/2/3 files (`aad_json.c`, `aad_rating.c`) must land, then run `dll\test\build_harness.bat`, then
  `harness numfmt` on a Python `repr` list.
- `tools/aad_trace.py` (Haiku 5) must write the same format (utc `-` rule, zero-padded hex, `%016llx` bits).
- Index: this record is not yet linked from `docs/HANDOFF_2026-10-16.md`. Owner or Claude to add the link
  (existing handoff not edited here to keep its before-state).
