#**************************************************************************************************
#
#   raylib makefile for Desktop platforms, Web (Wasm), Raspberry Pi (DRM mode) and Android
#
#   Copyright (c) 2013-2026 Ramon Santamaria (@raysan5)
#
#   This software is provided "as-is", without any express or implied warranty. In no event
#   will the authors be held liable for any damages arising from the use of this software.
#
#   Permission is granted to anyone to use this software for any purpose, including commercial
#   applications, and to alter it and redistribute it freely, subject to the following restrictions:
#
#     1. The origin of this software must not be misrepresented; you must not claim that you
#     wrote the original software. If you use this software in a product, an acknowledgment
#     in the product documentation would be appreciated but is not required.
#
#     2. Altered source versions must be plainly marked as such, and must not be misrepresented
#     as being the original software.
#
#     3. This notice may not be removed or altered from any source distribution.
#
#**************************************************************************************************

.PHONY: all clean tests test-run resources check_game

# Define required environment variables
#------------------------------------------------------------------------------------------------
# Define target platform: PLATFORM_DESKTOP, PLATFORM_WEB, PLATFORM_DRM, PLATFORM_ANDROID
PLATFORM              ?= PLATFORM_DESKTOP

# Define project variables
PROJECT_NAME          ?= raylib_game
PROJECT_VERSION       ?= 1.0
# Local desktop dev default: keeps every build artifact (objects, the linked binary, staged
# resources, config/, the GAME_MARKER) out of src/ entirely, in a sibling build/ directory --
# nothing but source is left next to the .cpp/.h files. Release CI workflows
# (build_linux/macos/webassembly.yml) override this to a sibling build/release/ instead (their own
# packaging steps need a stable path to `cp`/`copy` the final artifact from) -- same "nothing but
# source in src/" property, just a different directory name; never back to PROJECT_BUILD_PATH=.
# (src/ itself) anymore, which this Makefile's own `clean:` target used to special-case for.
PROJECT_BUILD_PATH    ?= ../build/desktop

# Which concrete game module (src/game/<GAME>/) to build -- first needed once a second one
# (game/camera_fps, docs/adr/0017; game/flare_reactor, docs/rfc/0001-flare-reactor-pipeline-
# experiment.md) existed alongside game/sandbox. Deliberately does NOT change PROJECT_NAME/the
# actual linked target below -- switching GAME just changes what
# $(PROJECT_BUILD_PATH)/$(PROJECT_NAME) contains, same relink-detection reasoning as GAME_MARKER/
# check_game further down. build.sh additionally copies that binary to
# $(PROJECT_BUILD_PATH)/$(GAME) after linking, purely for local-dev convenience (so multiple builds
# -- e.g. sandbox, camera_fps, and flare_reactor -- can sit side by side under build/desktop/ under
# their own names); that copy step lives in build.sh, not here, so it never touches the release CI
# workflows, which call this Makefile directly and only ever reference $(PROJECT_NAME).
GAME                  ?= sandbox

# Web build artifacts (.html/.js/.wasm/.data) land in build/web/ instead of src/, so
# `make clean` and .gitignore only have to deal with one build output location
ifeq ($(PLATFORM),PLATFORM_WEB)
    PROJECT_BUILD_PATH := ../build/web
endif
ifeq ($(PLATFORM),PLATFORM_DRM)
    PROJECT_BUILD_PATH := ../build/drm
endif

# No plain-C sources remain (docs/adr/0014): the raylib template's screen_*.c files converted to
# .cpp as part of moving into src/game/sandbox/ -- see PROJECT_SOURCE_FILES_CPP below. Left empty,
# not deleted, as the seam a future genuinely-C game module could still use.
PROJECT_SOURCE_FILES  ?=

# C++ sources: the game-agnostic app/ engine (always built, regardless of GAME) plus whichever
# concrete game module GAME selects (docs/adr/0014 -- src/app/ must never name a specific game;
# src/game/<game-id>/ owns its own composition root, screens, and concrete IGameView).
# entity_file_parser_yaml.cpp (docs/adr/0008) and engine_config.cpp (docs/adr/0011) are here
# because Engine::Init() now actually calls LoadOrCreateEngineConfig() -> YamlEntityFileParser ->
# mini-yaml. level_loader.cpp and base_game_logic.cpp (docs/adr/0010) join them for the same
# reason: game/sandbox/screen_gameplay.cpp's InitGameplayScreen() and game/flare_reactor's
# FlareReactorGameLogic are both real callers of BaseGameLogic/LevelLoader/EntityFactory, not just
# their own unit tests -- game/camera_fps doesn't call into either, but they're cheap enough to
# always link in rather than making them conditional too. input_bindings.cpp (docs/adr/0013) joins
# them once game/flare_reactor's FlareReactorView became its first real caller.
# app/view/human_view_base.cpp (docs/adr/0016, promoted out of game/sandbox/human_view in
# docs/adr/0017) is the generic half of every concrete "human" IGameView -- all three game modules
# below subclass it.
PROJECT_SOURCE_FILES_CPP ?= \
    app/core/engine.cpp \
    app/process/process_manager.cpp \
    app/view/debug_overlay.cpp \
    app/entity/entity_file_parser_yaml.cpp \
    app/core/engine_config.cpp \
    app/entity/level_loader.cpp \
    app/view/base_game_logic.cpp \
    app/view/human_view_base.cpp \
    app/input/input_bindings.cpp

# game/sandbox (docs/adr/0014): the migrated raylib template, with a Logo/Title/Options/Gameplay/
# Ending screen state machine and data-driven (YAML) level content. game_config.cpp is
# GameConfig's first real caller (docs/adr/0011) -- main.cpp reads its characterTexturePath/
# coinSoundPath instead of hardcoding them, the same YamlEntityFileParser/mini-yaml round trip
# engine_config.cpp already does.
ifeq ($(GAME),sandbox)
PROJECT_SOURCE_FILES_CPP += \
    game/sandbox/main.cpp \
    game/sandbox/screen_logo.cpp \
    game/sandbox/screen_title.cpp \
    game/sandbox/screen_options.cpp \
    game/sandbox/screen_gameplay.cpp \
    game/sandbox/screen_ending.cpp \
    game/sandbox/human_view.cpp \
    game/sandbox/game_config.cpp
endif

# game/camera_fps (docs/adr/0017): raylib's own examples/core/core_3d_camera_fps.c ported in as
# the second concrete game module -- a single Engine-driven scene, no screens.h state machine, no
# GameConfig (no assets to point at). Its level/entities (player + 4 towers) ARE data-driven
# (assets/levels/camera_fps.yaml). game_logic.cpp's CameraFpsLogic (a real BaseGameLogic subclass)
# owns the FPS movement simulation; human_view.cpp's CameraFpsView only reads input and renders.
ifeq ($(GAME),camera_fps)
PROJECT_SOURCE_FILES_CPP += \
    game/camera_fps/main.cpp \
    game/camera_fps/game_logic.cpp \
    game/camera_fps/human_view.cpp
endif

# game/flare_reactor (docs/rfc/0001-flare-reactor-pipeline-experiment.md): the end-to-end pipeline
# experiment -- a single Engine-driven scene, no screens.h state machine, no GameConfig (no
# game-specific settings needed yet). Phase 1 ("esqueleto"): three data-driven entities
# (player/reactor/sentinel) rendered via app/scene/renderable.h's Renderable. Phase 3 ("evento +
# validação") adds game_logic.cpp -- FlareReactorGameLogic, this module's first BaseGameLogic
# subclass. Phase 6 ("AI") adds ai_view.h -- AIView, one per SentinelTag actor; header-only, same
# shape as app/scene/renderable.h, so it needs no entry of its own here.
ifeq ($(GAME),flare_reactor)
PROJECT_SOURCE_FILES_CPP += \
    game/flare_reactor/main.cpp \
    game/flare_reactor/human_view.cpp \
    game/flare_reactor/game_logic.cpp \
    game/flare_reactor/game_config.cpp \
    game/flare_reactor/skybox.cpp \
    game/flare_reactor/lighting.cpp
endif

# Source location of game content, staged into ./resources before linking (see 'resources' target)
PROJECT_RESOURCES_PATH ?= ../assets

# raylib library variables
RAYLIB_PATH           ?= C:\raylib\raylib
RAYLIB_INCLUDE_PATH   ?= $(RAYLIB_PATH)/src
RAYLIB_LIB_PATH       ?= $(RAYLIB_PATH)/src
# Library type used for raylib: STATIC (.a) or SHARED (.so/.dll)
RAYLIB_LIBTYPE        ?= STATIC

# EnTT (header-only; only an include path is needed, no library to build/link).
# ENTT_PATH should point at a checkout of https://github.com/skypjack/entt (see CI workflows,
# which check it out as a sibling directory next to raylib/); single_include/ has the amalgamated
# entt/entt.hpp header so no build step or additional include roots are required.
ENTT_PATH             ?= C:\entt\entt
ENTT_INCLUDE_PATH      = $(ENTT_PATH)/single_include

# doctest (header-only; only an include path is needed, no library to build/link). See
# docs/adr/0006-doctest-for-unit-tests.md. DOCTEST_PATH should point at a checkout of
# https://github.com/doctest/doctest (see test.sh and ci_sanity.yml, which check it out as a
# sibling directory next to raylib/ and entt/); the header lives at doctest/doctest.h inside that
# checkout, matching the #include "doctest/doctest.h" used by tests/*.cpp.
DOCTEST_PATH          ?= C:\doctest\doctest
DOCTEST_INCLUDE_PATH   = $(DOCTEST_PATH)

# mini-yaml (docs/adr/0008): one header (yaml/Yaml.hpp) + one small .cpp (yaml/Yaml.cpp), the same
# vendoring shape EnTT/doctest already use, just not header-only -- Yaml.cpp needs compiling
# wherever entity_file_parser_yaml.cpp is (currently: only the test build; nothing in the main
# product consumes ADR-0008's pieces yet, that's ADR-0009's job). No release tags exist upstream
# (see test.sh's comment), so MINI_YAML_PATH is expected to point at a checkout pinned by commit
# SHA, not a tag. yaml/Yaml.hpp lives directly under the checkout root, so unlike ENTT_INCLUDE_PATH
# there's no single_include-style subdirectory to add.
MINI_YAML_PATH        ?= C:\mini-yaml\mini-yaml
MINI_YAML_INCLUDE_PATH = $(MINI_YAML_PATH)

# Build mode for project: DEBUG or RELEASE
BUILD_MODE            ?= RELEASE

# PLATFORM_WEB: Default properties
BUILD_WEB_ASYNCIFY    ?= FALSE
BUILD_WEB_SHELL       ?= platform/minshell.html
BUILD_WEB_HEAP_SIZE   ?= 128MB
BUILD_WEB_STACK_SIZE  ?= 1MB
BUILD_WEB_ASYNCIFY_STACK_SIZE ?= 1048576
BUILD_WEB_RESOURCES   ?= TRUE
BUILD_WEB_RESOURCES_PATH  ?= $(PROJECT_BUILD_PATH)/resources

# Determine PLATFORM_OS in case PLATFORM_DESKTOP selected
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    # No uname.exe on MinGW!, but OS=Windows_NT on Windows!
    # ifeq ($(UNAME),Msys) -> Windows
    ifeq ($(OS),Windows_NT)
        PLATFORM_OS = WINDOWS
    else
        UNAMEOS = $(shell uname)
        ifeq ($(UNAMEOS),Linux)
            PLATFORM_OS = LINUX
        endif
        ifeq ($(UNAMEOS),FreeBSD)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),OpenBSD)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),NetBSD)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),DragonFly)
            PLATFORM_OS = BSD
        endif
        ifeq ($(UNAMEOS),Darwin)
            PLATFORM_OS = OSX
        endif
    endif
endif
ifeq ($(PLATFORM),PLATFORM_DRM)
    UNAMEOS = $(shell uname)
    ifeq ($(UNAMEOS),Linux)
        PLATFORM_OS = LINUX
    endif
endif

ifeq ($(PLATFORM),PLATFORM_WEB)
    # Emscripten required variables
    EMSDK_PATH         ?= C:/raylib/emsdk
    EMSCRIPTEN_PATH    ?= $(EMSDK_PATH)/upstream/emscripten
    CLANG_PATH          = $(EMSDK_PATH)/upstream/bin
    PYTHON_PATH         = $(EMSDK_PATH)/python/3.13.3_64bit
    NODE_PATH           = $(EMSDK_PATH)/node/22.16.0_64bit/bin
    # NOTE: Using := instead of = so Make expands it immediately and avoids 
    # creating a recursively-expanded variable that references itself
    export PATH        := $(EMSDK_PATH);$(EMSCRIPTEN_PATH);$(CLANG_PATH);$(NODE_PATH);$(PYTHON_PATH);$(PATH)
endif

# Define default C compiler: CC
#------------------------------------------------------------------------------------------------
CC = gcc

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),OSX)
        # OSX default compiler
        CC = clang
    endif
    ifeq ($(PLATFORM_OS),BSD)
        # FreeBSD, OpenBSD, NetBSD, DragonFly default compiler
        CC = clang
    endif
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    # HTML5 emscripten compiler
    # WARNING: To compile to HTML5, code must be redesigned
    # to use emscripten.h and emscripten_set_main_loop()
    CC = emcc
endif
ifeq ($(PLATFORM),PLATFORM_DRM)
    ifeq ($(USE_RPI_CROSS_COMPILER),TRUE)
        # Define RPI cross-compiler
        #CC = armv6j-hardfloat-linux-gnueabi-gcc
        CC = $(RPI_TOOLCHAIN)/bin/arm-linux-gnueabihf-gcc
    endif
endif

# Define default C++ compiler: CXX (for PROJECT_SOURCE_FILES_CPP; mirrors CC's platform logic)
#------------------------------------------------------------------------------------------------
CXX = g++

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),OSX)
        CXX = clang++
    endif
    ifeq ($(PLATFORM_OS),BSD)
        CXX = clang++
    endif
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    CXX = em++
endif

# Define default make program: MAKE
#------------------------------------------------------------------------------------------------
MAKE ?= make

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
        MAKE = mingw32-make
    endif
endif

# Define compiler flags: CFLAGS
#------------------------------------------------------------------------------------------------
#  -O1                  defines optimization level
#  -g                   include debug information on compilation
#  -s                   strip unnecessary data from build
#  -Wall                turns on most, but not all, compiler warnings
#  -std=c99             defines C language mode (standard C from 1999 revision)
#  -std=gnu99           defines C language mode (GNU C from 1999 revision)
#  -Wno-missing-braces  ignore invalid warning (GCC bug 53119)
#  -Wno-unused-value    ignore unused return values of some functions (i.e. fread())
#  -D_DEFAULT_SOURCE    use with -std=c99 on Linux and PLATFORM_WEB, required for timespec
#  -MMD -MP             emit a %.d dependency file per object (project headers only, not system
#                        ones; -MP adds a phony target per header so a deleted/renamed header
#                        doesn't hard-error make instead of just forcing a rebuild) -- without this,
#                        editing a header (e.g. a shared component definition) doesn't invalidate
#                        any .cpp that only reaches it transitively, since GNU Make's built-in %.o:%.cpp
#                        rule below only ever depends on the .cpp file itself. Confirmed causing a
#                        real bug, not just a theoretical staleness risk: adding Renderable::model
#                        (app/scene/renderable.h) left game_logic.o linked against the old, smaller
#                        struct layout because nothing touched game_logic.cpp itself, silently
#                        corrupting the very Renderable BeaconPulseProcess writes through -- the
#                        reactor model would vanish the instant the beacon pulse's first Update() ran.
#                        The generated .d files are -include'd near the end of this Makefile.
CFLAGS = -Wall -D_DEFAULT_SOURCE -Wno-missing-braces -Wno-unused-value -Wno-unused-result -Wno-pointer-sign -MMD -MP $(PROJECT_CUSTOM_FLAGS)
#CFLAGS += -Wextra -Wmissing-prototypes -Wstrict-prototypes

ifeq ($(PLATFORM),PLATFORM_WEB)
    CFLAGS += -std=gnu99
else
    CFLAGS += -std=c99
endif

ifeq ($(BUILD_MODE),DEBUG)
    CFLAGS += -g -D_DEBUG
else
    ifeq ($(PLATFORM),PLATFORM_WEB)
        ifeq ($(BUILD_WEB_ASYNCIFY),TRUE)
            CFLAGS += -O3
        else
            CFLAGS += -Os
        endif
    else
        ifeq ($(PLATFORM_OS),OSX)
            CFLAGS += -O2
        else
            CFLAGS += -s -O2
        endif
    endif
endif
ifeq ($(PLATFORM),PLATFORM_DRM)
    CFLAGS += -std=gnu99 -DEGL_NO_X11
endif

# CXXFLAGS: everything CFLAGS has (warnings, optimization, PROJECT_CUSTOM_FLAGS, ...) with the C
# language-standard flag swapped for its C++ equivalent (EnTT requires C++20) rather than
# recomputing platform/optimization logic a second time. -Wno-pointer-sign is C/ObjC-only and
# filtered out to avoid a "valid for C/ObjC but not for C++" compiler warning.
CXXFLAGS = $(filter-out -Wno-pointer-sign,$(patsubst -std=c99,-std=c++20,$(patsubst -std=gnu99,-std=gnu++20,$(CFLAGS))))

# Define include paths for required headers: INCLUDE_PATHS
#------------------------------------------------------------------------------------------------
INCLUDE_PATHS += -I. -Iexternal -I$(RAYLIB_INCLUDE_PATH) -I$(ENTT_INCLUDE_PATH) -I$(MINI_YAML_INCLUDE_PATH)

# Define additional directories containing required header files
ifeq ($(PLATFORM),PLATFORM_DRM)
    # DRM required libraries
    INCLUDE_PATHS += -I/usr/include/libdrm
endif
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),BSD)
        # Consider -L$(RAYLIB_H_INSTALL_PATH)
        INCLUDE_PATHS += -I/usr/local/include
    endif
endif

# Define library paths containing required libs: LDFLAGS
#------------------------------------------------------------------------------------------------
LDFLAGS = -L. -L$(RAYLIB_LIB_PATH)

ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
        # NOTE: The resource .rc file contains windows executable icon and properties
        LDFLAGS += $(RAYLIB_PATH)/src/raylib.rc.data
        # -Wl,--subsystem,windows hides the console window
        ifeq ($(BUILD_MODE), RELEASE)
            LDFLAGS += -Wl,--subsystem,windows
        endif
    endif
    ifeq ($(PLATFORM_OS),BSD)
        # Consider -L$(RAYLIB_INSTALL_PATH)
        LDFLAGS += -Lsrc -L/usr/local/lib
    endif
    ifeq ($(PLATFORM_OS),LINUX)
        # Reset everything.
        # Precedence: immediately local, installed version, raysan5 provided libs
        #LDFLAGS += -L$(RAYLIB_RELEASE_PATH)
    endif
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    # -Os                        # size optimization
    # -O2                        # optimization level 2, if used, also set --memory-init-file 0
    # -sUSE_GLFW=3               # Use glfw3 library (context/input management)
    # -sALLOW_MEMORY_GROWTH=1    # to allow memory resizing -> WARNING: Audio buffers could FAIL!
    # -sTOTAL_MEMORY=16777216    # to specify heap memory size (default = 16MB) (67108864 = 64MB)
    # -sUSE_PTHREADS=1           # multithreading support
    # -sWASM=0                   # disable Web Assembly, emitted by default
    # -sASYNCIFY                 # lets synchronous C/C++ code interact with asynchronous JS
    # -sFORCE_FILESYSTEM=1       # force filesystem to load/save files data
    # -sASSERTIONS=1             # enable runtime checks for common memory allocation errors (-O1 and above turn it off)
    # -sMINIFY_HTML=0            # minify generated html from shell.html
    # --profiling                # include information for code profiling
    # --memory-init-file 0       # to avoid an external memory initialization code file (.mem)
    # --preload-file resources   # specify a resources folder for data compilation
    # --source-map-base          # allow debugging in browser with source map
    # --shell-file shell.html    # define a custom shell .html and output extension
    LDFLAGS += -sUSE_GLFW=3 -sTOTAL_MEMORY=$(BUILD_WEB_HEAP_SIZE) -sSTACK_SIZE=$(BUILD_WEB_STACK_SIZE) -sFORCE_FILESYSTEM=1 -sMINIFY_HTML=0

    # Build using asyncify
    ifeq ($(BUILD_WEB_ASYNCIFY),TRUE)
        LDFLAGS += -sASYNCIFY -sASYNCIFY_STACK_SIZE=$(BUILD_WEB_ASYNCIFY_STACK_SIZE)
    endif

    # Add resources building if required
    ifeq ($(BUILD_WEB_RESOURCES),TRUE)
        LDFLAGS += --preload-file $(BUILD_WEB_RESOURCES_PATH)
    endif

    # Add debug mode flags if required
    ifeq ($(BUILD_MODE),DEBUG)
        LDFLAGS += -sASSERTIONS=1 --profiling
    endif

    # Define a custom shell .html and output extension
    LDFLAGS += --shell-file $(BUILD_WEB_SHELL)
    EXT = .html
endif

# Define libraries required on linking: LDLIBS
# NOTE: To link libraries (lib<name>.so or lib<name>.a), use -l<name>
#------------------------------------------------------------------------------------------------
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
        # Libraries for Windows desktop compilation
        # NOTE: WinMM library required to set high-res timer resolution
        LDLIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -lcomdlg32 -lole32
    endif
    ifeq ($(PLATFORM_OS),LINUX)
        # Libraries for Debian GNU/Linux desktop compiling
        # NOTE: Required packages: libegl1-mesa-dev
        LDLIBS = -lraylib -lGL -lm -lpthread -ldl -lrt

        # On Wayland windowing system, additional libraries requires
        ifeq ($(USE_WAYLAND_DISPLAY),TRUE)
            LDLIBS += -lwayland-client -lwayland-cursor -lwayland-egl -lxkbcommon
        else
            # On X11 requires also below libraries
            LDLIBS += -lX11
            # NOTE: It seems additional libraries are not required any more, latest GLFW just dlopen them
            #LDLIBS += -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor
        endif
        # Explicit link to libc
        ifeq ($(RAYLIB_LIBTYPE),SHARED)
            LDLIBS += -lc
        endif
    endif
    ifeq ($(PLATFORM_OS),OSX)
        # Libraries for OSX 10.9 desktop compiling
        # NOTE: Required packages: libopenal-dev libegl1-mesa-dev
        LDLIBS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo
    endif
    ifeq ($(PLATFORM_OS),BSD)
        # Libraries for FreeBSD, OpenBSD, NetBSD, DragonFly desktop compiling
        # NOTE: Required packages: mesa-libs
        LDLIBS = -lraylib -lGL -lpthread -lm

        # On XWindow requires also below libraries
        LDLIBS += -lX11 -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor
    endif
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
    # Libraries for web (HTML5) compiling
    LDLIBS = $(RAYLIB_LIB_PATH)/libraylib.web.a
endif
ifeq ($(PLATFORM),PLATFORM_DRM)
    # Libraries for DRM compiling
    # NOTE: Required packages: libasound2-dev (ALSA)
    LDLIBS = -lraylib -lGLESv2 -lEGL -lpthread -lrt -lm -lgbm -ldrm -ldl
endif


# Define all object files from source files
#------------------------------------------------------------------------------------------------
# Every object lands under $(OBJ_DIR), mirroring its source path (e.g. app/core/engine.cpp ->
# $(OBJ_DIR)/app/core/engine.o) -- keeps src/ itself free of build artifacts, so an editor's file
# tree or `git status` isn't cluttered with .o files sitting right next to the .cpp that produced
# them. mini_yaml.o: mini-yaml (docs/adr/0008) lives outside src/ (vendored, like raylib/EnTT), so
# it can't go through PROJECT_SOURCE_FILES_CPP's pattern rule the way in-tree sources do -- gets
# its own object + rule below, same as the test build's mini_yaml.o does.
OBJ_DIR = $(PROJECT_BUILD_PATH)/obj
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(PROJECT_SOURCE_FILES)) $(patsubst %.cpp, $(OBJ_DIR)/%.o, $(PROJECT_SOURCE_FILES_CPP)) $(OBJ_DIR)/mini_yaml.o

# Define processes to execute
#------------------------------------------------------------------------------------------------
# For Android platform we call a custom Makefile.Android
ifeq ($(PLATFORM),PLATFORM_ANDROID)
    MAKEFILE_TARGET = -f Makefile.Android
    export PROJECT_NAME
    export PROJECT_SOURCE_FILES
else
    MAKEFILE_TARGET = $(PROJECT_BUILD_PATH)/$(PROJECT_NAME)$(EXT)
endif

# Default target entry
all:
	$(MAKE) $(MAKEFILE_TARGET)

# Marker tracking which GAME the current $(PROJECT_NAME) binary was actually built for. Without
# this, switching GAME (e.g. sandbox -> flare_reactor) can silently relink nothing: each GAME's own
# game/<GAME>/*.o may already be individually up to date (left over from an earlier build of that
# same GAME), so if $(PROJECT_NAME) itself happens to be newer than every file in the *new* GAME's
# $(OBJS) list too (e.g. because some *other* GAME was linked more recently), `make` considers
# $(PROJECT_NAME) already up to date and does nothing -- leaving the *previous* GAME's binary in
# place with no error or warning. Reproduced for real switching sandbox -> flare_reactor mid-
# session. check_game (always phony, so its recipe always runs) only rewrites this file -- and
# therefore only bumps its mtime -- when GAME actually differs from the last build, forcing exactly
# one extra relink per GAME switch and no relink otherwise.
GAME_MARKER = $(PROJECT_BUILD_PATH)/.built_game
check_game:
	@if [ ! -f $(GAME_MARKER) ] || [ "$$(cat $(GAME_MARKER) 2>/dev/null)" != "$(GAME)" ]; then \
		echo "$(GAME)" > $(GAME_MARKER); \
	fi

# Project target defined by PROJECT_NAME
# NOTE: Linked with $(CXX), not $(CC) -- OBJS includes C++ object files (see
# PROJECT_SOURCE_FILES_CPP), and linking those requires the C++ standard library, which the C
# compiler driver doesn't pull in automatically. Target is $(PROJECT_BUILD_PATH)/$(PROJECT_NAME),
# not the bare $(PROJECT_NAME) this used to be -- needed once PROJECT_BUILD_PATH stopped defaulting
# to "." (the CI release workflows pass their own explicit PROJECT_BUILD_PATH=../build/release now
# too, so their $(PROJECT_NAME)-named output just lands one directory over, same as local dev's).
$(PROJECT_BUILD_PATH)/$(PROJECT_NAME)$(EXT): $(OBJS) resources $(GAME_MARKER)
	@mkdir -p $(PROJECT_BUILD_PATH)
	$(CXX) -o $@ $(OBJS) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

$(GAME_MARKER): check_game

# Stage game content next to the built executable (docs/adr's own asset staging note). Always
# phony -- assets/ can gain/change files (e.g. a new game module's entities/levels) between builds,
# and cp -r is cheap enough at this project's asset-set size that unconditionally re-syncing every
# build is simpler than tracking individual asset file staleness.
resources:
	@mkdir -p $(PROJECT_BUILD_PATH)/resources
	@cp -r $(PROJECT_RESOURCES_PATH)/. $(PROJECT_BUILD_PATH)/resources

# Compile source files
# NOTE: This pattern will compile every module defined on $(OBJS). @mkdir -p $(dir $@) first --
# unlike the old in-place %.o:%.cpp rule, the target's directory (e.g. $(OBJ_DIR)/app/core/)
# doesn't already exist just because the source directory does.
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

# NOTE: PROJECT_SOURCE_FILES_CPP entries (EnTT-using code) compile through this pattern instead,
# with the C++ standard (CXXFLAGS) and the C++ compiler driver (CXX).
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

# NOTE: deliberately no prerequisite on this rule's own line, same reason as the test build's
# mini_yaml.o rule below -- MINI_YAML_PATH's default placeholder is a Windows path
# ("C:\mini-yaml\mini-yaml"), and a colon inside a prerequisite makes GNU Make misparse the line as
# a static pattern rule.
$(OBJ_DIR)/mini_yaml.o:
	@mkdir -p $(dir $@)
	$(CXX) -c $(MINI_YAML_PATH)/yaml/Yaml.cpp -o $@ $(CXXFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

# Unit tests (doctest, see docs/adr/0006-doctest-for-unit-tests.md)
#------------------------------------------------------------------------------------------------
# Mostly independent of the PLATFORM machinery above: everything under tests/ needs no window, GL,
# or platform library, or linking against libraylib.a -- just CXX/CXXFLAGS plus whichever headers
# a given test includes. Most tests exercise pure-logic systems (EventManager, ProcessManager,
# ResourceCache) with no raylib or EnTT dependency at all; ResourceCache is templated over its
# Load/Unload pair (docs/adr/0004), so its test exercises it with a fake int-based loader/unloader
# rather than a real raylib type, the same reason it needs neither header. hierarchy_test.cpp
# (docs/adr/0002) is the one exception needing real headers -- entt::registry and raylib's
# Vector3/Quaternion/Matrix math (raymath.h) -- but raymath.h's functions are header-only
# (RMAPI/static inline), so this still never links libraylib.a or opens a window/GL context.
# entity_factory_test.cpp (docs/adr/0008) needs entt::registry too, same reasoning as hierarchy's.
# entity_file_parser_yaml_test.cpp needs mini-yaml, which is genuinely compiled (not header-only) --
# see the tests/entity_file_parser_yaml.o rule below. level_loader_test.cpp (docs/adr/0009) needs
# entt::registry + mini-yaml too, for the same reasons. engine_config_test.cpp/file_io_test.cpp/
# game_config_test.cpp/input_bindings_test.cpp (docs/adr/0011, docs/adr/0013) do real file I/O
# against a gitignored scratch directory (test_scratch/), not fakes -- their whole job is that
# read/write round trip. input_bindings_test.cpp needs raylib's headers too (KEY_* constants,
# InputAction's default bindings) but never calls IsKeyDown/IsKeyPressed themselves (those aren't
# exercised by a test -- same raylib-runtime-vs-headers-only split as hierarchy_test.cpp).
# beacon_pulse_process_test.cpp (docs/rfc/0001 Phase 4) tests game/flare_reactor/
# beacon_pulse_process.h directly -- header-only, same reasoning as hierarchy.h, so it needs no
# tests/-local object rule below; it deliberately avoids raylib's own ColorLerp (a real linked
# symbol) for exactly this reason (see that header's own comment). sentinel_ai_test.cpp
# (docs/rfc/0001 Phase 6) tests game/flare_reactor/sentinel_ai.h the same way -- also header-only,
# also no linked raylib symbols, also no tests/-local object rule needed.
TEST_SOURCE_FILES_CPP ?= \
    tests/main_test.cpp \
    tests/event_manager_test.cpp \
    tests/process_manager_test.cpp \
    tests/resource_cache_test.cpp \
    tests/event_type_id_test.cpp \
    tests/byte_stream_test.cpp \
    tests/serializable_event_test.cpp \
    tests/hierarchy_test.cpp \
    tests/entity_def_test.cpp \
    tests/entity_file_parser_yaml_test.cpp \
    tests/entity_factory_test.cpp \
    tests/level_loader_test.cpp \
    tests/file_io_test.cpp \
    tests/engine_config_test.cpp \
    tests/game_config_test.cpp \
    tests/base_game_logic_test.cpp \
    tests/input_bindings_test.cpp \
    tests/beacon_pulse_process_test.cpp \
    tests/sentinel_ai_test.cpp

TEST_CXXFLAGS = $(CXXFLAGS) -I. -I$(DOCTEST_INCLUDE_PATH) -I$(RAYLIB_INCLUDE_PATH) -I$(ENTT_INCLUDE_PATH) -I$(MINI_YAML_INCLUDE_PATH)

# Test build artifacts get their own build/ tree, sibling to the product's (../build/desktop/obj
# above) -- same reason (keep tests/ itself free of .o files), plus they genuinely can't share
# objects with the product build even for identical sources (TEST_CXXFLAGS omits -D$(PLATFORM) and
# adds doctest's include path; see the comment below on why e.g. process_manager.cpp gets compiled
# twice, once per flag set).
TEST_BUILD_PATH ?= ../build/test
TEST_OBJ_DIR = $(TEST_BUILD_PATH)/obj

# ProcessManager's implementation lives in app/process/process_manager.cpp (EventManager is
# header-only, needs no object of its own). Built into a $(TEST_OBJ_DIR)-local object, never
# $(OBJ_DIR)/app/process/process_manager.o, so it can't collide with the object the main product
# target above produces from the same source with different flags. entity_file_parser_yaml.cpp/
# engine_config.cpp/level_loader.cpp/base_game_logic.cpp/game/sandbox/game_config.cpp (all now also
# in PROJECT_SOURCE_FILES_CPP, docs/adr/0008/0009/0010/0011) get the same treatment, so the test
# build's flags (TEST_CXXFLAGS) never collide with the main product's (CXXFLAGS).
# game/sandbox/human_view.cpp and game/sandbox/screen_gameplay.cpp
# (docs/adr/0010, docs/adr/0014) deliberately get no such object -- both call real raylib runtime
# functions (IsKeyDown, DrawCubeWires, BeginMode3D, ...), and the test build never links -lraylib
# (see TEST_CXXFLAGS above: raylib's *headers* are on the include path for types like Camera3D, but
# nothing links its library), so either would fail at link time.
# base_game_logic.cpp has no such dependency (only entt::registry,
# EventManager, ProcessManager, LevelLoader -- none of them touch raylib), so it's tested the same
# way level_loader.cpp already is, with a FakeView standing in for HumanView. mini-yaml isn't
# header-only either, so its own yaml/Yaml.cpp needs compiling into the test binary too
# ($(TEST_OBJ_DIR)/mini_yaml.o below). Paths below reflect app/'s topic-subfolder split
# (core/events/process/scene/entity/view/input/resource/io) -- same reasoning as always, just
# relocated.
TEST_OBJS = $(patsubst tests/%.cpp, $(TEST_OBJ_DIR)/tests/%.o, $(TEST_SOURCE_FILES_CPP)) $(TEST_OBJ_DIR)/app/process/process_manager.o $(TEST_OBJ_DIR)/app/entity/entity_file_parser_yaml.o $(TEST_OBJ_DIR)/app/core/engine_config.o $(TEST_OBJ_DIR)/app/entity/level_loader.o $(TEST_OBJ_DIR)/app/view/base_game_logic.o $(TEST_OBJ_DIR)/game/sandbox/game_config.o $(TEST_OBJ_DIR)/app/input/input_bindings.o $(TEST_OBJ_DIR)/mini_yaml.o

tests: $(TEST_OBJS)
	@mkdir -p $(TEST_BUILD_PATH)
	$(CXX) -o $(TEST_BUILD_PATH)/tests_runner $(TEST_OBJS) $(TEST_CXXFLAGS)

test-run: tests
	$(TEST_BUILD_PATH)/tests_runner

$(TEST_OBJ_DIR)/app/process/process_manager.o: app/process/process_manager.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

$(TEST_OBJ_DIR)/app/entity/entity_file_parser_yaml.o: app/entity/entity_file_parser_yaml.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

$(TEST_OBJ_DIR)/app/core/engine_config.o: app/core/engine_config.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

$(TEST_OBJ_DIR)/app/entity/level_loader.o: app/entity/level_loader.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

$(TEST_OBJ_DIR)/app/view/base_game_logic.o: app/view/base_game_logic.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

$(TEST_OBJ_DIR)/game/sandbox/game_config.o: game/sandbox/game_config.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

$(TEST_OBJ_DIR)/app/input/input_bindings.o: app/input/input_bindings.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

# NOTE: deliberately no prerequisite on this rule's own line (unlike process_manager.o/
# entity_file_parser_yaml.o above) -- MINI_YAML_PATH's default placeholder value is a Windows path
# ("C:\mini-yaml\mini-yaml"), and a colon inside a prerequisite makes GNU Make misparse the whole
# line as a static pattern rule ("target pattern contains no '%'"), even on platforms that never
# hit this rule's recipe. Referencing $(MINI_YAML_PATH) only inside the recipe body sidesteps that
# parse-time failure; the tradeoff is Make can't tell if Yaml.cpp itself changed and needs a
# rebuild, which is fine for a pinned, never-locally-edited vendored file.
$(TEST_OBJ_DIR)/mini_yaml.o:
	@mkdir -p $(dir $@)
	$(CXX) -c $(MINI_YAML_PATH)/yaml/Yaml.cpp -o $@ $(TEST_CXXFLAGS)

$(TEST_OBJ_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

# Auto-generated header dependencies (see CFLAGS's own -MMD -MP comment above) -- one %.d file per
# %.o, listing every header that .o's source transitively includes. Leading "-" means a missing .d
# (first build, or after `make clean`) is silently skipped rather than erroring; once objects exist,
# these make every %.o target above also depend on the headers it actually includes, so e.g. editing
# app/scene/renderable.h now correctly invalidates every .o that reaches it, not just the ones whose
# own .cpp got touched directly.
-include $(OBJS:.o=.d)
-include $(TEST_OBJS:.o=.d)

# Clean everything
# NOTE: used to also carry a WINDOWS/LINUX/OSX find-and-delete-relative-to-cwd block here, predating
# PROJECT_BUILD_PATH defaulting away from "." -- kept only because the release CI workflows
# (build_linux/macos/webassembly.yml) used to call `make` with PROJECT_BUILD_PATH=. (cwd already
# src/ by the time they call it), so build output landed directly in cwd and only cwd-scoped find/rm
# could clean it up (see build_macos.yml's intermediate `make clean` between its x86_64/arm64
# passes). Those workflows now pass an explicit, non-"." PROJECT_BUILD_PATH (build/release/,
# consistent with local dev's build/desktop/ and ci_sanity.yml's own default) -- the path-aware
# lines below already clean that up correctly wherever PROJECT_BUILD_PATH points, no cwd-scoped
# fallback needed anymore. PLATFORM_DRM's block is unrelated (never had a PROJECT_BUILD_PATH-based
# alternative to begin with -- DRM builds were never routed through the release workflows' path
# convention), kept as-is.
clean:
ifeq ($(PLATFORM),PLATFORM_DRM)
	find . -type f -executable -delete
	find . -name "*.o" -delete
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
	find . -name "*.o" -delete
	rm -rf $(PROJECT_BUILD_PATH)
endif
	@rm -rf $(OBJ_DIR) $(TEST_OBJ_DIR)
	@rm -f $(PROJECT_BUILD_PATH)/$(PROJECT_NAME)$(EXT) $(PROJECT_BUILD_PATH)/$(GAME)$(EXT) $(GAME_MARKER)
	@rm -f $(TEST_BUILD_PATH)/tests_runner
	@echo Cleaning done

