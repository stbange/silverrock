cmake_minimum_required(VERSION 3.25)
project(silverrock VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SR_BUILD_APP      "Build the SDL3/OpenGL game executable" ON)
option(SR_BUILD_TESTS    "Build unit tests" ON)
option(SR_COVERAGE       "Instrument first-party code for coverage" OFF)
option(SR_SANITIZE       "Build with address and undefined-behaviour sanitizers" OFF)
option(SR_TEST_HOOKS     "Compile the test command/query interface" ON)
option(SR_WERROR         "Treat warnings as errors in first-party code" ON)

# ---------------------------------------------------------------------------
# Warnings, determinism and instrumentation for first-party targets only.
# ---------------------------------------------------------------------------
function(sr_configure_target tgt)
  set_target_properties(${tgt} PROPERTIES C_EXTENSIONS OFF)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /W4 /fp:precise /utf-8 /wd4100 /wd4244 /wd4267 /wd4305 /wd4996 /wd4127 /wd4701 /wd4703 /wd4706 /wd4204 /wd4221)
    if(SR_WERROR)
      target_compile_options(${tgt} PRIVATE /WX)
    endif()
    target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
  else()
    target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic -Wshadow
                           -Wstrict-prototypes -Wno-unused-parameter -ffp-contract=off
                           $<$<C_COMPILER_ID:GNU>:-Wno-format-truncation>)
    if(SR_WERROR)
      target_compile_options(${tgt} PRIVATE -Werror)
    endif()
  endif()
  if(SR_COVERAGE AND NOT MSVC)
    target_compile_options(${tgt} PRIVATE --coverage -O0 -g)
    target_link_options(${tgt} PRIVATE --coverage)
  endif()
  if(SR_SANITIZE AND NOT MSVC)
    target_compile_options(${tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
                           -fno-sanitize-recover=undefined)
    target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
  endif()
  if(SR_TEST_HOOKS)
    target_compile_definitions(${tgt} PRIVATE SR_TEST_HOOKS=1)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Engine: everything pure (no SDL, no OpenGL). See docs/concept/02-technical-design.md.
# ---------------------------------------------------------------------------
file(GLOB SR_ENGINE_SOURCES CONFIGURE_DEPENDS
  src/core/*.c src/world/*.c src/sim/*.c src/input/*.c
  src/render/*.c src/ui/*.c src/game/*.c)
add_library(sr_engine STATIC ${SR_ENGINE_SOURCES})
target_include_directories(sr_engine PUBLIC src)
sr_configure_target(sr_engine)
if(NOT MSVC)
  target_link_libraries(sr_engine PUBLIC m)
endif()

# Headless runner: scenario scripts and autoplayer without SDL or a GPU.
add_executable(sr_headless tools/headless_main.c)
target_link_libraries(sr_headless PRIVATE sr_engine)
sr_configure_target(sr_headless)

# ---------------------------------------------------------------------------
# Platform layer and game executable (SDL3 + OpenGL 3.3 / GLES 3.0).
# ---------------------------------------------------------------------------
if(SR_BUILD_APP)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL SYSTEM)

  file(GLOB SR_PLATFORM_SOURCES CONFIGURE_DEPENDS src/platform/*.c src/render/gl/*.c)
  add_library(sr_platform STATIC ${SR_PLATFORM_SOURCES})
  target_include_directories(sr_platform PUBLIC src)
  target_link_libraries(sr_platform PUBLIC sr_engine SDL3::SDL3-static)
  sr_configure_target(sr_platform)

  add_executable(silverrock src/app/main.c)
  target_link_libraries(silverrock PRIVATE sr_platform)
  sr_configure_target(silverrock)

  if(EMSCRIPTEN)
    set_target_properties(silverrock PROPERTIES SUFFIX ".html")
    target_link_options(silverrock PRIVATE
      -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1
      -sENVIRONMENT=web -lidbfs.js
      "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','IDBFS','addRunDependency','removeRunDependency']"
      --shell-file ${CMAKE_SOURCE_DIR}/web/shell.html)
    if(SR_TEST_HOOKS)
      target_link_options(silverrock PRIVATE
        "-sEXPORTED_FUNCTIONS=['_main','_sr_web_test_exec','_sr_web_set_safe_area','_sr_web_context_lost','_sr_web_context_restored']")
    else()
      target_link_options(silverrock PRIVATE
        "-sEXPORTED_FUNCTIONS=['_main','_sr_web_set_safe_area','_sr_web_context_lost','_sr_web_context_restored']")
    endif()
    configure_file(${CMAKE_SOURCE_DIR}/web/manifest.webmanifest ${CMAKE_BINARY_DIR}/manifest.webmanifest COPYONLY)
  endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(SR_BUILD_TESTS)
  enable_testing()
  file(GLOB SR_UNIT_TESTS CONFIGURE_DEPENDS tests/unit/*.c)
  add_executable(sr_tests ${SR_UNIT_TESTS} tests/sr_test.c)
  target_include_directories(sr_tests PRIVATE tests tests/unit)
  target_link_libraries(sr_tests PRIVATE sr_engine)
  sr_configure_target(sr_tests)
  if(EMSCRIPTEN)
    target_link_options(sr_tests PRIVATE -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1)
    add_test(NAME unit COMMAND node $<TARGET_FILE:sr_tests>)
  else()
    add_test(NAME unit COMMAND sr_tests)
  endif()

  file(GLOB SR_SCENARIOS CONFIGURE_DEPENDS tests/game/*.scn)
  foreach(scn ${SR_SCENARIOS})
    get_filename_component(scn_name ${scn} NAME_WE)
    if(EMSCRIPTEN)
      continue()
    endif()
    add_test(NAME game_${scn_name} COMMAND sr_headless --script ${scn})
  endforeach()

  if(SR_BUILD_APP AND NOT EMSCRIPTEN)
    file(GLOB SR_PLATFORM_TESTS CONFIGURE_DEPENDS tests/platform/*.c)
    add_executable(sr_platform_tests ${SR_PLATFORM_TESTS} tests/sr_test.c)
    target_include_directories(sr_platform_tests PRIVATE tests tests/platform)
    target_link_libraries(sr_platform_tests PRIVATE sr_platform)
    target_compile_definitions(sr_platform_tests PRIVATE
      SR_GOLDEN_DIR="${CMAKE_SOURCE_DIR}/tests/render")
    sr_configure_target(sr_platform_tests)
    add_test(NAME platform COMMAND sr_platform_tests)
    add_test(NAME game_app_headless_script COMMAND silverrock --headless --script
             ${CMAKE_SOURCE_DIR}/tests/game/new_game_start.scn)
  endif()
endif()
