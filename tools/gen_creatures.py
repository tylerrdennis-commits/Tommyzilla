#!/usr/bin/env python3
"""Builds the two hero models that had no 3D mesh: Captain Clamshell (crab) and Slinkysnoot (serpent).

Writes assets/polypizza/Crab.glb and assets/polypizza/Serpent.glb: skinned glTF 2.0, solid-colour materials (no textures),
Y up, facing +Z, feet on y=0, with the same logical clip set as the other monsters (Idle / Walk / Run / Attack / Death / Hit /
Eat / Stun / Jump / Cheer). Materials follow the pack's naming so the game's recolouring works: *_Main -> body colour,
*_Secondary -> accent, names containing "Eye" -> eye colour. Original work for this game (CC0).
Usage: python3 tools/gen_creatures.py   then   python3 tools/gen_models_json.py
"""
import json, math, os, struct
import numpy as np

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'polypizza')
FPS = 24

# ------------------------------------------------------------------ small math
def qmul(a, b):
    x1, y1, z1, w1 = a; x2, y2, z2, w2 = b
    return (w1*x2 + x1*w2 + y1*z2 - z1*y2, w1*y2 - x1*z2 + y1*w2 + z1*x2, w1*z2 + x1*y2 - y1*x2 + z1*w2, w1*w2 - x1*x2 - y1*y2 - z1*z2)
def qaxis(ax, a):
    s = math.sin(a / 2); return (ax[0]*s, ax[1]*s, ax[2]*s, math.cos(a / 2))
def rx(a): return qaxis((1, 0, 0), a)
def ry(a): return qaxis((0, 1, 0), a)
def rz(a): return qaxis((0, 0, 1), a)
def rot(x=0, y=0, z=0): return qmul(qmul(ry(y), rx(x)), rz(z))   # radians
QI = (0.0, 0.0, 0.0, 1.0)
D = math.radians

# ------------------------------------------------------------------ geometry builders (parts carry per-vertex joints/weights)
class Part:
    def __init__(self, mat): self.mat = mat; self.P = []; self.N = []; self.J = []; self.W = []; self.I = []
    def add(self, P, N, I, J, W):
        base = len(self.P); self.P += [tuple(p) for p in P]; self.N += [tuple(n) for n in N]; self.J += J; self.W += W
        for t in range(0, len(I), 3):   # make every triangle's winding agree with its vertex normals (outward = counter-clockwise)
            a, b, c = I[t], I[t + 1], I[t + 2]
            pa, pb, pc = np.array(P[a], float), np.array(P[b], float), np.array(P[c], float)
            nn = np.array(N[a], float) + np.array(N[b], float) + np.array(N[c], float)
            if np.dot(np.cross(pb - pa, pc - pa), nn) < 0: b, c = c, b
            self.I += [base + a, base + b, base + c]

def rigid(joint, n): return [(joint, 0, 0, 0)] * n, [(1.0, 0, 0, 0)] * n

def ellipsoid(center, radii, nu=14, nv=9):
    P, N, I = [], [], []
    for j in range(nv + 1):
        th = math.pi * j / nv
        for i in range(nu + 1):
            ph = 2 * math.pi * i / nu
            u = np.array([math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)])
            P.append(np.array(center) + u * np.array(radii))
            n = u / np.array(radii); N.append(n / np.linalg.norm(n))
    for j in range(nv):
        for i in range(nu):
            a = j * (nu + 1) + i; b = a + nu + 1
            I += [a, b, a + 1, a + 1, b, b + 1]
    return P, N, I

def frame_from(axis):
    a = np.array(axis, float); a /= np.linalg.norm(a)
    h = np.array([0, 1.0, 0]) if abs(a[1]) < 0.9 else np.array([1.0, 0, 0])
    u = np.cross(a, h); u /= np.linalg.norm(u); v = np.cross(a, u)
    return a, u, v

def frustum(p0, p1, r0, r1, seg=10):
    p0 = np.array(p0, float); p1 = np.array(p1, float)
    a, u, v = frame_from(p1 - p0)
    P, N, I = [], [], []
    slope = (r0 - r1) / np.linalg.norm(p1 - p0)
    for k, (p, r) in enumerate(((p0, r0), (p1, r1))):
        for i in range(seg + 1):
            ph = 2 * math.pi * i / seg
            d = u * math.cos(ph) + v * math.sin(ph)
            P.append(p + d * r); n = d + a * slope; N.append(n / np.linalg.norm(n))
    for i in range(seg):
        I += [i, seg + 1 + i, i + 1, i + 1, seg + 1 + i, seg + 2 + i]
    for k, (p, sgn) in enumerate(((p0, -1), (p1, 1))):   # end caps
        c = len(P); P.append(p); N.append(a * sgn)
        r = (r0, r1)[k]
        for i in range(seg + 1):
            ph = 2 * math.pi * i / seg; P.append(p + (u * math.cos(ph) + v * math.sin(ph)) * r); N.append(a * sgn)
        for i in range(seg):
            I += [c, c + 1 + i, c + 2 + i] if sgn > 0 else [c, c + 2 + i, c + 1 + i]
    return P, N, I

def box(center, half):
    c = np.array(center, float); h = np.array(half, float); P, N, I = [], [], []
    for ax in range(3):
        for sgn in (-1, 1):
            n = np.zeros(3); n[ax] = sgn
            u = np.zeros(3); u[(ax + 1) % 3] = 1; v = np.cross(n, u)
            base = len(P)
            for du, dv in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                P.append(c + (n + u * du + v * dv) * h); N.append(n)
            I += [base, base + 1, base + 2, base, base + 2, base + 3] if sgn > 0 else [base, base + 2, base + 1, base, base + 3, base + 2]
    return P, N, I

# ------------------------------------------------------------------ skeleton + glb writer
class Rig:
    def __init__(self): self.names = []; self.parent = []; self.pos = []   # pos = world rest position (rest rotations are identity)
    def add(self, name, parent, pos):
        self.names.append(name); self.parent.append(parent); self.pos.append(np.array(pos, float)); return len(self.names) - 1
    def idx(self, name): return self.names.index(name)
    def local(self, j): return self.pos[j] - (self.pos[self.parent[j]] if self.parent[j] >= 0 else 0)

def write_glb(path, rig, parts, materials, clips):
    """materials: name -> rgba; clips: name -> (duration, loop, {joint: fn(t)->(quat or None, trans or None)})."""
    bin_ = bytearray(); views = []; accs = []
    def view(data, target=None):
        while len(bin_) % 4: bin_.append(0)
        views.append({'buffer': 0, 'byteOffset': len(bin_), 'byteLength': len(data), **({'target': target} if target else {})})
        bin_.extend(data); return len(views) - 1
    def acc(vi, comp, count, typ, mn=None, mx=None):
        a = {'bufferView': vi, 'componentType': comp, 'count': count, 'type': typ}
        if mn is not None: a['min'] = mn; a['max'] = mx
        accs.append(a); return len(accs) - 1
    matnames = list(materials.keys())
    prims = []
    for part in parts:
        if not part.I: continue
        P = np.array(part.P, np.float32); N = np.array(part.N, np.float32)
        J = np.array(part.J, np.uint8); W = np.array(part.W, np.float32); W /= W.sum(axis=1, keepdims=True)
        I = np.array(part.I, np.uint16)
        attrs = {
            'POSITION': acc(view(P.tobytes(), 34962), 5126, len(P), 'VEC3', P.min(0).tolist(), P.max(0).tolist()),
            'NORMAL': acc(view(N.tobytes(), 34962), 5126, len(N), 'VEC3'),
            'JOINTS_0': acc(view(J.tobytes(), 34962), 5121, len(J), 'VEC4'),
            'WEIGHTS_0': acc(view(W.tobytes(), 34962), 5126, len(W), 'VEC4'),
        }
        prims.append({'attributes': attrs, 'indices': acc(view(I.tobytes(), 34963), 5123, len(I), 'SCALAR'), 'material': matnames.index(part.mat), 'mode': 4})
    nj = len(rig.names)
    ibm = np.zeros((nj, 4, 4), np.float32)
    for j in range(nj):
        m = np.eye(4, dtype=np.float32); m[:3, 3] = -rig.pos[j]; ibm[j] = m.T   # column-major
    ibm_acc = acc(view(ibm.tobytes()), 5126, nj, 'MAT4')
    nodes = []
    for j in range(nj):
        n = {'name': rig.names[j], 'translation': [float(x) for x in rig.local(j)]}
        kids = [k for k in range(nj) if rig.parent[k] == j]
        if kids: n['children'] = [k for k in kids]
        nodes.append(n)
    nodes.append({'name': 'Mesh', 'mesh': 0, 'skin': 0})
    roots = [j for j in range(nj) if rig.parent[j] < 0]
    animations = []
    for cname, (dur, loop, fns) in clips.items():
        frames = max(2, int(round(dur * FPS)) + 1); times = np.linspace(0, dur, frames).astype(np.float32)
        samplers, channels = [], []
        tin = acc(view(times.tobytes()), 5126, frames, 'SCALAR', [float(times[0])], [float(times[-1])])
        for jname, fn in fns.items():
            j = rig.idx(jname); R = []; T = []; hasR = hasT = False
            for t in times:
                q, tr = fn(float(t))
                if q is not None: hasR = True
                if tr is not None: hasT = True
                R.append(q if q is not None else QI); T.append(tr if tr is not None else (0, 0, 0))
            if hasR:
                R = np.array(R, np.float32)
                for k in range(1, len(R)):   # keep neighbouring quaternions in the same hemisphere (short-way slerp)
                    if np.dot(R[k], R[k - 1]) < 0: R[k] = -R[k]
                samplers.append({'input': tin, 'output': acc(view(R.tobytes()), 5126, frames, 'VEC4'), 'interpolation': 'LINEAR'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': j, 'path': 'rotation'}})
            if hasT:
                base = rig.local(j); Tv = np.array([base + np.array(x) for x in T], np.float32)
                samplers.append({'input': tin, 'output': acc(view(Tv.tobytes()), 5126, frames, 'VEC3'), 'interpolation': 'LINEAR'})
                channels.append({'sampler': len(samplers) - 1, 'target': {'node': j, 'path': 'translation'}})
        animations.append({'name': cname, 'samplers': samplers, 'channels': channels})
    gltf = {
        'asset': {'version': '2.0', 'generator': 'tools/gen_creatures.py (original model for Tommyzilla City Smash, CC0)'},
        'scene': 0, 'scenes': [{'nodes': roots + [nj]}], 'nodes': nodes,
        'meshes': [{'name': 'Body', 'primitives': prims}],
        'skins': [{'name': 'Armature', 'joints': list(range(nj)), 'inverseBindMatrices': ibm_acc, 'skeleton': roots[0]}],
        'materials': [{'name': n, 'pbrMetallicRoughness': {'baseColorFactor': list(c), 'metallicFactor': 0.0, 'roughnessFactor': 0.8}, 'doubleSided': False} for n, c in materials.items()],
        'animations': animations, 'accessors': accs, 'bufferViews': views, 'buffers': [{'byteLength': len(bin_)}],
    }
    js = json.dumps(gltf, separators=(',', ':')).encode()
    while len(js) % 4: js += b' '
    while len(bin_) % 4: bin_.append(0)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(bin_)))
        f.write(struct.pack('<II', len(js), 0x4E4F534A)); f.write(js)
        f.write(struct.pack('<II', len(bin_), 0x004E4942)); f.write(bin_)
    return gltf

def hexc(h, a=1.0):
    h = h.lstrip('#'); return (int(h[0:2], 16) / 255, int(h[2:4], 16) / 255, int(h[4:6], 16) / 255, a)

def lin(c):   # glTF colours are linear; the game's recolours are sRGB hex, so store sRGB-ish values as-is like the other models
    return c

# ================================================================== CAPTAIN CLAMSHELL (crab)
def build_crab():
    rig = Rig(); parts = {}
    def part(mat):
        if mat not in parts: parts[mat] = Part(mat)
        return parts[mat]
    root = rig.add('Root', -1, (0, 0, 0))
    body = rig.add('Body', root, (0, 0.62, 0))
    eyeL = rig.add('Eye.L', body, (0.30, 0.95, 0.55)); eyeR = rig.add('Eye.R', body, (-0.30, 0.95, 0.55))
    arms = {}
    for s, side in ((1, 'L'), (-1, 'R')):
        sh = rig.add(f'Shoulder.{side}', body, (0.55 * s, 0.62, 0.50))
        el = rig.add(f'Elbow.{side}', sh, (0.85 * s, 0.70, 0.95))
        wr = rig.add(f'Wrist.{side}', el, (0.62 * s, 0.72, 1.38))
        pu = rig.add(f'PincerUp.{side}', wr, (0.62 * s, 0.76, 1.50))
        pd = rig.add(f'PincerDown.{side}', wr, (0.62 * s, 0.66, 1.50))
        arms[side] = (sh, el, wr, pu, pd)
    legs = []
    for s, side in ((1, 'L'), (-1, 'R')):
        for k, z in enumerate((0.38, 0.0, -0.38)):
            hip = rig.add(f'Hip.{side}{k}', body, (0.62 * s, 0.58, z))
            knee = rig.add(f'Knee.{side}{k}', hip, (1.02 * s, 0.80, z + (0.10 if k == 0 else -0.10 if k == 2 else 0)))
            legs.append((side, k, hip, knee, s, z))
    main, sec, belly = 'Crab_Main', 'Crab_Secondary', 'Crab_Belly'
    # body: a wide shell with a lighter underside, a shell crest, a captain's hat
    def put(mat, geo, j):
        P, N, I = geo; J, W = rigid(j, len(P)); part(mat).add(P, N, I, J, W)
    put(main, ellipsoid((0, 0.66, 0), (0.92, 0.44, 0.74)), body)
    put(belly, ellipsoid((0, 0.44, 0.06), (0.80, 0.26, 0.64)), body)
    put(sec, ellipsoid((0, 0.96, -0.12), (0.50, 0.16, 0.40)), body)
    put('Hat_Navy', frustum((0, 1.06, -0.12), (0, 1.40, -0.12), 0.40, 0.34, 14), body)
    put('Hat_Navy', frustum((0, 1.04, -0.12), (0, 1.09, -0.12), 0.62, 0.62, 18), body)
    put('Hat_Gold', frustum((0, 1.09, -0.12), (0, 1.18, -0.12), 0.405, 0.405, 14), body)
    put('Hat_Gold', box((0, 1.23, 0.24), (0.12, 0.06, 0.02)), body)
    put('Crab_Pupil', box((0, 0.50, 0.70), (0.20, 0.035, 0.02)), body)      # tiny grumpy mouth line
    for s, j in ((1, eyeL), (-1, eyeR)):
        put(main, frustum((0.30 * s, 0.90, 0.55), (0.30 * s, 1.07, 0.58), 0.05, 0.045, 8), j)
        put('Crab_Eye', ellipsoid((0.30 * s, 1.14, 0.60), (0.13, 0.13, 0.13), 10, 7), j)
        put('Crab_Pupil', ellipsoid((0.30 * s, 1.14, 0.72), (0.065, 0.065, 0.05), 8, 6), j)
    for side, (sh, el, wr, pu, pd) in arms.items():
        s = 1 if side == 'L' else -1
        put(main, frustum((0.55 * s, 0.62, 0.50), (0.85 * s, 0.70, 0.95), 0.11, 0.10, 8), sh)
        put(main, frustum((0.85 * s, 0.70, 0.95), (0.62 * s, 0.72, 1.38), 0.10, 0.12, 8), el)
        put(main, ellipsoid((0.62 * s, 0.72, 1.40), (0.16, 0.14, 0.14), 9, 6), wr)
        put(sec, ellipsoid((0.62 * s, 0.84, 1.72), (0.19, 0.13, 0.36), 10, 7), pu)     # upper pincer
        put(sec, ellipsoid((0.62 * s, 0.58, 1.68), (0.17, 0.10, 0.30), 10, 7), pd)     # lower pincer
    for side, k, hip, knee, s, z in legs:
        zk = rig.pos[knee][2]
        put(main, frustum((0.62 * s, 0.58, z), (1.02 * s, 0.80, zk), 0.105, 0.085, 8), hip)
        put(main, frustum((1.02 * s, 0.80, zk), (1.34 * s, 0.03, zk + (0.18 if k == 0 else -0.18 if k == 2 else 0)), 0.085, 0.045, 8), knee)
    materials = {main: hexc('#f0e0c0'), sec: hexc('#2fa89a'), belly: hexc('#c8b890'), 'Hat_Navy': hexc('#1d2b4f'), 'Hat_Gold': hexc('#f2c14e'),
                 'Crab_Eye': hexc('#ffffff'), 'Crab_Pupil': hexc('#151515')}
    # ---- clips: fn(t) -> (quat|None, trans|None)
    def cyc(t, T): return 2 * math.pi * t / T
    clips = {}
    def swing(side, k, T, amp, lift, phase_off=0.0):
        sgn = 1 if side == 'L' else -1
        grp = (k % 2) ^ (0 if side == 'L' else 1)   # alternating tripod: L0 R1 L2 / R0 L1 R2
        ph = math.pi * grp + phase_off
        def hipf(t):
            a = cyc(t, T) + ph
            return (rot(0, amp * math.sin(a), sgn * max(0.0, math.cos(a)) * lift), None)
        return hipf
    def locomotion(T, amp, lift, bob, claws):
        f = {}
        for side, k, hip, knee, s, z in legs:
            f[rig.names[hip]] = swing(side, k, T, amp, lift)
            f[rig.names[knee]] = (lambda sgn, kk: (lambda t: (rot(0, 0, sgn * 0.12 * math.sin(cyc(t, T) + kk)), None)))(s, k)
        f['Body'] = lambda t: (rot(0.03 * math.sin(cyc(t, T) * 2), 0.04 * math.sin(cyc(t, T)), 0.05 * math.sin(cyc(t, T))), (0, bob * abs(math.sin(cyc(t, T))), 0))
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(-claws * 0.5 + 0.08 * math.sin(cyc(t, T) + sg), sg * 0.10 * math.sin(cyc(t, T) * 2), 0), None)))(sg)
            f[f'PincerUp.{side}'] = lambda t: (rot(-0.15 - 0.10 * math.sin(cyc(t, T) * 2), 0, 0), None)
        f['Eye.L'] = lambda t: (rot(0.12 * math.sin(cyc(t, T) * 2), 0, 0), None); f['Eye.R'] = lambda t: (rot(0.12 * math.sin(cyc(t, T) * 2 + 1), 0, 0), None)
        return f
    clips['CrabArmature|Walk'] = (0.8, True, locomotion(0.8, 0.30, 0.55, 0.05, 0.1))
    clips['CrabArmature|Run'] = (0.5, True, locomotion(0.5, 0.42, 0.75, 0.08, 0.25))
    def idle(T=2.4):
        f = {'Body': lambda t: (rot(0.02 * math.sin(cyc(t, T)), 0, 0.02 * math.sin(cyc(t, T) + 1)), (0, 0.02 * math.sin(cyc(t, T)), 0))}
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'PincerUp.{side}'] = (lambda sg: (lambda t: (rot(-0.12 - 0.22 * max(0, math.sin(cyc(t, T) * 2 + sg)), 0, 0), None)))(sg)
            f[f'PincerDown.{side}'] = (lambda sg: (lambda t: (rot(0.08 + 0.15 * max(0, math.sin(cyc(t, T) * 2 + sg)), 0, 0), None)))(sg)
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(0, sg * 0.06 * math.sin(cyc(t, T)), 0), None)))(sg)
        f['Eye.L'] = lambda t: (rot(0.15 * math.sin(cyc(t, T) * 2), 0.1 * math.sin(cyc(t, T)), 0), None)
        f['Eye.R'] = lambda t: (rot(0.15 * math.sin(cyc(t, T) * 2 + 2), -0.1 * math.sin(cyc(t, T)), 0), None)
        return f
    clips['CrabArmature|Idle'] = (2.4, True, idle())
    def smooth(t, a, b):
        x = min(1, max(0, (t - a) / (b - a))); return x * x * (3 - 2 * x)
    def attack():   # wind-up (claws back + body rears), snap (lunge, pincers clap twice), recover
        T = 0.7
        f = {'Body': lambda t: (rot(-0.25 * smooth(t, 0, .2) + 0.55 * smooth(t, .2, .32) - 0.30 * smooth(t, .5, .7), 0, 0), (0, 0.05 * smooth(t, 0, .2), 0.35 * smooth(t, .2, .32) - 0.35 * smooth(t, .5, .7)))}
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(-0.5 * smooth(t, 0, .2) + 0.9 * smooth(t, .2, .32) - 0.4 * smooth(t, .5, .7), sg * (-0.25 * smooth(t, 0, .2) + 0.35 * smooth(t, .2, .32) - 0.1 * smooth(t, .5, .7)), 0), None)))(sg)
            f[f'PincerUp.{side}'] = lambda t: (rot(-0.9 * smooth(t, 0, .2) + 0.9 * smooth(t, .2, .3) - 0.7 * max(0, math.sin((t - .3) * 25)) * (1 if .3 < t < .5 else 0), 0, 0), None)
            f[f'PincerDown.{side}'] = lambda t: (rot(0.7 * smooth(t, 0, .2) - 0.7 * smooth(t, .2, .3) + 0.6 * max(0, math.sin((t - .3) * 25)) * (1 if .3 < t < .5 else 0), 0, 0), None)
        return f
    clips['CrabArmature|Attack'] = (0.7, False, attack())
    def eat():   # claws alternate to the mouth and munch
        T = 1.0; f = {'Body': lambda t: (rot(0.12 * math.sin(cyc(t, T) * 2), 0, 0), None)}
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(-0.45 * max(0, math.sin(cyc(t, T) + (0 if sg > 0 else math.pi))), -sg * 0.35 * max(0, math.sin(cyc(t, T) + (0 if sg > 0 else math.pi))), 0), None)))(sg)
            f[f'PincerUp.{side}'] = lambda t: (rot(-0.35 * max(0, math.sin(cyc(t, T) * 4)), 0, 0), None)
        return f
    clips['CrabArmature|Eat'] = (1.0, False, eat())
    def hit():
        f = {'Body': lambda t: (rot(-0.35 * math.sin(math.pi * min(1, t / 0.4)), 0, 0.15 * math.sin(math.pi * min(1, t / 0.4))), (0, 0, -0.25 * math.sin(math.pi * min(1, t / 0.4))))}
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(-0.7 * math.sin(math.pi * min(1, t / 0.4)), 0, 0), None)))(sg)
        return f
    clips['CrabArmature|Hit'] = (0.4, False, hit())
    def stun():
        T = 1.2; f = {'Body': lambda t: (rot(0.05, 0, 0.14 * math.sin(cyc(t, T) * 2)), None), 'Eye.L': lambda t: (rot(0, cyc(t, T) * 3, 0), None), 'Eye.R': lambda t: (rot(0, -cyc(t, T) * 3, 0), None)}
        for side in 'LR': f[f'Shoulder.{side}'] = lambda t: (rot(-0.3, 0, 0), None)
        return f
    clips['CrabArmature|Stun'] = (1.2, True, stun())
    def jump():
        T = 0.7
        f = {'Body': lambda t: (rot(0.15 * math.sin(math.pi * t / T) - 0.15 * smooth(t, 0, .15), 0, 0), (0, 1.1 * math.sin(math.pi * t / T) ** 1.2, 0))}
        for side, k, hip, knee, s, z in legs:
            f[rig.names[hip]] = (lambda s: (lambda t: (rot(0, 0, s * 0.5 * math.sin(math.pi * t / T)), None)))(s)
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(-0.9 * math.sin(math.pi * t / T), 0, 0), None)))(sg)
        return f
    clips['CrabArmature|Jump'] = (0.7, False, jump())
    def cheer():
        T = 1.0; f = {'Body': lambda t: (rot(0, 0.15 * math.sin(cyc(t, T)), 0), (0, 0.06 * abs(math.sin(cyc(t, T))), 0))}
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(-1.2 + 0.25 * math.sin(cyc(t, T) * 2 + (0 if sg > 0 else math.pi)), sg * -0.2, 0), None)))(sg)
            f[f'PincerUp.{side}'] = lambda t: (rot(-0.4 * max(0, math.sin(cyc(t, T) * 4)), 0, 0), None)
        return f
    clips['CrabArmature|Cheer'] = (1.0, True, cheer())
    def death():   # flip onto its back, legs curl up, claws flop
        T = 1.4; e = lambda t: smooth(t, 0.05, 0.8)
        f = {'Body': lambda t: (rot(0, 0, math.pi * e(t)), (0, 0.45 * e(t) - 0.12 * smooth(t, .8, 1.0) , 0))}
        for side, k, hip, knee, s, z in legs:
            f[rig.names[hip]] = (lambda s: (lambda t: (rot(0, 0, -s * 0.9 * smooth(t, .5, 1.1)), None)))(s)
            f[rig.names[knee]] = (lambda s: (lambda t: (rot(0, 0, -s * 0.6 * smooth(t, .5, 1.1)), None)))(s)
        for side in 'LR':
            sg = 1 if side == 'L' else -1
            f[f'Shoulder.{side}'] = (lambda sg: (lambda t: (rot(0.3 * smooth(t, .4, 1.0), 0, -sg * 0.5 * smooth(t, .4, 1.0)), None)))(sg)
        return f
    clips['CrabArmature|Death'] = (1.4, False, death())
    return rig, list(parts.values()), materials, clips

# ================================================================== SLINKYSNOOT (serpent)
def catmull(pts, n):
    pts = [np.array(p, float) for p in pts]; out = []
    P = [pts[0]] + pts + [pts[-1]]
    segs = len(pts) - 1
    for i in range(n):
        u = i / (n - 1) * segs; s = min(segs - 1, int(u)); t = u - s
        p0, p1, p2, p3 = P[s], P[s + 1], P[s + 2], P[s + 3]
        out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    return out

def build_serpent():
    rig = Rig(); parts = {}
    def part(mat):
        if mat not in parts: parts[mat] = Part(mat)
        return parts[mat]
    spine_ctrl = [(0, 0.09, -2.1), (0, 0.13, -1.5), (0, 0.19, -0.9), (0, 0.26, -0.3), (0, 0.40, 0.15), (0, 0.75, 0.42), (0, 1.25, 0.52), (0, 1.62, 0.48), (0, 1.80, 0.50)]
    NJ = 16
    sp = catmull(spine_ctrl, NJ)
    radius = lambda s: 0.05 + 0.17 * math.sin(min(1, s / 0.55) * math.pi / 2) if s < 0.55 else 0.22 - 0.05 * (s - 0.55) / 0.45
    root = rig.add('Root', -1, (0, 0, 0))
    BELLY = 6
    jid = {}
    jid[BELLY] = rig.add('Spine6', root, sp[BELLY])
    for i in range(BELLY + 1, NJ): jid[i] = rig.add(f'Spine{i}', jid[i - 1], sp[i])
    for i in range(BELLY - 1, -1, -1): jid[i] = rig.add(f'Spine{i}', jid[i + 1], sp[i])
    head_pos = sp[NJ - 1]
    head = jid[NJ - 1]
    jaw = rig.add('Jaw', head, head_pos + np.array([0, -0.05, 0.10]))
    tongue = rig.add('Tongue', jaw, head_pos + np.array([0, -0.06, 0.62]))
    main, sec, belly = 'Snoot_Main', 'Snoot_Secondary', 'Snoot_Belly'
    # tube along the spine with smooth skin weights (blend the two nearest joints)
    seg = 12; rings = 70
    dense = catmull(spine_ctrl, rings)
    P = []; N = []; I = []; J = []; W = []
    prev_u = None
    for r, c in enumerate(dense):
        s = r / (rings - 1)
        tan = (dense[min(rings - 1, r + 1)] - dense[max(0, r - 1)]); tan /= np.linalg.norm(tan)
        a, u, v = frame_from(tan)
        rad = radius(s) * (1.0)
        # joint blend: map the dense param to joint coordinate (spine samples are evenly spaced in control param)
        jf = s * (NJ - 1); j0 = int(math.floor(jf)); j1 = min(NJ - 1, j0 + 1); w1 = jf - j0
        w1 = w1 * w1 * (3 - 2 * w1)
        for i in range(seg + 1):
            ph = 2 * math.pi * i / seg
            d = u * math.cos(ph) + v * math.sin(ph)
            P.append(c + d * rad); N.append(d); J.append((jid[j0], jid[j1], 0, 0)); W.append((1 - w1, w1, 0, 0))
    for r in range(rings - 1):
        for i in range(seg):
            a_ = r * (seg + 1) + i; b_ = a_ + seg + 1
            I += [a_, a_ + 1, b_, a_ + 1, b_ + 1, b_]
    # ensure outward winding: test one triangle
    p0, p1, p2 = [np.array(P[k]) for k in I[0:3]]
    nrm = np.cross(p1 - p0, p2 - p0)
    if np.dot(nrm, np.array(N[I[0]])) < 0: I = [x for t in range(0, len(I), 3) for x in (I[t], I[t + 2], I[t + 1])]
    part(main).add(P, N, I, J, W)
    # belly stripe: a thinner tube slightly lower/forward, lighter colour (rigid blend reuse by offsetting radius on the lower half)
    P2 = []; N2 = []; I2 = []; J2 = []; W2 = []
    for r, c in enumerate(dense):
        if r > rings - 6: break
        s = r / (rings - 1)
        tan = (dense[min(rings - 1, r + 1)] - dense[max(0, r - 1)]); tan /= np.linalg.norm(tan)
        a, u, v = frame_from(tan)
        # "down" direction perpendicular to the tangent
        down = np.array([0, -1.0, 0]); down = down - a * np.dot(down, a); down /= np.linalg.norm(down)
        side = np.cross(a, down)
        rad = radius(s) * 1.012
        jf = s * (NJ - 1); j0 = int(math.floor(jf)); j1 = min(NJ - 1, j0 + 1); w1 = jf - j0; w1 = w1 * w1 * (3 - 2 * w1)
        for i in range(7):
            ph = -0.9 + 1.8 * i / 6   # arc of +-0.9 rad around "down"
            d = down * math.cos(ph) + side * math.sin(ph)
            P2.append(c + d * rad); N2.append(d); J2.append((jid[j0], jid[j1], 0, 0)); W2.append((1 - w1, w1, 0, 0))
    for r in range(len(P2) // 7 - 1):
        for i in range(6):
            a_ = r * 7 + i; b_ = a_ + 7; I2 += [a_, a_ + 1, b_, a_ + 1, b_ + 1, b_]
    p0, p1, p2 = [np.array(P2[k]) for k in I2[0:3]]
    if np.dot(np.cross(p1 - p0, p2 - p0), np.array(N2[I2[0]])) < 0: I2 = [x for t in range(0, len(I2), 3) for x in (I2[t], I2[t + 2], I2[t + 1])]
    part(belly).add(P2, N2, I2, J2, W2)
    # back spots (secondary): small flattened ellipsoids on top, skinned to the nearest joint
    for k in range(2, NJ - 3):
        c = sp[k] + np.array([0, radius(k / (NJ - 1)) * 0.92, 0]); rr = radius(k / (NJ - 1))
        G = ellipsoid(c, (rr * 0.55, rr * 0.22, rr * 0.5), 8, 5); Pg, Ng, Ig = G; Jg, Wg = rigid(jid[k], len(Pg)); part(sec).add(Pg, Ng, Ig, Jg, Wg)
    # head: rounded skull + long soft "snoot" (the nose), big eyes, nostrils; lower jaw + forked tongue
    h = head_pos
    def put(mat, geo, j):
        Pg, Ng, Ig = geo; Jg, Wg = rigid(j, len(Pg)); part(mat).add(Pg, Ng, Ig, Jg, Wg)
    put(main, ellipsoid(h + np.array([0, 0.03, 0.12]), (0.27, 0.24, 0.36)), head)
    put(main, ellipsoid(h + np.array([0, -0.02, 0.50]), (0.19, 0.16, 0.30)), head)            # snoot
    put(sec, ellipsoid(h + np.array([0, 0.02, 0.77]), (0.10, 0.09, 0.07), 8, 6), head)        # nose tip
    put('Snoot_Pupil', ellipsoid(h + np.array([0.06, 0.07, 0.83]), (0.025, 0.025, 0.02), 6, 4), head)
    put('Snoot_Pupil', ellipsoid(h + np.array([-0.06, 0.07, 0.83]), (0.025, 0.025, 0.02), 6, 4), head)
    for s in (1, -1):
        put('Snoot_Eye', ellipsoid(h + np.array([0.19 * s, 0.20, 0.22]), (0.115, 0.12, 0.115), 10, 7), head)
        put('Snoot_Pupil', ellipsoid(h + np.array([0.21 * s, 0.205, 0.32]), (0.06, 0.07, 0.04), 8, 6), head)
    put(belly, ellipsoid(h + np.array([0, -0.09, 0.40]), (0.15, 0.06, 0.38), 10, 6), jaw)       # lower jaw
    put('Snoot_Tongue', box(h + np.array([0, -0.06, 0.74]), (0.025, 0.008, 0.14)), tongue)
    put('Snoot_Tongue', box(h + np.array([0.04, -0.06, 0.93]), (0.012, 0.008, 0.07)), tongue)
    put('Snoot_Tongue', box(h + np.array([-0.04, -0.06, 0.93]), (0.012, 0.008, 0.07)), tongue)
    materials = {main: hexc('#ff9a1f'), sec: hexc('#9ad04a'), belly: hexc('#ffd8a0'), 'Snoot_Eye': hexc('#ffffff'), 'Snoot_Pupil': hexc('#151515'), 'Snoot_Tongue': hexc('#e0304a')}
    def cyc(t, T): return 2 * math.pi * t / T
    def smooth(t, a, b):
        x = min(1, max(0, (t - a) / (b - a))); return x * x * (3 - 2 * x)
    clips = {}
    def slither(T, amp, sway, rear):
        f = {}
        for i in range(NJ):
            if i == BELLY: continue
            dist = abs(i - BELLY)
            f[f'Spine{i}'] = (lambda i, dist: (lambda t: (rot(rear * (0.03 if i > BELLY else 0), amp * math.sin(cyc(t, T) - 0.55 * i), 0), None)))(i, dist)
        f['Spine6'] = lambda t: (rot(0, 0.05 * math.sin(cyc(t, T)), 0), (0, 0, 0))
        f['Jaw'] = lambda t: (rot(0.05 + 0.04 * math.sin(cyc(t, T) * 2), 0, 0), None)
        f['Tongue'] = lambda t: (rot(0.2 * max(0, math.sin(cyc(t, T) * 3)), 0, 0), None)
        return f
    clips['SerpentArmature|Walk'] = (1.1, True, slither(1.1, 0.20, 0.0, 1.0))
    clips['SerpentArmature|Run'] = (0.65, True, slither(0.65, 0.28, 0.0, 1.6))
    def idle():
        T = 3.0; f = {}
        for i in range(NJ):
            if i == BELLY: continue
            f[f'Spine{i}'] = (lambda i: (lambda t: (rot(0.012 * math.sin(cyc(t, T) + 0.3 * i), 0.07 * math.sin(cyc(t, T) - 0.45 * i), 0), None)))(i)
        f['Jaw'] = lambda t: (rot(0.06 + 0.05 * max(0, math.sin(cyc(t, T) * 2)), 0, 0), None)
        f['Tongue'] = lambda t: (rot(0.35 * max(0, math.sin(cyc(t, T) * 4)), 0, 0), None)
        return f
    clips['SerpentArmature|Idle'] = (3.0, True, idle())
    def attack():   # pull back, strike, snap, recover
        T = 0.75; f = {}
        for i in range(BELLY + 1, NJ):
            f[f'Spine{i}'] = (lambda i: (lambda t: (rot(-0.10 * smooth(t, 0, .25) + 0.22 * smooth(t, .25, .36) - 0.12 * smooth(t, .55, .75), 0, 0), None)))(i)
        f['Jaw'] = lambda t: (rot(0.9 * smooth(t, .2, .35) - 0.9 * smooth(t, .42, .5) , 0, 0), None)
        f['Tongue'] = lambda t: (rot(0.4 * smooth(t, 0, .2), 0, 0), None)
        return f
    clips['SerpentArmature|Attack'] = (0.75, False, attack())
    def eat():
        T = 1.0; f = {}
        for i in range(BELLY + 1, NJ): f[f'Spine{i}'] = (lambda i: (lambda t: (rot(0.05 * math.sin(cyc(t, T) * 2), 0, 0), None)))(i)
        f['Jaw'] = lambda t: (rot(0.55 * max(0, math.sin(cyc(t, T) * 3)), 0, 0), None)
        return f
    clips['SerpentArmature|Eat'] = (1.0, False, eat())
    def hit():
        f = {}
        for i in range(BELLY + 1, NJ): f[f'Spine{i}'] = (lambda i: (lambda t: (rot(-0.16 * math.sin(math.pi * min(1, t / 0.4)), 0.10 * math.sin(math.pi * min(1, t / 0.4)), 0), None)))(i)
        f['Jaw'] = lambda t: (rot(0.5 * math.sin(math.pi * min(1, t / 0.4)), 0, 0), None)
        return f
    clips['SerpentArmature|Hit'] = (0.4, False, hit())
    def stun():
        T = 1.2; f = {}
        for i in range(BELLY + 1, NJ): f[f'Spine{i}'] = (lambda i: (lambda t: (rot(0.04, 0.12 * math.sin(cyc(t, T) * 2 - 0.4 * i), 0.08 * math.sin(cyc(t, T) * 2)), None)))(i)
        f['Tongue'] = lambda t: (rot(0.3 * max(0, math.sin(cyc(t, T) * 6)), 0, 0), None)
        return f
    clips['SerpentArmature|Stun'] = (1.2, True, stun())
    def jump():
        T = 0.7; f = {'Spine6': lambda t: (None, (0, 1.0 * math.sin(math.pi * t / T) ** 1.2, 0))}
        for i in range(BELLY + 1, NJ): f[f'Spine{i}'] = (lambda i: (lambda t: (rot(-0.12 * math.sin(math.pi * t / T), 0, 0), None)))(i)
        for i in range(0, BELLY): f[f'Spine{i}'] = (lambda i: (lambda t: (rot(0.10 * math.sin(math.pi * t / T), 0, 0), None)))(i)
        return f
    clips['SerpentArmature|Jump'] = (0.7, False, jump())
    def cheer():
        T = 1.0; f = {}
        for i in range(BELLY + 1, NJ): f[f'Spine{i}'] = (lambda i: (lambda t: (rot(-0.06, 0.30 * math.sin(cyc(t, T) - 0.3 * i), 0), None)))(i)
        f['Jaw'] = lambda t: (rot(0.35 * max(0, math.sin(cyc(t, T) * 3)), 0, 0), None)
        f['Tongue'] = lambda t: (rot(0.3 * max(0, math.sin(cyc(t, T) * 6)), 0, 0), None)
        return f
    clips['SerpentArmature|Cheer'] = (1.0, True, cheer())
    def death():   # head drops, the body goes limp and rolls belly-up
        T = 1.4; e = lambda t: smooth(t, 0.05, 0.9)
        f = {'Spine6': lambda t: (rot(0, 0, math.pi * e(t)), (0, 0.30 * e(t) - 0.05 * smooth(t, .8, 1.0), 0))}
        for i in range(BELLY + 1, NJ): f[f'Spine{i}'] = (lambda i: (lambda t: (rot(0.14 * smooth(t, .05, .7), 0.10 * math.sin(i) * smooth(t, .2, .9), 0), None)))(i)
        f['Jaw'] = lambda t: (rot(0.5 * smooth(t, .3, .9), 0, 0), None)
        f['Tongue'] = lambda t: (rot(0.2 * smooth(t, .3, .9), 0, 0), None)
        return f
    clips['SerpentArmature|Death'] = (1.4, False, death())
    return rig, list(parts.values()), materials, clips

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    for name, build in (('Crab.glb', build_crab), ('Serpent.glb', build_serpent)):
        rig, parts, mats, clips = build()
        g = write_glb(os.path.join(OUT, name), rig, parts, mats, clips)
        tris = sum(len(p.I) for p in parts) // 3
        print(f'{name}: {len(rig.names)} joints, {tris} triangles, {len(clips)} clips, {os.path.getsize(os.path.join(OUT, name)) / 1024:.0f} KB')
