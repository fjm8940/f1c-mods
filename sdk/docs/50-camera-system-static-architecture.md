# 50 — Camera system: how are camera files, modes, targets, and rendering connected?

**Game:** F1 Challenge 99-02 (`F1 Challenge Online.exe`)  
**Date:** 2026-08-30 · **Author:** Codex for Matias · **Status:** STATIC ONLY

## Abstract (TLDR)

F1 Challenge does not have one monolithic camera. Static analysis shows a
manager that owns three camera families: local/onboard views, static trackside
views, and tracking views. A separate action state machine selects a family and
mode, target-selection code chooses the watched vehicle, and the selected view
object supplies the transform and projection inputs used for rendering.

- `LocalCam` blocks supply the familiar nose, cockpit, TV cockpit, swingman,
  onboard, and auxiliary views. Four names occupy reserved slots; other blocks
  enter a file-ordered array capped at 32 entries. **DERIVED**
- `StaticCam` blocks add a world position, orientation, activation location,
  and activation radius. **DERIVED**
- `TrackingCam` blocks add tracking rate, movement rate, and an optional
  movement path. The loader stages at most 128 tracking cameras. **DERIVED**
- The camera manager changes modes through a bounded action dispatcher rather
  than by directly choosing arbitrary view pointers. **DERIVED**
- Target names have special meanings for `Player`, `Original`, `Last`,
  `AnyTrackside`, and `None`; other names are resolved through all three camera
  families. **DERIVED**
- This paper does not authorize direct calls or memory writes. The reviewed SDK
  still needs ABI, lifecycle, and runtime proof before these paths become public
  commands. **CONFIRMED by SDK policy**

The best static-only product opportunity is to extend the supported camera
editor with lossless inspection of static and tracking cameras and their
movement paths. Runtime behavior should be a later validation gate, not a
prerequisite for completing the schema and architecture work.

## 1. Introduction

The research question is: how does the current executable connect camera
configuration blocks to mode changes, target selection, per-frame transforms,
and the render projection? The goal is a bounded architecture map suitable for
future camera-editor, broadcast-director, replay, and spectator projects.

In scope are the manager lifecycle, configuration loaders, object layouts,
camera families, mode/action dispatch, target resolution, and the final static
render path. Out of scope are new hooks, live memory writes, historical
developer-editor key bindings, and claims about how a parameter looks or feels
in game.

## 2. Method: static analysis

The analysis used the canonical `tools/f1c_mod.py search` and `inspect` front
door over the raw Hex-Rays corpus, reviewed-name catalogue, caller/callee
graph, strings, existing configuration parser, and preserved camera projects.
The primary functions examined were:

| Address | Current corpus name | Role in this paper |
|---|---|---|
| `0x0040C920` | `CamMan_Init` | Manager initialization and config bootstrap |
| `0x0040C9E0` | `CamMan_Restart` | Restart-state reset |
| `0x0040CCE0` | `CamMan_Post` | Owned-family teardown |
| `0x0040CA30` | `MainCameraDirectorUpdate` | Per-frame manager update |
| `0x0040C140` | unresolved | `Cams.cfg` / `Default.cfg` loader path |
| `0x00408E20` | `CameraViewSystem` | `LocalCam` loader and fixed/generic allocation |
| `0x004562F0` | unresolved | `LocalCam` block parser |
| `0x004572B0` | unresolved | static-camera block parser |
| `0x0040B850` | `MainView` | `TrackingCam` collection loader |
| `0x004585E0` | unresolved | tracking-camera block parser |
| `0x00458A60` | unresolved | tracking-camera serializer |
| `0x0040CDE0` | `GameActionStateMachine` | Logical camera action dispatcher |
| `0x00409A00` | `CameraModeViewportUpdate` | Selected-view application |
| `0x0040DB00` | `CameraTargetNameResolver` | Named and trackside target resolution |
| `0x00542050` | unresolved | FOV/projection parameter construction |
| `0x00541B70` | `CameraProjectionUpdate` | Projection/view matrix submission |
| `0x00548C20` | `SceneRenderTransform` | Render-view transform path |

Decompiler-generated names are leads, not proof of ABI or object type. In
particular, the constructor currently named `F_3DSoundEmitter` is repeatedly
used to create 200-byte camera-view objects; this paper describes the observed
use without silently renaming the reviewed SDK symbol.

## 3. Method: runtime verification

**PENDING.** No game process, memory poke, hook, or executable launch was used
for this research pass. Existing camera-editor and free-camera records contain
earlier runtime observations, but they were treated as references rather than
new verification.

A later guarded-weekend pass should validate only the questions in section 5.6.
Every new conclusion below therefore remains `DERIVED` unless explicitly tied
to an already independent confirmed contract.

## 4. Results

### 4.1 Architectural layers

```text
camera configuration files
  LocalCam / StaticCam / TrackingCam blocks
                    |
                    v
camera-family loaders and owned collections
  local/onboard | static trackside | tracking/movement-path
                    |
                    v
manager action state + target selection
  user key queue | automatic director | watched vehicle changes
                    |
                    v
selected 200-byte view object
  position/orientation | FOV | clip/LOD | type-specific fields
                    |
                    v
per-frame transform and projection submission
```

This separation is important for mod design. Configuration authoring, target
selection, and rendering are related but not interchangeable capabilities.
A safe SDK should expose them through different interfaces. **DERIVED**

### 4.2 Manager lifecycle and ownership

The static object rooted at `0x005EA510` is passed to `CamMan_Init` and the
per-frame director update in existing corpus evidence. Initialization resolves
the `MainView` asset, clears related global state, loads scene/module paths,
creates the config-backed family objects, and marks camera facilities active.
**DERIVED**

The manager layout visible through static access is:

| Manager offset | Static interpretation | Evidence |
|---:|---|---|
| `+0x08` | currently active camera-family object | Read and virtually dispatched each frame |
| `+0x0C` | logical camera action/mode | Written by `GameActionStateMachine` |
| `+0x14` | saved mode used across player/spectator transitions | Written by mode guards |
| `+0x18` | local/onboard camera collection | Loader allocation and target lookup |
| `+0x1C` | second trackside family, consistent with static cameras | Teardown and target lookup |
| `+0x20` | third trackside family, consistent with tracking cameras | Teardown, target lookup, action 5 preference |
| `+0x24` | input/director guard byte | Blocks manual and random mode changes |
| `+0x2C` | one-shot mode-change flag | Forces an action result on next update |
| `+0x2D` | first-update initialization flag | Activates the initial local view once |
| `+0x2E` | automatic/random director selector | Chooses random director versus keyboard mapping |
| `+0x30/+0x34` | automatic-director interval inputs | Passed to the timer reset helper |
| `+0x38` | last watched-driver identity | Compared against the current watched vehicle |
| `+0x3C` | automatic-director countdown | Decremented by frame/session delta |

The static interpretation of `+0x1C` versus `+0x20` as static versus tracking
is supported by action 5 preferring the `+0x20` family when it has entries and
falling back to `+0x1C`. Exact class names remain unreviewed. **DERIVED**

`CamMan_Restart` resets the last watched-driver identity to `-1`, clears a
transition byte, and resets shared camera state. `CamMan_Post` clears the active
pointer and invokes virtual destruction on the three owned camera families at
`+0x18`, `+0x1C`, and `+0x20`. **DERIVED**

### 4.3 Configuration families and limits

#### Local/onboard views

`CameraViewSystem` reads `LocalCam` blocks and treats four names specially:

| Name | Collection offset | Applied render mode |
|---|---:|---:|
| `NOSECAM` | `+0x10` | `0` |
| `COCKPIT` | `+0x14` | `1` |
| `TVCOCKPIT` | `+0x18` | `2` |
| `SWINGMAN` | `+0x1C` | `3` |

Every other `LocalCam` is copied into a contiguous 200-byte array at collection
offset `+0x20`, in file order. The parser stages no more than 32 generic
entries. Count and selected index are stored at `+0x38` and `+0x3C`; render
mode `4` selects `array + 200 * index`. **DERIVED**

The same collection also owns four programmatically constructed auxiliary
views selected by render modes `5` through `8`: `2SEATER`, `3SEATER`,
`LeftOfPits`, and `RightOfPits`. They occupy collection offsets `+0x24` through
`+0x30`. **DERIVED**

#### Static trackside views

The static-camera parser consumes:

- `Position` and `Orientation`;
- the common render fields listed in section 4.4;
- `ActivationLocation`;
- `ActivationRadius`.

It marks the parsed object with internal type value `1`. The static call graph
does not yet prove the user-facing block keyword or collection capacity, so
those remain open rather than inferred from legacy prose. **DERIVED**

#### Tracking views

`MainView` scans for `TrackingCam` blocks, stages them in 208-byte temporary
objects, and accepts at most 128 entries. It then allocates a right-sized array
and copies each staged object into owned storage. **DERIVED**

Tracking-camera data adds:

| Object offset | Parsed field |
|---:|---|
| `+0x7C/+0x80/+0x84` | activation location |
| `+0x88` | activation radius |
| `+0x8C` | squared activation radius cache |
| `+0xA4` | tracking rate |
| `+0xA8` | movement rate |
| `+0xB0` | movement-path container |

The tracking parser assigns internal type value `2`. Its serializer emits all
common fields followed by activation location/radius, tracking rate, movement
rate, and the movement path. This is strong static support for a lossless
offline tracking-camera inspector and writer. Behavioral meaning still needs
runtime validation. **DERIVED**

### 4.4 Common 200-byte view layout

The `LocalCam` parser writes the following common layout. Offset meanings are
supported by direct parser assignments and later consumers, but visual effects
are not claimed without runtime proof.

| View offset | Configuration field | Representation |
|---:|---|---|
| `+0x30/+0x34` | `Fov` | two floats |
| `+0x38/+0x3C` | `Fov` mirror | repeated pair |
| `+0x40/+0x44` | `ClipPlanes` | two floats |
| `+0x48` | `LODMultiplier` | float |
| `+0x4C/+0x50` | `Size` | two floats |
| `+0x54/+0x58` | `Center` | two floats |
| `+0x5C` | `MipmapLODBias` | float |
| `+0x60` | `Clear` | byte |
| `+0x64/+0x68/+0x6C` | `Color` | three integers |
| `+0x70` | internal camera type | `0` local, `1` static, `2` tracking |
| `+0x74/+0x78` | `Flags1` / `Flags2` | integers |
| `+0x80/+0x84` | `RadiusLimits` | two floats; local views |
| `+0x88/+0x8C/+0x90` | `OrientationRate` | three floats; local views |
| `+0x94/+0x98/+0x9C` | `PositionOffset` | three floats; local views |
| `+0xA0/+0xA4/+0xA8` | `OrientationOffset` | three floats; local views |
| `+0xAC` | `Radius` | float; local views |

Offsets after `+0x7C` are type-specific and must not be interpreted with the
local layout when the internal type is static or tracking. This is a necessary
safety rule for future generic tooling. **DERIVED**

The loader appears key-based and order-insensitive because it repeatedly asks
the config reader for named fields inside each block. The existing standalone
parser has round-trip tests for comments, negative values, precision, and fixed
slot classification. **CONFIRMED for the standalone parser; DERIVED for exact
game-parser tolerance**

### 4.5 Mode and target state machines

`GameActionStateMachine` accepts logical action identifiers rather than raw
view pointers:

| Action | Static effect |
|---:|---|
| `0`, `1`, `2` | select the local collection and dispatch local modes `0`, `2`, `1` respectively |
| `3` | request local mode `3`, with a guarded fallback to local mode `2` |
| `4` | request local mode `4`, the generic/onboard array |
| `5` | select tracking cameras when available, otherwise static cameras |
| `6` | no handled case; returns failure/default result |
| `7`, `8`, `9`, `10` | select local auxiliary modes `5`, `6`, `7`, `8` |

The non-monotonic mapping between logical actions and render modes is why a
future API should expose semantic enums and never invite mods to guess numeric
values. **DERIVED**

Manual actions are read from the game's pending-key queue by
`KeyboardInputActionMapper`. The observed switch supports changing targets and
requesting actions `0`, `3`, `4`, and `5`, subject to spectator/player locks.
An automatic director path instead chooses randomly among actions `0`, `3`,
`4`, and `5`, sometimes advances the race target, and resets a countdown.
**DERIVED**

Target resolution uses two complementary paths:

1. `Player`, `Original`, `Last`, `AnyTrackside`, and `None` receive special
   handling.
2. Other names are queried against the local, static, and tracking family
   objects in that order through a common virtual lookup method.

`CameraTargetRaceLeader` does not simply fetch position one. It asks a
finish-position helper for the adjacent target in a direction selected by its
boolean argument, then submits the resulting identity through the shared game
state dispatch. **DERIVED**

Player/spectator transitions are guarded by comparisons between the controlled
driver and watched driver. The manager can save a previous mode, fall back from
swingman-like views, and restore the saved mode when the target relationship
changes. Exact UI labels for these transitions remain a runtime item.
**DERIVED**

### 4.6 Per-frame data flow to the renderer

`MainCameraDirectorUpdate` performs the following static sequence:

1. Detect a watched-player change and update target-dependent audio/view state.
2. Run a short manager-state update.
3. Activate the initial local view on the first update.
4. Process either automatic-director timing or queued manual camera actions.
5. Apply a requested mode change through the selected family object.
6. Obtain world/session transform inputs and update the active camera target.
7. Submit position/orientation and speed-dependent camera information to a
   downstream camera/render object.

`CameraModeViewportUpdate` then selects one concrete view from render mode
`0..8`, invokes that view's virtual update method, reads the resulting position
and orientation, and submits them through the common transform funnel. Mode
`3` additionally clamps the first orientation-offset component to approximately
`[-2.95, -0.05]` before invoking the view's recomputation method. **DERIVED**

The selected view's positive scalar at `+0x48`—the parser's first `Fov`
component—is passed to `ConditionalFloatSetter`, which stores it in a
downstream projection object. `0x00542050` builds horizontal/vertical tangent
terms, aspect-ratio-dependent projection values, and projection matrices;
`CameraProjectionUpdate` submits projection and view matrices based on flag
bits. This establishes a static path from a selected view's FOV to projection
state, but the exact owning object and update timing still require reviewed ABI
and runtime correlation. **DERIVED**

`SceneRenderTransform` obtains scene/camera position and orientation, rebuilds
view transforms, invokes `CameraProjectionUpdate` with combined flags, traverses
render entries, and renders the scene. This is the downstream render boundary;
it should not be conflated with the configuration-layer view object. **DERIVED**

## 5. Discussion

### 5.1 Best immediate static deliverable

The highest-value implementation that does not require a running game is a
read-only, lossless camera configuration inspector supporting all three camera
families. It should:

- preserve unknown keys, comments, block ordering, and numeric precision;
- distinguish local, static, and tracking type-specific fields;
- enforce the 32 generic-local and 128 tracking staging limits as warnings;
- visualize activation spheres and movement paths without claiming in-game
  behavior;
- emit a semantic diff against the untouched source configuration;
- write only to a separately named staged or exported file.

The current `camcfg.py` deliberately models only `LocalCam`; expanding it
directly should wait for a separately specified preservation-aware parser so
unknown static/tracking data cannot be dropped.

### 5.2 SDK boundary implied by the architecture

A future safe SDK should separate:

1. immutable camera snapshots;
2. semantic mode requests;
3. semantic target requests;
4. staged configuration authoring;
5. optional render/overlay interfaces.

Raw manager offsets or action numbers should remain expert evidence, not the
public contract. Every runtime command needs target identity, game-thread
ownership, lifecycle checks, rollback/unload behavior, and second-session
tests.

### 5.3 Hard limits visible statically

- Generic non-reserved `LocalCam` staging: 32 entries.
- `TrackingCam` staging: 128 entries.
- Local mode application: nine concrete render modes (`0..8`).
- Logical action dispatcher: handled actions `0..5` except `6`, then `7..10`.

It is not yet proven whether file entries beyond the staging limits are ignored,
rejected, or partly parsed before being discarded.

### 5.4 Naming and trust hazards

Many current function names are `LEGACY`, and several parsers/serializers have
no reviewed public name. A decompiler label does not establish callability.
This paper intentionally describes data flow without promoting any function to
the SDK.

The shared 200-byte object constructor is currently named like an audio object,
which is inconsistent with its repeated camera allocation sites. This is a
classification lead, not authorization to rename the canonical symbol.

### 5.5 Relationship to prior camera projects

The existing camera editor already has a safe-source-file model and a tested
`LocalCam` parser. The existing free-camera work contains valuable prior live
evidence about the separation between a camera entry, transform funnel, and
final render camera. This static paper explains why those experiments found
multiple layers: they are distinct objects and stages in the original design.

### 5.6 Runtime verification backlog

When dynamic work becomes available, use a guarded full-weekend session to
answer these bounded questions:

1. What are the exact success/fallback semantics between `Cams.cfg` and
   `Default.cfg` in the config-loader vtable calls?
2. Do entries beyond 32 generic local cameras or 128 tracking cameras fail,
   truncate, or alter later parsing?
3. Which user-facing views correspond to each logical action and render mode,
   especially the unhandled action `6`?
4. Does `AnyTrackside` use a null sentinel intentionally, and how is the final
   target chosen?
5. Which common fields take effect immediately versus only after view/session
   reconstruction?
6. How does the selected view's `Fov` pair propagate into the final projection
   object, including aspect-ratio and clip-plane scaling?
7. What are the exact units and interpolation semantics for tracking rate,
   movement rate, activation radius, and movement-path points?
8. How do all three camera-family objects behave across garage, load, restart,
   spectator changes, session exit, and a second session?

## 6. Conclusion

Static analysis is sufficient to map the camera system's architecture and
configuration surface productively. The executable separates configuration
families, logical actions, target selection, view-object updates, and final
render projection. That architecture favors a lossless offline inspector now
and a semantic, lifecycle-safe SDK later.

The next static slice should map the static-camera collection loader and
movement-path element schema completely, then extend parser tests using copied
fixtures. No runtime claim is needed to deliver that value.

## Appendix A — Corrections to prior notes/tools

1. Existing summaries often describe `Default.cfg` as the sole camera loader.
   Static code shows an additional `Cams.cfg` path participating in manager
   initialization. Exact precedence is not yet proven and must remain open.
2. A generic 200-byte camera view must not be treated as one universal layout
   past offset `+0x7C`; local, static, and tracking types reuse that region for
   different fields.
3. The selected view's FOV is not statically disconnected from rendering:
   `CameraModeViewportUpdate` passes its positive FOV scalar into downstream
   projection state. The final object relationship remains unverified, but the
   previous “render path ignores the entry” wording is too absolute.

## References

- `F1 Challenge Online.exe` canonical decompiler corpus, current supported
  executable profile.
- `docs/README.md` — research evidence and status contract.
- `research/game_mechanics/09-complete-game/CRE-06-graphics-audio-ui-misc.md`
  — camera configuration overview.
- `research/game_mechanics/09-complete-game/ISI_F1C_TECHNICAL_DOCUMENTATION_EVIDENCE.md`
  — legacy camera-authoring evidence and verification backlog.
- `mods/sdk_sandbox/camera_editor/README.md` and `camcfg.py` — existing
  supported editor architecture and local-camera parser.
- `mods/sdk_sandbox/freecam/README.md` — preserved prior runtime investigation.

---
*Changelog: 2026-08-30 — initial static architecture map.*
