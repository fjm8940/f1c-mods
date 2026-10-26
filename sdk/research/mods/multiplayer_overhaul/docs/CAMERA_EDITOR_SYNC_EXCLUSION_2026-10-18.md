# Camera Editor files left out of content sync (2026-10-18)

Owner request (2026-10-18): "the multiplayer overhaul mod should exclude camera editor specific files and not sync
them to all clients joining".

Stage: staged as Multiplayer Overhaul 1.0.4 (owner 2026-10-18), NOT deployed, NOT run in game. Release: [releases/v1.0.4/RELEASE_RECORD.md](../releases/v1.0.4/RELEASE_RECORD.md).

## Finding (verified in source)
Content sync (`dll/coop_mode.cpp`, `build_season_set`) sends every file in each season car folder under
`SeasonData\Vehicles`. Camera Editor (`development/mods/camera_editor/app/layers.py`, `plugin/camera_editor.c`)
saves two of its layers in those car folders:
- `camera_editor.profile.cfg` (one car model, scope "vehicle")
- `<driver>.camera_editor.cfg` (one `.veh`, scope "driver")
- plus temporary/backup copies `*.tmp` / `*.camera_editor.bak` written while it saves.

So a host's personal camera setup was listed and installed on every joining client. The other Camera Editor files
(`Save\Default.camera_editor.cfg`, `CameraProfiles\...`) are outside `SeasonData` and were never synced.

## Change
`sync_camera_editor_file()`: a file name containing `camera_editor` (any case) is never synced. Used in
- `sync_skip_name` (host: file is not listed), and
- `sync_path_ok` (host refuses `GET`; client drops the row when it reads `LIST`), so a new client also ignores these
  files from an older host (1.0.3 and earlier). The client already skips refused rows quietly; the join continues.

Before-state: `dll/coop_mode.cpp.BEFORE_20261018_camexcl`, `README.md.BEFORE_20261018_camexcl`.

## Verification (offline)
- `dll/build.bat` PASS (-Werror). Dev DLL `%TEMP%\multiplayer_overhaul_build\multiplayer_overhaul.dll`
  SHA-256 `61d597bd5de78175ed3018c67d84d65501645c4cf09c2ccdbd3079d3ff7628f8`.
- Filter test (functions copied from the source, `%TEMP%\mo_camexcl_test`): 9/9. Both profile names, an upper-case
  `.tmp`, a `.bak` are refused; `.veh`, `.cam`, `F1_2002.gdb` still sync.
- `tests/`: 10/10 scripts PASS. `test_target_probes.py` needs `F1C_EXE` set
  (`%USERPROFILE%\Documents\VM\F1 Challenge V10_clean\F1 Challenge Online.exe`); its default path
  `Desktop\F1\F1 Challenge V10` no longer exists. That is an old problem in the test, not caused by this change.

## Limits / pending
- A camera file that already reached a client through an older host stays there (sync never deletes files). The
  client can delete it, or restore it from `CoOpBackup\<stamp>\backup\`.
- Inference: a camera file that differs between host and client no longer counts as "content differs", so there is
  no download prompt for it. Not checked in a live session.
- Live check pending: host with a `camera_editor.profile.cfg` in a season car folder; client joins; the host log
  `content set: ... files=N` should not include it and the client should not be asked to download it.
- Version bumped to 1.0.4 and packaged (owner 2026-10-18); deploy is owner-run. The dev DLL hash above is the pre-bump build; the release DLL is 355BC295.

## Wasted-effort check
- Failed/retried commands: inline filter-test script broke twice on backslash escaping in the shell; moved to a script file.
- Repeated reading, broad builds, idle polling: none observed.
- Workspace validator: same 4 errors as before this task (`.claude`, `LOG`, `plans` top-level folders; Randomizer productionPath); none caused by this change.
