Fisheye black screen (Tester B report)
Last updated 2026-10-05
Author Matias

# Fisheye turns the whole screen black on "basegame" — investigation + fix (plug-in 1.0.5, published 2026-10-06)

## Report (Discord, Tester B, 2026-10-05)

Applying any Fisheye value on the base game turns the whole screen black, HUD and pit box included.

## Finding (verified offline, cause in 1.0.4 `plugin/fisheye.h`)

1.0.4 pastes its fisheye picture over the whole frame at `Present` whenever a fisheye is on, even if the game never
drew into that picture this frame. Everything (world, HUD, pit box) is then covered by an empty texture = black.
Three ways the game can bypass the picture, all reachable in 1.0.4:

1. **Game never calls `SetRenderTarget`** (depth buffer made with the device). 1.0.4 only redirected the frame start
   when it had seen a depth buffer through `SetRenderTarget`; without one it never redirected, yet still pasted at
   `Present`. Reproduced: `tests/fisheye_test.c` "no SetRenderTarget" case FAILED on the 1.0.4 code (picture
   replaced by stale picture content; on a fresh game that content is black).
2. **Anti-aliasing on**: the game's depth buffer is multisampled and cannot pair with a plain texture, so
   `SetRenderTarget(picture)` fails (result was not checked) and the empty picture is pasted.
3. **Back buffer not recognised** (pointer differs, e.g. a Direct3D 8 wrapper such as dgVoodoo / d3d8to9): world
   goes to the real screen, empty picture pasted over it.

Which of the three Tester B hits is **not known** (inference: the owner's canonical Online run calls
`SetRenderTarget` every frame with anti-aliasing off — probe runs 1-3 — so the base game difference is likely
settings or a wrapper, not code). Their `plugins\lightloader\camera_editor_log.txt` from 1.0.5 will say which:
`fisheye: own depth buffer …` (2), `fisheye: the game drew the 3D world outside the fisheye picture` (3),
`fisheye: the graphics card refused the picture as a render target` (any refusal).

## Fix (development, `plugin/fisheye.h`, plug-in version 1.0.5)

- The picture is pasted only when the game really drew into it this frame (`fe_tex_live`, set only when
  `SetRenderTarget(picture)` succeeds).
- No `SetRenderTarget` seen: depth buffer taken from the device (`GetDepthStencilSurface`).
- Game depth multisampled or smaller than the picture: own plain depth buffer (`fe_own_depth`, released on Reset).
- Full-screen 3D world drawn on any other same-size target: that frame is left as the game drew it
  (`fe_world_elsewhere`), never covered.
- Return to the back buffer falls back to no depth if the device refuses the pair.
- `camera_editor.meta.json` version was still `1.0.3` in the 1.0.4 build; now `1.0.5`, matching `PLUGIN_VERSION_TEXT`.

## Verification (2026-10-05, offline, real Direct3D 8 device, owner PC)

| Check | Result |
|---|---|
| `plugin/build.bat` | exit 0; SIG TEST PASS (5 EXEs + moved-code control); RUNTIME 37/37; FISHEYE TEST PASS (13 checks incl. 3 new: no SetRenderTarget, unrecognised back buffer, anti-aliased depth) |
| `python tests/test_app.py` | APP TEST PASS (93) — app unchanged |
| DLL | `plugin/camera_editor.dll` SHA-256 `f265743946745689c2f66d3a1b824c077f249d5c74802de79dfb15937da931a3` (development build, not packaged) |

Limits: the anti-aliasing test pairs a multisampled depth with a plain back buffer (a real AA game has both
multisampled); the wrapper case is simulated by bypassing the hook. Not run in a game.

## Pending

- Tester B (or owner with anti-aliasing on / without `SetRenderTarget` path) in-game test of 1.0.5 + their log.
- Released as 1.0.5 on 2026-10-06: [release record](../../../../operations/release_management/camera_editor/RELEASE_1_0_5.md).
