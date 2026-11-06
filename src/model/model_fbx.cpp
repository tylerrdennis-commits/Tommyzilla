// FBX loader (ufbx). Converts to +Y up, right-handed, metres. Meshes are triangulated per material part;
// skinning uses the first skin deformer of each mesh; animation stacks are baked to linear TRS keys.
#include <cstring>
#include <map>
#include "../../third_party/ufbx/ufbx.h"
#include "model_loader.h"

namespace tz {
namespace mdl {
namespace {
Mat4 toRowMat(const ufbx_matrix& u) {  // ufbx column-vector 3x4 -> row-vector Mat4 (row i = column i)
  Mat4 m = Mat4::identity();
  for (int i = 0; i < 4; i++) { m.m[i][0] = (float)u.cols[i].x; m.m[i][1] = (float)u.cols[i].y; m.m[i][2] = (float)u.cols[i].z; }
  return m;
}
Transform toTransform(const ufbx_transform& t) {
  Transform o;
  o.t = Vec3((float)t.translation.x, (float)t.translation.y, (float)t.translation.z);
  o.r = normalizeQ({(float)t.rotation.x, (float)t.rotation.y, (float)t.rotation.z, (float)t.rotation.w});
  o.s = Vec3((float)t.scale.x, (float)t.scale.y, (float)t.scale.z);
  return o;
}
std::string str(const ufbx_string& s) { return std::string(s.data ? s.data : "", s.length); }
std::string baseName(std::string p) { for (char& c : p) if (c == '\\') c = '/'; size_t s = p.find_last_of('/'); return s == std::string::npos ? p : p.substr(s + 1); }
}  // namespace

bool loadFbx(const std::string& path, Model& M, std::string& err, const LoadOptions& opt) {
  ufbx_load_opts o; std::memset(&o, 0, sizeof o);
  o.target_axes = ufbx_axes_right_handed_y_up;
  o.target_unit_meters = 1.0f;
  o.space_conversion = UFBX_SPACE_CONVERSION_TRANSFORM_ROOT;
  o.generate_missing_normals = true;
  ufbx_error e;
  ufbx_scene* sc = ufbx_load_file(path.c_str(), &o, &e);
  if (!sc) { char b[512]; ufbx_format_error(b, sizeof b, &e); err = "FBX load failed: " + path + ": " + b; return false; }
  struct Free { ufbx_scene* s; ~Free() { ufbx_free_scene(s); } } guard{sc};
  M.format = "fbx";
  const std::string dir = dirOf(path);

  // ---- textures (by ufbx texture typed_id)
  std::map<uint32_t, int> texIndex;
  auto texture = [&](const ufbx_texture* t) -> int {
    if (!t || !opt.loadTextures) return -1;
    auto it = texIndex.find(t->typed_id); if (it != texIndex.end()) return it->second;
    Texture tx; std::string er; bool ok = false;
    if (t->content.size > 0) ok = decodeImage((const unsigned char*)t->content.data, t->content.size, tx, er);
    std::string rel = str(t->relative_filename); for (char& c : rel) if (c == '\\') c = '/';
    std::string cand[] = {dir + rel, str(t->filename), dir + baseName(rel), dir + "Textures/" + baseName(rel), dir + "../Textures/" + baseName(rel)};
    for (const std::string& c : cand) if (!ok && !c.empty() && c != dir && fileExists(c)) ok = decodeImageFile(c, tx, er);
    int idx = -1;
    if (ok) { if (tx.name.empty()) tx.name = str(t->name); idx = (int)M.textures.size(); M.textures.push_back(std::move(tx)); }
    else M.warnings.push_back("texture '" + rel + "' not loaded" + (er.empty() ? "" : ": " + er));
    texIndex[t->typed_id] = idx;
    return idx;
  };
  // ---- materials (by ufbx material typed_id), plus a default
  for (size_t i = 0; i < sc->materials.count; i++) {
    const ufbx_material* m = sc->materials.data[i];
    Material mt; mt.name = str(m->name);
    const ufbx_material_map& dc = m->fbx.diffuse_color;
    ufbx_vec4 c = {0.8, 0.8, 0.8, 1};
    if (m->pbr.base_color.has_value) c = m->pbr.base_color.value_vec4;
    else if (dc.has_value) { c.x = dc.value_vec3.x; c.y = dc.value_vec3.y; c.z = dc.value_vec3.z; }
    mt.baseColor = {linearToSrgb((float)c.x), linearToSrgb((float)c.y), linearToSrgb((float)c.z), 1};
    const ufbx_texture* t = m->pbr.base_color.texture ? m->pbr.base_color.texture : dc.texture;
    mt.texture = texture(t);
    if (m->pbr.roughness.has_value) mt.roughness = (float)m->pbr.roughness.value_real;
    if (m->pbr.metalness.has_value) mt.metallic = (float)m->pbr.metalness.value_real;
    if (mt.texture >= 0) mt.baseColor = {1, 1, 1, 1};  // FBX exporters write a grey/white diffuse beside the map
    M.materials.push_back(mt);
  }
  const int defaultMat = (int)M.materials.size();
  M.materials.push_back(Material{"default", Color{0.8f, 0.8f, 0.8f, 1}, -1, true, false});
  bool usedDefault = false;

  // ---- nodes (index == ufbx typed_id)
  M.nodes.resize(sc->nodes.count);
  for (size_t i = 0; i < sc->nodes.count; i++) {
    const ufbx_node* n = sc->nodes.data[i]; Node& d = M.nodes[i];
    d.name = n->is_root ? std::string("RootNode") : str(n->name);
    d.parent = n->parent ? (int)n->parent->typed_id : -1;
    d.local = toTransform(n->local_transform);
  }
  // ---- meshes: one Mesh per mesh-carrying node (static meshes bake geometry_to_node into the vertices)
  std::vector<uint32_t> tri;
  for (size_t i = 0; i < sc->nodes.count; i++) {
    const ufbx_node* n = sc->nodes.data[i];
    const ufbx_mesh* mesh = n->mesh;
    if (!mesh || mesh->num_faces == 0) continue;
    const ufbx_skin_deformer* skin = mesh->skin_deformers.count ? mesh->skin_deformers.data[0] : nullptr;
    Mesh me; me.name = str(mesh->name).empty() ? M.nodes[i].name : str(mesh->name);
    me.skinned = skin != nullptr;
    me.hasVertexColors = mesh->vertex_color.exists;
    const Mat4 g2n = toRowMat(n->geometry_to_node);
    tri.resize(mesh->max_face_triangles * 3);
    for (size_t pi = 0; pi < mesh->material_parts.count; pi++) {
      const ufbx_mesh_part& part = mesh->material_parts.data[pi];
      if (part.num_triangles == 0) continue;
      SubMesh sm; sm.firstIndex = (uint32_t)me.indices.size();
      sm.material = part.index < mesh->materials.count && mesh->materials.data[part.index] ? (int)mesh->materials.data[part.index]->typed_id : defaultMat;
      usedDefault |= sm.material == defaultMat;
      for (size_t fi = 0; fi < part.face_indices.count; fi++) {
        ufbx_face face = mesh->faces.data[part.face_indices.data[fi]];
        uint32_t nt = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);
        for (uint32_t k = 0; k < nt * 3; k++) {
          uint32_t ix = tri[k];
          Vertex v; std::memset(&v, 0, sizeof v);
          ufbx_vec3 p = ufbx_get_vertex_vec3(&mesh->vertex_position, ix);
          ufbx_vec3 nr = mesh->vertex_normal.exists ? ufbx_get_vertex_vec3(&mesh->vertex_normal, ix) : ufbx_vec3{0, 1, 0};
          Vec3 P((float)p.x, (float)p.y, (float)p.z), N((float)nr.x, (float)nr.y, (float)nr.z);
          if (!skin) {
            P = g2n.transformPoint(P);
            Vec3 o = g2n.transformPoint(Vec3()); N = normalize(g2n.transformPoint(N) - o);  // fine for rotation + uniform scale
          }
          v.pos[0] = P.x; v.pos[1] = P.y; v.pos[2] = P.z; v.nrm[0] = N.x; v.nrm[1] = N.y; v.nrm[2] = N.z;
          if (mesh->vertex_uv.exists) { ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, ix); v.uv[0] = (float)uv.x; v.uv[1] = 1.0f - (float)uv.y; }
          v.color[0] = v.color[1] = v.color[2] = v.color[3] = 255;
          if (mesh->vertex_color.exists) {
            ufbx_vec4 c = ufbx_get_vertex_vec4(&mesh->vertex_color, ix);
            v.color[0] = (uint8_t)(clampf((float)c.x, 0, 1) * 255 + 0.5f); v.color[1] = (uint8_t)(clampf((float)c.y, 0, 1) * 255 + 0.5f);
            v.color[2] = (uint8_t)(clampf((float)c.z, 0, 1) * 255 + 0.5f); v.color[3] = (uint8_t)(clampf((float)c.w, 0, 1) * 255 + 0.5f);
          }
          if (skin) {
            uint32_t vi = mesh->vertex_indices.data[ix];
            const ufbx_skin_vertex& sv = skin->vertices.data[vi];
            float wsum = 0; int used = 0;
            for (uint32_t w = 0; w < sv.num_weights && used < 4; w++) {  // ufbx sorts weights by decreasing weight
              const ufbx_skin_weight& sw = skin->weights.data[sv.weight_begin + w];
              v.joints[used] = (uint16_t)sw.cluster_index; v.weights[used] = (float)sw.weight; wsum += (float)sw.weight; used++;
            }
            if (wsum > 1e-6f) for (int w = 0; w < 4; w++) v.weights[w] /= wsum; else v.weights[0] = 1;
          }
          me.indices.push_back((uint32_t)me.vertices.size());
          me.vertices.push_back(v);
        }
      }
      sm.indexCount = (uint32_t)me.indices.size() - sm.firstIndex;
      me.parts.push_back(sm);
    }
    M.nodes[i].mesh = (int)M.meshes.size();
    if (skin) {
      Skin s; s.name = str(skin->name);
      for (size_t c = 0; c < skin->clusters.count; c++) {
        const ufbx_skin_cluster* cl = skin->clusters.data[c];
        s.joints.push_back(cl->bone_node ? (int)cl->bone_node->typed_id : -1);
        s.inverseBind.push_back(toRowMat(cl->geometry_to_bone));
      }
      M.nodes[i].skin = (int)M.skins.size();
      M.skins.push_back(std::move(s));
    }
    M.meshes.push_back(std::move(me));
  }
  if (!usedDefault) M.materials.pop_back();

  // ---- animation stacks -> baked clips
  if (opt.loadAnimations) {
    for (size_t i = 0; i < sc->anim_stacks.count; i++) {
      const ufbx_anim_stack* st = sc->anim_stacks.data[i];
      ufbx_bake_opts bo; std::memset(&bo, 0, sizeof bo);
      ufbx_error be;
      ufbx_baked_anim* b = ufbx_bake_anim(sc, st->anim, &bo, &be);
      if (!b) { M.warnings.push_back("could not bake animation " + str(st->name)); continue; }
      AnimationClip c; c.name = str(st->name);
      const double t0 = b->playback_time_begin;
      for (size_t k = 0; k < b->nodes.count; k++) {
        const ufbx_baked_node& bn = b->nodes.data[k];
        if (bn.typed_id >= M.nodes.size()) continue;
        if (bn.translation_keys.count > 1 || !bn.constant_translation) {
          Channel ch; ch.node = (int)bn.typed_id; ch.path = Path::Translation;
          for (size_t q = 0; q < bn.translation_keys.count; q++) { auto& kk = bn.translation_keys.data[q]; ch.times.push_back((float)(kk.time - t0)); ch.values.insert(ch.values.end(), {(float)kk.value.x, (float)kk.value.y, (float)kk.value.z}); }
          c.channels.push_back(std::move(ch));
        }
        if (bn.rotation_keys.count > 1 || !bn.constant_rotation) {
          Channel ch; ch.node = (int)bn.typed_id; ch.path = Path::Rotation;
          for (size_t q = 0; q < bn.rotation_keys.count; q++) { auto& kk = bn.rotation_keys.data[q]; ch.times.push_back((float)(kk.time - t0)); ch.values.insert(ch.values.end(), {(float)kk.value.x, (float)kk.value.y, (float)kk.value.z, (float)kk.value.w}); }
          c.channels.push_back(std::move(ch));
        }
        if (bn.scale_keys.count > 1 || !bn.constant_scale) {
          Channel ch; ch.node = (int)bn.typed_id; ch.path = Path::Scale;
          for (size_t q = 0; q < bn.scale_keys.count; q++) { auto& kk = bn.scale_keys.data[q]; ch.times.push_back((float)(kk.time - t0)); ch.values.insert(ch.values.end(), {(float)kk.value.x, (float)kk.value.y, (float)kk.value.z}); }
          c.channels.push_back(std::move(ch));
        }
      }
      ufbx_free_baked_anim(b);
      M.clips.push_back(std::move(c));
    }
  }
  return true;
}

}  // namespace mdl
}  // namespace tz
