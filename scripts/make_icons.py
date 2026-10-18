#!/usr/bin/env python3
"""The apps' icons: a coloured rounded square, a white symbol. Pure Python (no imaging library):
shapes are sampled 3×3 per pixel and written as RGBA PNGs to assets/icons/<app>.png.
Run again after changing a shape (`make_icons.py [app…]`: only those); the PNGs are committed."""
import math, os, struct, zlib

SIZE = 512
SS = 3  # samples per axis per pixel

def png_bytes(pixels, size=SIZE):
    raw = b''.join(b'\x00' + bytes(row) for row in pixels)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def png(path, pixels):
    with open(path, 'wb') as f:
        f.write(png_bytes(pixels))

# Shapes in unit coordinates (0..1, y down): functions (x, y) -> inside?
def rounded(x0, y0, x1, y1, r):
    def inside(x, y):
        if not (x0 <= x <= x1 and y0 <= y <= y1):
            return False
        cx = min(max(x, x0 + r), x1 - r)
        cy = min(max(y, y0 + r), y1 - r)
        return (x - cx) ** 2 + (y - cy) ** 2 <= r * r
    return inside

def circle(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r

def ring(cx, cy, r, w):
    return lambda x, y: (r - w) ** 2 <= (x - cx) ** 2 + (y - cy) ** 2 <= r * r

def polygon(points):
    def inside(x, y):
        c = False
        for (ax, ay), (bx, by) in zip(points, points[1:] + points[:1]):
            if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
                c = not c
        return c
    return inside

def stroke(points, w):
    def inside(x, y):
        for (ax, ay), (bx, by) in zip(points, points[1:]):
            dx, dy = bx - ax, by - ay
            t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
            if (x - ax - t * dx) ** 2 + (y - ay - t * dy) ** 2 <= (w / 2) ** 2:
                return True
        return False
    return inside

def union(*shapes):
    return lambda x, y: any(s(x, y) for s in shapes)

def minus(a, b):
    return lambda x, y: a(x, y) and not b(x, y)

def star(cx, cy, r1, r2, n):
    pts = []
    for i in range(2 * n):
        r = r1 if i % 2 == 0 else r2
        a = -math.pi / 2 + i * math.pi / n
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return polygon(pts)

ICONS = {
    # Signer: a check mark — signing.
    'signer': ((37, 99, 235), stroke([(0.29, 0.52), (0.44, 0.67), (0.72, 0.35)], 0.09)),
    # Reserve: stacked coins — the treasury.
    'reserve': ((4, 120, 87), union(rounded(0.28, 0.58, 0.72, 0.70, 0.06), rounded(0.28, 0.44, 0.72, 0.56, 0.06),
                                    rounded(0.28, 0.30, 0.72, 0.42, 0.06))),
    # Admin: a shield — the chain's authority.
    'admin': ((109, 40, 217), polygon([(0.5, 0.24), (0.74, 0.32), (0.72, 0.52), (0.5, 0.78), (0.28, 0.52),
                                       (0.26, 0.32)])),
    # Registrar: a person and a plus — new accounts.
    'registrar': ((14, 116, 144), union(circle(0.40, 0.38, 0.10), rounded(0.23, 0.52, 0.57, 0.74, 0.10),
                                        rounded(0.70, 0.40, 0.79, 0.68, 0.02), rounded(0.61, 0.49, 0.88, 0.58, 0.02))),
    # Genesis: a star — the start.
    'genesis': ((180, 83, 9), star(0.5, 0.52, 0.27, 0.11, 5)),
    # Fees: one split three ways — fees handed on.
    'fees': ((21, 128, 61), union(circle(0.5, 0.31, 0.10), circle(0.27, 0.70, 0.09), circle(0.5, 0.72, 0.09),
                                  circle(0.73, 0.70, 0.09), stroke([(0.5, 0.36), (0.27, 0.66)], 0.05),
                                  stroke([(0.5, 0.36), (0.5, 0.66)], 0.05), stroke([(0.5, 0.36), (0.73, 0.66)], 0.05))),
    # Wallet: a wallet and its clasp — your own accounts.
    'wallet': ((15, 118, 110), union(minus(rounded(0.20, 0.30, 0.80, 0.74, 0.08), rounded(0.54, 0.43, 0.84, 0.61, 0.05)),
                                     circle(0.63, 0.52, 0.035))),
    # Token: a coin with a T — an issuer's token.
    'token': ((190, 18, 60), union(ring(0.5, 0.5, 0.27, 0.06), rounded(0.37, 0.36, 0.63, 0.44, 0.02),
                                   rounded(0.46, 0.36, 0.54, 0.66, 0.02))),
}

def render(colour, glyph, size=SIZE):
    tile = rounded(0.06, 0.06, 0.94, 0.94, 0.2)
    rows = []
    for py in range(size):
        row = []
        for px in range(size):
            inTile = inGlyph = 0
            for sy in range(SS):
                for sx in range(SS):
                    x = (px + (sx + 0.5) / SS) / size
                    y = (py + (sy + 0.5) / SS) / size
                    if tile(x, y):
                        inTile += 1
                        if glyph(x, y):
                            inGlyph += 1
            n = SS * SS
            if inTile == 0:
                row += [0, 0, 0, 0]
                continue
            g = inGlyph / inTile
            r, gr, b = (round(c * (1 - g) + 255 * g) for c in colour)
            row += [r, gr, b, round(255 * inTile / n)]
        rows.append(row)
    return rows

if __name__ == '__main__':
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'icons')
    os.makedirs(out, exist_ok=True)
    import sys
    for name, (colour, glyph) in ICONS.items():
        if len(sys.argv) > 1 and name not in sys.argv[1:]:
            continue
        png(os.path.join(out, name + '.png'), render(colour, glyph))
        print('wrote', name + '.png')
