-----------------------------------
_DISCLAIMER:_

Welcome to the **raylib game template**!

This template provides a base structure to start developing a small raylib game in plain C. The repo is also pre-configured with a default `LICENSE` (zlib/libpng) and a `README.md` (this one) to be properly filled by users. Feel free to change the LICENSE as required.

All the sections defined by `$(Data to Fill)` are expected to be edited and filled properly. It's recommended to delete this disclaimer message after editing this `README.md` file.

-----------------------------------

## Project Layout

Source is organized in layers, loosely following *Game Coding Complete*'s architecture:

```
assets/          # Game content (art, audio) — see CONVENTIONS.md for organization
src/
    app/         # Application layer: window/audio init, main loop, screen state machine
    game/        # Game logic + view per screen (Init/Update/Draw/Unload) — still fused,
                 # not yet split into separate logic/view modules
    platform/    # OS/packaging assets (icons, .rc, .plist, web shell) — not compiled code
    Makefile, Makefile.Android, CMakeLists.txt   # build scripts
```

`src/engine/` (event bus, process manager, resource cache) and `src/game/ai/` (FSM, steering,
pathfinding) are reserved for systems described in `.claude/skills/engine-architecture` and
`.claude/skills/engine-ai-behavior` — they don't exist yet and aren't scaffolded until actually
built.

-----------------------------------

## Getting Started with this template

### Windows: Visual Studio

- After extracting the zip, the parent folder `raylib-game-template` should exist in the same directory as `raylib` itself.  So, your file structure should look like this:
    - Some parent directory
        - `raylib`
            - the contents of https://github.com/raysan5/raylib
        - `raylib-game-template`
            - this `README.md` and all other raylib-game-template files
- If using Visual Studio, open projects/VS2022/raylib-game-template.sln
- Select on `raylib_game` in the solution explorer, then in the toolbar at the top, click `Project` > `Set as Startup Project`
- Now you're all set up!  Click `Local Windows Debugger` with the green play arrow and the project will run.

### Linux

When setting up this template on linux for the first time, install the dependencies from this page:
([Working on GNU Linux](https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux))

You can use this templates in a few ways: using Visual Studio, using CMake, or make your own build setup. This repository comes with Visual Studio and CMake already set up.

Chose one of the follow setup options that fit in you development environment.

### CLI: CMake (recommended on Linux)

Install the build tools and raylib's system dependencies:

```sh
sudo apt update && sudo apt install -y cmake build-essential git \
    libasound2-dev libx11-dev libxrandr-dev libxi-dev libxcursor-dev \
    libxext-dev libgl1-mesa-dev libglu1-mesa-dev
```

`CMakeLists.txt` fetches and builds raylib automatically (via `FetchContent`), so no manual clone is needed:

```sh
cmake -S . -B build
cmake --build build
```

The executable is generated at `build/raylib-game-template/raylib-game-template`. To run it:

```sh
./build/raylib-game-template/raylib-game-template
```

Or use the helper scripts at the repo root, which wrap the commands above:

```sh
./build.sh      # configure + compile
./run.sh        # run the already-built executable
./build-run.sh  # build.sh followed by run.sh
```

### CLI: Makefile

```sh
mkdir ~/raylib-gamejam && cd ~/raylib-gamejam
git clone --depth 1 --branch 6.0 https://github.com/raysan5/raylib
make -C raylib/src
git clone https://github.com/$(User Name)/$(Repo Name).git
cd $(Repo Name)
make -C src
src/raylib_game
```

### CLI: Web (WebAssembly)

Requires the [Emscripten SDK](https://emscripten.org/) (`emcc`), not just a C compiler:

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh     # puts emcc on PATH for this shell session
```

Build raylib itself for the web target, then the game, both via the raw Makefile (there's no
CMake+Emscripten path set up in this repo — `PLATFORM=Web` in `CMakeLists.txt` assumes a
toolchain file the project doesn't ship):

```sh
git clone --depth 1 --branch 6.0 https://github.com/raysan5/raylib
make -C raylib/src PLATFORM=PLATFORM_WEB RAYLIB_LIBTYPE=STATIC

cd src
make PLATFORM=PLATFORM_WEB BUILD_MODE=RELEASE RAYLIB_PATH=../../raylib
```

This produces `build/web/raylib_game.html`, `.js`, `.wasm`, and `.data` (the `assets/` content,
preloaded into a virtual filesystem the WASM binary reads at runtime) — same top-level `build/`
directory the CMake build already uses, just under a `web/` subfolder.

**Opening `raylib_game.html` directly from disk (`file://`) will not work** — the browser blocks
the `fetch`/XHR calls Emscripten uses to load the `.wasm` and `.data` files under `file://` for
security reasons. Serve it over HTTP instead:

```sh
cd build/web
python3 -m http.server 8765
# open http://localhost:8765/raylib_game.html
```

This template has been created to be used with raylib (www.raylib.com) and it's licensed under an unmodified zlib/libpng license.

_Copyright (c) 2014-2026 Ramon Santamaria ([@raysan5](https://github.com/raysan5))_

-----------------------------------

## $(Game Title)

![$(Game Title)](screenshots/screenshot000.png "$(Game Title)")

### Description

$(Your Game Description)

### Features

 - $(Game Feature 01)
 - $(Game Feature 02)
 - $(Game Feature 03)

### Controls

Keyboard:
 - $(Game Control 01)
 - $(Game Control 02)
 - $(Game Control 03)

### Screenshots

_TODO: Show your game to the world, animated GIFs recommended!._

### Developers

 - $(Developer 01) - $(Role/Tasks Developed)
 - $(Developer 02) - $(Role/Tasks Developed)
 - $(Developer 03) - $(Role/Tasks Developed)

### Links

 - YouTube Gameplay: $(YouTube Link)
 - itch.io Release: $(itch.io Game Page)
 - Steam Release: $(Steam Game Page)

### License

This project sources are licensed under an unmodified zlib/libpng license, which is an OSI-certified, BSD-like license that allows static linking with closed source software. Check [LICENSE](LICENSE) for further details.

$(Additional Licenses)

*Copyright (c) $(Year) $(User Name) ($(User Twitter/GitHub Name))*
