# Known differences from the original

Places where STG-Engine deliberately or knowingly behaves differently from
the original `sgens.exe`. Things that might differ but haven't been checked
are in [UNCONFIRMED.md](UNCONFIRMED.md).

## Front end and options

- **The menu comes first.** The original goes from the title straight into
  a new game. Here the captain's menu comes first, so a saved game can be
  loaded.
- **Escape gives up choosing a key.** On the Controls screen, once a box is
  clicked, the original takes the next key whatever it is, Escape included
  (shown as `?!?`). Here Escape cancels instead, so it can't be bound.
- **Settings live in STG-Engine's settings file,** not `SGENS.CFG`. The key
  maps are kept there as lists of key and action numbers.
- **Saves are STG-Engine's own format.** They can't be exchanged with the
  original's saves. The save version is 7. Older STG-Engine saves still
  load; actors in them that guard something (Arvada 1, Bersus) circle the
  player until a script changes their orders.
- **A save in an away mission keeps the moment.** The original saves only
  where the story is, so loading starts the mission again ("Save game at
  start of current mission"). Here the mission's whole state is saved, and
  loading carries on from where you were. Saving in space combat works as
  in the original: loading starts the fight again from its briefing.
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
- **Keys the key map leaves free keep this engine's own uses.** In the
  original, loading the key map replaces the game's whole built-in key
  table, so only the keys on the Controls screen do anything there. Here,
  any key not bound on the Controls screen still does what this engine gives
  it: WASD, End, X, C, Space, B, H, O, `[`, `]` and 1-9 on away missions;
  Q, E, F, G, = and -, Tab, T and V in space combat. A key bound on the
  Controls screen always does its bound action instead.

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
- **Mouse Look's speed is scaled to the keys'.** The original turns the view
  a fifth of the way to the mouse's heading each frame. Here that rate is
  scaled by the same ratio as the turn and look keys' speeds, so the two
  stay in step.

## Versions

- **Only the 2-CD Windows English release is recognised.** Other releases
  are found by a fallback and reported as unknown versions. They may well
  run, but nothing about them has been checked.
