# Automatic camera profiles

This sandbox DLL applies camera profiles after F1 Challenge creates its camera
objects. It never edits `Save\Default.cfg`.

Profile precedence is:

```text
Save\Default.cfg
Save\Default.camera_editor.cfg
CameraProfiles\Seasons\1995\Default.camera_editor.cfg
SeasonData\Vehicles\Ferrari\1995_412T2\camera_editor.profile.cfg
CameraProfiles\Drivers\1995_Ferrari27\Default.camera_editor.cfg
```

The season and vehicle are inferred from the player vehicle's loaded `.veh`
path. For example, a vehicle under `SeasonData\Vehicles\Ferrari\1995_412T2`
uses season `1995` and vehicle key `1995_412T2`. Vehicle-level profiles are
kept beside the corresponding `.veh` files, not in a central folder.

A **Current driver** profile uses the exact `.veh` filename without its
extension (for example `1995_Ferrari27`). It is loaded after the vehicle
profile, so it changes one driver without affecting a teammate in the same car.

Create the profile directories under the game root. Each profile file uses the
same `LocalCam` format as `Save\Default.cfg`; only live camera fields are
applied. `Clear`, `Color`, `Flags1`, `Flags2`, and `RadiusLimits` remain
file-only and are ignored by this first runtime layer.

## ONBOARD view manager

The GUI's **Manage ONBOARD views...** button manages a separate, scope-local
visibility file beside the selected profile. It can disable an inherited
ONBOARD view or clone the selected ONBOARD into a new view. These operations
take effect when F1 Challenge starts its next on-track session: the injected
DLL writes a temporary merged config and lets the game allocate its normal,
game-owned camera array from it. It does not resize the live array and it never
edits `Save\Default.cfg`.

Without the DLL, the temporary file is ignored and the game uses the untouched
`Save\Default.cfg` exactly as usual. With the DLL injected, layer precedence
remains global → season → vehicle → driver for both camera values and ONBOARD
visibility.

Build and stage with:

```powershell
python tools/f1c_mod.py build mods/sdk_sandbox/camera_profiles
python tools/f1c_mod.py stage mods/sdk_sandbox/camera_profiles "C:\path\to\F1 Challenge V10" --exe "C:\path\to\F1 Challenge V10\F1 Challenge Online.exe" --replace-sdk-owned
```

Inject the staged DLL using the existing game injector. The runtime log is
`camera_profiles_log.txt` beside the DLL. Unload cooperatively before staging
or replacing the DLL.

This is the first runtime slice: profile selection is automatic by season and
vehicle, while the GUI profile-management workflow will be added next.
