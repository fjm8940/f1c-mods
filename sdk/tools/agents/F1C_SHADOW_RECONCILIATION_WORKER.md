# F1C Shadow Architecture Reconciliation Worker

Reconcile validated subsystem proposals into a conservative engine map. This is
an independent candidate that will be checked against other candidates later.

## Rules

1. Use only the supplied packet and write exactly one JSON object to the
   requested output path.
2. Follow `analysis/shadow_reconciliation_schema.json`.
3. `packet_id` must exactly match the packet.
4. Every `required_function_ref` and every `required_class_proposal_ref` must
   occur in the output or in `unresolved_evidence_refs`.
5. Use only `allowed_evidence_refs`. Do not invent addresses, field offsets,
   masks, state values, or evidence references.
6. Trust is limited to `DERIVED`, `GUESS`, and `UNKNOWN`.
7. Never merge objects merely because both use `this+the_same_offset`. `this`
   offsets from unrelated functions are not globally unique. Merge class
   proposals only when method roles, responsibilities, call relationships, or
   explicit owner evidence support it.
8. Treat field samples as boundary clues, not complete layouts. Preserve the
   source proposal references and describe a `field_policy` for each type family.
9. Compatibility contracts must be observable requirements supported by input
   evidence. Do not invent APIs or file formats.
10. Record contradictions in `conflicts`; do not erase them to create a clean map.

## Recommended shapes

- `engine_subsystems`: `name`, `responsibility`, `functions`, `trust`,
  `evidence_refs`.
- `canonical_type_families`: `name`, `responsibility`, `source_class_refs`,
  `methods`, `field_policy`, `trust`, `evidence_refs`.
- `state_machines`: `name`, `owner`, `states`, `transitions`, `trust`,
  `evidence_refs`.
- `interfaces`: `name`, `purpose`, `functions`, `trust`, `evidence_refs`.
- `dependencies`: `from`, `to`, `kind`, `trust`, `evidence_refs`.
- `compatibility_contracts`: `name`, `category`, `description`, `trust`,
  `source_contract_refs`, `evidence_refs`.
- `conflicts`: `subject`, `alternatives`, `resolution`, `trust`,
  `evidence_refs`.

Prefer a small, defensible engine map over an imaginative rewrite.
