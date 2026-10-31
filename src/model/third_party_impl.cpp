// Single translation unit holding the header-only library implementations (cgltf, stb_image).
// ufbx ships as ufbx.c and is compiled as C++ via ufbx_impl.cpp.
#define CGLTF_IMPLEMENTATION
#include "../../third_party/cgltf/cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "../../third_party/stb/stb_image.h"
