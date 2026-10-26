# Research H5: root causes for bugs B1, B2, B5, B7, B9 (Telemetry console), 2026-10-14

Product: Telemetry (`web_telemetry`), console at `/` (4.4.0 line, storage v2 1.1.x).
Author: Matias. Task: READ-ONLY root-cause analysis. No file in the component was edited.
Proposed diffs below are text only and are NOT applied. No game, no build, no test exe, no
delete or move. The only file written is this record.

Evidence tiers used below:
- FACT = read in a file at the cited line, or listed on disk (names only).
- INFERENCE = reasoned from FACT, needs a test or a live check before a fix is trusted.
- OPEN = not answerable from code or disk alone.

## 0. Summary and next action

| Bug | Root cause (one line) | Where | Tier | Fix size |
|---|---|---|---|---|
| B1 | Practice has no lap limit; the game's TOTAL_LAPS (INT_MAX) is shown raw. Header adds `/ ?` from phase 0 | `telemetry_viewer.js:4751`, `:4748`, `:4061` | FACT (code), INFERENCE (INT_MAX value) | 3 lines, JS only |
| B2 | Session label is parsed from the file stem. The parser rejects the `_2` collision suffix and mode-only stems, so the label is empty and falls back to "Session". Weekend names with no mode fall back to raw text | `telemetry_viewer.js:4420` (regex), `:5627` (fallback), `:4440` (raw) ; writer `storage_v2.inc:1101`, `storage_writer.inc:986-991` | FACT (regex on disk names), INFERENCE (old builds) | JS regex + DLL META field |
| B5 | Live header prints the raw track folder name `trk` (`2002_Silverstone`). No display name is sent to the viewer | `telemetry_viewer.js:4061`, `:4023`, `:4516`; DLL `web_telemetry.c:2406-2407` | FACT | DLL field `trknm` + 2 JS lines |
| B7 | Graph channel catalog returns "not recorded" for every tyre-temp channel. Only `tyre wear` is wired, although per-wheel temps are already in the live sample (`ttemp`) | `workbench_ui.js:877-884` | FACT | 1 branch, JS only |
| B9 | `goPast()` sets source to past before any recording is chosen. `pastNoData()` then reports "Live values are hidden" | `telemetry_viewer.js:3991-3995`, `:3971`, `:4057` | FACT | 1 branch, JS only |

Next action: apply the four JS fixes (B1, B2 regex, B7, B9) in one batch with the tests in section 7,
then do the DLL field for B5 (and the META type name for old B2 files) as a second batch. Both need
the owner's gate run; the DLL change also needs a DLL payload re-pin.

## 1. Method and limits

- Grep over `*.js`, `*.c`, `*.inc`, `*.h`, `*.cpp` for the markers named in the task
  (2147483647, INT_MAX, 0x7FFFFFFF, lapsTotal, totalLaps, "LAP ", "Session", "not recorded").
  Note: no `2147483647` or `INT_MAX` literal exists in source. The INT_MAX value is an owner
  observation and is treated as INFERENCE (section 2).
- Read in full: `TELEMETRY_B2_RECORD_2026-10-14.md`, `STORAGE_V2_BATCH5_RECORD_2026-10-13.md`
  (the batch 1-4 records were grepped for naming and session terms, not read in full).
- Listed names only (no file contents) of the owner's recordings folder
  `%USERPROFILE%\Documents\F1 Challenge V10\Telemetry\recordings` (read-only `ls`/`find`).
- Ran the viewer's own name regex with `node -e` on real folder names (inline, no file written).
- Workspace validator (`tools/workspace/check-workspace-structure.ps1`): 2 errors, both the
  known pre-existing top-level folders `.claude` and `plans`. Unchanged by this task.
- Not done: governance cold start (`governance\README.md`, `PIPELINE.md`, `PRODUCT_STATUS.json`)
  was not read in this task. No stage claim is made here. The `findings.jsonl` rule applies to
  game-behaviour results; these are viewer bugs, so no finding was appended.

## 2. B1: session panel shows `LAP 1 / 2147483647`; header shows `Practice 1 / ?`

### Root cause (FACT, code)

Lap line, `telemetry_viewer.js:4751` (session panel, `Console` view):

```js
h += '<div class="k">LAP</div><div class="v">' + (d.tl > 0 ? lapNo(d.ll) : (d.ll || 0)) + ' / ' + (d.tl || 0) + '</div>';
```

The total `d.tl` is the raw game value. Nothing treats a huge value as "no limit". The value is
read from the game global `TOTAL_LAPS` (`web_telemetry.c:575`, mirror `:156`, `:187`,
address `0x008ACF8C` at `:308`) into `g_telemetry.total_laps` at `web_telemetry.c:5149`, and it
is sent as `"tl"` in the live JSON. The live JSON `tl` field is built around `web_telemetry.c:5880`.

Header, `telemetry_viewer.js:4748` (panel head) and `:4061` (chrome header):

```js
p.head.querySelector('.r').textContent = sessName(d.ty) + ' / ' + phaseName(d.ph);
... esc(sessName(j.ty)) + ' / ' + esc(phaseName(j.ph))
```

`PHASE` maps 0 to `'?'` (`telemetry_viewer.js:2355`). In practice the race phase is 0, so the
header prints `Practice 1 / ?`. Phase has no meaning outside a race.

### Why INT_MAX (INFERENCE, needs one live read)

- The owner reports `2147483647`, which is INT_MAX. The game's own cockpit code treats the same
  global as "unlimited": `findings.jsonl` line 803 records `lap dword_8ACF8C ('--' unlimited)`.
  This makes a sentinel for "no lap limit" the likely meaning in practice.
- Tag: INFERENCE. A read of `0x008ACF8C` in a practice session will confirm it. Read-only.

### Code excerpt (FACT)

```
telemetry_viewer.js:4748  p.head.querySelector('.r').textContent = sessName(d.ty) + ' / ' + phaseName(d.ph);
telemetry_viewer.js:4751  h += '<div class="k">LAP</div><div class="v">' + (d.tl > 0 ? lapNo(d.ll) : (d.ll || 0)) + ' / ' + (d.tl || 0) + '</div>';
telemetry_viewer.js:4061  '<span><b>' + esc(j.trk || 'unknown track') + '</b></span><span>' + esc(sessName(j.ty)) + ' / ' + esc(phaseName(j.ph)) + '</span>' +
```

### Proposed diff (NOT applied)

```diff
--- telemetry_viewer.js (session panel, around 4748-4751)
-            p.head.querySelector('.r').textContent = sessName(d.ty) + ' / ' + phaseName(d.ph);
+            p.head.querySelector('.r').textContent = sessName(d.ty) + (d.ph > 0 ? ' / ' + phaseName(d.ph) : '');
...
-            h += '<div class="k">LAP</div><div class="v">' + (d.tl > 0 ? lapNo(d.ll) : (d.ll || 0)) + ' / ' + (d.tl || 0) + '</div>';
+            var lapCur = d.tl > 0 ? lapNo(d.ll) : (d.ll || 0);
+            var lapLim = d.tl > 0 && d.tl < 1000 ? ' / ' + d.tl : '';   /* INT_MAX = no lap limit */
+            h += '<div class="k">LAP</div><div class="v">' + lapCur + lapLim + '</div>';
--- telemetry_viewer.js (chrome header, 4061)
-                '<span><b>' + ... + '</b></span><span>' + esc(sessName(j.ty)) + ' / ' + esc(phaseName(j.ph)) + '</span>' +
+                '<span><b>' + ... + '</b></span><span>' + esc(sessName(j.ty)) + (j.ph > 0 ? ' / ' + esc(phaseName(j.ph)) : '') + '</span>' +
```

Design choice: the fix is in the viewer, not the DLL. Changing `web_telemetry.c:5149` would
also change `session_reset_check` (`web_telemetry.c:4860-4862`), which compares lap totals to
detect a session reset. Keep the DLL as it is.

### Test that should cover it

- Add a pure helper `lapCountText(tl, ll)` (or the same logic) and export it on the `V` object
  next to `recTitle` (`telemetry_viewer.js:6081`). Assert `(0,3)` gives `'3'`, `(53,3)` gives
  `'3 / 53'`, `(2147483647,3)` gives `'3'`.
- Assert the header text for `ph=0` has no trailing `/ ?`.
- Existing coverage: none for the lap text or the phase text (grep: no `2147483647`, no `'?'` assertion).

## 3. B2: recordings show `Session` and raw group titles

### Root cause A (FACT): the name parser rejects the `_N` collision suffix

The viewer parses session and weekend names with one regex, `telemetry_viewer.js:4420`:

```js
var m = /^(.+?) (\d{4}) - (.+?) - (?:(.+?) - )?(\d{4})-(\d{2})-(\d{2}) (\d{2})-(\d{2})$/.exec(String(name || ''));
```

The end is anchored with `$` right after the time. The writer adds a collision suffix when a stem
already exists: `sv2_unique_stem` (`storage_v2.inc:1181-1193`) appends `_2`, `_3` ...; it is
called for sessions at `storage_writer.inc:1023` and at `:1166` (repair). The suffix is therefore
part of the stem and the regex rejects it.

Tested with `node -e` on the owner's real names (FACT):

```
"GERMANY 2002 - Race - 2026-10-13 19-12_2"                 NO MATCH
"AUSTRALIA 2002 - CO-OP - Practice 1 - 2026-10-13 17-20_2" NO MATCH
"GERMANY 2002 - Practice 1 - 2026-10-13 19-13_2"           NO MATCH
"GERMANY 2002 - Race - 2026-10-13 19-14"                   OK  mode=Race session=''
"GERMANY 2002 - CO-OP - 2026-10-13 18-28"                  OK  mode=CO-OP session=''
"GERMANY 2002 - 2026-10-13 19-14"                          NO MATCH
```

Disk evidence (FACT, names only, `Documents\...\recordings`): every one of the first 40 session
files listed ends in `_2` or `_3`, for example
`GERMANY 2002 - 2026-10-13 19-12/GERMANY 2002 - Race - 2026-10-13 19-12_2.f1t`.
Why the first file in a folder gets `_2` is OPEN. It may be a stem left by an earlier attempt in
the same minute. A live check of one folder is needed; the DLL is not changed by this proposal.

Consequence (FACT from the code): `sessionLabel` (`telemetry_viewer.js:4445-4448`) returns `''`
when `parseRecName` is null, and the row falls back to the word "Session" at `:5627`:

```js
var key = r.id + '/' + s.id, open = s.state === 'open', b = stateBadge(s.state), lab = sessionLabel(s.id) || 'Session';
```

and `recTitle` (`:4440-4443`) returns the raw name when the parse fails, which is the source of
raw group titles such as `GERMANY 2002 - 2026-10-13 19-14`.

### Root cause B (FACT on disk, INFERENCE on age): mode-only and mode-less names

Two older naming shapes exist in the owner's folders:

1. Weekend folder with no mode, e.g. `GERMANY 2002 - 2026-10-13 19-14`. Written when the game
   mode was not in the name (see the batch 5 naming decision, `STORAGE_V2_BATCH5_RECORD_2026-10-13.md`
   section 2, Decision A, which added the mode). Folders created before that build keep the old name.
2. Session with mode but no session part, e.g. `GERMANY 2002 - Race - 2026-10-13 18-28_2.f1t`.
   The current writer always adds a session part when the game mode is not Test Day
   (`storage_writer.inc:991`: `has_session = e->game_mode != 1 && session[0] != 0`), so this
   shape is from an older build (INFERENCE). For such a name the regex reads `Race` as the mode
   and returns `session = ''`, so the label is empty. No session type is left in the name.

For these files the only truth is the session type stored in the file itself. The writer already
stores it in the META block: `storage_writer.inc:796` writes `"session_type_name"` next to
`session_type` (`:796-800`, name from `sw_type_name` at `:767`). The recordings list does not
expose it yet (`STORAGE_V2_BATCH4_RECORD_2026-10-13.md` line 117: "Not shown yet: session
`type_name`"). The list builder is in `storage_api.inc` (session list built from distinct stems,
about line 518).

### Root cause C (FACT, related): the secondary `sty` map is wrong (dead code)

`telemetry_viewer.js:1169` (in `Viewer.prototype.refreshChrome`, line 1164) maps
`{1:'Practice', 2:'Qualifying', 4:'Race', 5:'Race'}`. That disagrees with the live map at
`:2354` (`2` is Practice 2, `5` is Qualifying). README says `/viewer` is compiled out in 4.4.0,
so the impact is low. Fix it with the rest or delete the dead function later (owner decision).

### Proposed diff (NOT applied)

```diff
--- telemetry_viewer.js parseRecName (line 4420)
-        var m = /^(.+?) (\d{4}) - (.+?) - (?:(.+?) - )?(\d{4})-(\d{2})-(\d{2}) (\d{2})-(\d{2})$/.exec(String(name || ''));
+        var m = /^(.+?) (\d{4}) - (.+?) - (?:(.+?) - )?(\d{4})-(\d{2})-(\d{2}) (\d{2})-(\d{2})(?:_\d+)?$/.exec(String(name || ''));
+        if (!m) {   /* weekend folder written with no mode (older build) */
+            var w = /^(.+?) (\d{4}) - (\d{4})-(\d{2})-(\d{2}) (\d{2})-(\d{2})(?:_\d+)?$/.exec(String(name || ''));
+            if (!w) return null;
+            return { country: w[1], year: w[2], mode: '', session: '', y: +w[3], mo: +w[4], d: +w[5], hh: w[6], mi: w[7] };
+        }
--- telemetry_viewer.js recParts (line 4437)
-        return { track: titleWords(p.country) + ' ' + p.year, mode: MODE_TEXT[p.mode] || p.mode, session: session,
+        return { track: titleWords(p.country) + ' ' + p.year, mode: MODE_TEXT[p.mode] || p.mode || 'mode not recorded', session: session,
--- telemetry_viewer.js recCardHtml (line 5627), use the file's own type when the name has none
-            key = r.id + '/' + s.id, ... lab = sessionLabel(s.id) || 'Session';
+            key = r.id + '/' + s.id, ... lab = (s.type_name || sessionLabel(s.id) || 'Session (type not in name)');
--- storage_api.inc (session list item, about line 518-560): add the META session type
+   "type_name": <session_type_name from the file META block, or "" when the file has none>
```

The `type_name` change is a DLL change: it touches `storage_api.inc` and needs the API contract
note (`API_CONTRACT_2026-10-09.md`) and the payload re-pin (`pin_sidecars.py --payload`).
The JS fallback chain above works without it for the new names.

Name trace for the owner's folders with the JS fix only:

| Name on disk | Today | After JS fix | After DLL `type_name` |
|---|---|---|---|
| `GERMANY 2002 - CO-OP - Practice 1 - 2026-10-13 18-28_2` | Session | Practice 1 | Practice 1 |
| `GERMANY 2002 - Race - 2026-10-13 19-12_2` (mode-only) | Session | Session (type not in name) | from META |
| `GERMANY 2002 - 2026-10-13 19-14` (weekend) | raw text | Germany 2002 - mode not recorded - Mon... | same |

### Test that should cover it

- `viewer_test.js:965-969` already tests `sessionLabel`. Add: `sessionLabel('GERMANY 2002 - CO-OP - Practice 1 - 2026-10-13 17-20_2') === 'Practice 1'`, and `sessionLabel('GERMANY 2002 - Race - 2026-10-13 19-14') === ''` (mode-only, must not invent a label).
- `viewer_test.js:960-964` covers `recTitle`. Add: `recTitle('GERMANY 2002 - 2026-10-13 19-14')` no longer equals its raw input, and reads `Germany 2002 - mode not recorded - Tue 13 Oct, 19:14`.
- Add one real-name fixture list (the 8 names above) to `viewer_test.js` so the regex cannot regress on `_N`.
- DLL side (if `type_name` is added): a case in `runtime_test.c` near the session-stem checks (`runtime_test.c:2599-2620`) and a `test_telemetry.py` contract assertion on the recordings list field.

## 4. B5: live header shows `2002_Silverstone`

### Root cause (FACT)

The header uses the raw `trk` string from the live JSON:

```
telemetry_viewer.js:4061  esc(j.trk || 'unknown track')
telemetry_viewer.js:4023  txt = liveLineText({ running: running, track: j && j.trk, ... })
telemetry_viewer.js:4516  return 'Live: ' + (st.track || 'unknown track') + ...
```

`trk` is set in the DLL from `g_telemetry.track_name` (`web_telemetry.c:2406-2407`, JSON key
`trk` at `:5895`). `track_name` is the base name of the AIW folder, `2002_Silverstone`
(`web_telemetry.c:4380-4385`). No display name is sent anywhere.

### Track display-name data that IS available (FACT)

- Full AIW path, live, in the DLL: `g_telemetry.aiw_path` (`web_telemetry.c:1051`, set at `:4372`
  from the game path, relative from `SEASONDATA\...`). Example in the file comment
  `web_telemetry.c:375`: `SEASONDATA\CIRCUITS\Italy\2001_Monza\2001_Monza.AIW`. The country is
  the folder after `CIRCUITS\`.
- The country and year logic already exists in `storage_v2.inc`: `sv2_country_from_aiw`
  (`storage_v2.inc:1060`, aliases Adelaide, Aida, Estoril, Jerez; `_` to space; upper case) and
  `sv2_aiw_parse` (used by the writer at `storage_writer.inc:1006`).
- The JS has no track-to-country table (grep: no match for Monza/Hockenheim/Silverstone in
  `telemetry_viewer.js` or `workbench_ui.js` beyond a comment at `:2709`). The viewer cannot
  build "Great Britain" alone without a new table.

### Proposed diff (NOT applied)

DLL (`web_telemetry.c`, in the live JSON builder next to `"trk"` at about `:5895`): add a display
name `trknm` built from `aiw_path` with the same rules as the writer:
`"<Title Case country> <year> - <Track>"`, for example `Great Britain 2002 - Silverstone`. Empty
when the AIW is not read. Build it with `sv2_country_from_aiw` and `sv2_aiw_parse`, which are
defined in `storage_v2.inc`; check the include order against the JSON builder (the writer includes
`storage_v2.inc` before `storage_writer.inc`, `web_telemetry.c` must do the same before `:2406`).

JS:

```diff
--- telemetry_viewer.js (refreshChrome header, 4061)
-                '<span><b>' + esc(j.trk || 'unknown track') + '</b></span><span>' + esc(sessName(j.ty)) + ...
+                '<span><b>' + esc(j.trknm || j.trk || 'unknown track') + '</b></span><span>' + esc(sessName(j.ty)) + ...
--- telemetry_viewer.js liveLineText (4516) and call site (4023)
-        return 'Live: ' + (st.track || 'unknown track') + ...
+        return 'Live: ' + (st.track || 'unknown track') + ...   /* call site passes track: j && (j.trknm || j.trk) */
-            txt = liveLineText({ running: running, track: j && j.trk, ...
+            txt = liveLineText({ running: running, track: j && (j.trknm || j.trk), ...
```

Target text: `Great Britain 2002 - Silverstone - Practice 1` (the header keeps `sessName` for the last
part, so Practice 1 comes from `ty = 1`).

### Test that should cover it

- DLL: `runtime_test.c` case: `sv2_country_from_aiw("SEASONDATA\\CIRCUITS\\Great Britain\\2002_Silverstone\\2002_Silverstone.AIW")` gives `GREAT BRITAIN`, and the title-case display name is `Great Britain 2002 - Silverstone`. Empty `aiw_path` gives no name.
- JS: `viewer_test.js` helper (new, pure) `headerTrackText(j)` returns `Great Britain 2002 - Silverstone` when `trknm` is set and `2002_Silverstone` (raw) when not, never empty.
- Contract: `test_telemetry.py` or `contract_validate.py` for the new live field (the live JSON is an API surface; `API_CONTRACT_2026-10-09.md` must note it).

## 5. B7: tyre temps say "not recorded per sample yet" for the live car

### Root cause (FACT)

Graph channel catalog, `workbench_ui.js:877-884`:

```js
var w = /^(fl|fr|rl|rr) (.+)$/.exec(n);
if (w && w[2] === 'tyre wear') {
    ...   /* only tyre wear is wired */
}
if (/tyre .*temp/.test(n))
    return none(n, 'Tyre temperatures are not recorded per sample yet.');   /* workbench_ui.js:883-884 */
```

The channel names come from the layout files (`workbench_ui.js:741`, for example `fl tyre crown temp`,
`fl tyre camber temp`, `fl tyre average temp`). Every temp channel falls into the `none()` branch
because no branch returns a source for temps.

The data IS in the live sample. Per wheel, one temp per wheel, FL FR RL RR:

- `workbench_ui.js:322` (live car): `ttemp: ((car.vf6 || 0) & (VF6.TTEMP | VF6.TTEMP_RX)) ? quad(car.ttc) : null`
- `workbench_ui.js:471` (recorded samples): `ttemp: (f & TRF.TTEMP) ? quad(s.tt) : null`
- The Tyre panel already draws it: `telemetry_viewer.js:1799`, `:1813-1816`, and the live JSON
  per-zone `ttmp` at `:1872`.

So the message is wrong for the live car. The condition that prints it is the regex at `:883`,
which runs only for channels that are not wired, and wires no temp channel.

Note: `ttemp` is ONE value per wheel. The three zones (crown, camber, inner/outer) are only in the
focus-car `ttmp` (`telemetry_viewer.js:1872`, `:4935`). Per-zone channels therefore cannot be
drawn from the per-sample series; they stay honest "not in this feed" items.

### Proposed diff (NOT applied)

```diff
--- workbench_ui.js (after the 'tyre wear' branch, before line 883)
+        var tt = /^(fl|fr|rl|rr) tyre (average|crown|camber) temp$/.exec(n);
+        if (tt) {
+            var ti = WHEELS.indexOf(tt[1]);
+            if (tt[2] === 'average')
+                return src(tt[1].toUpperCase() + ' tyre average temp', 'C', function (p) {
+                    return p.ttemp && Number.isFinite(p.ttemp[ti]) ? p.ttemp[ti] : null;
+                }, { emptyWhy: 'Tyre temperature is not in this sample (no valid temperature bit).' });
+            return none(n, 'Crown and camber are per-zone temperatures. The live feed has one value per wheel (use average).');
+        }
         if (/tyre .*temp/.test(n))
             return none(n, 'Tyre temperatures are not recorded per sample yet.');
```

### Test that should cover it

- `viewer_test.js` line 139 (`live tyre temps`) covers the data path only. Add a catalog test:
  the source for `fl tyre average temp` on a point with `ttemp: [80,81,82,83]` returns `80`, and the
  same for `rr` returns `83`; a point with `ttemp: null` returns `null` with the emptyWhy text.
- The catalog function is not exported today. Export it (or a `channelSource(name)` helper) for the test.
- `audit_regression_test.js` / `viewer_canvas_test.js` may already load the catalog; check before adding.

## 6. B9: status bar shows `PAST  Live values are hidden in past mode.` with no past session

### Root cause (FACT)

`goPast()` (`telemetry_viewer.js:3991-3996`) sets the source to past and then applies the mode,
without a recording chosen:

```js
Console.prototype.goPast = function () {
    if (this.review) return;
    this.source = 'past';                               /* :3993 */
    if (!this.past) { this.setTopTab('tel'); this.setPage('sessions'); }
    this.applyMode();
};
```

`pastNoData()` (`:3971`) is true for any past source that is not a review:

```js
Console.prototype.pastNoData = function () { return this.isPast() && !this.review; };
```

`refreshChrome` (`:4057`) then prints the notice whenever `nodata` is true:

```js
if (nodata) this.sessEl.innerHTML = '<span class="tv-live stale">PAST</span><span>Live values are hidden in past mode.</span>';
```

The correct text already exists one function earlier, in `refreshSource` (`:4028`):
`'Past session: none open. Pick a recording on the Recordings page.'`. The status bar contradicts it.

Other transitions checked (FACT): `goLive` (`:3984-3988`) and `:5426` clear `this.past`. `enterPast`
(`:3976`) sets `this.past`. So the only path to "past with no recording" is `goPast` (and `goChange`
at `:3996`, which calls `setPage('sessions')` and does not set the source).

### Proposed diff (NOT applied)

```diff
--- telemetry_viewer.js refreshChrome (4057)
-        if (nodata) this.sessEl.innerHTML = '<span class="tv-live stale">PAST</span><span>Live values are hidden in past mode.</span>';
+        if (past && !this.review && !this.past)
+            this.sessEl.innerHTML = '<span class="tv-live stale">PAST</span><span>No past session open. Pick a recording on the Recordings page.</span>';
+        else if (nodata) this.sessEl.innerHTML = '<span class="tv-live stale">PAST</span><span>Live values are hidden in past mode.</span>';
```

`pastNoData()` is left as it is, because `:3836` uses it to decide the page notice. Changing it
would change that page behaviour too. Keep the change in the header only. The `else if (j)` branch
(`:4059`) draws live values while in past mode with nothing open; with the change above, the first
branch wins, so live values are not drawn in that state.

### Test that should cover it

- `viewer_test.js:999` checks only that the text `'PAST SESSION'` exists in the source. Add a pure
  helper `statusNoticeText(past, review, hasPast)` and assert: `(true,false,false)` gives
  `No past session open...`, `(true,false,true)` gives `Live values are hidden in past mode.`,
  `(false,false,false)` gives no notice.
- Add a `goPast()` case: after `goPast()` with `this.past == null`, `refreshChrome` output contains
  `No past session open` and not `Live values are hidden`.

## 7. Test commands

Fast loop (no game, no build, runs in seconds; the viewer suite is the spec rule):

```
cd "%USERPROFILE%/Desktop/F1/f1c_ida_decompiled/mods/sdk_sandbox/web_telemetry"
node viewer_test.js telemetry_viewer.js workbench_ui.js
```

Baseline before this task: `VIEWER PASS 525 checks` (`TELEMETRY_B2_RECORD_2026-10-14.md` section 3).

Name regex check with real names (no file written):

```
node -e 'const re=/^(.+?) (\d{4}) - (.+?) - (?:(.+?) - )?(\d{4})-(\d{2})-(\d{2}) (\d{2})-(\d{2})(?:_\d+)?$/; console.log(re.test("GERMANY 2002 - Race - 2026-10-13 19-12_2"))'
```

DLL side (after the DLL change only, owner gate, game closed):

```
gcc -O1 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -o %TEMP%\h5\runtime_test.exe runtime_test.c   (build artefacts only in %TEMP%)
```

Full gate (owner-run, `build.bat` with `TELEMETRY_TESTS=1`, game closed): runs `test_telemetry.py`,
`runtime_test`, and `viewer_test.js`. The owner has to run it, per the no-gates-while-driving rule.

## 8. Open items and risks

- OPEN, why the first session file in most folders gets `_2`: check one folder live; compare its
  `.f1t` names with the `sv2_unique_stem` path (`storage_writer.inc:1023`).
- OPEN, the INT_MAX value in practice: confirm with one read of `0x008ACF8C` (read-only, game
  running, sim advancing, per the check-pause rule).
- OPEN, the age of the mode-only and mode-less names: confirm by the file's META `session_type_name`
  and the build date in the file header, before the JS fallback text is final.
- RISK, DLL change for B5 and `type_name` (B2): needs the payload re-pin, the API contract note, and
  a Lightloader/owner install decision. JS-only fixes (B1, B2 regex, B7, B9) do not.
- RISK, the `Session (type not in name)` text is an honest fallback. The owner may prefer a different word.

## 9. Changed files and state

- Changed: `RESEARCH_H5_BBUGS_2026-10-14.md` (this file, new). Nothing else in the component.
- Not changed: every `.js`, `.c`, `.inc`, `.h`, `.json`, `.ini`, test, manifest, backup, and
  `CURRENT_STATE.md`. No build, no game, no deploy. Not indexed in `CURRENT_STATE.md` (the task
  says to edit nothing else; the index line is for the owner to add or to approve).
- Restoration: delete is not needed; this file can be moved to a quarantine folder by the owner
  (never hard-deleted by an agent).

## 10. Documentation gate

- Durable record: this file, in the component.
- Indexed: NO (pending owner approval; see section 9).
- Fact vs inference: tagged in every section (FACT / INFERENCE / OPEN).
- Pending validation: the INT_MAX read (B1), the `_2` cause (B2), the age of old names (B2), the
  viewer test additions (section 7), and the owner gate.
- No secrets, no `api1.txt`, no private keys, no raw recording content. Only folder and file names
  were listed (owner's own recordings folder).
