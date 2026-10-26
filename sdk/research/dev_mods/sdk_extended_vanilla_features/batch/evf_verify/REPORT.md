# evf_verify merge report

- items assigned: 236
- rows accepted : 233
- rows rejected : 3
- items missing : 3

## Verdicts / fix types

- CONFIRMS: 225
- MISMATCH: 8

## why_silent / confidence

- no_emitter: 127
- emitter_exists_untested: 49
- blocked_cooldown: 23
- emitter_guard_never_true: 17
- UNKNOWN: 17

## Per worker

| worker | lines | accepted | rejected | assigned |
|---|--:|--:|--:|--:|
| A | 12 | 11 | 1 | 12 |
| B | 12 | 12 | 0 | 12 |
| C | 12 | 12 | 0 | 12 |
| D | 12 | 12 | 0 | 12 |
| E | 12 | 12 | 0 | 12 |
| F | 12 | 12 | 0 | 12 |
| G | 12 | 12 | 0 | 12 |
| H | 12 | 12 | 0 | 12 |
| I | 12 | 11 | 1 | 12 |
| J | 12 | 12 | 0 | 12 |
| K | 12 | 12 | 0 | 12 |
| L | 12 | 12 | 0 | 12 |
| M | 12 | 12 | 0 | 12 |
| N | 12 | 11 | 1 | 12 |
| O | 12 | 12 | 0 | 12 |
| P | 12 | 12 | 0 | 12 |
| Q | 11 | 11 | 0 | 11 |
| R | 11 | 11 | 0 | 11 |
| S | 11 | 11 | 0 | 11 |
| T | 11 | 11 | 0 | 11 |

## Rejections

- `CI-121` worker A line 7: eid 130 maps to 'PWS_MECHANWARN0', row claims 'MechWarn0'
- `CI-109` worker I line 6: eid -1 not in full_event_dump.json
- `CI-154` worker N line 8: eid 184-189 not in full_event_dump.json

## Items with no accepted row

CI-109, CI-121, CI-154
