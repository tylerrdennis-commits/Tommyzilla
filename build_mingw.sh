#!/bin/sh
# Cross-compile Tommyzilla.exe from Linux with mingw-w64. Output: dist/Tommyzilla.exe (+ dist/data, dist/assets) and ./Tommyzilla.exe.
set -e
cd "$(dirname "$0")"
# Shader gate: the exe compiles its HLSL at startup with D3DCompile (fxc rules), which is stricter than Wine's vkd3d.
# If DXC (DirectXShaderCompiler) and/or FXC (Windows SDK fxc.exe, run under Wine) are set or dxc is on PATH, every
# entry point must compile before the exe is built. TZ_SKIP_SHADER_CHECK=1 skips it.
if [ -z "$TZ_SKIP_SHADER_CHECK" ]; then
  [ -z "$DXC" ] && command -v dxc >/dev/null 2>&1 && DXC=$(command -v dxc)
  if [ -n "$DXC$FXC" ]; then
    mkdir -p build-mingw && python3 tools/check_shaders.py ${DXC:+--dxc "$DXC"} ${FXC:+--fxc "$FXC"} > build-mingw/shader_check.log 2>&1 || { cat build-mingw/shader_check.log; echo "HLSL check failed (see above)"; exit 1; }
    tail -1 build-mingw/shader_check.log
  else
    echo "note: no DXC/FXC set, skipping the HLSL compile check (tests still scan for reserved words)"
  fi
fi
cmake -S . -B build-mingw -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw -j"$(nproc 2>/dev/null || echo 4)"
mkdir -p dist
cp build-mingw/Tommyzilla.exe dist/
rm -rf dist/data && cp -r data dist/data
# models mapped in data/models.json (+ their textures and the asset licences) -> dist/assets
rm -rf dist/assets && cmake -DSRC_DIR="$(pwd)" -DDEST="$(pwd)/dist" -P tools/copy_models.cmake
cp build-mingw/Tommyzilla.exe ./Tommyzilla.exe   # root copy: runs next to ./data when double-clicked
echo "Built dist/Tommyzilla.exe and ./Tommyzilla.exe"
