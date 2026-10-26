# STOP 2 review (Claude, 2026-10-09)

Verdict: NOT ACCEPTED - INCOMPLETE. Good parser work; core STOP 2 deliverables missing.

Verified (fact): STOP 1 corrections present; `tools/test_vb6_pcode.py` passes; doc text kept in %TEMP% (not workspace); before-state folder exists; no vendor writes.

Missing / wrong:
1. File schemas absent. Assignment STOP 2 requires .spt/.lap/.dat/.svm/.cfg/.ini schemas with byte offsets, header/magic/version, record size/stride, channel descriptors, cadence, lap boundaries, units, sentinels, plus reproducible decoding examples against multiple real files, tied to reader/writer p-code (Open/Get/Put/Seek call sites and record lengths). Need decoder + tests (synthetic or excerpt fixtures, no vendor copies).
2. P-code procedures still 0/N. N is now known (195 RTMI / 207 methods). Produce per-procedure table: object, name, address, size, callees/runtime calls, classification (startup/file_io/record_decoding/lap/reference/transform/render/cursor/track/filter/derived/stats/shutdown/ui/unknown), evidence. Update FUNCTION_COVERAGE (it still says "pending STOP 2").
3. Inconsistency: 207 methods = 207 ExitProcHresult, but only 159/195 streams end in exit-like opcode. Explain the 36 (or 12 method/RTMI gap) or mark open.
4. Time vs distance axis mode (overview.ini literal "time" vs owner screenshot distance axis) not addressed.
5. Layout language: finish grammar from p-code parser of the INI reader, not only from samples.
