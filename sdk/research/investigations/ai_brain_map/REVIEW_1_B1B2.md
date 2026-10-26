# Claude review 1 — B1+B2 (2026-10-02)

Verdict: **REWORK B1/B2 before B3.** Test passes (re-run: OK), structure good, but three defects would poison B3/B5.

Fixes required (DeepSeek):

1. **Add a `base` column** (the identifier before `+`: `this`, `a2`, `v10`, ...). Today `v10 + 148` (waypoint
   position) lands in the same table as vehicle offsets, so 0x94/0x9C look like vehicle fields. B5 glossary must use
   only `base == this` (or the function's documented vehicle argument) as vehicle fields; other bases go to a separate
   "pointer fields" table with the base variable named.
2. **Report what the regex misses.** Add a second output `unmatched_<Name>.txt`: every source line that contains
   `this +` or `[` index on a float/int pointer (`v3[37]`) or a double deref `*(_DWORD *)v2 + N` and produced no row.
   Known misses: indexed arrays `(this + 4 * v25 + 17236)` (sub_426100 1, sub_426430 2, AIRacingLine 2), float-array
   indexing `v3[37]` (= +148 bytes on a float*), double deref `*(float *)(*(_DWORD *)v2 + 32)`. Either parse these
   (add column `form` = direct / indexed / array / deref2 with byte offset = index * element size) or list them; do
   not silently drop.
3. **One function naming scheme.** `func` column mixes `AIMainUpdate` and `sub_425920`. Use the section-4 name
   (file stem without address) everywhere. Also drop the self-call row from the signature line (AIMainUpdate line 2
   `CALL sub_424E70`).

Extend the test: assert a `base` value for a `this` row and a non-`this` row, one indexed row (sub_426100 line with
`4 *`), and that no row has `func` starting `sub_4` when the file stem has a name.

Then regenerate all CSVs and continue to B3 group 1. No other changes.
