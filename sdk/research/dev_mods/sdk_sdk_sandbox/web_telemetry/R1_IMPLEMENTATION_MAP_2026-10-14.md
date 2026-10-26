# Recording review R1: implementation map (READ-ONLY survey), 2026-10-14

Product: Telemetry (`web_telemetry`), shipped 1.2.0, API still 9 (`web_telemetry.c:56`, `schemas/version.json:2`).
Pipeline stage of this record: **investigate** (feeds the R1 implementation). Author: Matias.
Input spec: `RECORDING_REVIEW_SPEC_2026-10-13.md` (read in full, 456 lines).
Scope rules: no edits outside this file; no build, no compile, no game, no EXE; no changes to CURRENT_STATE.md.
Tags: FACT = read in the source at the line given (2026-10-14). INFERENCE = reasoned, to verify in R1. OPEN = owner or spec decision.

---

## 0. Read first

**Next action:** amend `RECORDING_REVIEW_SPEC_2026-10-13.md` sections 2.3, 5.2, 5.4, AR-1, and the R1 file list (section 10) before writing code. The spec was written before the storage v2 B1 and B2 batches (2026-10-13 and 2026-10-14). Several of its premises are now false (section 5 below). The existing code already does part of R1 (lap index, lap channel, pit and incident events).

**Main findings**
1. The v2 writer already stores lap number (ch 69), position (68), pit state (70), track limits (73), contact (101) and EVENT records `lap`, `pit`, `incident` (`storage_writer.inc:263`, `:917`). Spec F-V2-2 and section 14 say the opposite.
2. A lap index already exists in the reader: `sv2_index_build` (`storage_v2.inc:1349`), `sv2_idx_read_block` (`:1418`), `sv2_idx_span` (`:1434`), `sv2_idx_lap_time` (`:1453`). It is tested offline by `tests/storage_b2_index_test.c` (126 checks, B2 record). It has **no caller** outside that test. The HTTP route and the job are not built (B2 record, table rows "DEFERRED").
3. The existing lap rule (channel 69 change plus EVENT `last_lap_s`, sim time) differs from spec 5.2 (distance-crossing plus `tick_ms`). Use the B2 rule. Spec AR-1 says `tick_ms`; `tick_ms` is wall clock (`sv2_catalog` id 0 and DATA `dt_ms`), so AR-1 must use sim time (`sim_ms`, `sv2_data_t` `storage_v2.inc:533`).
4. Both reader entry points take a whole-file buffer (`sv2_index_build(buf,len,...)`, `sv2_idx_read_block(buf,len,...)`). AR-4 and T4 (no whole-file buffer on review) are not met by the existing code. A file-backed frame read is needed (section 1.6).
5. `lap_len` is not in the writer. It is available on the capture thread at OPEN and CLOSE under `g_telemetry_cs` (sections 1.7 and 5.7). The spec's open question R-1 is answered: yes.
6. Shape conflicts with the schemas: the trace sample keys in spec 5.3 are wrong (the schema uses 19 short keys), `trace_data.json` caps samples at 2400 while the builder uses 3200, and `source` has no `recording` value. Several schemas are `additionalProperties: false`.
7. Review routes are not local-only today. The 1.0.1 review guard (`web_telemetry.c:9136-9145`) has no peer check. Spec S-R2 (local only) needs a new check, not a reuse.
8. Line drift is widespread (section 6). Every spec citation below was re-checked.

**Decisions this map needs from the spec owner** (details in section 7): D1 lap rule (use B2), D2 trace sample cap (2400 vs 3200), D3 version number (1.2.x or 1.3.0; spec says 1.1.0), D4 review peer policy (local only, add check), D5 whether the review index is a new job kind or shares the `sj_run` dispatch.

---

## 1. Code anchors (user item 1)

### 1.1 Review dispatcher (`?session=`)
- FACT `web_telemetry.c:9136-9145`. Review guard, runs for every non-POST request (also `/`, `/recordings*`, `/events`):
  `if (!is_post) { char review_id[AR_ID_LEN + 8]; if (qs_get(query,"session",...)) { if (!ar_id_valid(review_id) || !ar_session_exists(review_id)) { 404 "unknown or deleted session"; } if (http_archive_route(client,path,query,review_id)) return; } }`
  A v2 id fails `ar_session_exists` and gets 404 today (`:9139-9141`). The v2 branch must go **before** `:9139`.
- FACT `web_telemetry.c:9090-9091`. Query split: `query = strchr(path,'?')`, empty string when absent.
- FACT `web_telemetry.c:9083-9088`. Method gate: non-GET/POST gets 405. HEAD is treated as GET only for the 405 text.
- FACT `web_telemetry.c:9098-9109`. `GET /recordings` and `GET /recordings/jobs` are served before the review guard (they do not carry `session=` in practice). Insert `GET /recordings/review` here, and make sure the guard at `:9137` does not 404 it.
- FACT `web_telemetry.c:8809-8863` `http_archive_route(SOCKET, path, query, id)`:
  - raw documents: `/telemetry`→`frame.json` (`:8813`), `/weather`→`weather.json` (`:8814`), `/track`→`track.json` (`:8815`). These write `404 "not recorded for this session"` when missing (`:8822`).
  - `/laps`→`ar_build_laps_json` (`:8830`), `/pits` (`:8831`), `/incidents` (`:8832`), `/trace/status` and `/trace/select`→`ar_build_trace_status_json` (`:8833-8834`).
  - `/driver?id=N`→`ar_build_driver_json` (`:8835-8840`).
  - `/trace/data?car=<1000+slot>&lap=N`→`ar_build_trace_data_json(id, which-1000, lap, ...)` (`:8841-8849`).
  - `/trace/open`, `/trace/compare`, `/trace/export` → 404 "not available for an archived session" (`:8850-8851`, `:8858`).
  - any other path returns 0 (`:8852-8854`), so `/` falls through to the page.
  - Error mapping: len -2 → 400 (`:8857`), len <0 → 404 "lap not recorded in this session" (`:8859`).
  - The whole body runs under `g_ar_lock` (`:8819`, `:8828`, `:8856`). The response buffer is `g_http_big` (`:8810`).
- FACT `web_telemetry.c:8018` `static char g_http_big[TRACE_JSON_BYTES]`; `TRACE_JSON_BYTES` is `2097152` at `:3233`.
- FACT `web_telemetry.c:8134` `qs_get`. `:8767` `http_peer_is_local`. `:8795-8797` `http_request_is_local` (peer and proxy-header check). `:8799` `http_send_json_error`.
- FACT `web_telemetry.c:9117`. POST routes check `http_request_is_local` (the recordings writes). The GET review branch has **no** peer check.

### 1.2 Session id regex
- FACT server, inline page script `web_telemetry.c:8281`: `var m=/[?&]session=([A-Za-z0-9_-]{8,47})(?:&|$)/` (the `/` page; passes `review:S`, `:8283`; the first `/telemetry` fetch with `Q` is at `:8285`).
- FACT viewer `telemetry_viewer.js:3000`: `/^[A-Za-z0-9_-]{8,47}$/.test(opts.review)`.
- FACT server `session_archive.inc:135-145` `ar_id_valid`: length 8 to 47 (`n >= AR_ID_LEN` refuses; `AR_ID_LEN` 48 at `:33`), characters `[0-9A-Za-z_-]`.
- A `v2_` + 16 hex id (19 chars) passes all three. No regex change is needed (spec 5.1 is right on this point).
- The v2 id must NOT go through `ar_id_valid` / `ar_session_exists` (they build paths under `g_ar_root`). The lookup goes through the published snapshot (section 1.3).

### 1.3 Snapshot (`sa_snapshot_rebuild`, `sa_pub`, `sa_cs`)
- FACT `storage_api.inc:75-82`: `sa_build` (writer thread only, `:75`), `sa_pub` (published copy, `:76`), `sa_pub_len` (`:77`), `sa_pub_over` (`:78`), `sa_pub_open[160]` (`:79`), `sa_pub_alg` (`:80`), `CRITICAL_SECTION sa_cs` (`:81`), `sa_cs_ready` (`:82`).
- FACT `storage_api.inc:149-177` `sa_session_json`: writes `{"id": <stem>, "state": open|recovered|closed|archived, "archived", "bytes", "ticks", "files":[...]}`. Stem is `sa_st[j]`. **Add `review_id` here** (spec 5.1 wants it on each session). State derivation: open if any `SA_K_PART` (`:156`), recovered if the work file has `SW_ST_RECOVERED` (`:157`), closed if a work file exists (`:158`).
- FACT `storage_api.inc:187-270` `sa_snapshot_rebuild`: classify (`:194-195`), size text and notice (`:197-199`), per-folder JSON, `sa_build` filled with `sa_f`, then publish under `sa_cs` at `:263-268` (`memcpy(sa_pub, sa_build, b.n)`). Early return on size overflow `:262` keeps the old copy.
- FACT `storage_api.inc:281-310` `sa_api_recordings`: copies `sa_pub` under `sa_cs` (`:286-302`). This is the only reader of the snapshot. The review lookup should use the same pattern: a small table `{review_id, folder, stem}` built in the snapshot, copied under `sa_cs`.
- FACT `storage_writer.inc:526-532`: `sw_index_write` calls `sa_snapshot_rebuild()` at `:532`. Snapshot is rebuilt on each index write (at most 5 s, `SW_INDEX_MS` `storage_writer.inc:22`, check at `:1260`).
- FACT `storage_api.inc:441-449` `sa_name_ok`: names up to 119 bytes, no `..`, no `\/:*?"<>|`, no bytes `>= 0x80`. The v2 stems contain spaces and `-`, so they pass `sa_name_ok` (they are file-system names). They are never sent to the browser as review ids.

### 1.4 Jobs thread
- FACT `storage_jobs.inc:18-21` defines: `SJ_QUEUE_N 8u`, `SJ_MAX_PATHS 64u`, `SJ_JOB_ARCHIVE 1`, `SJ_JOB_DELETE 2`.
- FACT `storage_jobs.inc:49-58` `sj_job_t` (`used, kind, state, result, recycle_working, seq, n, ..., folder[64][120], file[64][180], msg`). State 0 queued, 1 running, 2 done. `sj_jobs[SJ_QUEUE_N]` at `:60`.
- FACT `storage_jobs.inc:610-637` `sj_submit(kind, n, folders, files, expect, recycle, &seq)`: finds a free slot, or reuses the oldest done slot (`:615-620`), then `SetEvent(sj_wake)` (`:636`).
- FACT `storage_jobs.inc:674-711` `sj_run(idx)`: dispatch `if (j->kind == SJ_JOB_ARCHIVE) sj_run_archive(...)` (`:683-685`), `else if (j->kind == SJ_JOB_DELETE)` (`:685`), then `state = 2` and `sw_rebuild_req` (`:704-708`).
- FACT `storage_jobs.inc:712-730` `sj_main`: runs `sj_repair_all()` once (`:715`), then loops: claim the first queued slot (`:719-723`), run it, else `WaitForSingleObject(sj_wake, 500)` (`:725`).
- FACT `storage_jobs.inc:90` `sj_read_file` reads a whole file (the archive path). The review index must not use it (section 1.6).
- INFERENCE for R1: a review index is a third job kind (`SJ_JOB_INDEX 3`, new `sj_run` branch, new result field). It runs on this thread, so no new thread. Use `sj_submit` (`:610`) with `n=1`; the result is read back through `sj_jobs[slot]` under `sj_cs`. `sj_submit` takes folders and files arrays, so the v2 path is `folder` plus `file` (the `.f1t`).
- FACT `storage_jobs.inc:729-745` `sj_start`/`sj_stop` (single thread, stop timeout). `storage_jobs.inc:18` `SJ_QUEUE_N` is 8, which is enough for one review index at a time (cap the review index to 2 cached entries per spec 6).

### 1.5 Locks (spec 7)
- FACT `compatibility_runtime.inc:37` `TryEnterCriticalSection(&g_telemetry_cs)` in the capture callback, `:205` `EnterCriticalSection`, `:237` `sw_capture_tick(GetTickCount())`, `:243` `sw_capture_idle`, `:245` `LeaveCriticalSection`. Capture (and the writer ring copy in `sw_emit`) runs under `g_telemetry_cs`.
- FACT `storage_writer.inc:308-344` `sw_emit` runs under that lock (called from `sw_capture_tick`, `:346-358`). So the capture thread may read `g_telemetry.lap_len` there (section 1.7).
- No review code takes `g_telemetry_cs` (spec R-T1 stays valid).

### 1.6 storage_v2.inc reader API
Layout and constants (all FACT):
- Magic and sizes: `SV2_MAGIC "F1CTEL02"` `:9`; `SV2_HEADER_BYTES 192` `:10`; `SV2_TABLE_MAX_CHANNELS 128` `:11`; `SV2_MAX_CARS 104` `:12`; `SV2_MAX_BLOCK_TICKS 255` `:13`; `SV2_MAX_PAYLOAD (1<<20)` `:14`; `SV2_META_MAX 16 KiB` `:15`; `SV2_BLOCK_MAGIC "BLK2"` `:16`; `SV2_BLOCK_HEADER_BYTES 40` `:17`; `SV2_BLOCK_DATA 1, META 2, END 3, GAP 4, EVENT 5` `:18-22`; `SV2_EVENT_MAX 512` `:23`; `SV2_BLOCK_FLAG_COMPRESSED 1` `:24`.
- Header flags: `SV2_MINOR 1` `:27`; `SV2_FLAG_HAS_SIM (1<<9)` `:28` (every DATA block carries `sim_ms`); ARCHIVED `:29`, CLOSED `:30`, COMPRESSED_BLOCKS `:31`, UNCOMPRESSED_FALLBACK `:32`, PLAYER_PCH `:33`, RECOVERED `:34`, LEGACY_IMPORT `:35`, TICK_GAPS `:36`.
- Compression: `SV2_COMP_NONE 0`, `SV2_COMP_XPRESS 3`, `SV2_COMP_LZMS 5` `:37-39`.
- Value types and scopes: `SV2_TYPE_Q16 0`, `RAW32 1`, `TICK_MS 2` `:40-42`; `SV2_SCOPE_PLAYER 1`, `AI 2`, `ALL 3` `:43-45`.
- Reason codes (spec "SV2_R_*, storage_v2.inc:42-48" is stale):
  - `SV2_OK 0` `:48`, `SV2_R_HEADER 1` `:49`, `SV2_R_TABLE 2` `:50`, `SV2_R_BLOCK_HEADER 3` `:51`, `SV2_R_BLOCK_CRC 4` `:52`, `SV2_R_BLOCK_DECODE 5` `:53`, `SV2_R_DECODER_UNAVAILABLE 6` `:54`, `SV2_R_VERSION 7` `:55`.
  - Index only: `SV2_IDX_R_CAPACITY 8` `:1203` (not in the `SV2_R_` list; the spec must name it).
- Header struct `sv2_header_t` `:396-403` (`flags, compression_id, block_ticks_max, block_ms_max, channel_table_offset, channel_table_bytes, start_unix_utc, tz_offset_min, session_type, broker_generation, channels_declared, track[65], session_name[61]`). Encode `:405`, decode `:435`. Header CRC is over bytes 0..187, stored at 188 (`:439`). Minor must equal 1 or the result is `SV2_R_VERSION` (`:440`).
- Block header struct `sv2_block_t` `:460-464` (`type, flags, seq, n_ticks, raw_len, stored_len, payload_crc, t_first`). Decode `sv2_block_hdr_decode` `:481-506`: magic at 0, `t_first` (session sim seconds), CRC over bytes 0..35 stored at 36 (`:485`), type range, flags, `raw_len`/`stored_len` ≤ `SV2_MAX_PAYLOAD` (`:499`).
- Channel table: `sv2_channel_t` `:307-312`; `sv2_catalog[]` `:171-302` (ids 0..124); `SV2_CATALOG_COUNT` `:304`; encode `sv2_table_encode` `:329`; decode `sv2_table_decode` `:352-393` (trailing CRC over the table, `:357`).
- DATA payload: `sv2_car_t` `:519-526` (slot, scope, first_tick, count, `mask_lo/hi`, `cmask_lo/hi`, `vals`); `sv2_data_t` `:528-535` (`n_ticks, n_cars, dt_ms[255]` (wall, `dt_ms[0]=0`), `sim_ms[255]` (session sim ms, `sim_ms[0]=0`), cars, decode pool). Encode `:569`, decode `sv2_data_decode` `:635-710`.
- Compression: `sv2_alg_available` `:759`, `sv2_compress_block` `:768`, `sv2_decompress_block(alg, in, n, out, cap, &out_n)` `:780-793`, `sv2_pack_payload` `:794`. Cabinet binding `:738-757` (cabinet.dll, optional).
- Whole-file scan: `sv2_scan_t` `:826-837`; `sv2_scan(buf, len, scratch, work, work_cap, out)` `:840-930` (validates header, table, every block; CRC `:874`; decompress `:881-894`; META `:900-906`; EVENT `:918`). Used by `storage_jobs.inc:200, 240, 261, 346, 350, 422` and by `runtime_test.c` (tests at `:2498-3489`).
- Per-block frame read: `sv2_frame_at(buf, len, pos, seq, comp_id, &b, &payload, &plen, work, work_cap)` `:1240-1265` (bounds, seq check `:1245`, CRC `:1249`, decompress `:1253`). This is the function a file-backed reader should reuse.
- Index: types `sv2_idx_block_t` `:1207-1210` (`file_offset, seq, ticks, t_first, t_last`; 32 bytes), `sv2_idx_lap_t` `:1212-1218` (`car, lap, end_sim, start_sim, game_time, has_start, has_game`; 40 bytes), `sv2_idx_car_t` `:1220`, `sv2_idx_t` `:1222-1231`. `SV2_IDX_LAP_TOL 0.002` `:1204`. `sv2_idx_init` `:1233`. `sv2_idx_lap_put` `:1266` (merge by car, lap, end time). `sv2_idx_apply_data` `:1288`. `sv2_json_num` `:1316`. `sv2_idx_apply_event` `:1332` (EVENT lap only). `sv2_index_build(buf,len,d,work,work_cap,ix)` `:1349-1417` (**whole buffer**; requires `SV2_FLAG_HAS_SIM`, `:1372`; walks blocks from `channel_table_offset + channel_table_bytes`, `:1361-1363`). `sv2_idx_read_block(buf,len,ix,i,d,work,work_cap)` `:1418-1432` (**whole buffer**; counts `blocks_read`). `sv2_idx_span(ix, t0, t1, &first)` `:1434-1444`. `sv2_idx_lap_find(ix, car, lap)` `:1445`. `sv2_idx_lap_time(r, &out, &from_game)` `:1453`.
- Lap channel: `SV2_CH_LAP 69` `:1205` (RAW32 integer; `storage_v2.inc:247` catalog entry).
- Callers of the index: none outside `storage_v2.inc` and `tests/storage_b2_index_test.c` (FACT, grep 2026-10-14).

Channel ids for the review adapter (catalog `storage_v2.inc:172-302`, writer `storage_writer.inc:209-268`):

| Spec use | Channel id, key, type, scope | Writer line |
|---|---|---|
| tick_ms (wall) | 0 `tick_ms` TICK_MS | `dt_ms` in DATA block |
| sim time (use this for laps) | `sim_ms` in DATA block (`:533`) and block `t_first` (`:463`) | writer `sw_block_add` `:827` |
| dist (lap distance, m) | 1 `dist` Q16 ALL (0..8000) | `:209` `SW_SET(1, c->lap_dist)` |
| speed | 2 Q16 ALL | `:215` (car_valid&1) |
| throttle | 3 Q16 ALL | (car_valid&2) |
| brake | 4 Q16 ALL | (car_valid&4) |
| steer | 5 Q16 **AI only** | `:218` |
| glat | 6 Q16 ALL | (car_valid&32) |
| glon (longitudinal) | 7 Q16 ALL | (car_valid&32) |
| gvert | 8 Q16 ALL | (car_valid&32) |
| yaw, yaw_rate | 9, 10 Q16 ALL | `:210-211` |
| tread fl, fr, rl, rr | 11-14 Q16 ALL | (car_valid&16) |
| rpm | 15 Q16 ALL | `:212` |
| gear | 16 RAW32 ALL | `:238` |
| tx, tz (track map) | 17, 18 Q16 ALL | `:213-214` |
| flags | 19 RAW32 ALL, **never written** | `:239-240` (comment) |
| ttemp fl..rr | 20-23 Q16 PLAYER | `:241` |
| position | 68 RAW32 ALL | `:263` |
| lap number | 69 RAW32 ALL | `:263` |
| pit state | 70 RAW32 ALL | `:263` |
| track limits | 73 RAW32 | `:264` |
| contact | 101 RAW32 | `:273` |
| last lap time (game) | 86 RAW32 f32 bits | `:268` (`SW_SET(86, c->last_lap)`) |

Note: the spec's "channels per page" (5.6) is still right in kind. Channel 19 is not stored, so `f` bit validity must be synthesized (section 3.5).

### 1.7 META writer and `lap_len`
- FACT `storage_writer.inc:766` section header; `sw_type_name` `:768`; `sw_json_body` `:775`; `sw_meta_format` `:787-803` (schema string `f1c.telemetry.meta/2` at `:794`; keys `session_name, weekend, track, track_raw, season, session_type, session_type_name, tz_min, ticks, data_blocks, dropped_ticks`); `sw_meta_write(final)` `:805-820` (`js[1024]`, raw block, `sv2_block_put`).
- FACT **no `lap_len` anywhere in `storage_writer.inc`** (grep, 2026-10-14). `lap_len` appears in `session_archive.inc` only (`:49`, `:86`, `:216`, `:267`, `:300`, `:352`, `:354`, `:875`).
- FACT the writer runs on its own thread (`sw_writer_main` `:1242`). It does not read `g_telemetry`. Data crosses by the ring element `sw_event_t` (`:44-54`), filled on the capture thread in `sw_emit` (`:308`). OPEN fills `aiw`, `session_type`, `game_mode`, `mp_kind`, `generation`, `local`, `utc` (`:323-335`). CLOSE fills `aiw` (`:316`).
- FACT `g_telemetry.lap_len` is set by the AIW parser (`web_telemetry.c:4407` reads `lap_length`; `:4437` sets `g_telemetry.lap_len = ll`). The 1.0.1 archive copies it at session begin (`session_archive.inc:267`, `ar_begin` `:260`) and updates it per tick (`:300`).
- **Answer to spec R-1 / section 5.7 INFERENCE:** yes, available at OPEN on the capture thread. Plan:
  1. `storage_writer.inc:44-54` `sw_event_t`: add `float lap_len;`.
  2. `storage_writer.inc:329` (OPEN branch): `e->lap_len = g_telemetry.lap_len;` (same lock as `e->game_mode`, `:326`).
  3. `storage_writer.inc:316` (CLOSE branch): `e->lap_len = g_telemetry.lap_len;` (final META fallback when OPEN saw 0).
  4. `storage_writer.inc:640-666` `sw_wr_t`: add `float lap_len;`. Set it in `sw_open_session` (`:994`, near `:1029`), and update it from later events if it was 0.
  5. `storage_writer.inc:787-803` `sw_meta_format`: add `"lap_len_m":%.1f` and `"laps_count"` if wanted (ints are cheap).
  6. `sw_meta_write(0)` at `:1075` (initial) and `sw_meta_write(1)` at `:1118` (final, in `sw_close_session` `:1096`) already exist. The final value is written whenever the session closes.
- INFERENCE: `lap_len` can be 0 if the track has not loaded at OPEN. Then the initial META has 0 and the final META (CLOSE) is the fallback. R-1 (no lap list for an open session) is then accepted by the spec (refused until closed, Q3).
- The new key is additive (reader ignores unknown keys: `sv2_scan` META branch `:900-906` only checks size and JSON shape). Confirm in R1 that the META parser does not reject unknown keys (not yet read in detail; INFERENCE).

### 1.8 Writer and sessions (for the snapshot and the lap index)
- FACT `storage_writer.inc:38-42` `sw_car_t` (`slot, scope, mask_lo, mask_hi, v[SW_NCH]`), `:44-54` `sw_event_t`, `:56-61` `sw_ring_t`. `SW_NCH 128` (catalog ids 0..127), `SW_BLOCK_TICKS 20` `:18`.
- FACT `storage_writer.inc:827` `sw_block_add` (DATA block build, per-tick `sim` `blk_sim`), `:878` `sw_write_event`, `:902-943` `sw_events_from_sample` (EVENT `lap` at `:917`, `pit` and `incident` nearby).
- FACT `storage_writer.inc:994` `sw_open_session`, `:1096` `sw_close_session`, `:1185` `sw_apply`, `:1278` `sw_start`, `:1298` `sw_stop`.

---

## 2. Viewer side (user item 2)

### 2.1 How review mode starts (`/?session=`)
- FACT server page `web_telemetry.c:8279-8290`: the inline script reads `?session=` (`:8281`), calls `F1CViewer.mountConsole(..., {review:S, fetchTrack:...})` (`:8283`), skips the SSE feed when `S` is set (`:8287`, `con()` only when `!S`), and fetches `/telemetry<Q>` once (`:8285`).
- FACT viewer `telemetry_viewer.js:2997-3003`: `this.review` validated by regex (`:3000`); `WB.LIMITS` raised in review (`:3001`); `this.source = this.review ? 'past' : 'live'` (`:3003`).
- FACT `telemetry_viewer.js:3044-3047` `startPolls`: review calls only `pollSessions()` and returns (no live polls).
- FACT `telemetry_viewer.js:34-38` `fetchJson(url)` applies `reviewUrl` to every console fetch.
- FACT `telemetry_viewer.js:86-89` `reviewUrl(url,id)`: skips URLs starting with `/sessions` or `/recordings` (so the Recordings page and the new `/recordings/review` are not tagged). Anything else gets `session=<id>`.
- FACT `telemetry_viewer.js:91-95` `reviewTitle(id)`: parses only the 1.0.1 pattern; for a v2 id it returns the id itself. **Must change** for `v2_` ids to show the display name (spec 8.2). The display name must come from the snapshot (`sessions[].name` or from `GET /recordings/review`).

### 2.2 PAST mode (1.2.1, already in the code)
- FACT `telemetry_viewer.js:54-55` `isPast`, `pastNoData` (`isPast && !review`).
- FACT `telemetry_viewer.js:56-59` `pastShortLabel`.
- FACT `telemetry_viewer.js:60-64` `enterPast(folder, stem)`: `source='past'`, `this.past={folder,stem}`, `applyMode()`.
- FACT `telemetry_viewer.js:66-73` `goLive` (clears the selection with `recordingPost('load',{recording:''})` when `past` was set; review goes to `/`).
- FACT `telemetry_viewer.js:75-80` `goPast`.
- FACT `telemetry_viewer.js:3850-3858` `renderView`: when `pastNoData()` and not on the Recordings page, the pages draw a notice: `"Viewing a stored recording is not available in this version yet..."` (`:3855`) or `"No past session is open."` (`:3860`). The pages draw nothing from live data.
- FACT `telemetry_viewer.js:4070-4080` `refreshChrome`: live pill word `REVIEW` if `this.review`, `PAST` if past, `NO DATA`, `STALE`, `LIVE` (`:4072`). Banner in review: `'Saved session <b>' + esc(reviewTitle(this.review)) + '</b> · <a href="/" ...>back to live</a>'` (`:4079`). Spec wants `REVIEW: <name>` and `Back to LIVE` (8.2): change the text at `:4079` for `v2_` ids only.
- FACT `telemetry_viewer.js:3990-3998` `enterPast` / `goLive` / `this.review` checks.

**What must change for v2 review (R2 in the spec, plus the current PAST code):**
- `telemetry_viewer.js:3000` regex: accept `v2_` ids (already accepted; no change).
- `telemetry_viewer.js:91-95` `reviewTitle`: accept a display name for `v2_` ids.
- `telemetry_viewer.js:4079`: banner text for `v2_` (spec 8.2).
- `telemetry_viewer.js:3850-3858`: the PAST notice must not show in review (review has `this.review` set, so `pastNoData` is already false; verify).
- A polling function for `GET /recordings/review` while `state` is `building` (spec 5.3). Add it near `pollSessions` (`:3044-3047`, `startPolls`), and do not route it through `reviewUrl` (it is excluded by the `/recordings` prefix, `:87`).

### 2.3 Recordings page "Open" button
- FACT `telemetry_viewer.js:5725` (folder card): `<button data-act="load" data-rec="...">Open</button>`.
- FACT `telemetry_viewer.js:5740` (session row): `<button class="f1x-link" data-act="load" data-rec="<folder>" data-id="<stem>">Open</button>`. This is the button the owner means.
- FACT `telemetry_viewer.js:5646-5652` click handler (`box.addEventListener('click', ...)`): `act === 'load' && rec` → `self.loadRecording(rec, id)` (`:5650`).
- FACT `telemetry_viewer.js:5528-5535` `loadRecording(rec, stem)`: `recordingPost('load', {recording: rec})`; on success `enterPast(rec, stem)` and a toast `"Past session opened: ... Loading does not change any file."` (`:5530-5531`).
- **So Open does not go to review.** It selects the recording and switches the page to PAST mode, which shows the notice. This is current behaviour; the spec's F-LOAD-2 ("nothing opens a page") is stale (the 1.2.1 PAST mode now exists).

**What must change so Open goes into review with a v2 id (spec 8.3, R2):**
1. `storage_api.inc` snapshot (section 1.3): add `review_id` per session (`sa_session_json`, `:149-177`) and to `recordings.json` (section 3.2).
2. `telemetry_viewer.js:5740` session row: add a `Review` button (`data-act="review"`, `data-review="<review_id>"`) next to `Open`. Enabled only when `state === 'closed'` (spec 8.3: open, recovered and archived rows disabled with the spec 8.4 text).
3. `telemetry_viewer.js:5650` handler: `else if (act === 'review' && rid) location.href = '/?session=' + encodeURIComponent(rid);` (the same navigation as 1.0.1; spec 8.1 says page load).
4. Keep `Open` as selection only (spec Q9 PROPOSAL, owner decision Q9 open). Do not change `loadRecording`.
5. The "session ladder" (spec 8.2) is new UI; skip in R2 unless the owner wants it.

---

## 3. Schema, contract and gate (user item 3)

### 3.1 Version and API number
- FACT `web_telemetry.c:56` `#define API_VERSION "9"` (comment says 9 = recordings manager).
- FACT `web_telemetry.c:2413`, `:8631`: `API_VERSION` used in the `/health` and `/weather` documents (via the macro; no separate constant).
- FACT `mod.json:8` `"version": "1.2.0"`, `web_telemetry.dll.mod.json:5` `"version": "1.2.0"`, `f1c_mod.json:29` `"version": "1.1.0"` (stale).
- FACT `schemas/version.json:2` `"api_version": "9"`, `:3` `schema_set_sha256`.
- FACT `schemas/health.json:27-30` `api_version` is `type: string, pattern ^[0-9]+$` (no const; no edit needed).
- FACT `schemas/health.json:298` description mentions "api_version 9 (batch 4)"; `schemas/recordings.json:4` title says api_version 9; `schemas/recordings_accepted.json:4` and `recordings_jobs.json:4` titles say 9 (text only; the hash changes anyway).
- OPEN D3: spec says "Telemetry 1.1.0 (API 9)". Shipped is 1.2.0 with API 9. The next product version and the API 10 bump need a decision (the owner has approved API 10).

### 3.2 Schemas that change
- `schemas/recordings.json` (`additionalProperties: false`): `$defs.session` at `:47` has `required: [id, state, archived, bytes, ticks, files]` and `additionalProperties: false`. Add `review_id` (string, pattern `^v2_[0-9a-f]{16}$` or null) to properties and required. The list is generated per session (`sa_session_json`).
- New schema file `schemas/recordings_review.json` for `f1c.recordings.review/1` (spec 5.3): `{schema, session, state: none|building|ready|error, laps, cars, lap_len, reason, built_ms}`. New file changes the schema-set hash (see 3.3).
- `schemas/laps.json` (`additionalProperties: false`, top props `gen, cars, archive`): add `source` (enum `archive|recording|legacy|live`), `lap_len`, and `validated` (bool) for the recording answers (spec 5.3 wants `"source":"recording"`, `"lap_len"`, `"validated":false`). `archive` pattern `^[A-Za-z0-9_-]{8,47}$` already accepts `v2_` ids (19 chars).
- `schemas/trace_status.json` (`additionalProperties: false`, top props include `lap_len`, `archive`, `mode` enum `selected|all|archive` at `:42` region): add `source` and `validated` if the spec keeps them. Each lap item `required: [lap, t, samples, flags, start, lid]` (`laps` items). `lid` (`$defs.lid`) requires `gen, rev, slot, lap, src` and `src` enum `archive|import|live` (**no `recording`**: add it).
- `schemas/trace_data.json` (`additionalProperties: false`): `source` enum `archive|legacy|live` (`:source` description) (**add `recording`**). `samples` `maxItems 2400` (**conflicts with `TRACE_SAMPLES 3200`, `web_telemetry.c:1466`**; see D2). Sample item `required` = 19 keys: `t, d, s, th, br, st, gl, go, gv, y, yr, rpm, ge, tx, tz, tr, f, tt, rx`. Every key is required (value may be null). `f` is an integer of TRF bits, `tr` is 4 numbers (or null) gated by bit 1024, `tt` nullable array gated by 32768, `rx` nullable array gated by 65536-524288.
- `schemas/telemetry.json` (`additionalProperties: false`, required list `:` includes `c`, `lapm`, `trk`, `ty`, `ph`, `tk`, ...): the spec's `"review":{...}` key (5.5) is **not allowed**. Add `review` (object, optional, not in `required`) or drop it.
- `schemas/laps.json`, `trace_status.json`, `trace_data.json`: 1.0.1 review docs are already validated against these schemas (`test_telemetry.py:550-557`), so the new `source` and `lid.src` values must be added in the same change or the 1.0.1 cases still pass (they use `archive`).

### 3.3 Gate and the version pairing
- FACT `contract_validate.py`: `load_schema` `:20`, `schema_set_sha256` `:24-31` (SHA-256 over sorted `schemas/*.json` names and bytes, excluding `version.json`), `check_contract_version(api)` `:34-43` (compares `version.json.api_version` to `/health api_version` and the hash), `validate(name, doc)` `:46`, `check_schema_contract()` `:50-103` (api 2 to 3 rules, weather rows, `lid` required in laps and trace documents), `self_test()` `:105`.
- **No API 10 rule exists yet.** Add one in `check_schema_contract` (for the new `source` and `review` keys) or leave it to the schema files. The spec's "`contract_validate.py` contract 10 passes" needs this line.
- FACT `schemas/update_version.py`: usage `python schemas/update_version.py <api>` writes `schemas/version.json` with the new `api_version` and the hash (`main` writes the file; docstring line 3-8 names the procedure: bump `API_VERSION` in `web_telemetry.c` together with it). Run **after** all schema edits, then the gate.
- Rule for R1: run `update_version.py 10` once, at the end. The hash covers every `schemas/*.json` including `recordings_review.json`, so any later schema edit needs another run.

### 3.4 Other pins
- FACT `pin_sidecars.py` (sidecar hashes, `VERIFICATION_MANIFEST.json` keys `sidecar`, `sidecar_viewer`, `:6`, `:20`). The viewer and workbench sidecar hashes in `mod.json` (`:14-24`) and the manifest must be re-pinned after `telemetry_viewer.js` changes.
- FACT `verify_payload.py` (`payload` entry in the manifest, `pin` function `:38-51`); `verify_sidecar.py` (sidecar check, run by `build.bat`).
- FACT `build.bat:6` `TELEMETRY_BUILD_DIR` default `%TEMP%\f1c-web-telemetry-4.4.0` (stale name; not part of R1).

### 3.5 Trace-sample mapping for the adapter (1.0.1 shape, `f1c.trace/1`)
The adapter must emit the 19 keys per sample. Source is the DATA channel. `f` bits are the TRF values (`web_telemetry.c:1475-1490`, TRF_DIST `1<<0` ... TTEMP `1<<15`, RX `1<<16..19`, TTEMP_RX `1<<20`).
- `t` sim seconds (`sim_ms/1000` plus the block `t_first` base).
- `d` = channel 1 (`dist`). Set `f |= 1` (TRF_DIST), the viewer's validator needs bit 1 (`engValidateTrace` `web_telemetry.c:7219-7225`, `s.f & 1`).
- `s` = 2 (speed) with `f |= 2`; `th` = 3 with `f |= 4`; `br` = 4 with `f |= 8`; `st` = 5 (AI only) with `f |= 16`.
- `gl` = 6 (glat) with `f |= 32`; `go` = 7 (glon, longitudinal) with `f |= 64`; `gv` = 8 with `f |= 128`.
- `y` = 9 with `f |= 256`; `yr` = 10 with `f |= 512`.
- `rpm` = 15 with `f |= 4096`; `ge` = 16 (gear, RAW32 integer) with `f |= 8192`.
- `tx`, `tz` = 17, 18 with `f |= 16384`.
- `tr` = [11, 12, 13, 14] with `f |= 1024` (as an array of 4).
- `tt` = [20, 21, 22, 23] (player only) with `f |= 32768`, else null.
- `rx` = null (research not stored, NG-8); no `f` bits 65536-524288.
- Missing channel for a car: value null and the matching `f` bit clear.
- Assumption to check in R1: the 1.0.1 `go` (longitudinal) may use a different signal than v2 channel 7 (`glon`). INFERENCE; verify against `trace_sample_car_at` (`web_telemetry.c:1824`).
- The `f` bit 1 (TRF_DIST) must stay set on every sample, or the viewer validator rejects the lap.

---

## 4. Existing tests to extend (user item 4)

### 4.1 `runtime_test.c` (C, offline, synthetic storage v2 writer)
- FACT `runtime_test.c:11` `CHECK(x)` macro (exits on the first failure).
- FACT `runtime_test.c:4217` `main`. The storage calls at `:4242` (`test_storage_v2()`) and `:4243` (`test_storage_writer()`). Session archive `:4237`.
- FACT storage v2 batch 1 tests: `t2_u1_quantizer` `:2181`, `t2_u2_zigzag_varint` `:2223`, `t2_u3_crc` `:2253`, `t2_u4_data_roundtrip` `:2310`, `t2_u5_header_table` `:2352`, `t2_fix_block_crc` `:2404`, `t2_u6_block_framing` `:2458`, `t2_u7_compression` `:2510`, `t2_u8_aiw` `:2539`, `t2_u9_naming` `:2588`, `t2_f1_truncation` `:2653`, `t2_f2_mutation` `:2690`, `t2_b_fallback_and_decoder` `:2703`, `test_storage_v2` `:2742`.
- FACT batch 2 writer harness: `b2_mkdir` `:2758`, `b2_write_file` `:2779`, `b2_run_scenario` `:2802`, `t3_session_replay` `:2824`, `t3_u10_index_and_os_gate` `:2899`, `t3_u12_rebuild_keeps_open` `:2954`, `t3_u11_ring` `:3012`, `t3_set_tick` `:3046`, `t3_run_writer` `:3073`, `t3_check_file` `:3122`, `t3_b1_format21` `:3143`, `t3_slow_disk` `:3182`, `t3_s1_soak` `:3198`, `test_storage_writer` `:3209`.
- FACT batch 3 jobs: `j3_repair_cases` `:3247`, `j3_archive_cases` `:3318`, `j3_delete_cases` `:3369`, `j3_startup_order` `:3426`, `j3_channel_round_trip` `:3471`. `sj_submit` callers in tests at `:3558`, `:3569`.
- FACT snapshot tests use `sa_snapshot_rebuild()` at `:3668` and `:3704`.
- Plan for R1 tests (spec T1-T4), in `runtime_test.c` or a new file under `tests/`:
  - T1 (lap index, known laps): `tests/storage_b2_index_test.c` already covers lap truths `:208-255` and `sv2_idx_span`/`read_block` `:291-322`. Extend it with: short laps dropped (under 30 samples, `TRACE_MIN_LAP_SAMPLES` `web_telemetry.c:1470`), first partial lap not timed (already at `:244`), and a pit lap (channel 70). Do not write a second lap rule.
  - T2 (edges): sim-time gap and missing channel; reverse dist step; AI steer only. The index does not use dist, so these become "lap rule" tests only where the B2 rule changes.
  - T4 (read path): add a file-backed reader test: read only the header, the table and the block index by `ReadFile`-style offsets (a test double), assert no buffer larger than `SV2_MAX_PAYLOAD + 40` (`storage_v2.inc:14`, `:17`), and assert `blocks_read` equals the blocks of one lap (already instrumented, `storage_v2.inc:1228`).
  - T5 (fuzz): reuse `t2_f1_truncation` (`:2653`) and `t2_f2_mutation` (`:2690`) for the review reader; add block-boundary truncation.
- The B2 test's synthetic writer is `tests/storage_b2_index_test.c` (60 s, 20 Hz, 3 cars, 1200 ticks, 60 DATA blocks; its CHECK is at `:27`; `main` at `:258`). Reuse it for T1-T2.
- Build for the offline gate (game closed): `gcc -O1 -std=c11 -Wall -Wextra -Werror -Wno-unused-function -o %TEMP%\...\storage_b2_index_test.exe tests\storage_b2_index_test.c` (B2 record). Not run in this task.

### 4.2 `test_telemetry.py` (Python, live HTTP against `runtime_test serve`)
- FACT harness: `test_telemetry.py:130-131` starts `runtime_test.exe serve <dll>` on `127.0.0.1:18080`; `:172` asserts `listen_address`. Not read in full in this task.
- FACT 1.0.1 review cases: `:417` comment; `:423` bad ids (`/laps?session=...` unknown, `..%5C..%5Cx`, `/telemetry?session=abc`) → 404; `:465-472` review loop over `/laps /pits /incidents /driver /trace/status /trace/data /telemetry` with `session=<archive_id>` and value asserts (`:470` laps `[83.25]`, `:474` `mode=='archive'`, `:477` count 40); `:478-482` `/trace/open` and `/trace/data?lap=9` 404.
- FACT contract validation of review docs: `:550-557` validates `laps, incidents, pits, driver, trace_status, trace_data, telemetry` with `session=` against the schemas.
- FACT api pins: `:182` `health['api_version']=='9'`; `:235` `weather['api_version']=='9'`; `:576` `version['api_version']=='9'`; `:577` `check_schema_contract`. All change to `'10'` with the bump (`:182`, `:235`, `:576`).
- FACT recordings guards: `:485-520` (403/415/413/400/422/404 on `/recordings/load`); `:535-537` validates `recordings.json` and `recordings_jobs.json` against the schemas.
- FACT `:578-579` `PASS contract` print (uses `version['api_version']`).
- R1 test cases to add (spec T3, T6, T8): `GET /recordings/review?session=<v2 id>` (state `none|building|ready`), `/laps?session=v2_...`, `/trace/status?session=v2_...`, `/trace/data?car=1000&lap=N&session=v2_...`, `/telemetry?session=v2_...` each validated against the new or changed schema; unknown `v2_` id 404; open session 409 (spec 8.4); `..`, `\`, `/`, spaces in `session=` 404; a `v2_` id from a LAN peer 403 (once D4 adds the check).
- The `runtime_test serve` mode needs synthetic v2 folders in `%TEMP%` (see the seed-archive pattern `serve_seed_archive` `runtime_test.c:2106` and `serve_drop_archive` `:2157`).

### 4.3 Viewer tests
- FACT `viewer_test.js` (1090 lines); the print at `:1090` is `VIEWER PASS <checks> checks`. The spec's "431 now" is **not verified** (not run in this task).
- R2 cases (spec T9): `reviewUrl` for `v2_` ids; `reviewTitle` for `v2_`; banner text for `v2_`; Review button states per `state`; no polling in review except `/recordings/review`. Add beside the existing review cases.

---

## 5. Spec claims that no longer match the code (user item 5)

| # | Spec claim (section) | Current fact (file:line) | Action |
|---|---|---|---|
| 1 | v2 has no lap number, lap time, pits, incidents (F-V2-2, 14) | Lap number ch 69 (`storage_v2.inc:247`, writer `storage_writer.inc:263`); last lap ch 86 (`:268`); pit ch 70, position 68, track limits 73, contact 101; EVENT `lap`/`pit`/`incident` (`storage_writer.inc:917` and nearby) | Rewrite F-V2-2. Pits and incidents are stored; only names, team, weather and flags are missing. |
| 2 | "Flags not stored" is the only trace-validity gap (F-LAP-3) | Still true: flags ch 19 never written (`storage_writer.inc:239-240`) | Keep, but say pit state (ch 70) is available for pit-lap marking (Q7). |
| 3 | Lap rule: distance crossing, `tick_ms` difference (5.2, AR-1, T1) | B2 rule: channel 69 change plus EVENT `last_lap_s` (game time), sim time (`storage_v2.inc:1198-1199`, `:1266-1343`), offline PASS (`tests/storage_b2_index_test.c:208-255`) | Replace 5.2 with the B2 rule. AR-1 uses sim time (`sim_ms`), not `tick_ms` (wall clock, `sv2_catalog` id 0). Keep dist crossing only as a fallback if lap number is absent (none in B1+ files). |
| 4 | Block iterator is new in R1 (5.4) | `sv2_frame_at` `:1240`, `sv2_idx_read_block` `:1418`, `sv2_idx_span` `:1434` exist | Change 5.4 to "adapt existing functions to file-backed reads" (section 1.6). |
| 5 | Lap index built on the jobs thread (5.2, 7) | Index exists but has no job and no caller (`grep`, 2026-10-14) | Not a claim error; a work item (sections 1.4, 1.6). |
| 6 | `lap_len` not in writer; availability unknown (5.7, R-1) | Still not in writer. Available on capture thread at OPEN and CLOSE (`storage_writer.inc:308-344`, `compatibility_runtime.inc:237`, `web_telemetry.c:4437`) | Answer R-1 in the spec; plan in section 1.7. |
| 7 | v2 sample keys `{t,dist,speed,...}` (5.3) | `trace_data.json` requires 19 short keys (`t,d,s,th,br,st,gl,go,gv,y,yr,rpm,ge,tx,tz,tr,f,tt,rx`) | Replace with the mapping in 3.5. |
| 8 | `trace_data.json` allows 3200 samples (6, F-LAP-2) | `schemas/trace_data.json` `maxItems 2400`; `TRACE_SAMPLES 3200` (`web_telemetry.c:1466`) | Owner decision D2. |
| 9 | `source:"recording"` and `lid` for v2 (5.3) | `source` enum `archive|legacy|live` (`trace_data.json`), `lid.src` enum `archive|import|live` (`trace_status.json` `$defs.lid`) | Add `recording` to both (3.2). |
| 10 | `/telemetry` review frame has an extra `review` key (5.5) | `telemetry.json` has `additionalProperties: false` and no `review` (`schemas/telemetry.json`) | Add to schema or drop (3.2). |
| 11 | Review routes are local only in phase 1 (S-R2) | 1.0.1 review has no peer check (`web_telemetry.c:9136-9145`) | State that S-R2 is a new check, not existing behaviour (D4). |
| 12 | `/recordings/review` is a new GET (5.3) | The `session=` guard at `:9137` runs before any new route and would 404 a v2 id | Insert the v2 branch before `:9137` (1.1). |
| 13 | Viewer: Load only selects, nothing opens a page (F-LOAD-2, 2.1) | Load switches to PAST (`telemetry_viewer.js:5528-5535`, `enterPast` `:60`) with a notice (`:3855`) | Update 2.1 and 8.3 (section 2.3). |
| 14 | Banner text `Saved session ...` and `back to live` (F-REV-6, 8.2) | `telemetry_viewer.js:4079`: `Saved session <b>...</b> · back to live`. Spec 8.2 wants `REVIEW: <name>` and `Back to LIVE` | Change text for `v2_` only (1.0.1 keeps its text). |
| 15 | Schema hash and API 9 texts (3.x) | `schemas/version.json` api 9 and hash `7A8DB190...` (`:2-3`) | Update via `update_version.py 10` at the end (3.3). |
| 16 | Spec says version 1.1.0 (header) | `mod.json:8` 1.2.0; `f1c_mod.json:29` 1.1.0 | Owner decision D3. |
| 17 | Spec: 1.0.1 viewer test count 431 (T8, 11.1) | Not verified in this task (`viewer_test.js` prints its own count) | Remove the number or re-run and record it. |
| 18 | Spec: `g_http_big` at `web_telemetry.c:8000`; `TRACE_JSON_BYTES` `:3229` | `:8018` and `:3233` | Line drift. |
| 19 | Spec: `dcam_host_ok` at `director_state.inc:448` | `director_state.inc:1407` | Line drift (the TV Director work moved it). |
| 20 | Spec: `http_peer_is_local` at `:8697` | `web_telemetry.c:8767` | Line drift. |
| 21 | Spec: `sv2_scan` at `storage_v2.inc:706`; `sv2_data_decode` `:531`; SV2_R codes `:42-48` | `:840`; `:635`; `:48-55` | Line drift. |
| 22 | Spec: reviewer `ar_id_valid` `session_archive.inc:135-143`, `AR_ID_LEN` `:33` | `:135-145`, `:33` | OK (minor). |
| 23 | Spec: viewer `this.review` at `telemetry_viewer.js:2930`; regex `:2930`; `reviewUrl` `:3436-3438` and `:3897-3900`; `loadRecording` `:4759-4763`; click `:4848`; banner `:3550-3556`; pages `:2285-2287` | `:3000`; `:3000`; `:86-89`; `:5528-5535`; `:5646-5652`; `:4070-4080`; `:794-800` | Line drift; use the numbers in this map. |
| 24 | Spec: `/` page regex `web_telemetry.c:8213` | `web_telemetry.c:8281` | Line drift. |
| 25 | Spec: 1.0.1 route dispatcher `:8714-8765`; session check `:8864-8868`; `/trace/open` 404 at `:8763` | `:8809-8863`; `:9136-9145`; `:8850-8858` | Line drift. |
| 26 | Spec: block header 40 bytes, "about 24 bytes per block index entry" (5.4) | `sv2_idx_block_t` is 32 bytes (`storage_v2.inc:1207-1210`) | Correct the estimate (about 115 KB per 1-hour file at 3,600 blocks). |
| 27 | Spec: `SV2_MAX_PAYLOAD` `storage_v2.inc:14` | `:14` | OK. |
| 28 | Spec: `API_CONTRACT` section 17 is the next free section | Sections in use go to 16 (`API_CONTRACT_2026-10-09.md:450`) | OK; use section 17. |

---

## 6. Other spec sections, anchors and gaps

### 6.1 Section 1 (objective, acceptance AR-1..AR-9)
- AR-1 (lap times): see row 3 above. Owner check L3 unchanged.
- AR-2 (read-only): grep for `WriteFile`, `CreateFile` with write, `MoveFile`, `SHFileOperation`, `Delete*` in the review code. The jobs file has `sj_delete` (`storage_jobs.inc:501`); the review code must stay out of `sj_delete` and `sj_archive_file`.
- AR-3 (capture isolation, 19 Hz): the review index runs on `storage_jobs` (`sj_main`, `storage_jobs.inc:712`), and the capture path is not changed (`compatibility_runtime.inc:237`). ESTIMATE: measure.
- AR-4 (memory, no whole-file read): **not met** by the existing reader (section 1.6). Required change: file-backed block read.
- AR-5 (`/telemetry` answers within 50 ms during index build): the HTTP thread is one accept loop (spec F-THR-1; `web_telemetry.c:9347-9353`, not re-read). The index runs on the jobs thread, so the rule holds if no review code runs on `http_thread` during a build.
- AR-6 (refusals): the 409 for open sessions needs a state check through the snapshot (`sa_session_json` `state`).
- AR-7 (corrupt input): `SV2_R_*` codes exist. Index adds `SV2_IDX_R_CAPACITY 8`.
- AR-8 (1.0.1 unchanged): `test_telemetry.py:465-482` and `:550-557` are the regression cases.
- AR-9 (UI): see section 2.2.

### 6.2 Section 2 (facts) — corrections
- F-REV-3 and F-REV-4: correct (the 404 for v2 ids is `:9139-9141`; the dispatcher is `:8809-8863`).
- F-REV-2: `reviewUrl` location is `telemetry_viewer.js:86-89`, not `:3436-3438`.
- F-REV-7 shapes: correct for `/laps` (`session_archive.inc:693-707`), `/trace/status` (`:869-912`, car header `:885-887`, all required keys present), `/trace/data` (`:943-963`, with `lid`).
- F-SEC-1: bind `127.0.0.1` unless `Remote=1` (not re-read in this task).
- F-SEC-2: only `/director/state` checks Host (`dcam_host_ok`, now `director_state.inc:1407`; used at `web_telemetry.c:8886`, `:9077`, `:9232`). The spec's `:8955` is now `:9232` (drift).
- F-SEC-3: write guards (`web_telemetry.c:9117-9129`).

### 6.3 Section 4 (pages)
- Page ids and labels: `telemetry_viewer.js:794-800` (overview, dynamics, braking, tyres, compare, live, research). Spec 4.1 lists `timing`, `race`, `focus` (not in this list; check the other nav builder before R2).
- Spec "Driver Focus `/driver`" → `ar_build_driver_json` (`session_archive.inc:778`) uses the `pits.bin` file (`:802`) and names from the 1.0.1 archive; v2 has no names (R3).

### 6.4 Section 6 (memory)
- `g_http_big` is 2 MiB static (`web_telemetry.c:8018`); `TRACE_JSON_BYTES` (`:3233`) is the response cap.
- Index memory (estimate, see row 26): block index about 32 B per block; lap rows about 40 B per lap. For 1 hour at 20 Hz (about 3,600 DATA blocks) that is about 115 KB; the B2 array caps (`sv2_idx_init` callers) must be sized for it. The test uses 256 blocks (`tests/storage_b2_index_test.c:186`).

### 6.5 Section 7 (threading)
- Verified: capture under `g_telemetry_cs` (section 1.5); writer thread separate; jobs thread single (`storage_jobs.inc:731`). Rule R-T1 holds.

### 6.6 Section 8 (UI) — see section 2.

### 6.7 Section 9 (security)
- S-R1 (read-only): true for the reader if the review code uses only `ReadFile`.
- S-R2 (local only): **not in the code**. The 1.0.1 review branch (`web_telemetry.c:9136-9145`) has no peer check; `http_peer_is_local` (`:8767`) is used only by POST guards (`:9117`) and `http_request_is_local` (`:8795`). Required: a check in the new v2 branch (or in the guard at `:9136`) returning 403 for non-local peers. Decision D4.
- S-R3 (Host allowlist): none on review routes (the spec says the same). Reuse of `dcam_host_ok` is an M3 agreement with the TV Director owner (`director_state.inc:1407`).
- S-R4 (ids): the lookup must be a snapshot table (section 1.3), not `ar_path`.
- S-R5 (escaping): `esc_json` for server strings; the viewer uses `esc()`.

### 6.8 Section 10 (batches)
- R1 file list: `storage_v2.inc` (file-backed reader, adapter helpers), `storage_jobs.inc` (`SJ_JOB_INDEX`), `storage_writer.inc` (`lap_len` on event and META, plus `review_id` in snapshot), `storage_api.inc` (`GET /recordings/review`, `review_id` in `sa_session_json`, resolver), `web_telemetry.c` (guard `:9136`, dispatcher `http_archive_route` `:8809`, `API_VERSION` `:56`), `schemas/` (new `recordings_review.json`, changes listed in 3.2, `update_version.py 10`), `API_CONTRACT` section 17, `contract_validate.py` (API 10 rule, 3.3), `test_telemetry.py` (`:182`, `:235`, `:576`, and new cases, 4.2).
- Note: `storage_v2.inc`, `storage_jobs.inc` and `storage_writer.inc` are written in batches B1 and B2 (2026-10-13 and 2026-10-14). The B2 record says "API still 9; full gate EXIT 0 after manifest payload re-pin". Re-pin after R1 (3.4).
- R2 file list (viewer): as in section 2.2 and 2.3. `viewer_test.js` (4.3).

### 6.9 Section 11 (tests)
- Mapped in section 4. T7 (threading static check): grep for `g_telemetry_cs` in the review functions (none expected); T7(c) hash of `Telemetry\` before and after a review pass.

### 6.10 Section 12 (risks) — updates
- R-1 `lap_len` at OPEN: answered (section 1.7). Residual: `lap_len` may be 0 at OPEN; use CLOSE.
- R-2 pit laps: channel 70 exists; the spec may mark pit laps from it (D7).
- R-3 lapped cars: the B2 rule uses channel 69 per car, so lapped cars count their own laps. Owner check still needed.
- R-5 `reviewUrl`: new GET routes are excluded only by the `/recordings` prefix (`telemetry_viewer.js:87`). `/recordings/review` is safe; any `/recordings*` path is safe.
- R-7 META change: the reader ignores unknown keys (INFERENCE; confirm in `sv2_scan` META branch `storage_v2.inc:900-906`).

---

## 7. Decisions and open items for the R1 start

- D1 Lap rule: use the existing B2 rule (channel 69 and EVENT `last_lap_s`, sim time). Recommended. Needs the spec amended (rows 3 and 4).
- D2 Trace sample cap: `TRACE_SAMPLES 3200` in code, `maxItems 2400` in schema. Recommended: raise the schema to 3200 (contract change) or cap the lap to 2400 (data loss). Decide.
- D3 Version: product number (spec says 1.1.0; shipped 1.2.0) and API 10 (approved). Decide the product version string.
- D4 Peer policy for review: local only (spec S-R2). Requires a new check; 1.0.1 review is not local-only today.
- D5 Index job: a third job kind on `sj_main`, or a separate worker thread. Recommended: job kind (no new thread).
- D6 `lap_len` in META: yes (owner Q4 decided). Also `laps_count` optional.
- D7 Pit and out laps: channel 70 gives pit state. Recommended: mark laps that contain a pit state change (owner Q7: show and mark).
- D8 Review of a session with `lap_len` 0: refuse list with spec 8.4 text.
- D9 Sample `f` validity and the AI `steer` (channel 5 only for AI; player has no steer).

---

## 8. Documentation gate (AGENTS.md)
- Durable record: this file (`R1_IMPLEMENTATION_MAP_2026-10-14.md`, component root). It is discoverable from the component folder. **Not indexed** in `CURRENT_STATE.md` (not edited, per the owner's rule "no edits except your output file"). Claude should add one index line after the owner reads it.
- Facts vs inference: tagged in each section (FACT, INFERENCE, OPEN).
- Pending validation: D1-D9; the R1 offline gate; the owner live list (spec 11.2) after R1 and R2.
- Negative result recorded: no spec claim about `sv2_*` or writer lap channel 69 holds as written; no existing caller for the index (section 1.6).
- Not done in this task: no build, no compile, no test run, no game, no EXE, no file other than this one. `viewer_test.js` and `runtime_test` were not run. Their counts are not verified.
- Workspace validator: `tools/workspace/check-workspace-structure.ps1` (run with forward slashes from `F1\`) reports 2 errors that already existed (`.claude`, `plans` top-level). No structural change.
- No `PRODUCT_STATUS.json` change (product `web_telemetry` stage `observe`, shipped, not changed by a survey).
- No `findings.jsonl` entry (no game behaviour finding).
- Not read: `api1.txt` and private keys (none needed).
