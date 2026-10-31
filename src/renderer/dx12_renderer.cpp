#include "dx12_renderer.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>
#include "meshes.h"
#include "../model/dx12_model_renderer.h"

#ifndef TZ_DEBUG_LAYER
#ifdef _DEBUG
#define TZ_DEBUG_LAYER 1
#else
#define TZ_DEBUG_LAYER 0
#endif
#endif

namespace tz {
namespace {
template <class T> struct Com {  // tiny ComPtr (avoids a WRL dependency so mingw-w64 builds too)
  T* p = nullptr;
  Com() = default;
  Com(const Com&) = delete;
  Com& operator=(const Com&) = delete;
  ~Com() { reset(); }
  void reset() { if (p) { p->Release(); p = nullptr; } }
  T** put() { reset(); return &p; }
  void** putVoid() { return reinterpret_cast<void**>(put()); }
  T* operator->() const { return p; }
  T* get() const { return p; }
  explicit operator bool() const { return p != nullptr; }
};
std::string hrText(HRESULT hr) { char b[32]; std::snprintf(b, sizeof b, "0x%08lX", (unsigned long)hr); return b; }
#define TZ_HR(expr, what) do { HRESULT hr_ = (expr); if (FAILED(hr_)) { err = std::string(what) + " failed (HRESULT " + hrText(hr_) + ")"; return false; } } while (0)

const char* kShader = R"HLSL(
cbuffer Frame : register(b0) {
  row_major float4x4 viewProj;
  float4 eye;        // xyz
  float4 fogColor;   // rgb
  float4 fogParams;  // x = near, y = far
  float4 lightDir;   // xyz = direction towards the sun
  float4 sunColor;   // rgb * intensity
  float4 hemiSky;    // rgb * intensity
  float4 hemiGround; // rgb * intensity
  float4 screen;     // x = width, y = height
};
struct VSIn {
  float3 pos : POSITION; float3 nrm : NORMAL;
  float4 c0 : WORLD0; float4 c1 : WORLD1; float4 c2 : WORLD2; float4 col : COLOR0; float4 prm : PARAMS0;
};
struct PSIn { float4 pos : SV_Position; float3 wpos : TEXCOORD0; float3 nrm : TEXCOORD1; float4 col : COLOR0; float4 prm : TEXCOORD2; };
PSIn VSMain(VSIn i) {
  PSIn o;
  float4 p = float4(i.pos, 1);
  float3 w = float3(dot(p, i.c0), dot(p, i.c1), dot(p, i.c2));
  // rows of the 3x3 part (row-vector convention): normal matrix = inverse-transpose = cofactors / det
  float3 a0 = float3(i.c0.x, i.c1.x, i.c2.x), a1 = float3(i.c0.y, i.c1.y, i.c2.y), a2 = float3(i.c0.z, i.c1.z, i.c2.z);
  float3 n = i.nrm.x * cross(a1, a2) + i.nrm.y * cross(a2, a0) + i.nrm.z * cross(a0, a1);
  float det = dot(a0, cross(a1, a2));
  o.nrm = n * (det < 0 ? -1.0 : 1.0);
  o.wpos = w;
  o.pos = mul(float4(w, 1), viewProj);
  o.col = i.col; o.prm = i.prm;
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
float4 PSMain(PSIn i) : SV_Target {
  float3 base = i.col.rgb, c;
  if (i.prm.x > 0.5) c = base;
  else {
    float l2 = dot(i.nrm, i.nrm);
    float3 n = l2 > 1e-12 ? i.nrm * rsqrt(l2) : float3(0, 1, 0);
    float3 amb = lerp(hemiGround.rgb, hemiSky.rgb, n.y * 0.5 + 0.5);
    float ndl = saturate(dot(n, lightDir.xyz));
    c = base * (amb + sunColor.rgb * ndl);
    if (i.prm.z > 0.5) c = finishShade(i.prm.z, i.prm.w, base, c, n, i.wpos);  // primitive-rig fallback for kaiju finishes
  }
  c += base * i.prm.y;
  float f = saturate((distance(i.wpos, eye.xyz) - fogParams.x) / max(1e-3, fogParams.y - fogParams.x));
  return float4(lerp(saturate(c), fogColor.rgb, f), i.col.a);
}
// textured ground: world-space UVs (xz / tile metres), slice = surface, tinted by the old flat colour and divided by the
// texture's mean brightness (palette unchanged), same hemisphere + sun + fog as the primitives (normal = up).
Texture2DArray groundTex : register(t1); SamplerState groundSmp : register(s1);
cbuffer Ground : register(b2) { float4 gnd0; float4 gnd1; };  // gnd0.x = time (s); 1 / mean brightness: gnd0.yzw, gnd1.xy
struct GIn { float3 pos : POSITION; float4 col : COLOR0; float4 prm : TEXCOORD0; };  // prm: surface, lane, laneAt, tile (m)
struct GOut { float4 pos : SV_Position; float3 wpos : TEXCOORD0; float4 col : COLOR0; float4 prm : TEXCOORD1; };
GOut GroundVS(GIn i) { GOut o; o.wpos = i.pos; o.pos = mul(float4(i.pos, 1), viewProj); o.col = i.col; o.prm = i.prm; return o; }
float4 GroundPS(GOut i) : SV_Target {
  float s = floor(i.prm.x + 0.5);
  float2 uv = i.wpos.xz / i.prm.w;
  float t;
  if (s == 3) {  // water: two slowly drifting layers
    float tm = gnd0.x;
    t = 0.5 * (groundTex.Sample(groundSmp, float3(uv + float2(0.021, 0.013) * tm, s)).r + groundTex.Sample(groundSmp, float3(uv * 0.71 + float2(-0.015, 0.018) * tm + 0.37, s)).r);
  } else t = groundTex.Sample(groundSmp, float3(uv, s)).r;
  float inv = s < 0.5 ? gnd0.y : s < 1.5 ? gnd0.z : s < 2.5 ? gnd0.w : s < 3.5 ? gnd1.x : gnd1.y;
  float k = t * inv;
  float3 base = i.col.rgb * k;
  if (i.prm.y > 0.5) {  // dashed centre line (6 m period, 3 m dashes, 0.3 m wide), anti-aliased with fwidth
    bool alongX = i.prm.y < 1.5;
    float across = abs((alongX ? i.wpos.z : i.wpos.x) - i.prm.z), along = alongX ? i.wpos.x : i.wpos.z;
    float aw = max(fwidth(across), 1e-4), d = frac(along / 6.0), dw = max(fwidth(along) / 6.0, 1e-4);
    float laneMask = 1 - smoothstep(0.15 - aw, 0.15 + aw, across);
    float dash = smoothstep(0, dw * 2, d) * (1 - smoothstep(0.5, 0.5 + dw * 2, d));
    base = lerp(base, float3(0.62, 0.57, 0.40) * k, laneMask * dash * 0.9);
  }
  float3 c = base * (hemiSky.rgb + sunColor.rgb * saturate(lightDir.y));
  float f = saturate((distance(i.wpos, eye.xyz) - fogParams.x) / max(1e-3, fogParams.y - fogParams.x));
  return float4(lerp(saturate(c), fogColor.rgb, f), 1);
}
struct UIIn { float2 pos : POSITION; float4 col : COLOR0; };
struct UIOut { float4 pos : SV_Position; float4 col : COLOR0; };
UIOut UIVS(UIIn i) {
  UIOut o; o.pos = float4(i.pos.x / screen.x * 2 - 1, 1 - i.pos.y / screen.y * 2, 0, 1); o.col = i.col; return o;
}
float4 UIPS(UIOut i) : SV_Target { return i.col; }
// after-credits video: full-screen triangle, letterboxed by root constants (scale.xy), BGRX texture
Texture2D vidTex : register(t0); SamplerState vidSmp : register(s0);
cbuffer Vid : register(b1) { float4 vidScale; };
struct VidOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VidOut VidVS(uint id : SV_VertexID) {
  float2 p = float2((id << 1) & 2, id & 2);  // (0,0) (2,0) (0,2)
  VidOut o; o.pos = float4(p * 2 - 1, 0, 1); o.uv = float2(p.x, 1 - p.y);  // uv top-down (the video rows are top-down)
  return o;
}
float4 VidPS(VidOut i) : SV_Target {
  float2 uv = (i.uv - 0.5) / vidScale.xy + 0.5;  // letterbox: outside the picture is black
  if (any(uv < 0) || any(uv > 1)) return float4(0, 0, 0, 1);
  return float4(vidTex.SampleLevel(vidSmp, uv, 0).rgb, 1);
}
)HLSL";

struct FrameCB {
  float viewProj[16];
  float eye[4], fogColor[4], fogParams[4], lightDir[4], sunColor[4], hemiSky[4], hemiGround[4], screen[4];
};
constexpr int kFrames = 2;
constexpr DXGI_FORMAT kColorFmt = DXGI_FORMAT_R8G8B8A8_UNORM, kDepthFmt = DXGI_FORMAT_D32_FLOAT;
D3D12_RESOURCE_DESC bufferDesc(UINT64 size) {
  D3D12_RESOURCE_DESC d = {};
  d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; d.Width = size; d.Height = 1; d.DepthOrArraySize = 1; d.MipLevels = 1;
  d.SampleDesc.Count = 1; d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  return d;
}
D3D12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE t) { D3D12_HEAP_PROPERTIES h = {}; h.Type = t; h.CreationNodeMask = 1; h.VisibleNodeMask = 1; return h; }
}  // namespace

struct Dx12Renderer::Impl {
  HWND hwnd = nullptr; int width = 0, height = 0;
  std::string adapter; bool warp = false, lost = false;
  Com<IDXGIFactory4> factory; Com<ID3D12Device> device; Com<ID3D12CommandQueue> queue; Com<IDXGISwapChain3> swap;
  Com<ID3D12DescriptorHeap> rtvHeap, dsvHeap; UINT rtvStride = 0;
  Com<ID3D12Resource> backBuffers[kFrames]; Com<ID3D12Resource> depth;
  Com<ID3D12CommandAllocator> alloc[kFrames]; Com<ID3D12GraphicsCommandList> cmd;
  Com<ID3D12Fence> fence; UINT64 fenceValue = 0, frameFence[kFrames] = {}; HANDLE fenceEvent = nullptr;
  Com<ID3D12RootSignature> rootSig; Com<ID3D12PipelineState> psoOpaque, psoAlpha, psoUI;
  Com<ID3D12Resource> meshBuf; D3D12_VERTEX_BUFFER_VIEW meshVB[MESH_COUNT] = {}; D3D12_INDEX_BUFFER_VIEW meshIB[MESH_COUNT] = {}; UINT meshIdx[MESH_COUNT] = {};
  Com<ID3D12Resource> upload[kFrames]; uint8_t* uploadPtr[kFrames] = {}; UINT64 uploadCap[kFrames] = {};
  Dx12ModelRenderer models;  // loaded glTF/FBX models (src/model); disabled if its init fails
  // after-credits video texture (created on the first frame, re-created if the size changes)
  Com<ID3D12RootSignature> vidRoot; Com<ID3D12PipelineState> psoVid; Com<ID3D12DescriptorHeap> vidHeap; Com<ID3D12Resource> vidTex;
  int vidW = 0, vidH = 0; uint64_t vidSerial = ~0ull; bool vidReady = false;
  // textured ground (setGroundTextures): Texture2DArray + its SRV heap, own root signature / PSO
  Com<ID3D12RootSignature> groundRoot; Com<ID3D12PipelineState> psoGround; Com<ID3D12DescriptorHeap> groundHeap; Com<ID3D12Resource> groundTex;
  float groundInvMean[GS_COUNT] = {}; bool groundOk = false;
  struct GroundVertex { float x, y, z; float r, g, b, a; float surface, lane, laneAt, tile; };
  bool createGroundPipeline(std::string& err) {
    D3D12_DESCRIPTOR_RANGE range = {}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1; range.BaseShaderRegister = 1;
    D3D12_ROOT_PARAMETER rp[3] = {};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; rp[0].Descriptor.ShaderRegister = 0; rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; rp[1].Constants.ShaderRegister = 2; rp[1].Constants.Num32BitValues = 8; rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[2].DescriptorTable.NumDescriptorRanges = 1; rp[2].DescriptorTable.pDescriptorRanges = &range; rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC ss = {}; ss.Filter = D3D12_FILTER_ANISOTROPIC; ss.MaxAnisotropy = 8;  // wrap + trilinear/aniso over the mip chain
    ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; ss.MaxLOD = D3D12_FLOAT32_MAX; ss.ShaderRegister = 1; ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rsd = {}; rsd.NumParameters = 3; rsd.pParameters = rp; rsd.NumStaticSamplers = 1; rsd.pStaticSamplers = &ss;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Com<ID3DBlob> sig, sigErr;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, sig.put(), sigErr.put()))) { err = "ground root signature"; return false; }
    TZ_HR(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), __uuidof(ID3D12RootSignature), groundRoot.putVoid()), "CreateRootSignature(ground)");
    Com<ID3DBlob> vs, ps;
    if (!compile("GroundVS", "vs_5_0", vs, err) || !compile("GroundPS", "ps_5_0", ps, err)) return false;
    const D3D12_INPUT_CLASSIFICATION V = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    D3D12_INPUT_ELEMENT_DESC il[] = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, V, 0}, {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, V, 0},
                                     {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 28, V, 0}};
    static_assert(sizeof(GroundVertex) == 44, "ground vertex layout");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = groundRoot.get(); pd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()}; pd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pd.InputLayout = {il, 3};
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pd.RasterizerState.DepthClipEnable = TRUE;
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.DepthStencilState.DepthEnable = TRUE; pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; pd.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pd.SampleMask = 0xffffffffu; pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1; pd.RTVFormats[0] = kColorFmt; pd.DSVFormat = kDepthFmt; pd.SampleDesc.Count = 1;
    TZ_HR(device->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState), psoGround.putVoid()), "CreateGraphicsPipelineState(ground)");
    return true;
  }
  bool uploadGround(const GroundTextureSet& set, std::string& err) {
    if (!set.ok || set.size <= 0) { err = set.error.empty() ? "no ground textures" : set.error; return false; }
    if (!groundRoot && !createGroundPipeline(err)) return false;
    const UINT mips = (UINT)set.mips[0].size();
    for (int s = 0; s < GS_COUNT; s++) if (set.mips[s].size() != mips) { err = "ground mip chains differ"; return false; }
    D3D12_RESOURCE_DESC td = {}; td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = (UINT64)set.size; td.Height = (UINT)set.size;
    td.DepthOrArraySize = GS_COUNT; td.MipLevels = (UINT16)mips; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES hp = heapProps(D3D12_HEAP_TYPE_DEFAULT);
    Com<ID3D12Resource> tex;
    TZ_HR(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource), tex.putVoid()), "Create ground texture");
    const UINT subs = mips * GS_COUNT;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fp(subs); std::vector<UINT> rows(subs); std::vector<UINT64> rowBytes(subs); UINT64 total = 0;
    device->GetCopyableFootprints(&td, 0, subs, 0, fp.data(), rows.data(), rowBytes.data(), &total);
    Com<ID3D12Resource> staging; D3D12_HEAP_PROPERTIES up = heapProps(D3D12_HEAP_TYPE_UPLOAD); D3D12_RESOURCE_DESC bd = bufferDesc(total);
    TZ_HR(device->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), staging.putVoid()), "Create ground staging buffer");
    uint8_t* p = nullptr; D3D12_RANGE none = {0, 0};
    TZ_HR(staging->Map(0, &none, (void**)&p), "Map ground staging buffer");
    for (int s = 0; s < GS_COUNT; s++) for (UINT m = 0; m < mips; m++) {
      UINT i = (UINT)s * mips + m; int dim = std::max(1, set.size >> m);
      const std::vector<uint8_t>& src = set.mips[s][m];
      if (src.size() != (size_t)dim * dim * 4) { staging->Unmap(0, nullptr); err = "ground mip size"; return false; }
      for (int y = 0; y < dim; y++) std::memcpy(p + fp[i].Offset + (UINT64)y * fp[i].Footprint.RowPitch, src.data() + (size_t)y * dim * 4, (size_t)dim * 4);
    }
    staging->Unmap(0, nullptr);
    waitIdle();
    TZ_HR(alloc[0]->Reset(), "CommandAllocator::Reset(ground)");
    TZ_HR(cmd->Reset(alloc[0].get(), nullptr), "CommandList::Reset(ground)");
    for (UINT i = 0; i < subs; i++) {
      D3D12_TEXTURE_COPY_LOCATION dst = {}; dst.pResource = tex.get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = i;
      D3D12_TEXTURE_COPY_LOCATION src = {}; src.pResource = staging.get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; src.PlacedFootprint = fp[i];
      cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = tex.get(); b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST; b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmd->ResourceBarrier(1, &b);
    TZ_HR(cmd->Close(), "CommandList::Close(ground)");
    ID3D12CommandList* lists[] = {cmd.get()}; queue->ExecuteCommandLists(1, lists);
    waitIdle();  // the staging buffer goes away at the end of this scope
    if (!groundHeap) {
      D3D12_DESCRIPTOR_HEAP_DESC hd = {}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      TZ_HR(device->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), groundHeap.putVoid()), "CreateDescriptorHeap(ground)");
    }
    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {}; sv.Format = td.Format; sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY; sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2DArray.MipLevels = mips; sv.Texture2DArray.ArraySize = GS_COUNT;
    device->CreateShaderResourceView(tex.get(), &sv, groundHeap->GetCPUDescriptorHandleForHeapStart());
    groundTex.reset(); groundTex.p = tex.p; tex.p = nullptr;
    for (int s = 0; s < GS_COUNT; s++) groundInvMean[s] = 1.f / std::max(0.05f, set.meanLum[s]);
    groundOk = true;
    return true;
  }
  bool createVideoPipeline(std::string& err) {
    D3D12_DESCRIPTOR_RANGE range = {}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1; range.BaseShaderRegister = 0;
    D3D12_ROOT_PARAMETER rp[2] = {};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[0].DescriptorTable.NumDescriptorRanges = 1; rp[0].DescriptorTable.pDescriptorRanges = &range; rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; rp[1].Constants.ShaderRegister = 1; rp[1].Constants.Num32BitValues = 4; rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC ss = {}; ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    ss.MaxLOD = D3D12_FLOAT32_MAX; ss.ShaderRegister = 0; ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC rsd = {}; rsd.NumParameters = 2; rsd.pParameters = rp; rsd.NumStaticSamplers = 1; rsd.pStaticSamplers = &ss;
    Com<ID3DBlob> sig, sigErr;
    if (FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, sig.put(), sigErr.put()))) { err = "video root signature"; return false; }
    TZ_HR(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), __uuidof(ID3D12RootSignature), vidRoot.putVoid()), "CreateRootSignature(video)");
    Com<ID3DBlob> vs, ps;
    if (!compile("VidVS", "vs_5_0", vs, err) || !compile("VidPS", "ps_5_0", ps, err)) return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = vidRoot.get(); pd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()}; pd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pd.RasterizerState.DepthClipEnable = TRUE;
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.DepthStencilState.DepthEnable = FALSE; pd.SampleMask = 0xffffffffu; pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1; pd.RTVFormats[0] = kColorFmt; pd.DSVFormat = kDepthFmt; pd.SampleDesc.Count = 1;
    TZ_HR(device->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState), psoVid.putVoid()), "CreateGraphicsPipelineState(video)");
    D3D12_DESCRIPTOR_HEAP_DESC hd = {}; hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    TZ_HR(device->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), vidHeap.putVoid()), "CreateDescriptorHeap(video)");
    vidReady = true;
    return true;
  }
  bool ensureVideoTexture(int w, int h) {
    if (vidTex && vidW == w && vidH == h) return true;
    waitIdle(); vidTex.reset(); vidSerial = ~0ull;
    D3D12_RESOURCE_DESC td = {}; td.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; td.Width = (UINT64)w; td.Height = (UINT)h; td.DepthOrArraySize = 1; td.MipLevels = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    D3D12_HEAP_PROPERTIES hp = heapProps(D3D12_HEAP_TYPE_DEFAULT);
    if (FAILED(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, __uuidof(ID3D12Resource), vidTex.putVoid()))) return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {}; sv.Format = td.Format; sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; sv.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(vidTex.get(), &sv, vidHeap->GetCPUDescriptorHandleForHeapStart());
    vidW = w; vidH = h;
    return true;
  }

  bool fail(const std::string& what) { MessageBoxA(hwnd, what.c_str(), "Tommyzilla - graphics error", MB_ICONERROR | MB_OK); return false; }
  bool checkDevice(HRESULT hr, const char* where) {
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_HUNG) {
      HRESULT why = device ? device->GetDeviceRemovedReason() : hr;
      lost = true;
      fail(std::string("The graphics device was removed or reset during ") + where + " (reason " + hrText(why) +
           ").\n\nThis usually means a driver crash, a driver update or the GPU was disconnected. Your progress is saved - please restart the game.");
      return false;
    }
    if (FAILED(hr)) { lost = true; fail(std::string(where) + " failed (HRESULT " + hrText(hr) + ")"); return false; }
    return true;
  }
  void waitIdle() {
    if (!queue || !fence) return;
    UINT64 v = ++fenceValue;
    if (FAILED(queue->Signal(fence.get(), v))) return;
    if (fence->GetCompletedValue() < v) { fence->SetEventOnCompletion(v, fenceEvent); WaitForSingleObject(fenceEvent, 5000); }
  }
  bool createDevice(std::string& err) {
#if TZ_DEBUG_LAYER
    { Com<ID3D12Debug> dbg; if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), dbg.putVoid()))) dbg->EnableDebugLayer(); }
    UINT factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#else
    UINT factoryFlags = 0;
#endif
    TZ_HR(CreateDXGIFactory2(factoryFlags, __uuidof(IDXGIFactory4), factory.putVoid()), "CreateDXGIFactory2");
    // first hardware adapter that supports feature level 11_0, else WARP (software)
    for (UINT i = 0;; i++) {
      Com<IDXGIAdapter1> a; if (factory->EnumAdapters1(i, a.put()) == DXGI_ERROR_NOT_FOUND) break;
      DXGI_ADAPTER_DESC1 desc; a->GetDesc1(&desc);
      if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
      if (SUCCEEDED(D3D12CreateDevice(a.get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), device.putVoid()))) {
        char name[256]; WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof name, nullptr, nullptr); adapter = name; break;
      }
    }
    if (!device) {
      Com<IDXGIAdapter> a; TZ_HR(factory->EnumWarpAdapter(__uuidof(IDXGIAdapter), a.putVoid()), "EnumWarpAdapter");
      TZ_HR(D3D12CreateDevice(a.get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), device.putVoid()), "D3D12CreateDevice (WARP)");
      adapter = "WARP software renderer"; warp = true;
    }
#if TZ_DEBUG_LAYER
    { Com<ID3D12InfoQueue> iq; if (IsDebuggerPresent() && SUCCEEDED(device->QueryInterface(__uuidof(ID3D12InfoQueue), iq.putVoid()))) { iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE); iq->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE); } }
#endif
    D3D12_COMMAND_QUEUE_DESC qd = {}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    TZ_HR(device->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), queue.putVoid()), "CreateCommandQueue");
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = (UINT)width; sd.Height = (UINT)height; sd.Format = kColorFmt; sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.BufferCount = kFrames; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    Com<IDXGISwapChain1> sc1;
    TZ_HR(factory->CreateSwapChainForHwnd(queue.get(), hwnd, &sd, nullptr, nullptr, sc1.put()), "CreateSwapChainForHwnd");
    TZ_HR(sc1->QueryInterface(__uuidof(IDXGISwapChain3), swap.putVoid()), "IDXGISwapChain3");
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);  // the game handles Alt+Enter / F11 itself (borderless)
    D3D12_DESCRIPTOR_HEAP_DESC hd = {}; hd.NumDescriptors = kFrames; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    TZ_HR(device->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), rtvHeap.putVoid()), "CreateDescriptorHeap(RTV)");
    hd.NumDescriptors = 1; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    TZ_HR(device->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), dsvHeap.putVoid()), "CreateDescriptorHeap(DSV)");
    rtvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (int i = 0; i < kFrames; i++) TZ_HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), alloc[i].putVoid()), "CreateCommandAllocator");
    TZ_HR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[0].get(), nullptr, __uuidof(ID3D12GraphicsCommandList), cmd.putVoid()), "CreateCommandList");
    cmd->Close();
    TZ_HR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), fence.putVoid()), "CreateFence");
    fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent) { err = "CreateEvent failed"; return false; }
    return true;
  }
  bool createTargets(std::string& err) {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (int i = 0; i < kFrames; i++) {
      TZ_HR(swap->GetBuffer((UINT)i, __uuidof(ID3D12Resource), backBuffers[i].putVoid()), "GetBuffer");
      D3D12_CPU_DESCRIPTOR_HANDLE h = rtv; h.ptr += (SIZE_T)i * rtvStride;
      device->CreateRenderTargetView(backBuffers[i].get(), nullptr, h);
    }
    D3D12_RESOURCE_DESC dd = {};
    dd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; dd.Width = (UINT64)std::max(1, width); dd.Height = (UINT)std::max(1, height);
    dd.DepthOrArraySize = 1; dd.MipLevels = 1; dd.Format = kDepthFmt; dd.SampleDesc.Count = 1; dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE cv = {}; cv.Format = kDepthFmt; cv.DepthStencil.Depth = 1.0f;
    D3D12_HEAP_PROPERTIES hp = heapProps(D3D12_HEAP_TYPE_DEFAULT);
    TZ_HR(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, __uuidof(ID3D12Resource), depth.putVoid()), "Create depth buffer");
    device->CreateDepthStencilView(depth.get(), nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
  }
  bool compile(const char* entry, const char* target, Com<ID3DBlob>& out, std::string& err) {
    Com<ID3DBlob> errors;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if TZ_DEBUG_LAYER
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    HRESULT hr = D3DCompile(kShader, std::strlen(kShader), "tommyzilla.hlsl", nullptr, nullptr, entry, target, flags, 0, out.put(), errors.put());
    if (FAILED(hr)) {
      err = std::string("Shader compile failed (") + entry + "): " + (errors ? std::string((const char*)errors->GetBufferPointer(), errors->GetBufferSize()) : hrText(hr));
      return false;
    }
    return true;
  }
  bool createPipelines(std::string& err) {
    D3D12_ROOT_PARAMETER rp = {}; rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; rp.Descriptor.ShaderRegister = 0; rp.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rsd = {}; rsd.NumParameters = 1; rsd.pParameters = &rp; rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Com<ID3DBlob> sig, sigErr;
    HRESULT hr = D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, sig.put(), sigErr.put());
    if (FAILED(hr)) { err = "D3D12SerializeRootSignature failed " + hrText(hr); return false; }
    TZ_HR(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), __uuidof(ID3D12RootSignature), rootSig.putVoid()), "CreateRootSignature");
    Com<ID3DBlob> vs, ps, uvs, ups;
    if (!compile("VSMain", "vs_5_0", vs, err) || !compile("PSMain", "ps_5_0", ps, err) || !compile("UIVS", "vs_5_0", uvs, err) || !compile("UIPS", "ps_5_0", ups, err)) return false;
    const D3D12_INPUT_CLASSIFICATION V = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, I = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
    D3D12_INPUT_ELEMENT_DESC scene[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, V, 0},     {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, V, 0},
        {"WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, I, 1},     {"WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, I, 1},
        {"WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, I, 1},    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, I, 1},
        {"PARAMS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, I, 1}};
    D3D12_INPUT_ELEMENT_DESC ui[] = {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, V, 0}, {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, V, 0}};
    static_assert(sizeof(Instance) == 80, "instance layout");
    static_assert(sizeof(UIVertex) == 24, "ui vertex layout");
    static_assert(sizeof(MeshVertex) == 24, "mesh vertex layout");
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = rootSig.get();
    pd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()}; pd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pd.InputLayout = {scene, (UINT)(sizeof scene / sizeof scene[0])};
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;  // no culling: mirrored parts stay visible
    pd.RasterizerState.DepthClipEnable = TRUE; pd.RasterizerState.DepthBias = 0;
    D3D12_RENDER_TARGET_BLEND_DESC& bt = pd.BlendState.RenderTarget[0];
    bt.BlendEnable = FALSE; bt.SrcBlend = D3D12_BLEND_SRC_ALPHA; bt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; bt.BlendOp = D3D12_BLEND_OP_ADD;
    bt.SrcBlendAlpha = D3D12_BLEND_ONE; bt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA; bt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    bt.LogicOp = D3D12_LOGIC_OP_NOOP; bt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.DepthStencilState.DepthEnable = TRUE; pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; pd.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pd.SampleMask = 0xffffffffu; pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1; pd.RTVFormats[0] = kColorFmt; pd.DSVFormat = kDepthFmt; pd.SampleDesc.Count = 1;
    TZ_HR(device->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState), psoOpaque.putVoid()), "CreateGraphicsPipelineState(opaque)");
    bt.BlendEnable = TRUE; pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    TZ_HR(device->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState), psoAlpha.putVoid()), "CreateGraphicsPipelineState(alpha)");
    pd.VS = {uvs->GetBufferPointer(), uvs->GetBufferSize()}; pd.PS = {ups->GetBufferPointer(), ups->GetBufferSize()};
    pd.InputLayout = {ui, 2}; pd.DepthStencilState.DepthEnable = FALSE;
    TZ_HR(device->CreateGraphicsPipelineState(&pd, __uuidof(ID3D12PipelineState), psoUI.putVoid()), "CreateGraphicsPipelineState(ui)");
    return true;
  }
  bool createMeshes(std::string& err) {
    std::vector<uint8_t> blob; UINT64 vbOff[MESH_COUNT], ibOff[MESH_COUNT]; MeshData md[MESH_COUNT];
    for (int i = 0; i < MESH_COUNT; i++) {
      md[i] = buildMesh((MeshId)i);
      vbOff[i] = blob.size(); size_t vb = md[i].v.size() * sizeof(MeshVertex); blob.resize(blob.size() + vb); std::memcpy(blob.data() + vbOff[i], md[i].v.data(), vb);
      blob.resize((blob.size() + 15) & ~(size_t)15);
      ibOff[i] = blob.size(); size_t ib = md[i].idx.size() * sizeof(uint16_t); blob.resize(blob.size() + ib); std::memcpy(blob.data() + ibOff[i], md[i].idx.data(), ib);
      blob.resize((blob.size() + 15) & ~(size_t)15);
    }
    // small static data: an upload-heap buffer is fine (read once per draw, a few KB)
    D3D12_HEAP_PROPERTIES hp = heapProps(D3D12_HEAP_TYPE_UPLOAD); D3D12_RESOURCE_DESC rd = bufferDesc(blob.size());
    TZ_HR(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), meshBuf.putVoid()), "Create mesh buffer");
    void* ptr = nullptr; D3D12_RANGE none = {0, 0};
    TZ_HR(meshBuf->Map(0, &none, &ptr), "Map mesh buffer");
    std::memcpy(ptr, blob.data(), blob.size()); meshBuf->Unmap(0, nullptr);
    D3D12_GPU_VIRTUAL_ADDRESS base = meshBuf->GetGPUVirtualAddress();
    for (int i = 0; i < MESH_COUNT; i++) {
      meshVB[i] = {base + vbOff[i], (UINT)(md[i].v.size() * sizeof(MeshVertex)), (UINT)sizeof(MeshVertex)};
      meshIB[i] = {base + ibOff[i], (UINT)(md[i].idx.size() * sizeof(uint16_t)), DXGI_FORMAT_R16_UINT};
      meshIdx[i] = (UINT)md[i].idx.size();
    }
    return true;
  }
  bool ensureUpload(int f, UINT64 need) {
    if (uploadCap[f] >= need && upload[f]) return true;
    UINT64 cap = std::max<UINT64>(need + need / 2, 4ull << 20);
    upload[f].reset(); uploadPtr[f] = nullptr; uploadCap[f] = 0;
    D3D12_HEAP_PROPERTIES hp = heapProps(D3D12_HEAP_TYPE_UPLOAD); D3D12_RESOURCE_DESC rd = bufferDesc(cap);
    HRESULT hr = device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), upload[f].putVoid());
    if (!checkDevice(hr, "allocating the per-frame upload buffer")) return false;
    void* ptr = nullptr; D3D12_RANGE none = {0, 0};
    if (!checkDevice(upload[f]->Map(0, &none, &ptr), "mapping the upload buffer")) return false;  // stays mapped for its lifetime
    uploadPtr[f] = (uint8_t*)ptr; uploadCap[f] = cap;
    return true;
  }
};

Dx12Renderer::Dx12Renderer() : d(new Impl) {}
Dx12Renderer::~Dx12Renderer() { shutdown(); delete d; }
const std::string& Dx12Renderer::adapterName() const { return d->adapter; }
bool Dx12Renderer::usingWarp() const { return d->warp; }
bool Dx12Renderer::groundReady() const { return d->groundOk; }
bool Dx12Renderer::setGroundTextures(const GroundTextureSet& set, std::string& err) {
  if (!d->device || d->lost) { err = "no device"; return false; }
  d->groundOk = false;
  if (!d->uploadGround(set, err)) { d->groundOk = false; return false; }
  return true;
}
void Dx12Renderer::modelSkinCounts(int& skinned, int& reused) const { d->models.skinCounts(skinned, reused); }
void Dx12Renderer::modelDrawCounts(int& draws, int& batches, int& calls) const { d->models.drawCounts(draws, batches, calls); }

bool Dx12Renderer::init(HWND__* hwnd, int width, int height, std::string& err) {
  d->hwnd = (HWND)hwnd; d->width = std::max(1, width); d->height = std::max(1, height);
  if (!(d->createDevice(err) && d->createTargets(err) && d->createPipelines(err) && d->createMeshes(err))) return false;
  std::string merr;  // model renderer failure is not fatal: models fall back to the primitive rigs
  if (!d->models.init(d->device.get(), kFrames, (int)kColorFmt, (int)kDepthFmt, merr)) OutputDebugStringA(("Tommyzilla: model renderer disabled: " + merr + "\n").c_str());
  std::string verr;  // video pass failure is not fatal either: the in-engine closing shot plays instead
  if (!d->createVideoPipeline(verr)) OutputDebugStringA(("Tommyzilla: video pass disabled: " + verr + "\n").c_str());
  return true;
}

void Dx12Renderer::resize(int width, int height) {
  if (!d->swap || d->lost) return;
  width = std::max(1, width); height = std::max(1, height);
  if (width == d->width && height == d->height) return;
  d->waitIdle();
  for (auto& b : d->backBuffers) b.reset();
  d->depth.reset();
  d->width = width; d->height = height;
  HRESULT hr = d->swap->ResizeBuffers(kFrames, (UINT)width, (UINT)height, kColorFmt, 0);
  if (!d->checkDevice(hr, "ResizeBuffers")) return;
  std::string err;
  if (!d->createTargets(err)) { d->lost = true; d->fail(err); }
}

bool Dx12Renderer::render(const RenderList& rl, const ModelDrawList* models, const RenderList* right) {
  Impl& r = *d;
  if (r.lost) return false;
  const UINT fi = r.swap->GetCurrentBackBufferIndex();
  // wait until the GPU has finished with this frame's allocator + upload buffer
  if (r.fence->GetCompletedValue() < r.frameFence[fi]) { r.fence->SetEventOnCompletion(r.frameFence[fi], r.fenceEvent); WaitForSingleObject(r.fenceEvent, 5000); }
  // views: rl (fullscreen, or the left half in split screen) and optionally the right half. rl also carries the UI / video.
  const RenderList* views[2] = {&rl, right}; const int nv = right ? 2 : 1;
  // upload: constants (one 256-byte block per view), then instances per view + mesh (opaque, alpha), UI vertices, ground, video
  size_t inst = rl.instanceCount() + (right ? right->instanceCount() : 0);
  const bool vid = rl.video && rl.videoW > 0 && rl.videoH > 0 && r.vidReady && r.ensureVideoTexture(rl.videoW, rl.videoH);
  const UINT vidPitch = ((UINT)rl.videoW * 4 + 255) & ~255u;
  const bool vidUpload = vid && rl.videoSerial != r.vidSerial;
  bool groundPass[2] = {false, false}; UINT groundVerts[2] = {0, 0}; UINT64 groundBytes = 0;
  for (int v = 0; v < nv; v++) { groundPass[v] = r.groundOk && !views[v]->ground.empty(); groundVerts[v] = groundPass[v] ? (UINT)views[v]->ground.size() * 6 : 0; groundBytes += (UINT64)groundVerts[v] * sizeof(Impl::GroundVertex) + 16; }
  UINT64 need = 512 + inst * sizeof(Instance) + rl.ui.size() * sizeof(UIVertex) + 1024 + (vidUpload ? (UINT64)vidPitch * rl.videoH + 512 : 0) + groundBytes + 64;
  if (!r.ensureUpload((int)fi, need)) return false;
  uint8_t* base = r.uploadPtr[fi]; D3D12_GPU_VIRTUAL_ADDRESS gpu = r.upload[fi]->GetGPUVirtualAddress(); UINT64 off = 0;
  auto set4 = [](float* dst, float a, float b, float c, float e) { dst[0] = a; dst[1] = b; dst[2] = c; dst[3] = e; };
  for (int v = 0; v < nv; v++) {
    const RenderList& V = *views[v];
    FrameCB cb = {};
    Mat4 vp = V.view * V.proj;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) cb.viewProj[i * 4 + j] = vp.m[i][j];
    Vec3 L = normalize(V.lightDir * -1.f);
    set4(cb.eye, V.eye.x, V.eye.y, V.eye.z, 1);
    set4(cb.fogColor, V.fog.r, V.fog.g, V.fog.b, 1);
    set4(cb.fogParams, V.fogNear, V.fogFar, 0, 0);
    set4(cb.lightDir, L.x, L.y, L.z, 0);
    set4(cb.sunColor, V.sunColor.r * V.sunIntensity, V.sunColor.g * V.sunIntensity, V.sunColor.b * V.sunIntensity, 1);
    set4(cb.hemiSky, V.hemiSky.r * V.hemiIntensity, V.hemiSky.g * V.hemiIntensity, V.hemiSky.b * V.hemiIntensity, 1);
    set4(cb.hemiGround, V.hemiGround.r * V.hemiIntensity, V.hemiGround.g * V.hemiIntensity, V.hemiGround.b * V.hemiIntensity, 1);
    set4(cb.screen, (float)r.width, (float)r.height, 0, 0);  // UI pixel space is always the whole window
    std::memcpy(base + v * 256, &cb, sizeof cb);
  }
  off = 512;
  struct Batch { int mesh; UINT64 off; UINT count; };
  std::vector<Batch> opaque[2], alpha[2];
  for (int v = 0; v < nv; v++)
    for (int pass = 0; pass < 2; pass++)
      for (int m = 0; m < MESH_COUNT; m++) {
        const auto& vv = pass ? views[v]->alpha[m] : views[v]->opaque[m]; if (vv.empty()) continue;
        std::memcpy(base + off, vv.data(), vv.size() * sizeof(Instance));
        (pass ? alpha[v] : opaque[v]).push_back({m, off, (UINT)vv.size()}); off += vv.size() * sizeof(Instance);
      }
  UINT64 uiOff = off; std::memcpy(base + off, rl.ui.data(), rl.ui.size() * sizeof(UIVertex)); off += rl.ui.size() * sizeof(UIVertex);
  UINT64 groundOff[2] = {0, 0};
  for (int v = 0; v < nv; v++) {
    groundOff[v] = (off + 15) & ~(UINT64)15;
    if (!groundPass[v]) continue;
    Impl::GroundVertex* gv = (Impl::GroundVertex*)(base + groundOff[v]);
    for (const GroundQuad& q : views[v]->ground) {  // two triangles per ground rectangle (top face, y up)
      float tile = groundMaterial(q.surface).tile;
      auto vtx = [&](float x, float z) { return Impl::GroundVertex{x, q.y, z, q.color.r, q.color.g, q.color.b, 1, (float)q.surface, (float)q.lane, q.laneAt, tile}; };
      Impl::GroundVertex a = vtx(q.x0, q.z0), b = vtx(q.x1, q.z0), cc = vtx(q.x1, q.z1), dd = vtx(q.x0, q.z1);
      *gv++ = a; *gv++ = b; *gv++ = cc; *gv++ = a; *gv++ = cc; *gv++ = dd;
    }
    off = groundOff[v] + (UINT64)groundVerts[v] * sizeof(Impl::GroundVertex);
  }
  UINT64 vidOff = (off + 511) & ~(UINT64)511;
  if (vidUpload) { for (int y = 0; y < rl.videoH; y++) std::memcpy(base + vidOff + (UINT64)y * vidPitch, rl.video + (size_t)y * rl.videoStride, (size_t)rl.videoW * 4); off = vidOff + (UINT64)vidPitch * rl.videoH; }

  ID3D12GraphicsCommandList* c = r.cmd.get();
  if (!r.checkDevice(r.alloc[fi]->Reset(), "CommandAllocator::Reset")) return false;
  if (!r.checkDevice(c->Reset(r.alloc[fi].get(), r.psoOpaque.get()), "CommandList::Reset")) return false;
  D3D12_RESOURCE_BARRIER bar = {}; bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  bar.Transition.pResource = r.backBuffers[fi].get(); bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  bar.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; bar.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  c->ResourceBarrier(1, &bar);
  D3D12_CPU_DESCRIPTOR_HANDLE rtv = r.rtvHeap->GetCPUDescriptorHandleForHeapStart(); rtv.ptr += (SIZE_T)fi * r.rtvStride;
  D3D12_CPU_DESCRIPTOR_HANDLE dsv = r.dsvHeap->GetCPUDescriptorHandleForHeapStart();
  float clear[4] = {rl.sky.r, rl.sky.g, rl.sky.b, 1};
  c->ClearRenderTargetView(rtv, clear, 0, nullptr);
  c->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  c->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
  D3D12_VIEWPORT vpt = {0, 0, (float)r.width, (float)r.height, 0, 1}; D3D12_RECT sc = {0, 0, r.width, r.height};
  c->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  const bool haveModels = models && r.models.ready();
  D3D12_GPU_VIRTUAL_ADDRESS cbv = gpu;
  auto restore = [&](ID3D12PipelineState* pso) { c->SetGraphicsRootSignature(r.rootSig.get()); c->SetGraphicsRootConstantBufferView(0, cbv); c->SetPipelineState(pso); };
  for (int v = 0; v < nv; v++) {
    const RenderList& V = *views[v];
    cbv = gpu + (UINT64)v * 256;
    int x0 = std::max(0, std::min(r.width, V.vpX)), vw = V.vpW > 0 ? std::min(V.vpW, r.width - x0) : r.width - x0;
    D3D12_VIEWPORT vv = {(float)x0, 0, (float)std::max(1, vw), (float)r.height, 0, 1}; D3D12_RECT vs = {x0, 0, x0 + std::max(1, vw), r.height};
    c->RSSetViewports(1, &vv); c->RSSetScissorRects(1, &vs);
    if (v > 0) { float vc[4] = {V.sky.r, V.sky.g, V.sky.b, 1}; c->ClearRenderTargetView(rtv, vc, 1, &vs); }
    restore(r.psoOpaque.get());
    auto drawBatches = [&](const std::vector<Batch>& bs) {
      for (const Batch& b : bs) {
        D3D12_VERTEX_BUFFER_VIEW vbs[2] = {r.meshVB[b.mesh], {gpu + b.off, (UINT)(b.count * sizeof(Instance)), (UINT)sizeof(Instance)}};
        c->IASetVertexBuffers(0, 2, vbs); c->IASetIndexBuffer(&r.meshIB[b.mesh]);
        c->DrawIndexedInstanced(r.meshIdx[b.mesh], b.count, 0, 0, 0);
      }
    };
    if (groundPass[v]) {  // textured ground first, then the primitives
      float gk[8] = {V.time, r.groundInvMean[0], r.groundInvMean[1], r.groundInvMean[2], r.groundInvMean[3], r.groundInvMean[4], 0, 0};
      c->SetGraphicsRootSignature(r.groundRoot.get()); c->SetPipelineState(r.psoGround.get());
      ID3D12DescriptorHeap* hs[] = {r.groundHeap.get()}; c->SetDescriptorHeaps(1, hs);
      c->SetGraphicsRootConstantBufferView(0, cbv); c->SetGraphicsRoot32BitConstants(1, 8, gk, 0);
      c->SetGraphicsRootDescriptorTable(2, r.groundHeap->GetGPUDescriptorHandleForHeapStart());
      D3D12_VERTEX_BUFFER_VIEW gvb = {gpu + groundOff[v], groundVerts[v] * (UINT)sizeof(Impl::GroundVertex), (UINT)sizeof(Impl::GroundVertex)};
      c->IASetVertexBuffers(0, 1, &gvb); c->DrawInstanced(groundVerts[v], 1, 0, 0);
      restore(r.psoOpaque.get());
    }
    drawBatches(opaque[v]);
    if (haveModels) { r.models.draw(c, cbv, (int)fi, *models, v ? 2 : 0); restore(r.psoOpaque.get()); }
    if (!alpha[v].empty()) { c->SetPipelineState(r.psoAlpha.get()); drawBatches(alpha[v]); }
    if (haveModels) { r.models.draw(c, cbv, (int)fi, *models, v ? 3 : 1); restore(r.psoAlpha.get()); }
  }
  // fullscreen again for the video + UI (both HUDs and the menus are in rl.ui, in window pixels)
  cbv = gpu;
  c->RSSetViewports(1, &vpt); c->RSSetScissorRects(1, &sc);
  restore(r.psoUI.get());
  if (vid) {  // after-credits video over the scene, under the UI (sign overlay / caption)
    if (vidUpload) {
      D3D12_RESOURCE_BARRIER tb = {}; tb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; tb.Transition.pResource = r.vidTex.get(); tb.Transition.Subresource = 0;
      tb.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE; tb.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST; c->ResourceBarrier(1, &tb);
      D3D12_TEXTURE_COPY_LOCATION dst = {}; dst.pResource = r.vidTex.get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; dst.SubresourceIndex = 0;
      D3D12_TEXTURE_COPY_LOCATION src = {}; src.pResource = r.upload[fi].get(); src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      src.PlacedFootprint.Offset = vidOff; src.PlacedFootprint.Footprint = {DXGI_FORMAT_B8G8R8A8_UNORM, (UINT)rl.videoW, (UINT)rl.videoH, 1, vidPitch};
      c->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
      std::swap(tb.Transition.StateBefore, tb.Transition.StateAfter); c->ResourceBarrier(1, &tb);
      r.vidSerial = rl.videoSerial;
    }
    float va = (float)rl.videoW / (float)rl.videoH, sa = (float)r.width / (float)r.height;
    float k[4] = {va < sa ? va / sa : 1.f, va < sa ? 1.f : sa / va, 0, 0};
    c->SetGraphicsRootSignature(r.vidRoot.get()); c->SetPipelineState(r.psoVid.get());
    ID3D12DescriptorHeap* hs[] = {r.vidHeap.get()}; c->SetDescriptorHeaps(1, hs);
    c->SetGraphicsRootDescriptorTable(0, r.vidHeap->GetGPUDescriptorHandleForHeapStart()); c->SetGraphicsRoot32BitConstants(1, 4, k, 0);
    c->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); c->DrawInstanced(3, 1, 0, 0);
    restore(r.psoUI.get());
  }
  if (!rl.ui.empty()) {
    c->SetPipelineState(r.psoUI.get());
    D3D12_VERTEX_BUFFER_VIEW uvb = {gpu + uiOff, (UINT)(rl.ui.size() * sizeof(UIVertex)), (UINT)sizeof(UIVertex)};
    c->IASetVertexBuffers(0, 1, &uvb); c->DrawInstanced((UINT)rl.ui.size(), 1, 0, 0);
  }
  bar.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; bar.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  c->ResourceBarrier(1, &bar);
  if (!r.checkDevice(c->Close(), "CommandList::Close")) return false;
  ID3D12CommandList* lists[] = {c};
  r.queue->ExecuteCommandLists(1, lists);
  if (!r.checkDevice(r.swap->Present(1, 0), "Present")) return false;
  r.frameFence[fi] = ++r.fenceValue;
  if (!r.checkDevice(r.queue->Signal(r.fence.get(), r.fenceValue), "Signal")) return false;
  if (r.fence->GetCompletedValue() == UINT64_MAX) return r.checkDevice(r.device->GetDeviceRemovedReason(), "rendering");  // removed devices report ~0
  return true;
}

void Dx12Renderer::shutdown() {
  if (!d) return;
  if (!d->lost) d->waitIdle();
  for (auto& u : d->upload) u.reset();
  d->models.shutdown();
  if (d->fenceEvent) { CloseHandle(d->fenceEvent); d->fenceEvent = nullptr; }
  d->vidTex.reset(); d->vidHeap.reset(); d->psoVid.reset(); d->vidRoot.reset();
  d->groundTex.reset(); d->groundHeap.reset(); d->psoGround.reset(); d->groundRoot.reset(); d->groundOk = false;
  d->psoOpaque.reset(); d->psoAlpha.reset(); d->psoUI.reset(); d->rootSig.reset(); d->meshBuf.reset();
  d->cmd.reset(); for (auto& a : d->alloc) a.reset();
  d->depth.reset(); for (auto& b : d->backBuffers) b.reset();
  d->rtvHeap.reset(); d->dsvHeap.reset(); d->swap.reset(); d->fence.reset(); d->queue.reset(); d->device.reset(); d->factory.reset();
}
}  // namespace tz
