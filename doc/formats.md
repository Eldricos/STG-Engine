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

## SGEN script (`GAME.BIN`): decoded, `engines/stg/script.h`, `vm.h`

This is the compiled script for each location. Everything below comes from `sgens.exe`: the loader at
`0x45F160`, the object factory and code scanner at `0x43519F`, `SendEvent` at
`0x44F4E1`, and the interpreter at `0x44F9D4` (a 245-case switch at `0x455A28`).

### Container

```
char[4]  "SGEN"
uint16   22 (version; the loader rejects anything else)
uint16   2
uint32   file size - 12
uint32   code size in bytes
uint32   section sizes[6]
uint16   code[]                  starts at offset 40
uint8    sections[6][]
char[]   resource names, NUL separated, to the end of the file
```

- **Section 0** is a list of `(int16, int16)` pairs ending in −1. The level
  loader passes each pair to a game function (`0x4B5220`).
- **Section 1** is the resource table: 12-byte records, one per name. At load
  time the third field is overwritten with a pointer to the name (the stale
  `0x00424A38`-style values in the file are leftovers).
- **Sections 2 and 3** go to a level sub-object (`0x41EA4B` / `0x41EB62`).
  Sections 4 and 5 are unexplored.

### Program

```
program := object* 0x1000
object  := type param state+ 0x2000      param is the object's id
state   := (0x8000 | id) block+ 0x3000
block   := event next-state instruction* 0x0000
```

Object types 1, 2, 3, 4, 5, 6, 7 and 8 each create a different C++ class;
verid1 uses types 1, 2, 4, 7 and 8 (178 objects, 658 states, 1010 blocks).
The grammar consumes exactly all 21,969 words of verid1's code.

### Execution

Each object is a state machine. It starts in state −1. When an object
receives an event, the block registered for
`((event & 0xFF) << 8) | (state & 0xFF)` runs. If the object has no such
block, the parent object is searched next, then built-in default behaviour.
When the block finishes, the object switches to the state set by
`GOTO_STATE` or `CASE`, or else to the block's next state (−1 means stay
put). Each state change sends **event 3 (leave)** to the old state, then
**event 2 (enter)** to the new one.

A block runs on its own 16-bit stack. Event arguments are pushed last to
first, so the first argument is on top. The block's result starts at 0 and
is set with `SET_RESULT`.

### Instruction set

Every instruction is one word, except `PUSH n` (2 words), `PUSH32` (3) and
`PUSH_BLOCK` (8). Binary operations compute **top op below**: for example,
`PUSH 10, PUSH 3, SUB` gives −7.

| Op | Name | Effect |
|----|------|--------|
| 00 | END | end of block |
| 01 | NOP | |
| 02 n | PUSH | push n |
| 03 | POP | discard top |
| 04 | PUSH_SELF | push this object's id |
| 05 ×7 | PUSH_BLOCK | push the seven operand words |
| 06 ×2 | PUSH32 | push two operand words |
| 07 | SET_RESULT | pop into the block's result |
| 08 | GOTO_STATE | pop the state to enter when the block ends |
| 09 | GAME_CALL_2 | pop two values and pass them to the game |
| 0A | CASE | pop c b a; push a; if a == c, go to state b and end the block |
| 0B–10 | IF_EQ/NE/GT/GE/LT/LE | pop b a; push a; if the test fails, skip to the matching ELSE/ENDIF |
| 11 | ENDIF | |
| 12 | ELSE | skip to the matching ENDIF |
| 13 | RETURN | end the block |
| 14 | ALLOC_LOCALS | pop n; allocate n locals |
| 15 | GAME_INIT | pop one value for the game |
| 16 / 17 | GET_LOCAL / GET_GLOBAL | pop i; push the variable |
| 18 / 19 | SET_LOCAL / SET_GLOBAL | pop i, pop v; variable[i] = v |
| 1A / 1B | INC / DEC | |
| 1C–1F | ADD SUB MUL DIV | top op below |
| 20 | SIGN_DIFF | sign(top − below) |
| 21–24 | AND OR XOR NOT | |
| 25 / 26 | HIGH_BITS / LOW_BITS | top & 0xE000 / top & 0x1FFF (object ids look like `type << 13 | n`) |
| 27–F4 | NATIVE_xx | calls into the game |

The native opcodes' stack effects were read from each handler and are in
`engines/stg/opcodes.cpp`, with notes on which game method each calls. Run
through every block of verid1, those effects never underflow the stack
except in a few blocks that take more than two event arguments.

To get a full listing of a script:

```sh
stg --extract --path=DATA/VERID1.DAT --pattern='verid1/game.bin' -o out
stg --probe --path=out/verid1/game.bin
```

## Level data (`GAME.LV2`): partly decoded, `engines/stg/level.h`

Loaded by the routine at `0x4BD130` in `sgens.exe`. All values are little endian.

| Offset | Type | Meaning |
|--------|------|---------|
| 0 | u32 | version, 1 |
| 4 | u32[9] | record sizes: 1024, 8, 292, 152, 140, 276, 196, 64, 24. The original refuses the file if any differs |
| 40 | u32 | number of directory entries |
| 44 | u32 | directory size in bytes (entries × 16) |
| 48 | entries | u32 size, u32 file offset, u16 type, u16 id, u16 count A, u16 count B |

The chunks follow the directory. In verid1's file (448 entries), every byte
belongs to the header, the directory or exactly one chunk. The game loads a
chunk by (type, id). The tables use id 1, and count B is the number of
records.

| Type | Record | Contents |
|------|--------|----------|
| 1 | 4 | palette, 256 × (R, G, B, 0) |
| 2 | 8 | vertices: int32 x, y in 16.16 fixed point. The floor plan, y up |
| 3 | 292 | sectors (see below) |
| 4 | 152 | walls (see below) |
| 5, 6, 7 | 140, 276, 196 | polygon-like records handled by the same code as sectors; unexplored |
| 12 | – | a single 161 KB chunk; unexplored |
| 13 | – | textures, one chunk per texture, id = texture number |

**Sectors** are convex polygons that tile the floor plan. They can lie above
one another, as rooms over rooms.

| Offset | Meaning |
|--------|---------|
| 108 | u32 flags |
| 124 | edge planes, 16 bytes each: a, b (always 0), c, d with a·x + c·y + d = 0 |
| 266 | u16 vertex count, 3–8 |
| 268 | u16 vertex indices |

Fields not yet understood: +32 and +44..+56 (probably floor and ceiling
heights), +68..+80 (plane-like, possibly slopes), +100 (an object link), +256 and +262.

**Walls** join the vertices at u16 +134 and +136. Their line is
nx·x + ny·y + d = 0, with the unit normal at +28 (x) and +36 (y) and d at +40,
all in 16.16. +32 is unknown.

**Textures** have a 20-byte header: 12 zero bytes (pointers filled in at
run time), then u16 id, width, height and flags. Then come width × height
pixels stored column by column (top first, as far as can be told), and 3 ×
width bytes of per-column hints like GS2's. Palette index 255 (magenta) is
transparent.

At load time the game builds a 64 × 64 grid of 32-byte cells over the map
(around `0x4BEE07`), presumably for collision and visibility.

Opening a `GAME.LV2` in the viewer draws its floor plan. `]` and `[` step
through its textures.
