# STG-Engine

A reimplementation of the engine behind MicroProse's 1997 **Star Trek: Generations**,
so the game can run natively on modern systems. It is structured like
[ScummVM](https://www.scummvm.org/): a thin platform backend, a shared
platform-independent layer, and game engines that only use that layer. The
backend is built on **SDL3**.

You need your own copy of the original game. No game data is included.

## Status

The whole game runs, from the title to the credits:

| Area | State |
|------|-------|
| SDL3 backend: window, scaling, fullscreen, palette, software cursor, input, audio | Done |
| `common/`: OSystem interface, files and archives (case-insensitive), ZIP with our own DEFLATE decoder, streams, config, saves, events, surfaces, palettes, BMP decoder, software mixer, PCM/WAV | Done |
| `engines/`: Engine/MetaEngine framework, table-driven detection, launcher and command line | Done |
| Data: the `.DAT`/`MUSIC.*` archives, images, speech, music, subtitles, fonts, overlays (`.OVR`), sprite sets (`.GS2`) | Done |
| Movies: Cinepak video with 22 kHz PCM in AVI | Done |
| The front end: title, captain's menu (load, save, settings, controls), intro, scrolling text, credits | Done |
| The story (`STATES.INI`): movies, away missions and combats in order, branching on how each went, autosaves, the endings | Done |
| The bridge: the star map, warp, scans, the Nexus, fleets, the crew's reports | Done |
| Away missions: levels (`GAME.LV2`) in a software 3D renderer, collision and zones, doors and lifts, scripts (`GAME.BIN`) with every instruction the eleven missions use, actors, weapons and damage, the HUD, inventory, close-up panels and map | Done |
| Space combat: the ships (DRE models, `SYSTEMS.DAT`), the console, helm orders, phasers and torpedoes, shields and system damage, waves, the enemies' AI, the crew's reports | Done |

Still open:

- A full playthrough looking for bugs ([doc/TESTING.md](doc/TESTING.md)).
  Check these when you get there:
  - Space combat openings: the original's radar showed fewer ships than the
    mission file holds at the start of Halee's fight (SC16); here they all
    show. The cause isn't known.
  - SC9's later waves: the original may fade the arriving ships in with the
    uncloak sound; here they appear at once.
  - Epsion's shell overlay (SHELL.OVR, view overlay mode 2): drawn as mode 1;
    it should look the same.
- Some map marker styles haven't been seen in the original yet, so how they
  look is unconfirmed. See [doc/formats.md](doc/formats.md).
- Only the 2 CD Windows English release is in the detection table. Other
  versions are found by a fallback that looks for an executable mentioning
  "Generations", and reported as unknown versions.

More in [doc/](doc/):

- [KNOWN_DIFFERENCES.md](doc/KNOWN_DIFFERENCES.md): where the engine knowingly
  differs from the original.
- [UNCONFIRMED.md](doc/UNCONFIRMED.md): behaviour worked out but not yet
  confirmed, and what would settle each item.
- [TESTING.md](doc/TESTING.md): what to check in the playthrough, and how to
  report a problem.
- [REVERSE_ENGINEERING.md](doc/REVERSE_ENGINEERING.md): how the formats and
  behaviour were worked out.
- [formats.md](doc/formats.md): every format and system, field by field.

## Layout

```
sdl/        Platform backend. The only code that includes SDL.
            OSystem_SDL, graphics (SDL_Renderer), events, audio output, main().
engines/    Engine base class, MetaEngine registry, detection, launcher.
  stg/      The Star Trek: Generations engine.
common/     Platform independent services used by engines:
            system.h (OSystem), events, fs/archive/file, stream, config-manager,
            savefile, md5, random, debug, graphics/, audio/, image/.
test/       Unit tests, plus a headless OSystem that runs the engine without SDL.
```

Dependencies only go downward: `sdl → engines → common`. Engines reach the
display, input, clock, audio and storage only through `OSystem` and the
managers it hands out (`EventManager`, `Audio::Mixer`, `SaveFileManager`), the
same way ScummVM engines use `g_system`. The build enforces this. `common` and
`engines` are compiled without SDL's include paths, and the `layering` test
fails if either one includes an SDL header, or if `common` includes engine code.

Since engines never touch SDL, adding another backend later (a different
library, or a console port) means implementing `OSystem` and nothing else. The
test suite's `OSystem_Null` shows how little is needed.

## Building

You need CMake 3.16+, a C++17 compiler and Git.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

The build uses an installed SDL 3.2 or newer if it finds one. Otherwise it
downloads SDL3 and links it statically, so the executable doesn't need an SDL
DLL next to it. To control which SDL is used:

- An installed SDL3 in a non-standard location: add `-DCMAKE_PREFIX_PATH=/path/to/sdl3`
  (or `-DSDL3_DIR=/path/to/sdl3/lib/cmake/SDL3`).
- Always use the downloaded copy: add `-DSTG_VENDORED_SDL=ON`.
- Never download (for offline or packaging builds): add `-DSTG_DOWNLOAD_SDL=OFF`.

On Linux, the downloaded SDL can only use the windowing and audio systems
whose development headers are installed. On Debian or Ubuntu, install
`libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev
libwayland-dev libxkbcommon-dev libegl1-mesa-dev libasound2-dev libpulse-dev`
before configuring.

The executable is `build/sdl/stg` (`stg.exe` on Windows).

## Running

Point `--path` at the *installed* game folder, the one containing `sgens.exe`
and `DATA/`. The installer leaves `COMBAT.DAT`, `STELLAR.DAT`, the music and
the movies on the CDs. Pass the discs, or folders you've copied them to, with
`--cd1` and `--cd2`. Both are searched at once, so there's no disc swapping.

```sh
stg --path=/games/stg --cd1=D:/ --cd2=E:/   # installed game plus both discs
stg --add --path=/games/stg --cd1=... --cd2=...   # remember all three paths
stg /path/to/stg                 # same
stg --add --path=/path/to/stg    # remember it as a target in the config file
stg stg-win                      # run a remembered target
stg --detect --path=...          # only report what is detected
stg --dump-files --path=...      # file sizes and detection checksums
stg --probe --path=...           # identify each file's format; hex dump unknown ones
stg --extract --path=X.DAT --pattern='*.OVR' -o DIR   # extract files from an archive
stg --help                       # every option
```

Useful options: `-f` for fullscreen, `--filtering`, `--integer-scaling`,
`-d<level>` with `--debugflags=` `resources`, `scripts` and/or `sound` for
debug output (`-d2 --debugflags=scripts` logs every script event), and
`--engine=stg --game=stg` to force the engine on files detection doesn't
recognise.

In game: **Alt+Enter** toggles fullscreen and **Ctrl+Alt+F** toggles filtering.

### Playing

With disc 1's `STELLAR.DAT` (which holds the story script, `DESIGN/STATES.INI`)
and `OPTIONS.DAT`, the game starts as the original does: the title, then the
captain's menu (`CAPTAINS.BMP`). The original goes straight from the title into
a new game; here the menu comes first, so a saved game can be loaded instead. New starts the story: its movies,
then each away mission in the order the script gives, branching on whether its
objectives were met, through to one of the endings and the credits. Esc in a
mission opens the menu: Load, Save (nine slots plus the automatic save the
story makes), New, Quit (the credits, then out), Settings (effects and music
volume, difficulty, subtitles) and Controls (the keys: click a box and press
the key or mouse button for it, or Esc to give up; each action can have two,
on the away and starship pages, and Lookspring levels the view when Mouse
Look is let go). Settings, keys included, are kept in the settings file.

Between missions the story runs on the bridge, the star map of the ten
stars. Click a star, planet, station or fleet to select it; SET COURSE warps
the Enterprise there, FULL SECTOR VIEW / SYSTEM VIEW zoom, LONG / SHORT RANGE
SCAN reads what's there (short range only in the same system), SIM.
DESTRUCTION shows where the Nexus would go if a star died, and the button
under SET COURSE offers TRANSPORT (beam down), TACTICAL (engage) or the
COMPUTER LOG (the current hint). Time passes only as you act: a warp takes a
stardate, a scan a tenth, and the story's deadlines run on stardates. Space
skips the speech playing; Esc or OPTIONS opens the menu.

Space combat starts from the bridge, or when a hostile fleet is where the
Enterprise arrives. After the briefing (PROCEED), with the original's
default keys, fly by hand with the arrows or Num 8/2/4/6 (Up dives, Down
climbs), Ctrl and Num 0 to roll, Num + and Num - for speed, or give the helm
an order with 1-7 (Close on target, Evade, Maintain distance, Full stop (also
Num 5), Stand by, Warp out, Maintain status) or the menu the CONN line calls
up. Shift fires phasers, Enter photon torpedoes. The Controls screen changes
any of these; while the keys are free, Q / E also roll, = and - set the speed,
and F and G fire. T or the button by the radar picks the next target, V switches
between the forward and enemy views, the slider at the right of the
viewscreen zooms it, the one by the radar sets its range, holding the mouse on
the viewscreen steers towards it, the buttons at the foot of each systems panel
switch it between the system and shield facing displays, and clicking the
target's schematic
picks a system to aim at. Win by destroying or driving off every ship (or
beating Soran's); warping out loses the fight, and losing the Enterprise
ends the game. The enemies fly the original's own AI: they make attack runs
at the Enterprise, fire every seven seconds, swerve off before a collision and
break off when badly hurt, and Soran keeps his distance while he has company.

For testing, `--combat=N` goes straight into space combat N (1-25, the
`SCn.MIS` files), and `--story=SECTION` starts the story at any section of
`STATES.INI` as if a new game had got there, with the Enterprise at that
section's star: `--story=Amargosa2`, `--story=Veridian`, `--story=Halee1Or2`.
`--story=list` prints the section names to the console.

`--viewer` starts the resource viewer instead, which is also what runs when
the story's data isn't there.

### The resource viewer

The starting screen lists the away missions found in the game's data (the
fifteen locations sgens.exe knows, from AMARGOS1 to VERID1): Up/Down choose
one and Enter plays it in the 3D view, after its briefing picture (click PROCEED, or press Enter or Space). Esc leaves a
mission for the list, and a mission that ends itself returns there too, saying
whether its objectives were met. F5 saves a mission in progress and F7 picks
it up again.
In the viewer, Left/Right steps through images and PgUp/PgDn jumps between
archives. Tab and Shift+Tab jump to the next and previous 3D level (`GAME.LV2`),
whose name the 3D view shows on its status line. Away from the starting screen, Up/Down steps through sounds and Space replays the current one. On a level, `]`/`[` step
from the floor plan to a 3D view and then through the textures. In the 3D view, the
keys are the Controls screen's, by default the original's: the arrows walk and
turn, Num / and Num * slide, PgUp/PgDn look up and down, Home levels the view,
Enter jumps, Ctrl crouches while held, Shift runs, and holding the right
mouse button looks with the mouse (the further from the middle of the view,
the faster it turns). While the keys are free, WASD also walk and slide, End
levels the view, X jumps and C crouches. L switches the lighting off and on,
and Shift+C lets you walk through walls. Insert or the left
mouse button attacks: with the weapon in use (verid1 starts with the phaser),
held up in the view, it fires at the mouse cursor (or the centre of the view
when the cursor is elsewhere); with none, bare-handed at the centre. `[` and
`]` set the weapon's power, which is what a shot costs and the damage it does
(the phaser starts at 4 of 16; Shift+`[`/`]` still change pages). 1-9 use the
items carried, as clicking them would; the status line shows health, the item
in use, its power and charge. With the game's console art the 3D view takes
the top of the screen above the original's console: click inventory items to
use them (right click to have them described), drag them to another slot or
out of the inventory to drop them, click the close-up panel the scripts open
(Space uses it), and switch the left panel with its tabs or B (life), H (map)
and O (object). Home
returns to the starfield, F8 plays the movies in turn, F5/F7 save and load, and Esc quits (or, in a
mission, leaves it; when playing the story, it opens the menu).

Settings and saves go in the per-user directory SDL picks for the platform:
`~/.local/share/STG-Engine/stg/` on Linux, `%APPDATA%\STG-Engine\stg\` on
Windows, and `~/Library/Application Support/STG-Engine/stg/` on macOS. The
settings file is `stg.ini` and saves go in `saves/`.

## Adding a game version to detection

1. Run `stg --dump-files --path=/path/to/game`.
2. Pick two or three files unique to that release, usually the executable
   and a main data file.
3. Add an entry to `engines/stg/detection_tables.h` with their names, sizes and
   MD5s (the template is in the comment there).

## How the work was done

Everything was worked out from the original: the data formats by reading the
files, and the behaviour by disassembling `sgens.exe`. Addresses in the code's
comments (like `0x4A7C91`) point at the original functions each piece follows.
[doc/REVERSE_ENGINEERING.md](doc/REVERSE_ENGINEERING.md) describes the method,
and [doc/formats.md](doc/formats.md) records what is known about each format
and system.

New platform-neutral code (a decoder, a format reader) goes in `common/` so
other engines could use it. Anything specific to Generations stays in
`engines/stg/`.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE). *Star Trek* is a trademark of CBS
Studios / Paramount. This project is not affiliated with them or with
MicroProse.
