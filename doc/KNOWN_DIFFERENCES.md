# Known differences from the original

Places where STG-Engine deliberately or knowingly behaves differently from
the original `sgens.exe`. Things that might differ but haven't been checked
are in [UNCONFIRMED.md](UNCONFIRMED.md).

## Front end and options

- **The menu comes first.** The original goes from the title straight into
  a new game. Here the captain's menu comes first, so a saved game can be
  loaded.
- **The keys can't be changed.** The Controls screen lists them, but it
  can't rebind them yet.
- **Saves are STG-Engine's own format.** They can't be exchanged with the
  original's saves. The save version is 7. Older STG-Engine saves still
  load; actors in them that guard something (Arvada 1, Bersus) circle the
  player until a script changes their orders.
- **Space combat can't be saved.** Its state isn't kept, so Save is off
  during a fight.
- **A mission can't be saved while its briefing waits for PROCEED.**

## Extras the original doesn't have

- The resource viewer (`--viewer`), which shows every picture, sound, level,
  overlay and sprite set in the game's data.
- `--story=SECTION` and `--combat=N`, which start the story at any section,
  or go straight into a space combat.
- Alt+Enter for fullscreen and Ctrl+Alt+F for filtering.
- Debug logging (`-d2 --debugflags=scripts` logs every script event).
- In the 3D view, Shift+C turns collision with walls off, and L turns the
  lighting off.

## Away missions

- **Everything is drawn in 256 colours.** The original drew to a 16-bit
  screen; here the screen is 8-bit with one palette at a time. In the
  briefing's crossfade into the game, the briefing picture is remapped into
  the level's palette, so its colours shift slightly for that second.
- **The teleport transition moves the player at the start of the
  crossfade.** The original moves the player at once but keeps showing the
  old view until then, so this looks the same.
- **The held weapon isn't raised as the player looks down.** The original
  raises it, but by an amount that isn't known. The playthrough video shows
  no visible change, so it's left out.
- **One script array (the F2/F3 instructions) holds 4096 words.** The
  original's limit isn't known. Nothing comes near it.

## Space combat

- **No shield models.** When shields are hit, the hit animation plays, but
  the ship's shield bubble isn't drawn. That includes the panels' shield
  facing display, which shows only the ship.
- **Arriving waves appear at once.** In the original, later waves may fade
  in as they uncloak (see [UNCONFIRMED.md](UNCONFIRMED.md)). Only SC9 has
  later waves.
- **Per-system states on the schematics.** Each system's part of a
  schematic is bright when working, dim when damaged and red when
  destroyed. The original's flags behind "bright" and "dim" weren't fully
  decoded; this is the reading that fits the pictures and the video.
- **Timing based on frames.** Where the original steps something once a
  frame (the streaming stars), the engine takes the original as running at
  twenty frames a second and scales by the real time.

## Versions

- **Only the 2-CD Windows English release is recognised.** Other releases
  are found by a fallback and reported as unknown versions. They may well
  run, but nothing about them has been checked.
