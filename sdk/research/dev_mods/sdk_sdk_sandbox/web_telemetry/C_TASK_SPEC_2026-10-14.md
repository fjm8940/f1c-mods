# C task spec: community feedback C1/C2/C3 in the viewer (Haiku implements, design by Claude), 2026-10-14

Author: Matias. Research: `RESEARCH_C1C2C3_COMMUNITY_2026-10-14.md` (line numbers there may have drifted; locate by code text).
Files: ONLY `telemetry_viewer.js`, `workbench_ui.js`, `viewer_test.js`. No .c/.inc/.py/.json edits (another agent edits the DLL now).
Before editing: copy the three files to `%TEMP%\C_before_2026-10-14\`. Edit with the Edit tool only (no PowerShell Set-Content).
Test command (bash, component dir): `node viewer_test.js telemetry_viewer.js workbench_ui.js` must print `VIEWER PASS <n>` with n > 580; also `node --check` both files.
You do NOT design UI: use the exact markup and CSS below. Never use the word "Matias". Author Matias.

## C1 official lap time in exports
- As research C1.3 item 1: when a lap completes in `workbench_ui.js` (`completeLap`), keep it pending until the car's frame `ll` (last lap) changes or 8 s pass; store `official_time` (seconds) on the lap; `null` if it never arrives.
- `exportDoc` (`telemetry_viewer.js`): `lap_time = official_time ?? derived`, add `lap_time_src: "game"|"derived"`, `derived_lap_time`; `lap_time: null` when the lap is not complete. `driver` = clean driver name (`cleanName(n)`), add `team` (C3 helper) and `car` (raw `cn`).
- Tests: pure helper `officialLapPick(derived, official)` exported; assertions for both sources and the null case.

## C3 Team column
- Helper next to `cleanName`: `function teamOf(c) { return String((c && (c.tn || c.cn)) || '').replace(/^\d{4}\s+/, '').trim(); }` exported as `teamOf`.
- Add a `Team` column right after `Driver` in: Timing Tower (both full and short `towerPane` column lists and row cells), TV Director drivers table (`tvdDrivers` gets `team: teamOf(c)`; header `Team` after `Driver` in `tvRenderDrivers`).
- Cell markup: `<td class="team">` + esc(team) + `</td>`. CSS (add to the console CSS list next to the tower styles): `'.f1x td.team,.tvd-table td.team{color:#9fb2e6;max-width:120px;overflow:hidden;text-overflow:ellipsis}'`.
- Tests: `teamOf({cn:'2002 Ferrari'}) === 'Ferrari'`, `teamOf({tn:'McLaren', cn:'1995 McLaren'}) === 'McLaren'`, `teamOf(null) === ''`; tower header array contains 'Team' right after 'Driver'.

## C2 Export laps dialog (all laps of chosen drivers or a team)
- The top-bar `Export` button opens this dialog (the old one-click export of A/B stays as the dialog's "Shown laps (A/B)" option).
- Data: research C2.3/C2.4. `GET /trace/status` (add `?session=<id>` when a past 1.0.1 review session is open, as the existing code does for other routes), then per lap `GET /trace/data?car=<numeric slot>&lap=<raw lap>` JSON, ONE REQUEST AT A TIME (the DLL has one shared HTTP buffer). Official times: research C2.4 join rule with the 0.25 s check.
- Output formats:
  - `JSON` = one file `{"schema":"f1c.lapset/1","source","exported","laps":[<f1c.trace/1 doc + lap_time, lap_time_src, team>...]}`.
  - `CSV (one row per sample)` = columns exactly research C2.5 list, one header line.
  - File name `f1c_laps_<live|session>_<driver-or-team>_<YYYYMMDD-HHMM>.<json|csv>` (spaces -> `_`).
- Lap filter: "All completed laps" (flags & 1) or "Best lap per driver"; checkbox "Include pit laps" (flags & 2, default off).
- Errors: a lap answering 404/409 is listed in the dialog's result list as `Lap N: not recorded` / `evicted`; never dropped silently.
- Exact markup (build with the file's `el()` helper; ids not needed):
```
<div class="f1x-modal">                       (fixed overlay, click on it outside the box closes)
  <div class="f1x-dlg" role="dialog" aria-label="Export laps">
    <div class="f1x-dlg-h"><b>Export laps</b><button class="f1x-dlg-x" title="Close">×</button></div>
    <div class="f1x-dlg-b">
      <div class="f1x-dlg-row"><label>What</label>
        <select>  options: "Shown laps (A/B)", "All laps of chosen drivers", "All laps of a team"  </select></div>
      <div class="f1x-dlg-row f1x-dlg-pick"><label>Drivers</label><div class="f1x-dlg-list">  one <label><input type=checkbox> P<pos> <name> <span>team</span></label> per car </div></div>
      <div class="f1x-dlg-row f1x-dlg-team"><label>Team</label><select> one option per distinct teamOf() </select></div>
      <div class="f1x-dlg-row"><label>Laps</label><select> "All completed laps", "Best lap per driver" </select>
           <label class="chk"><input type=checkbox> Include pit laps</label></div>
      <div class="f1x-dlg-row"><label>Format</label><select> "CSV (one row per sample)", "JSON (f1c.lapset/1)" </select></div>
      <div class="f1x-dlg-msg"></div>          (progress "Lap 3 of 41..." and the per-lap problems list)
    </div>
    <div class="f1x-dlg-f"><button class="tv-btn">Cancel</button><button class="tv-btn primary">Export</button></div>
  </div>
</div>
```
  Show only the row that matches "What" (pick list for drivers, team select for team; neither for shown laps).
- Exact CSS (add to the console CSS list):
```
'.f1x-modal{position:fixed;inset:0;z-index:9000;background:rgba(0,0,0,.6);display:flex;align-items:center;justify-content:center}',
'.f1x-dlg{width:560px;max-width:94vw;max-height:86vh;display:flex;flex-direction:column;background:#040b26;border:1px solid #38d6e8;border-radius:6px;box-shadow:0 12px 40px rgba(0,0,0,.7);font:12px var(--sans);color:#dfe7ff}',
'.f1x-dlg-h{display:flex;align-items:center;justify-content:space-between;padding:10px 14px;border-bottom:1px solid #14307f;font-size:13px}',
'.f1x-dlg-x{border:0;background:transparent;color:#9fb2e6;font-size:18px;cursor:pointer;line-height:1}',
'.f1x-dlg-b{padding:12px 14px;overflow:auto;display:flex;flex-direction:column;gap:10px}',
'.f1x-dlg-row{display:flex;align-items:flex-start;gap:10px}',
'.f1x-dlg-row>label:first-child{width:70px;flex:none;color:#9fb2e6;padding-top:3px}',
'.f1x-dlg-row label.chk{display:inline-flex;align-items:center;gap:5px;color:#dfe7ff;margin-left:10px}',
'.f1x-dlg-list{flex:1;max-height:220px;overflow:auto;border:1px solid #14307f;border-radius:3px;padding:4px 6px;display:grid;grid-template-columns:1fr 1fr;gap:2px 10px}',
'.f1x-dlg-list label{display:flex;align-items:center;gap:6px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;cursor:pointer}',
'.f1x-dlg-list span{color:#5f7fc4}',
'.f1x-dlg-msg{min-height:18px;color:#9fb2e6;white-space:pre-wrap}',
'.f1x-dlg-f{display:flex;justify-content:flex-end;gap:8px;padding:10px 14px;border-top:1px solid #14307f}',
'.f1x-dlg .tv-btn{height:26px;padding:0 14px;border:1px solid #2a43a8;border-radius:3px;background:#0b1a5e;color:#dfe7ff;cursor:pointer}',
'.f1x-dlg .tv-btn.primary{background:#e10600;border-color:#ff4a3d;color:#fff}'
```
- Tests: pure helpers exported and asserted: `exportFileName(src, who, date)`, `lapsetCsvHeader()` (exact column list), `pickLaps(laps, mode, includePit)` (best per driver / all completed / pit filter), `joinOfficial(traceLaps, officialTimes)` (0.25 s rule).

## Record
`C_RECORD_2026-10-14.md` in the component: changes with file:line, pasted test output, before-copy path, deviations. Stop and report after 3 failed fix attempts. Final reply under 15 lines.
