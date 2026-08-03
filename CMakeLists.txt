cmake_minimum_required(VERSION 3.24...3.30)
project(raylib-game-template)

include(FetchContent)

# Generate compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# EnTT (see below) requires C++20; the template's own C sources are unaffected by this.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
set(RAYLIB_VERSION 6.0)
set(ENTT_VERSION v4.0.0)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG ${RAYLIB_VERSION}
    GIT_SHALLOW TRUE
    FIND_PACKAGE_ARGS
)

# EnTT is header-only; its own tests/docs/testbed default OFF already. Turn on
# ENTT_INCLUDE_HEADERS so the headers show up as sources in IDEs (no effect on the build itself).
set(ENTT_INCLUDE_HEADERS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG ${ENTT_VERSION}
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(raylib entt)

# Our Project
add_executable(${PROJECT_NAME})
add_subdirectory(src)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>)

if ("${PLATFORM}" STREQUAL "Web")
    add_custom_command(
        TARGET ${PROJECT_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/../resources
    )
    #DEPENDS ${PROJECT_NAME}
else()
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    )
    #DEPENDS ${PROJECT_NAME}
endif()

#set(raylib_VERBOSE 1)
target_link_libraries(${PROJECT_NAME} raylib EnTT::EnTT)

# Web Configurations
if ("${PLATFORM}" STREQUAL "Web")
    # Tell Emscripten to build an example.html file.
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${PROJECT_NAME} PUBLIC -sUSE_GLFW=3 PUBLIC --preload-file resources)
endif()

# Checks if OSX and links appropriate frameworks (Only required on MacOS)
if (APPLE)
    target_link_libraries(${PROJECT_NAME} "-framework IOKit")
    target_link_libraries(${PROJECT_NAME} "-framework Cocoa")
    target_link_libraries(${PROJECT_NAME} "-framework OpenGL")
endif()
