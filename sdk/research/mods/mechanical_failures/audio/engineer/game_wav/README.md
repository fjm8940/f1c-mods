# Engineer lines - game format (2026-10-07)

`MF_*.wav` = `../src_leveled/MF_*.wav` resampled 24000 -> 22050 Hz (Python `audioop.ratecv`),
mono 16-bit PCM - the same format as the game's driver-name clips (`Audio\Commentary\PWS<Name>_FB.wav`).
Shipped to `<game>\Audio\MechanicalFailures\engineer\` and played through the game's own commentary
player (see `docs/TESTER_REPORT_TESTER_B_2026-10-07.md`, native voice section).
