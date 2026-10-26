# F1 Challenge 99-02 — identified original source files (.cpp)

31 original 2002 source filenames, baked into the shipped exe by MSVC
`__FILE__` string literals. Extracted deterministically from the IDB
(`tools/extract_strings.py` headless) — see `strings_modules.json` /
`STRINGS_REPORT.md` for the per-function mapping.

Trust: extraction (FACT — the strings are in the binary).

| # | Source file | Functions mapped to it |
|---|---|---|
| 1 | ai_db.cpp | 2 |
| 2 | camera.cpp | 3 |
| 3 | config.cpp | 1 |
| 4 | dynman.cpp | 6 |
| 5 | game.cpp | 9 |
| 6 | garage.cpp | 1 |
| 7 | hdvehicle.cpp | 5 |
| 8 | htree.cpp | 1 |
| 9 | hwinput.cpp | 6 |
| 10 | lbmpeg.cpp | 3 |
| 11 | netcomm.cpp | 17 |
| 12 | o_appgz.cpp | 1 |
| 13 | o_basegz.cpp | 1 |
| 14 | onscreen.cpp | 6 |
| 15 | options.cpp | 1 |
| 16 | osman.cpp | 2 |
| 17 | pitcrew.cpp | 3 |
| 18 | plrfile.cpp | 5 |
| 19 | render.cpp | 3 |
| 20 | replay.cpp | 1 |
| 21 | searchgrid.cpp | 1 |
| 22 | setup.cpp | 4 |
| 23 | sound.cpp | 6 |
| 24 | specialfx.cpp | 10 |
| 25 | steward.cpp | 6 |
| 26 | tire_manager.cpp | 7 |
| 27 | util.cpp | 1 |
| 28 | vegfx.cpp | 2 |
| 29 | vehload.cpp | 3 |
| 30 | vidman.cpp | 7 |
| 31 | weather.cpp | 1 |
