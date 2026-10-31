// Tommyzilla City Smash - Win32 entry point: window, main loop, data/save files, and the DX12 / XAudio2 / input layers.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <memory>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <string>
#include "audio/audio_xaudio2.h"
#include "core/json.h"
#include "game/game.h"
#include "platform/video_mf.h"
#include "input/input_win.h"
#include "renderer/dx12_renderer.h"
#include "model/model_library.h"
#include "model/model_loader.h"
#include "model/model_test_scene.h"
#include "model/game_models.h"

using namespace tz;
namespace {
Dx12Renderer* gRenderer = nullptr;
WinInput gInput;
bool gResized = false, gMinimized = false, gFullscreen = false;
WINDOWPLACEMENT gPlacement = {sizeof(WINDOWPLACEMENT)};

std::wstring widen(const std::string& s) {
  int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
  std::wstring w((size_t)n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w;
}
bool readFileW(const std::wstring& path, std::string& out) {
  HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER sz; if (!GetFileSizeEx(h, &sz) || sz.QuadPart > (64ll << 20)) { CloseHandle(h); return false; }
  out.resize((size_t)sz.QuadPart); DWORD got = 0;
  BOOL ok = out.empty() || ReadFile(h, &out[0], (DWORD)out.size(), &got, nullptr);
  CloseHandle(h); return ok && got == out.size();
}
bool writeFileAtomicW(const std::wstring& path, const std::string& data) {
  std::wstring tmp = path + L".tmp";
  HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  DWORD wrote = 0; BOOL ok = WriteFile(h, data.data(), (DWORD)data.size(), &wrote, nullptr) && wrote == data.size();
  FlushFileBuffers(h); CloseHandle(h);
  if (!ok) { DeleteFileW(tmp.c_str()); return false; }
  return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
std::wstring exeDir() {
  wchar_t buf[MAX_PATH * 2]; DWORD n = GetModuleFileNameW(nullptr, buf, (DWORD)(sizeof buf / sizeof buf[0]));
  std::wstring p(buf, n); size_t s = p.find_last_of(L"\\/"); return s == std::wstring::npos ? L"." : p.substr(0, s);
}
std::wstring saveDir() {  // %APPDATA%\Tommyzilla
  wchar_t buf[MAX_PATH * 2]; DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, (DWORD)(sizeof buf / sizeof buf[0]));
  std::wstring base = (n > 0 && n < sizeof buf / sizeof buf[0]) ? std::wstring(buf, n) : exeDir();
  std::wstring dir = base + L"\\Tommyzilla";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir;
}
void toggleFullscreen(HWND h) {  // borderless fullscreen on the current monitor
  DWORD style = (DWORD)GetWindowLongPtrW(h, GWL_STYLE);
  if (!gFullscreen) {
    MONITORINFO mi = {sizeof(mi)};
    if (GetWindowPlacement(h, &gPlacement) && GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), &mi)) {
      SetWindowLongPtrW(h, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(h, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
      gFullscreen = true;
    }
  } else {
    SetWindowLongPtrW(h, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(h, &gPlacement);
    SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    gFullscreen = false;
  }
}

LRESULT CALLBACK wndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
  gInput.handleMessage(msg, (unsigned long long)wp, (long long)lp);
  switch (msg) {
    case WM_SIZE: gMinimized = wp == SIZE_MINIMIZED; gResized = true; return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO* mm = (MINMAXINFO*)lp; mm->ptMinTrackSize.x = 640; mm->ptMinTrackSize.y = 400; return 0; }
    case WM_SYSKEYDOWN:
      if (wp == VK_RETURN && (lp & (1 << 29))) { toggleFullscreen(h); return 0; }  // Alt+Enter
      if (wp == VK_F4) break;
      return 0;  // swallow F10 / Alt menu activation
    case WM_KEYDOWN: if (wp == VK_F11 && !(lp & (1 << 30))) toggleFullscreen(h); return 0;
    case WM_SYSCOMMAND: if ((wp & 0xfff0) == SC_KEYMENU) return 0; break;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(h, msg, wp, lp);
}
// -modeltest: model viewer (src/model/model_test_scene.cpp). Loads data\models.json + assets\ beside the exe,
// writes modeltest_log.txt there (models, clip names, fallbacks) and runs until the window is closed or Esc.
std::string narrowAcp(const std::wstring& w) {  // narrow fopen() paths use the ANSI code page on Windows
  int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
  std::string s((size_t)n, '\0'); WideCharToMultiByte(CP_ACP, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr); return s;
}
int runModelTest(HWND hwnd, Dx12Renderer& renderer, int w, int h, int camLock) {
  const std::string root = narrowAcp(exeDir()) + "\\";
  LARGE_INTEGER freq, t0, t1; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&t0);
  mdl::ModelLibrary lib; std::string err;
  std::string log = "Tommyzilla -modeltest\n";
  if (!lib.loadManifestFile(root + "data\\models.json", root, err)) log += "manifest: " + err + "\n";
  ModelTestScene scene(lib); scene.init(); scene.camLock = camLock;
  QueryPerformanceCounter(&t1);
  for (const std::string& l : lib.log()) log += l + "\n";
  char b[160]; std::snprintf(b, sizeof b, "loaded %d of %d actors in %.2f s\n", scene.loadedCount(), scene.actorCount(), (double)(t1.QuadPart - t0.QuadPart) / (double)freq.QuadPart); log += b;
  writeFileAtomicW(exeDir() + L"\\modeltest_log.txt", log);
  OutputDebugStringA(log.c_str());
  SetWindowTextW(hwnd, widen("Tommyzilla model test  -  " + renderer.adapterName()).c_str());
  LARGE_INTEGER last; QueryPerformanceCounter(&last);
  const LARGE_INTEGER start = last;
  RenderList rl; ModelDrawList ml;
  LARGE_INTEGER fpsT = last; int fpsFrames = 0;
  for (int frame = 0;; frame++) {
    {
      LARGE_INTEGER n; QueryPerformanceCounter(&n); fpsFrames++;
      double el = (double)(n.QuadPart - fpsT.QuadPart) / (double)freq.QuadPart;
      if (el >= 2.0) {
        scene.wallFps = (float)(fpsFrames / el); fpsT = n; fpsFrames = 0;
        if (frame < 2000) { std::snprintf(b, sizeof b, "fps %.1f at frame %d\n", scene.wallFps, frame); log += b; writeFileAtomicW(exeDir() + L"\\modeltest_log.txt", log); }
      }
    }
    if (frame == 1 || frame == 100) {  // first frame (GPU uploads + PSO warm-up) and steady state timing
      LARGE_INTEGER n; QueryPerformanceCounter(&n);
      std::snprintf(b, sizeof b, "frame %d after %.2f s\n", frame, (double)(n.QuadPart - start.QuadPart) / (double)freq.QuadPart);
      log += b; writeFileAtomicW(exeDir() + L"\\modeltest_log.txt", log);
    }
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) return 0;
      if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { DestroyWindow(hwnd); continue; }
      TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = std::min(0.1f, std::max(0.0f, (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart))); last = now;
    if (gResized) {
      gResized = false; RECT cr; GetClientRect(hwnd, &cr);
      w = std::max(1, (int)(cr.right - cr.left)); h = std::max(1, (int)(cr.bottom - cr.top));
      if (!gMinimized) renderer.resize(w, h);
    }
    if (gMinimized) { Sleep(16); continue; }
    scene.update(dt);
    scene.render(rl, ml, w, h);
    if (!renderer.render(rl, &ml)) return 1;
  }
}
}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdLine, int show) {
  const bool noSound = cmdLine && wcsstr(cmdLine, L"-nosound") != nullptr;  // run silent (e.g. machines without audio)
  const bool noIntro = cmdLine && wcsstr(cmdLine, L"-nointro") != nullptr;  // skip the startup cinematic (testing)
  const bool modelTest = cmdLine && wcsstr(cmdLine, L"-modeltest") != nullptr;  // 3D model viewer instead of the game
  SetProcessDPIAware();
  HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  // ---- data: data\*.json next to the exe (editable), falling back to the copies embedded in the exe
  std::wstring dataDir = exeDir() + L"\\data\\";
  auto load = [&](const char* name) {
    std::string s;
    if (readFileW(dataDir + widen(std::string(name) + ".json"), s)) return s;
    return embeddedJson(name);
  };
  std::string err = loadGameData(load("config"), load("moves"), load("monsters"), load("levels"), load("eggs"));
  if (!err.empty()) {
    std::string again = loadGameData(embeddedJson("config"), embeddedJson("moves"), embeddedJson("monsters"), embeddedJson("levels"), embeddedJson("eggs"));
    MessageBoxA(nullptr, ("A file in the data folder could not be loaded:\n\n" + err + "\n\nUsing the built-in data instead.").c_str(), "Tommyzilla", MB_ICONWARNING);
    if (!again.empty()) { MessageBoxA(nullptr, ("Built-in data is broken: " + again).c_str(), "Tommyzilla", MB_ICONERROR); return 1; }
  }
  // ---- window
  WNDCLASSEXW wc = {sizeof(wc)};
  wc.style = CS_HREDRAW | CS_VREDRAW; wc.lpfnWndProc = wndProc; wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, (LPCWSTR)IDC_ARROW); wc.hIcon = LoadIconW(nullptr, (LPCWSTR)IDI_APPLICATION);
  wc.lpszClassName = L"TommyzillaWindow";
  RegisterClassExW(&wc);
  RECT r = {0, 0, 1280, 720}; AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
  HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Tommyzilla City Smash", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr, nullptr, inst, nullptr);
  if (!hwnd) { MessageBoxA(nullptr, "Could not create the game window.", "Tommyzilla", MB_ICONERROR); return 1; }
  ShowWindow(hwnd, show); UpdateWindow(hwnd);
  gInput.init(hwnd);
  RECT cr; GetClientRect(hwnd, &cr);
  int w = std::max(1, (int)(cr.right - cr.left)), h = std::max(1, (int)(cr.bottom - cr.top));
  std::unique_ptr<Dx12Renderer> renderer(new Dx12Renderer()); gRenderer = renderer.get();
  std::string rerr;
  if (!renderer->init(hwnd, w, h, rerr)) {
    MessageBoxA(hwnd, ("Direct3D 12 could not be started:\n\n" + rerr + "\n\nTommyzilla needs Windows 10/11 with a DirectX 12 capable GPU (or the WARP software renderer).").c_str(), "Tommyzilla", MB_ICONERROR);
    return 1;
  }
  SetWindowTextW(hwnd, widen("Tommyzilla City Smash  -  " + renderer->adapterName()).c_str());
  if (modelTest) {
    const wchar_t* cam = wcsstr(cmdLine, L"-modelcam=");  // -modelcam=N holds camera preset N (0 overview .. 4)
    int rc = runModelTest(hwnd, *renderer, w, h, cam ? _wtoi(cam + 10) : -1);
    renderer->shutdown(); renderer.reset();
    if (SUCCEEDED(co)) CoUninitialize();
    return rc;
  }
  XAudioPlayer audio; std::string aerr;
  if (noSound) aerr = "sound disabled by -nosound";
  else if (!audio.init(aerr)) OutputDebugStringA(("Tommyzilla: " + aerr + "\n").c_str());  // keep going, silently
  // ---- game + 3 save slots: %APPDATA%\Tommyzilla\save_slotN.json, or saves\ beside the exe if that isn't writable
  std::wstring appDir = saveDir(), slotDir = appDir;
  auto writable = [](const std::wstring& dir) {
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring probe = dir + L"\\.write_test";
    bool ok = writeFileAtomicW(probe, "ok"); DeleteFileW(probe.c_str()); return ok;
  };
  if (!writable(slotDir)) slotDir = exeDir() + L"\\saves";
  static SlotStore store;
  store.dir = std::filesystem::path(slotDir);
  store.writer = [](const std::filesystem::path& p, const std::string& t) { return writeFileAtomicW(p.wstring(), t); };  // temp file + MoveFileEx
  if (!store.exists(0)) { std::string old; if (readFileW(appDir + L"\\save.json", old)) store.write(0, old); }  // migrate the old single save into slot 1
  std::unique_ptr<Game> game(new Game());
  game->slots = &store;
  game->playIntro = C.introEnabled > 0.5f && !noIntro;
  game->devSkipGoal = cmdLine && wcsstr(cmdLine, L"-devgoal") != nullptr;  // testing: each level's goal completes after 3 s (boss fights still need winning)
  // ---- 3D models (data\models.json + assets\ beside the exe). Off with -nomodels, "enabled": false in
  // models.json, or "models": {"enabled": false} in config.json; anything missing keeps its primitive look.
  const bool noModels = cmdLine && wcsstr(cmdLine, L"-nomodels") != nullptr;
  const std::string exeRoot = narrowAcp(exeDir()) + "\\";
  mdl::ModelLibrary modelLib; GameModels gameModels(modelLib); ModelDrawList modelDraws;
  {
    std::string mlog = "Tommyzilla models\n", merr;
    if (noModels) mlog += "disabled by -nomodels\n";
    else if (C.modelsEnabled <= 0.5f) mlog += "disabled by config.json models.enabled\n";
    else if (!modelLib.loadManifestFile(exeRoot + "data\\models.json", exeRoot, merr)) mlog += "manifest: " + merr + " -> primitives\n";
    else if (!modelLib.manifest().enabled) mlog += "disabled by models.json \"enabled\": false\n";
    else {
      LARGE_INTEGER f0, a0, a1; QueryPerformanceFrequency(&f0); QueryPerformanceCounter(&a0);
      int n = modelLib.loadAll(); QueryPerformanceCounter(&a1);
      char b[160]; std::snprintf(b, sizeof b, "loaded %d/%zu models in %.0f ms\n", n, modelLib.manifest().models.size(), 1000.0 * (double)(a1.QuadPart - a0.QuadPart) / (double)f0.QuadPart);
      mlog += b;
      for (auto& l : modelLib.log()) mlog += l + "\n";
      gameModels.soloMax = (int)C.modelSoloMax; gameModels.lodNear = C.modelLodNear; gameModels.lodFar = C.modelLodFar;
      game->models = &gameModels;
    }
    writeFileAtomicW(exeDir() + L"\\models_log.txt", mlog);
  }
  game->init(nullptr);  // intro, then the save-slot screen
  // ---- textured ground (data\textures\ground_*.png). -flatground, or any missing / bad file or GPU upload
  // failure, keeps the original flat-coloured ground boxes. Result in ground_log.txt beside the exe.
  {
    std::string glog = "Tommyzilla ground textures\n", gerr;
    if (cmdLine && wcsstr(cmdLine, L"-flatground")) glog += "disabled by -flatground -> flat colours\n";
    else {
      GroundTextureSet gt = loadGroundTextures(exeRoot + "data\\", [](const std::string& path, int& w, int& h, std::vector<uint8_t>& rgba, std::string& err) {
        mdl::Texture t; if (!mdl::decodeImageFile(path, t, err)) return false;
        w = t.width; h = t.height; rgba = std::move(t.rgba); return true;
      });
      if (!gt.ok) glog += "load failed: " + gt.error + " -> flat colours\n";
      else if (!renderer->setGroundTextures(gt, gerr)) glog += "GPU upload failed: " + gerr + " -> flat colours\n";
      else { game->groundTextured = true; glog += "loaded " + std::to_string(GS_COUNT) + " ground textures (" + std::to_string(gt.size) + " px, " + std::to_string(gt.mips[0].size()) + " mips)\n"; }
    }
    writeFileAtomicW(exeDir() + L"\\ground_log.txt", glog);
  }
  // credits (data\credits.json is editable) + the after-credits video (Media Foundation; falls back to the in-engine shot)
  game->loadCredits(load("credits"));
  game->afterVideo = exeRoot + "data\\video\\after_credits.mp4";
  // -devvideopattern: a synthetic 1280x720 test-pattern "video" of the MP4's length, to check the renderer's video
  // texture pass where Media Foundation can't decode H.264 (e.g. Wine). Not used in normal play.
  struct PatternPlayer : VideoPlayer {
    std::vector<uint8_t> px; float t = 0, len = 10; uint64_t serial = 0; int lastF = -1;
    bool start(const std::string& path) override { float d = mp4DurationSeconds(path); len = d > 0 ? d : 10; t = 0; px.assign(1280 * 720 * 4, 0); update(0); return true; }
    void update(float dt) override {
      t += dt; int f = (int)(t * 24); if (f == lastF) return; lastF = f; serial++;
      int bar = (f * 13) % 1280;
      for (int y = 0; y < 720; y++) for (int x = 0; x < 1280; x++) {
        uint8_t* q = &px[((size_t)y * 1280 + x) * 4];
        bool sign = x >= 256 && x < 461 && y >= 108 && y < 324, edge = (x % 160 < 3) || (y % 120 < 3);
        q[0] = (uint8_t)(y * 255 / 719); q[1] = (uint8_t)(x * 255 / 1279); q[2] = (uint8_t)(std::abs(x - bar) < 12 ? 255 : 60); q[3] = 255;
        if (sign) { q[0] = 200; q[1] = 235; q[2] = 245; } if (edge) { q[0] = q[1] = q[2] = 255; }
      }
    }
    bool finished() const override { return t >= len; }
    float duration() const override { return len; }
    void stop() override {}
    const uint8_t* frame(int& w, int& h, int& stride, uint64_t& sr) const override { if (px.empty()) return nullptr; w = 1280; h = 720; stride = 1280 * 4; sr = serial; return px.data(); }
  };
  const bool devPattern = cmdLine && wcsstr(cmdLine, L"-devvideopattern") != nullptr;
  std::unique_ptr<VideoPlayer> videoPlayer(devPattern ? static_cast<VideoPlayer*>(new PatternPlayer()) : createMediaFoundationPlayer());
  game->video = videoPlayer.get();
  // testing (screenshots): -devspin (egg spinner), -devending (credits -> after-credits), -devzoo (Kaiju Zoo opening),
  // -devfinish=gold|chrome|holo (current kaiju look), -devgrow=XP (current kaiju growth XP), -devbulk=1|2|3 (L20 stage)
  auto devArg = [&](const wchar_t* k) -> const wchar_t* { const wchar_t* p = cmdLine ? wcsstr(cmdLine, k) : nullptr; return p ? p + wcslen(k) : nullptr; };
  // testing: -devstart=sandbox | -devstart=N (adventure level N, 1-based) skips the menus; nothing is saved
  if (const wchar_t* ds = cmdLine ? wcsstr(cmdLine, L"-devstart=") : nullptr) {
    game->menus.clear(); game->playIntro = false;
    if (wcsncmp(ds + 10, L"sandbox", 7) == 0) { game->save.tutorial.done = true; game->startMode(false); }
    else {
      int lv = std::max(1, std::min((int)T.levels.size(), _wtoi(ds + 10)));
      game->save.tutorial.done = true; game->save.adventureUnlocked = true; game->startMode(true);
      game->save.adventure.level = lv - 1; game->startLevel(lv - 1);
    }
  }
  if (devArg(L"-devzoo")) { game->menus.clear(); game->playIntro = false; game->save.tutorial.done = true; game->save.spinSlot = true; game->startMode(false); }
  if (const wchar_t* f = devArg(L"-devfinish=")) {
    std::string id = game->kaiju.def().id; KaijuLook l = game->lookOf(id);
    l.finish = wcsncmp(f, L"gold", 4) == 0 ? FIN_GOLD : wcsncmp(f, L"chrome", 6) == 0 ? FIN_CHROME : wcsncmp(f, L"holo", 4) == 0 ? FIN_HOLO : FIN_NONE;
    if (wcsncmp(f, L"roll", 4) == 0) { Rng rr((uint64_t)GetTickCount64()); l = eggs::rollLook(rr, id, T); l.finish = FIN_HOLO; }
    game->save.looks[id] = l; if (!game->P().xp.count(id)) game->P().xp[id] = E.growthFullXp;
  }
  if (const wchar_t* g = devArg(L"-devgrow=")) { std::string id = game->kaiju.def().id; if (!game->hasLook(id)) game->save.looks[id] = game->lookOf(id); game->P().xp[id] = (float)_wtof(g); game->kaiju.camScale = -1; }
  if (const wchar_t* b = devArg(L"-devbulk=")) {
    int st = _wtoi(b); Profile& A = game->AP(); A.gauntlet = A.gauntletSlot = A.gauntletOn = true; A.stones.clear(); for (auto& sd : E.stones) A.stones.push_back(sd.id);
    for (int i = 0; i < 6; i++) { game->kaiju.pos = game->level.pedestal[i]; game->interact(); }
    game->kaiju.pos = Vec3(T.levels.back().startX, 0, T.levels.back().startZ);
    if (st >= 2) { game->restartBulkStage2(); }
    if (st >= 3) { game->boss.stage = 3; game->boss.maxHp = E.bulkStage3Hp; game->boss.hp = game->boss.maxHp * 0.45f; }
  }
  // -devspin[=seconds]: start an egg spin after that many seconds (default 0) so a screenshot can catch it mid-spin
  double devSpinAt = -1;
  if (const wchar_t* sp = devArg(L"-devspin")) { game->menus.clear(); game->playIntro = false; game->save.tutorial.done = true; game->startMode(false); devSpinAt = *sp == L'=' ? _wtof(sp + 1) : 0; }
  double devClock = 0;
  if (devArg(L"-devending") || devArg(L"-devcredits")) { game->menus.clear(); game->playIntro = false; game->startEnding(false); }
  // testing (screenshots): -devlookat keeps the monster + camera turned toward the boss / mini-boss / nearest enemy
  const bool devLookAt = cmdLine && wcsstr(cmdLine, L"-devlookat") != nullptr;
  // -devlookat=riot|armor|flame: pull the nearest unit of that kind in front of the camera (ladder screenshots)
  int devLookType = -1;
  if (const wchar_t* lt = devArg(L"-devlookat=")) devLookType = wcsncmp(lt, L"riot", 4) == 0 ? U_RIOT : wcsncmp(lt, L"armor", 5) == 0 ? U_ARMOR : wcsncmp(lt, L"flame", 5) == 0 ? U_FLAME : -1;
  const bool fpsLog = cmdLine && wcsstr(cmdLine, L"-fpslog") != nullptr;  // fps_log.txt beside the exe every 2 s
  double msList = 0, msModels = 0, msGpu = 0, msSim = 0;
  std::string fpsText; LARGE_INTEGER fpsT0; QueryPerformanceCounter(&fpsT0); int fpsFrames = 0;
  // ---- main loop
  LARGE_INTEGER freq, last; QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&last);
  RenderList rl, rlP2;  // rlP2: split-screen right view
  bool running = true;
  while (running) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) { running = false; break; }
      TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if (!running) break;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    float dt = (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart); last = now;
    dt = std::min(std::max(dt, 0.0f), 0.1f);  // clamp hitches (window drags, breakpoints)
    if (gResized) {
      gResized = false; GetClientRect(hwnd, &cr);
      w = std::max(1, (int)(cr.right - cr.left)); h = std::max(1, (int)(cr.bottom - cr.top));
      if (!gMinimized) renderer->resize(w, h);
    }
    gInput.split = game->players >= 2;  // Titan co-op
    InputState in = gInput.poll(dt, game->wantCapture && GetForegroundWindow() == hwnd);
    game->introHold = GetForegroundWindow() != hwnd || !IsWindowVisible(hwnd) || IsIconic(hwnd);
    devClock += dt;
    if (devSpinAt >= 0 && devClock >= devSpinAt) { devSpinAt = -1; game->P().spins = std::max(game->P().spins, 1); game->beginSpin(SPIN_TOKEN); }
    game->frame(dt, in);
    {  // video_log.txt beside the exe: which after-credits path ran, the length the game uses, and stage changes
      static int lastStage = -1; static Mode lastMode = Mode::Title;
      if ((game->mode == Mode::Ending && game->ending.stage != lastStage) || (lastMode == Mode::Ending && game->mode != Mode::Ending)) {
        if (FILE* vf = _wfopen((exeDir() + L"\\video_log.txt").c_str(), lastStage < 0 ? L"w" : L"a")) {
          SYSTEMTIME st; GetLocalTime(&st);
          const char* nm[] = {"credits", "the-end", "after-credits", "video-done(wait-button)"};
          if (game->mode == Mode::Ending)
            std::fprintf(vf, "%02d:%02d:%02d.%03d stage %d %s | path %s | video length %.3f s | file %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
                         game->ending.stage, nm[std::min(3, std::max(0, game->ending.stage))], game->ending.stage >= 2 ? (game->ending.usedVideo ? "media-foundation" : "in-engine-fallback") : "-",
                         game->ending.videoLen, game->afterVideo.c_str());
          else std::fprintf(vf, "%02d:%02d:%02d.%03d left the ending -> %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, game->mode == Mode::Title ? "title" : "other");
          std::fclose(vf);
        }
        lastStage = game->mode == Mode::Ending ? game->ending.stage : 99;
      }
      lastMode = game->mode;
    }
    if (devLookAt && game->mode == Mode::Play) {
      const Vec3 kp = game->kaiju.pos; bool have = false; Vec3 tp; float best = 1e9f;
      if (game->boss.active) { tp = game->boss.pos; have = true; }
      else if (const Unit* mb = game->enemies.miniBoss()) { tp = mb->pos; have = true; }
      else for (Unit& u : game->enemies.units) {
        if (devLookType >= 0 && (int)u.type != devLookType) continue;
        float dd = dist2D(u.pos.x, u.pos.z, kp.x, kp.z);
        if (!u.wreck && dd < best) { best = dd; tp = u.pos; have = true; }
      }
      if (devLookType >= 0 && !have && game->enemies.units.size() < 60) game->enemies.addUnit((UnitType)devLookType);
      if (devLookType >= 0 && have) {  // pin it between the camera and the monster, a bit to the side, facing the camera
        const Vec3 cp = game->kaiju.cam.pos;
        for (Unit& u : game->enemies.units) if ((int)u.type == devLookType && !u.wreck && dist2D(u.pos.x, u.pos.z, kp.x, kp.z) == best) {
          Vec3 d(cp.x - kp.x, 0, cp.z - kp.z); float l = std::max(1.f, length(d)); d = d * (1.f / l); Vec3 side(d.z, 0, -d.x);
          float k = devLookType == U_ARMOR ? 0.12f : -0.05f;
          u.pos.x = kp.x + d.x * l * k + side.x * (devLookType == U_ARMOR ? 13.f : 8.f); u.pos.z = kp.z + d.z * l * k + side.z * (devLookType == U_ARMOR ? 13.f : 8.f);
          u.vel = Vec3(); u.dir = std::atan2(cp.x - u.pos.x, cp.z - u.pos.z) - 0.5f; break; }
        have = false;  // keep the normal camera
      }
      if (have && dist2D(tp.x, tp.z, kp.x, kp.z) > 1) {
        game->kaiju.facing = std::atan2(tp.x - kp.x, tp.z - kp.z);
        game->kaiju.cam.yaw = game->kaiju.facing + PI; game->kaiju.cam.pitch = 0.22f;
      }
    }
    for (const SoundEvent& e : game->sounds) audio.play(e);
    audio.setMaster(game->save.settings.volume, game->save.settings.muted);
    audio.setLoopVolumes(game->loopVol, dt);
    if (game->rumbleT > 0) gInput.setRumble(game->rumble, game->rumbleT);
    if (game->quit) { DestroyWindow(hwnd); continue; }
    if (gMinimized) { Sleep(16); continue; }
    LARGE_INTEGER q0, q1, q2, q3; QueryPerformanceCounter(&q0);
    const bool split = game->render(rl, w, h, &rlP2);  // 2P split screen: left = P1 (rl), right = P2 (rlP2)
    QueryPerformanceCounter(&q1);
    modelDraws.clear();
    if (game->models) { if (split) gameModels.build(rl, rlP2, modelDraws); else gameModels.build(rl, modelDraws); }
    QueryPerformanceCounter(&q2);
    if (!renderer->render(rl, game->models ? &modelDraws : nullptr, split ? &rlP2 : nullptr)) break;  // device lost: the renderer already showed an error box
    QueryPerformanceCounter(&q3);
    if (fpsLog) { auto ms = [&](LARGE_INTEGER a, LARGE_INTEGER b) { return 1000.0 * (double)(b.QuadPart - a.QuadPart) / (double)freq.QuadPart; };
                  msList += ms(q0, q1); msModels += ms(q1, q2); msGpu += ms(q2, q3); msSim += ms(now, q0); }
    if (fpsLog) {
      fpsFrames++; LARGE_INTEGER n; QueryPerformanceCounter(&n);
      double el = (double)(n.QuadPart - fpsT0.QuadPart) / (double)freq.QuadPart;
      if (el >= 2.0) {
        int sk = 0, ru = 0, dc = 0, bc = 0, cc = 0; renderer->modelSkinCounts(sk, ru); renderer->modelDrawCounts(dc, bc, cc);
        int ppl = 0; for (auto& p : game->people.list) ppl += p.active;
        char b[480];
        std::snprintf(b, sizeof b, "t=%.0fs fps %.1f | level %d boss %d | units %zu people %d | model req %d drawn %d culled %d solo %d shared %d anim %d | skinned %d reused %d batches %d calls %d | prim inst %d | ms/frame sim %.1f list %.1f models %.1f render+present %.1f\n",
                      game->time, fpsFrames / el, game->adventure ? game->save.adventure.level + 1 : 0, game->boss.active ? 1 : 0, game->enemies.units.size(), ppl,
                      gameModels.stats.requests, gameModels.stats.drawn, gameModels.stats.culled, gameModels.stats.solo, gameModels.stats.shared, gameModels.stats.animators, sk, ru, bc, cc, (int)rl.instanceCount(), msSim / fpsFrames, msList / fpsFrames, msModels / fpsFrames, msGpu / fpsFrames);
        msSim = msList = msModels = msGpu = 0;
        fpsText += b; writeFileAtomicW(exeDir() + L"\\fps_log.txt", fpsText);
        fpsT0 = n; fpsFrames = 0;
      }
    }
  }
  game->persist();
  gInput.shutdown(); audio.shutdown(); renderer->shutdown();
  game.reset(); renderer.reset();
  if (SUCCEEDED(co)) CoUninitialize();
  return 0;
}
