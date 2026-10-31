#include "video_mf.h"
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <cstring>
#include <vector>
namespace tz {
namespace {
template <class T> void release(T*& p) { if (p) { p->Release(); p = nullptr; } }
std::wstring widenUtf8(const std::string& s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0); std::wstring w(n > 0 ? n - 1 : 0, L'\0');
  if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n); return w;
}
struct MfPlayer : VideoPlayer {
  bool mfUp = false; IMFSourceReader* reader = nullptr;
  int w = 0, h = 0; float len = 0, t = 0; bool eos = false;
  std::vector<uint8_t> cur; uint64_t serial = 0;
  LONGLONG nextTs = -1; std::vector<uint8_t> pending; bool havePending = false;  // the decoded frame waiting for its time
  ~MfPlayer() override { stop(); if (mfUp) MFShutdown(); }
  bool start(const std::string& path) override {
    stop(); t = 0; eos = false; serial = 0; havePending = false;
    if (!mfUp) { if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) return false; mfUp = true; }
    IMFAttributes* attr = nullptr;
    if (FAILED(MFCreateAttributes(&attr, 2))) return false;
    attr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);  // lets the reader convert NV12 -> RGB32
    HRESULT hr = MFCreateSourceReaderFromURL(widenUtf8(path).c_str(), attr, &reader);
    release(attr);
    if (FAILED(hr) || !reader) { reader = nullptr; return false; }
    reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
    IMFMediaType* mt = nullptr;
    if (FAILED(MFCreateMediaType(&mt))) { stop(); return false; }
    mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video); mt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    hr = reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, mt);
    release(mt);
    if (FAILED(hr)) { stop(); return false; }  // no H.264 decoder / converter available
    IMFMediaType* out = nullptr;
    if (FAILED(reader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &out))) { stop(); return false; }
    UINT32 fw = 0, fh = 0; MFGetAttributeSize(out, MF_MT_FRAME_SIZE, &fw, &fh); release(out);
    w = (int)fw; h = (int)fh; if (w <= 0 || h <= 0) { stop(); return false; }
    PROPVARIANT pv; PropVariantInit(&pv);
    if (SUCCEEDED(reader->GetPresentationAttribute((DWORD)MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &pv)) && pv.vt == VT_UI8) len = (float)((double)pv.uhVal.QuadPart / 1e7);
    PropVariantClear(&pv);
    if (len <= 0) len = 10;
    cur.assign((size_t)w * h * 4, 0);
    if (!readNext()) { stop(); return false; }  // could not decode even one frame -> fallback
    promote();
    return true;
  }
  bool readNext() {  // decode one frame into 'pending'
    if (!reader || eos) return false;
    DWORD flags = 0; LONGLONG ts = 0; IMFSample* smp = nullptr;
    HRESULT hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, &ts, &smp);
    if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) { release(smp); eos = true; return false; }
    if (flags & MF_SOURCE_READERF_ERROR) { release(smp); eos = true; return false; }
    if (!smp) return readNext();  // stream tick / gap
    IMFMediaBuffer* buf = nullptr;
    if (SUCCEEDED(smp->ConvertToContiguousBuffer(&buf))) {
      pending.assign((size_t)w * h * 4, 0);
      IMF2DBuffer* b2 = nullptr; BYTE* p = nullptr; LONG pitch = 0;
      if (SUCCEEDED(buf->QueryInterface(IID_PPV_ARGS(&b2))) && SUCCEEDED(b2->Lock2D(&p, &pitch))) {
        for (int y = 0; y < h; y++) std::memcpy(&pending[(size_t)y * w * 4], p + (LONGLONG)y * pitch, (size_t)w * 4);  // negative pitch = bottom-up, handled
        b2->Unlock2D();
      } else {
        DWORD maxLen = 0, curLen = 0;
        if (SUCCEEDED(buf->Lock(&p, &maxLen, &curLen))) { std::memcpy(pending.data(), p, std::min<size_t>(pending.size(), curLen)); buf->Unlock(); }
      }
      release(b2); release(buf);
    }
    release(smp);
    nextTs = ts; havePending = true;
    return true;
  }
  void promote() { if (havePending) { cur.swap(pending); havePending = false; serial++; } }
  void update(float dt) override {
    if (!reader && !havePending) { t += dt; return; }
    t += dt;
    LONGLONG now = (LONGLONG)(t * 1e7);
    int guard = 0;
    while (guard++ < 8) {  // catch up (drops frames if the game hitches)
      if (!havePending && !readNext()) break;
      if (nextTs > now) break;
      promote();
    }
  }
  bool finished() const override { return (eos && !havePending && t >= len - 0.1f) || t >= len + 0.25f; }
  float duration() const override { return len; }
  void stop() override { release(reader); }  // keeps the last frame for the "press any button" hold
  const uint8_t* frame(int& fw, int& fh, int& stride, uint64_t& s) const override {
    if (cur.empty() || serial == 0) return nullptr;
    fw = w; fh = h; stride = w * 4; s = serial; return cur.data();
  }
};
}  // namespace
VideoPlayer* createMediaFoundationPlayer() { return new MfPlayer(); }
}  // namespace tz
