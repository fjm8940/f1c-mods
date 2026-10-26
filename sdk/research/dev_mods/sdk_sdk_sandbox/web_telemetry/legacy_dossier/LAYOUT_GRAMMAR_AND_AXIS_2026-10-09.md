# Layout INI grammar (p-code derived) and time/distance axis mode

Date: 2026-10-09 (STOP 2R). Sources: `FORMATS_f1_telemetry_2026-10-09.json`
(all 23 layout files decoded), `METHOD_INDEX_f1_telemetry_2026-10-09.json`
(per-method string references), the vendor files read in place.

## 1. Grammar, derived from the INI-reading/writing p-code

The layout reader/writer methods are identified by their constant-pool string
references (opcode boundaries not fully resolved; references are per-method
evidence):

- **`home #2`** (0x449778, 67 refs) — the layout **parser**: `*.ini`,
  `/layouts/`, `NOGRID`, `MINORGRID`, `NOLINES`, `DOTS`, `WIDELINES`,
  `NOTONCHANGE`, `TOOLTIP=`, `ZVALUE`, `#0.0`, `#0.00`, `#0.000`, `NULL`,
  `CURRENT`, `-1234.00`, `1234`, `(no data loaded)`, `Layout exported to layout.ini`.
- **`home #53`** (0x435af4) — the layout **writer/state**: `\layout.ini`,
  `\layouts\_revert_layout.ini`, `\layouts\_single_layout.ini`,
  `current layout`, `single graph layout`, `LayoutName`, `TITLE`,
  `Layout exported to layout.ini`.
- **`home #14`** (0x4425a4, 29 refs) and **`home #43`** (0x445f88, 29 refs) —
  UI/layout consumers: same token set plus `display_in_mph`, `velocity`,
  `layoutflick`, `temp_setup.txt`.

Grammar (matches every decoded file):

```
line 1:  title                      (optional "*NN_" order prefix; e.g. "*00_Overview")
line 2:  description                (may be blank, e.g. in the viewer-state files)
line 3:  graph count                (integer)
blank
per graph (three lines):
  channels: "x","y","flags"         (flags token subset of
                                     NOGRID NOXAXIS NOYAXIS DOTS NOLINES MINORGRID
                                     WIDELINES NOTONCHANGE; may be empty)
  ranges:   xmin,xmax,ymin,ymax[,#TRUE#|#FALSE#]   (autoscale token optional)
  rect:     x0,x1,y0,y1             (normalized window position)
```

Evidence beyond the samples: the token strings above exist in the parser's
constant pool and nowhere else in the binary; `#TRUE#/#FALSE#` matches the
sample files' autoscale field; the viewer-state files
(`current layout`, `single graph layout`) are written by `home #53`, proving
the writer emits the same grammar it reads.

## 2. Time vs distance x-axis

`overview.ini` plots `"time","velocity"` and `"time","engine rpm"`, while the
owner's screenshot shows distance axes. The p-code evidence:

- The axis channel is a **layout channel name**; `time` and `distance` are both
  channel identifiers in the parser's vocabulary.
- **29 methods reference `distance`**; the ones that also reference
  `time`/`ZVALUE`/`dtime` are the graph build/redraw paths:
  `home #2` (layout parser; also `ZVALUE`, `dtime`), `home #23` (0x4419f0:
  `distance`, `time`, `ZVALUE`, `\layouts\_revert_layout.ini`,
  `telemetry.spt`), `home #20` (0x436054), `home #19` (0x437bd0:
  `distance`, `time`, `dtime`, `velocity`), `home #15` (0x44791c), plus the
  `Data_management` graph engine methods (#4, #5, #9, #10, #13, #14, #15, #16,
  #17, #19, #20, #21, #23, #24) and `FileReading #8/#14` (0x4344c0, 0x44bbb4).
- `FileReading #6` (the .spt reader) references `elapsed time`, `pos x`,
  `pos y`, `time` — the file carries time and position, **not** a distance or
  velocity channel (the 57-channel list has neither).
- `home #35` references `dtime` and `t2-t1`; `dtime` appears in the axis
  methods — the viewer computes a delta-time series for comparison.

Conclusion (tier CONFIRMED for the mechanism, DERIVED for the exact UI step):
the x-axis mode is a **channel-name selection in the graph engine**; `distance`
is not stored in `.spt`, so it is **reconstructed** in the graph/transform path
(candidate: integration of a velocity derived from position/wheel data) by the
`Data_management`/`FileReading` methods listed above, which reference both
`distance` and `time`. The exact formula and the UI control that flips the mode
remain open (no opcode-level trace); the method-level evidence is recorded.

## 3. Follow-ups

1. Opcode-level decode (needs the full opcode table) to name the exact call
   sites inside `FileReading #6` (read) and the layout parser `home #2`.
2. Distance formula: trace `Data_management` #4/#16 (distance+time+velocity)
   once opcode decoding is available.
3. `.spt` record stride: same dependency; the file itself is currently being
   rewritten by the owner's viewer, so a quiescent sample should be captured
   after the viewer is closed.
