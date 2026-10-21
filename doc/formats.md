# Star Trek: Generations data formats

Notes from reverse engineering the 2 CD Windows release. "Decoded" means a
decoder exists in `engines/stg` and has been checked against real files.
"Partial" and "unknown" are working notes. All values are little-endian.

## Containers

| File | Format | Notes |
|------|--------|-------|
| `DATA/*.DAT`, `GENS/*.DAT` | ZIP (stored + deflated) | One archive per location, plus `COMMON.DAT`, `OPTIONS.DAT`, `COMBAT.DAT`, `STELLAR.DAT`. Read by `common/zip.h`. |
| `GENS/MUSIC.1`, `MUSIC.2` | ZIP (stored) | 196 stereo 22 kHz WAV music tracks, including `WIN*`, `LOSE*`, `TIMEOUT*`, `LOOP*`, `CREDITS`. |
| `GENS/MOVIE/*.AVI` | AVI | Cinepak video, 640x320 at 24 fps, with 22 kHz 16-bit stereo PCM audio. |

Each location archive (e.g. `VERID1.DAT`) holds a `verid1/` folder with
`GAME.BIN`, `GAME.LV2`, `*.GS2`, `*.BMP`, `*.OVR` and `sounds/*.WAV`.

## Standard formats inside the archives

- **BMP:** 8-bit Windows bitmaps, each with its own palette.
- **WAV:** 16-bit PCM. Speech is 22 kHz mono; music is 22 kHz stereo.
- **TXT:** for each speech sample `sounds/XXXX0000.WAV` there is a
  `sounds/XXXX0000.TXT` subtitle, e.g. `RIKER: "..."`. Some `.TXT` files are
  other things: `sounds/worlds.txt` is a list of WAV names, and `BRIEFING.TXT`
  is a mission briefing.

## FONT (`.FNT`): decoded, `engines/stg/font.h`

```
char[4]  "FONT"
uint32   size of the rest of the file
uint16   glyph count
uint8    transparent colour index
uint8    line height
uint32   0
glyph[]:
  uint16 character code
  uint16 width * height + 2
  uint8  width
  uint8  height              (top aligned, may be < line height)
  uint8  pixels[height][width]
```

## OVERLAY (`.OVR`): decoded, `engines/stg/overlay.h`

These are first-person animations, such as the player's fist (`KIRKHIT.OVR`) or a hand holding an
item (`NUPLANT.OVR`).

```
char[8]  "OVERLAY\0"
uint32   width
uint32   row count (all frames stacked vertically)
uint32   row offsets[row count], relative to the end of this table
rows: int16 runs until the row reaches `width`
  n < 0: skip -n transparent pixels
  n > 0: n palette indices follow, padded to an even byte count
```

Every row of both samples decodes to exactly the width. The **frame height
is not stored**. It looks like 252 for the 504- and 640-wide overlays and 88 for
NUPLANT (152 wide), so it presumably comes from the game script. There's no
palette either; the overlay uses the palette of the current scene.

## GS2 sprite sets: decoded, `engines/stg/sprite.h`

These are billboard sprites for the 3D away missions: `verid1/001.gs2` is a crew member
(standing, walking, beaming in), and `008.gs2` is a flame.

```
uint32  2                           version (GSF files have 1, see below)
uint32  frame count                 (25 in verid1/001.gs2, 11 in 008.gs2)
uint32  0
uint32  sequence count              (16 in both)
uint32  0
uint16  ?, uint16 ?                 (128, 256 / 64, 128: a size hint)
sequences[sequence count]:
  uint8 n, uint8 frame index[n]     (001: [0..4], [], [5..19], [20..24], 12 empty)
frames[frame count]:
  uint32 world width, 24.8 fixed point (00 9c 00 00 = 156.0 for 78 columns)
  uint32 column count
  uint32 column height              (256 in 001, 128 in 008)
  uint8  pixels[columns][height]    column-major, BOTTOM row first, 0xFF transparent
  uint8  tables[3][columns]         per-column hints; the first is twice the
                                    topmost opaque row. Not needed to draw.
```

Both samples parse to exactly their end with the advertised frame count.
The world width is always 2 × columns for 256-high frames and 4 × columns for
128-high ones, i.e. columns × 512 / height. The sequences are probably
animation states, possibly per direction. As with overlays, there's no palette.

## GSF (`.GSF`): unknown

These start like GS2 but with version 1:
`01 00 00 00 33 00 00 00 f8 06 00 00 10 00 00 00 18 00 00 00 80 00 00 01 ...`
(51 frames, with something in the third dword). They're refused by the GS2
decoder rather than misread.

## SGEN script (`GAME.BIN`): container decoded, bytecode not yet

This is the compiled script for each location. `engines/stg/script.h` reads the container:

```
char[4]  "SGEN"
uint16   22 (version)
uint16   2
uint32   file size - 12
uint32   code size in bytes          (43938 in verid1)
uint32   section sizes[6]            (8, 444, 4616, 19208, 1300, 96 in verid1)
uint16   code[]                      starts at offset 40
uint8    sections[6][]
char[]   resource names, NUL separated, to the end of the file
```

The sizes account for every byte of verid1's file, and AMARGOS1's header
adds up the same way.

What's known so far about the contents:

- **Code** is a 16-bit word stream for a stack machine. `0002 n` pushes `n`
  (5,315 of the 21,969 words in verid1 are pushes). It starts with
  `0001 e3e7 8000`, then `push -1, push 32, op 0x15`, then
  `push 0, push N, op 0x19` for N = 0, 1, 2, ..., which looks like
  initialising variables. Other instructions take operands too, so operand
  counts can't be inferred from the data alone. The interpreter in `sgens.exe`
  is needed.
- **Section 1** has 12-byte records holding values like `0x00424A38`, which are
  addresses inside `sgens.exe` (image base 0x400000).
- **Section 2** starts with `0x12345678` and contains `0x87654321`
  markers.
- **Resource names** are the BMPs and OVRs the location uses (`PHASER.BMP`,
  `KIRKHIT.OVR`, ...).

## Level data (`GAME.LV2`): unknown

It starts with a table of dwords (`1, 1024, 8, 292, 152, 140, 276, 196, 64, 24, ...`)
and contains long runs of the bytes `0x33`–`0x35` (the ASCII digits `3`–`5`). These look
like height, light or texture maps.
