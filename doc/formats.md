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
  uint32 half width at scale 1.0, 16.16 (00 9c 00 00 = 0.609 for 78 columns)
  uint32 column count
  uint32 column height              (256 in 001, 128 in 008)
  uint8  pixels[columns][height]    column-major, BOTTOM row first, 0xFF transparent
  uint8  tables[3][columns]         per-column hints; the first is twice the
                                    topmost opaque row. Not needed to draw.
```

Both samples parse to exactly their end with the advertised frame count.
The half width is always columns / 128 for 256-high frames and columns / 64
for 128-high ones. Actors scale it (see Actors below). As with overlays,
there's no palette.

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
- **Section 2** is the sound cue table and **section 3** the sound table,
  both handed to the sound manager (`0x41EB62` / `0x41EA4B`); see Sounds
  below.
- **Section 4** holds 100-byte projectile records (+0x40: impact sound).
  Section 5 is unexplored.

### Sounds

Everything a 3D level plays comes from its script's sound table: speech,
effects, ambience and music. Scripts play entries by index. The engine
plays nothing by itself; footsteps, doors and hits are all script handlers.

**Section 3** has 56-byte entries and ends with a key of −1 (verid1 has 342):

| Offset | Meaning |
|--------|---------|
| 0x00 | key: bits 0–15 a number, bits 16–23 a speaker prefix, bit 24 "in COMMON" |
| 0x04 | volume, 0–64 (PLAY_SOUND overwrites it) |
| 0x08 | priority |
| 0x0C | position: 0 none, 1 the point below, 2 a billboard, 3 an actor |
| 0x10 | the billboard or actor (& 0x1FFF) |
| 0x14, 0x18, 0x1C | x, y, z (16.16) |
| 0x20 | radius: silent at this distance and beyond |
| 0x24 | start delay in ms (zeroed at load; set by scripts and cues) |
| 0x28 | repeats: 0 or 1 once, 0x7FFFFFFF forever |
| 0x2C, 0x30 | random pause between repeats, min and max ms (0: seamless loop) |
| 0x34 | flags: 0x1 speech with a subtitle, 0x4 streamed, 0x8 / 0x10 hard left / right, 0x20 higher priority, 0x40 fade in, 0x80 fade out, 0x100 muted |

**File names** (`0x41F8A8`): the prefix, then the number as four digits,
then `.WAV`, or `.TXT` for the subtitle.
- The prefixes come from the table at `0x53FDA8`: MISC PICA TROI GEOR RIKE
  DATA WORF CRUS KIRK ROMU KLIN SORA CHOD HANU EXTR KRAU LURS BETO MUSI TEMP
  COMP.
- The files are in COMMON if bit 24 is set. Music (prefix MUSI) is in the
  MUSIC.1 / MUSIC.2 archives. Everything else is in the mission's `sounds/`
  folder.
- For example, verid1's entry 57 is RIKE0107 and entry 136 is MUSI7501.

**Section 2** holds cues, ended by −1. Each cue is:
- `0x12345678`;
- the cue number;
- flags;
- a count (0 in the file);
- (member, used) pairs, where a member ≥ 0 is a sound and a negative member is a pause of −member ms;
- `0x87654321`.

PLAY_SOUND_CUE (`0x41F4E1`) picks members by the flags:
- **0x20:** all of them in order (with 0x1, only the first time).
- **0x10:** the first member, then a pick.
- **0x4:** the first member the first time, picks after that.
- **0x8:** the last member is kept back until the rest are used up.
- **0x1:** never repeat.
- **0x2:** pick at random (otherwise the first unused).

The picks play one after another, with their pauses in between. Then the
object that asked gets **event 46**.

**Playback:**
- **Channels:** there are 8. A new sound takes the channel of the lowest
  priority, oldest sound.
- **Overlap:** speech lines overlap rather than queue, and each shows its
  subtitle as it starts, replacing the last one.
- **Position:** a positional sound is recalculated every frame (`0x41E583`).
  Its distance d is the largest axis plus a quarter of the other two.
  - At d ≥ radius it is silent. Inside, its volume is 10 + (volume − 10) × (radius − d) / radius.
  - It pans by the direction from the player, less so up close.
  - A one-off positional sound out of range when it starts is dropped.
- **Watchdog:** a sound still playing long after its length (by its length
  again, at least 3 s) is stopped.
- **Subtitles** are shown for the sample's length, at least 2 s. They may
  contain `\T<RRGGBB>\` (colour), `\W<hex>\` (delay), `\B<hex>\`, and
  `\\` for a backslash.

**References.** Natives that take a 32-bit sound reference read it as follows:
- high word 0: a sound;
- high word 0xFFFF: every playing copy of that sound;
- otherwise: the copy whose play call returned that value.

PLAY_SOUND and the fade-in play return such a handle, or −1.

`engines/stg/sound.h` implements all this.

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
| 09 | SEND_SIGNAL | pop id, pop v; send event 4 with v to object id |
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
| 27–F4 | natives | calls into the game (see below) |

The natives' stack effects are in `engines/stg/opcodes.cpp`. They were
computed from the stack pointer adjustments in each handler of the
interpreter, where a 32-bit argument takes two slots: the low word is
pushed first, so the high word is on top. Run through every block of
verid1, they never underflow the stack, except in a few blocks that take
more event arguments than the test supplied.

### Objects and the world

The SGEN loader's object factory (`0x43519F`) makes one C++ object per
script object, its class chosen by the object's type. The top three bits
of an object's id give its type, and for objects that stand for parts of
the level, the rest is the id in the level records' offset 0x14:

| Id bits | Type | Stands for |
|---------|------|------------|
| 0xE000 | 1 | logic: no level record; also behaviour templates other objects inherit from |
| 0x4000 | 2 | walls with that id |
| 0xA000 | 4 | the actor (`GAME.LV2` type 6) with that id |
| 0xC000 | 7 | flats with that id |
| 0x2000 | 8 | billboards with that id |

A native that takes an object id calls the object's own method if the id
is the running object's, and otherwise looks the id up in the mission
object (`0x545EA8`, vtable `0x531768`) and calls the same method on it.
The original doesn't check that the object exists.

### Events

**Delivery.** Events are delivered at once, nested inside the block that
sends them (`0x44F4E1`). An object disabled with `DISABLE_OBJECT`, or still
in state −1, gets no script blocks. Events from the engine are queued
during an engine tick and delivered at its end (`0x4B8120`, `0x4B80D0`).

| Event | Sent when |
|-------|-----------|
| 1 | timer tick (`SET_TIMER_*`); repeats until `KILL_TIMER` |
| 2 / 3 | state entered / left |
| 4 | `SEND_SIGNAL`, with one value |
| 5 | `SEND_SET_VALUE`, with one value |
| 6 | broadcast when the mission begins (`0x43E742`); verid1 places the player then |
| 9 / 10 | broadcast: the player was hurt / healed |
| 14 / 15 | the player started / stopped touching a wall, a billboard's area, an actor, or the floor stood on |
| 16 | the object was hit by the player's attack |
| 22–25 | a door or lift starts opening, is open, starts closing, is closed |
| 36 / 37 | an actor arrived or finished turning / finished an animation cycle |
| 38–42 | an actor was hit: lesser hits, hurt, stunned, dead |
| 45 | `TRIGGER`, or the player used an item |
| 51 / 52, 53 / 54 | the ceiling / floor above or under the player changed: new / old |
| 55 | a screen effect finished |
| 59, 61 | broadcast: hit report; the player left or landed on the ground |

Broadcasts go only to objects that subscribed with `SUBSCRIBE`. Contacts are
reported on change only: the old thing gets 15 and the new one 14.

**Level start** (`0x4336F0`, `0x4356FF`):
1. Every object is created in state −1.
2. Each object is set to state 0, so it receives event 2.
   - The order is the original's hash map order: bucket `(id >> 4) % 17`
     ascending, and within a bucket the last object in the file first.
   - Events sent to objects that haven't been visited yet reach only the
     built-in defaults.
3. Nothing is sent every frame without a reason. Scripts run on timers,
   contacts and each other's signals.

**Frame time.** The mission clock advances by the frame time, clamped to
8–1000 ms.

### Natives

Natives that verid1 uses, by purpose. Arguments are listed as popped, top
of the stack first. A *position* is x, y, z (32-bit 16.16 each) and a
heading in degrees.

- **Objects:**
  - DISABLE_OBJECT, ENABLE_OBJECT
  - SUBSCRIBE, UNSUBSCRIBE
  - SET_PARENT: inherit another object's handlers; they run with SELF unchanged
  - TRIGGER (event 45)
  - SEND_SET_VALUE
  - RANDOM n (0 to n−1)
  - GET_ARRAY, SET_ARRAY: a second array of words that persists across missions
- **Timers:** SET_TIMER_MS, SET_TIMER_SECONDS, SET_TIMER_RANDOM, KILL_TIMER.
- **The level:**
  - HIDE, SHOW: clear or set flag 0x100, which also stops a wall blocking
  - SET_SURFACE_TEXTURE id, texture, frames, period: a frame lasts 4 × period ms
  - RESET_DEFAULT: back to the original texture
  - SET_FULLBRIGHT
  - SET_SKY_ENABLED
  - SET_ZONE_ON_MAP
- **Positions:**
  - SET_POSITION id, position: moves actors; billboards ignore it (`0x4586D0`), and items just settle again
  - GET_POSITION id (0 = the player)
  - SET_HEADING
  - DISTANCE_TO_PLAYER
  - DROP_AT id, where: shows the object and moves it to the player (0) or an actor, items onto the floor there
  - TELEPORT_PLAYER position
  - PUSH_PLAYER
- **The player:**
  - GET_HEALTH, ADD_HEALTH, DAMAGE_PLAYER
  - SET_EYE_HEIGHT, SET_STEP_HEADROOM, SET_PLAYER_WIDTH, SET_PLAYER_MOTION
  - SET_PLAYER_CONTROLS, LOW_GRAVITY
- **Actors:**
  - ACTOR_WANDER, ACTOR_APPROACH_PLAYER, ACTOR_ATTACK, ACTOR_FACE, ACTOR_STATE_1/4/5, SHOW, HIDE: set the mode (see Actors)
  - SET_ACTOR_COMBAT_STATS, SET_ACTOR_WEAPON, SET_ACTOR_TEAM, and size and reach settings
- **Sound** (see Sounds):
  - PLAY_SOUND sound, volume (0–64)
  - PLAY_SOUND_DELAYED, PLAY_SOUND_FADE_IN, PLAY_SOUND_CUE
  - STOP_SOUND, STOP_ONE_SOUND, FADE_OUT_SOUND
  - SET_SOUND_VOLUME, SET_SOUND_LOOP, SET_SOUND_PRIORITY, SET_SOUND_POSITION, SET_SOUND_OBJECT, SET_SOUND_MAX_DISTANCE, MUTE_SOUND
  - PRELOAD_SOUND
- **Interface:**
  - the close-up panel (PANEL_*)
  - the automap (MAP_*)
  - the inventory and items (INVENTORY_*, ATTACH_ITEM_BEHAVIOUR, ...)
  - screen flashes and tints, view shake

`engines/stg/mission.h` runs a level's scripts. It implements the natives
for objects, timers, the level, positions, actors' modes and settings,
sound, and the player's health and size. The rest balance the stack and are counted as missing; the viewer
logs each one once with `--debugflags=scripts`.

To get a full listing of a script:

```sh
stg --extract --path=DATA/VERID1.DAT --pattern='verid1/game.bin' -o out
stg --probe --path=out/verid1/game.bin
```

## Level data (`GAME.LV2`): decoded enough to render, `engines/stg/level.h`, `render3d.h`

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
| 3 | 292 | flats: floor and ceiling polygons (see below) |
| 4 | 152 | walls (see below) |
| 5 | 140 | billboards: sprites that always face the camera |
| 6 | 276 | actors (see Actors below) |
| 7 | 196 | empty in verid1 |
| 12 | – | the sky (see below) |
| 13 | – | textures, one chunk per texture, id = texture number |

World coordinates are 16.16 fixed point. x and y are the floor plan, and
z is height, growing upwards. In verid1, walls are mostly 12 units high and
a texture usually covers 10 or 15 units.

The renderer was traced through `sgens.exe`:
- **Camera.** `0x4C3600` transforms the vertices with the camera at
  `0x57D9B8` (x, z, y). Yaw cos/sin are at `0x57D9CC`/`0x57D9D0`, and a pitch
  shear is at `0x57D9D8`. Looking up and down shears the view instead of
  rotating it.
- **Projection.** screen x = centre + x/depth · W/2 and screen y = centre −
  y/depth · W/2, set up at `0x4B58B0`. That is a 90° horizontal field of view.
- **Drawing.** `0x4C13F0` draws flats and `0x4C1A50` texture-maps them;
  `0x4C7D00` draws walls.

Several records share a layout for animated textures: a frame count and a
speed, with the current frame being the base texture id plus an offset
that advances with time.

**Flats** (type 3) are horizontal convex polygons. Each one is a floor or a
ceiling at a single height; a level's floors and ceilings are stacked to
make rooms over rooms.

| Offset | Meaning |
|--------|---------|
| 0x14 | u32 group: flats with the same value are linked in one list (count A groups) |
| 0x20 | height |
| 0x3C | u64 zone the flat is in (one bit set); see Collision and zones |
| 0x44, 0x48 | u offset and scale: u = x · scale + offset, in texels |
| 0x4C, 0x50 | v offset and scale: v = y · scale + offset |
| 0x5C | maximum drawing distance, 15000 |
| 0x60, 0x62 | i16 u and v scroll speeds |
| 0x64, 0x66, 0x68 | lighting: i16 light group, u16 brightness, byte light level (see Lighting) |
| 0x69 | byte record kind: 2 on floors and 3 on ceilings |
| 0x6C | u32 flags: 0x100 visible, 0x100000 ceiling, 0x4000 extra pass (`0x4C8120`), 0x1 left out of the zone lists, 0x1000 not a floor for actors and items: their ground searches (`0x4B6AB0` callers `0x4C3AC0`, `0x4B7AA0`) pass over it |
| 0x74 | u64 zones in play while standing on this floor |
| 0x7C | 16 bytes per vertex: an edge plane a, c, d (a·x + c·y + d = 0) at +0, +8 and +12; +4 receives the transformed vertex's address at load time |
| 0xFC | animation start time (run time) |
| 0x100, 0x102, 0x104 | u16 animation frame count, speed and current frame |
| 0x10A | u16 vertex count, 3–8 |
| 0x10C | u16 vertex indices |
| 0x11C | u16 texture id |
| 0x11E | u16 texture rotation angle, a full turn being 65536 |

A floor is drawn only when the camera is above it, and a ceiling only when
the camera is below it.

**Walls** (type 4) are vertical quads.

| Offset | Meaning |
|--------|---------|
| 0x1C, 0x24, 0x28 | line nx·x + ny·y + d = 0 with a unit normal, used to cull walls facing away |
| 0x20 | unknown, 0 to 0.87 |
| 0x2C, 0x30 | heights of the top and bottom edges |
| 0x44, 0x48 | u at the first vertex, and u length in texels along the wall |
| 0x4C, 0x50 | v at the top, and v length in texels down the wall |
| 0x62 | i16 v scroll speed |
| 0x64, 0x66, 0x68 | lighting, as for flats |
| 0x69 | byte record kind, 1 |
| 0x3C | u64 zones the wall is in |
| 0x6C | u32 flags: 0x100 visible, 0xC0 drawn from both sides, 0x8000 blocks from both sides, 0x8 masked (has transparent texels), 0x4 unknown, 0x2000 set aside for script handling at `0x4B7DB5` |
| 0x78 | extra thickness, added to the radius in one kind of collision test |
| 0x7C, 0x80, 0x82, 0x84 | animation start time, speed, frame count, current frame |
| 0x86, 0x88 | u16 vertex indices |
| 0x8A | u16 scripted object id |
| 0x96 | u16 texture id |

The u length divided by the wall's length is always a whole texture over a
whole number of units (12.8 texels per unit = 128 over 10, and so on), which
confirms the u and v fields.

**Billboards** (type 5) stand at x +0x1C, z +0x20, y +0x24. They are
2 × (+0x34) wide and +0x38 high, with the texture id at u16 +0x8A and the
frame count at +0x86. Lighting is at +0x64..+0x68, as for flats (record kind 4 or 5).

Billboards with flag 0x400000 are **items** (record kind 5). The level keeps
the ones not yet in play in a storage room off the map, in a zone of their
own. At the start (`0x4BDD70`) and whenever a script puts one somewhere,
`0x4B7AA0` drops it onto the highest floor no more than 2 units above its
base, passing over floors with flag 0x1000. It takes that floor's zones and
light, so it is drawn wherever it lands. Flag 0x1000000 keeps its height.

**Sky** (type 12, id 0) is a 64-byte header followed by an image stored
row by row. The header holds the data size at +16 and the height at +24;
the width is size / height. verid1's sky is 640 × 252 and wraps seamlessly
from left to right. The header also holds 504 at +20, which the drawing
routine at `0x4C2F50` uses as its row stride. That doesn't fit verid1's
image, so this field isn't understood yet. Nor is how the sky maps to the
view angle; the reimplementation tiles it at one sky pixel per screen pixel.

**Textures** have a 20-byte header: 12 zero bytes (pointers filled in at
run time), then u16 id, width, height and flags. Then come width × height
pixels stored column by column, each column top first, and 3 × width bytes
of per-column hints like GS2's. Flag 1 means the texture has no hints. Palette index 255
(magenta) is transparent.

At load time the game builds a 64 × 64 grid of 32-byte cells over the map
(around `0x4BEE07`), presumably for collision and visibility.

### Lighting

Every wall, flat, billboard and actor is drawn at one of 8 light levels.
- **Shade tables.** Each level has a colour table, built at `0x4BFC60` and
  held as 8 pointers at `0x575D80`. Table n maps each palette entry to the
  entry nearest to it blended n/8 of the way toward a fog colour; table 0 is
  the identity, drawn by faster routines. The builder is only called when a
  level starts, and then with black, so the levels are darkness. There is
  no distance fog. Where there is no sky, the background is filled with the
  entry nearest the fog colour, kept at `0x575670`.
- **Choosing the level.** A surface's level comes from offsets 0x64-0x68:
  - If the i16 light group at 0x64 is 0, the level is the byte at 0x68
    (0 for every such surface in verid1).
  - Otherwise, on load (`0x4BE0E0`) the level is 7 − min(7, brightness ÷
    3200), with the u16 brightness at 0x66.
  - A script can set a group's brightness percentage (the native at
    `0x4B7BA0`). The level then becomes 7 − min(7, percent × brightness ÷
    320000).
- **Actors.** An actor takes the light level of the flat it stands on
  (`0x4B7A50`).

In verid1, about half the walls and flats are in light groups, at
brightness 5120 to 20480. That leaves the caves under the ice noticeably
darker than the open canyon.

### Actors

Type 6 records are the actors themselves: the engine updates them in
place (`0x4C3C10`). Records sharing an id (+0x14) are parts of one actor,
chained at +0, and change mode together. verid1 has 40 actors, each with
one part.

| Offset | Meaning |
|--------|---------|
| 0x14 | id; the script object is 0xA000 \| id |
| 0x1C, 0x20, 0x24 | x, z (feet), y |
| 0x28 | reach: "walk to" has arrived within this distance |
| 0x30 | added to z for the sprite's bottom edge |
| 0x34 | sprite scale (at run time, the current half width) |
| 0x38 | sprite height in world units |
| 0x54, 0x58 | collision radius and height |
| 0x64..0x68 | light, as walls; at run time the floor's light is copied in |
| 0x6C | flags: 0x1 not drawn, 0x10 fullbright, 0x400000 never frozen, 0x1000000 flyer (keeps its height) |
| 0x90, 0x94, 0x98 | wander: outer, inner and home radius |
| 0x9C, 0xA0 | wander: walk between this and this far before a random turn |
| 0xAC | step height |
| 0xB0 | home point (the start position) |
| 0xC4 | 7 animation entries of 8 bytes (below) |
| 0xFC | u16 sprite set number: the actor's sprites are `NNN.gs2` next to GAME.LV2, and the number is also its class |
| 0x100, 0x102 | wander: random turn range, 65536 to a turn |
| 0x104 | turning speed, heading units per 1024 ms |
| 0x108 | heading: 65536 to a turn, 0 faces +y, 16384 faces −x |
| 0x10D | wander around the player rather than home |
| 0x10E | mode (below) |

**Animation entries.** Each entry has eight bytes:
- u16 speed, 8.8 units walked per frame (0 means 0x40);
- rate;
- frames per view direction (0 means 1);
- loop start, clamped to frames − 1;
- number of view directions (8 or 1);
- mirror flag;
- the sequence's number (unused).

The entry for mode m is min(m, 6). Its GS2 sequence is given by the table
at `0x552338`:

| Entry | Animation | GS2 sequence |
|-------|-----------|--------------|
| 0 | stand | 0 |
| 1 | pain | 2 |
| 2 | stand | 0 |
| 3 | attack | 3 |
| 4 | death A | 4 |
| 5 | death B | 5 |
| 6 | walk | 1 |

A sequence holds each view direction's frames in turn.

**Timing** (`0x4C4230`):
- Each tick advances the animation by rate × dt / 1024 frames (dt in ms),
  which is about `rate` frames a second.
- The first pass plays every frame; after that the animation loops from
  loop start. A loop start of frames − 1 plays the animation once and holds
  the last frame.
- Each cycle raises script event 37, unless the animation has only one frame.
- Walking moves speed × frames advanced.

**Drawing** (`0x4C3C10`):
- **Facing:** let A be the angle from the camera to the actor, measured like a heading.
  The octant is `(u16)(−A − heading + 0x1000) >> 13`, and direction =
  octant × directions / 8.
  - Direction 0 shows the actor's back, 2 its right side and 4 its front.
  - With the mirror flag, directions 5–7 are drawn as 3–1 flipped.
  - The frame is the sequence's entry at frame + direction × frames, or entry 0 if that is past the end.
- **Size:** the sprite is centred on x, y. It is 2 × the frame's half width
  × scale wide and 0x38 high, and stands on z + 0x30.
- **Light:** it is lit like the floor it stands on.

**Modes**. Scripts set them, and nothing changes mode by itself:

| Mode | Behaviour |
|------|-----------|
| 0 | idle |
| 1 | pain animation |
| 2 | turn to face the target or the player; event 36 once facing |
| 3 | face and fire at the start of each cycle (each shot raises 36) |
| 4, 5 | death animations |
| 6 | wander (below) |
| 7 | walk straight at the target or the player; event 36 every tick while within reach. Blocked, it turns a quarter and tries again (no path finding) |
| 8 | hidden: neither drawn nor updated |

**Wander** (mode 6):
- Walks its heading and turns at the turning speed.
- After walking a random distance, it turns by a random angle in the
  turn range, always in the same direction.
- Beyond the outer radius from its centre it turns a quarter towards the
  centre. Beyond the home radius it walks home until it is inside the outer
  radius again. Crossing inwards past the inner radius turns it a quarter.
- A wall turns it a quarter.
- SET_ACTOR_COMBAT_PARAMS stores its angles as given. verid1 gives degrees,
  so its wanderers turn by under a degree at a time, as in the original.

**Movement:** actors move with the player's sweep and slide, using their own
radius, height and step height. They snap onto floors and never fall.

**Frozen actors:** an actor whose floor is in a zone the player can't see
is frozen.

**Proximity events.** SET_PROXIMITY_RANGES sets four distances, r1 to r4, for
an actor class and a target class (0 is the player). Each is measured from
the player's eye to the actor's feet:
- 29 when the player comes within r1 (with a line of sight in the original);
- 31 when the player goes beyond r2;
- 33 when the player comes within r3;
- 35 when the player goes beyond r4.

The player collides with actors as with wall ends. A bump sends 14 and 15,
as walls do.

`engines/stg/actors.h` implements all this. Shots, combat damage and the
line-of-sight check are not implemented yet.

### Collision and zones

The player update at `0x4B46A0` moves the player with these settings, set
at `0x4BE150`:

| Address | Value | Meaning |
|---------|-------|---------|
| `0x57D98C` | 6.0 | eye height above the feet |
| `0x57D990` | 0.51 | headroom: the body reaches this far above the eye |
| `0x57D994` | 2.0 | step height |
| `0x57D998` | 1.2 | radius |

A move goes from the previous position to the new one (`0x4B9870`):
- **Sweep.** `0x4B9940` sweeps a circle of the player's radius and finds
  the first wall or actor touched. Walls come from a grid along the path.
- **Which walls count.** A wall is tested only if it is visible, in an
  active zone, and its bottom-to-top range overlaps the player's body above
  the step height. Walls below that are stepped over.
- **Wall tests.** Each wall is tested as a line (`0x4BA670`) and as its two
  end points (`0x4BA1E0`). The line only blocks from the front, the side
  its normal points to, unless flag 0x8000 is set.
- **On a hit.** The player stops at the wall. The result includes the wall's
  direction, and what is left of the move slides along it. If the slide
  hits something too, the move ends there.
- **Script event.** A wall the player touches sends an event to the scripts,
  with the record kind at +0x69 and the wall's +0x14.

After moving, the player stands on the highest floor under them that is no
more than the step height above the feet (`0x4B4C50`). This search uses the
64 × 64 grid of flats built at `0x4BEDE0`, where each flat's outline is
pushed out by 3 units. With no floor underneath the player falls; the
original's falling physics (`0x4B4800`) is not decoded yet.

**Zones** are 64 bits that partition the level. Every flat is in one zone,
and walls and billboards are in one or more. The floor the player stands
on lists the zones in play at 0x74 and makes them active (`0x4B68A0`).
Only walls and floors in active zones are collided with, and only
surfaces in active zones are drawn. That is how a level keeps rooms stacked
above one another, or caves under a canyon, apart. With no floor
underfoot, every zone is active. A script can add zones of its own; these
are kept at `0x57D9AC`.

Opening a `GAME.LV2` in the viewer draws its floor plan. `]` switches to a
3D view of the level that you can walk through (arrows or WASD, PgUp/PgDn
to look up and down, End to level the view, L to switch lighting off and on,
C to walk through walls). Pressing `]` again steps
through the textures.
