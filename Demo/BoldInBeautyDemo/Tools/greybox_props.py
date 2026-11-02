"""Composite greybox props for the detail passes (greybox_detail.py): each prop is several basic shapes so the greybox reads
as a real place (a car has a body, cabin, wheels and lights; a desk set has a desk, chair, monitor and keyboard). Every prop
takes a GreyboxMap `m`, a position, a yaw and a folder; sizes are centimetres. Props never take collision off unless noted:
the player can climb onto crates, cars, tanks and stalls."""

import math


def _off(x, y, yaw, dx, dy):
    a = math.radians(yaw)
    return x + dx * math.cos(a) - dy * math.sin(a), y + dx * math.sin(a) + dy * math.cos(a)


def car(m, x, y, z, yaw, folder, mat="Metal", wrecked=False, rng=None):
    tilt = rng.uniform(-12, 12) if (wrecked and rng) else 0.0
    m.block((x, y, z + 70), (440, 180, 80), "Car_Body", folder, mat, yaw=yaw, roll=tilt, tags=["Car", "JumpUp"])
    cx, cy = _off(x, y, yaw, -30, 0)
    if not (wrecked and rng and rng.random() < 0.3):   # some wrecks lost the cabin
        m.block((cx, cy, z + 145), (230, 165, 70), "Car_Cabin", folder, "Glass" if not wrecked else "Dark", yaw=yaw, roll=tilt)
    for dx in (-140, 140):
        for dy in (-90, 90):
            if wrecked and rng and rng.random() < 0.25:
                continue
            wx, wy = _off(x, y, yaw, dx, dy)
            m.block((wx, wy, z + 33), (66, 66, 22), "Car_Wheel", folder, "Dark", yaw=yaw + 90, pitch=90, shape="cylinder",
                    collision=False)
    for dy in (-60, 60):
        lx, ly = _off(x, y, yaw, 222, dy)
        m.block((lx, ly, z + 80), (6, 30, 14), "Car_Light", folder, "Lamp" if not wrecked else "Dark", yaw=yaw, collision=False)


def truck(m, x, y, z, yaw, folder, mat="NeonRed", wrecked=False, rng=None):
    tilt = rng.uniform(-10, 10) if (wrecked and rng) else 0.0
    m.block((x, y, z + 160), (800, 250, 260), "Truck_Box", folder, mat, yaw=yaw, roll=tilt, tags=["Truck", "JumpUp"])
    cx, cy = _off(x, y, yaw, 520, 0)
    m.block((cx, cy, z + 150), (240, 240, 240), "Truck_Cab", folder, mat, yaw=yaw, roll=tilt)
    for dx in (-300, 0, 520):
        for dy in (-125, 125):
            wx, wy = _off(x, y, yaw, dx, dy)
            m.block((wx, wy, z + 50), (100, 100, 40), "Truck_Wheel", folder, "Dark", yaw=yaw + 90, pitch=90, shape="cylinder",
                    collision=False)


def lamp(m, x, y, z, yaw, folder, h=600.0, head="Lamp", broken=False, rng=None):
    pitch = rng.uniform(20, 80) if (broken and rng) else 0.0
    m.block((x, y, z + h / 2), (20, 20, h), "Lamp_Pole", folder, "Metal", shape="cylinder", pitch=pitch, yaw=yaw)
    if pitch:
        return
    ax, ay = _off(x, y, yaw, 70, 0)
    m.block((ax, ay, z + h), (160, 12, 12), "Lamp_Arm", folder, "Metal", yaw=yaw, collision=False)
    hx, hy = _off(x, y, yaw, 140, 0)
    m.block((hx, hy, z + h - 15), (60, 40, 20), "Lamp_Head", folder, head, yaw=yaw, collision=False)


def bench(m, x, y, z, yaw, folder, mat="Wood"):
    m.block((x, y, z + 45), (180, 50, 8), "Bench_Seat", folder, mat, yaw=yaw, tags=["JumpUp"])
    bx, by = _off(x, y, yaw, 0, -25)
    m.block((bx, by, z + 80), (180, 6, 50), "Bench_Back", folder, mat, yaw=yaw)
    for dx in (-80, 80):
        lx, ly = _off(x, y, yaw, dx, 0)
        m.block((lx, ly, z + 22), (8, 50, 44), "Bench_Leg", folder, "Metal", yaw=yaw)


def bin_(m, x, y, z, folder):
    m.block((x, y, z + 45), (60, 60, 90), "Bin", folder, "Dark", shape="cylinder")
    m.block((x, y, z + 93), (66, 66, 6), "Bin_Lid", folder, "Metal", shape="cylinder")


def crate(m, x, y, z, yaw, folder, s=100.0, mat="Wood"):
    m.block((x, y, z + s / 2), (s, s, s), "Crate", folder, mat, yaw=yaw, tags=["Crate", "JumpUp"])


def crate_stack(m, x, y, z, yaw, folder, rng, mat="Wood"):
    n = rng.randint(2, 5)
    for k in range(n):
        dx, dy = rng.uniform(-60, 60), rng.uniform(-60, 60)
        level = k // 2
        cx, cy = _off(x, y, yaw, dx if level == 0 else dx * 0.3, dy if level == 0 else dy * 0.3)
        crate(m, cx, cy, z + level * 100, yaw + rng.uniform(-10, 10), folder, mat=mat)


def barrel(m, x, y, z, folder, mat="Hazard", fallen=False, yaw=0.0):
    if fallen:
        m.block((x, y, z + 30), (60, 60, 90), "Barrel", folder, mat, shape="cylinder", pitch=90, yaw=yaw)
        return
    m.block((x, y, z + 45), (60, 60, 90), "Barrel", folder, mat, shape="cylinder", tags=["JumpUp"])
    m.block((x, y, z + 60), (64, 64, 6), "Barrel_Ring", folder, "Dark", shape="cylinder", collision=False)


def tree(m, x, y, z, folder, rng, h=None, crown="Tree", trunk="Trunk", dead=False):
    h = h or rng.uniform(500, 900)
    m.block((x, y, z + h / 2), (50, 50, h), "Tree_Trunk", folder, trunk, shape="cylinder", roll=rng.uniform(-4, 4))
    if dead:
        for k in range(3):
            a = rng.uniform(0, 360)
            bx, by = _off(x, y, a, 80, 0)
            m.block((bx, by, z + h * rng.uniform(0.6, 0.9)), (160, 14, 14), "Tree_Branch", folder, trunk, yaw=a, pitch=rng.uniform(-40, 20),
                    collision=False)
        return
    for k in range(3):
        r = rng.uniform(220, 340)
        m.block((x + rng.uniform(-90, 90), y + rng.uniform(-90, 90), z + h + rng.uniform(-60, 120)), (r * 2, r * 2, r * 1.6),
                "Tree_Crown", folder, crown, shape="sphere", collision=False)


def palm(m, x, y, z, folder, rng, h=None):
    h = h or rng.uniform(600, 1100)
    lean = rng.uniform(-10, 10)
    for k in range(3):
        m.block((x + lean * k * 6, y, z + h * (k + 0.5) / 3), (45 - k * 6, 45 - k * 6, h / 3 + 10), "Palm_Trunk", folder, "Trunk",
                shape="cylinder", pitch=lean)
    tx = x + lean * 18
    for k in range(6):
        a = k * 60 + rng.uniform(-10, 10)
        fx, fy = _off(tx, y, a, 160, 0)
        m.block((fx, fy, z + h), (330, 70, 8), "Palm_Frond", folder, "Palm", yaw=a, pitch=-rng.uniform(15, 35), collision=False)


def bush(m, x, y, z, folder, rng, mat="Hedge"):
    r = rng.uniform(80, 160)
    m.block((x, y, z + r * 0.6), (r * 2, r * 2, r * 1.4), "Bush", folder, mat, shape="sphere", collision=False)


def flower_patch(m, x, y, z, folder, rng, mats):
    for k in range(rng.randint(3, 6)):
        m.block((x + rng.uniform(-120, 120), y + rng.uniform(-120, 120), z + 15), (30, 30, 30), "Flower", folder, rng.choice(mats),
                shape="sphere", collision=False)


def rubble_pile(m, x, y, z, folder, rng, mat="Rubble", n=None):
    for k in range(n or rng.randint(3, 6)):
        s = rng.uniform(60, 220)
        m.block((x + rng.uniform(-200, 200), y + rng.uniform(-200, 200), z + s * 0.35), (s, s * rng.uniform(0.6, 1.4), s * 0.7),
                "Rubble", folder, mat, yaw=rng.uniform(0, 360), pitch=rng.uniform(-20, 20), roll=rng.uniform(-20, 20))


def rebar(m, x, y, z, folder, rng, n=4):
    for k in range(n):
        h = rng.uniform(100, 300)
        m.block((x + rng.uniform(-60, 60), y + rng.uniform(-60, 60), z + h / 2), (6, 6, h), "Rebar", folder, "Rust" if "Rust" in m.mat else "Metal",
                pitch=rng.uniform(-30, 30), roll=rng.uniform(-30, 30), collision=False)


def rock(m, x, y, z, folder, rng, s=None, mat="Rock"):
    s = s or rng.uniform(80, 400)
    m.block((x, y, z + s * 0.25), (s, s * rng.uniform(0.6, 1.2), s * rng.uniform(0.4, 0.8)), "Rock", folder, mat, shape="sphere",
            yaw=rng.uniform(0, 360), tags=["JumpUp"] if s > 150 else None)


def sandbags(m, x, y, z, yaw, folder, length=400.0):
    n = int(length // 70)
    for row in range(2):
        for k in range(n - row):
            bx, by = _off(x, y, yaw, -length / 2 + 35 + k * 70 + row * 35, 0)
            m.block((bx, by, z + 18 + row * 32), (68, 40, 32), "Sandbag", folder, "Sand", yaw=yaw)


def barrier(m, x, y, z, yaw, folder, mat="Hazard"):
    m.block((x, y, z + 50), (200, 50, 100), "Barrier", folder, mat, yaw=yaw, tags=["Cover"])


def traffic_light(m, x, y, z, yaw, folder):
    m.block((x, y, z + 300), (20, 20, 600), "TL_Pole", folder, "Metal", shape="cylinder")
    ax, ay = _off(x, y, yaw, 200, 0)
    m.block((ax, ay, z + 580), (400, 14, 14), "TL_Arm", folder, "Metal", yaw=yaw, collision=False)
    bx, by = _off(x, y, yaw, 380, 0)
    m.block((bx, by, z + 540), (30, 40, 110), "TL_Box", folder, "Dark", yaw=yaw, collision=False)
    for k, mat in enumerate(("NeonRed", "NeonYellow", "NeonGreen")):
        lx, ly = _off(x, y, yaw, 380, -22)
        m.block((lx, ly, z + 575 - k * 33), (20, 4, 20), "TL_Light", folder, mat, yaw=yaw, collision=False)


def vending(m, x, y, z, yaw, folder, rng, mats):
    m.block((x, y, z + 95), (90, 70, 190), "Vending_Body", folder, "Grey", yaw=yaw, tags=["JumpUp"])
    fx, fy = _off(x, y, yaw, 0, 36)
    m.block((fx, fy, z + 120), (70, 4, 110), "Vending_Panel", folder, rng.choice(mats), yaw=yaw, collision=False)
    m.block((fx, fy, z + 35), (50, 4, 20), "Vending_Slot", folder, "Dark", yaw=yaw, collision=False)


def ac_unit(m, x, y, z, yaw, folder):
    m.block((x, y, z + 50), (120, 80, 100), "AC_Unit", folder, "Metal", yaw=yaw, tags=["JumpUp"])
    fx, fy = _off(x, y, yaw, 0, 41)
    m.block((fx, fy, z + 50), (70, 70, 4), "AC_Fan", folder, "Dark", yaw=yaw, pitch=90, shape="cylinder", collision=False)


def water_tank(m, x, y, z, folder, r=150.0):
    for dx in (-r * 0.6, r * 0.6):
        for dy in (-r * 0.6, r * 0.6):
            m.block((x + dx, y + dy, z + 100), (14, 14, 200), "Tank_Leg", folder, "Metal")
    m.block((x, y, z + 200 + r), (r * 2, r * 2, r * 2), "Tank_Body", folder, "Wood", shape="cylinder", tags=["Climbable"])
    m.block((x, y, z + 200 + r * 2 + 30), (r * 2.1, r * 2.1, 60), "Tank_Lid", folder, "Dark", shape="cone", collision=False)


def antenna(m, x, y, z, folder, h=500.0):
    m.block((x, y, z + h / 2), (14, 14, h), "Antenna_Mast", folder, "Metal", collision=False)
    for k in range(2):
        m.block((x, y, z + h * (0.6 + k * 0.25)), (120 - k * 40, 6, 6), "Antenna_Bar", folder, "Metal", yaw=k * 90, collision=False)
    m.block((x, y, z + h), (20, 20, 20), "Antenna_Light", folder, "NeonRed", shape="sphere", collision=False)


def power_pole(m, x, y, z, folder, h=900.0, mat="Wood"):
    m.block((x, y, z + h / 2), (30, 30, h), "Pole", folder, mat, shape="cylinder")
    m.block((x, y, z + h - 60), (220, 14, 14), "Pole_Crossarm", folder, mat, collision=False)
    m.block((x, y, z + h - 160), (50, 50, 80), "Pole_Transformer", folder, "Metal", shape="cylinder", collision=False)


def wire(m, a, b, folder, sag=120.0, mat="Dark"):
    """A cable from a to b drawn as two sagging segments."""
    mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2 - sag)
    for p, q in ((a, mid), (mid, b)):
        dx, dy, dz = q[0] - p[0], q[1] - p[1], q[2] - p[2]
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        if length < 1:
            continue
        m.block(((p[0] + q[0]) / 2, (p[1] + q[1]) / 2, (p[2] + q[2]) / 2), (length, 4, 4), "Wire", folder, mat,
                yaw=math.degrees(math.atan2(dy, dx)), pitch=math.degrees(math.atan2(dz, math.hypot(dx, dy))), collision=False)


def stall(m, x, y, z, yaw, folder, rng, awning):
    m.block((x, y, z + 50), (300, 120, 100), "Stall_Counter", folder, "Wood", yaw=yaw, tags=["JumpUp"])
    for dx in (-150, 150):
        for dy in (-60, 120):
            px, py = _off(x, y, yaw, dx, dy)
            m.block((px, py, z + 130), (10, 10, 260), "Stall_Post", folder, "Metal", collision=False)
    ax, ay = _off(x, y, yaw, 0, 30)
    m.block((ax, ay, z + 265), (330, 220, 8), "Stall_Awning", folder, awning, yaw=yaw, roll=-8, collision=False)
    for k in range(3):
        gx, gy = _off(x, y, yaw, -100 + k * 100, 0)
        m.block((gx, gy, z + 112), (60, 40, 25), "Stall_Goods", folder, rng.choice(["Coral", "Lime", "Sunflower", "Grey"]), yaw=yaw,
                collision=False)


def boat(m, x, y, z, yaw, folder, mat="Wood", wrecked=False, rng=None):
    tilt = rng.uniform(15, 40) if (wrecked and rng) else 0.0
    m.block((x, y, z + 60), (700, 220, 120), "Boat_Hull", folder, mat, yaw=yaw, roll=tilt, tags=["Boat", "JumpUp"])
    bx, by = _off(x, y, yaw, 330, 0)
    m.block((bx, by, z + 60), (160, 160, 118), "Boat_Bow", folder, mat, yaw=yaw + 45, roll=tilt)
    cx, cy = _off(x, y, yaw, -120, 0)
    m.block((cx, cy, z + 170), (220, 160, 110), "Boat_Cabin", folder, "Grey", yaw=yaw, roll=tilt)
    if not wrecked:
        m.block((x, y, z + 400), (12, 12, 500), "Boat_Mast", folder, "Wood", collision=False)


def desk_set(m, x, y, z, yaw, folder, rng):
    m.block((x, y, z + 74), (160, 80, 4), "Desk_Top", folder, "Wood", yaw=yaw, tags=["Desk", "JumpUp"])
    for dx in (-76, 76):
        lx, ly = _off(x, y, yaw, dx, 0)
        m.block((lx, ly, z + 37), (4, 76, 72), "Desk_Side", folder, "Grey", yaw=yaw)
    mx, my = _off(x, y, yaw, 0, 25)
    m.block((mx, my, z + 105), (60, 4, 38), "Desk_Monitor", folder, "Dark", yaw=yaw, collision=False)
    m.block((mx, my, z + 105), (54, 2, 32), "Desk_Screen", folder, "Holo", yaw=yaw, collision=False)
    kx, ky = _off(x, y, yaw, 0, -10)
    m.block((kx, ky, z + 77), (45, 15, 2), "Desk_Keyboard", folder, "Dark", yaw=yaw, collision=False)
    cx, cy = _off(x, y, yaw, rng.uniform(-20, 20), -70)
    cyaw = yaw + rng.uniform(-30, 30)
    m.block((cx, cy, z + 45), (50, 50, 8), "Chair_Seat", folder, "Dark", yaw=cyaw)
    bx, by = _off(cx, cy, cyaw, 0, -24)
    m.block((bx, by, z + 80), (48, 6, 60), "Chair_Back", folder, "Dark", yaw=cyaw, collision=False)
    m.block((cx, cy, z + 22), (8, 8, 44), "Chair_Stem", folder, "Metal", collision=False)


def cabinet(m, x, y, z, yaw, folder):
    m.block((x, y, z + 65), (50, 60, 130), "FilingCabinet", folder, "Metal", yaw=yaw, tags=["JumpUp"])
    for k in range(3):
        fx, fy = _off(x, y, yaw, 0, 31)
        m.block((fx, fy, z + 25 + k * 40), (30, 2, 4), "Cabinet_Handle", folder, "Dark", yaw=yaw, collision=False)


def plant(m, x, y, z, folder, rng):
    m.block((x, y, z + 25), (50, 50, 50), "Pot", folder, "Concrete", shape="cylinder")
    m.block((x, y, z + 90), (90, 90, 90), "Plant", folder, "Flora", shape="sphere", collision=False)


def server_rack(m, x, y, z, yaw, folder, rng, h=220.0):
    m.block((x, y, z + h / 2), (80, 110, h), "ServerRack", folder, "Dark", yaw=yaw, tags=["JumpUp"])
    for k in range(4):
        fx, fy = _off(x, y, yaw, 41, 0)
        m.block((fx, fy, z + 30 + k * (h - 40) / 4), (2, 90, 6), "Rack_Lights", folder, rng.choice(["NeonGreen", "NeonCyan", "Screen"]),
                yaw=yaw, collision=False)


def console(m, x, y, z, yaw, folder, rng):
    m.block((x, y, z + 50), (160, 70, 100), "Console", folder, "Metal", yaw=yaw, tags=["JumpUp"])
    sx, sy = _off(x, y, yaw, 0, -15)
    m.block((sx, sy, z + 140), (150, 6, 80), "Console_Screen", folder, rng.choice(["Screen", "Holo", "NeonYellow"]), yaw=yaw, pitch=0,
            collision=False)
    for k in range(4):
        bx, by = _off(x, y, yaw, -60 + k * 40, 20)
        m.block((bx, by, z + 102), (14, 14, 4), "Console_Button", folder, rng.choice(["NeonRed", "NeonGreen", "NeonYellow"]), yaw=yaw,
                collision=False)


def tank(m, x, y, z, folder, r=300.0, h=900.0, mat="Metal"):
    m.block((x, y, z + h / 2), (r * 2, r * 2, h), "Tank", folder, mat, shape="cylinder", tags=["Climbable"])
    m.block((x, y, z + h + 30), (r * 2 + 20, r * 2 + 20, 60), "Tank_Cap", folder, "Dark", shape="cylinder")
    for k in range(3):
        m.block((x, y, z + h * (0.25 + k * 0.25)), (r * 2 + 16, r * 2 + 16, 12), "Tank_Band", folder, "Hazard", shape="cylinder",
                collision=False)
    m.block((x + r + 20, y, z + h / 2), (30, 60, h), "Tank_Ladder", folder, "Ladder", tags=["Climbable"])


def pipe(m, a, b, folder, r=40.0, mat="Metal", supports=0.0, ground=0.0):
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    m.block(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2), (length, r * 2, r * 2), "Pipe", folder, mat, yaw=yaw, pitch=pitch,
            tags=["Climbable"])
    if supports > 0:
        n = int(length // supports)
        for k in range(1, n + 1):
            t = k / float(n + 1)
            px, py, pz = a[0] + dx * t, a[1] + dy * t, a[2] + dz * t
            m.block((px, py, (pz + ground) / 2), (20, 20, max(10.0, pz - ground)), "Pipe_Support", folder, "Dark", collision=False)


def angel(m, x, y, z, yaw, folder, moving=None):
    """An angel placeholder: a glowing body, two wing plates and a halo ring (owner: angels are beings of pure light)."""
    tags = ["AngelPlaceholder"]
    if moving:
        m.mover((x, y, z + 200), (90, 90, 280), "ANGEL_PLACEHOLDER", folder, "Angel", offset=moving[0], period=moving[1], phase=moving[2],
                collision=False, tags=tags, shape="sphere")
    else:
        m.block((x, y, z + 200), (90, 90, 280), "ANGEL_PLACEHOLDER", folder, "Angel", collision=False, tags=tags, shape="cylinder")
    for side in (-1, 1):
        wx, wy = _off(x, y, yaw, -20, side * 120)
        m.block((wx, wy, z + 260), (20, 200, 120), "Angel_Wing", folder, "Angel", yaw=yaw, roll=side * 25, collision=False)
    m.block((x, y, z + 380), (80, 80, 6), "Angel_Halo", folder, "Lamp", shape="cylinder", collision=False)


def window_grid(m, origin, along_yaw, width, height, folder, rng, cols, rows, mat="Glass", lit=None, out=12.0, normal_yaw=None):
    """Window panels on a flat face: origin = bottom-left corner on the face, the face runs along `along_yaw`, panels stick
    out `out` cm along `normal_yaw`."""
    a = math.radians(along_yaw)
    n = math.radians(normal_yaw if normal_yaw is not None else along_yaw - 90)
    cw, rh = width / cols, height / rows
    for i in range(cols):
        for j in range(rows):
            if rng.random() < 0.08:
                continue
            u = (i + 0.5) * cw
            x = origin[0] + u * math.cos(a) + out * math.cos(n)
            y = origin[1] + u * math.sin(a) + out * math.sin(n)
            z = origin[2] + (j + 0.5) * rh
            matj = lit if (lit and rng.random() < 0.18) else mat
            m.block((x, y, z), (cw * 0.7, 6, rh * 0.6), "Window", folder, matj, yaw=along_yaw, collision=False)


# Elite Bundle dressing (greybox_dress.py): a map with a mapping rule for a prop gets the real asset instead; otherwise unchanged.
import greybox_dress  # noqa: E402

greybox_dress.dress_props(globals())
