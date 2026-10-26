# How F1 Challenge positions sound, and why the wheel-gun WAVs were too loud — 2026-09-28

Research record for the Pit Stop Failures wheel-gun audio. Evidence tiers:
**FACT** = read directly from the binary strings, the decompiled corpus or a
file on disk; **DERIVED** = computed from facts; **INFERENCE** = reasoned but
not observed live.

## 1. The game has no hardware or Miles 3D audio

- **FACT.** The game EXE (`F1 Challenge V10\modified f1 challenge 99-02.exe`)
  imports only the 2D Miles Sound System sample API: `AIL_set_sample_volume_levels`,
  `AIL_set_sample_playback_rate`, `AIL_start_sample` and similar.
  It imports no `AIL_open_3D_provider` / `AIL_set_3D_*` function. It does not
  use DirectSound3D either. (`strings` on the EXE.)
- **FACT.** Positional audio is done in software by
  `F_3DSoundPositioningUpdate` at `0x00405B90`
  (`f1c_ida_decompiled/functions/F_3DSoundPositioningUpdate_00405b90.c`):
  1. It computes the distance from the sound source (`this+0x28..0x30`) to the
     listener object (`*(this+0x54)+0x58..0x60`).
  2. It calculates attenuation with `0x00406020`:
     `x = a*d + b`. If `x < 1`, the level is `base`. Otherwise it is
     `base * (1/x + (1/x³ − 1/x) * k)`. The fields are `a=this[7]`, `b=this[8]`,
     `k=this[9]` and `base=this[16]` (`+0x40`). This is an inverse-distance law
     with a full-volume radius. `k` blends it toward a steeper 1/x³ curve.
  3. It calculates pan with `0x00406060`:
     `pan = (dot(dir, listener_right)*0.6 + 1) * 0.5`, which ranges from 0.2 to 0.8.
     The two levels are `(1-pan)*att` and `pan*att`. **A centred source reaches
     each channel at 0.5 × its attenuated level.**
  4. It applies Doppler with `0x00405F80`, using a speed of sound of 330 m/s. The
     rate is clamped to 0.125–8.
- **FACT.** In the V10 install, the `VS_*WHEELGUN*` / `VS_*AIR_WRENCH` lines in
  `SeasonData/Vehicles/f1sounds.sfx` are commented out. No `WHEELGUN` string
  exists in the EXE. The only `VS_` keys the EXE formats are engine, shift,
  road, skid, hit, horn, limiter, TC and wind sounds. **The engine has no native
  wheel-gun slot to reuse.** These lines are rFactor-style leftovers.
- **Unresolved.** This static pass did not identify the per-sound constants
  `a/b/k/base` for pit or ambient sounds. A live read of a sound-source
  object would provide them.

## 2. The player's sliders

- **FACT.** `PlayerOptionsInitializer` at `0x004AA270` defines the `.PLR`
  `[ Sound Options ]` values as fields of the options object. The current
  value offsets are: SoundFX Volume `+3636`, Engine `+3672`,
  Player Volume Ratio `+3744`, Other Volume Ratio `+3780`, Pit Volume `+3852`,
  Crowd Range/Shape/Ambient `+3924/+3960/+3996`. **Unresolved:** the global that
  points to the live options object has not been found (only one caller is known,
  `UIControlWindowInitialization 0x004A70E0`).
- **FACT.** In the owner's profile `Save\Matias\Matias.PLR`:
  `SoundFX Volume=0.38537`, `Player Volume Ratio=1.0`,
  `Other Volume Ratio=0.40`, `Pit Volume=0.50`.

## 3. What shipped 1.0.3 does instead

- **FACT.** In `release_candidates/pitstop_failures-v1.0.3-rc.1/source`, the
  `legacy_audio.h` and `wheel_audio_spatial.h` files play bursts through eight
  private `waveOut` voices. This path is outside the game's Miles mixer, so no
  game slider affects it.
- **FACT.** The level is 1.0 (full scale) when the source is the spectated car.
  This also covers the player's own stop. Otherwise the level is
  `((80-d)/72)²` for d > 8 m, and 1.0 for d ≤ 8 m. Distance is measured from the
  spectated car's position, not from the camera. No pan is applied.
- **FACT.** The WAVs are mastered like native effects. `wheelguns_1..3.wav` are
  48 kHz stereo with a peak of 0 dBFS and an RMS of −10 to −15 dB.
  `hit1_wall.wav` and `DOWNSHIFT_*.wav` are also at 0 dBFS peak. **The assets
  are not the problem; the gain chain is.**
- **DERIVED.** With the owner's sliders, a centred native effect on the
  player's car plays at 0.385 × 0.5 ≈ −14 dB. On an AI car it plays at
  0.385 × 0.4 × 0.5 ≈ −22 dB before distance. The shipped wheel guns play at
  0 dB, so they are **about 14–22 dB louder than the game's own effects**.
  A car driving past on the straight about 20 m from the box still received
  −3 dB. This matches the owner's report.

## 4. Consequences for the mod

The wheel guns should follow the same chain the game uses: the SoundFX slider,
the player or other-car ratio, a centred-pan level of 0.5, inverse-distance
attenuation beyond a crew-scale radius, and masking when the listener is on
the circuit. The implemented candidate is described in
`verification/wheelgun-audio-2026-09-28/REPORT.md`.

Possible later improvements (not implemented):

- **Stereo pan.** The snapshot has no heading, only `orientation_up_dot`, so
  true left/right panning needs a heading or camera source.
- **Camera listener.** The camera position is not exposed by the broker; see
  `f1c_ida_decompiled/docs/50-camera-system-static-architecture.md`.
- **Live slider reads.** Reading the options-object global would replace the
  current newest-`.PLR` heuristic.
