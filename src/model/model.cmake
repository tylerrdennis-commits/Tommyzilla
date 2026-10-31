# Tommyzilla model module (glTF/GLB + FBX loader, animator, manifest, DX12 skinned renderer).
# Included from the top-level CMakeLists.txt after tz_core is defined:  include(src/model/model.cmake)
# Defines: tz_model (static, platform independent), TZ_MODEL_WIN_SOURCES (DX12 renderer + test scene for the exe).
set(TZ_TP ${CMAKE_SOURCE_DIR}/third_party)
add_library(tz_model STATIC
  ${CMAKE_SOURCE_DIR}/src/model/model.cpp
  ${CMAKE_SOURCE_DIR}/src/model/model_loader.cpp
  ${CMAKE_SOURCE_DIR}/src/model/model_gltf.cpp
  ${CMAKE_SOURCE_DIR}/src/model/model_fbx.cpp
  ${CMAKE_SOURCE_DIR}/src/model/animator.cpp
  ${CMAKE_SOURCE_DIR}/src/model/model_manifest.cpp
  ${CMAKE_SOURCE_DIR}/src/model/model_library.cpp
  ${CMAKE_SOURCE_DIR}/src/model/model_test_scene.cpp
  ${CMAKE_SOURCE_DIR}/src/model/game_models.cpp
  ${CMAKE_SOURCE_DIR}/src/model/third_party_impl.cpp
  ${CMAKE_SOURCE_DIR}/src/model/ufbx_impl.cpp)
target_link_libraries(tz_model PUBLIC tz_core)
target_include_directories(tz_model PUBLIC ${CMAKE_SOURCE_DIR}/src ${TZ_TP})
# vendored single-file libraries: no warnings
if(MSVC)
  set_source_files_properties(${CMAKE_SOURCE_DIR}/src/model/ufbx_impl.cpp ${CMAKE_SOURCE_DIR}/src/model/third_party_impl.cpp PROPERTIES COMPILE_OPTIONS "/W0")
else()
  set_source_files_properties(${CMAKE_SOURCE_DIR}/src/model/ufbx_impl.cpp ${CMAKE_SOURCE_DIR}/src/model/third_party_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()
set(TZ_MODEL_WIN_SOURCES ${CMAKE_SOURCE_DIR}/src/model/dx12_model_renderer.cpp)

# Copies the files listed in data/models.json (+ their "extra" files and asset licences) to <dest>/assets.
# Usage: cmake -DSRC_DIR=<repo> -DDEST=<dir> -P tools/copy_models.cmake
