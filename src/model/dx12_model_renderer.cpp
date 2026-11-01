#include "dx12_model_renderer.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tz {
namespace {
template <class T> struct Com {
  T* p = nullptr;
  Com() = default;
  Com(const Com&) = delete;
  Com& operator=(const Com&) = delete;
  Com(Com&& o) noexcept : p(o.p) { o.p = nullptr; }
  ~Com() { reset(); }
  void reset() { if (p) { p->Release(); p = nullptr; } }
  T** put() { reset(); return &p; }
  void** putVoid() { return reinterpret_cast<void**>(put()); }
  T* operator->() const { return p; }
  T* get() const { return p; }
  explicit operator bool() const { return p != nullptr; }
};
std::string hrText(HRESULT hr) { char b[32]; std::snprintf(b, sizeof b, "0x%08lX", (unsigned long)hr); return b; }

const char* kModelShader = R"HLSL(
cbuffer Frame : register(b0) {
  row_major float4x4 viewProj;
  float4 eye; float4 fogColor; float4 fogParams; float4 lightDir; float4 sunColor; float4 hemiSky; float4 hemiGround; float4 screen;
  float4 skyZenith; float4 skyHorizon; float4 fillDir; float4 grade;  // same layout as the primitive Frame cbuffer
};
cbuffer Draw : register(b1) { float4 w0; float4 w1; float4 w2; float4 color; float4 params; };  // node transform rows + material colour
Texture2D tex0 : register(t0);
SamplerState samp0 : register(s0);
// Skinned meshes are skinned on the CPU into a per-frame vertex buffer (model::skinMesh), so one VS serves both.
// (GPU skinning via a dynamically indexed joint cbuffer ran at 0.2-3 fps under Wine's vkd3d: see MERGE_NOTES.md.)
// Instanced: draws of the same model + look + pose share one DrawIndexedInstanced; slot 1 carries per-instance
// world rows, tint, colour mix (rgb + amount) and glow.
struct VSIn { float3 pos : POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; float4 col : COLOR0;
              float4 i0 : INSTW0; float4 i1 : INSTW1; float4 i2 : INSTW2; float4 tint : INSTTINT; float4 mixc : INSTMIX; float4 ip : INSTPARAM; };
struct PSIn { float4 pos : SV_Position; float3 wpos : TEXCOORD0; float3 nrm : TEXCOORD1; float2 uv : TEXCOORD2; float4 col : COLOR0; float glow : TEXCOORD3; float2 fin : TEXCOORD4; float2 surf : TEXCOORD5; };
PSIn MVS(VSIn i) {
  float4 p = float4(i.pos, 1);
  PSIn o;
  float4 m = float4(dot(p, w0), dot(p, w1), dot(p, w2), 1);
  float3 w = float3(dot(m, i.i0), dot(m, i.i1), dot(m, i.i2));
  float3 nm = float3(dot(i.nrm, w0.xyz), dot(i.nrm, w1.xyz), dot(i.nrm, w2.xyz));
  o.nrm = float3(dot(nm, i.i0.xyz), dot(nm, i.i1.xyz), dot(nm, i.i2.xyz));
  float4 c = color; c.rgb = lerp(c.rgb, i.mixc.rgb, i.mixc.a); c *= i.tint;
  o.wpos = w; o.pos = mul(float4(w, 1), viewProj); o.uv = i.uv; o.col = i.col * c; o.glow = i.ip.y; o.fin = float2(i.ip.x, i.ip.z);
  o.surf = params.xy;  // material roughness, metallic (draw constants)
  return o;
}
// Kaiju finishes (params: id 1 gold, 2 chrome, 3 holographic; t = time). base = surface colour, lit = normal shading.
float3 finishShade(float id, float t, float3 base, float3 lit, float3 n, float3 wpos) {
  float3 v = normalize(eye.xyz - wpos);
  float3 r = reflect(-v, n);
  float ndl = saturate(dot(n, lightDir.xyz));
  float fres = pow(1 - saturate(dot(n, v)), 3);
  float spec = pow(saturate(dot(r, lightDir.xyz)), 28);
  if (id < 1.5) {        // GOLD: metallic tint, luminance-preserving, warm env + sharp highlight
    float3 gold = float3(1.0, 0.76, 0.26);
    float lum = dot(base, float3(0.3, 0.59, 0.11));
    float3 env = lerp(hemiGround.rgb, hemiSky.rgb, r.y * 0.5 + 0.5);
    return gold * (0.35 + 0.8 * lum) * (env * 0.7 + sunColor.rgb * ndl * 0.7) + gold * (spec * 1.3 + fres * 0.45);
  } else if (id < 2.5) { // CHROME: env-ish reflection (sky above the horizon band, dark ground below) + streaks
    float band = smoothstep(-0.08, 0.12, r.y);
    float3 env = lerp(float3(0.16, 0.13, 0.18), float3(0.88, 0.93, 1.0), band) * (0.85 + 0.15 * sin(r.x * 9 + r.z * 5));
    env += float3(1, 0.95, 0.9) * smoothstep(0.02, 0.0, abs(r.y - 0.04)) * 0.6;   // bright horizon line
    return env * 0.85 + base * 0.08 + spec * 1.6 + fres * 0.3;
  } else {               // HOLOGRAPHIC: hue shifts with view angle and time
    float h = frac(dot(n, v) * 1.6 + t * 0.22 + wpos.y * 0.035);
    float3 rgb = saturate(abs(frac(h + float3(0, 2.0 / 3, 1.0 / 3)) * 6 - 3) - 1);
    return lerp(lit, rgb * (0.55 + 0.55 * ndl) + 0.15, 0.7) + fres * 0.5 + spec * 0.8;
  }
}
float3 toneMap(float3 c) {
  c *= max(grade.x, 0.01);
  float a = 2.51, b = 0.03, cc = 2.43, d = 0.59, e = 0.14;
  c = saturate((c * (a * c + b)) / (c * (cc * c + d) + e));
  return saturate((c - 0.5) * grade.y + 0.5);
}
float3 fogApply(float3 c, float3 wpos) {
  float3 dir = wpos - eye.xyz;
  float dist = length(dir);
  float3 vd = dir / max(dist, 1e-3);
  float upv = saturate(vd.y * 1.15 + 0.02);
  float3 skyCol = lerp(skyHorizon.rgb, skyZenith.rgb, upv);
  float f = saturate((dist - fogParams.x) / max(1e-3, fogParams.y - fogParams.x));
  float haze = saturate((10.0 - wpos.y) / 28.0) * 0.28;
  f = saturate(f + haze * (1.0 - f));
  return lerp(c, skyCol, f);
}
float contactAo(float3 n, float3 wpos) {
  float h = smoothstep(0.15, 4.5, wpos.y);
  float grazing = 1.0 - saturate(n.y);
  float strength = grade.z > 0.05 ? grade.z : 0.72;
  return lerp(1.0, lerp(strength, 1.0, h), grazing);
}
float3 shadeLit(float3 base, float3 n, float3 wpos, float glow, float rough, float metal) {
  float3 v = normalize(eye.xyz - wpos);
  float3 L = normalize(lightDir.xyz);
  float ndotl = dot(n, L);
  float wrap = saturate((ndotl + 0.55) / 1.55);
  float3 amb = lerp(hemiGround.rgb, hemiSky.rgb, n.y * 0.5 + 0.5);
  float fill = saturate(dot(n, normalize(fillDir.xyz))) * fillDir.w;
  float mx = max(base.r, max(base.g, base.b));
  float mn = min(base.r, min(base.g, base.b));
  float sat = mx > 1e-4 ? (mx - mn) / mx : 0;
  float roughC = saturate(rough * lerp(0.62, 1.0, 1.0 - sat));
  float metalC = saturate(metal);
  float3 diff = base * (1.0 - metalC * 0.85) * (amb + sunColor.rgb * wrap + hemiSky.rgb * fill);
  float3 H = normalize(L + v);
  float specPow = lerp(10.0, 72.0, 1.0 - roughC);
  float spec = pow(saturate(dot(n, H)), specPow) * saturate(ndotl);
  float3 f0 = lerp(float3(0.04, 0.04, 0.04), base, metalC);
  diff += f0 * spec * sunColor.rgb * lerp(0.35, 1.35, 1.0 - roughC);
  diff *= contactAo(n, wpos);
  diff += base * glow;
  return diff;
}
float4 MPS(PSIn i) : SV_Target {
  float4 base = i.col * tex0.Sample(samp0, i.uv);
  float l2 = dot(i.nrm, i.nrm);
  float3 n = l2 > 1e-12 ? i.nrm * rsqrt(l2) : float3(0, 1, 0);
  float rough = saturate(i.surf.x);
  float3 c = shadeLit(base.rgb, n, i.wpos, i.glow, rough, i.surf.y);
  if (i.fin.x > 0.5) c = finishShade(i.fin.x, i.fin.y, base.rgb, c, n, i.wpos) + base.rgb * i.glow;
  return float4(toneMap(fogApply(c, i.wpos)), base.a);
}
)HLSL";

struct DrawCB { float w0[4], w1[4], w2[4], color[4], params[4]; };
struct InstData { float w0[4], w1[4], w2[4], tint[4], mix[4], params[4]; };  // 96 bytes, input slot 1
D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE t) { D3D12_HEAP_PROPERTIES h = {}; h.Type = t; h.CreationNodeMask = 1; h.VisibleNodeMask = 1; return h; }
D3D12_RESOURCE_DESC bufferDesc(UINT64 size) {
  D3D12_RESOURCE_DESC d = {};
  d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; d.Width = size; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1;
  d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  return d;
}
constexpr UINT kMaxSrv = 2048;
constexpr int kMaxFrames = 4;
}  // namespace

struct Dx12ModelRenderer::Impl {
  ID3D12Device* dev = nullptr;
  int frames = 2;
  Com<ID3D12RootSignature> root; Com<ID3D12PipelineState> pso[2];  // [blend]
  Com<ID3D12DescriptorHeap> srvHeap; UINT srvStride = 0, srvUsed = 0;
  struct GpuMesh { D3D12_VERTEX_BUFFER_VIEW vb = {}; D3D12_INDEX_BUFFER_VIEW ib = {}; };
  struct GpuModel {
    Com<ID3D12Resource> geo;
    std::vector<GpuMesh> meshes;
    std::vector<UINT> texSlot;                 // per Model::textures
    std::vector<Com<ID3D12Resource>> tex, staging;
  };
  std::map<uint32_t, std::unique_ptr<GpuModel>> models;
  Com<ID3D12Resource> white, whiteStaging; bool whiteReady = false;
  Com<ID3D12Resource> ring[kMaxFrames]; uint8_t* ringPtr[kMaxFrames] = {}; UINT64 ringCap[kMaxFrames] = {}, ringUsed = 0;
  std::vector<std::vector<UINT64>> skinOff;  // per draw, per node: ring offset of its CPU-skinned vertices (pass 0)
  struct Batch { size_t first = 0; UINT64 instOff = 0; UINT count = 0; bool blendInst = false; };  // same model/look/pose
  std::vector<Batch> batches; std::vector<size_t> batchOf;
  struct { int draws = 0, batches = 0, calls = 0; } drawStats;
  bool ok = false, warned = false;
  struct { int skinned = 0, reused = 0; } skinStats;  // last frame: mesh instances CPU-skinned / reused from a shared pose

  D3D12_CPU_DESCRIPTOR_HANDLE cpu(UINT slot) { D3D12_CPU_DESCRIPTOR_HANDLE h = srvHeap->GetCPUDescriptorHandleForHeapStart(); h.ptr += (SIZE_T)slot * srvStride; return h; }
  D3D12_GPU_DESCRIPTOR_HANDLE gpu(UINT slot) { D3D12_GPU_DESCRIPTOR_HANDLE h = srvHeap->GetGPUDescriptorHandleForHeapStart(); h.ptr += (UINT64)slot * srvStride; return h; }

  bool makeTexture(ID3D12GraphicsCommandList* c, int w, int h, const uint8_t* rgba, Com<ID3D12Resource>& tex, Com<ID3D12Resource>& staging, UINT& slot) {
    if (srvUsed >= kMaxSrv) return false;
    D3D12_RESOURCE_DESC td = {};
    td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = (UINT64)w; td.Height = (UINT)h; td.DepthOrArraySize = 1; td.MipLevels = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_HEAP_PROPERTIES hd = heapProps(D3D12_HEAP_TYPE_DEFAULT), hu = heapProps(D3D12_HEAP_TYPE_UPLOAD);
    if (FAILED(dev->CreateCommittedResource(&hd, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource), tex.putVoid()))) return false;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp; UINT rows = 0; UINT64 rowSize = 0, total = 0;
    dev->GetCopyableFootprints(&td, 0, 1, 0, &fp, &rows, &rowSize, &total);
    D3D12_RESOURCE_DESC bd = bufferDesc(total);
    if (FAILED(dev->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), staging.putVoid()))) return false;
    uint8_t* p = nullptr; D3D12_RANGE none = {0, 0};
    if (FAILED(staging->Map(0, &none, (void**)&p))) return false;
    for (UINT y = 0; y < rows; y++) std::memcpy(p + fp.Offset + (UINT64)y * fp.Footprint.RowPitch, rgba + (size_t)y * (size_t)w * 4, (size_t)w * 4);
    staging->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};
    dst.pResource = tex.get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
    src.pResource = staging.get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp;
    c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = tex.get();
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    c->ResourceBarrier(1, &b);
    D3D12_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sd.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; sd.Texture2D.MipLevels = 1;
    slot = srvUsed++;
    dev->CreateShaderResourceView(tex.get(), &sd, cpu(slot));
    return true;
  }
  GpuModel* upload(ID3D12GraphicsCommandList* c, const mdl::Model& m) {
    auto it = models.find(m.uid);
    if (it != models.end()) return it->second.get();
    std::unique_ptr<GpuModel> g(new GpuModel());
    std::vector<uint8_t> blob; std::vector<UINT64> vbOff, ibOff;
    for (const mdl::Mesh& me : m.meshes) {
      blob.resize((blob.size() + 255) & ~(size_t)255);
      vbOff.push_back(blob.size()); size_t vb = me.vertices.size() * sizeof(mdl::Vertex);
      blob.resize(blob.size() + vb); if (vb) std::memcpy(blob.data() + vbOff.back(), me.vertices.data(), vb);
      blob.resize((blob.size() + 255) & ~(size_t)255);
      ibOff.push_back(blob.size()); size_t ib = me.indices.size() * sizeof(uint32_t);
      blob.resize(blob.size() + ib); if (ib) std::memcpy(blob.data() + ibOff.back(), me.indices.data(), ib);
    }
    if (blob.empty()) blob.resize(256);
    D3D12_HEAP_PROPERTIES hu = heapProps(D3D12_HEAP_TYPE_UPLOAD); D3D12_RESOURCE_DESC rd = bufferDesc(blob.size());
    if (FAILED(dev->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), g->geo.putVoid()))) return nullptr;
    void* p = nullptr; D3D12_RANGE none = {0, 0};
    if (FAILED(g->geo->Map(0, &none, &p))) return nullptr;
    std::memcpy(p, blob.data(), blob.size()); g->geo->Unmap(0, nullptr);
    D3D12_GPU_VIRTUAL_ADDRESS base = g->geo->GetGPUVirtualAddress();
    for (size_t i = 0; i < m.meshes.size(); i++) {
      GpuMesh gm;
      gm.vb = {base + vbOff[i], (UINT)(m.meshes[i].vertices.size() * sizeof(mdl::Vertex)), (UINT)sizeof(mdl::Vertex)};
      gm.ib = {base + ibOff[i], (UINT)(m.meshes[i].indices.size() * sizeof(uint32_t)), DXGI_FORMAT_R32_UINT};
      g->meshes.push_back(gm);
    }
    for (const mdl::Texture& t : m.textures) {
      Com<ID3D12Resource> tx, st; UINT slot = 0;
      if (t.width > 0 && t.height > 0 && makeTexture(c, t.width, t.height, t.rgba.data(), tx, st, slot)) {
        g->texSlot.push_back(slot); g->tex.push_back(std::move(tx)); g->staging.push_back(std::move(st));
      } else g->texSlot.push_back(0);  // white
    }
    GpuModel* raw = g.get();
    models[m.uid] = std::move(g);
    return raw;
  }
};

Dx12ModelRenderer::Dx12ModelRenderer() : d(new Impl) {}
Dx12ModelRenderer::~Dx12ModelRenderer() { shutdown(); delete d; }
bool Dx12ModelRenderer::ready() const { return d->ok; }
size_t Dx12ModelRenderer::uploadedModels() const { return d->models.size(); }
void Dx12ModelRenderer::skinCounts(int& skinned, int& reused) const { skinned = d->skinStats.skinned; reused = d->skinStats.reused; }
void Dx12ModelRenderer::drawCounts(int& draws, int& batches, int& calls) const { draws = d->drawStats.draws; batches = d->drawStats.batches; calls = d->drawStats.calls; }

bool Dx12ModelRenderer::init(ID3D12Device* device, int framesInFlight, int colorFmt, int depthFmt, std::string& err) {
  Impl& r = *d; r.dev = device; r.frames = std::min(kMaxFrames, std::max(1, framesInFlight));
  // root signature: b0 frame CBV, b1 draw CBV, t0 texture table, s0 static sampler
  D3D12_DESCRIPTOR_RANGE range = {}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1; range.BaseShaderRegister = 0;
  range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
  D3D12_ROOT_PARAMETER rp[3] = {};
  rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; rp[0].Descriptor.ShaderRegister = 0; rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; rp[1].Descriptor.ShaderRegister = 1; rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[2].DescriptorTable.NumDescriptorRanges = 1; rp[2].DescriptorTable.pDescriptorRanges = &range;
  rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
  D3D12_STATIC_SAMPLER_DESC ss = {};
  ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  ss.MaxLOD = D3D12_FLOAT32_MAX; ss.ShaderRegister = 0; ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; ss.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
  D3D12_ROOT_SIGNATURE_DESC rsd = {}; rsd.NumParameters = 3; rsd.pParameters = rp; rsd.NumStaticSamplers = 1; rsd.pStaticSamplers = &ss;
  rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  Com<ID3DBlob> sig, sigErr;
  HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, sig.put(), sigErr.put());
  if (FAILED(hr)) { err = "model root signature: " + hrText(hr) + (sigErr ? std::string(" ") + (const char*)sigErr->GetBufferPointer() : ""); return false; }
  hr = device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), __uuidof(ID3D12RootSignature), r.root.putVoid());
  if (FAILED(hr)) { err = "model CreateRootSignature " + hrText(hr); return false; }
  Com<ID3DBlob> vs, ps, errs;
  UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
  hr = D3DCompile(kModelShader, std::strlen(kModelShader), "tommyzilla_model.hlsl", nullptr, nullptr, "MVS", "vs_5_0", flags, 0, vs.put(), errs.put());
  if (FAILED(hr)) { err = "model VS compile: " + (errs ? std::string((const char*)errs->GetBufferPointer(), errs->GetBufferSize()) : hrText(hr)); return false; }
  hr = D3DCompile(kModelShader, std::strlen(kModelShader), "tommyzilla_model.hlsl", nullptr, nullptr, "MPS", "ps_5_0", flags, 0, ps.put(), errs.put());
  if (FAILED(hr)) { err = "model PS compile: " + (errs ? std::string((const char*)errs->GetBufferPointer(), errs->GetBufferSize()) : hrText(hr)); return false; }
  const D3D12_INPUT_CLASSIFICATION V = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, I = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
  D3D12_INPUT_ELEMENT_DESC il[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, V, 0},     {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, V, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, V, 0},       {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 32, V, 0},  // joints/weights unused on the GPU
      {"INSTW", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, I, 1},     {"INSTW", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, I, 1},
      {"INSTW", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, I, 1},    {"INSTTINT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, I, 1},
      {"INSTMIX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, I, 1},  {"INSTPARAM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80, I, 1}};
  static_assert(sizeof(InstData) == 96, "instance layout");
  static_assert(sizeof(mdl::Vertex) == 64, "vertex layout");
  D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
  pd.pRootSignature = r.root.get();
  pd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()}; pd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
  pd.InputLayout = {il, (UINT)(sizeof il / sizeof il[0])};
  pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pd.RasterizerState.DepthClipEnable = TRUE;
  D3D12_RENDER_TARGET_BLEND_DESC& bt = pd.BlendState.RenderTarget[0];
  bt.BlendEnable = FALSE; bt.SrcBlend = D3D12_BLEND_SRC_ALPHA; bt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; bt.BlendOp = D3D12_BLEND_OP_ADD;
  bt.SrcBlendAlpha = D3D12_BLEND_ONE; bt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA; bt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  bt.LogicOp = D3D12_LOGIC_OP_NOOP; bt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
  pd.DepthStencilState.DepthEnable = TRUE; pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; pd.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
  pd.SampleMask = 0xffffffffu; pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pd.NumRenderTargets = 1; pd.RTVFormats[0] = (DXGI_FORMAT)colorFmt; pd.DSVFormat = (DXGI_FORMAT)depthFmt; pd.SampleDesc.Count = 1;
  hr = device->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState), r.pso[0].putVoid());
  if (FAILED(hr)) { err = "model PSO (opaque) " + hrText(hr); return false; }
  bt.BlendEnable = TRUE; pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
  hr = device->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState), r.pso[1].putVoid());
  if (FAILED(hr)) { err = "model PSO (blend) " + hrText(hr); return false; }
  D3D12_DESCRIPTOR_HEAP_DESC hd = {}; hd.NumDescriptors = kMaxSrv; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  hr = device->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), r.srvHeap.putVoid());
  if (FAILED(hr)) { err = "model SRV heap " + hrText(hr); return false; }
  r.srvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  r.srvUsed = 0;
  r.ok = true;
  return true;
}

void Dx12ModelRenderer::draw(ID3D12GraphicsCommandList* c, unsigned long long frameCB, int fi, const ModelDrawList& list, int pass) {
  Impl& r = *d;
  if (!r.ok || list.draws.empty() || fi < 0 || fi >= r.frames) return;
  if (!r.whiteReady) {  // slot 0: 1x1 white for untextured materials
    const uint8_t px[4] = {255, 255, 255, 255}; UINT slot = 0;
    if (!r.makeTexture(c, 1, 1, px, r.white, r.whiteStaging, slot) || slot != 0) { r.ok = false; return; }
    r.whiteReady = true;
  }
  const size_t nd = list.draws.size();
  if (pass == 0) {  // size + (re)create this frame's ring, CPU-skin every skinned mesh instance into it
    UINT64 need = 4096;
    for (const ModelDraw& dr : list.draws) {
      if (!dr.model) continue;
      for (auto& me : dr.model->meshes) need += me.parts.size() * 256 * 4;  // draw constants: opaque + blended, for up to two views (split screen)
      for (auto& n : dr.model->nodes) if (n.mesh >= 0 && dr.model->meshes[(size_t)n.mesh].skinned) need += ((dr.model->meshes[(size_t)n.mesh].vertices.size() * sizeof(mdl::Vertex) + 255) & ~(UINT64)255);
    }
    need += (UINT64)nd * sizeof(InstData) + (UINT64)nd * 256;
    need += need / 4;
    if (r.ringCap[fi] < need) {
      r.ring[fi].reset(); r.ringPtr[fi] = nullptr; r.ringCap[fi] = 0;
      UINT64 cap = std::max<UINT64>(need * 3 / 2, 1ull << 20);
      D3D12_HEAP_PROPERTIES hu = heapProps(D3D12_HEAP_TYPE_UPLOAD); D3D12_RESOURCE_DESC rd = bufferDesc(cap);
      if (FAILED(r.dev->CreateCommittedResource(&hu, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), r.ring[fi].putVoid()))) return;
      void* p = nullptr; D3D12_RANGE none = {0, 0};
      if (FAILED(r.ring[fi]->Map(0, &none, &p))) { r.ring[fi].reset(); return; }
      r.ringPtr[fi] = (uint8_t*)p; r.ringCap[fi] = cap;
    }
    r.ringUsed = 0; r.skinStats.skinned = r.skinStats.reused = 0;
    r.skinOff.assign(nd, {});
    std::unordered_map<const void*, size_t> firstUse;  // crowds share poses: skin each (pose, mesh node) once per frame
    for (size_t i = 0; i < nd; i++) {
      const ModelDraw& dr = list.draws[i];
      if (!dr.model || !dr.pose) continue;
      const mdl::Model& m = *dr.model;
      r.skinOff[i].assign(m.nodes.size(), ~(UINT64)0);
      auto fu = firstUse.find(dr.pose);
      const bool reuse = fu != firstUse.end() && list.draws[fu->second].model == dr.model;
      if (reuse) r.skinOff[i] = r.skinOff[fu->second];
      else firstUse[dr.pose] = i;
      for (size_t ni = 0; ni < m.nodes.size(); ni++) {
        if (r.skinOff[i][ni] != ~(UINT64)0) { r.skinStats.reused++; continue; }
        const mdl::Node& n = m.nodes[ni];
        if (n.mesh < 0 || n.mesh >= (int)m.meshes.size() || n.skin < 0 || n.skin >= (int)dr.pose->skinMats.size()) continue;
        if (dr.nodeVisible && ni < dr.nodeVisible->size() && !(*dr.nodeVisible)[ni]) continue;
        const mdl::Mesh& me = m.meshes[(size_t)n.mesh];
        if (!me.skinned || me.vertices.empty()) continue;
        const UINT64 bytes = (me.vertices.size() * sizeof(mdl::Vertex) + 255) & ~(UINT64)255;
        if (r.ringUsed + bytes > r.ringCap[fi]) break;
        mdl::skinMesh(me, dr.pose->skinMats[(size_t)n.skin], (mdl::Vertex*)(r.ringPtr[fi] + r.ringUsed));
        r.skinOff[i][ni] = r.ringUsed; r.ringUsed += bytes; r.skinStats.skinned++;
        if (reuse) r.skinOff[fu->second][ni] = r.skinOff[i][ni];  // node hidden in the first user's look
      }
    }
    // group into instanced batches: same model + pose + look (+ transparent tint), instance data into the ring
    struct Key { const void *m, *p, *c, *v; bool b; bool operator==(const Key& o) const { return m == o.m && p == o.p && c == o.c && v == o.v && b == o.b; } };
    struct KeyHash { size_t operator()(const Key& k) const { size_t h = std::hash<const void*>()(k.m); for (const void* x : {k.p, k.c, k.v}) h = h * 1000003u ^ std::hash<const void*>()(x); return h ^ (size_t)k.b; } };
    std::unordered_map<Key, size_t, KeyHash> byKey;
    r.batches.clear(); r.batchOf.assign(nd, ~(size_t)0);
    std::vector<std::vector<size_t>> members;
    for (size_t i = 0; i < nd; i++) {
      const ModelDraw& dr = list.draws[i];
      if (!dr.model || !dr.pose || dr.pose->global.size() != dr.model->nodes.size()) continue;
      Key k{dr.model, dr.pose, dr.materialColors, dr.nodeVisible, dr.tint.a < 0.999f};
      auto it = byKey.find(k);
      if (it == byKey.end()) { it = byKey.emplace(k, r.batches.size()).first; Impl::Batch b; b.first = i; b.blendInst = k.b; r.batches.push_back(b); members.emplace_back(); }
      members[it->second].push_back(i); r.batchOf[i] = it->second;
    }
    for (size_t bi = 0; bi < r.batches.size(); bi++) {
      Impl::Batch& b = r.batches[bi];
      const UINT64 bytes = (members[bi].size() * sizeof(InstData) + 255) & ~(UINT64)255;
      if (r.ringUsed + bytes > r.ringCap[fi]) { b.count = 0; continue; }
      b.instOff = r.ringUsed; b.count = (UINT)members[bi].size(); r.ringUsed += bytes;
      InstData* out = (InstData*)(r.ringPtr[fi] + b.instOff);
      for (size_t j = 0; j < members[bi].size(); j++) {
        const ModelDraw& dr = list.draws[members[bi][j]]; InstData& o = out[j];
        for (int row = 0; row < 4; row++) { o.w0[row] = dr.world.m[row][0]; o.w1[row] = dr.world.m[row][1]; o.w2[row] = dr.world.m[row][2]; }
        o.tint[0] = dr.tint.r; o.tint[1] = dr.tint.g; o.tint[2] = dr.tint.b; o.tint[3] = dr.tint.a;
        o.mix[0] = dr.mix.r; o.mix[1] = dr.mix.g; o.mix[2] = dr.mix.b; o.mix[3] = clampf(dr.mixK, 0, 1);
        o.params[0] = (float)dr.finish; o.params[1] = dr.glow; o.params[2] = dr.finishT; o.params[3] = 0;
      }
    }
    r.drawStats.draws = (int)nd; r.drawStats.batches = (int)r.batches.size(); r.drawStats.calls = 0;
  }
  if (!r.ring[fi] || r.skinOff.size() != nd) return;
  const D3D12_GPU_VIRTUAL_ADDRESS ringVA = r.ring[fi]->GetGPUVirtualAddress();
  c->SetGraphicsRootSignature(r.root.get());
  c->SetPipelineState(r.pso[(pass & 1) ? 1 : 0].get());
  ID3D12DescriptorHeap* heaps[] = {r.srvHeap.get()};
  c->SetDescriptorHeaps(1, heaps);
  c->SetGraphicsRootConstantBufferView(0, frameCB);
  c->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  if (r.batchOf.size() != nd) return;
  for (const Impl::Batch& bt : r.batches) {
    if (bt.count == 0) continue;
    const size_t i = bt.first;
    const ModelDraw& dr = list.draws[i];
    const mdl::Model& m = *dr.model;
    Impl::GpuModel* g = r.upload(c, m);
    if (!g) continue;
    D3D12_VERTEX_BUFFER_VIEW iv = {ringVA + bt.instOff, (UINT)(bt.count * sizeof(InstData)), (UINT)sizeof(InstData)};
    bool instBound = false;
    for (size_t ni = 0; ni < m.nodes.size(); ni++) {
      const mdl::Node& n = m.nodes[ni];
      if (n.mesh < 0 || n.mesh >= (int)m.meshes.size()) continue;
      if (dr.nodeVisible && ni < dr.nodeVisible->size() && !(*dr.nodeVisible)[ni]) continue;
      const mdl::Mesh& me = m.meshes[(size_t)n.mesh];
      if (me.indices.empty()) continue;
      const bool skinned = me.skinned && n.skin >= 0 && r.skinOff[i].size() == m.nodes.size() && r.skinOff[i][ni] != ~(UINT64)0;
      if (me.skinned && !skinned) continue;  // ring full / no palette
      const Mat4 node = skinned ? Mat4::identity() : dr.pose->global[ni];
      bool meshBound = false;
      for (const mdl::SubMesh& sm : me.parts) {
        if (sm.indexCount == 0) continue;
        const mdl::Material* mt = sm.material >= 0 && sm.material < (int)m.materials.size() ? &m.materials[(size_t)sm.material] : nullptr;
        Color col = mt ? mt->baseColor : Color{0.8f, 0.8f, 0.8f, 1};
        if (dr.materialColors && sm.material >= 0 && sm.material < (int)dr.materialColors->size()) col = (*dr.materialColors)[(size_t)sm.material];
        const bool blend = (mt && mt->blend) || col.a < 0.999f || bt.blendInst;
        if (blend != ((pass & 1) == 1)) continue;
        if (r.ringUsed + 256 > r.ringCap[fi]) { if (!r.warned) { OutputDebugStringA("Tommyzilla: model upload ring full\n"); r.warned = true; } return; }
        DrawCB cb = {};
        for (int row = 0; row < 4; row++) { cb.w0[row] = node.m[row][0]; cb.w1[row] = node.m[row][1]; cb.w2[row] = node.m[row][2]; }
        cb.color[0] = col.r; cb.color[1] = col.g; cb.color[2] = col.b; cb.color[3] = col.a;
        cb.params[0] = mt ? mt->roughness : 1.f; cb.params[1] = mt ? mt->metallic : 0.f;
        std::memcpy(r.ringPtr[fi] + r.ringUsed, &cb, sizeof cb);
        c->SetGraphicsRootConstantBufferView(1, ringVA + r.ringUsed); r.ringUsed += 256;
        UINT slot = mt && mt->texture >= 0 && mt->texture < (int)g->texSlot.size() ? g->texSlot[(size_t)mt->texture] : 0;
        c->SetGraphicsRootDescriptorTable(2, r.gpu(slot));
        if (!instBound) { c->IASetVertexBuffers(1, 1, &iv); instBound = true; }
        if (!meshBound) {
          D3D12_VERTEX_BUFFER_VIEW vb = g->meshes[(size_t)n.mesh].vb;
          if (skinned) vb.BufferLocation = ringVA + r.skinOff[i][ni];
          c->IASetVertexBuffers(0, 1, &vb); c->IASetIndexBuffer(&g->meshes[(size_t)n.mesh].ib); meshBound = true;
        }
        c->DrawIndexedInstanced(sm.indexCount, bt.count, sm.firstIndex, 0, 0);
        r.drawStats.calls++;
      }
    }
  }
}

void Dx12ModelRenderer::shutdown() {
  if (!d) return;
  d->models.clear();
  for (auto& x : d->ring) x.reset();
  d->white.reset(); d->whiteStaging.reset(); d->whiteReady = false;
  d->srvHeap.reset(); d->pso[0].reset(); d->pso[1].reset(); d->root.reset();
  d->ok = false;
}
}  // namespace tz
