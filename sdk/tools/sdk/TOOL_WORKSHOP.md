# F1C tool workshop

The repository exposes jobs, not a pile of scripts. Start with:

```powershell
python tools/f1c_mod.py recommend "what you need to do" --json
python tools/f1c_mod.py capabilities --json
python tools/f1c_mod.py tool-info TOOL_OR_PATH --json
```

The workshop analogy is a selection rule:

| Role | Meaning |
|---|---|
| Workbench | Orients and coordinates the complete workflow. |
| Tool chest | Selects and explains the correct tool. |
| Magnifying glass | Finds static evidence and prior art. |
| Measuring tape | Measures known static state. |
| Multimeter | Observes live state without commanding it. |
| Camera | Captures a visual, trace, or memory record. |
| Level | Validates, compares, and tests. |
| Saw | Generates or builds a bounded artifact. |
| Jig | Repeats a multi-step workflow deterministically. |
| Tool cart | Stages or packages owned output. |
| Hammer | Intentionally commands input or live state. |
| Dremel | Performs a narrow expert task with sharp edges. |
| Label maker | Builds indexes and generated reference material. |
| Locked cabinet | Can affect credentials, binaries, production state, or uncontrolled process state. |
| Museum shelf | Preserved history, a superseded implementation, or evidence only. |

There is only one `preferred` capability for a job. An implementation labelled
`backend` is used through `f1c_mod.py`; `specialist` means it has a narrower job
that the preferred tool does not cover. `superseded`, `evidence-only`, `paused`,
and `locked-cabinet` implementations are never selected automatically.

Nothing is deleted when a tool loses selection priority. The catalogue records
why it is no longer the default and which capability owns the job now.

The machine-readable source of truth is
`tools/sdk/F1C_TOOL_CATALOG.json`. The CLI inventories executable-like files in
the root, `tools/`, and mod-local tool directories and applies the catalogue's
exact overrides and ordered rules. A newly added tool must therefore receive a
role and status before the complete SDK gate can pass.
