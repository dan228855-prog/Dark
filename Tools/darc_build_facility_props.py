# -*- coding: utf-8 -*-
"""
D.A.R.C. — наполнение главного здания: детали фасада, мебель, игровые объекты маршрута выезда
(разметка DARC.*), зоны комнат, редкие события. Всё — в папке Outliner «FacilityProps».

Не запускается сам по себе: его вызывает Tools/darc_build_facility.py, и только если папки
FacilityProps на уровне ещё нет. Автор двигает эти объекты руками — пересборка здания их не трогает.
Чтобы расставить заново по скрипту: удалить папку FacilityProps в Outliner и запустить сборку.
"""
# --- Детали фасада и крыши ---
DET = "Main/Details"
EBOX = "/Game/Scene_Warehouse/Assets/MS/3D/Urb_Str_ElectricalBox_Metal_Worn_01/SM_Urb_Str_ElectricalBox_Metal_Worn_01"
ECAB = "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Cabinet_Electric_Metal_Dirty_01/SM_Ind_War_Cabinet_Electric_Metal_Dirty_01"
VENT = "/Game/SD_Art/Industrial_Infrastructure/Assets/Wall_Panels/SM_Wall_Vent_Medium_Cover"
HANG_LAMP = "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Light_Ceiling_Metal_Hanging_02/SM_Ind_War_Light_Ceiling_Metal_Hanging_02"
EXT = "/Game/HospitalCorridor/Meshes/Objects/SM_FireExtinguisher_01"


def on_wall(side, along, height, out=0.0):
    """Точка на наружной грани фасада: сторона, позиция вдоль (u или v), высота над Z0, вынос."""
    if side == "S":
        return W(along, -out) + (Z0 + height,), 270.0
    if side == "N":
        return W(along, SIZE + out) + (Z0 + height,), 90.0
    if side == "E":
        return W(SIZE + out, along) + (Z0 + height,), 0.0
    return W(-out, along) + (Z0 + height,), 180.0


# Водостоки по углам и у ризалита — трубы до земли.
for i, (u, v) in enumerate([(30, -18), (SIZE - 30, -18), (30, SIZE + 18), (SIZE - 30, SIZE + 18), (1500, -18), (2100, -18)]):
    x, y = W(u, v)
    put(CYL, (x, y, (G_MIN + zr) / 2), scale=(0.12, 0.12, (zr - G_MIN) / 100), folder=DET, label="Drainpipe_%d" % i, mat="rust")
# Наружные блоки вентиляции/кондиционеров — неравномерно по фасадам.
for i, (side, along, h) in enumerate([("S", 700, 260), ("S", 2700, 690), ("S", 3500, 1120), ("E", 900, 300),
                                      ("E", 2600, 700), ("N", 500, 1110), ("N", 2300, 280), ("N", 3100, 720)]):
    p, yaw = on_wall(side, along, h, 30)
    put(VENT, p, yaw=yaw, folder=DET, label="AC_%s_%d" % (side, i))
# Электрошкафы и распределительные коробки у земли.
for i, (side, along) in enumerate([("S", 1000), ("E", 1500), ("N", 2900), ("S", 3300)]):
    p, yaw = on_wall(side, along, 0, 20)
    put(EBOX if i % 2 else ECAB, (p[0], p[1], G_MAX), yaw=yaw + 90, folder=DET, label="ElecBox_%d" % i)
# Кабели вдоль фасада (пучок над первым этажом) и вертикальный стояк к крыше.
for i, (side, a0, a1, h) in enumerate([("S", 200, 1450, 395), ("S", 2150, 3900, 395), ("N", 300, 3700, 410), ("E", 300, 3700, 400)]):
    p0, _ = on_wall(side, a0, h, 8)
    p1, _ = on_wall(side, a1, h, 8)
    beam(p0, p1, 6, "rust", DET, "CableBundle_%d" % i)
p0, _ = on_wall("E", 3550, 0, 10)
beam((p0[0], p0[1], G_MAX), (p0[0], p0[1], zr + 60), 12, "metal", DET, "CableRiser_E")
# Камеры наблюдения (корпус + кронштейн) по углам и над входом.
for i, (side, along, h) in enumerate([("S", 120, 380), ("S", 3880, 380), ("N", 120, 380), ("S", 2050, 360), ("E", 3880, 380)]):
    p, yaw = on_wall(side, along, h, 25)
    put(CUBE, p, yaw=yaw, pitch=-15, scale=(0.32, 0.14, 0.14), folder=DET, label="Camera_%d" % i, mat="metal")
    q, _ = on_wall(side, along, h + 10, 6)
    put(CUBE, q, yaw=yaw, scale=(0.16, 0.06, 0.06), folder=DET, label="CameraMount_%d" % i, mat="metal")
# Светильники над дверями (тёплые) — вход, служебные выходы, гараж.
for i, (side, along) in enumerate([("S", 1850), ("N", 3050), ("E", 3150), ("W", 1850)]):
    p, yaw = on_wall(side, along, 300, 35)
    put(HANG_LAMP, p, yaw=yaw, folder=DET, label="DoorLamp_%d" % i, collide=False)
    point_light((p[0], p[1], p[2] - 40), DET, "DoorLight_%d" % i, (1.0, 0.62, 0.3, 1), 900.0, 1200.0, True)
# Огнетушители у выходов внутри.
for i, (u, v, f) in enumerate([(1700, 120, 0), (900, 1050, 1), (3100, 2950, 2), (950, 1250, 0)]):
    x, y = W(u, v)
    put(EXT, (x, y, zf(f)), folder=DET, label="Extinguisher_%d" % i)
# Крыша: вентблоки, трубы, мачты, обтекатель (радом), лестница-стремянка к будке.
for i, (u, v) in enumerate([(600, 600), (3300, 900), (2800, 3400), (500, 3300), (3500, 2400)]):
    x, y = W(u, v)
    box(x - 90, y - 60, zr, x + 90, y + 60, zr + 110, "metal", "Main/Roof", "RoofUnit_%d" % i)
    put(VENT, (x, y + 70, zr + 20), yaw=90.0, folder="Main/Roof", label="RoofUnitGrille_%d" % i)
for i, (u, v, h) in enumerate([(3600, 3600, 600), (3700, 3500, 380), (400, 3700, 450)]):
    x, y = W(u, v)
    put(CYL, (x, y, zr + h / 2), scale=(0.06, 0.06, h / 100), folder="Main/Roof", label="RoofMast_%d" % i, mat="metal")
rx, ry = W(2800, 600)
box(rx - 180, ry - 180, zr, rx + 180, ry + 180, zr + 120, "concrete", "Main/Roof", "RadomeBase")
put(SPHERE, (rx, ry, zr + 300), scale=(3.4, 3.4, 3.4), folder="Main/Roof", label="Radome", mat="plaster")
for i in range(6):
    a, b = W(300 + i * 600, 2000), W(300 + i * 600 + 400, 2000)
    beam((a[0], a[1], zr + 40), (b[0], b[1], zr + 40), 18, "rust", "Main/Roof", "RoofPipe_%d" % i)

# =====================================================================================
# Наполнение: створки дверей, мебель и оборудование по типам помещений, игровые объекты
# маршрута выезда (разметка DARC.* — см. docs/MAP_MARKUP.md), зоны комнат, редкие события.
# =====================================================================================
sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
frng = random.Random(4242)


def tag(actor, tags):
    if actor:
        actor.tags = [unreal.Name(t) for t in tags]
    return actor


def ensure_simple_collision(path):
    """Подвижным объектам (двери, предметы) нужна простая коллизия — без неё физика не работает."""
    mesh = asset(path)
    if mesh and sm_sub.get_simple_collision_count(mesh) == 0:
        sm_sub.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
        eal.save_loaded_asset(mesh)


def mesh_box(path):
    b = asset(path).get_bounding_box()
    return b.min, b.max


def size_of(path, scale=1.0):
    mn, mx = mesh_box(path)
    return (mx.x - mn.x) * scale, (mx.y - mn.y) * scale, (mx.z - mn.z) * scale


def place(path, u, v, f, yaw=0.0, folder="", label="", tags=None, z=None, scale=1.0, collide=True):
    """Модель центром основания в точку (u, v) на полу этажа f (или на высоте z)."""
    mn, mx = mesh_box(path)
    cx, cy = (mn.x + mx.x) / 2 * scale, (mn.y + mx.y) / 2 * scale
    ox, oy = rot2(cx, cy, yaw)
    x, y = W(u, v)
    zz = (zf(f) + 2 if z is None else z) - mn.z * scale
    a = put(path, (x - ox, y - oy, zz), yaw=yaw, scale=(scale, scale, scale), folder=folder, label=label, collide=collide)
    return tag(a, tags) if tags else a


# --- Модели ---
P = {
    "desk": "/Game/Med_Office_Desk/StaticMesh/SM_Med_Office_Desk",
    "table_old": "/Game/FreeFurniturePack/Meshes/SM_Old_Table",
    "table_meet": "/Game/FreeFurniturePack/Meshes/SM_Classic_table",
    "table_round": "/Game/FreeFurniturePack/Meshes/SM_Classic_Round_Table",
    "table_work": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Furniture_Table_Wood_Worn_01/SM_Ind_War_Furniture_Table_Wood_Worn_01_B",
    "coffee": "/Game/FreeFurniturePack/Meshes/SM_Coffee_Table",
    "chair": "/Game/FreeFurniturePack/Meshes/SM_Old_Chair",
    "chair_office": "/Game/FreeFurniturePack/Meshes/SM_Modern_Office_chair",
    "sofa": "/Game/FreeFurniturePack/Meshes/SM_Classic_Leather_sofa_2",
    "armchair": "/Game/FreeFurniturePack/Meshes/SM_Classic_Leather_Armchair",
    "cupboard": "/Game/FreeFurniturePack/Meshes/SM_Classic_Cupboard",
    "locker": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Storage_Cabinet_Metal_Rusty_01/SM_Ind_War_Storage_Cabinet_Metal_Rusty_01",
    "shelf": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Storage_Shelf_Metal_Rusted_02/SM_Ind_War_Storage_Shelf_Metal_Rusted_02",
    "ecab": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Cabinet_Electric_Metal_Dirty_01/SM_Ind_War_Cabinet_Electric_Metal_Dirty_01",
    "ebox": "/Game/Scene_Warehouse/Assets/MS/3D/Urb_Str_ElectricalBox_Metal_Worn_01/SM_Urb_Str_ElectricalBox_Metal_Worn_01",
    "box_big": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Storage_Box_Cardboard_Worn_02/SM_Ind_War_Storage_Box_Cardboard_Worn_02",
    "box": "/Game/IndustryPropsPack6/Meshes/SM_CartonBox03",
    "box_s": "/Game/IndustryPropsPack6/Meshes/SM_CartonBox01",
    "crate": "/Game/Construction_VOL1/Meshes/SM_Crate_01a",
    "barrel": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_Aba_Storage_Barrel_Metal_Rust_01/SM_Ind_Aba_Storage_Barrel_Metal_Rust_01",
    "gas": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_Aba_Tank_Gas_Metal_Worn_02/SM_Ind_Aba_Tank_Gas_Metal_Worn_02",
    "watertank": "/Game/Construction_VOL1/Meshes/SM_WaterTank_01a",
    "reel": "/Game/Construction_VOL1/Meshes/SM_CableReel",
    "battery": "/Game/Sci_Fi_Light/Static_Meshes/SM_Sci_Fi_Battery_Unit/Mesh/SM_Sci_Fi_Battery_Unit",
    "toilet": "/Game/FreeFurniturePack/Meshes/SM_Toilet",
    "bench": "/Game/HospitalCorridor/Meshes/Objects/SM_HospitalBench_01",
    "bin": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Trash_Bin_Metal_Worn_02/SM_Ind_War_Trash_Bin_Metal_Worn_02_A",
    "monitor": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerMonitor_A01_N1",
    "monitor2": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerMonitor_A02_N1",
    "keyboard": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerKeyboard_A01_N1",
    "tower": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerTower_B01_N1",
    "printer": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerPrinter_A01_N1",
    "phone": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_Phone_A01_N1",
    "parts": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerParts_A05_N1",
    "speaker": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerSpeakers_A01_N1",
    "drive": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_Record_HardDrive_A01_N1",
    "module": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_ComputerParts_A04_N1",
    "toolbox": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_Toolbox_A01_N1",
    "lamp_desk": "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_DeskLamp_A01_N1",
    "fusebox": "/Game/HospitalCorridor/Meshes/Objects/SM_UtilityElectronics_01_C",
    "cardreader": "/Game/HospitalCorridor/Meshes/Objects/SM_UtilityElectronics_01_N",
    "keypad": "/Game/Sci_Fi_Light/Static_Meshes/SM_Sci_Fi_Info_Terminal/Mesh/SM_Sci_Fi_Info_Terminal",
    "inlet": "/Game/HospitalCorridor/Meshes/Objects/SM_UtilityElectronics_01_B",
    "generator": "/Game/Construction_VOL1/Meshes/SM_LightGenerator_Base_01a",
    "pallet": "/Game/Scene_Warehouse/Assets/MS/3D/Ind_War_Storage_Pallet_Wood_Worn_01/SM_Ind_War_Storage_Pallet_Wood_Worn_01",
    "mug": "/Game/UnrealBucket/Library/ArchViz/CoffeeGrinder/SM_Grinder_Body",
}
PAPERS = ["/Game/Deko_MatrixDemo/Apartment/Meshes/SM_BooksPamphlets_%s01_N1" % c for c in "ABCDEGHL"]
P = {k: v for k, v in P.items() if eal.does_asset_exist(v)}
PAPERS = [p for p in PAPERS if eal.does_asset_exist(p)]


unreal.log("[DARC] props: doors")
# --- Створки дверей ---
LEAF = PP + "Doors/SM_Door_Single_01"
ensure_simple_collision(LEAF)
SERVER_ROOM = NAMES[1]["CoreBlock"]
RECORDS_ROOM = NAMES[1]["E2"]
DOOR_AT = {}   # (этаж, помещение) → запись двери (для устройств рядом с дверью)
# Старые створки (пересборка коробки их сохраняет) — убрать, ставим заново вместе с наполнением.
_old_leaves = [a for a in sub.get_all_level_actors() if "/Doors" in str(a.get_folder_path())
               and str(a.get_folder_path()).startswith(ROOT + "/")]
if _old_leaves:
    sub.destroy_actors(_old_leaves)
for i, d in enumerate(DOOR_LIST):
    names = set(d["pair"])
    opening = WALLS[d["kind"]][2]
    s = d["stretch"]
    px, py, z = d["pivot"]
    ox, oy = rot2(opening[0] * s, -8.5, d["yaw"])
    leaf = put(LEAF, (px + ox, py + oy, z), yaw=d["yaw"], scale=((opening[1] - opening[0]) * s / 140.0, 1, 240.0 / 309.0),
               folder="Main/F%d/Doors" % (d["f"] + 1), label="Door_%03d" % i)
    flags, extra = "DARC.Door", []
    if d["f"] == 1 and SERVER_ROOM in names:
        flags += ".Locked"
        extra.append("Id=ServerDoor")
    elif d["f"] == 1 and RECORDS_ROOM in names and any(is_corridor(n) for n in names):
        extra.append("Id=RecordsDoor")
    elif any(n.startswith("Stair") for n in names) or frng.random() < 0.35:
        flags += ".Open"
    tag(leaf, [flags] + extra)
    for n in names:
        if n and not is_corridor(n) and n != "Outside":
            DOOR_AT.setdefault((d["f"], n), d)


# --- Свободные места вдоль стен помещения (не у дверей, не со стороны коридора) ---
def rect_of(f, name):
    for r in plan(f):
        if r[0] == name:
            return r[1:]
    return None


def door_centers(f, axis, line):
    out = []
    for d in DOOR_LIST:
        if d["f"] == f and d["axis"] == axis and abs(d["line"] - line) < 1:
            out.append(d["center"])
    return out


def wall_slots(f, name, margin=40):
    """[(сторона, линия, от, до, наружу_ли)]: участки стены, к которым можно ставить мебель."""
    u0, v0, u1, v1 = rect_of(f, name)
    out = []
    for side, axis, line, a0, a1, probe in [("S", "v", v0, u0, u1, -50), ("N", "v", v1, u0, u1, 50),
                                            ("W", "u", u0, v0, v1, -50), ("E", "u", u1, v0, v1, 50)]:
        free, start = [], None
        doors = door_centers(f, axis, line)
        for a in range(int(a0) + margin, int(a1) - margin, 50):
            pu, pv = (a + 25, line + probe) if axis == "v" else (line + probe, a + 25)
            nb = room_at(f, pu, pv)
            ok = not is_corridor(nb) and not any(abs(a + 25 - c) < 130 for c in doors)
            if nb is None:   # наружная дверь в этом модуле фасада — не загораживать
                ok = ok and not any(abs((a + 25) - (m_ * 400 + 200)) < 220 for (s_, m_, f_) in FACADE_DOORS
                                    if s_ == side and f_ == f)
            if ok and start is None:
                start = a
            if not ok and start is not None:
                free.append((start, a))
                start = None
        if start is not None:
            free.append((start, int(a1) - margin))
        for s0, s1 in free:
            if s1 - s0 >= 100:
                out.append((side, line, s0, s1, room_at(f, *((s0 + 25, line + probe) if axis == "v" else (line + probe, s0 + 25))) is None))
    return out


# Куда смотрит «лицо» модели: по короткой горизонтальной оси, в плюс; у этих — в минус.
FRONT_FLIP = set()


def face_yaw(side, front_axis, flip=False):
    """Поворот, при котором лицо модели (локальная +X или +Y) смотрит в комнату от стены side."""
    if front_axis == "Y":
        yaw = {"S": 0.0, "N": 180.0, "W": -90.0, "E": 90.0}[side]
    else:
        yaw = {"S": 90.0, "N": -90.0, "W": 0.0, "E": 180.0}[side]
    return yaw + (180.0 if flip else 0.0)


def along(slot, path, f, spacing, folder, prefix, max_count=99, scale=1.0, tags=None, gap=12):
    """Ряд моделей вдоль участка стены: длинной стороной вдоль стены, лицом в комнату."""
    side, line, s0, s1, _ = slot
    sx, sy, _ = size_of(path, scale)
    wall_along_u = side in ("S", "N")
    long_x = sx >= sy
    yaw = face_yaw(side, "Y" if long_x else "X", path in FRONT_FLIP)
    width, depth = max(sx, sy), min(sx, sy)
    inward = {"S": 1, "N": -1, "W": 1, "E": -1}[side]
    n = min(max_count, int((s1 - s0 + spacing - width) // spacing))
    placed = []
    if n <= 0:
        return placed
    start = s0 + ((s1 - s0) - (n - 1) * spacing) / 2
    for i in range(n):
        a = start + i * spacing
        off = line + inward * (depth / 2 + gap)
        u, v = (a, off) if wall_along_u else (off, a)
        placed.append((place(path, u, v, f, yaw=yaw, folder=folder, label="%s_%d" % (prefix, i), scale=scale, tags=tags), u, v, yaw, side))
    return placed


def on_top(path, base, base_path, f, folder, label, du=0.0, dv=0.0, yaw=None, tags=None):
    """Модель сверху на другой (стол → монитор, полка → коробка)."""
    actor, u, v, byaw, _ = base
    top = zf(f) + 2 + size_of(base_path)[2]
    return place(path, u + du, v + dv, f, yaw=byaw if yaw is None else yaw, z=top, folder=folder, label=label, tags=tags)


def inward_vec(side, dist):
    return {"S": (0, dist), "N": (0, -dist), "W": (dist, 0), "E": (-dist, 0)}[side]


OFFICE = {"AdminBlock", "Security", "Garage_Office", "WorkRoom", "Office_2A", "Office_2B", "Office_2C", "Office_2D",
          "Office_3A", "Office_3B", "Office_3C", "Director", "Medical", "Records"}
TECHR = {"Electrical", "Switchgear", "TechRoom", "Telemetry", "Engineering", "SignalProcessing", "AntennaEquipment", "AntennaControl"}
STORE = {"Storage", "Storage_2", "Archive", "Archive_B", "Archive_3", "Library", "CoreCloset", "Utility", "Darkroom"}
WORKR = {"Workshop", "ToolRoom", "Boiler", "Ventilation", "Generator"}
LABS = {"Lab", "Laboratory"}


def furnish_office(f, name, folder):
    for slot in wall_slots(f, name):
        if slot[4]:   # у окна — столы с компьютерами
            for desk in along(slot, P["desk"], f, 230, folder, name + "_Desk", max_count=3):
                side = desk[4]
                # Монитор (лицо +Y) — экраном в комнату, к стулу; клавиатура перед ним.
                mx, my = inward_vec(side, -12)
                on_top(P["monitor" if frng.random() < 0.6 else "monitor2"], desk, P["desk"], f, folder, name + "_Monitor",
                       du=mx, dv=my, yaw=face_yaw(side, "Y"))
                kx, ky = inward_vec(side, 18)
                on_top(P["keyboard"], desk, P["desk"], f, folder, name + "_Keyboard", du=kx, dv=ky, yaw=face_yaw(side, "Y"))
                if PAPERS:
                    px_, py_ = (frng.uniform(-55, 55), 0) if side in ("S", "N") else (0, frng.uniform(-55, 55))
                    on_top(frng.choice(PAPERS), desk, P["desk"], f, folder, name + "_Papers", du=px_, dv=py_,
                           yaw=face_yaw(side, "Y") + frng.uniform(-20, 20))
                # Стул — перед столом, спинкой в комнату (лицом к столу), чуть небрежно.
                cx, cy = inward_vec(side, 80)
                place(P["chair_office" if frng.random() < 0.5 else "chair"], desk[1] + cx, desk[2] + cy, f,
                      yaw=face_yaw(side, "Y") + 180.0 + frng.uniform(-25, 25), folder=folder, label=name + "_Chair")
        else:
            along(slot, P["locker" if frng.random() < 0.5 else "cupboard"], f, 150, folder, name + "_Cabinet", max_count=3)
    u0, v0, u1, v1 = rect_of(f, name)
    place(P["bin"], u0 + 60, v0 + 60, f, folder=folder, label=name + "_Bin")


def furnish_tech(f, name, folder):
    for i, slot in enumerate(wall_slots(f, name)):
        along(slot, P["ecab"], f, 150, folder, "%s_Cab%d" % (name, i), max_count=4)
    u0, v0, u1, v1 = rect_of(f, name)
    place(P["reel"], (u0 + u1) / 2, (v0 + v1) / 2, f, yaw=frng.uniform(0, 90), folder=folder, label=name + "_Reel")
    place(P["box"], (u0 + u1) / 2 + 120, (v0 + v1) / 2 - 80, f, yaw=frng.uniform(0, 90), folder=folder, label=name + "_Box")


def furnish_store(f, name, folder):
    # Архивные шкафы вдоль стен, у окон — штабеля коробок (стеллаж из пака перекошен — не используем).
    for i, slot in enumerate(wall_slots(f, name)):
        if slot[4]:
            for b in along(slot, P["box_big"], f, 150, folder, "%s_Stack%d" % (name, i), max_count=3):
                if frng.random() < 0.7:
                    on_top(P["box"], b, P["box_big"], f, folder, name + "_StackTop", yaw=b[3] + frng.uniform(-15, 15))
        else:
            along(slot, P["cupboard"] if name.startswith(("Archive", "Library", "Records")) else P["locker"], f, 150,
                  folder, "%s_Cabinet%d" % (name, i), max_count=4)
    u0, v0, u1, v1 = rect_of(f, name)
    if (u1 - u0) * (v1 - v0) > 600 * 600:
        p = place(P["pallet"], (u0 + u1) / 2, (v0 + v1) / 2, f, yaw=frng.uniform(0, 30), folder=folder, label=name + "_Pallet")
        place(P["box_big"], (u0 + u1) / 2, (v0 + v1) / 2, f, z=zf(f) + 2 + size_of(P["pallet"])[2], yaw=frng.uniform(0, 30),
              folder=folder, label=name + "_PalletBox")


def furnish_work(f, name, folder):
    slots = wall_slots(f, name)
    for i, slot in enumerate(slots):
        if i % 2 == 0:
            for t in along(slot, P["table_work"], f, 220, folder, "%s_Bench%d" % (name, i), max_count=2):
                on_top(P["toolbox"], t, P["table_work"], f, folder, name + "_Toolbox", du=frng.uniform(-40, 40))
        else:
            along(slot, P["barrel"], f, 90, folder, "%s_Barrel%d" % (name, i), max_count=3)
    u0, v0, u1, v1 = rect_of(f, name)
    for k in range(3):
        place(P["gas"], u0 + 80 + k * 45, v1 - 80, f, folder=folder, label="%s_Gas%d" % (name, k))
    if name in ("Boiler", "Ventilation"):
        place(P["watertank"], (u0 + u1) / 2, (v0 + v1) / 2, f, folder=folder, label=name + "_Tank")


def furnish_lab(f, name, folder):
    for i, slot in enumerate(wall_slots(f, name)):
        for t in along(slot, P["table_old"], f, 210, folder, "%s_Table%d" % (name, i), max_count=3):
            on_top(frng.choice([P["parts"], P["printer"], P["tower"], P["speaker"]]), t, P["table_old"], f, folder,
                   name + "_Equip", du=frng.uniform(-40, 40))


def furnish_batteries(f, name, folder):
    for i, slot in enumerate(wall_slots(f, name)):
        for b in along(slot, P["battery"], f, 80, folder, "%s_Batt%d" % (name, i), max_count=8):
            on_top(P["battery"], b, P["battery"], f, folder, name + "_Batt2")


def furnish_wc(f, name, folder):
    slots = wall_slots(f, name)
    if slots:
        along(max(slots, key=lambda s: s[3] - s[2]), P["toilet"], f, 110, folder, name + "_Toilet", max_count=4)


def furnish_rest(f, name, folder):
    u0, v0, u1, v1 = rect_of(f, name)
    cu, cv = (u0 + u1) / 2, (v0 + v1) / 2
    if name == "Meeting":
        place(P["table_meet"], cu, cv, f, yaw=90.0, folder=folder, label="Meeting_Table")
        for k in range(6):
            du = (k % 3 - 1) * 90
            dv = 80 if k < 3 else -80
            place(P["chair"], cu + du, cv + dv, f, yaw=-90.0 if dv > 0 else 90.0, folder=folder, label="Meeting_Chair%d" % k)
    else:
        slots = wall_slots(f, name)
        if slots:
            along(slots[0], P["sofa"], f, 300, folder, name + "_Sofa", max_count=1)
        place(P["coffee"], cu, cv, f, folder=folder, label=name + "_Coffee")
        place(P["table_round"], cu + 150, cv - 150, f, folder=folder, label=name + "_Table")
        for k in range(3):
            a = math.radians(k * 120)
            place(P["chair"], cu + 150 + 85 * math.cos(a), cv - 150 + 85 * math.sin(a), f, yaw=k * 120 + 180,
                  folder=folder, label="%s_Chair%d" % (name, k))


def furnish_lobby(f, name, folder):
    for i, slot in enumerate(wall_slots(f, name)):
        along(slot, P["bench"], f, 260, folder, "%s_Bench%d" % (name, i), max_count=1)


def furnish(f, name):
    folder = "Props/F%d/%s" % (f + 1, name)
    if name in OFFICE:
        furnish_office(f, name, folder)
    elif name in TECHR:
        furnish_tech(f, name, folder)
    elif name in STORE:
        furnish_store(f, name, folder)
    elif name in WORKR:
        furnish_work(f, name, folder)
    elif name in LABS:
        furnish_lab(f, name, folder)
    elif name == "UPS_Batteries":
        furnish_batteries(f, name, folder)
    elif name.startswith("WC") or name == "CoreWC":
        furnish_wc(f, name, folder)
    elif name in ("BreakRoom", "Meeting"):
        furnish_rest(f, name, folder)
    elif name.startswith("Lobby") or name.startswith("Hall"):
        furnish_lobby(f, name, folder)


unreal.log("[DARC] props: furniture")
for f in range(FLOORS):
    for r in plan(f):
        name = r[0]
        if is_corridor(name) and not name.startswith(("Lobby", "Hall")):
            continue
        if name.startswith("Stair") or (f == 1 and name == SERVER_ROOM):
            continue
        try:
            furnish(f, name)
        except Exception as e:
            unreal.log_warning("[DARC] furnish %s F%d: %s" % (name, f + 1, e))


unreal.log("[DARC] props: gameplay")
# --- Игровые объекты маршрута выезда (2-й этаж; щитки корпуса — 1-й) ---
GP = "Gameplay"
ensure_simple_collision(P["drive"])
ensure_simple_collision(P["module"])


def wall_device(f, name, path, side, along_pos, height, tags, label, out=14.0, yaw_fix=0.0):
    """Устройство на стене помещения (щиток, табличка): сторона S/N/W/E, позиция вдоль, высота."""
    u0, v0, u1, v1 = rect_of(f, name)
    if side == "S":
        u, v, yaw = along_pos, v0 + out, 90.0
    elif side == "N":
        u, v, yaw = along_pos, v1 - out, -90.0
    elif side == "W":
        u, v, yaw = u0 + out, along_pos, 0.0
    else:
        u, v, yaw = u1 - out, along_pos, 180.0
    return place(path, u, v, f, yaw=yaw + yaw_fix, z=zf(f) + height, folder=GP, label=label, tags=tags)


# Щитки корпуса (1 эт., электрощитовая): коридор и здание. Модель смотрит «лицом» в −X → доворот 180.
el = NAMES[0]["SW"]
eu0, ev0, eu1, ev1 = rect_of(0, el)
wall_device(0, el, P["fusebox"], "N", eu0 + 250, 120, ["DARC.FuseBox", "Circuit=Corridor", "Name=Name_FuseBox_Corridor", "Hint=Hint_FuseBox"], "FuseBox_Corridor", yaw_fix=180)
wall_device(0, el, P["fusebox"], "N", eu0 + 450, 120, ["DARC.FuseBox", "Circuit=Building", "Name=Name_FuseBox_Building", "Hint=Hint_FuseBox"], "FuseBox_Building", yaw_fix=180)

# Техпомещение (2 эт., угол СЗ — вход через аккумуляторную): щиток сервера без предохранителя и интерфейсный модуль.
tr = NAMES[1]["SW"]
tu0, tv0, tu1, tv1 = rect_of(1, tr)
wall_device(1, tr, P["fusebox"], "W", tv0 + 400, 120,
            ["DARC.FuseBox.NoFuse", "Circuit=Server", "Collateral=Corridor", "TaskOnBlow=RestoreCorridorLight",
             "Name=Name_FuseBox_Server", "Hint=Hint_FuseBox_NoFuse"], "FuseBox_Server", yaw_fix=180)
mt = place(P["table_work"], (tu0 + tu1) / 2, (tv0 + tv1) / 2, 1, folder=GP, label="ModuleTable")
place(P["module"], (tu0 + tu1) / 2, (tv0 + tv1) / 2, 1, z=zf(1) + 2 + size_of(P["table_work"])[2], folder=GP, label="InterfaceModule",
      tags=["DARC.Module", "Id=InterfaceModule", "Name=Name_InterfaceModule", "Hint=Hint_InterfaceModule"])

# Запасной предохранитель — на полке в кладовой (2 эт., N2).
st = NAMES[1]["N2"]
su0, sv0, su1, sv1 = rect_of(1, st)
fs = place(P["table_work"], su0 + 160, sv1 - 80, 1, folder=GP, label="FuseTable")
place(P["module"], su0 + 160, sv1 - 80, 1, z=zf(1) + 2 + size_of(P["table_work"])[2], folder=GP, label="SpareFuse",
      tags=["DARC.Fuse", "Id=SpareFuse", "Name=Name_Fuse", "Hint=Hint_Fuse"])

# Генератор — в аккумуляторной (2 эт., W1): тащить к вводу сервера.
up = NAMES[1]["W1"]
uu0, uv0, uu1, uv1 = rect_of(1, up)
place(P["generator"], (uu0 + uu1) / 2 + 80, (uv0 + uv1) / 2, 1, yaw=90.0, folder=GP, label="Generator",
      tags=["DARC.Generator", "Mass=70", "Name=Name_Generator", "Hint=Hint_Generator"])

# Рабочая комната (2 эт., S1): терминал каталога (код с карты охранника) и «левитирующая» кружка.
wr = NAMES[1]["S1"]
wu0, wv0, wu1, wv1 = rect_of(1, wr)
tdesk = place(P["desk"], (wu0 + wu1) / 2, wv0 + 160, 1, yaw=0.0, folder=GP, label="CatalogDesk")
dz = zf(1) + 2 + size_of(P["desk"])[2]
place(P["monitor"], (wu0 + wu1) / 2, wv0 + 160, 1, yaw=0.0, z=dz, folder=GP, label="CatalogTerminal",
      tags=["DARC.Terminal", "Code=CatalogCode", "Task=OpenCatalog", "Circuit=Building", "Title=Screen_CatalogTitle",
            "Words=File_Corrupted_1,File_Corrupted_2,File_Corrupted_3,File_Corrupted_4",
            "Lines=File_VerifyNote,File_MainStorage", "Name=Name_Terminal", "Hint=Hint_Terminal"])
place(P["chair_office"], (wu0 + wu1) / 2, wv0 + 250, 1, yaw=90.0, folder=GP, label="CatalogChair")
mug_desk = place(P["table_old"], wu1 - 160, wv1 - 140, 1, folder=GP, label="MugTable")

# Кабинет 2A: табличка с кодом двери серверной (код меняется каждый выезд).
oa = NAMES[1]["S2"]
ou0, ov0, ou1, ov1 = rect_of(1, oa)
x_s, y_s = W(ou1 - 370, ov1 - 14)
tag(put(CUBE, (x_s, y_s, zf(1) + 170), yaw=-90.0, scale=(0.04, 0.6, 0.4), folder=GP, label="ServerCodeSign", mat="metal"),
    ["DARC.CodeSign", "Code=ServerDoorCode", "Size=20"])

# Серверная (2 эт., ядро): запертая дверь (считыватель + кодовая панель снаружи), стойка, станция, носитель.
sr = SERVER_ROOM
ru0, rv0, ru1, rv1 = rect_of(1, sr)
sd = DOOR_AT.get((1, sr))
if sd:
    # Сторона коридора: дверь на линии u (вертикальная граница) — коридор с меньшим u (запад), см. планировку.
    corridor_west = room_at(1, sd["line"] - 50, sd["center"]) is not None and is_corridor(room_at(1, sd["line"] - 50, sd["center"]))
    cu_ = sd["line"] - 14 if corridor_west else sd["line"] + 14
    yaw_out = 180.0 if corridor_west else 0.0
    place(P["cardreader"], cu_, sd["center"] + 120, 1, yaw=yaw_out - 90.0, z=zf(1) + 115, folder=GP, label="ServerCardReader",
          tags=["DARC.CardReader", "Door=ServerDoor", "Task=ServerDoorCard", "Name=Name_CardReader", "Hint=Hint_CardReader"])
    place(P["keypad"], cu_, sd["center"] - 125, 1, yaw=yaw_out + 180.0, z=zf(1) + 95, folder=GP, label="ServerKeypad",
          tags=["DARC.Keypad", "Door=ServerDoor", "Task=ServerDoorKeypad", "Name=Name_Keypad", "Hint=Hint_Keypad"])
# Ряды стоек (декор) вдоль длинных стен, тяжёлая стойка — в дальнем конце, точка доставки — у станции.
for i, slot in enumerate(wall_slots(1, sr)):
    along(slot, P["ecab"], 1, 145, "Props/F2/" + sr, "%s_Rack%d" % (sr, i), max_count=4)
place(P["ecab"], (ru0 + ru1) / 2, rv1 - 260, 1, yaw=0.0, folder=GP, label="HeavyServerRack",
      tags=["DARC.Heavy", "Mass=80", "Deliver=RackPoint", "Task=MoveServerRack", "Name=Name_Rack", "Hint=Hint_Rack"])
rp = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*W((ru0 + ru1) / 2, rv0 + 420)) + unreal.Vector(0, 0, zf(1) + 10))
rp.set_folder_path(root_for(GP))
rp.set_actor_label("RackPoint")
tag(rp, ["DARC.Point", "Id=RackPoint"])
sdesk = place(P["table_old"], (ru0 + ru1) / 2, rv0 + 180, 1, folder=GP, label="StationDesk")
sz = zf(1) + 2 + size_of(P["table_old"])[2]
place(P["parts"], (ru0 + ru1) / 2 - 45, rv0 + 180, 1, z=sz, folder=GP, label="InterfaceSlot",
      tags=["DARC.Slot", "Id=InterfaceSlot", "Accepts=Module", "Task=ConnectInterface", "Name=Name_InterfaceSlot", "Hint=Hint_InterfaceSlot"])
place(P["tower"], (ru0 + ru1) / 2 + 45, rv0 + 180, 1, z=sz, folder=GP, label="DriveSlot",
      tags=["DARC.Slot.Eject", "Id=DriveSlot", "Accepts=Drive", "Holds=ArchiveDrive", "TaskRemove=TakeDrive",
            "Name=Name_DriveSlot", "Hint=Hint_DriveSlot"])
place(P["drive"], (ru0 + ru1) / 2 + 45, rv0 + 180, 1, z=sz + 50, folder=GP, label="ArchiveDrive",
      tags=["DARC.Drive", "Id=ArchiveDrive", "Name=Name_Drive", "Hint=Hint_Drive"])
place(P["monitor2"], (ru0 + ru1) / 2, rv0 + 140, 1, yaw=0.0, z=sz, folder=GP, label="TransferStation",
      tags=["DARC.Station", "Interface=InterfaceSlot", "DriveSlot=DriveSlot", "Task=CopyArchive", "TaskMains=PowerViaPanel",
            "TaskGenerator=PowerViaGenerator", "Circuit=Server", "Flicker=Corridor", "Name=Name_Server", "Hint=Hint_Server"])
place(P["monitor"], ru1 - 120, (rv0 + rv1) / 2, 1, yaw=90.0, z=zf(1) + 2 + size_of(P["table_old"])[2], folder=GP, label="ServerConsole",
      tags=["DARC.Terminal.Unlocked", "Circuit=Server", "Title=Screen_ArchiveTitle", "Lines=Screen_ArchiveList,Screen_ExtraFileHint",
            "Commands=COPY:TakeExtraFile", "Name=Name_Console", "Hint=Hint_Console"])
place(P["table_old"], ru1 - 120, (rv0 + rv1) / 2, 1, yaw=90.0, folder=GP, label="ConsoleDesk")
wall_device(1, sr, P["inlet"], "S", (ru0 + ru1) / 2 + 200, 60, ["DARC.Inlet", "Circuit=Server", "Name=Name_Inlet", "Hint=Hint_Inlet"],
            "ServerInlet", yaw_fix=180)


# --- Зоны комнат (память мира, события) ---
def room_volume(f, rect, room_id, label):
    u0, v0, u1, v1 = rect
    x0, y0 = W(u0, v0)
    x1, y1 = W(u1, v1)
    tb = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TriggerBox, unreal.Vector((x0 + x1) / 2, (y0 + y1) / 2, zf(f) + 200))
    tb.set_actor_scale3d(unreal.Vector((x1 - x0) / 64.0, (y1 - y0) / 64.0, 400 / 64.0))
    tb.set_folder_path(root_for("Rooms"))
    tb.set_actor_label(label)
    tag(tb, ["DARC.Room", "Id=" + room_id])


room_volume(0, rect_of(0, "Lobby"), "Lobby", "Room_Lobby")
room_volume(1, (800, 1100, 1100, 2900), "Corridor", "Room_Corridor")
room_volume(1, rect_of(1, wr), "WorkRoom", "Room_WorkRoom")
room_volume(1, rect_of(1, tr), "TechRoom", "Room_TechRoom")
room_volume(1, rect_of(1, st), "Storage", "Room_Storage")
room_volume(1, rect_of(1, sr), "ServerRoom", "Room_ServerRoom")
room_volume(1, rect_of(1, RECORDS_ROOM), "TechCabinetRoom", "Room_Records")
tb = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TriggerBox, unreal.Vector(*W(2000, -700)) + unreal.Vector(0, 0, Z0 + 200))
tb.set_actor_scale3d(unreal.Vector(6000 / 64.0, 1400 / 64.0, 600 / 64.0))
tb.set_folder_path(root_for("Rooms"))
tb.set_actor_label("Room_Outside")
tag(tb, ["DARC.Room", "Id=Outside"])


# --- Редкие события ---
# Динамик в коридоре 2-го этажа (Морзе / шёпот / щелчок / радио — по таблице событий).
place(P["speaker"], 830, 1500, 1, yaw=0.0, z=zf(1) + 300, folder=GP, label="CorridorSpeaker",
      tags=["DARC.Anchor", "Events=Speaker", "Room=Corridor", "Behavior=SoundOnly"])
# Кружка в рабочей комнате: висит в сантиметре над столом, пока на неё не смотрят.
place(P["module"] if "mug" not in P else P["mug"], wu1 - 160, wv1 - 140, 1, z=zf(1) + 2 + size_of(P["table_old"])[2],
      folder=GP, label="LevitationMug", tags=["LevitationMug"]).static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
lv = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*W(wu1 - 160, wv1 - 140)) + unreal.Vector(0, 0, zf(1) + 120))
lv.set_folder_path(root_for(GP))
lv.set_actor_label("Anchor_Levitation")
tag(lv, ["DARC.Anchor", "Events=Levitation", "Room=WorkRoom", "Behavior=Levitate", "TargetTag=LevitationMug"])
# Исчезающая комната: шкаф в «Записях» стоит иначе, дверь не открывается картой.
rec = rect_of(1, RECORDS_ROOM)
place(P["cupboard"], (rec[0] + rec[2]) / 2, rec[3] - 60, 1, yaw=0.0, folder=GP, label="RecordsCabinet", tags=["RecordsCabinet"]).static_mesh_component.set_mobility(unreal.ComponentMobility.MOVABLE)
va = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*W((rec[0] + rec[2]) / 2, (rec[1] + rec[3]) / 2)) + unreal.Vector(0, 0, zf(1) + 100))
va.set_folder_path(root_for(GP))
va.set_actor_label("Anchor_VanishingRoom")
tag(va, ["DARC.Anchor", "Events=VanishingRoom", "Room=TechCabinetRoom", "Behavior=VanishingRoom", "Door=RecordsDoor",
         "TargetTag=RecordsCabinet"])
# Окно 3-го этажа над входом: при отъезде в тёмном окне загорается свет.
wl = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.TargetPoint, unreal.Vector(*W(2900, 150)) + unreal.Vector(0, 0, zf(2) + 220))
wl.set_folder_path(root_for(GP))
wl.set_actor_label("Anchor_WindowLight")
tag(wl, ["DARC.Anchor", "Events=WindowLight", "Behavior=LightOn"])

