"""Address-range subsystem classifier for F1 Challenge Online.exe.

Two-tier classification:
  1. Confirmed ISI class from cross_binary_final.csv (authoritative — NASCAR match)
  2. Address-range heuristic (fallback) derived from confirmed anchors

Ranges are derived from confirmed function identities:
  - AIDatabase::Init @ 0x4040a0, AI corner speed @ 0x419500  -> AI band
  - Game::* @ 0x42d370, DynMan::* @ 0x42c130               -> game/dynman band
  - tire_manager funcs @ 0x4d4a80+, TireGripCoefficient @ 0x4e88b0
"""

# (start, end, subsystem, path_class)
# path_class: 'ai' | 'player_physics' | 'shared_physics' | 'render' | 'net' | 'ui' | 'audio' | 'engine_lib' | 'game'
RANGES = [
    (0x00401000, 0x00420000, "ai_db",           "ai"),
    (0x00420000, 0x0042b800, "game/dynman",      "game"),
    (0x0042b800, 0x00430000, "dynman/setup",     "shared_physics"),
    (0x00430000, 0x0043e000, "hdvehicle/vehload","player_physics"),
    (0x0043e000, 0x00450000, "garage/vehgfx",    "player_physics"),
    (0x00450000, 0x00460000, "hwinput/onscreen", "ui"),
    (0x00460000, 0x00470000, "netcomm/replay",   "net"),
    (0x00470000, 0x00490000, "plrfile/options",  "ui"),
    (0x00490000, 0x004a0000, "onscreen/gizmo",   "ui"),
    (0x004a0000, 0x004b0000, "osman/pitcrew",    "game"),
    (0x004b0000, 0x004c0000, "render/steward",   "shared_physics"),
    (0x004c0000, 0x004d0000, "sound/specialfx",  "audio"),
    (0x004d0000, 0x004e0000, "steward/tire_mgr", "player_physics"),
    (0x004e0000, 0x00500000, "tire/dynamics",    "shared_physics"),
    (0x00500000, 0x00700000, "engine_library",   "engine_lib"),
]


def classify_by_range(addr):
    """addr: int VA. Returns (subsystem, path_class)."""
    for start, end, subsystem, path_class in RANGES:
        if start <= addr < end:
            return subsystem, path_class
    return "unknown", "unknown"


# Source file -> path_class, for when we have a confirmed ISI class
CLASS_PATH = {
    "AIDatabase": "ai",
    "DynMan": "shared_physics",
    "TireManager": "shared_physics",
    "HDVehicle": "player_physics",
    "Steward": "game",
    "Game": "game",
    "CamMan": "render",
    "Render": "render",
    "VidMan": "render",
    "SpecialFX": "render",
    "Sound": "audio",
    "HWInput": "ui",
    "OnScreen": "ui",
    "Options": "ui",
    "OSMan": "game",
    "PitCrew": "game",
    "PlayerFile": "game",
    "NetComm": "net",
    "Setup": "game",
    "CMpeg": "engine_lib",
    "Cube/DX8": "engine_lib",
    "Cube/Texture": "engine_lib",
}


def classify(addr, cross_binary_row=None):
    """Prefer confirmed class; fall back to address range.
    Returns dict: {subsystem, path_class, identity, source}.
    """
    if cross_binary_row and cross_binary_row.get("class"):
        cls = cross_binary_row["class"]
        path = CLASS_PATH.get(cls)
        if path:
            method = cross_binary_row.get("method", "")
            ident = f"{cls}::{method}" if method else cls
            return {
                "subsystem": cls,
                "path_class": path,
                "identity": ident,
                "source": "confirmed_class",
            }
    subsystem, path_class = classify_by_range(addr)
    ident = ""
    if cross_binary_row and cross_binary_row.get("name"):
        ident = cross_binary_row["name"]
    return {
        "subsystem": subsystem,
        "path_class": path_class,
        "identity": ident,
        "source": "address_range",
    }
