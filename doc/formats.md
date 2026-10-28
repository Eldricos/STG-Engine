# Star Trek: Generations data formats

Notes from reverse engineering the 2 CD Windows release. "Decoded" means a
decoder exists in `engines/stg` and has been checked against real files.
"Partial" and "unknown" are working notes. All values are little-endian.

## Containers

| File | Format | Notes |
|------|--------|-------|
| `DATA/*.DAT`, `GENS/*.DAT` | ZIP (stored + deflated) | One archive per location, plus `COMMON.DAT`, `OPTIONS.DAT`, `COMBAT.DAT`, `STELLAR.DAT`. Read by `common/zip.h`. |
| `GENS/MUSIC.1`, `MUSIC.2` | ZIP (stored) | 196 stereo 22 kHz WAV music tracks, including `WIN*`, `LOSE*`, `TIMEOUT*`, `LOOP*`, `CREDITS`. |
| `GENS/MOVIE/*.AVI` | AVI | Cinepak video, 640x320 at 24 fps, with 22 kHz 16-bit stereo PCM audio, interleaved in `rec ` lists. The first frame is `00db`, the rest `00id`. Decoded by `engines/stg/movie.h`. |

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
is not stored**: whoever loads an overlay sets it (`0x4029A5`), and without that
it's one frame of every row (`0x402740`). The held items (nu*.ovr) are cut into
88-row frames (`0x430E06`), and the punches of SET_PUNCH into frames as tall as
the 3D view, 252 rows (`0x431D25`, view word +0x7A), so KIRKHIT's 1260 rows are 5
frames and the 1008-row ones 4. The view overlays of SHOW_VIEW_OVERLAY are 640 x
252, one frame. There's no
palette either; the overlay uses the palette of the current scene. The frames
are stacked top down, but each frame's rows are stored bottom up: only that
way up do NUPLANT's nozzle tips sit on the hotspots sgens.exe gives its poses,
and Kirk's fists in KIRKHIT swing up from the bottom of the view.

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

## GSF (`.GSF`): not used

These start like GS2 but with version 1:
`01 00 00 00 33 00 00 00 f8 06 00 00 10 00 00 00 18 00 00 00 80 00 00 01 ...`
(51 frames, with something in the third dword). They're refused by the GS2
decoder rather than misread. Only ANTILIOS.DAT has two, older than the
`011.gs2` and `012.gs2` beside them, and the game only loads `NNN.gs2`
(`0x4C01B0`), so they're leftovers.

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
- **Section 4** holds 100-byte projectile types, ending at a kind of 0 (see Projectiles below).
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
| 16 | the object was hit: damage, power, the attacker's team. Without a handler, an actor not on that team takes the hit (`0x457B29`) |
| 22–25 | a door or lift starts opening, is open, starts closing, is closed |
| 36 / 37 | an actor arrived or finished turning / finished an animation cycle |
| 38–42 | an actor took a hit (`0x45747E`): 38 stunned, 39 noticed only, 40 hurt, 41 knocked out, 42 killed. Power above the full threshold takes damage from hit points and stun points, above the stun threshold from stun points only (`SET_ACTOR_COMBAT_STATS`) |
| 29 / 31 | an actor started / stopped seeing the player: within its near distance and in line of sight (`0x4BB900`: walls and floors in its and the player's zones block, from halfway between its muzzle and its top to the player's eye) |
| 45 | `TRIGGER`, or the player used an item |
| 51 / 52, 53 / 54 | the ceiling / floor above or under the player changed: new / old |
| 55 | a screen effect finished |
| 59, 61 | broadcast: hit report (the object hit or 0, damage, power, team); the player left or landed on the ground |

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

The away-team locations are a table in sgens.exe (`0x545600`, 15 records of
132 bytes: a game class number, then the folder): AMARGOS1, AMARGOS2, ANTILIOS,
ARVADA1, ARVADA2, BERSUS, ENTERP, EPSION, GUERN, HALEE1, HALEE2, QURASH,
GALORN1, GALORN2, VERID1. The location's game object is built by `0x4331FA`
with the controller as its +0x4C.
The retail game ships twelve of them; ARVADA2, GUERN and HALEE2 are in the table but not on
the discs.

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
  - SET_EYE_HEIGHT, SET_STEP_HEADROOM, SET_PLAYER_WIDTH
  - SET_PLAYER_MOTION (0xB2) speed, turn rate, look rate, acceleration, turn and look acceleration,
    jump (popped in that order; 32-bit 16.16 or words). 0x80000000 / 0x8000 keep a value,
    0x80000001 / 0x8001 restore the default (`0x545334`: 1.5, 15, 10, 1.0, 10, 10, 13.9).
    Cutscenes set speed 0.
  - SET_PLAYER_CONTROLS (0xAE) mask: game+0x1E8, 0x5F by default, tested in `0x43AD04` (through
    the copy at +0x1E4, which is 6 while the player falls outside low gravity: no walking in the
    air): 1 walk and strafe, 2 turn, 4 look up and down, 8 jump (X or keypad .: from the floor, or
    once a second in low gravity), 0x10 crouch (C or Enter: the eye eases to 2.5), 0x20 the view
    bobs while walking (no script sets it), 0x40 in low gravity, walking follows the view's pitch.
  - SET_GRAVITY (0xA3) gravity, fastest fall, step down (16.16; keep / default as above):
    defaults 64.4, 200 and 0.25 (verid1: 80 and 25000). The step down is the drop onto a lower
    floor taken without falling (`0x4B4904`); the original sets it only when the fastest fall
    isn't kept (`0x441257`). LOW_GRAVITY (0xB5): 0, 10 and 0, or the defaults. A fall adds
    gravity · t to the speed, up to the fastest, and moves by speed · t (`0x4B4800`). A jump
    (`0x4B6800`) takes the jump speed (SET_PLAYER_MOTION's last, 13.9) off the fall speed; going
    up, the head stops at the ceiling (event 3).
  - SET_FALL_DAMAGE (0xEE) multiplier, divisor (16.16): landing at fall speed v (event 2), or a
    jump hitting the ceiling at v (event 3), hurts v · multiplier / divisor, damage kind 1
    (`0x4357E5`, `0x43A662`); defaults 1 and 32 (verid1: 47), and keep / default as above. A flat
    marked with SET_FLAT_SAFE (0xED) id, on (its object's +0x6C) does no harm either way.
  - PUSH_PLAYER (0x77) position, strength, tRamp, tHold, tEnd: moves the player towards the point
    (away for a negative strength), ramping up, holding, easing off (`0x43B447`); 0x78 stops it.
  - GET_DIFFICULTY (0xD5): the configuration's +0x340, 1 by default.
- **Actors:**
  - ACTOR_WANDER, ACTOR_APPROACH_PLAYER, ACTOR_ATTACK, ACTOR_FACE, ACTOR_STATE_1/4/5, SHOW, HIDE: set the mode (see Actors)
  - SET_ACTOR_COMBAT_STATS, SET_ACTOR_WEAPON, SET_ACTOR_TEAM, and size and reach settings
  - ACTOR_GO_TO (0x4C) id, x, y, height: mode 7 towards a point (waypoints; event 37 on arrival)
  - SET_ACTOR_ALWAYS_ACTIVE (0x5D) and SET_ACTOR_FLYER (0xEA) id, on: flags 0x400000, 0x1000000
  - SET_ACTOR_CLIMB_ANGLE (0xE8) id, degrees: the share of a flyer's speed going across, +0x80 =
    cos(|degrees|) in 16.16 (`0x456E9C`)
  - RELEASE_ACTOR_CLASS (0xA9): frees a sprite class's frames when unused
  - FIRE_PROJECTILE_AT (0xE2) type, from x, y, height, to x, y, height: a shot from no one
- **Lifts, lights and the world** (from amargos1, arvada1, enterp, galorn1/2, halee1):
  - Script objects come in types: 1 logic (0xExxx), 2 walls (0x4xxx), 3 movers/doors (0x8xxx),
    4 actors (0xAxxx), 5 lifts (0x8xxx), 6 light groups (0x6xxx), 7 flats (0xCxxx), 8 billboards
    (0x2xxx) (`0x43519F`).
  - LIFT_SETUP (0x5E) flat, count, 8 heights (16.16; on the lift itself); LIFT_GO_TO (0x64) id,
    level: every flat of the group jumps to that height (`0x4583EA`). The player isn't carried:
    the car's own panel follows it with TELEPORT_PLAYER_RELATIVE by the same distance.
  - SET_LIGHT_LEVEL (0x66) id, percent: light group id & 0x1FFF; RESET_LIGHT_LEVEL (0x67) id.
  - SET_TEXTURE_SCROLL (0xAB) id, u speed, v speed: the texture moves by
    ((speed · ms) & 0x1FFFF) / 1024 of its 128 units (`0x4B78B0`).
  - SET_FLAT_ZONES (0xC0) id, low, high: the zones in play standing there. ADD_FLAT_ZONE_BAND (0xC1)
    id, height, low, high: zones by height for a lift, the first at or below its height counting.
  - SET_SKY (0xB6) n: sky n, and the sky on. SHOW_VIEW_OVERLAY (0xB8) image, mode: a script
    resource's .OVR over the view, one frame of every row (-1 removes it). The mode is how it is
    drawn (`0x409DDC`): 1 centred (helm.ovr, BIGCROP.OVR), 2 row by row (epsion's SHELL.OVR, the same
    for these 640 x 252 pictures), anything else not at all. MAP_HEIGHT_BAND (0xC9) below, above.
  - ENGAGE (0x73) id: event 35 once the player walks 3 units off (another engaged object gets it at
    once). TELEPORT_PLAYER_RELATIVE (0x76) from, to: moved and turned by their difference.
  - Health: SET_ENVIRONMENT_DAMAGE (0x88) period: 1 health a period (kind 13), 0 stops;
    SET_HEALTH (0x8A); SET_WORN_ITEM (0x8B) id: a kind 9 item (30/30) soaks whole hits while it has
    charge, except kind 12, and gets event 48 when empty; SET_WORN_GAUGE (0xE7); SET_ITEM_CHARGE_MAX
    (0xCB, 0xE6) id, charge, most; GET_ITEM_VALUE (0xCC): a worn item's charge, a weapon's team.
  - TRANSITION_TELEPORT (0x93) position, t1, t2, t3: the game dissolves to the game at the new
    place (see Transitions below), then event 7. SET_TRANSITION_COLOUR (0x94) r, g, b: the
    speckles' colour, the palette's nearest (`0x43E084`; index 0x30 until set).
  - SET_MISSION_OUTCOME (0x97) code: up to five codes for the campaign (story branches).
  - SET_PLAYER_SPIN (0xAF) degrees a second. SET_PLAYER_CONTROLS: see above.
  - Actors: SET_ACTOR_MAP_MARKER (0x40), ACTOR_GO_TO_ACTOR (0x4A), SET_ACTOR_TARGET (0x5B),
    SET_ACTOR_ANIM_SPEED (0x5C, 16.16 stored as 8.8), FIRE_PROJECTILE_AT_OBJECT (0xE3),
    GET_ALERTED_COUNT (0xEF, actors seeing the player now).
  - ACTOR_GO_TO_BILLBOARD (0x4B) id, billboard: mode 7 towards a billboard (`0x456CC9`).
    DISENGAGE (0x74) id: forgets the ENGAGE object if it is this one, without event 35.
    SET_TRANSITION_STYLE (0x95) n: game+0x330 (1 by default), the transitions' speckles: 1 single
    pixels, 2 streaks a sixteenth of the screen wide (epsion uses 2).
  - No effect here: 0xED (a flat's +0x6C, unread), ALLOC_GLOBALS (0x15), the logging natives 0x27-0x2A (0x28 pops only its count
    and logs the words under it), LOAD_ACTOR_CLASS (0xA8), 0xAD, SET_PIXEL_PICK (0xE5).
- **Doors** (movers, `engines/stg/movers.h`): built at load (`0x4B7CA0`) from walls with a mover id
  and run every tick (`0x4B82E0`). See "Movers" below. MOVER_OPEN (0x39), MOVER_CLOSE (0x3A),
  MOVER_TOGGLE (0x3B), MOVER_LOCK (0x3C), MOVER_UNLOCK (0x3D), MOVER_SET_OPEN (0x3E),
  MOVER_SET_CLOSED (0x3F) id: set the state at once, locked or not (`0x45643E`).
- **Sound** (see Sounds):
  - PLAY_SOUND sound, volume (0–64)
  - PLAY_SOUND_DELAYED, PLAY_SOUND_FADE_IN, PLAY_SOUND_CUE
  - STOP_SOUND, STOP_ONE_SOUND, FADE_OUT_SOUND
  - SET_SOUND_VOLUME, SET_SOUND_LOOP, SET_SOUND_PRIORITY, SET_SOUND_POSITION, SET_SOUND_OBJECT, SET_SOUND_MAX_DISTANCE, MUTE_SOUND
  - PRELOAD_SOUND
- **The mission:**
  - While the mission loads, its BRIEFING.BMP shows; then COMMON.DAT's PROCEED.BMP (83 x 38: the
    pressed look above the normal one, magenta see-through) goes on it at the place the location
    table gives (0x545600, a rectangle per location: AMARGOS1's is 366,188-448,206). Clicking it
    plays PROCEED.WAV (`0x46233F`) and the mission starts.
  - Event 6 starts it. verid1's e001 answers with BRIEFING_TRANSITION (0x91) t1, t2, t3 (32-bit ms,
    1000 each): the briefing gives way to the game (`0x43D8FF`), then event 7 starts the mission
    proper. Its sound is SET_TRANSITION_SOUND (0x96) sound, volume.
  - END_TRANSITION (0x92) t1, t2, t3: the game dissolves to black, then the mission ends
    (`0x43DC37`); ignored while another transition runs. verid1's death (e00b) and its leaving
    (e001 on 46) use it.
  - Transitions (`0x43E13A`, run instead of the game's tick, so the game stands still): three
    phases, each ending at the sum of the times so far. 1: the first picture with speckles of the
    colour growing to a quarter of each row (the display's 0x64, `0x40BC05`, amount
    elapsed·256/t1 in 1024ths: that many random pixels per row; past half it fills with the
    colour and shows that many of the picture's instead). 2: a crossfade to the second picture
    (0x6C, `0x40C852`: random pixels of each row taken from the second, then past half the other
    way round), with a quarter's speckles over it. 3: the second picture with the speckles
    clearing. The briefing goes to the game, the game to black for the ending, and the game
    before a teleport to the game after it.
  - END_MISSION (0xBF): ends it at once (game.vt114 → the controller's `0x443384`, +0x50 = 1).
  - SET_OBJECTIVES (0x99) mask: game+0x94 (-1 until set); OBJECTIVE_DONE (0x9A) bits: game+0x90 |=
    bits; 0x9B clears them. Once they're equal the game broadcasts event 8, once (`0x43AD04`), and
    sets the controller's +0x10: the mission was accomplished. Nothing else says how it went.
  - The player never dies in the engine: damage leaves at least 1 health and broadcasts event 9, and
    the scripts decide (verid1's hypospray steps in at 1 on the easiest setting).
  - GET_ARRAY/SET_ARRAY's words (0x56F844) outlive the mission: verid1 stores the health and the
    phaser's charge there before leaving.
- **Interface:**
  - SET_INVENTORY_ENABLED (0xD6) on; SET_ITEM_ENABLED (0xD7) id, on: greyed out slots
    (grayout.bmp) that ignore clicks
  - SET_ITEM_ICON (0x83) id, icon: the item's INVENT.BMP frame
  - MAP_ZOOM_LEVELS (0xC8): per level a scale and a shift, pixels = units · 65536 · scale >> shift
  - SET_BILLBOARD_UNPICKABLE (0xF4) id, on: flag 0x400, which picking passes over
  - SET_FLAT_GROUPS (0xCD) a, b, c: a pool of water (see Water below). SET_FLAT_FLAG_68 (0xAD) sets
    the flat object's +0x68, which nothing reads (its getter is only reachable by an actor opcode).
  - the close-up panel (PANEL_*)
  - the automap (MAP_*)
  - the inventory and items (INVENTORY_*, ATTACH_ITEM_BEHAVIOUR, ...)
  - screen flashes and tints, view shake

`engines/stg/mission.h` runs a level's scripts. Every native the eleven
away missions use has a handler. Anything else balances the stack and is logged once with
`--debugflags=scripts`; `-d2` with it also logs every event sent, with its
signal value and the target's state.

To get a full listing of a script:

```sh
stg --extract --path=DATA/VERID1.DAT --pattern='verid1/game.bin' -o out
stg --probe --path=out/verid1/game.bin
```

### Projectiles

Section 4's records become projectile types (`0x43FE9C`), numbered from 0;
`SET_ACTOR_WEAPON` gives an actor one to fire in attack mode, and weapon
items name one too. `engines/stg/projectiles.h`.

| Offset | Meaning |
|--------|---------|
| 0x00 | kind: 1 beam (a head and a trail), 2 bolt, 3 burst (sparks), 4 nothing; 0 ends the table |
| 0x04 | animation rate: frame = age in ms × rate / 65536, wrapped by the frame count |
| 0x08 | lifetime in ms |
| 0x0C | first texture (level texture id) |
| 0x10, 0x14 | 16.16 half width (also the radius it hits with) and height; drawn centred on the shot |
| 0x18 | mirrored view directions |
| 0x1C, 0x20 | keyed (index 255 transparent), see-through |
| 0x24–0x30 | u, v start and length, in 128ths of the texture |
| 0x34, 0x38 | frames, view directions |
| 0x3C | 16.16 speed S: S × 1000 / 65536 units a second (verid1's phaser 106.8) |
| 0x40, 0x44 | fire sound, sounded where the shot starts; impact sound, where it hits; −1 none |
| 0x48, 0x4C | damage, team |
| 0x54, 0x58 | beams: trail sprites and their 16.16 spacing; bursts: sparks and the cone's half angle (1/65536 turns) |
| 0x5C | type thrown where it hits or runs out, or −1 (sparks) |

Shots fly straight without gravity (`0x4C4C70`). Each frame they sweep from
where they were to where they are (`0x4BA9E0`) and stop at the nearest of:
a wall crossed from its front, or either way if it is solid on both sides,
within its height give or take the shot's radius; a floor crossed going
down or a ceiling going up; and, as upright cylinders, billboards
(+0x54 radius, +0x58 high), actors other than the shooter and the player
(radius 1.2 from 6 below the eye to 0.51 above it). Walls, floors and
billboards count only in the zones in play.

A beam strings its trail out behind the head from the muzzle; when the head
hits, the trail runs on into the hit point and vanishes there. Bolts and
beam heads report hits (engine code 20, `0x43696F`): the impact sounds, a
player hit is damage (`game.vt34C`: health falls but never below 1, then
broadcast 9 with source 0, damage, damage, team), and anything else gets
event 16 (damage, damage, team) while everyone hears event 59 (the id,
damage, damage, team). The impact type's sparks fly off what was hit: off
a wall mirrored about it, off a floor with the pitch turned over, off
anything else straight back. Sparks bounce and do no harm.

### The player's attack

Ctrl (or the left mouse button) attacks once per press (`0x43CF95`).

- **With a weapon in use**, the active item (`SET_ACTIVE_ITEM`; items use it
  when the player uses them, event 45), it fires (`0x43FDC4`). Weapon items
  are `ATTACH_ITEM_BEHAVIOUR` kinds 2–6 and 11–13, with c the projectile
  type. They start full (`0x449918`); by kind, charge / highest power level /
  power level: 2 (phaser) 320/16/4, 3 200/12/4, 4 400/16/8, 5 1200/18/18,
  6 240/12/4, 11 and 12 100/10/10, 13 360/9/3. The power level is the
  damage (and power) of the shot and what it costs in charge: setting it
  writes the projectile type's damage (`0x44A03A`), so it holds for every
  item firing that type. `[` and `]` change it by one (`0x44A1B7`,
  `0x44A159`). `ADD_ITEM_CHARGE` keeps the charge within 0 and the most it
  holds; `SET_ITEM_SETTING` is how long the muzzle flashes, in ms
  (`0x449E2C`, verid1's phaser: 300). A shot is broadcast as event 50.
- **Aim.** While the mouse is over the view, weapons aim at it, else at the
  centre (`0x430756`). The shot flies from the gun's muzzle — ahead a, up
  b − 0.5 − 0.375 y / H and to the left c (W/2 − x) / (W/2), for a cursor at
  (x, y) in a W x H view and the item's (a, b, c) — towards the point that
  shows under the cursor, or one W/2 ahead if that is nothing or within the
  player's radius (`0x4B4E10`, `0x4320D1`). The original also raises the
  muzzle as the player looks down, by an amount not worked out. A shot
  never hits the player.
- **The gun held up** (`0x430D19`): the item's overlay, 12 frames of 88 rows
  (each stored bottom up, like every overlay's frames)
  — poses 6–11 (right-low, right-high, centre-high, left-high, left-low,
  centre-low), and 0–5 the same while the muzzle flashes. For a cursor at
  (x, y) its hotspot goes to X = W/2 + 140 (x − W/2) / (W/2),
  Y = A + (B − A) y / B, with A = H − (88 − the highest of the high poses'
  dy) and B = H − (88 − the lowest of the low poses' dy) / 2; below
  H − (lowest dy) − 5 the item can't be used (an arrow cursor). The pose is
  low for the third of the view the cursor is in, or high if the low one
  wouldn't reach the bottom. Overlays, hotspots (dx, dy for poses 6–11) and
  muzzles (a, b, c), from `0x5428D8` and `0x542A30` on:

  | Kind | Overlay | Hotspots | Muzzle |
  |------|---------|----------|--------|
  | 2 | nuphaser.ovr | 116,34 116,3 75,0 35,2 35,33 74,29 | 1.5, 0, 1 |
  | 3 | nudisrup.ovr | 110,34 110,3 75,0 41,2 41,33 74,31 | 1.5, 0, 1.0625 |
  | 4 | nudrifle.ovr | 94,18 92,8 73,0 48,8 48,19 73,14 | 1.5, 0, 1 |
  | 6 | nuklidis.ovr | 100,33 111,2 77,0 46,3 55,34 77,30 | 1.5, 0.09375, 1.1875 |
  | 11 | nuplant.ovr | 119,33 120,11 75,4 28,12 28,32 75,26 | 1.5, 0.125, 1.5 |
  | 12 | nuplant2.ovr | as 11 | 1.5, 0.125, 1.5 |
- **Otherwise** the player punches (`SET_PUNCH`: overlay, ms per frame, hit
  frame, damage, power, swing sound and volume, hit sound and volume). The
  overlay plays over the view, at most a frame a tick, and on the hit frame
  the blow lands on what the centre of the view shows (`0x43A39F`, code 19,
  `0x4364B8`): within the player's radius plus half a unit, plus the radius
  of a billboard or actor, it gets event 16 (damage, power, team 2) and the
  hit sounds. Event 59 reports the hit, or 0 for something out of reach.

### The away-team screen

The 3D view is the top 640 x 252 (the original renders 640 x 320 and shows
rows 34-285, the same picture; a narrower 504-wide view with borderl/borderr
.bmp is also possible). The console below, at (0,252)-(639,479), is drawn
from bitmaps: plain names from COMMON.DAT, the mission's from its folder.
Each bitmap has its own palette; its colours are drawn in their nearest
matches in the level's palette, as Windows did when blitting a DIB to the
8-bit screen. Frame n of a strip is the n-th from the bottom (`0x40390E`):
so 3health.bmp's frame 2, for health above 15, is the top one.
Some are drawn see-through (image panels with their flag set, `0x425A00`):
pixels matching the first pixel in the file, its bottom left, are skipped —
INVENT.BMP's purple (index 59), P-ARROW.BMP's red (159), 3HEALTH.BMP's
magenta (79). Others are opaque: UNITHLT0.BMP is a single colour. The
bitmap class's default key, before a panel asks for that, is 0xFE
(`0x4039CE`).
Rectangles are inclusive. `engines/stg/hudscreen.h`, `hud.h`.

| Part | Where | What |
|------|-------|------|
| left panel | (0,253)-(205,458) | l-m-m-o.bmp, frames of 206 rows by mode: off, object, map, life; tabs at (6,261), (6,331), (6,401), 23 x 52 |
| life | (40,269), (118,393) | body picture (`SET_BODY_IMAGE`, -1 = BODY.BMP, frames of 177 rows); health: 3health.bmp (frames of 50 rows: over 15, over 5, else), then unithlt0.bmp units at x 119, unit k's top at 442 - h - 1 - 3k (- 7 from the 16th) |
| map | (39,267)-(192,414) | walls flagged 0x800 in the height band shown, markers (map\dot-*.bmp, 7 x 7, four frames; dot-enrg and dot-tril for energy and trilithium), the player (agpface2.bmp, 8 headings); buttons of 27-row frames: levels.bmp (43,415) and zoomz.bmp (74,415), three frames for the band and zoom; s-life, s-energ, s-lith.bmp at (117,415), (141,415), (165,415), dim (0) or lit (1), filtering the markers |
| object | (39,267) | the mission's OBJECT.BMP |
| right | (206,252)-(639,479) | actionx.bmp |
| weapon display | (222,252)-(431,296) | weapphas.bmp for the phaser, p-arrow.bmp at x 222 + 11 level − 4 (clicking sets level (x − 239) / 11 + 1), and a gauge of 59 ticks 3 apart whose empty part, 59 − ⌈59 charge / most⌉ ticks, is wiped in colour 0 from (413,280) leftwards, 2 x 7 each; wepcycle.bmp at (207,253) picks the next weapon |
| close-up viewer | (223,311)-(433,458) | the visible panel (frames of 148 rows) and its buttons, or the mission's AUXDEF.BMP; auxcycle.bmp at (207,425) shows the next panel, blinking while there are several |
| inventory | (435,256)-(639,457) | 3 x 4 slots, icons at x 436, 505, 574 and y 257, 308, 359, 410: frame b (of `ATTACH_ITEM_BEHAVIOUR`) of the mission's INVENT.BMP, 64 x 46; the item in use framed in palette index 0x22 |
| text | (10,461)-(639,477) | subtitles, Fonts\Action.fnt |

The painter sets wall flag 0x800 on every wall it draws (`0x4C8602`), so the
automap shows what has been seen; `SET_ZONE_ON_MAP` clears or sets it by zone.
Clicking an inventory slot (press and release on it) makes an item the
player can hold the one in use and sends it event 45; a right click sends 62.
Moving 4 pixels with the button down drags the item (not slot 0's): dropped
on a slot of 1-11 it goes there and what was there to the first free slot,
dropped elsewhere in the inventory to the first free slot, and dropped
outside it leaves the inventory and gets event 63 (`0x44763A`). Clicking the close-up viewer
sends 45 (no buttons) or 47 with the button hit (0 for none), a right click
62, and Space sends 45; the panel's object gets 35 every frame once the
player is 2 units from where it opened. Screen effects blend the palette:
`c = (c (128 - k) + colour k) >> 7` per slot, slot 3 first and slot 0 last.

### Movers (doors)

Doors are built from fields of the wall records (152 bytes) and flat records (292 bytes):

| record | offset | meaning |
|--------|--------|---------|
| wall | 0x8A | mover id (0 for none) |
| wall | 0x8C | mode: how the part moves; also ties followers to a leader |
| wall | 0x8E | speed: the mover's rate is speed · 64 / 100 (default 0x4000) |
| wall | 0x90 | wait: the door closes by itself after wait · 4 ms (default 0x100, only if set) |
| wall | 0x92 | the mover walking into this wall starts (`0x4B94D0`) |
| wall | 0x94 | how: 0 open if shut and not locked (every file), 1 open the other way, 2 close, 3 toggle |
| wall | 0x6C | flag 0x2000: a follower part; 0x80000: reverses when it meets the player |
| flat | 0x106, 0x108 | mover id and mode (6 or 7), for flats with flag 0x2000 |

Walls with a mover id and no 0x2000 are the mover's leaders; followers (walls and flats with 0x2000)
go with the leader of the same mover whose mode is theirs. A mover's position runs from 0 (shut) to
±0x4000, by rate · ms >> 10 a tick. Every part is placed from its original geometry with
f = position / 0x4000, A and B the leader's ends and H its height:

| mode | the part |
|------|----------|
| 0, 1 | slides by f · (B − A) towards A, or B |
| 2, 3 | swings by f · 90° about A, one way or the other |
| 4, 5 | swings about B |
| 6 | slides up: the leader's bottom rises by f · H (its v length shrinking); followers' tops rise; flats rise by f · H |
| 7 | slides down: the leader's top sinks, its texture with it; followers' bottoms sink; flats sink |

Moved walls take normals recomputed as (dy, −dx) / length, distance −(n · v2), as every wall in the
files has. States: 0 idle, 1 open, 2 opening, 3 closed, 4 closing, 5–7 the same the other way. Open
movers with a wait count it down and close; changes send the script object 0x8000 | id event 22
(opening), 23 (open), 24 (closing) or 25 (closed); doors' scripts play their own sounds. A step that
brings a part nearer the player once within 1 unit, or a ceiling down onto their head, is undone, and
a closing door that reverses opens again. The player rides floors that move. Actors aren't checked.

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
| 1 | 4 | palette, 256 Windows RGBQUADs (B, G, R, 0); the console bitmaps use the same indices |
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
| 0x6C | u32 flags: 0x100 visible, 0x100000 ceiling, 0x4000 extra pass (`0x4C8120`), 0x1 left out of the zone lists, 0x1000 pass-through: nothing rests on it. The player (`0x4B4904`), actors (`0x4B6AB0`) and items (`0x4B7AA0`) go through to the floor below; the player's script events still fire. These are trigger floors and water surfaces; as ceilings, they don't stop the head |
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
| 0x20 | the heading of vertex 1 to vertex 2 in turns (0x10000 = 360°); scripts that turn objects rebuild the line from it (`0x4B7670`) |
| 0x2C, 0x30 | heights of the top and bottom edges |
| 0x44, 0x48 | u at the first vertex, and u length along the wall, in 128ths of the texture's width |
| 0x4C, 0x50 | v at the bottom edge, and v length up the wall, in 128ths of the texture's height; v counts rows from the bottom of the texture. Walls stacked on one line continue each other this way (verid1 and halee1 have 17 such pairs, none the other way round) |
| 0x62 | i16 v scroll speed |
| 0x64, 0x66, 0x68 | lighting, as for flats |
| 0x69 | byte record kind, 1 |
| 0x3C | u64 zones the wall is in |
| 0x6C | u32 flags: 0x100 visible; a wall faces the side its normal points to (the right of vertex 1 to vertex 2) and is drawn only from there, unless 0x8000, which also blocks from both sides (0x4000 alone: unseen from behind but hides what is past it); 0x20 mirrors the texture; seen from behind, u still runs from vertex 1 to vertex 2 if exactly one of 0x20 and 0x40000 is set, and the other way otherwise (`0x4C9393`; halee1's 55 rock-edge caps keep their dark edge on the end joining the rock only this way); 0x2 draws columns that are one run of texels over that run alone, so a silhouette isn't repeated above itself (`0x4C0AD0`); 0x40, 0x80 hide what is past the wall below or above it; 0x8 masked (has transparent texels), 0x4 unknown, 0x2000 set aside for script handling at `0x4B7DB5` |
| 0x60 | i16 u scroll speed |
| 0x70, 0x74 | u offsets of the ends for a texture running on along a strip of walls: u0 − (+0x74) at vertex 1 and u0 − (+0x70) at vertex 2, added rather than taken away when mirrored as above |
| 0x78 | extra thickness, added to the radius in one kind of collision test |
| 0x7C, 0x80, 0x82, 0x84 | animation start time, speed, frame count, current frame |
| 0x86, 0x88 | u16 vertex indices |
| 0x8A | u16 scripted object id |
| 0x96 | u16 texture id |

Texture coordinates on walls and floors are in 128ths of the texture: a u
or v length of 128 shows a texture once, whether it is 32, 64, 128 or 256
texels across. 128 is by far the most common length in all four levels
checked, for every texture size. A 256-texel texture can hold two pictures,
one above the other, and walls pick either half; arvada1's texture 10 has a
cliff over a misty mountain range.

**Billboards** (type 5) stand at x +0x1C, z +0x20, y +0x24. They are
2 × (+0x34) wide and +0x38 high, with the texture id at u16 +0x8A and the
frame count at +0x86. Lighting is at +0x64..+0x68, as for flats (record kind 4 or 5).

How walls, billboards and actors are drawn depends on their flags
(`0x4C5820`, `0x4C0AD0`, `0x4C80B4`):

- Index 255 is skipped. Without flag 0x4 the original draws billboards and actors with a routine
  that doesn't skip it, but none in verid1 lack the flag.
- Flag 0x8 makes them **see-through**: walls with 0x8 alone, billboards and actors with 0x8 and
  0x4. Texels 0xF0..0xFE are then averaged with the picture behind them through a 15 × 256 table
  built at level load (`0x4BD620`): entry [s − 0xF0][d] is the palette entry nearest
  ((pal[s] + pal[d]) >> 1), the first one winning a tie. Those texels aren't shaded; the rest are
  drawn as usual. verid1's steam (textures 143..147, flags 0x30C) is drawn this way.
- Without 0x8, texels 0xF0..0xFE are ordinary colours.
- Flag 0x2 (trim columns to their opaque run), and 0x40 and 0x80 (hide what is above or below),
  only save work. Flag 0x10 is fullbright and 0x20 mirrors.

Billboards with flag 0x400000 are **items** (record kind 5). The level keeps
the ones not yet in play in a storage room off the map, in a zone of their
own. At the start (`0x4BDD70`) and whenever a script puts one somewhere,
`0x4B7AA0` drops it onto the highest floor no more than 2 units above its
base, passing over floors with flag 0x1000. It takes that floor's zones and
light, so it is drawn wherever it lands. Flag 0x1000000 keeps its height.

**Sky** (type 12, id 0) is a 64-byte header followed by an image stored
row by row. The header holds the data size at +16 and the height at +24;
the width is size / height. verid1's sky is 640 × 252 and wraps seamlessly
from left to right. halee1's header has the same fields apart from the size,
65536, which 252 doesn't divide: its sky is 512 × 128. Where the height
doesn't fit, the reimplementation takes the width at which neighbouring rows
match best. The header also holds 504 at +20, which the drawing
routine at `0x4C2F50` uses as its row stride. That doesn't fit verid1's
image, so this field isn't understood yet. Nor is how the sky maps to the
view angle; the reimplementation tiles it at one sky pixel per screen pixel.

**Textures** have a 20-byte header: 12 zero bytes (pointers filled in at
run time), then u16 id, width, height and flags. Then come width × height
pixels stored column by column, each column bottom first (billboard pictures that stand on the ground fill the first stored row; in verid1, halee1, amargos1 and arvada1, 85 do and 11 fill the last instead), and 3 × width bytes
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
- Its centre (`0x4C4015`) is its home point, or with a guard mode (record
  +0x10D) what it guards: SET_ACTOR_COMBAT_PARAMS id, mode, target, ... sets
  mode 1 for an actor or 2 for a billboard, found by number (`0x4B5DF0`), and
  target 0 means the player. arvada1 and bersus have actors guard others
  (10, 12, 18, 38-47); the rest wander about home or the player.
- SET_ACTOR_COMBAT_PARAMS (`0x457703`) keeps any value given as 0x8000 (or
  0x80000000 for the distances). It stores its angles as given. verid1 gives
  degrees, so its wanderers turn by under a degree at a time, as in the
  original.

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

`engines/stg/actors.h` implements all this, with shots (`projectiles.h`),
damage and the line-of-sight check.

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
more than the step height above the feet (`0x4B4C50`). Under a ceiling lower
than the eye plus the headroom, the eye comes down to fit (`0x4B46A0`); the
upper doorways in AMARGOS1 are only that tall. A wall whose bottom the head
just touches doesn't block. This search uses the
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

### Water

SET_FLAT_GROUPS (0xCD, `0x440D3D`) a, b, c makes a pool from three flat ids
(the flats' own +0x14, not script ids): a is the surface seen from below (a
pass-through ceiling), b the surface seen from above (a pass-through floor)
and c the pool's bottom. It marks them 2, 1 and 3 at the flat's +0x18 (the
bottom also records b). antilios has six pools, verid1 two, and arvada1,
bersus, galorn1/2 and halee1 more.

Every frame `0x43D0C9` looks at the floor under the player. On a bottom, it
measures the eye's depth under the surface above it (the eye height, if
there's no surface overhead): 0.6 or more is under water (2), more than 0.6
above is wading or afloat (1), and in between the player goes under if
sinking and floats up if not. A change is broadcast: 56 under water, 57
wading, 58 out of the water. Gravity follows, unless control 0x40 is off:
afloat, it is (height of the eye over the surface − 1) / 6 of normal
(buoyancy), under water none. With 0x40 off, under water it is half, with a
fastest fall of 100. Afloat, the zones above the surface are shown too
(`0x43D765`).

The scripts do the rest. antilios's pools, on 56, turn LOW_GRAVITY on, tint
the screen, show a view overlay, slow the player (SET_PLAYER_MOTION) and take
control 0x40 away, so the player sinks slowly and walks along the bottom; 58
undoes it.

Opening a `GAME.LV2` in the viewer draws its floor plan. `]` switches to a
3D view of the level that you can walk through (arrows or WASD, PgUp/PgDn
to look up and down, End to level the view, L to switch lighting off and on,
Shift+C to walk through walls, X to jump, C to crouch). Pressing `]` again steps
through the textures.

## The story (`STELLAR.DAT:DESIGN/STATES.INI`): decoded, `engines/stg/campaign.h`

sgens.exe's game manager (`0x419250`) runs this script. Each `[Section]` is a
list of commands, one a line, with `;` starting a comment; a line can start
with conditions (`IfFlag n`, `IfNotFlag n`, `IfActionGamePlayed n`,
`IfSunsDestroyed n`, `IfEnterpriseAt place`, `IfClue`, `IfObjectFlag`) that
must all hold. `[Start]` shows the title and jumps to `[NewGame]`.

| Command | Effect |
|---------|--------|
| `Jump S`, `JumpRandom S1 S2 ...`, `Call S`, `Pop` | go to a section |
| `Intro` | INTRO1.BMP for 5 seconds |
| `Movie n` | `MOVIE/MOV_nn.AVI` |
| `ActionGame n Won Lost` | the away mission whose location type is n (the second column of the location table), then `Jump Won` or `Jump Lost` by whether its objectives were met |
| `SpaceCombat n Won Lost` | space combat; a result of 2 jumps to `[EnterpriseDestroyed]` |
| `ScrollText n` | `OPTIONS.DAT:TEXTn.TXT` (see below) |
| `Options` | the menu, with no game to return to: the game is over |
| `EndGame` | the credits, then quit |
| `AutoSave` | save to the automatic slot |
| `OnTransport S` / `OnTactical S` | on the bridge, go to S when the player beams down / engages; otherwise the section goes on |
| `SetFlag n TRUE/FALSE`, `SetActionGamePlayed n`, `Destroy place`, `SetObjectFlag` | state for the conditions |

Everything else (`ShowStellar`, `Move`, `WarpFleetIn`, `QueAudio`, `QueMusic`,
`Wait`, `SDTimer`, `Scanner`, ...) drives the bridge. Without it, this engine
takes `OnTransport` at once and never `OnTactical`.

`TEXTn.TXT` is the background picture, the music (in MUSIC.1/2), the font,
then the text, centred by hand with spaces. The credits are a table in the
exe at `0x5401CC`: records of a heading, a name and a style (0 a blank line, 1
more names follow, 2 the last, 3 a section title), with music from
`Credits.WAV` on (`0x540198`).

## The menu (`OPTIONS.DAT`): `engines/stg/optionsmenu.h`

`CAPTAINS.BMP` with buttons down the left (`LOAD`, `SAVE`, `NEW`, `QUIT`,
`SETTINGS`, `CONTROLS.BMP`, 120 x 27, at x 107 from y 32, 30 apart) and the
panels (`OPTSAVE`, `OPTSET`, `OPTNEW`, `OPTQUIT`, `AWAY`/`STARSHIP.BMP`, 320 x 360)
at (227, 29). Button strips have three frames counted from the bottom: dim,
normal, lit. The Load and Save rows are 30 pixels apart from y 33 of the panel,
the automatic save's at 303. Every picture's colours are matched to
`CAPTAINS.BMP`'s palette. Sounds: `X_1000`..`X_1003` (opened, pressed, empty
slot, accept/cancel) and `M_2000`..`M_2002` (cancelled, confirmed, return).
The original keeps its settings in `SGENS.CFG` (two 256-byte key maps, then
music and effects volume 0..29, difficulty, window size, subtitles).

## The bridge (`STELLAR.DAT`): `engines/stg/bridge.h`

`DESIGN/UNIVERSE.INI` describes the map: `[Universe]` (the ten suns, 25
fleets), `[Display]` (MinZ = zoom distance, MaxZ = world scale and the sector
camera's height, SizeMagnification, DistanceMagnification, CenterX/Y), each
sun (`Location=x,y,z`: with z = 0 a screen position in the sector view; its
texture, destruction and scan pictures, GotoSound, SimDestructionSound,
planets), each planet and moon (`Distance`, `Speed` in seconds an orbit,
`OrbitPitch`/`OrbitIntersect`), `[SpaceStation]`, `[FleetN]` (`Type`,
`NumShips`, `Cloaked`, `Alert`), `[DataN]`/`[PicardN]` (frames and
`SequenceK=frame,ms`) and `[Nexus]`. Most sections appear again at the end
with the scan texts; lookups see both. The projection (0x46CD96) is
`x = CenterX + (X - camera.x) / d`, `r = Size / 2 * SizeMagnification / d`
(1..63) with `d = |Z - camera.z|`, the sun at
`((x + 320 - CenterX) * MaxZ, (y + 180 - CenterY) * MaxZ, 10 z)`.

The fleets' and the Nexus's `.RLE` files are OVERLAY data (frames top down,
each frame's rows bottom up), in `FLEET\ENTSIN.BMP`'s and `NEXUS\NEXUS.BMP`'s
palettes. Every other picture has its own palette, so the bridge is drawn in
true colour; index 254 is see-through.

The stardate (48623.4 on a new game) only moves as the player acts: a warp
1.0, a simulated destruction 0.3, a scan or a zoom 0.1; the display climbs a
tenth every 500 ms. `SDTimer d` sets a deadline d past the stardate shown, and
`Wait Timer` holds until it's passed with the Enterprise at a star.
`OnTransport`/`OnTactical` don't wait: they arm the panel's button, which
lights when a short-range scan has found Soran in the Enterprise's system.
`IfClue` is true when the last away mission set any outcome code.

## Space combat (`COMBAT.DAT`): `engines/stg/combatdata.h`, `combat.h`

| File | Format |
|------|--------|
| `.3DV`, `.3DR`, `.PCn` | DRE object v1.10: parts of triangles with a material, 16.16 UVs, face normals; x across, y up, z forward |
| `.MTL`, `.MTR`, `.DMG` | 36-byte materials: RGB555 colour, flags (1 textured, 2 effect, 3 flat), texture name |
| `.IMG` | GIF87a textures, each with its own palette |
| `.LBM` | IFF PBM effect textures |
| `.RAS` | Sun rasterfiles: the console; colour (255,0,163) is see-through |
| `.SPR` | animations ('SPRT', 'TIME', 'COMP' frames packed four ways) in `STANDARD.PAL` (6-bit) |
| `.MET` | big-endian glyph cells into `SCANNER.RAS` / `A-Z_0-9.RAS` |
| `SYSTEMS.DAT` | ship classes: speed, agility, 27 systems (shields 1-4, weapons, life support, engines, power, computer, sensors, damage control, cloak, 10 phaser banks with firing octants, 4 photon tubes), the shields' leak tables, phaser and photon range and damage |
| `SCn.MIS` | `SpaceCombat n`: `:wave` and ship lines `id type x y z yaw pitch velocity AI ...` (x, y the plane, z altitude; yaw in degrees from +y towards -x, so 90 faces -x; AI `soran`, `quarash` or `standard-N` to aim at system N) |
| `COMBATn.BMP`, `PROCEED.BMP` | the briefing; PROCEED's position is a table in the exe (0x548618) |

The console is COMBAT.RAS with the 3D view in its hole at (52,10), 532 x 231,
60 degrees across. The game reads the module's result as 0 the Enterprise
destroyed, 1 lost (warped out), 2 or 3 won (the fleet is marked beaten).

- At the start (`0x47B688` → `0x49461C`) the ship nearest the Enterprise is
  the target, and the Enterprise is turned to face it, level (`0x4A6639`,
  `0x4A2A80`); the player line's yaw and pitch go unused. With yaw read from +y
  towards -x, 66 of the 68 enemies the missions start with face the
  Enterprise (read the other way, 26).
- Cloaking isn't in the engine yet. In the original, ships with a CLOAK
  system fade out (`0x4AD474`, `0x4AD604`); the radar leaves them out
  (`0x4AAC68`: +0xD4 = 0x4000 with flag 0x2000, not the Enterprise), and a
  cloaked target can't be locked ("Target cloaked - no weapons lock"), so
  torpedoes fly unguided.
- The forward view (`0x4A0D13`) sits two of the Enterprise's radii out along
  its own axis, ahead of it, so the Enterprise is never in sight. The enemy
  view (`0x4A1E85`) sits one radius out towards the target and looks at it,
  keeping the Enterprise's up. Both add the viewscreen zoom × 600 to that
  distance.
- The viewscreen zoom slider (618,127-633,180, hotspot 14): clicking or
  dragging in it sets the knob to that row, 18 steps from top to bottom
  (`0x495B49`), and the zoom is the steps below it, 0 to 18 (`0x4AEDFE`, 0 at
  the start). SVIEWN.RAS is drawn above the knob and the lit SVIEWF.RAS below
  it. Besides moving the camera out, the zoom spreads the stars from the
  centre by 1 + zoom/30 (`0x4ADC6A`).
- The radar's slider (450,317-465,371, hotspot 13) works the same way, with
  SRADARN/SRADARF. Its step s sets the range to (19 - s) × 7500 + 10
  (`0x4AB7E2`); the fight starts at step 12, 52510 (`0x49D640`).
- The radar (`0x4AB102`, drawn by `0x4AB5C1`) is round the Enterprise, which
  faces up. A ship more than the range off on any axis isn't shown. The rest
  sit on the plane at (321 + sideways × 102 / range, 320 − ahead × 24 /
  range), on a stalk as tall as height × 24 / range. The stalk is white with
  the 11 x 11 marker (S???MAP, MAPH for the target) at its top for a ship
  level or above, and red with the marker at its foot for one below. Ships
  more than 96 pixels to the side or 18 ahead or behind, outside the plane's
  ellipse (half-widths at 0x54F750) or off rows 280-360 aren't drawn.
- The right panel shows the Enterprise's schematic (SENT_n). Clicking the CONN
  line (262,440-438,460, `0x4AECC6`) calls up the helm menu (CONNMNU0 at
  (472,310)) over it, and letting go puts it away. The CONN line shows the
  order in hand, row `mode`×19 (176 x 17) of CONNBUTT.RAS at (262,442).
- The readouts are in FONTLIT (A-Z_0-9.RAS, whose cells include their
  spacing). The ships' names go at (2,310) for the target and (604,310) for
  the Enterprise: the class's name from `0x54E660` (NCC-1701D, BIRD OF PREY,
  WARBIRD, DREADNOUGHT, VORCHA, PROBE), with " n" for the second of a kind and
  on. Their speeds are `SPEED: %.3f` of the class's top speed at (123,449) and
  (470,449) (`0x49F302`), and the range is `RANGE: %6.0f` at (209,365). FONTLIT's
  minus sign looks like "=".

### The enemies' AI

Each enemy runs `0x4A7C91` every frame, then the steering at `0x4A53A1`. The
AI's clock (`0x47B83E`) counts 291.3 ticks a second.

- **Firing.** Every 2037 ticks (seven seconds): phasers if the Enterprise is
  within the class's phaser range (`0x49ABBC`: each charged bank whose firing
  octant covers it, each shot a roll to hit), and the photon tube facing it,
  if it's loaded and the Enterprise is within photon range (`0x4999AB`: all
  the tube holds, spread 230 angle units apart).
- **Closing.** Beyond three quarters of phaser range, the waypoint is the
  Enterprise, at full speed. Inside it, no new waypoint is set and the ship
  holds its last turn, so it sweeps past and circles out until it's far
  enough to dive in again.
- **Collisions.** Within 1000 units, if the two ships are moving (or closer
  than their radii plus 500) and are, or will be a second later, closer than
  the larger radius plus eight times the faster speed (`0x4A7880`), the ship
  swerves for 1455 ticks to a point 1100 units to either side of and below
  the Enterprise.
- **Running off** (`0x4AFF2A`): to a point two of the Enterprise's phaser
  ranges off it on each axis, side and height at random, below it; at most
  every 2910 ticks. A ship does this when a hit leaves two or more of its
  systems destroyed (`0x4B0103`), and Soran (`soran`, `soran9`, `quarash`:
  flag 0x40000) does it all the time while any other ship is left in the
  fight, including waves still to come.

### TACTICS (`*.BIN`): not used

COMBAT.DAT's eleven `.BIN` files (KLINGON.BIN, WARBIRD.BIN, CHODAK.BIN, ...)
are compiled AI scripts, and sgens.exe still has their interpreter
(`0x4B0000`-`0x4B4630`). But only `0x4A40F2` attaches one, and the mission
loader always passes it no name, so none is ever loaded. The format, for the
record:

- "TACTICS", then a header to 0x6B (a version, sizes, "MAIN").
- Tokens: u32 kind, u32 code, u32 size, then size bytes.
  - Kind 3 is a page: the code that follows, named at +8 ("KERNEL",
    "ATTACK_SET", "SMALL_LOOP", ...). Branches name pages by file offset.
  - Kind 0 is "when": a condition (table at `0x550BC8`; 14, 23, 28, 29 and
    35 are used), followed by the action to take. Reaching it queues it; once
    a page ends, the queue is tested every frame.
  - Kind 1 is a command (table at `0x550C80`).
  - Kind 2 is a branch to a page: codes 1, 4, 5, 0x16, 0x21, 0x22, 0x2B and
    0x2D are used (5 and 0x22 also drop the condition that led there). What
    each does wasn't traced, since nothing runs them.
