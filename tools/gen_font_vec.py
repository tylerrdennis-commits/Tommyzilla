#!/usr/bin/env python3
"""Generates src/ui/font_vec.h: the UI text font as *vector* glyphs (printable ASCII), so text is smooth at any size.

Source outlines: Poppins (Indian Type Foundry, SIL Open Font License 1.1) -- Medium for body text, Bold for headings.
Each glyph is stored as simplified contour vertices + a triangulation of the filled area + per-vertex outward "miter" normals.
At runtime ui_draw.cpp insets the fill by half a pixel and adds a one-pixel alpha-faded fringe along every contour, which
gives anti-aliased edges without any texture, shader or renderer change (UI vertices already carry per-vertex colour).

Usage: python3 tools/gen_font_vec.py [Poppins-Medium.ttf] [Poppins-Bold.ttf]   (writes src/ui/font_vec.h)
"""
import sys, math, os
from fontTools.ttLib import TTFont
from fontTools.pens.basePen import BasePen

MED = sys.argv[1] if len(sys.argv) > 1 else '/usr/share/fonts/truetype/google-fonts/Poppins-Medium.ttf'
BOLD = sys.argv[2] if len(sys.argv) > 2 else '/usr/share/fonts/truetype/google-fonts/Poppins-Bold.ttf'
EPS = 0.0042      # contour simplification tolerance (em)
Q = 2048.0        # fixed-point scale for coordinates (1/2048 em)

class FlatPen(BasePen):
    def __init__(self, gs, upm):
        super().__init__(gs); self.upm = upm; self.contours = []; self.cur = None
    def _moveTo(self, p): self.cur = [self._n(p)]
    def _lineTo(self, p): self.cur.append(self._n(p))
    def _n(self, p): return (p[0] / self.upm, p[1] / self.upm)
    def _qCurveToOne(self, p1, p2):
        p0 = self.cur[-1]; p1 = self._n(p1); p2 = self._n(p2)
        n = 8
        for i in range(1, n + 1):
            t = i / n
            self.cur.append(((1-t)**2*p0[0] + 2*(1-t)*t*p1[0] + t*t*p2[0], (1-t)**2*p0[1] + 2*(1-t)*t*p1[1] + t*t*p2[1]))
    def _curveToOne(self, p1, p2, p3):
        p0 = self.cur[-1]; p1 = self._n(p1); p2 = self._n(p2); p3 = self._n(p3)
        n = 10
        for i in range(1, n + 1):
            t = i / n; a = (1-t)**3; b = 3*(1-t)**2*t; c = 3*(1-t)*t*t; d = t**3
            self.cur.append((a*p0[0] + b*p1[0] + c*p2[0] + d*p3[0], a*p0[1] + b*p1[1] + c*p2[1] + d*p3[1]))
    def _closePath(self):
        if self.cur and len(self.cur) > 2: self.contours.append(self.cur)
        self.cur = None
    _endPath = _closePath

def area(pts):
    return 0.5 * sum(pts[i][0]*pts[(i+1) % len(pts)][1] - pts[(i+1) % len(pts)][0]*pts[i][1] for i in range(len(pts)))

def dedupe(pts):
    out = []
    for p in pts:
        if not out or math.hypot(p[0]-out[-1][0], p[1]-out[-1][1]) > 1e-5: out.append(p)
    if len(out) > 1 and math.hypot(out[0][0]-out[-1][0], out[0][1]-out[-1][1]) < 1e-5: out.pop()
    return out

def dp(pts, eps):  # Douglas-Peucker on an open polyline
    if len(pts) < 3: return pts
    a, b = pts[0], pts[-1]; dx, dy = b[0]-a[0], b[1]-a[1]; L = math.hypot(dx, dy)
    best, bi = -1, 0
    for i in range(1, len(pts)-1):
        d = abs((pts[i][0]-a[0])*dy - (pts[i][1]-a[1])*dx) / L if L > 1e-9 else math.hypot(pts[i][0]-a[0], pts[i][1]-a[1])
        if d > best: best, bi = d, i
    if best <= eps: return [a, b]
    return dp(pts[:bi+1], eps)[:-1] + dp(pts[bi:], eps)

def simplify_closed(pts, eps):
    if len(pts) < 6: return pts
    # split at the two most distant points so the closed loop becomes two open runs
    i0 = 0; j = max(range(len(pts)), key=lambda k: math.hypot(pts[k][0]-pts[i0][0], pts[k][1]-pts[i0][1]))
    A = pts[i0:j+1]; B = pts[j:] + pts[:i0+1]
    return dp(A, eps)[:-1] + dp(B, eps)[:-1]

def pip(pt, poly):
    x, y = pt; ins = False
    for i in range(len(poly)):
        x1, y1 = poly[i]; x2, y2 = poly[(i+1) % len(poly)]
        if (y1 > y) != (y2 > y) and x < (x2-x1)*(y-y1)/(y2-y1) + x1: ins = not ins
    return ins

def tri_area2(a, b, c): return (b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])

def in_tri(p, a, b, c):
    d1 = tri_area2(p, a, b); d2 = tri_area2(p, b, c); d3 = tri_area2(p, c, a)
    return not ((d1 < -1e-12 or d2 < -1e-12 or d3 < -1e-12) and (d1 > 1e-12 or d2 > 1e-12 or d3 > 1e-12))

def bridge_holes(outer, holes):
    """outer CCW, holes CW: merge each hole into the outer ring through a bridge (vertex indices into one combined list)."""
    ring = list(outer)
    for hole in sorted(holes, key=lambda h: -max(p[0] for p in h)):
        mi = max(range(len(hole)), key=lambda k: hole[k][0]); M = hole[mi]
        # ray to +x: nearest crossed ring edge
        bestx, bi = 1e9, -1
        for i in range(len(ring)):
            a = ring[i]; b = ring[(i+1) % len(ring)]
            if (a[1] <= M[1] <= b[1] or b[1] <= M[1] <= a[1]) and a[1] != b[1]:
                x = a[0] + (M[1]-a[1]) * (b[0]-a[0]) / (b[1]-a[1])
                if x >= M[0] - 1e-9 and x < bestx: bestx, bi = x, i
        a = ring[bi]; b = ring[(bi+1) % len(ring)]
        P = bi if a[0] > b[0] else (bi+1) % len(ring)
        I = (bestx, M[1])
        # any reflex ring vertex inside triangle (M, I, P) blocks visibility: take the one with the smallest angle
        cand, bestang = P, 1e9
        for k in range(len(ring)):
            q = ring[k]
            if q is ring[P] or k == P: continue
            if in_tri(q, M, I, ring[P]) and q[0] >= M[0]:
                ang = abs(math.atan2(q[1]-M[1], q[0]-M[0]))
                if ang < bestang: cand, bestang = k, ang
        P = cand
        ring = ring[:P+1] + hole[mi:] + hole[:mi+1] + ring[P:]
    return ring

def ear_clip(ring):
    n = len(ring); idx = list(range(n)); tris = []
    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1; clipped = False
        for k in range(len(idx)):
            ia, ib, ic = idx[k-1], idx[k], idx[(k+1) % len(idx)]
            a, b, c = ring[ia], ring[ib], ring[ic]
            if tri_area2(a, b, c) <= 1e-12: continue   # reflex or degenerate
            ok = True
            for j in idx:
                if j in (ia, ib, ic): continue
                p = ring[j]
                if p == a or p == b or p == c: continue   # bridge duplicates
                if in_tri(p, a, b, c): ok = False; break
            if ok:
                tris.append((ia, ib, ic)); idx.pop(k); clipped = True; break
        if not clipped:   # nothing clips: drop the flattest vertex to keep going (rare, degenerate input)
            k = min(range(len(idx)), key=lambda k: abs(tri_area2(ring[idx[k-1]], ring[idx[k]], ring[idx[(k+1) % len(idx)]])))
            idx.pop(k)
    if len(idx) == 3: tris.append(tuple(idx))
    return tris

def build_glyph(gs, name, upm):
    pen = FlatPen(gs, upm); gs[name].draw(pen)
    cons = []
    for c in pen.contours:
        c = simplify_closed(dedupe(c), EPS)
        if len(c) >= 3 and abs(area(c)) > 1e-7: cons.append(c)
    outers = [c for c in cons if area(c) < 0]     # TrueType: outer contours are clockwise (y up)
    holes = [c for c in cons if area(c) > 0]
    if not outers and holes: outers, holes = holes, []
    outers = [list(reversed(c)) for c in outers]  # -> CCW
    holes = [list(reversed(c)) for c in holes]    # -> CW
    groups = [(o, []) for o in outers]
    for h in holes:
        owners = [g for g in groups if pip(h[0], g[0])]
        if owners: min(owners, key=lambda g: abs(area(g[0])))[1].append(h)
    V, N, T, C = [], [], [], []
    for o, hs in groups:
        base = len(V); loops = [o] + hs
        # per-contour vertices + outward miter normals (y up; right-hand side of travel direction is outside the fill)
        for loop in loops:
            L = len(loop); start = len(V)
            for i in range(L):
                p0, p1, p2 = loop[i-1], loop[i], loop[(i+1) % L]
                e0 = (p1[0]-p0[0], p1[1]-p0[1]); e1 = (p2[0]-p1[0], p2[1]-p1[1])
                l0 = math.hypot(*e0) or 1; l1 = math.hypot(*e1) or 1
                n0 = (e0[1]/l0, -e0[0]/l0); n1 = (e1[1]/l1, -e1[0]/l1)
                m = (n0[0]+n1[0], n0[1]+n1[1]); ml = math.hypot(*m)
                if ml < 1e-6: m = n0
                else:
                    m = (m[0]/ml, m[1]/ml); d = max(0.5, m[0]*n0[0] + m[1]*n0[1]); m = (m[0]/d, m[1]/d)
                V.append(p1); N.append(m)
            C.append(list(range(start, start + L)))
        # triangulate this outer + its holes
        ring_pts = bridge_holes(o, hs)
        tri = ear_clip(ring_pts)
        # map ring points back to vertex indices (by coordinate)
        lookup = {}
        for k in range(base, len(V)): lookup.setdefault(V[k], k)
        for a, b, c in tri: T.append((lookup[ring_pts[a]], lookup[ring_pts[b]], lookup[ring_pts[c]]))
    adv = gs[name].width / upm
    return dict(adv=adv, V=V, N=N, T=T, C=C)

def verify(font_path, glyphs):
    """Compare the triangulated fill against Pillow's raster of the same font (IoU) to catch triangulation bugs."""
    from PIL import Image, ImageDraw, ImageFont
    import numpy as np
    f = ImageFont.truetype(font_path, 256); worst = 1.0; bad = []
    for ch, g in glyphs.items():
        if ch == ' ' or not g['T']: continue
        W = H = 512; ox, oy = 128, 330       # baseline y = 330 px, em = 256 px
        ref = Image.new('L', (W, H), 0); ImageDraw.Draw(ref).text((ox, oy), ch, font=f, fill=255, anchor='ls')
        mine = Image.new('L', (W, H), 0); d = ImageDraw.Draw(mine)
        for a, b, c in g['T']:
            d.polygon([(ox + g['V'][i][0]*256, oy - g['V'][i][1]*256) for i in (a, b, c)], fill=255)
        r = np.array(ref) > 127; m = np.array(mine) > 127
        iou = (r & m).sum() / max(1, (r | m).sum()); worst = min(worst, iou)
        if iou < 0.965: bad.append((ch, round(float(iou), 3)))
    return worst, bad

def emit(font_path, tag):
    f = TTFont(font_path); gs = f.getGlyphSet(); cmap = f.getBestCmap(); upm = f['head'].unitsPerEm
    glyphs = {}
    for cp in range(32, 127):
        glyphs[chr(cp)] = build_glyph(gs, cmap[cp], upm)
    worst, bad = verify(font_path, glyphs)
    print(f'{tag}: {len(glyphs)} glyphs, verts={sum(len(g["V"]) for g in glyphs.values())}, tris={sum(len(g["T"]) for g in glyphs.values())}, worst IoU vs Pillow raster={worst:.3f}, below 0.965: {bad}')
    gl, V, N, T, C = [], [], [], [], []
    for cp in range(32, 127):
        g = glyphs[chr(cp)]
        gl.append((int(round(g['adv']*Q)), len(V)//2, len(g['V']), len(T)//3, len(g['T']), len(C), len(g['C'])))
        for p in g['V']: V += [int(round(p[0]*Q)), int(round(-p[1]*Q))]            # y down
        for m in g['N']: N += [max(-127, min(127, int(round(m[0]*60)))), max(-127, min(127, int(round(-m[1]*60))))]
        for a, b, c in g['T']: T += [a, b, c]
        for loop in g['C']: C += [len(loop)] + loop
    out = f'// {tag}\n'
    out += f'inline const VecGlyph VF_{tag}_G[95] = {{' + ','.join('{%d,%d,%d,%d,%d,%d,%d}' % t for t in gl) + '};\n'
    out += f'inline const int16_t VF_{tag}_V[] = {{' + ','.join(map(str, V)) + '};\n'
    out += f'inline const int8_t VF_{tag}_N[] = {{' + ','.join(map(str, N)) + '};\n'
    out += f'inline const uint16_t VF_{tag}_T[] = {{' + ','.join(map(str, T)) + '};\n'
    out += f'inline const uint16_t VF_{tag}_C[] = {{' + ','.join(map(str, C)) + '};\n'
    return out, f, upm

hdr = '''// GENERATED by tools/gen_font_vec.py -- do not edit. UI text font as vector glyphs (see the script for the format).
// Glyph outlines derived from Poppins (c) Indian Type Foundry, licensed under the SIL Open Font License 1.1
// (https://openfontlicense.org) -- see assets/fonts/POPPINS_LICENSE.txt.
#pragma once
#include <cstdint>
namespace tz {
// adv: advance (1/2048 em); v0/nv: first vertex index (x,y pairs) and count; t0/nt: first triangle and count; c0/nc: first contour (len, idx...) and contour count
struct VecGlyph { uint16_t adv, v0, nv, t0, nt, c0, nc; };
inline constexpr float VF_Q = 2048.f;           // fixed-point scale of coordinates
inline constexpr float VF_NORM = 60.f;          // fixed-point scale of the miter normals
inline constexpr float VF_BASELINE = 0.80f;     // baseline position inside an em-sized line box (fraction of em from the top)
inline constexpr float VF_CAP_M = 0.697f, VF_CAP_B = 0.697f;  // cap height (em)
'''
body = ''; 
o1, f1, upm1 = emit(MED, 'M'); o2, f2, upm2 = emit(BOLD, 'B')
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'ui', 'font_vec.h'), 'w').write(hdr + o1 + o2 + '}  // namespace tz\n')
print('wrote src/ui/font_vec.h')
