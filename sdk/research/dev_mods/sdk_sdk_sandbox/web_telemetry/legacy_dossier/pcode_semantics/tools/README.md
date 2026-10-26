# pcode_semantics tools

## Maintained tools (decoder, features, tests)

| File | Purpose |
|---|---|
| `opcode_table.py` | Opcode-table access (fixtures) + empirical overrides; lead-table size rule |
| `pcode_decode.py` | Stream extractor, decoder, invariant checker, listing generator |
| `pcode_features.py` | Clean-room reimplementation of the recovered viewer math |
| `test_pcode_decode.py` | 14 tests: table, 195/195 streams, invariants, call resolution |
| `test_pcode_features.py` | 16 tests: real-lap formula checks + behaviour units |
| `build_opcode_table.py` | Rebuilds `fixtures/opcode_table_v1.json` from the public page |
| `export_dossier_listings.py` | Regenerates `listings/` (full + excerpts) |

Run the tests with `python test_pcode_decode.py` and `python test_pcode_features.py`
(requires the vendor EXE read-only and the reference lap in place).

## Scratch probes (exploration history, NOT maintained, NOT part of the pipeline)

These were used to establish the ProcDscInfo layout, the LitStr addressing mode and the
formula candidates, and to validate the final decoder. They may be stale relative to the
final tools; the delivered evidence is the tooling above plus `listings/`. Kept under the
workspace retention policy (no deletion); quarantine is not used, per the reviewer's
allowed option "mark them as scratch in a tools/README".

| File | What it was for |
|---|---|
| `recon_methods.py` | List METHOD_INDEX methods by category/object (planning) |
| `probe_rtti_layout.py` | First look at the 0x30-byte record fields (ProcSize discovery) |
| `probe_extent.py` | Score candidate code extents (pool refs, branch landings, exit tail) |
| `probe_stubs.py` | Decode pool-pointed call stubs (IAT/CallAPI forms) |
| `probe_violations.py` | Inventory of LitStr/branch invariant violations per stream |
| `probe_dm8_map.py` | Extract the Data_management#8 channel-lookup variable map |
| `probe_bad.py` | Targeted dumps for the three anomaly classes found during decode |
| `ref_lap_probe.py` | Reference lap: channel list + distance/velocity candidates |
| `ref_lap_probe2.py` | Reference lap: exact distance/velocity statistics |
| `ref_lap_probe3.py` | Reference lap: crown/camber/aids/rear-diff candidates |
