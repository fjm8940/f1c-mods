# Multiplayer AI target compatibility evidence — 2026-08-23

This development candidate explicitly accepts two exact executable identities:

- canonical host SHA-256 `1A70E5ED0C4A059F7F66741B1682D0AEDB14921AA6140CDB70B09549FDF3A003`, PE checksum `00306F7B`;
- co-op client SHA-256 `DE151845DCB08B1B330641FD09779C09B0924EAE872DA4AFBA96317339BE6230`, PE checksum `0030ED71`.

The SDK variant profiler found the same machine, timestamp, image base, entry
point, image size, vehicle-array address, vehicle stride, and vehicle-liveness
offset. Symbol-recipe resolution for the client target returned `RESOLVED` at
confidence 1.0. All hook entry bytes and the first 64 bytes of every directly
called setup function are identical. The complete file comparison found 509
different bytes, but none overlap a hook or directly called function.

The runtime does not accept the alternate checksum generally. It computes the
main executable's complete SHA-256 with Windows CryptoAPI and accepts the
client variant only when the digest exactly matches the reviewed `DE151845...`
file. A mod-local compile-time checksum allowance lets the SDK detour helper
repeat its header check only after that stronger worker validation succeeds.
The canonical SDK header and probe validation remain in force. Any other
executable continues to fail closed.

This is development compatibility evidence, not completed gameplay validation.
The alternate client still requires a guarded full-weekend human test.

The multiplayer qualification-grid assignment entry at `00462E20` has the
same first 16 bytes in both reviewed executables. Its recovered ABI is a
game-thread `thiscall` receiving the session manager in `ecx`, no stack
arguments, and a byte result in `al`.
