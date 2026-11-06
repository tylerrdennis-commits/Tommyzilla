#!/usr/bin/env python3
"""Adds a proper standing idle ("CharacterArmature|TZ_Idle") to the Quaternius monster rigs that ship with an idle clip which
leaves the arms in the bind pose (Dino, Yeti, Fish, Demon, Blue Demon). The game used to cover for that by playing the WALK
cycle in place whenever a monster stood still (it jogged on the spot). TZ_Idle = the pack's own Idle (breathing, head, tail)
with the arms set to the middle of the Walk cycle's arm swing (relaxed, hanging naturally) plus a slow sway.
Idempotent: re-running replaces the clip. Usage: python3 tools/gen_idle_clips.py   (then tools/gen_models_json.py)"""
import json, math, os, struct, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); PP = os.path.join(HERE, '..', 'assets', 'polypizza')
FILES = ['Dino', 'Yeti', 'Fish', 'Demon', 'BlueDemon']
ARMS = ['UpperArm.L', 'UpperArm.R', 'LowerArm.L', 'LowerArm.R']
NAME = 'CharacterArmature|TZ_Idle'

def read(path):
    b = open(path, 'rb').read(); jl = struct.unpack('<I', b[12:16])[0]
    js = json.loads(b[20:20 + jl]); o = 20 + jl; bl = struct.unpack('<I', b[o:o + 4])[0]
    return js, bytearray(b[o + 8:o + 8 + bl])

def write(path, js, bn):
    j = json.dumps(js, separators=(',', ':')).encode()
    while len(j) % 4: j += b' '
    while len(bn) % 4: bn.append(0)
    js['buffers'][0]['byteLength'] = len(bn)
    j = json.dumps(js, separators=(',', ':')).encode()
    while len(j) % 4: j += b' '
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(j) + 8 + len(bn)))
        f.write(struct.pack('<II', len(j), 0x4E4F534A)); f.write(j)
        f.write(struct.pack('<II', len(bn), 0x004E4942)); f.write(bn)

def acc_arr(js, bn, i):
    a = js['accessors'][i]; bv = js['bufferViews'][a['bufferView']]
    n = {'SCALAR': 1, 'VEC3': 3, 'VEC4': 4}[a['type']]
    return np.frombuffer(bytes(bn), dtype=np.float32, count=a['count'] * n, offset=bv.get('byteOffset', 0) + a.get('byteOffset', 0)).reshape(a['count'], n).copy()

def qmul(a, b):
    x1, y1, z1, w1 = a; x2, y2, z2, w2 = b
    return np.array([w1*x2 + x1*w2 + y1*z2 - z1*y2, w1*y2 - x1*z2 + y1*w2 + z1*x2, w1*z2 + x1*y2 - y1*x2 + z1*w2, w1*w2 - x1*x2 - y1*y2 - z1*z2])

def add_accessor(js, bn, arr, typ, minmax=False):
    while len(bn) % 4: bn.append(0)
    data = arr.astype(np.float32).tobytes()
    js['bufferViews'].append({'buffer': 0, 'byteOffset': len(bn), 'byteLength': len(data)}); bn.extend(data)
    a = {'bufferView': len(js['bufferViews']) - 1, 'componentType': 5126, 'count': len(arr), 'type': typ}
    if minmax: a['min'] = [float(arr.min())]; a['max'] = [float(arr.max())]
    js['accessors'].append(a); return len(js['accessors']) - 1

def build(name):
    path = os.path.join(PP, name + '.glb'); js, bn = read(path)
    js['animations'] = [a for a in js['animations'] if a['name'] != NAME]   # idempotent
    nodes = js['nodes']; idx = {n.get('name'): i for i, n in enumerate(nodes)}
    anim = {a['name']: a for a in js['animations']}
    idle = anim['CharacterArmature|Idle']; walk = anim['CharacterArmature|Walk']
    chan = lambda an, node: next((c for c in an['channels'] if c['target']['node'] == node and c['target']['path'] == 'rotation'), None)
    # time base: the pack's idle (any rotation channel)
    t_acc = next(idle['samplers'][c['sampler']]['input'] for c in idle['channels'])
    times = acc_arr(js, bn, t_acc)[:, 0]; dur = float(times[-1])
    new = {'name': NAME, 'samplers': [], 'channels': []}
    done = set()
    for c in idle['channels']:   # keep everything the pack's idle does (body breathing, head, tail ...) except the arms
        node = c['target']['node']
        if nodes[node].get('name') in ARMS and c['target']['path'] == 'rotation': continue
        s = idle['samplers'][c['sampler']]
        new['samplers'].append(dict(s)); new['channels'].append({'sampler': len(new['samplers']) - 1, 'target': dict(c['target'])})
    for k, an in enumerate(ARMS):
        node = idx[an]; wc = chan(walk, node)
        Q = acc_arr(js, bn, walk['samplers'][wc['sampler']]['output'])
        for i in range(1, len(Q)):
            if np.dot(Q[i], Q[0]) < 0: Q[i] = -Q[i]
        mean = Q.mean(axis=0); mean /= np.linalg.norm(mean)
        out = []
        for t in times:
            sway = 0.035 * math.sin(2 * math.pi * t / dur + (0 if an.endswith('L') else math.pi) + (0.6 if an.startswith('Lower') else 0))
            q = qmul(mean, (0, 0, math.sin(sway / 2), math.cos(sway / 2)))
            out.append(q / np.linalg.norm(q))
        oa = add_accessor(js, bn, np.array(out), 'VEC4')
        new['samplers'].append({'input': t_acc, 'output': oa, 'interpolation': 'LINEAR'})
        new['channels'].append({'sampler': len(new['samplers']) - 1, 'target': {'node': node, 'path': 'rotation'}})
    js['animations'].append(new)
    write(path, js, bn)
    return os.path.getsize(path)

if __name__ == '__main__':
    for f in FILES: print(f, build(f), 'bytes')
