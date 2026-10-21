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
| `engines/stg`: the game's own data formats, scripts, scenes, combat, FMV | **Not started**: needs reverse engineering |

Right now, running the engine against a game directory lists what's there,
lets you browse any `.BMP` images and play any `.WAV` sounds, and otherwise
shows a test pattern. The detection table has no entries yet, because no
release has been checksummed. Until it does, the game is found by a fallback
that looks for an executable mentioning "Generations", and reported as an
unknown version.

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

You need CMake 3.16+, a C++17 compiler, and SDL 3.2 or newer.

```sh
# With SDL3 installed (e.g. from your package manager or a source build):
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

If SDL3 isn't in a standard location, add `-DCMAKE_PREFIX_PATH=/path/to/sdl3`.
To have CMake download and build SDL3 itself, add `-DSTG_VENDORED_SDL=ON`.

The executable is `build/sdl/stg` (`stg.exe` on Windows).

## Running

```sh
stg --path=/path/to/stg          # detect and run the game in that directory
stg /path/to/stg                 # same
stg --add --path=/path/to/stg    # remember it as a target in the config file
stg stg-win                      # run a remembered target
stg --detect --path=...          # only report what is detected
stg --dump-files --path=...      # file sizes and detection checksums
stg --help                       # every option
```

Useful options: `-f` for fullscreen, `--filtering`, `--integer-scaling`,
`-d<level>` and `--debugflags=resources` for debug output, and
`--engine=stg --game=stg` to force the engine on files detection doesn't
recognise.

In game: **Alt+Enter** toggles fullscreen and **Ctrl+Alt+F** toggles filtering.
The current viewer also uses Left/Right to browse images, Space to play the next
sound, click for a test tone, F5/F7 to save/load, and Esc to quit.

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

1. **Inventory the data.** Use `--dump-files` on each release (DOS, Windows,
   demos, localised versions), add the detection entries, and find which file
   types are the game's own containers.
2. **Container formats.** Write an `Common::Archive` for each one and register it
   with `SearchMan` in `STGEngine::run()`. The rest of the engine can then open
   resources by name with `Common::File`.
3. **Graphics and video.** Decode the sprite, background and palette formats
   into `Graphics::Surface`s, and write a decoder for the cutscene format.
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
