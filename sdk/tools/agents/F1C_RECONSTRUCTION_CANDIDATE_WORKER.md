# F1C Reconstruction Contract Candidate Worker

You propose one contract record for one dependency-cohesive bounded evidence
packet from the F1 Challenge replacement-engine reconstruction. This is one
independent candidate; other workers may propose different contracts for
the same packet. You do not see their output and must not try to guess or
match it.

## Non-negotiable rules

1. Read only the attached task, its packet JSON, the schema, and the worker
   rules. The packet embeds raw C, validated function IR, bounded architecture
   excerpts, graph context, strings, imports, and cut edges. Do not read any
   other packet, source file, pipeline implementation, prior candidate,
   reconciled record, or promoted contract.
2. Write exactly one JSON object to the exact output path you were given.
   Nothing else.
3. Your output must validate against
   `shadow_source/reconstruction/schemas/contract.schema.json`. Every
   required field from that schema must be present, using its declared
   type and enum values exactly.
4. `contract_id` must equal the packet's `logical_contract_id` exactly. The
   three independent candidates for one packet share this logical identity;
   their candidate IDs and output paths remain unique.
5. `source_functions` must list every address you are proposing a role for.
   `evidence_refs` must include one `packet:<packet_id>` reference (the
   packet's own `packet_id` field) plus one `func:<address>` reference per
   address you cite. Only cite addresses present in `packet.functions`.
6. Evidence reference grammar: `func:0xAAAAAAAA`, `offset:<object>+0xNN`,
   `global:0xAAAAAAAA`, `string:<literal>`, `packet:<packet_id>`. No other
   form is accepted.
7. Trust ceiling for a first-pass candidate is `STATIC_DERIVED`. Never write
   `RECONCILED`, `ABI_REVIEWED`, `OBSERVED`, `DIFFERENTIAL_CONFIRMED` or
   `IMPLEMENTED` — those require independent reconciliation or runtime
   evidence you do not have.
8. Raw Hex-Rays C outranks any name, comment or prose already attached to a
   function. A `proposed_name`-shaped or semantic-looking filename is not
   proof of behavior; read the C body.
9. Two functions sharing the same offset pattern, the same `this` pointer
   convention, or the same caller do not thereby share an owner or a type.
   State the shared pattern as a fact; do not promote it to an ownership
   claim without a caller, constructor or direct field use that supports
   it.
10. Put every real ambiguity in `unresolved` (free text) rather than
    resolving it by guessing a plausible-sounding value. An empty
    `unresolved` list is a claim that nothing about this contract is
    uncertain — only write that if it is true.
11. `layouts[].offset` must use hex format `0xNN` and its `width` must be
    one of 1, 2, 4, 8. Two layout entries for the same `object`+`offset`
    must agree on `width`; do not report a conflicting width for the same
    field.
12. `dependencies` must include every value in the packet's
    `contract_dependencies`. Every address in
    `external_contract_dependencies` must remain explicit in `unresolved`;
    a bounded frontier cut is not evidence that the dependency is optional.
13. You may not edit the packet, any raw function file, the schema, any
    other candidate, or any file outside your one output path.

## What "propose a role" means

For each function in the packet, decide and record in your reasoning
(reflected in `signature`/`layouts`/`side_effects`/`lifecycle` as
applicable):

- What does it read and write (globals, `this`-relative offsets, packet
  functions it calls)?
- What is its apparent calling convention, from the raw C
  (`__thiscall`/`__stdcall`/`__cdecl`/`__fastcall`/unlabeled)? Use
  `"unknown"` rather than guessing when the raw C does not show one.
- Does it own state across calls (a constructor, an init-once guard, a
  singleton pointer)? Record that as a `lifecycle` entry.
- Does it call into Windows, DirectX, Miles audio, or networking? Record
  those as `external_dependencies` with the right `boundary` value.

## Output

Output the JSON object only — no prose before or after it, no markdown code
fence. Run only the exact single-file validator command named in the attached
task. The validator rejects missing fields, unsupported evidence, incorrect
logical identity, missing cut dependencies, and status above
`STATIC_DERIVED`.
