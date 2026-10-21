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

## GS2 / GSF sprite sets: partial

`GS2` is a set of character sprites (e.g. `verid1/001.gs2` is a crew member).

```
uint32  2 (GSF files have 1 here)
uint32  frame count                 (25 in verid1/001.gs2)
uint32  0
uint32  sequence count              (16)
uint32  0
uint16  ?  uint16 ?                 (128, 256 or 256, 128)
sequences[sequence count]:
  uint8 n, uint8 frame index[n]     (verid1: [0..4], [], [5..19], [20..24], then 12 empty)
frames: header such as "9c 00 00 4e 00 00 00 00 01 00 00" (156 and 156/2),
        then pixel data with 0xFF as transparent
```

The pixel data is stored **column-major**: rendering it 256 bytes wide shows
the sprite lying on its side, as in Doom-style engines. The columns drift by
a byte now and then, so there's per-column length or offset information still
to find. The 16 sequences are possibly 8 directions × 2 states.

## SGEN script (`GAME.BIN`): unknown

```
char[4]  "SGEN"
uint16   22      (version?)
uint16   2
uint32   file size - 12
uint32   ?       (43938 in verid1; the size of a code section?)
uint32   8, 444, 4616, 19208, 1300, 96   (section sizes or counts?)
```

It ends with a table of the resources the location uses (`KIRKHIT.OVR`,
`PHASER.BMP`, `TRICRDR.BMP`, ...). The overlay frame heights are probably
set in here.

## Level data (`GAME.LV2`): unknown

It starts with a table of dwords (`1, 1024, 8, 292, 152, 140, 276, 196, 64, 24, ...`)
and contains long runs of the bytes `0x33`–`0x35` (the ASCII digits `3`–`5`). These look
like height, light or texture maps.
