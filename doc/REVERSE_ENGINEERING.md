# Reverse engineering: how this was done

STG-Engine was written without the original's source code. Everything in it
was worked out from a legally owned copy of the game: the data formats by
reading the files, and the behaviour by disassembling `sgens.exe`. This
document describes the method, so the work can be checked and continued.

No game data is in this repository, and none should be added. The game's
files were only read to learn their formats and behaviour; the engine reads
them from the player's own copy at run time.

## Sources

- **The game's data:** the installed `DATA/*.DAT` archives and the two CDs'
  `COMBAT.DAT`, `STELLAR.DAT`, `MUSIC.*` and movies.
- **The executable, `sgens.exe`:** 32-bit Windows code, compiled with
  Microsoft C++ and MFC, unpacked and unobfuscated.
- **Footage of the original:** a full playthrough video and short clips,
  for what the code alone couldn't settle (how something looks, its timing,
  or what the player actually sees).
- **Screenshots** of the original for particular screens.

## Data formats

Most formats were found by reading files directly:

1. **Archives first.** The `.DAT` files are ZIP archives (stored and
   deflated), which opened up everything else. `stg --probe` and
   `stg --extract` were built for this.
2. **Look for structure.** Hex dumps, then counts and offsets that add up:
   a table of row offsets that ends where the data does, a count that
   matches the number of records.
3. **Decode and render.** Each format got a decoder, and the result was
   drawn or played to confirm it. Pictures that come out right confirm a
   format quickly; a level that renders correctly confirms dozens of
   fields at once.
4. **Fill gaps from the code.** Where a file didn't say something (the
   overlays' frame height, say), the loader in `sgens.exe` that reads it was
   traced.

[formats.md](formats.md) records every format and what each field means.

## The executable

### Tools

- **Ghidra**, for the first survey: functions, classes and their vtables,
  strings and cross-references.
- **Capstone scripts** in Python, for targeted work: disassembling a
  function, finding every instruction that reads or writes a given field
  offset (say, every `[reg + 0x2FC]`), finding callers of a function and
  references to an address or constant, and dumping data tables.

### Method

1. **Start from something visible.** A string ("proceed.bmp",
   "SPEED: %.3f"), a file name, or a number from the data leads to the code
   that uses it. Script instructions were mapped through the interpreter's
   dispatch table, which leads to each instruction's handler.
2. **Name the objects.** MFC classes have vtables, so an object's methods
   can be listed and its fields followed. For each field, every
   instruction that touches its offset is found. That shows who sets it, who
   reads it and what it means.
3. **Follow the values.** For a behaviour, find where its numbers come
   from: constants in the code (a 0x444 step, a 5820-tick timer), tables in
   the executable (the location table with each mission's PROCEED button,
   the class names, the radar's half-widths), or the data files.
4. **Write it down with addresses.** Code comments and formats.md cite the
   original function (`0x4A7C91` style), so anyone can go back to the
   source of a claim.
5. **Check against the original.** Where possible, the result is compared
   with footage or screenshots of the original: a frame of the video at the
   same moment, the same screen, the same numbers. Several findings came
   only from this step:
   - the speckle dissolve between the briefing and the game;
   - the PROCEED wait;
   - the space combat camera ahead of the Enterprise;
   - the Enterprise facing the nearest enemy when a fight starts.
6. **Check across all the data.** A reading is tested against every
   mission, not just one. Reading the combat headings as "90 faces −x"
   makes 66 of the 68 first-wave enemies face the Enterprise, against 26
   the other way, which settled it.

### Things that went wrong, and what they taught

- **Notes can be wrong.** An early note said clicking the combat viewscreen
  enlarged it. Reading the handler properly showed it steers. Notes are
  marked by confidence, and anything marked medium or low gets rechecked
  before it's built on.
- **A guess shows up as a difference.** The overlays' frame heights were
  guessed at first. Tracing them showed the view overlays are one frame,
  and that a value read as a frame number was really a drawing mode. That
  value had been hiding the helmet and other overlays.
- **Dead code exists.** The routine that starts a ship cloaking is never
  called, and the TACTICS scripts in COMBAT.DAT are never loaded. Finding
  where something is called (or that it isn't) matters as much as reading
  the function.
- **Some things can't be settled from the code alone.** When the code leaves
  a choice open, the engine picks the reading that fits the evidence, and
  [UNCONFIRMED.md](UNCONFIRMED.md) lists it.

## Testing

- **Unit tests** (`test/`) check the decoders, the script interpreter,
  movement, collision, actors, transitions and parts of the missions,
  mostly with small hand-built data. They run with a headless `OSystem`
  that needs no SDL.
- **Running the engine** on the real data, with screenshots taken
  automatically under a virtual display, checks what tests can't: how
  screens look and how fights play.
- **The playthrough** is the last step: [TESTING.md](TESTING.md).

## Conventions

- The code that follows a function of the original says so, with its
  address, in a comment.
- Behaviour copies the original, including its oddities, unless it's listed
  in [KNOWN_DIFFERENCES.md](KNOWN_DIFFERENCES.md).
- New platform-neutral code (a decoder, a format reader) goes in `common/`.
  Anything specific to Generations stays in `engines/stg/`.
