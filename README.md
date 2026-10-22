# STG-Engine

A reimplementation of the engine behind MicroProse's 1997 **Star Trek: Generations**,
so the game can run natively on modern systems. It is structured like
[ScummVM](https://www.scummvm.org/): a thin platform backend, a shared
platform-independent layer, and game engines that only use that layer. The
backend is built on **SDL3**.

You need your own copy of the original game. No game data is included.

## Status

The framework is in place and working:

| Area | State |
|------|-------|
| SDL3 backend: window, scaling, fullscreen, palette, software cursor, input, audio | Done |
| `common/`: OSystem interface, files and archives (case-insensitive), streams, config, saves, events, surfaces, palettes, BMP decoder, software mixer, PCM/WAV | Done |
| `engines/`: Engine/MetaEngine framework, table-driven detection, launcher and command line | Done |
| `engines/stg`: detection, resource index, screen, save/load, and a viewer for the game's BMP/WAV files | Done |
| `common/`: ZIP archives (stored and deflated, with our own DEFLATE decoder) | Done |
| `engines/stg`: open the game's `.DAT`/`MUSIC.*` archives (they are ZIP files) | Done |
| `engines/stg`: images (BMP), speech and music (WAV), subtitles (TXT), fonts (`.FNT`) | Done |
| `engines/stg`: first-person overlay animations (`.OVR`) | Done (frame height guessed) |
| `engines/stg`: 3D sprite sets (`.GS2`) | Done |
| `engines/stg`: `.GSF` sprite sets | Not started |
| `engines/stg`: scripts (`GAME.BIN`): format, interpreter, disassembler | Done |
| `engines/stg`: the game opcodes the scripts call | Most of verid1's decoded; timers, signals, the level, the player, actors, sound and subtitles run; combat and the HUD to do |
| `engines/stg`: levels (`GAME.LV2`): geometry, textures, sky, lighting, collision, actors, and a software 3D renderer to walk through them | Done (combat to do) |
| `engines/stg`: game logic: the 3D away missions, starship combat, stellar cartography | **Not started** |
| Cutscenes: Cinepak video with 22 kHz stereo PCM in AVI | Not started (well documented formats) |

Right now, the engine runs as a viewer for the game's data. You can browse
every image in its archives, and play every speech line and music track, with
speech subtitles drawn in the game's own font, and walk through each level in 3D
with its scripts running. The 2 CD Windows release is in the detection table.
Other versions are found by a fallback that looks for an executable mentioning
"Generations", and reported as unknown versions.

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
`-d<level>` and `--debugflags=resources` for debug output, and
`--engine=stg --game=stg` to force the engine on files detection doesn't
recognise.

In game: **Alt+Enter** toggles fullscreen and **Ctrl+Alt+F** toggles filtering.
In the viewer, Left/Right steps through images and PgUp/PgDn jumps between
archives. Tab and Shift+Tab jump to the next and previous 3D level (`GAME.LV2`),
whose name the 3D view shows on its status line. Up/Down steps through sounds and Space replays the current one. On a level, `]`/`[` step
from the floor plan to a 3D view and then through the textures. In the 3D view, the
arrows or WASD move, PgUp/PgDn look up and down, End levels the view, L switches
the lighting off and on, and C lets you walk through walls. Ctrl or the left
mouse button attacks at the crosshair: with the weapon in use (verid1 starts
with the phaser) or, with none, bare-handed. Until there is an inventory panel,
1-9 use the items carried, as clicking them would; the status line shows health
and the item in use. With the game's console art the 3D view takes the top of
the screen above the original's console: click inventory items to use them
(right click to have them described), click the close-up panel the scripts
open (Space uses it), and switch the left panel with its tabs or B (life),
H (map) and O (object). Home
returns to the starfield, F5/F7 save and load, and Esc quits.

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

## Next steps

The real work is reverse engineering the original game, then filling in
`engines/stg`:

1. **Inventory the data.** Done for the 2 CD Windows release: the `.DAT` and
   `MUSIC.*` files are plain ZIP archives, which `common/zip.h` reads.
   `stg --probe --path=<file>.DAT` lists an archive's members and identifies
   or hex dumps one member of each file type.
2. **Inner formats.** Most are standard (BMP, WAV, TXT). The `FONT`,
   `OVERLAY` and `GS2` sprite formats are decoded, and so is most of the `GAME.LV2` level format. See [doc/formats.md](doc/formats.md) for
   everything known so far, including the `SGEN` script format and
   its instruction set.
3. **Graphics and video.** Decode the sprite, background and palette formats
   into `Graphics::Surface`s, and add AVI and Cinepak decoders for the cutscenes.
4. **Sound and music.** Add an `Audio::AudioStream` for each audio encoding the
   game uses.
5. **Game logic.** Disassemble the executable (Ghidra or IDA) to recover the
   scene, script, inventory, dialogue and 3D sections. Port each one into its
   own file under `engines/stg/` as it's understood.

New platform-neutral code (a decoder, a format reader) goes in `common/` so
other engines could use it. Anything specific to Generations stays in
`engines/stg/`.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE). *Star Trek* is a trademark of CBS
Studios / Paramount. This project is not affiliated with them or with
MicroProse.
