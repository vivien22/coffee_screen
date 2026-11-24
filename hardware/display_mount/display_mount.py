"""Display mount for the GC9A01 round screen (ZJY128S0800TG01 board) in the front panel of the machine.

The panel has a 40.3 mm hole with a small notch at the bottom. The mount is a sandwich of two printed parts:

  A (front)  collar: a thin flange seen from the front, a lip that hides the edge of the glass, a ring that goes
             through the hole, and a key that fills the notch of the panel (so A cannot turn).
  B (back)   plate: pressed against the back of the panel, four pegs go into the four holes of the PCB.

The panel is clamped between the flange of A and the plate B; the ring of A fits into the bore of B.

Frame of the model, seen from the front: x to the right, y up, z towards the back (z = 0 is the front face of A).
All sizes in mm. The defaults below are the measured values of the machine and the manufacturer's drawing.

Usage:
    python3 display_mount.py                     one pair A + B (STL files) with the defaults
    python3 display_mount.py plate=2.3 fit=0.1   same, with some values changed (any key of PARAMS)
    python3 display_mount.py --plate             the whole print plate (3MF): A, B and its fitting variants,
                                                 and the temporary pair for the pin header

Needs numpy and scikit-image (marching cubes for the plate B).
"""
import struct
import sys
import zipfile
from pathlib import Path

import numpy as np
from skimage.measure import marching_cubes


PARAMS = dict(
    # The panel (measured on the machine)
    T=1.0,              # thickness of the steel panel
    D_hole=40.3,        # diameter of the hole (not used by the geometry, kept for reference)
    key_w=1.8,          # key of A in the notch of the panel (notch measured about 2.1 wide, 1.8 below the hole)
    key_d=1.6,          # how far the key goes past r = 20

    # The screen (manufacturer's drawing)
    D_glass=36.2,       # pocket of the glass (backlight 35.6)
    g=1.5,              # front of the glass -> front of the PCB
    pcb=1.2,            # thickness of the PCB
    hole_sq=30.0,       # the 4 holes of the PCB are on a 30 x 30 square, centred on the screen, diameter 2.0
    pin_y=21.2,         # row of header pins, above the centre of the screen
    header_half=10.5,   # half width of the room left for the 8 pins (2.54 pitch: +/- 8.9)

    # Part A (collar)
    C=0.8,              # thickness of the flange
    D_col=46.0,         # diameter of the flange (hides the notch of the panel)
    D_plug=39.8,        # diameter of the ring that goes through the hole
    D_win=33.0,         # window (active area 32.4)
    chamfer=0.4,        # 45 degree chamfer on the outer edge and on the window

    # Part B (plate)
    plate=1.0,          # thickness of B = how far the PCB sits behind the panel
    D_plate=48.0,       # outer diameter of B
    fit=0.05,           # radial clearance between the ring of A and the bore of B
    D_peg=1.8,          # pegs in the PCB holes (2.0)
    peg_out=1.2,        # length of the pegs behind the PCB (to glue or melt them)
    pin_slot=0,         # 1: blind slot for the tips of the header pins (temporary mount with the pin header)
    pin_depth=1.05,     # depth of that slot; what is left insulates the pins from the steel
    slot_half=1.15,     # half height of that slot
    pips=0,             # number of small dots that tell the variants apart (on the PCB side, outside the PCB)
    pip_side=0,         # where the dots are: 0 = bottom, 1 = left
    res=0.1,            # mesh step of B

    # Added in v3 (0 = off)
    tab_half=0.0,       # half width of the room for the bottom tab of the glass (0 = no room)
    tab_y=-15.0,        # that room starts this far below the centre
    key_ab=0,           # 1: tooth of B (top) in a slot of the ring of A, so B only fits one way
    kab_w=2.0,          # width of that tooth
    kab_in=19.0,        # inner radius of that tooth
)


def angle_distance(a, b):
    """Distance between two angles in degrees, in 0..180."""
    d = (a - b) % 360
    return np.minimum(d, 360 - d)


def build(p):
    """Return the triangles of A and of B (both in the frame of the machine, z = 0 at the front of A)."""
    T, C, Pl = p['T'], p['C'], p['plate']
    Rc, Rp, Rw, Rg = p['D_col'] / 2, p['D_plug'] / 2, p['D_win'] / 2, p['D_glass'] / 2
    ch = p['chamfer']

    zb = C + T                          # back of the panel = front of B
    z_end = zb + Pl                     # the PCB rests here
    zp = max(0.6, z_end - p['g'])       # front of the glass (the lip of A ends here)

    h = p['hole_sq'] / 2
    pegs = [(sx * h, sy * h) for sx in (-1, 1) for sy in (-1, 1)]

    triA = _part_a(p, Rc, Rp, Rw, Rg, ch, C, zb, z_end, zp)
    triB = _part_b(p, Rp, zb, z_end, pegs)
    return triA, triB, dict(zb=zb, z_end=z_end, zp=zp)


def _part_a(p, Rc, Rp, Rw, Rg, ch, C, zb, z_end, zp):
    """Part A on an exact cylindrical grid (r, theta, z): every face lies on a grid line, the mesh is watertight."""
    Rk = 20.0 + p['key_d']
    kw = np.degrees(p['key_w'] / 2 / 20.0)

    # Grid lines: every size of the part, plus small steps for the chamfers
    zs = {0, zp, C, zb, z_end} | set(np.round(np.linspace(0, ch, 5), 4))
    zs = np.unique(np.round(np.array(sorted(zs)), 4))
    zm = (zs[:-1] + zs[1:]) / 2

    rs = {Rw, Rw + ch, Rg, Rp, Rk, Rc - ch, Rc, p['kab_in'] - 0.1}
    rs |= set(np.round(Rw + ch - np.clip(zs, 0, ch), 4)) | set(np.round(Rc - ch + np.clip(zs, 0, ch), 4))
    rs = np.unique(np.round(np.array(sorted(r for r in rs if Rw - 1e-9 <= r <= Rc + 1e-9)), 4))
    rm = (rs[:-1] + rs[1:]) / 2

    ts = set(np.arange(0, 360, 1.0)) | {270 - kw, 270 + kw}
    for rr in (Rg, Rp):
        a = np.degrees(np.arccos(min(1, p['tab_half'] / rr)))
        ts |= {180 + a, 360 - a}
    slot_half = np.degrees((p['kab_w'] / 2 + 0.2) / 19.4)
    ts |= {90 - slot_half, 90 + slot_half}
    ts = np.unique(np.round(np.array(sorted(t % 360 for t in ts)), 5))
    nt = len(ts)
    tm = (ts + np.r_[ts[1:], ts[0] + 360]) / 2 % 360

    # Solid cells, from the centres of the cells
    R, TH, Z = np.meshgrid(rm, tm, zm, indexing='ij')

    collar = (Z < C) & (R > Rw + ch - np.clip(Z, 0, ch)) & (R < Rc - ch + np.clip(Z, 0, ch))
    ring = (Z >= C) & (Z < z_end) & (R < Rp)
    key = (angle_distance(TH, 270) < kw) & (Z >= C) & (Z < zb) & (R < Rk)
    pocket = (Z > zp) & (R < Rg)

    # Behind the panel the ring of A goes into B: room for the bottom tab of the glass, slot of the A/B key
    behind = Z > zb
    X, Y = R * np.cos(np.radians(TH)), R * np.sin(np.radians(TH))
    tab = (np.abs(X) < p['tab_half']) & (Y < p['tab_y'])
    slot = (angle_distance(TH, 90) < slot_half) & (R > p['kab_in'] - 0.1) if p['key_ab'] else np.zeros_like(R, bool)

    solid = (collar | ring | key) & ~pocket & ~(behind & (tab | slot)) & (R > Rw)

    # Faces between a solid cell and an empty one, oriented outwards
    nr = len(rm)

    def corner(i, j, k):
        t = np.radians(ts[j % nt])
        return np.stack([rs[i] * np.cos(t), rs[i] * np.sin(t), zs[k]], -1)

    tris = []

    def add(quad, flip):
        a, b, c, d = quad
        b, d = np.where(flip[:, None], d, b), np.where(flip[:, None], b, d)
        tris.extend([np.stack([a, b, c], 1), np.stack([a, c, d], 1)])

    S = np.concatenate([np.zeros((1, nt, len(zm)), bool), solid, np.zeros((1, nt, len(zm)), bool)], 0)
    i, j, k = np.nonzero(S[1:] != S[:-1])
    add([corner(i, j, k), corner(i, j + 1, k), corner(i, j + 1, k + 1), corner(i, j, k + 1)], ~S[i, j, k])

    i, j, k = np.nonzero(solid != np.roll(solid, -1, 1))
    add([corner(i, j + 1, k), corner(i, j + 1, k + 1), corner(i + 1, j + 1, k + 1), corner(i + 1, j + 1, k)],
        ~solid[i, j, k])

    S = np.concatenate([np.zeros((nr, nt, 1), bool), solid, np.zeros((nr, nt, 1), bool)], 2)
    i, j, k = np.nonzero(S[:, :, 1:] != S[:, :, :-1])
    add([corner(i, j, k), corner(i + 1, j, k), corner(i + 1, j + 1, k), corner(i, j + 1, k)], ~S[i, j, k])

    return _outwards(np.concatenate(tris))


def _part_b(p, Rp, zb, z_end, pegs):
    """Part B from a distance field and marching cubes (the pegs are not centred on the axis)."""
    res = p['res']
    Ro = p['D_plate'] / 2
    Rb = Rp + p['fit']                      # bore that takes the ring of A
    z0, z1 = zb, z_end
    zt = z_end + p['pcb'] + p['peg_out']    # tip of the pegs

    xs = np.arange(-Ro - 1, Ro + 1, res)
    zs = np.arange(z0 - 0.5, zt + 0.5, res / 1.5)
    X, Y = np.meshgrid(xs, xs, indexing='ij')
    Rr = np.hypot(X, Y)

    # In the plane: the ring, open at the bottom for the tab of the glass, with the tooth of the A/B key
    plate2d = np.maximum(Rr - Ro, Rb - Rr)
    tab_zone = np.maximum(np.abs(X) - p['tab_half'], Y - p['tab_y'])
    plate2d = np.maximum(plate2d, -tab_zone)
    if p['key_ab']:
        tooth = np.maximum.reduce([p['kab_in'] - Rr, Rr - (Rb + 0.2), np.abs(X) - p['kab_w'] / 2, -Y])
        plate2d = np.minimum(plate2d, tooth)

    pin_slot = np.maximum(np.abs(X) - p['header_half'], np.abs(Y - p['pin_y']) - p['slot_half'])
    d_peg = np.min([np.hypot(X - px, Y - py) for px, py in pegs], 0)

    n_pips = int(p['pips'])
    if n_pips:
        offsets = (np.arange(n_pips) - (n_pips - 1) / 2) * 2.2
        if p['pip_side']:
            d_pip = np.min([np.hypot(X + 22.9, Y - o) for o in offsets], 0) - 0.6      # left, outside the PCB
        else:
            d_pip = np.min([np.hypot(X - o, Y + 22.9) for o in offsets], 0) - 0.6      # bottom, below the PCB

    F = np.empty((len(xs), len(xs), len(zs)), np.float32)
    for k, z in enumerate(zs):
        f = np.maximum(plate2d, max(z0 - z, z - z1))

        # Blind slot open on the PCB side: the tips of the pins go in, the steel stays covered
        if p['pin_slot'] and z > z1 - p['pin_depth']:
            f = np.maximum(f, -pin_slot)

        # Pegs, with a small chamfer at the tip
        r_peg = p['D_peg'] / 2 - max(0, z - (zt - 0.3))
        f = np.minimum(f, np.maximum(d_peg - r_peg, max(z0 - z, z - zt)))

        if n_pips:
            f = np.minimum(f, np.maximum(d_pip, max(z0 - z, z - (z1 + 0.5))))

        F[:, :, k] = f

    v, faces, _, _ = marching_cubes(F, 0.0, spacing=(res, res, res / 1.5), allow_degenerate=False)
    v += np.array([xs[0], xs[0], zs[0]])
    return _outwards(v[faces])


def _outwards(tri):
    """Make the normals point outwards (positive volume)."""
    volume = np.sum(np.einsum('ij,ij->i', tri[:, 0], np.cross(tri[:, 1], tri[:, 2]))) / 6
    return tri if volume > 0 else tri[:, [0, 2, 1]]


def on_bed(tri):
    """Put a part on the print bed (lowest point at z = 0). A prints front face down, B panel side down."""
    tri = tri.copy()
    tri[..., 2] -= tri[..., 2].min()
    return tri


def write_stl(path, tri):
    normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True) + 1e-12
    data = np.zeros(len(tri), dtype=[('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')])
    data['n'] = normals
    data['v'] = tri
    with open(path, 'wb') as f:
        f.write(b'coffee_screen display mount'.ljust(80))
        f.write(struct.pack('<I', len(tri)))
        f.write(data.tobytes())


def write_3mf(path, objects):
    """objects: list of (name, triangles, [(x, y), ...] positions on the bed)."""
    xml_objects, items = [], []
    for oid, (name, tri, positions) in enumerate(objects, 1):
        vertices, index = np.unique(np.round(tri.reshape(-1, 3), 5), axis=0, return_inverse=True)
        faces = index.reshape(-1, 3)
        vs = '\n'.join(f'<vertex x="{a:.4f}" y="{b:.4f}" z="{c:.4f}"/>' for a, b, c in vertices)
        fs = '\n'.join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in faces)
        xml_objects.append(f'<object id="{oid}" name="{name}" type="model"><mesh><vertices>\n{vs}\n'
                           f'</vertices><triangles>\n{fs}\n</triangles></mesh></object>')
        for x, y in positions:
            items.append(f'<item objectid="{oid}" transform="1 0 0 0 1 0 0 0 1 {x} {y} 0" printable="1"/>')

    model = ('<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xml:lang="en-US" '
             'xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             '<metadata name="Title">coffee_screen display mount</metadata>'
             '<resources>' + ''.join(xml_objects) + '</resources><build>' + ''.join(items) + '</build></model>')
    content_types = ('<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/'
                     'content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.'
                     'relationships+xml"/><Default Extension="model" ContentType="application/vnd.ms-package.'
                     '3dmanufacturing-3dmodel+xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/'
            'relationships"><Relationship Target="/3D/3dmodel.model" Id="rel0" '
            'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
        z.writestr('[Content_Types].xml', content_types)
        z.writestr('_rels/.rels', rels)
        z.writestr('3D/3dmodel.model', model)


# The print plate: the final pair, four B variants to find the best fit (told apart by their dots),
# and the temporary pair for the pin header (until the FFC cable is used).
PLATE = [
    # name, changes to PARAMS, which part(s), copies
    ('collar_A',          {},                                         'A', 2),
    ('plate_B1_ref',      dict(pips=1),                               'B', 1),
    ('plate_B2_fit0.00',  dict(pips=2, fit=0.00),                     'B', 1),
    ('plate_B3_fit0.10',  dict(pips=3, fit=0.10),                     'B', 1),
    ('plate_B4_peg1.7',   dict(pips=4, D_peg=1.7),                    'B', 1),
    ('plate_B5_peg1.9',   dict(pips=5, D_peg=1.9),                    'B', 1),
    ('collar_A_pins',     dict(plate=2.3),                            'A', 1),
    ('plate_B6_pins',     dict(plate=2.3, pin_slot=1, pin_depth=2.05, slot_half=0.8, pips=6), 'B', 1),
]


def make_plate(base, path):
    slots = [(x, y) for y in (188, 128, 68) for x in (68, 128, 188)]   # 3 x 3 on the 256 mm bed
    objects = []
    for name, changes, part, copies in PLATE:
        p = dict(base, **changes)
        p['res'] = 0.14
        triA, triB, _ = build(p)
        tri = on_bed(triA if part == 'A' else triB)
        objects.append((name.replace('.', '_'), tri, [slots.pop(0) for _ in range(copies)]))
        print(name, len(tri), 'triangles')
    write_3mf(path, objects)


if __name__ == '__main__':
    p = dict(PARAMS)
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    for a in args:
        key, value = a.split('=')
        p[key] = float(value)

    here = Path(__file__).parent
    if '--plate' in sys.argv:
        make_plate(p, here / 'display_mount_plate.3mf')
    else:
        triA, triB, z = build(p)
        write_stl(here / 'collar_A.stl', on_bed(triA))
        write_stl(here / 'plate_B.stl', on_bed(triB))
        print(f"glass {z['zp']:.2f} mm behind the front, PCB {z['z_end']:.2f} mm behind the front")
