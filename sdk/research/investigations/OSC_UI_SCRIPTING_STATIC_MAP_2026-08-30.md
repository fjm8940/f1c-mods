# F1 Challenge OSC UI Scripting — Static Map

Date: 2026-08-30  
Scope: read-only reconstruction of the executable's Options Screen Configuration
(OSC) reader, UI object construction, action representation, resource pathing,
and lifecycle. This report does **not** claim live behaviour and did not modify
the running game, executable, MAS archives, or OSC files.

## Confidence and evidence rules

- **Verified** means directly visible in the raw Hex-Rays C corpus at the stated
  executable address. It is still a decompiler reconstruction, not a source-code
  guarantee.
- **Inferred** means the conclusion follows from verified call/data flow or a
  derived skeleton label; it is explicitly not a supported SDK contract.
- **Open** means static evidence was insufficient. Do not fill these gaps with
  guessed OSC syntax in a mod or production asset.

The mapped subsystem is an executable-native configuration reader. It is not
Open Sound Control networking; the parser itself names its log files
`Log\\osc.information` and `Log\\osc.err`.

## Executive map

```text
screen/menu owner (0x00485B90)
  -> OSC reader (0x004944A0)
       -> open source + optional diagnostic sink (0x00491EE0)
       -> SourceMAS pre-pass (0x004920B0)
       -> Options object parse (0x00492160)
            -> AttributeSet (0x00493060)
            -> Layer (0x00492460)
                 -> Page (0x00492710)
                 -> Gizmo (0x00492A40)
                      -> widget factory (0x00493FC0)
                      -> Actions -> typed action nodes (0x00493480)
       -> stream cleanup (0x00491F60)
```

The parser operates on normalized, line-oriented text and recurses through
brace-delimited layer/page/gizmo/frame blocks. It builds a retained UI object
tree plus per-event action lists; it does not interpret actions while parsing.

## Verified parser and grammar mechanics

### Input and normalization

| Address | Role | Direct evidence |
|---|---|---|
| `0x00491EE0` | OSC input/log opening | Opens the input source in `rb`; if diagnostic mode is requested, opens a second `wt` stream. If that stream cannot be opened it disables the diagnostic flag, while input reading can continue. |
| `0x005351D0` | physical line reader | Reads one byte at a time until LF or capacity exhaustion and null-terminates. OSC calls it with a 256-byte buffer. |
| `0x005651B0` | line normalizer/classifier | Strips leading whitespace, removes `//` comments outside quoted text, tracks quotes and parenthesis nesting, removes whitespace while inside parentheses, uppercases ASCII outside locale conversion, and trims trailing literal spaces. Its precise status-code meaning is only partly recovered. |
| `0x00491FB0` | logical-record iterator | Calls the physical reader and normalizer, increments parser line counter `this[99]`, and skips selected blank/comment-like status records until it returns a logical record or reaches EOF/error. |
| `0x005654A0` | `KEY: value` extractor | Copies the current normalized line, splits at `:`, performs case-insensitive key comparison, then returns the value to a caller buffer. |
| `0x00565400`, `0x00565520`, `0x00565760`, `0x00565870` | typed field parsing family | The common `KEY: value` matcher feeds string, integer tuple, and float tuple consumers. `0x00565760` visibly accepts either `n` or `(n,...)`; derived cross-checks identify `0x00565870` as the analogous float/vector reader. |

Consequences that are directly supported by this code:

- Keywords are case-insensitive (`0x00588410`) and normalizer output is uppercased
  (`0x00589901`); authoring should not rely on case to distinguish names.
- `//` begins a comment only outside quoted text. Parentheses protect a tuple
  while its internal spacing is normalized away.
- `:` is the primary key/value separator (`0x005654A0`). `:` and tab are also
  used as the delimiter pair for `SetSymbol` assignments (`0x00492160`,
  `0x00493480`).
- Nested parsers stop on the same close-token test (`0x00587C20` against the
  constant at `0x005CC4F4`), while `0x00492030` is an explicit balanced-block
  skipper for an unknown/unsupported gizmo definition.

### Recovered grammar (verified tokens; brace spelling inferred)

The following expresses the structurally verified grammar, not a promise that
every optional field applies to every widget type:

```text
OSC       := { SourceMAS-block } { top-level directive }
top-level := AttributeSet | Layer | Gizmo | DoPage | LoadingScreenP
             | SystemFont | TooltipFont | TooltipColorFirst | TooltipColorEdge
             | SetSymbol | DisableGizmo
Layer     := NORMAL | MODAL, then { Name | SoundFocus | SoundLostFocus
             | Gizmo | Page | other surface/resource line }
Page      := { Name | EnterTarget | EscTarget | Location | PosWH | Location2
             | Surface | Gizmo }
Gizmo     := type-name, then { Name | SourceVariable | Variable | VariableValue
             | PosWH | Position | WidthHeight | Pos2Offset | Offset
             | Caption1 | Caption2 | Tooltip | Child | Actions | AttributeSet
             | FriendGizmo }
Actions   := event-block containing action directives
Frame     := { Location | PosWH | Surface | AnimDelta | AnimRule | SubAnimFrame }
```

`Layer`, `Page`, `Gizmo`, `Actions`, and `Frame` have verified nested parsing
functions, so they are blocks. The exact accepted opener syntax and full
top-level ordering remain open because the normalizer's status convention and
the shipped OSC assets were not live-read in this task.

## Top-level directives and state ownership

`0x00492160` is the primary options-screen reader. It first validates its
options owner at parser offset `+0x1A4` (420). A null owner emits a severity-3
OSC diagnostic rather than dereferencing it.

| Directive | Verified parser action | Owning destination / status |
|---|---|---|
| `AttributeSet` | Calls `0x00493060` | Allocates a 124-byte attribute object and rejects an empty or duplicate name. |
| `Layer` | Calls `0x00492460` | Allocates a 188-byte normal/modal layer and inserts it into its layer list. |
| `Gizmo` | Calls `0x00492A40` | Builds a widget, then links it as a root gizmo when accepted. |
| `DoPage` | Name lookup then a page-state call | Existing page is selected/activated through the UI list; exact transition semantics remain open. |
| `LoadingScreenP` | Name lookup then sets a state field | A matching page is marked with a `+0x30`-style setter. |
| `SystemFont` | Calls `0x00498440` | Validates against the registered system-font table; missing names show a Windows error and fall back to the first available font. |
| `TooltipFont` | Copies supplied value to the options owner | Stored near owner `+0x1538` (5432). |
| `TooltipColorFirst`, `TooltipColorEdge` | Typed integer-vector parser | Writes colour tuple fields near options owner `+0xB4/+0xB8` (180/184). |
| `SetSymbol` | Splits the value at `:`/TAB and updates symbol lookup | The exact backing table/value lifetime is open. |
| `DisableGizmo` | Looks up named object then calls virtual `+0x38` with `1` | Verified as a direct UI-object state change during loading. |

Parser-local fields visible across this chain: `+0x18C` line number (396),
`+0x190` normalizer/current-line workspace, `+0x194` nesting depth (404),
`+0x198` diagnostic mode byte (408), `+0x19A` resource-path mode byte (410),
`+0x19C` resource-path depth/count (412), `+0x1A4` options owner (420),
`+0x1A8` current layer (424), and `+0x1AC` current page (428). Exact structure
size and every field meaning require live/type recovery.

## Retained UI tree and widget lifecycle

### Construction

- `0x004944A0` is the OSC reader entry: it stores a source-name string, records
  mode bytes, opens streams, executes the `SourceMAS` pre-pass, optionally loads
  intro imagery, calls `0x00492160`, then cleans stream fields. It is called by
  `0x00485B90`, the screen/menu owner.
- `0x004920B0` accepts **at most 33** `SourceMAS` records: `v2` starts at 32,
  the body runs before `--v2`, and continuation is tested while `v2 >= 0`
  (iterations 32 down through 0). Each accepted file is handed to
  `0x004979F0`; a failed load emits `Could Not Load MAS file %s (line %d)`.
- `0x00492460` chooses `NORMAL` or `MODAL`; any other layer type emits the
  severity-3 message `undefined layer type %s`. It attaches child gizmos/pages
  while the current-layer pointer is live, then clears it on return.
- `0x00492710` allocates a 104-byte page node, records enter/escape target
  strings, rectangle geometry, optional surface and child gizmos, then clears
  the current-page pointer on return.
- `0x00492A40` sets widget parent references to current options/layer/page,
  parses geometry/text/variables/children/actions/attribute set/friend gizmos,
  and records source or variable strings on the widget. Unknown type invokes
  `0x00492030` to consume its whole nested block and logs a warning.
- `0x00493FC0` resolves a prelinked widget via the owning container first; if
  absent, it matches a 23-entry keyword table at `0x005CC270` and invokes one
  of 23 constructors. It avoids double-decoration for an object whose `+0x8C`
  field is zero; otherwise it logs `Prelinked Gizmo - <type>`.

The 23 factory **type names are not yet recovered**. Follow-up evidence confirms
why: the searchable corpus has the reference to `off_5CC270` only in
`0x00493FC0`; `evidence-query global 0x005CC270` reports exactly one reader and
zero writers, and no serialized pointer-table contents/string cross references.
The raw decompiler corpus omits the table's data bytes, so there is no
evidence-safe route from its 23 indices to names. Names such as Button/Text/List
are intentionally not guessed.

### Screen/game lifecycle context

| Stage | Verified calls affecting on-screen system |
|---|---|
| `Game_Init` `0x0042D6B0` | Initializes input/render/etc., then calls `OnScreen_Init` `0x0049EFA0`. The latter initializes on-screen state and loads four standard display resources. |
| `Game_Setup` `0x0042D5D0` | Calls `OnScreen_Setup` `0x0049EF70`. |
| `Game_Enter` `0x0042D370` | Calls `OnScreen_Enter` `0x0049EF40` after input entry and before session entry finishes. |
| `Game_Restart` `0x0042D7A0` | Calls `OnScreen_Restart` `0x0049F000`, which resets on-screen state and walks 25 registered slots, dispatching non-null ones. |
| game main loop `0x0042D8A0` | Calls `ShortComputation_f070(&dword_7CE048)` only in the normal render path. This is a strong but still **inferred** per-frame on-screen update boundary; its implementation was not recovered here. |
| `Game_Post`/`GameExit` | Call `OnScreen_Post` `0x0049F0D0` (multi-stage cleanup) and `OnScreen_Exit` `0x0049F120`. |

The menu/screen manager `0x00485B90` lazily creates an 8612-byte `ScreenData`
object, invokes the OSC reader, renders it (`UILargeRender`), and reacts to a
global screen-result state. `0x00470040` chooses one of three navigation lists
based on global `0x007D0410`; `0x0048D6B0` picks a selected UI item and dispatches
virtual `+0x2C` on its owner.

## Actions, callbacks, and variables

### Verified action representation

`0x00493480` turns an `Actions` block into 28-byte heap nodes and appends each
node to one of four event-linked lists based on its containing event name. The
gizmo owns their head fields at `+0xCC`, `+0xD0`, `+0xD4`, and `+0xD8`;
each action node links to the next at `+0x18`. The parser does **not** execute
the node there. This cleanly separates OSC construction from runtime evaluation.

| Event | Gizmo list head | Append helper |
|---|---:|---|
| `Click` | `+0xCC` | `0x00486480` |
| `Release` | `+0xD0` | `0x004864C0` |
| `Focus` | `+0xD4` | `0x00486500` |
| `LostFocus` | `+0xD8` | `0x00486540` |

| Opcode | OSC directive | Stored payload |
|---:|---|---|
| 1 | `DoPage` | target string |
| 2 | `DO_PAGE_TRANSITION_SLIDE` | target string |
| 3 | `Do_Page_Transition_Shred` | target string |
| 4 | `Do_Page_Transition_Iris_Open` | target string |
| 5 | `Do_Page_Transition_Iris_Close` | target string |
| 6 | `Do_Page_Transition_Attribute` | target string |
| 7 | `DoLayer` | target string |
| 9 | `SetSymbol` | name/value split at `:` or TAB |
| 10 | `DropPopups` | no string payload observed |
| 11 | `ExitGame` | no string payload observed |
| 12 | `SimulateClick` | target string |
| metadata | `TransitionAttr` | resolved attribute-set pointer stored in node field 5 |

There is no opcode 8 produced by this parser. The five transition spellings
need an asset/sample correlation before their external names or animation
meaning can be stated.

### Callback/evaluator status (superseded by the static closure addendum)

The event lists are verified to be owned by the gizmo/action structures. The
closest recovered runtime route is `UILargeRender` (`0x00496930`): it collects
input events, calls `UIClickEventDispatch` (`0x004957A0`), and that function
then invokes a virtual method at **vtable `+0x20`** on the UI manager. This is a
verified control-flow boundary, not proof that `+0x20` is the opcode evaluator.

The first pass did not recover the evaluator further from the current static corpus:
there are no direct xrefs from `0x0049A580` (the 28-byte action-node initializer)
other than `0x00493480`, and the concrete factory vtables/pointer-table data are
not represented as searchable raw C. Thus the list fields may be consumed by a
type-specific vtable implementation reached indirectly, but the target and its
switch could not yet be named safely. The addendum below subsequently identifies
`0x0049A5D0` as the concrete opcode evaluator, while retaining an exact
argument-handoff caveat at `0x00486600` and `0x00486630`. Therefore these bullets
describe the earlier boundary, not the report's current stopping point:

- Trigger routing (`Click`/`Release`/`Focus`/`LostFocus`) is verified at parse
  time; runtime firing order, bubbling, reentrancy, and callback ABI are open.
- `SimulateClick` is represented in configuration but its target-resolution and
  recursion guard are open.
- Page/layer transitions and `ExitGame` have verified command nodes but no safe
  callable address or SDK-facing command contract.

### Variables and symbols (initial boundary; registry closed in addendum)

At gizmo level, `SourceVariable`, `Variable`, and `VariableValue` all route a
string into a widget setter (`0x00492A40`). `SetSymbol` is accepted both at
top-level (`0x00492160`) and in action blocks (`0x00493480`), with the latter
allocating opcode 9. Existing lookups are case-insensitive: string comparison
at `0x00588410` folds ASCII case, and `0x0049A360` hashes normalized names for
another lookup path. The addendum proves the owner-relative symbol registry,
normalized lookup/update, 28-byte node allocation, and linked-list collision
chain. Bucket count, full node layout, and update timing remain open.

## MAS and resource resolution

| Boundary | Verified behaviour | Limit |
|---|---|---|
| `SourceMAS` -> `0x004979F0` | Concatenates the current base path (`dword_5EC408` or default empty path) with the requested MAS name, resolves/open-checks it, then stores a path/handle record in a parser-owned linked list. | The addendum closes the lower loader order as direct path first, then registered roots in ascending order; duplicate behavior above that boundary remains open. |
| `0x0048AED0` (`UIScreenLoad`) | Concatenates `dword_5EC408` with a screen path, opens it through the resource/file interface, reads it into an allocated buffer, and replaces the caller's previous buffer. | It is a screen-file loader, not itself the OSC evaluator. |
| `0x004F9D60` (`ResourceFileLoader`) | Attempts requested resource name, then base-path-prefixed name, loading/rendering it with an optional timed wait. | It does not expose an author-safe direct call ABI. |
| `0x00496380` | Selects `GameTitle.bmp` or a mode-specific alternative and invokes resource loader. | Included to show OSC reader reaches the normal resource system. |

**Inference:** OSC resource names are resolved through a mutable base-path
context plus a per-reader MAS list rather than by hard-coded absolute paths.
The reader's `+0x19A/+0x19C` flags gate name/path handling in nested sections.
Precedence among MAS entries, fallback order across loose/MAS files, and whether
the game permits traversal outside intended roots remain open and should be
validated with a disposable, non-production screen asset only.

## Error and recovery behaviour

- The parser constructor `0x00493F50` clears initial state and installs
  `Log\\osc.information` plus `Log\\osc.err` defaults.
- `0x00493C80` formats `OSC <severity> on line <n> : <message>`, opens/appends
  its log sink lazily, and writes the message. Severity 2 (`Error`) and 3
  (`CriticalError`) also show a MessageBox unless callback/diagnostic mode is
  enabled; it then calls a further severity-dependent handler.
- Invalid options owner, unknown layer type, missing SourceMAS and unknown
  gizmo type have direct error paths. Unknown gizmo recovery deliberately
  consumes the nested block, avoiding accidental interpretation of its fields
  as siblings.
- Allocation failures commonly return null/zero and parsing continues where the
  caller permits it. Exact transactional semantics (whether a partially
  constructed screen is later rejected) are open.

## Addresses worth preserving for a future controlled runtime pass

| Address | Proposed observation target | Confidence |
|---|---|---|
| `0x004944A0` | reader entry/source and mode flags | verified static |
| `0x00492160` | top-level token dispatch | verified static |
| `0x00492A40` | gizmo definition and parent links | verified static |
| `0x00493480` | action-node creation/event list routing | verified static |
| `0x00493C80` | OSC error diagnostics and line number | verified static |
| `0x00493FC0` | 23-entry widget factory/table `0x005CC270` | verified table/use; names unresolved |
| `0x0042D8A0` -> `ShortComputation_f070` | live UI tick boundary | inferred; implementation not mapped |

No direct function call, hook, memory write, UI click, or asset mutation is
authorized by this report. Any runtime work must use the SDK broker/guarded
full-weekend workflow and first identify a read-only observation capability.

## Open questions and next evidence

1. Recover the 23 factory keyword strings at `0x005CC270`, constructors, and
   the widget vtables used for click/focus handling.
2. Obtain/read-only inspect the concrete vtable data or an instruction-level
   xref for gizmo `+0xCC..+0xD8`, then map the virtual `+0x20` route to the
   opcode switch and event ordering.
3. Identify the actual OSC source locations and correlate a preserved, read-only
   sample to exact brace/open-token grammar.
4. Observe one normal menu interaction read-only to confirm parser load timing,
   selected-page transition, error log emission, and resource fallback order.
5. Type the options owner and symbol/variable registries before exposing any
   scripting-facing SDK interface.

## Evidence commands and task record

All project evidence was queried through the supported front door from the
repository root; no internal project script, injector, debugger automation, or
live-write capability was invoked.

- `python tools/f1c_mod.py start --json`, `sdk-info`, `doctor`, and `route ...`
  for orientation/routing.
- `recommend "map OSC UI resource scripting parser evaluator screen callbacks MAS lookup" --json`.
- `search` across raw/facts/derived corpus for `osc.err`, `osc.information`,
  `SourceMAS`, `DoPageTransition`, `SourceVariable`, `off_5CC270`, and function
  addresses.
- `inspect` of the functions cited in this report, notably the `0x00491EE0`
  through `0x004944A0` parse range, UI lifecycle functions, and resource
  functions. JSON output was filtered only to display the tool-returned source
  text; it was not transformed into project files.
- Follow-up: `evidence-query global 0x005CC270`, `evidence-query func
  0x00493480`, inspection of `0x00486480/0x004864C0/0x00486500/0x00486540`,
  `UILargeRender`, and `UIClickEventDispatch`. These established the exact
  action-list offsets, the 33-item SourceMAS bound, and the indirect-vtable
  stop condition described above.
- Root workspace structural check was run at orientation and reported pre-existing
  workspace errors (missing preserved game roots and existing production source
  placement); this investigation did not modify them. SDK `doctor` passed.

### Wasted-effort check

- Repeated reading: avoided by narrowing from broad keyword hits to exact parser
  functions after the first corpus search.
- Broad builds/tests: none; this was static research only.
- Failed/retried commands: one broad `OSC`/`MAS` search produced noisy results,
  and two `search --context 20` requests were rejected because the tool caps
  context at 10. The follow-up also confirmed that broad offset searches mix
  unrelated structs, so targeted function/xref evidence was used instead.
- Polling/waiting: none.

## Static closure addendum — action execution, factories, symbols, MAS, and limits

This additive addendum supersedes only the earlier *unresolved evaluator* note. It is static-only evidence from the canonical decompilation corpus; no game process, executable, resource file, or live UI was touched.

### Verified action records and semantic evaluator

`0x00493480` parses an `ACTIONS` block (called by gizmo parsing at `0x00492A40`). Each recognized command allocates a 28-byte record through `0x0049A580`, then places it on the supplied event list: `CLICK` -> widget `+0xCC`, `RELEASE` -> `+0xD0`, `FOCUS` -> `+0xD4`, and `LOSTFOCUS` -> `+0xD8`. The link field is `+0x18`. Parser `this[101]` is incremented on entry and decremented on exit.

| Opcode | OSC action | Proved fields | Static effect at `0x0049A5D0` |
|---:|---|---|---|
| 1 | `DOPAGE` | `+0x00` opcode, `+0x04` target | resolves a symbol/raw page; helper mode `-1`; writes `CURRENT_PAGE` |
| 2 | `DOPAGE_TRANSITION_SLIDE` | same | helper mode `0`; writes `CURRENT_PAGE` |
| 3 | `DOPAGE_TRANSITION_SHRED` | `+0x04`; optional `+0x14` `TRANSITION_ATTRIBUTE` | helper mode `1` |
| 4 | `DOPAGE_TRANSITION_IRIS_OPEN` | same | helper mode `2` |
| 5 | `DOPAGE_TRANSITION_IRIS_CLOSE` | same | helper mode `3` |
| 6 | `DOPAGE_TRANSITION_ATTRIBUTE` | same | helper mode `4` |
| 7 | `DOLAYER` | `+0x04` target | resolves an item and calls `0x004974A0` |
| 9 | `SET_SYMBOL` | `+0x04` key, `+0x0C` value | writes the symbol table |
| 10 | `DROP_POPUPS` | opcode only | calls `0x004979B0` |
| 11 | `EXIT_GAME` | opcode only | writes `0x005CC9E8 = -1` |
| 12 | `SIMULATECLICK` | parser string; evaluator reads `+0x0C` | finds object; virtual `+0x8C`, `0x004979C0`, virtual `+0x90` |

`0x0049A5D0` is the verified switch-based action semantic evaluator. `0x00486600` and `0x00486630` directly call it while traversing heads at `+0xCC` and `+0xD0`. **Unresolved boundary:** both small Hex-Rays functions show `sub_49A5D0(this)` while separately iterating an `i` node, which is not type-consistent with the evaluator’s opcode switch. Therefore the exact register/argument hand-off is not claimed. The earlier input boundary `0x00496930 -> 0x004957A0 -> virtual +0x20` remains separately proved.

### Verified symbol registry

The action evaluator uses the table at `*(a2+0x40)+0x168C` (5772 decimal). `0x0049A490` copies a key into a 256-byte stack buffer, normalizes with `0x00589901`, then updates an existing node (`0x0049A360`/`0x0049A2E0`) or creates one (`0x0049A3B0`). Lookup `0x0049A460` normalizes then returns the stored string. `0x0049A360` selects a bucket via `0x0049A330`, follows `+0x18` links, and case-insensitively compares with `0x00588410`; creation allocates 28 bytes and appends at `+0x18`.

**Strong inference:** this is the OSC symbol/variable registry used for `SET_SYMBOL` and page/layer indirection. Its bucket count and complete node layout remain unresolved because the index function and allocation initializer do not expose them in the inspected corpus.

### Widget factory: recoverable map and hard name boundary

`0x00493FC0` first asks the owner virtual `+0x18` for a prelinked gizmo. Otherwise it compares the requested type string to `off_5CC270[i]`, `i=0..22`. The corpus has no table data: `evidence-query global 0x005CC270` reports this as the sole reader and zero writers. The 23 textual keywords therefore cannot be recovered truthfully from this static corpus.

| Index | Allocation | Final vtable / construction result |
|---:|---:|---|
| 0, 1 | 228 | `0x00487220`, `off_5AB968`; flags `(0,0)` / `(1,0)` |
| 2 | 228 | `0x00487660`, `off_5ABA18`; `(0,0)` |
| 3 | 236 | base then `off_5AC8D8`; `+0xE4/+0xE8=0` |
| 4 | 240 | base then `off_5AC1D0`; `0x004883A0` initializes `+0xE0..+0xEC` |
| 5, 6, 7 | 236 | `0x00488B00`, `off_5ABC28`; `(1,-1)`, `(2,-1)`, `(3,-1)` |
| 8 | 272 | `0x0048AA40`, final `off_5ABE28` |
| 9 | 772 | `0x0048B420`, `off_5ABED0` |
| 10 | 372 | `0x004893C0`, `off_5ABD78` |
| 11 | 236 | manual `off_5AC830`, constant fields |
| 12 | 280 | `0x00488180`, `off_5ABB78` |
| 13 | 228 | `0x00487ED0`, `off_5ABAC8` |
| 14 | — | no switch case: default returns existing/null |
| 15 | 296 | `0x0048D740`, `off_5AC328` |
| 16 | 240 | `0x0048D660`, `off_5AC280` |
| 17 | 240 | `0x0048E070`, `off_5AC3E0` |
| 18 | 260 | `0x0048E300`, `off_5AC490` |
| 19 | 240 | `0x0048E600`, `off_5AC540` |
| 20 | 384 | `0x0048EAA0`; final vtable lies beyond the inspected excerpt |
| 21 | 232 | `0x0048FE60`, `off_5AC6B0` |
| 22 | 244 | `0x00490250`, `off_5AC760` |

All allocation failures immediately return zero. A found prelinked object with non-null `+0x8C` logs `Prelinked Gizmo - %s` and returns zero. Semantic widget names and vtable method meanings remain open without table data or independent evidence.

### SourceMAS precedence, failure, and exact bounds

`0x004920B0` begins `v2 = 32`, processes a record, decrements, and continues while `v2 >= 0`: **at most 33** `SOURCE_MAS` records, not 32 or 34. Failed `0x004979F0` calls produce `Could Not Load MAS file %s (line %d)` through `0x00493C80`.

`0x0053CBC0` uppercases the name, tries that direct path first, and only on failure tries each root at `0x008B5A3C + 132*i` in ascending order for `i < dword_8B8E30`; first success wins. This call order is verified; its user-facing priority meaning is strong inference. All failure calls `0x0053C590` with `Error opening MAS file %s` and returns `-2`; `0x00494FE0` clears the output pointer on any nonzero result. The loader reads exactly 12 header bytes. `CUBEMAS410` allocates/registers a 308-byte object, frees it for loader returns `-2/-1`, and increments `0x008B6AC0` only at return `0`. The non-410 comparison for `CUBEMAS400/401` is counterintuitive in raw code, so compatibility interpretation remains open.

### Limits, recovery, and stopping point

Verified fixed values: physical line reader call site 256 bytes; `SOURCE_MAS` value 256; top/action/factory token temporaries 125; diagnostic text 253; MAS paths 256 and header 16; action/symbol nodes 28; factory scan 23 slots; resource-path handling guarded at parse depth `<4`. No inspected static code fixes a maximum for layers, pages, gizmos, actions, attribute sets, or symbol entries beyond allocation failure and those guards. Visible `strcpy` calls lack a local bound check; that is only a static observation, not a vulnerability conclusion.

Evidence commands: `inspect 0x00493480 --corpus raw --max-lines 260`; `inspect 0x0049A5D0`, `0x00486600`, `0x00486630`; `inspect 0x0049A490/0x0049A360/0x0049A3B0/0x0049A460 --corpus raw`; `inspect 0x00493FC0 --corpus raw --max-lines 260`; constructor inspections; `evidence-query global 0x005CC270`; and `inspect 0x004920B0`, `0x0053CBC0`, `0x00494FE0`.

Wasted-effort prevention: table recovery was traced from its sole reader and queried directly; no pointer/data bytes exist in the searchable corpus, so repeated searches will not recover names. Exact event-node argument provenance is likewise blocked by the inconsistent Hex-Rays form at `0x00486600/0x00486630`; a new static artifact or permitted independent source is required.

Post-edit verification: the record exists; `python tools/f1c_mod.py doctor` from this component passed. The root workspace structure check still reports the same nine unrelated pre-existing errors (missing preserved game roots, one missing production path, and four pre-existing source-like files under production). It did not report an OSC investigation/documentation error. One attempted `doctor` from the workspace root used the wrong relative tool path and failed before inspection; it was immediately corrected by running it from this component root.
