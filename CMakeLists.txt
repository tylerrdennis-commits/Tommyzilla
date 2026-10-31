# Tommyzilla City Smash - native Windows (Direct3D 12) build.
#   Visual Studio 2022:  open this folder (or run build_windows.bat) - x64, MSVC.
#   mingw-w64 cross:     ./build_mingw.sh   (uses cmake/mingw-w64-x86_64.cmake)
#   Linux tests:         cmake -S . -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
cmake_minimum_required(VERSION 3.16)
project(Tommyzilla LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# ---- data/*.json -> generated/embedded_data.h (byte arrays; avoids MSVC's string literal limits)
set(TZ_DATA ${CMAKE_SOURCE_DIR}/data/config.json ${CMAKE_SOURCE_DIR}/data/moves.json ${CMAKE_SOURCE_DIR}/data/monsters.json ${CMAKE_SOURCE_DIR}/data/levels.json ${CMAKE_SOURCE_DIR}/data/eggs.json ${CMAKE_SOURCE_DIR}/data/credits.json)
set(TZ_GEN ${CMAKE_BINARY_DIR}/generated/embedded_data.h)
add_custom_command(OUTPUT ${TZ_GEN}
  COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${CMAKE_SOURCE_DIR} -DOUT=${TZ_GEN} -P ${CMAKE_SOURCE_DIR}/tools/embed.cmake
  DEPENDS ${TZ_DATA} ${CMAKE_SOURCE_DIR}/tools/embed.cmake
  COMMENT "Embedding data/*.json")

# ---- platform-independent game library (also used by the Linux test harness)
add_library(tz_core STATIC
  src/core/json.cpp src/core/rng.cpp
  src/game/config.cpp src/game/tables.cpp src/game/progress.cpp src/game/world.cpp src/game/effects.cpp
  src/game/people.cpp src/game/enemies.cpp src/game/kaiju.cpp src/game/rigs.cpp src/game/game.cpp src/game/embedded.cpp src/game/saveslots.cpp src/game/boss.cpp src/game/models.cpp
  src/game/titan.cpp src/game/titan_bosses.cpp src/game/titan_coop.cpp src/game/eggs.cpp src/game/kaiju_gen.cpp src/game/kaiju_gen_draw.cpp src/game/rex_play.cpp src/game/features.cpp src/game/arc.cpp src/game/ground.cpp src/game/city_gen.cpp src/game/atlas_stones.cpp
  src/ui/ui_draw.cpp src/ui/ui_style.cpp src/ui/hud_bar.cpp src/ui/vault_menus.cpp src/game/vault_powers.cpp src/ui/menus.cpp
  src/audio/audio_synth.cpp src/renderer/meshes.cpp
  ${TZ_GEN})
target_include_directories(tz_core PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/generated)
if(MSVC)
  target_compile_options(tz_core PUBLIC /W3 /utf-8 /MP /permissive- /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX)
else()
  target_compile_options(tz_core PUBLIC -Wall -Wno-misleading-indentation)
endif()

# ---- 3D model module (glTF/GLB + FBX loader, animator, manifest, skinned renderer): see src/model/ and MERGE_NOTES.md
include(${CMAKE_SOURCE_DIR}/src/model/model.cmake)

if(WIN32)
  add_executable(Tommyzilla WIN32
    src/platform/main_win.cpp src/platform/video_mf.cpp src/renderer/dx12_renderer.cpp src/audio/audio_xaudio2.cpp src/input/input_win.cpp
    ${TZ_MODEL_WIN_SOURCES})
  target_link_libraries(Tommyzilla PRIVATE tz_core tz_model d3d12 dxgi d3dcompiler mfplat mfreadwrite mfuuid)  # Media Foundation: after-credits video
  target_compile_definitions(Tommyzilla PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX $<$<CONFIG:Debug>:TZ_DEBUG_LAYER=1>)
  if(MSVC)
    target_link_libraries(Tommyzilla PRIVATE xinput xaudio2)          # Windows SDK: xinput.lib (1.4), xaudio2.lib (2.9)
  else()
    target_link_libraries(Tommyzilla PRIVATE xinput1_4 xaudio2_8 ole32 -static -static-libgcc -static-libstdc++)  # mingw-w64
    target_link_options(Tommyzilla PRIVATE -municode)  # wWinMain
  endif()
  # copy data/ next to the exe so it can be tweaked without rebuilding (the exe also has embedded copies)
  add_custom_command(TARGET Tommyzilla POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:Tommyzilla>/data
    COMMAND ${CMAKE_COMMAND} -DSRC_DIR=${CMAKE_SOURCE_DIR} -DDEST=$<TARGET_FILE_DIR:Tommyzilla> -P ${CMAKE_SOURCE_DIR}/tools/copy_models.cmake)
  set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Tommyzilla)
else()
  option(TZ_ASAN "Build tests with AddressSanitizer + UBSan" OFF)
  enable_testing()
  add_executable(tz_tests tests/tests.cpp)
  target_link_libraries(tz_tests PRIVATE tz_core)
  target_compile_definitions(tz_tests PRIVATE TZ_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
  if(TZ_ASAN)
    target_compile_options(tz_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer -g)
    target_link_options(tz_tests PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME tz_tests COMMAND tz_tests)
  add_executable(tz_vault_tests tests/vault_tests.cpp)  # Vault: menu style / HUD bar
  target_link_libraries(tz_vault_tests PRIVATE tz_core)
  target_compile_definitions(tz_vault_tests PRIVATE TZ_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
  add_test(NAME tz_vault_tests COMMAND tz_vault_tests)
  add_executable(tz_vault_ui_shot tests/vault_ui_shot.cpp)  # Vault: software UI screenshot (not a test)
  target_link_libraries(tz_vault_ui_shot PRIVATE tz_core)
  target_compile_definitions(tz_vault_ui_shot PRIVATE TZ_DATA_DIR="${CMAKE_SOURCE_DIR}/data")
  add_executable(tz_model_tests tests/model_tests.cpp)
  target_link_libraries(tz_model_tests PRIVATE tz_model)
  target_compile_definitions(tz_model_tests PRIVATE TZ_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  if(TZ_ASAN)
    target_link_options(tz_model_tests PRIVATE -fsanitize=address,undefined)
  endif()
  add_test(NAME tz_model_tests COMMAND tz_model_tests)
  # Optional: compile every embedded HLSL entry point with Microsoft's compilers (tools/check_shaders.py). Wine's vkd3d is
  # more lenient than the D3DCompile/fxc used on Windows. Point DXC (DirectXShaderCompiler Linux release) and/or FXC
  # (Windows SDK fxc.exe with its d3dcompiler_47.dll, run under Wine) at them, e.g. cmake -DDXC=/opt/dxc/bin/dxc -DFXC=...
  set(DXC "$ENV{DXC}" CACHE FILEPATH "dxc for the HLSL compile test (optional)")
  set(FXC "$ENV{FXC}" CACHE FILEPATH "fxc.exe for the HLSL compile test (optional, run under Wine)")
  if(NOT DXC)
    find_program(DXC_FOUND dxc)
    if(DXC_FOUND)
      set(DXC "${DXC_FOUND}")
    endif()
  endif()
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND AND (DXC OR FXC))
    set(_hlsl_args)
    if(DXC)
      list(APPEND _hlsl_args --dxc "${DXC}")
    endif()
    if(FXC)
      list(APPEND _hlsl_args --fxc "${FXC}")
    endif()
    add_test(NAME hlsl_compile COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/check_shaders.py ${_hlsl_args})
  endif()
  add_executable(tz_model_inspect tools/model_inspect.cpp)   # dev tool: dump what the loader sees in a model file
  target_link_libraries(tz_model_inspect PRIVATE tz_model)
endif()
