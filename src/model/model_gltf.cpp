// glTF 2.0 / GLB loader (cgltf). Meshes (triangles), node hierarchy, materials (base colour factor + texture),
// vertex colours, skins and TRS animation channels. Morph targets, cameras, lights and extensions are ignored.
#include <cstring>
#include "../../third_party/cgltf/cgltf.h"
#include "model_loader.h"

namespace tz {
namespace mdl {
namespace {
Transform decompose(const float a[16]) {  // column-major glTF matrix -> TRS (no shear)
  Mat4 m; std::memcpy(m.m, a, sizeof(float) * 16);  // == row-vector form (transpose of column-major)
  Transform t; t.t = Vec3(m.m[3][0], m.m[3][1], m.m[3][2]);
  Vec3 r0(m.m[0][0], m.m[0][1], m.m[0][2]), r1(m.m[1][0], m.m[1][1], m.m[1][2]), r2(m.m[2][0], m.m[2][1], m.m[2][2]);
  t.s = Vec3(length(r0), length(r1), length(r2));
  if (dot(cross(r0, r1), r2) < 0) t.s.x = -t.s.x;
  r0 = r0 * (1 / (t.s.x == 0 ? 1 : t.s.x)); r1 = r1 * (1 / (t.s.y == 0 ? 1 : t.s.y)); r2 = r2 * (1 / (t.s.z == 0 ? 1 : t.s.z));
  // rows of the row-vector rotation = columns of the column-vector rotation R; R[i][j] = row_j[i]
  float R00 = r0.x, R10 = r0.y, R20 = r0.z, R01 = r1.x, R11 = r1.y, R21 = r1.z, R02 = r2.x, R12 = r2.y, R22 = r2.z;
  float tr = R00 + R11 + R22; Quat q;
  if (tr > 0) { float s = std::sqrt(tr + 1) * 2; q.w = 0.25f * s; q.x = (R21 - R12) / s; q.y = (R02 - R20) / s; q.z = (R10 - R01) / s; }
  else if (R00 > R11 && R00 > R22) { float s = std::sqrt(1 + R00 - R11 - R22) * 2; q.w = (R21 - R12) / s; q.x = 0.25f * s; q.y = (R01 + R10) / s; q.z = (R02 + R20) / s; }
  else if (R11 > R22) { float s = std::sqrt(1 + R11 - R00 - R22) * 2; q.w = (R02 - R20) / s; q.x = (R01 + R10) / s; q.y = 0.25f * s; q.z = (R12 + R21) / s; }
  else { float s = std::sqrt(1 + R22 - R00 - R11) * 2; q.w = (R10 - R01) / s; q.x = (R02 + R20) / s; q.y = (R12 + R21) / s; q.z = 0.25f * s; }
  t.r = normalizeQ(q);
  return t;
}
const char* resultText(cgltf_result r) {
  switch (r) {
    case cgltf_result_data_too_short: return "data too short"; case cgltf_result_unknown_format: return "unknown format";
    case cgltf_result_invalid_json: return "invalid JSON"; case cgltf_result_invalid_gltf: return "invalid glTF";
    case cgltf_result_file_not_found: return "file not found"; case cgltf_result_io_error: return "I/O error";
    case cgltf_result_out_of_memory: return "out of memory"; case cgltf_result_legacy_gltf: return "legacy glTF 1.0";
    default: return "error";
  }
}
void computeNormals(Mesh& me, size_t firstVertex) {
  for (size_t i = firstVertex; i < me.vertices.size(); i++) { auto& v = me.vertices[i]; v.nrm[0] = v.nrm[1] = v.nrm[2] = 0; }
  for (size_t i = 0; i + 2 < me.indices.size(); i += 3) {
    uint32_t a = me.indices[i], b = me.indices[i + 1], c = me.indices[i + 2];
    if (a < firstVertex || b < firstVertex || c < firstVertex) continue;
    auto P = [&](uint32_t k) { return Vec3(me.vertices[k].pos[0], me.vertices[k].pos[1], me.vertices[k].pos[2]); };
    Vec3 n = cross(P(b) - P(a), P(c) - P(a));
    for (uint32_t k : {a, b, c}) { me.vertices[k].nrm[0] += n.x; me.vertices[k].nrm[1] += n.y; me.vertices[k].nrm[2] += n.z; }
  }
  for (size_t i = firstVertex; i < me.vertices.size(); i++) {
    auto& v = me.vertices[i]; Vec3 n = normalize(Vec3(v.nrm[0], v.nrm[1], v.nrm[2]));
    if (length(n) == 0) n = Vec3(0, 1, 0);
    v.nrm[0] = n.x; v.nrm[1] = n.y; v.nrm[2] = n.z;
  }
}
}  // namespace

bool loadGltf(const std::string& path, Model& M, std::string& err, const LoadOptions& opt) {
  cgltf_options o; std::memset(&o, 0, sizeof o);
  cgltf_data* d = nullptr;
  cgltf_result r = cgltf_parse_file(&o, path.c_str(), &d);
  if (r != cgltf_result_success) { err = std::string("glTF parse failed (") + resultText(r) + "): " + path; return false; }
  struct Free { cgltf_data* d; ~Free() { cgltf_free(d); } } guard{d};
  r = cgltf_load_buffers(&o, d, path.c_str());
  if (r != cgltf_result_success) { err = std::string("glTF buffers failed (") + resultText(r) + "): " + path; return false; }
  M.format = d->file_type == cgltf_file_type_glb ? "glb" : "gltf";
  const std::string dir = dirOf(path);

  // ---- images -> textures (index-aligned with d->images; failed decodes leave an empty slot that materials skip)
  std::vector<int> imageTex(d->images_count, -1);
  if (opt.loadTextures) {
    for (cgltf_size i = 0; i < d->images_count; i++) {
      const cgltf_image& im = d->images[i];
      Texture t; std::string e; bool ok = false;
      if (im.buffer_view) {
        const uint8_t* p = cgltf_buffer_view_data(im.buffer_view);
        ok = p && decodeImage(p, im.buffer_view->size, t, e);
      } else if (im.uri && std::strncmp(im.uri, "data:", 5) == 0) {
        const char* comma = std::strstr(im.uri, ";base64,");
        if (comma) {
          const char* b64 = comma + 8; size_t len = std::strlen(b64), pad = 0;
          while (len > 0 && b64[len - 1 - pad] == '=' && pad < 2) pad++;
          size_t size = len / 4 * 3 - pad; void* out = nullptr;
          if (cgltf_load_buffer_base64(&o, size, b64, &out) == cgltf_result_success) { ok = decodeImage((const unsigned char*)out, size, t, e); std::free(out); }
        }
      } else if (im.uri) {
        std::string u(im.uri); std::vector<char> tmp(u.begin(), u.end()); tmp.push_back(0);
        cgltf_decode_uri(tmp.data());
        ok = decodeImageFile(dir + tmp.data(), t, e);
      }
      if (ok) { if (im.name) t.name = im.name; imageTex[i] = (int)M.textures.size(); M.textures.push_back(std::move(t)); }
      else M.warnings.push_back("image " + std::to_string(i) + ": " + (e.empty() ? "unsupported source" : e));
    }
  }
  // ---- materials (+ one default at the end for primitives without a material)
  for (cgltf_size i = 0; i < d->materials_count; i++) {
    const cgltf_material& gm = d->materials[i];
    Material m; m.name = gm.name ? gm.name : ("material" + std::to_string(i));
    if (gm.has_pbr_metallic_roughness) {
      const float* f = gm.pbr_metallic_roughness.base_color_factor;
      m.baseColor = {linearToSrgb(f[0]), linearToSrgb(f[1]), linearToSrgb(f[2]), f[3]};
      const cgltf_texture* tx = gm.pbr_metallic_roughness.base_color_texture.texture;
      if (tx && tx->image) m.texture = imageTex[(size_t)(tx->image - d->images)];
      m.roughness = gm.pbr_metallic_roughness.roughness_factor;
      m.metallic = gm.pbr_metallic_roughness.metallic_factor;
    }
    m.doubleSided = gm.double_sided != 0;
    m.blend = gm.alpha_mode == cgltf_alpha_mode_blend;
    M.materials.push_back(m);
  }
  const int defaultMat = (int)M.materials.size();
  bool usedDefault = false;
  M.materials.push_back(Material{"default", Color{0.8f, 0.8f, 0.8f, 1}, -1, true, false});

  // ---- meshes: all triangle primitives of a glTF mesh go into one Mesh with one SubMesh each
  for (cgltf_size mi = 0; mi < d->meshes_count; mi++) {
    const cgltf_mesh& gm = d->meshes[mi];
    Mesh me; me.name = gm.name ? gm.name : ("mesh" + std::to_string(mi));
    for (cgltf_size pi = 0; pi < gm.primitives_count; pi++) {
      const cgltf_primitive& pr = gm.primitives[pi];
      if (pr.type != cgltf_primitive_type_triangles) { M.warnings.push_back(me.name + ": skipped non-triangle primitive"); continue; }
      const cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv = nullptr, *col = nullptr, *jnt = nullptr, *wgt = nullptr;
      for (cgltf_size a = 0; a < pr.attributes_count; a++) {
        const cgltf_attribute& at = pr.attributes[a];
        if (at.type == cgltf_attribute_type_position) pos = at.data;
        else if (at.type == cgltf_attribute_type_normal) nrm = at.data;
        else if (at.type == cgltf_attribute_type_texcoord && at.index == 0) uv = at.data;
        else if (at.type == cgltf_attribute_type_color && at.index == 0) col = at.data;
        else if (at.type == cgltf_attribute_type_joints && at.index == 0) jnt = at.data;
        else if (at.type == cgltf_attribute_type_weights && at.index == 0) wgt = at.data;
      }
      if (!pos) continue;
      const size_t base = me.vertices.size(), n = pos->count;
      me.vertices.resize(base + n);
      for (size_t k = 0; k < n; k++) {
        Vertex& v = me.vertices[base + k]; std::memset(&v, 0, sizeof v);
        float f[16] = {0};
        cgltf_accessor_read_float(pos, k, f, 16); v.pos[0] = f[0]; v.pos[1] = f[1]; v.pos[2] = f[2];
        if (nrm) { cgltf_accessor_read_float(nrm, k, f, 16); v.nrm[0] = f[0]; v.nrm[1] = f[1]; v.nrm[2] = f[2]; }
        if (uv) { cgltf_accessor_read_float(uv, k, f, 16); v.uv[0] = f[0]; v.uv[1] = f[1]; }
        v.color[0] = v.color[1] = v.color[2] = v.color[3] = 255;
        if (col) {
          f[3] = 1; cgltf_accessor_read_float(col, k, f, 16);
          for (int c = 0; c < 3; c++) v.color[c] = (uint8_t)(linearToSrgb(f[c]) * 255.f + 0.5f);
          v.color[3] = (uint8_t)(clampf(cgltf_num_components(col->type) >= 4 ? f[3] : 1.f, 0, 1) * 255.f + 0.5f);
        }
        if (jnt && wgt) {
          cgltf_uint j[8] = {0}; cgltf_accessor_read_uint(jnt, k, j, 8);
          cgltf_accessor_read_float(wgt, k, f, 16);
          float s = f[0] + f[1] + f[2] + f[3];
          for (int c = 0; c < 4; c++) { v.joints[c] = (uint16_t)j[c]; v.weights[c] = s > 1e-6f ? f[c] / s : (c == 0 ? 1.f : 0.f); }
        }
      }
      SubMesh sm; sm.firstIndex = (uint32_t)me.indices.size();
      if (pr.indices) for (cgltf_size k = 0; k < pr.indices->count; k++) me.indices.push_back((uint32_t)(base + cgltf_accessor_read_index(pr.indices, k)));
      else for (size_t k = 0; k < n; k++) me.indices.push_back((uint32_t)(base + k));
      sm.indexCount = (uint32_t)me.indices.size() - sm.firstIndex;
      sm.material = pr.material ? (int)(pr.material - d->materials) : defaultMat;
      usedDefault |= sm.material == defaultMat;
      if (!nrm) computeNormals(me, base);
      me.hasVertexColors |= col != nullptr;
      me.skinned |= (jnt && wgt);
      me.parts.push_back(sm);
    }
    M.meshes.push_back(std::move(me));
  }
  if (!usedDefault) M.materials.pop_back();

  // ---- nodes
  M.nodes.resize(d->nodes_count);
  for (cgltf_size i = 0; i < d->nodes_count; i++) {
    const cgltf_node& gn = d->nodes[i]; Node& n = M.nodes[i];
    n.name = gn.name ? gn.name : ("node" + std::to_string(i));
    n.parent = gn.parent ? (int)(gn.parent - d->nodes) : -1;
    if (gn.has_matrix) n.local = decompose(gn.matrix);
    else {
      if (gn.has_translation) n.local.t = Vec3(gn.translation[0], gn.translation[1], gn.translation[2]);
      if (gn.has_rotation) n.local.r = normalizeQ({gn.rotation[0], gn.rotation[1], gn.rotation[2], gn.rotation[3]});
      if (gn.has_scale) n.local.s = Vec3(gn.scale[0], gn.scale[1], gn.scale[2]);
    }
    n.mesh = gn.mesh ? (int)(gn.mesh - d->meshes) : -1;
    n.skin = gn.skin ? (int)(gn.skin - d->skins) : -1;
    if (n.mesh >= 0 && !M.meshes[(size_t)n.mesh].skinned) n.skin = -1;
  }
  // ---- skins
  for (cgltf_size i = 0; i < d->skins_count; i++) {
    const cgltf_skin& gs = d->skins[i]; Skin s; s.name = gs.name ? gs.name : ("skin" + std::to_string(i));
    for (cgltf_size j = 0; j < gs.joints_count; j++) {
      s.joints.push_back((int)(gs.joints[j] - d->nodes));
      Mat4 ib = Mat4::identity();
      if (gs.inverse_bind_matrices) { float f[16]; cgltf_accessor_read_float(gs.inverse_bind_matrices, j, f, 16); std::memcpy(ib.m, f, sizeof f); }
      s.inverseBind.push_back(ib);
    }
    M.skins.push_back(std::move(s));
  }
  // ---- animations
  if (opt.loadAnimations) {
    for (cgltf_size i = 0; i < d->animations_count; i++) {
      const cgltf_animation& ga = d->animations[i];
      AnimationClip c; c.name = ga.name ? ga.name : ("clip" + std::to_string(i));
      for (cgltf_size k = 0; k < ga.channels_count; k++) {
        const cgltf_animation_channel& ch = ga.channels[k];
        if (!ch.target_node || !ch.sampler || !ch.sampler->input || !ch.sampler->output) continue;
        Channel o; o.node = (int)(ch.target_node - d->nodes);
        int comps;
        if (ch.target_path == cgltf_animation_path_type_translation) { o.path = Path::Translation; comps = 3; }
        else if (ch.target_path == cgltf_animation_path_type_rotation) { o.path = Path::Rotation; comps = 4; }
        else if (ch.target_path == cgltf_animation_path_type_scale) { o.path = Path::Scale; comps = 3; }
        else continue;  // morph weights
        const cgltf_animation_sampler& s = *ch.sampler;
        o.step = s.interpolation == cgltf_interpolation_type_step;
        const bool cubic = s.interpolation == cgltf_interpolation_type_cubic_spline;
        size_t keys = s.input->count;
        o.times.resize(keys); o.values.resize(keys * (size_t)comps);
        for (size_t t = 0; t < keys; t++) {
          cgltf_accessor_read_float(s.input, t, &o.times[t], 1);
          float f[16] = {0};
          cgltf_accessor_read_float(s.output, cubic ? t * 3 + 1 : t, f, 16);  // cubic: in-tangent, value, out-tangent
          for (int c = 0; c < comps; c++) o.values[t * (size_t)comps + (size_t)c] = f[c];
        }
        c.channels.push_back(std::move(o));
      }
      M.clips.push_back(std::move(c));
    }
  }
  return true;
}

}  // namespace mdl
}  // namespace tz
