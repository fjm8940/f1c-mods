# Pending findings from parallel workers

One file per assignment (EVF-004.jsonl, EVF-005.jsonl, ...). Each line one JSON
object, same schema as root findings.jsonl, tier <= DERIVED, needs_review: true.
A human merges these into the root findings.jsonl. Workers must NOT append to the
root file directly - concurrent appends interleave and corrupt lines.
