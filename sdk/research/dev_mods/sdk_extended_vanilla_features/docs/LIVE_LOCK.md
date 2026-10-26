# LIVE_LOCK

Single-writer lock for the running game process. See
`ASSIGNMENT_PARALLEL_RULES.md` §4.

    STATUS: FREE
    HOLDER: -
    SESSION: -

To take it: replace `FREE` with your assignment id (e.g. `EVF-005`), fill
`SESSION` with the session state you need. Set it back to `FREE` when done.
If it is not `FREE`, you do not have the game — emit a `LIVE READ REQUEST`.
