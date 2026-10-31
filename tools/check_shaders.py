#!/usr/bin/env python3
"""Compile every embedded HLSL entry point with Microsoft's compilers, the way the game does at startup.

The game compiles kShader (src/renderer/dx12_renderer.cpp) and kModelShader (src/model/dx12_model_renderer.cpp) with
D3DCompile (fxc rules, vs_5_0 / ps_5_0, D3DCOMPILE_ENABLE_STRICTNESS, O3 in release, Od + debug in debug builds).
Wine's vkd3d is more lenient than that, so run this before shipping a shader change.

  python3 tools/check_shaders.py --dxc /path/to/dxc            # DirectXShaderCompiler (Linux release), SM 6.0 + 6.6
  python3 tools/check_shaders.py --fxc /path/to/fxc.exe         # Windows SDK fxc.exe (under Wine on Linux) with its
                                                                # d3dcompiler_47.dll beside it: the same compiler as D3DCompile
Both can be given. Entry points and targets are read from the C++ (compile("X", "vs_5_0") / D3DCompile(..., "X", "vs_5_0")),
so new shaders are covered automatically. Exit code 0 = everything compiled cleanly (warnings are errors).
"""
import argparse, os, re, subprocess, sys, tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = [("src/renderer/dx12_renderer.cpp", "kShader", "tommyzilla.hlsl"),
           ("src/model/dx12_model_renderer.cpp", "kModelShader", "tommyzilla_model.hlsl")]

def shaders():
    for rel, var, name in SOURCES:
        src = open(os.path.join(ROOT, rel), encoding="utf-8").read()
        m = re.search(r'const char\* ' + var + r' = R"HLSL\((.*?)\)HLSL"', src, re.S)
        if not m: sys.exit(f"{rel}: {var} not found")
        entries = sorted(set(re.findall(r'"(\w+)", "([vp]s_5_0)"', src)))
        if not entries: sys.exit(f"{rel}: no entry points found")
        yield name, m.group(1), entries

def run(cmd, env=None):
    p = subprocess.run(cmd, capture_output=True, text=True, errors="replace", env=env)
    out = (p.stdout + p.stderr).replace("\r", "")
    bad = p.returncode != 0 or re.search(r"\b(error|warning)\b", out, re.I)
    if bad and not out.strip(): out = f"exit code {p.returncode}"
    return (not bad), out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dxc"); ap.add_argument("--fxc"); ap.add_argument("--wine", default="wine")
    a = ap.parse_args()
    if not a.dxc and not a.fxc: sys.exit("give --dxc and/or --fxc")
    fails = 0; tmp = tempfile.mkdtemp(prefix="tz_hlsl_")
    for name, text, entries in shaders():
        path = os.path.join(tmp, name); open(path, "w", newline="\n").write(text)
        for entry, target in entries:
            stage = target[:2]
            runs = []
            if a.dxc:
                for sm in ("6_0", "6_6"):
                    runs.append((f"dxc {stage}_{sm}", [a.dxc, "-T", f"{stage}_{sm}", "-E", entry, "-Ges", "-O3", "-WX", path, "-Fo", os.devnull]))
            if a.fxc:
                env = dict(os.environ, WINEDLLOVERRIDES="d3dcompiler_47=n", WINEDEBUG="-all")
                pre = [] if os.name == "nt" else [a.wine]
                # fxc reads a leading '/' as an option, so under Wine pass Z:\ paths
                wpath = path if os.name == "nt" else "Z:" + path.replace("/", "\\")
                for fl in (["/O3"], ["/Zi", "/Od"]):
                    runs.append((f"fxc {target} {' '.join(fl)}", pre + [a.fxc, "/nologo", "/Ges", *fl, "/T", target, "/E", entry, "/Fo", wpath + ".cso", wpath], env))
            for r in runs:
                label, cmd = r[0], r[1]; ok, out = run(cmd, r[2] if len(r) > 2 else None)
                print(f"{'ok  ' if ok else 'FAIL'} {name} {entry:9s} {label}")
                if not ok:
                    fails += 1; print("     " + out.strip().replace("\n", "\n     "))
    print(f"{fails} failure(s)")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
