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

.PHONY: all clean tests test-run

# Define required environment variables
#------------------------------------------------------------------------------------------------
# Define target platform: PLATFORM_DESKTOP, PLATFORM_WEB, PLATFORM_DRM, PLATFORM_ANDROID
PLATFORM              ?= PLATFORM_DESKTOP

# Define project variables
PROJECT_NAME          ?= raylib_game
PROJECT_VERSION       ?= 1.0
PROJECT_BUILD_PATH    ?= .

# Web build artifacts (.html/.js/.wasm/.data) land in build/web/ instead of src/, so
# `make clean` and .gitignore only have to deal with one build output location
ifeq ($(PLATFORM),PLATFORM_WEB)
    PROJECT_BUILD_PATH := ../build/web
endif

# No plain-C sources remain (docs/adr/0014): the raylib template's screen_*.c files converted to
# .cpp as part of moving into src/game/sandbox/ -- see PROJECT_SOURCE_FILES_CPP below. Left empty,
# not deleted, as the seam a future genuinely-C game module could still use.
PROJECT_SOURCE_FILES  ?=

# C++ sources: the game-agnostic app/ engine plus the one concrete game module currently built,
# game/sandbox/ (docs/adr/0014 -- src/app/ must never name a specific game; src/game/<game-id>/
# owns its own composition root, screens, and concrete IGameView). entity_file_parser_yaml.cpp
# (docs/adr/0008) and engine_config.cpp (docs/adr/0011) are here because Engine::Init() now
# actually calls LoadOrCreateEngineConfig() -> YamlEntityFileParser -> mini-yaml. level_loader.cpp
# and base_game_logic.cpp (docs/adr/0010) join them for the same reason: game/sandbox/
# screen_gameplay.cpp's InitGameplayScreen() is the first real caller of BaseGameLogic/
# LevelLoader/EntityFactory, not just their own unit tests. game/sandbox/human_view.cpp and the
# rest of game/sandbox/ are the concrete game module itself, not engine code.
PROJECT_SOURCE_FILES_CPP ?= \
    app/engine.cpp \
    app/process_manager.cpp \
    app/debug_overlay.cpp \
    app/entity_file_parser_yaml.cpp \
    app/engine_config.cpp \
    app/level_loader.cpp \
    app/base_game_logic.cpp \
    game/sandbox/main.cpp \
    game/sandbox/screen_logo.cpp \
    game/sandbox/screen_title.cpp \
    game/sandbox/screen_options.cpp \
    game/sandbox/screen_gameplay.cpp \
    game/sandbox/screen_ending.cpp \
    game/sandbox/human_view.cpp

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
CFLAGS = -Wall -D_DEFAULT_SOURCE -Wno-missing-braces -Wno-unused-value -Wno-unused-result -Wno-pointer-sign $(PROJECT_CUSTOM_FLAGS)
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
# mini_yaml.o: mini-yaml (docs/adr/0008) lives outside src/ (vendored, like raylib/EnTT), so it
# can't go through PROJECT_SOURCE_FILES_CPP's %.o:%.cpp pattern the way in-tree sources do -- gets
# its own object + rule below, same as tests/mini_yaml.o does for the test build.
OBJS = $(patsubst %.c, %.o, $(PROJECT_SOURCE_FILES)) $(patsubst %.cpp, %.o, $(PROJECT_SOURCE_FILES_CPP)) mini_yaml.o

# Define processes to execute
#------------------------------------------------------------------------------------------------
# For Android platform we call a custom Makefile.Android
ifeq ($(PLATFORM),PLATFORM_ANDROID)
    MAKEFILE_TARGET = -f Makefile.Android
    export PROJECT_NAME
    export PROJECT_SOURCE_FILES
else
    MAKEFILE_TARGET = $(PROJECT_NAME)
endif

# Default target entry
all:
	$(MAKE) $(MAKEFILE_TARGET)

# Project target defined by PROJECT_NAME
# NOTE: Linked with $(CXX), not $(CC) -- OBJS includes C++ object files (see
# PROJECT_SOURCE_FILES_CPP), and linking those requires the C++ standard library, which the C
# compiler driver doesn't pull in automatically.
$(PROJECT_NAME): $(OBJS) resources
	$(CXX) -o $(PROJECT_BUILD_PATH)/$(PROJECT_NAME)$(EXT) $(OBJS) $(CFLAGS) $(INCLUDE_PATHS) $(LDFLAGS) $(LDLIBS) -D$(PLATFORM)

# Stage game content next to the built executable
# NOTE: Content now lives at PROJECT_RESOURCES_PATH (../assets by default), outside src/
resources:
	@mkdir -p $(PROJECT_BUILD_PATH)/resources
	@cp -r $(PROJECT_RESOURCES_PATH)/. $(PROJECT_BUILD_PATH)/resources

# Compile source files
# NOTE: This pattern will compile every module defined on $(OBJS)
%.o: %.c
	$(CC) -c $< -o $@ $(CFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

# NOTE: PROJECT_SOURCE_FILES_CPP entries (EnTT-using code) compile through this pattern instead,
# with the C++ standard (CXXFLAGS) and the C++ compiler driver (CXX).
%.o: %.cpp
	$(CXX) -c $< -o $@ $(CXXFLAGS) $(INCLUDE_PATHS) -D$(PLATFORM)

# NOTE: deliberately no prerequisite on this rule's own line, same reason as tests/mini_yaml.o
# below -- MINI_YAML_PATH's default placeholder is a Windows path ("C:\mini-yaml\mini-yaml"), and
# a colon inside a prerequisite makes GNU Make misparse the line as a static pattern rule.
mini_yaml.o:
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
# game_config_test.cpp (docs/adr/0011) do real file I/O against a gitignored scratch directory
# (test_scratch/), not fakes -- their whole job is that read/write round trip.
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
    tests/base_game_logic_test.cpp

TEST_CXXFLAGS = $(CXXFLAGS) -I. -I$(DOCTEST_INCLUDE_PATH) -I$(RAYLIB_INCLUDE_PATH) -I$(ENTT_INCLUDE_PATH) -I$(MINI_YAML_INCLUDE_PATH)

# ProcessManager's implementation lives in app/process_manager.cpp (EventManager is header-only,
# needs no object of its own). Built into a tests/-local object, never app/process_manager.o, so
# it can't collide with the object the main product target above produces from the same source
# with different flags. entity_file_parser_yaml.cpp/engine_config.cpp/level_loader.cpp/
# base_game_logic.cpp (all now also in PROJECT_SOURCE_FILES_CPP, docs/adr/0008/0009/0010/0011) get
# the same tests/-local treatment, so the test build's flags (TEST_CXXFLAGS) never collide with the
# main product's (CXXFLAGS). game/sandbox/human_view.cpp and game/sandbox/screen_gameplay.cpp
# (docs/adr/0010, docs/adr/0014) deliberately do NOT get a tests/-local object -- both call real
# raylib runtime functions (IsKeyDown, DrawCubeWires, BeginMode3D, ...), and the test build never
# links -lraylib (see TEST_CXXFLAGS above: raylib's *headers* are on the include path for types
# like Camera3D, but nothing links its library), so either would fail at link time.
# base_game_logic.cpp has no such dependency (only entt::registry,
# EventManager, ProcessManager, LevelLoader -- none of them touch raylib), so it's tested the same
# way level_loader.cpp already is, with a FakeView standing in for HumanView. mini-yaml isn't
# header-only either, so its own yaml/Yaml.cpp needs compiling into the test binary too
# (tests/mini_yaml.o below).
TEST_OBJS = $(patsubst tests/%.cpp, tests/%.o, $(TEST_SOURCE_FILES_CPP)) tests/process_manager.o tests/entity_file_parser_yaml.o tests/engine_config.o tests/level_loader.o tests/base_game_logic.o tests/mini_yaml.o

tests: $(TEST_OBJS)
	$(CXX) -o tests_runner $(TEST_OBJS) $(TEST_CXXFLAGS)

test-run: tests
	./tests_runner

tests/process_manager.o: app/process_manager.cpp
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

tests/entity_file_parser_yaml.o: app/entity_file_parser_yaml.cpp
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

tests/engine_config.o: app/engine_config.cpp
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

tests/level_loader.o: app/level_loader.cpp
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

tests/base_game_logic.o: app/base_game_logic.cpp
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

# NOTE: deliberately no prerequisite on this rule's own line (unlike process_manager.o/
# entity_file_parser_yaml.o above) -- MINI_YAML_PATH's default placeholder value is a Windows path
# ("C:\mini-yaml\mini-yaml"), and a colon inside a prerequisite makes GNU Make misparse the whole
# line as a static pattern rule ("target pattern contains no '%'"), even on platforms that never
# hit this rule's recipe. Referencing $(MINI_YAML_PATH) only inside the recipe body sidesteps that
# parse-time failure; the tradeoff is Make can't tell if Yaml.cpp itself changed and needs a
# rebuild, which is fine for a pinned, never-locally-edited vendored file.
tests/mini_yaml.o:
	$(CXX) -c $(MINI_YAML_PATH)/yaml/Yaml.cpp -o $@ $(TEST_CXXFLAGS)

tests/%.o: tests/%.cpp
	$(CXX) -c $< -o $@ $(TEST_CXXFLAGS)

# Clean everything
clean:
ifeq ($(PLATFORM),PLATFORM_DESKTOP)
    ifeq ($(PLATFORM_OS),WINDOWS)
	del *.o *.exe /s
    endif
    ifeq ($(PLATFORM_OS),LINUX)
	find . -type f -executable -delete
	find . -name "*.o" -delete
    endif
    ifeq ($(PLATFORM_OS),OSX)
	find . -name "*.o" -delete
	rm -f $(PROJECT_NAME)
    endif
endif
ifeq ($(PLATFORM),PLATFORM_DRM)
	find . -type f -executable -delete
	find . -name "*.o" -delete
endif
ifeq ($(PLATFORM),PLATFORM_WEB)
	find . -name "*.o" -delete
	rm -rf $(PROJECT_BUILD_PATH)
endif
	@echo Cleaning done

