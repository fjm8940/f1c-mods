# HUD icons are font glyphs — adding a custom icon (e.g. an SC badge)

How F1C draws its HUD flag icons, and how to add a NEW icon (for example a
safety-car "SC" badge shown while the safety car is out) with no exe patch
and no new render code. Container format cracked and tooling built
2026-08-03; the only remaining step is a live in-game test.

Prior art: finding `f1c-hud-flag-icons-are-font-characters` (2026-08-02) found
the icons are characters of the FTingame01 bitmap font, not sprites. This
doc adds the container format, the tool, and the exact paint recipe.

---

## 1. The render path (why no new render code is needed)

`HUDFlagRendererMain` (0x4D82D0) builds a NUL-terminated byte buffer, one byte
per flag — yellow 62, black 63, blue 65, checkered 66, pit 102, plus
68/130/140/141 in mode `dword_7CFEA0 == 2` — and draws it with
`sub_553070(fontobj = *(0x8AD394), str, x = 10, y = 0, device = *(0x8B8DC8))`,
which loops `sub_552910` per character. Character code = 32 + cell index.

So a new icon = a new GLYPH in FTingame01 + appending its byte to that
buffer (or calling `sub_553070` yourself from pace_probe.dll). The font is
loaded from `Options/font.MAS` by `sub_4D8AB0` (names at `off_5D4A48`..`5D4A54`,
archive `off_5D4A5C = font.mas`).

## 2. The container: CUBEMAS4.10 (fully cracked 2026-08-03)

Verified against `Options/font.MAS` (31 records, 760,694 B) and
`SeasonData/Circuits/Pits/Normal/pitanims.mas` (52 records).

```
0x00  char magic[12]            "CUBEMAS4.10" (also 4.00 / 4.01)
0x0C  u32 = 0                   unknown, copy verbatim
0x10  u32 = directory size in 256-byte units (= record count)
0x14  u32 = total stored data size (sum of storedSize)
0x18  directory[count * 256]:
        +0x00 u32 unk0          copy verbatim (font.MAS rec0 = 0x12)
        +0x04 u32 unk1          copy verbatim
        +0x08 u32 rawSize       decompressed size
        +0x0C u32 storedSize    compressed size
        +0x10 u32 region        copy verbatim (per-group heap size)
        +0x14 char name[236]    e.g. "FTINGAME01.BMP"
  data: sequential zlib streams immediately after the directory.
        entry i at 0x18 + count*256 + sum(storedSize[0..i-1]).
        file size == 0x18 + count*256 + sum(storedSize) exactly.
```

Notes, hard-won:

- **Entries are plain zlib** (0x78 0x9C), compatible with Python's `zlib`.
  The game's inflate is `sub_5357E0` (deflate method-8 check, window check,
  `% 0x1F` header checksum), wrapped by `sub_59B260`.
- **There is NO offset field.** Data blocks are contiguous; each entry's
  position is the running sum of previous stored sizes. Verified every step:
  first stream at 7,960 = 24 + 31×256.
- **`rawSize` aliases** across same-size fonts (all 14/16 pt fonts share
  251,928) — that made an earlier pass read it as a "file offset". It is the
  decompressed size; `region` (≈3.3 MB) is a per-group heap size.
- The dword at file offset 24 (0x12 in font.MAS) is **record 0's unk0**, not
  a directory length. The record count lives at offset 16.
- Game side: `sub_55FC40` (open + header), `sub_55FBE0` (ctor),
  `MeshVertexIndexBufferLoader_00550ad0` (entry loader; directory at obj+296,
  entry index obj+292<<8, sizes at record+8/+12).

## 3. The glyph grid of FTINGAME01.BMP

Extracted entry 2: **331×469, 24-bit, row stride 996** (467,180 B
decompressed, 41,645 B stored).

- Grid: **10 cols × 12 rows of 33×39 px cells**, `maxcode = 10*12+32 = 152` —
  matches the live-verified font object (+52..+68 = 33/39/10/12/152).
- Cell `i` top-left = `(1 + (i % 10) * 33, 1 + (i // 10) * 39)`; glyphs use
  the FULL cell (edge to edge — cell 0's right edge x=33 carries glyph
  pixels; there is no vertical separator).
- Cell bottom rows (y = 39, 78, 117, 156) are painted a solid colour (gray
  86,86,86 in the used region) — the "separator" lines. The game derives
  cellW/cellH from its CONVERTED 16-bit texture via `sub_552BA0`'s marker
  scan (marker pixel at (220,467)), which cannot be replicated from the raw
  24-bit file.
- The unused region (grid rows 5–11, cells 50–119) is a uniform magenta
  (243,0,22) = the transparency key.
- **Free cells: 110–119 (codes 142–151)** — verified unreferenced by every
  code path.

## 4. The tool: tools/font_mas_tool.py

```
python tools/font_mas_tool.py list    <font.MAS>
python tools/font_mas_tool.py extract <font.MAS> <entry> [outdir]
python tools/font_mas_tool.py scan    <font.bmp>
python tools/font_mas_tool.py paint   <font.bmp> <cell_idx> <icon.png> <out.bmp>
python tools/font_mas_tool.py repack  <font.MAS> <entry> <new.bmp> <out.MAS>
python tools/font_mas_tool.py verify  <font.MAS>
```

`entry` = 0-based index or case-insensitive name. `paint` needs Pillow; the
rest is stdlib. `scan` derives cellH from the separator rows and validates
against the live-verified constants (33×39, 10×12, maxcode 152).

Verified (off-game):

- `verify` — all 31 entries decompress to exactly rawSize; FTINGAME01 grid
  intact.
- Round-trip — extract all → repack unchanged → 31/31 streams
  byte-identical; file size exact; unk/region fields preserved; header
  total updated correctly.
- SC badge — `paint` cell 110 (black pixels → magenta transparency key),
  `repack` entry 2 → `verify` OK → `extract` shows 3,802 changed bytes in
  cell 110 (rows 430–468, cols 1–33), grid scan unchanged, marker pixel
  (330,468) untouched.

## 5. Recipe: the safety-car "SC" badge

1. `extract font.MAS FTINGAME01` (game's `Options/font.MAS`).
2. Make a 33×39 icon — black background, white "SC" — `paint ... 110 ...`.
3. `repack font.MAS 2 FTINGAME01_SC.BMP font_sc.MAS`, back up the original,
   copy `font_sc.MAS` over `Options/font.MAS`.
4. In pace_probe.dll, when the safety car deploys, draw byte 142 through
   the game's own text renderer:
   `sub_553070(*(0x8AD394), "\x8E", x, y, *(0x8B8DC8))` — or append 142 to
   the buffer `HUDFlagRendererMain` builds (a 1-byte patch to the renderer,
   gated on SC state).
5. Char code 142 = cell 110 = the new glyph, automatically — the grid is
   image-derived, nothing else to register.

## 6. Capacity beyond the free cells

The free cells are enough for ~10 badges. Beyond that the same BMP can be
enlarged (add cells) — nothing is hardcoded — subject to three caps:

1. Font object = 2,164 B (`OperatorNewNhMallocWrapper(2164)` in
   `sub_53CFE0`); glyph table at +104, 8 B/cell → ≤ 257 cells.
2. Char code is a **byte** (`sub_552910` takes `unsigned __int8 a2`) →
   codes 32..255 only → ≤ 223 glyphs per font, ever.
3. Texture width ≤ 2048 px (`sub_552470`'s ladder returns -1 above 2048).

Current: 120 cells, 109 used → ~114 icons fit before any cap. Sibling fonts
(FTFU30WH, FTim24hud, FTim24wh) are separate objects with their own cells.

## 7. Status

| Step | State |
|------|-------|
| Container format | CRACKED + repack-verified |
| Compression | zlib, game inflate identified |
| Glyph geometry | verified against live font object |
| Tool | built, round-trip + end-to-end verified |
| **In-game live test** | **PENDING — do not ship before it** |

Live test: copy the repacked font.MAS over the game's (backup first), drive
the race, draw char 142 from pace_probe.dll while the safety car is out, and
have the driver confirm the SC badge on screen. Findings: `f1c-cubemas410-
container-format-fully-cracked`, `f1c-cubemas-entries-are-zlib`,
`f1c-ftingame01-glyph-grid-geometry`, `f1c-font-mas-tool-built`.
