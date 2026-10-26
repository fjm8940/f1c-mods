"""
session_setup.py — choose the track/session by FILE EDIT, not by clicking menus.

WHY: menu navigation needs posted mouse clicks at screen coordinates, which are
fragile (they move with resolution, and screenshot pixels are not client pixels).
Menu selection also cannot be driven by memory writes — the UI's live selection
lives in heap widget objects; the globals that change on selection
(`CurrentTrackPath` 0x7CE5D0, `SettingsFilePath` 0x7CE5EC, `CurrentSceneIndex`
0x8AC48C) are RECORDS the game writes, and poking the index is ignored (tested).

What the menu actually does is write the player file. So write it directly:

    [ SCENE ]
    Scene File="SEASONDATA\\CIRCUITS\\<country>\\<track>\\<track>.SCN"
    Scene Description="<track>"
    AI Database File="...<track>.AIW"

Combined with `tools/lab.py autolaunch quickrace` this is a complete, click-free,
resolution-independent way to bring the game up on a chosen track — the intended
path for unattended research. See tools/GAME_CONTROL.md.

SAFETY: byte-exact backup on first touch (verified by hash), refuses to run if
the backup cannot be made, and `restore` puts it back. Only the [ SCENE ] lines
are rewritten; nothing else in the file is touched.

USAGE
-----
  python tools/session_setup.py list                      # tracks found on disk
  python tools/session_setup.py show                       # current selection
  python tools/session_setup.py set 2001_Monza             # by folder name
  python tools/session_setup.py set Monza --dry-run
  python tools/session_setup.py restore
"""
import hashlib
import io
import os
import re
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "..", "F1 Challenge V10")
PLR = os.path.join(GAME, "Save", "Matias", "Matias.PLR")
CIRCUITS = os.path.join(GAME, "SeasonData", "Circuits")
BACKUP_SUFFIX = ".f1c-session-setup.bak"


def sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def ensure_backup(path):
    bak = path + BACKUP_SUFFIX
    if not os.path.exists(bak):
        shutil.copy2(path, bak)
        if sha(bak) != sha(path):
            sys.exit("ABORT: backup does not match the original byte-for-byte")
        print(f"  backup created: {bak}")
    return bak


def find_tracks():
    """Every .SCN on disk -> (folder_name, scn_path_relative_to_game)."""
    out = {}
    if not os.path.isdir(CIRCUITS):
        sys.exit(f"circuits dir not found: {CIRCUITS}")
    for country in sorted(os.listdir(CIRCUITS)):
        cdir = os.path.join(CIRCUITS, country)
        if not os.path.isdir(cdir):
            continue
        for track in sorted(os.listdir(cdir)):
            tdir = os.path.join(cdir, track)
            if not os.path.isdir(tdir):
                continue
            for f in os.listdir(tdir):
                if f.lower().endswith(".scn"):
                    rel = os.path.join("SEASONDATA", "CIRCUITS", country, track, f)
                    aiw = os.path.splitext(rel)[0] + ".AIW"
                    aiw_abs = os.path.join(GAME, aiw)
                    if not os.path.exists(aiw_abs):
                        cand = [x for x in os.listdir(tdir) if x.lower().endswith(".aiw")]
                        aiw = (os.path.join("SEASONDATA", "CIRCUITS", country, track, cand[0])
                               if cand else None)
                    out[track] = (rel, aiw, country)
                    break
    return out


def read_scene(text):
    def grab(key):
        m = re.search(rf'^{re.escape(key)}\s*=\s*"([^"]*)"', text, re.M)
        return m.group(1) if m else None
    return {k: grab(k) for k in ("Scene File", "Scene Description", "AI Database File")}


def cmd_show():
    text = io.open(PLR, encoding="latin-1").read()
    cur = read_scene(text)
    print(f"  {PLR}")
    for k, v in cur.items():
        print(f"    {k:20s} = {v!r}")


def cmd_list():
    tracks = find_tracks()
    print(f"  {len(tracks)} tracks with a .SCN under SEASONDATA\\CIRCUITS:")
    for name, (rel, aiw, country) in sorted(tracks.items()):
        flag = "" if aiw else "   [no .AIW!]"
        print(f"    {name:28s} ({country}){flag}")


def cmd_set(query, dry_run):
    tracks = find_tracks()
    matches = {k: v for k, v in tracks.items() if query.lower() in k.lower()}
    if not matches:
        sys.exit(f"no track matching {query!r} — try: python tools/session_setup.py list")
    if len(matches) > 1:
        print("  ambiguous, matches:")
        for k in sorted(matches):
            print(f"    {k}")
        sys.exit(1)
    name, (rel, aiw, country) = next(iter(matches.items()))
    if not aiw:
        sys.exit(f"{name} has no .AIW next to its .SCN — the game needs one")

    text = io.open(PLR, encoding="latin-1").read()
    before = read_scene(text)

    def put(t, key, value):
        pat = rf'^({re.escape(key)}\s*=\s*")[^"]*(")'
        new, n = re.subn(pat, lambda m: m.group(1) + value + m.group(2), t, count=1, flags=re.M)
        if n != 1:
            sys.exit(f"could not rewrite {key!r} in the .PLR (found {n} matches)")
        return new

    text = put(text, "Scene File", rel)
    text = put(text, "Scene Description", name)
    text = put(text, "AI Database File", aiw)

    print(f"  {name}  ({country})")
    for k, v in read_scene(text).items():
        print(f"    {k:20s} {before[k]!r}\n{'':24s}-> {v!r}")

    if dry_run:
        print("  --dry-run: nothing written")
        return
    ensure_backup(PLR)
    with io.open(PLR, "w", encoding="latin-1", newline="") as f:
        f.write(text)
    print(f"  written. Arm unattended boot with: python tools/lab.py autolaunch quickrace")


def cmd_restore():
    bak = PLR + BACKUP_SUFFIX
    if not os.path.exists(bak):
        sys.exit("no backup to restore")
    shutil.copy2(bak, PLR)
    print(f"  restored {PLR} from backup")


def main():
    if not os.path.exists(PLR):
        sys.exit(f"player file not found: {PLR}")
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return
    cmd = args[0]
    if cmd == "show":
        cmd_show()
    elif cmd == "list":
        cmd_list()
    elif cmd == "set":
        if len(args) < 2:
            sys.exit("set needs a track name")
        cmd_set(args[1], "--dry-run" in args)
    elif cmd == "restore":
        cmd_restore()
    else:
        print(f"unknown command '{cmd}'")
        print(__doc__)


if __name__ == "__main__":
    main()
