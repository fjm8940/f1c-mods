# GP2FFB and MOZA R9 device-name mismatch

Date: 2026-08-30  
Pipeline stage: investigate  
Affected external installation: `C:\Grand-Prix-II_DOS_EN_v10b`  
No external configuration, executable, driver, or registry state was changed.

## Question

Why does GP2FFB 0.4.4 reject `Device: MOZA R9 Base` even though that is the
visible device name?

## Verified evidence

- `ffb.ini` line 3 contains the ASCII bytes for exactly `Device: MOZA R9 Base`
  followed by CRLF. There is no trailing space or hidden character in the file.
- `log.txt` records the configured target as `MOZA R9 Base`, but DirectInput
  enumeration reports `MOZA R9 Base` followed by an invalid/replacement glyph.
- Both the current-user and machine joystick OEM registry records for
  `VID_346E&PID_0002` contain an `OEMName` of length 13. Its code points are:
  `U+004D U+004F U+005A U+0041 U+0020 U+0052 U+0039 U+0020 U+0042 U+0061
  U+0073 U+0065 U+0090`.
- Therefore the DirectInput product name has the invisible control character
  `U+0090` appended to the normal 12-character name. The console renders this
  badly (as an accented or replacement glyph).
- GP2FFB source performs an exact `std::wstring` equality comparison between
  the DirectInput product name and the INI value. It then supports a numeric,
  1-based device index as an alternative. The local enumeration lists the MOZA
  wheel as device 1.

Primary source:
`https://github.com/gplaps/GP2FFB/blob/main/ffb_setup.cpp`

## Conclusion and recommendation

The visible spelling is correct, but the underlying Windows/MOZA DirectInput
name is not the same string because of the trailing `U+0090`. Use this setting:

```ini
Device: 1
```

This is the smallest workaround and avoids editing joystick registry data.
Close GP2FFB, change the setting, save the file, then relaunch GP2FFB after
x86GP2 is running. Begin testing with conservative force strength.

## Fact, inference, and limitations

- Fact: the INI and registered DirectInput strings differ by `U+0090`.
- Fact: device index 1 is supported by the inspected GP2FFB code and matches the
  only currently enumerated controller.
- Inference: the malformed OEM name was written by the MOZA driver or its setup
  process. The writer was not traced, so responsibility is not proven.
- Pending human validation: confirm that `Device: 1` initializes the wheel and
  that force direction/strength are safe in-game.
- Device ordering can change if another DirectInput controller is connected;
  re-check the enumeration number in that case.

## Verification and failures

- Read the local INI as text and bytes.
- Read the local GP2FFB log and x86GP2 changelog references.
- Queried present MOZA PnP devices and both joystick OEM registry locations.
- Compared behavior with the upstream GP2FFB source.
- The required workspace validator was run before documentation. It reported
  nine pre-existing errors unrelated to this investigation: missing live/clean
  game anchors, the corresponding Randomizer production path, and source-like
  files in older bug-report-site production snapshots.
- An initial workspace-wide text search exceeded the useful time window and
  returned no result; subsequent checks were scoped to the relevant paths.
- One read-only PowerShell registry query had a parser error and was corrected.

## Restoration information

No external state was modified. The only additions are this record and its
entry in `research\inputs\README.md`; no restoration action is needed for the
diagnosed GP2 installation.

## Wasted-effort check

- Repeated reading of the same large material: none observed.
- Unnecessarily broad builds or tests: the first workspace-wide text search was
  broader than necessary and was replaced with scoped inspection.
- Failed or retried commands: one PowerShell pipeline parse error was corrected.
- Polling, waiting, or idle time: none observed.

