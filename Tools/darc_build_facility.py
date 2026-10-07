# -*- coding: utf-8 -*-
"""
D.A.R.C. — сборка главного здания объекта (уровень 1) из модулей AbandonedPowerPlant.
Запуск в редакторе на карте 1LvlMap: консоль → py <путь>/Tools/darc_build_facility.py
Повторный запуск сносит всё в папке Outliner «Facility» и строит заново.

Главный корпус 40 × 40 м, 3 этажа (этаж 4.3 м): кольцо помещений вдоль фасадов, кольцевой
коридор, центральное ядро (лестницы, санузлы, серверная) и S-образный проход сквозь ядро,
ответвления к входу (юг) и к пристройке (запад). Пристройка 10 × 15 м, 2 этажа, на её крыше —
антенна; выход на крышу пристройки — дверью с 3-го этажа.

Планировка задаётся прямоугольниками помещений (PLAN). Стены строятся автоматически по
границам помещений на сетке 1 м; двери — между помещением и коридором (самая длинная общая
граница) и там, где перечислено явно (DOORS). Всё — в папках Outliner Facility/…
"""

import math
import random
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
rng = random.Random(1983)

ROOT = "Facility"
PP = "/Game/AbandonedPowerPlant/Assets/"
CUBE = "/Engine/BasicShapes/Cube.Cube"
PLANE = "/Engine/BasicShapes/Plane.Plane"
MAT = {
    "concrete": "/Game/DARC/Imported/Materials/M_concrete_wall_008",
    "concrete_dark": "/Game/DARC/Imported/Materials/M_concrete_layers",
    "floor_tiles": "/Game/DARC/Imported/Materials/M_floor_tiles_02",
    "linoleum": "/Game/DARC/Imported/Materials/M_old_linoleum_flooring_01",
    "ceiling": "/Game/DARC/Imported/Materials/M_Armstrong",
    "metal": "/Game/DARC/Imported/Materials/M_metal_plate",
    "plaster": "/Game/DARC/Imported/Materials/M_peeling_painted_wall",
    "glow": "/Game/DARC/Materials/MI_WindowGlow",
    "exit": "/Game/DARC/Materials/MI_ExitSign",
}

# --- Габариты (мировые см). Локальные координаты u (на восток), v (на север) — от ЮЗ угла. ---
XW, YS = -9800.0, 0.0
SIZE = 4000.0
FH, WALL_H, SLAB = 430.0, 400.0, 30.0
FLOORS = 3
ANNEX = (-1000.0, 1250.0, 0.0, 2750.0)   # u0, v0, u1, v1 (пристройка с запада)
ANNEX_FLOORS = 2


def W(u, v):
    return XW + u, YS + v


# --- Планировка: (имя, u0, v0, u1, v1). Имена коридоров начинаются с «Cor»/«Lobby»/«Hall». ---
COMMON = [
    # кольцевой коридор
    ("Cor", 800, 800, 3200, 1100), ("Cor", 800, 2900, 3200, 3200),
    ("Cor", 800, 1100, 1100, 2900), ("Cor", 2900, 1100, 3200, 2900),
    # S-образный проход сквозь ядро
    ("Cor", 1100, 1600, 2300, 1900), ("Cor", 2000, 1900, 2300, 2900),
    # ядро
    ("StairA", 1100, 1100, 2000, 1600), ("StairB", 1100, 1900, 2000, 2500),
    ("CoreCloset", 1100, 2500, 2000, 2900), ("CoreWC", 2000, 1100, 2900, 1600),
    ("CoreBlock", 2300, 1600, 2900, 2900),
    # ответвление на запад (к пристройке)
    ("Cor", 0, 1600, 800, 2000),
]
BANDS = [  # кольцо помещений вдоль фасадов (имена — по этажам ниже)
    ("SW", 0, 0, 800, 800), ("S1", 800, 0, 1600, 800), ("S2", 2000, 0, 3200, 800), ("SE", 3200, 0, 4000, 800),
    ("W1", 0, 800, 800, 1600), ("W2", 0, 2000, 800, 3200),
    ("E1", 3200, 800, 4000, 2000), ("E2", 3200, 2000, 4000, 3200),
    ("NW", 0, 3200, 800, 4000), ("N1", 800, 3200, 1600, 4000), ("N2", 1600, 3200, 2400, 4000),
    ("N3", 2400, 3200, 3200, 4000), ("NE", 3200, 3200, 4000, 4000),
]
NAMES = {
    0: {"SW": "Electrical", "S1": "AdminBlock", "S2": "Security", "SE": "Workshop", "W1": "Generator",
        "W2": "Utility", "E1": "Storage", "E2": "ToolRoom", "NW": "WC", "N1": "Archive", "N2": "Archive_B",
        "N3": "Garage_Office", "NE": "Boiler", "CoreBlock": "Switchgear", "Spur": "Lobby"},
    1: {"SW": "TechRoom", "S1": "WorkRoom", "S2": "Office_2A", "SE": "Lab", "W1": "UPS_Batteries",
        "W2": "Office_2B", "E1": "Meeting", "E2": "Records", "NW": "WC_2", "N1": "Office_2C", "N2": "Storage_2",
        "N3": "Office_2D", "NE": "Medical", "CoreBlock": "ServerSector", "Spur": "Hall_2"},
    2: {"SW": "SignalProcessing", "S1": "Laboratory", "S2": "Office_3A", "SE": "Ventilation", "W1": "Office_3B",
        "W2": "BreakRoom", "E1": "Archive_3", "E2": "Engineering", "NW": "WC_3", "N1": "Office_3C",
        "N2": "Darkroom", "N3": "Library", "NE": "Director", "CoreBlock": "Telemetry", "Spur": "Hall_3"},
}
# Явные двери: проходные комнаты (угловые — только через соседей), лестницы — со стороны площадки.
DOORS = [("SW", "W1"), ("SE", "E1"), ("NW", "W2"), ("NE", "E2"), ("N2", "N3"), ("S1", "Spur"),
         ("StairA", "Cor"), ("StairB", "Cor"), ("CoreWC", "Cor"), ("CoreCloset", "Cor")]
# Где у лестницы дверь (сторона площадки): A — запад, B — восток.
STAIR_DIR = {"StairA": ("u+", 1100.0), "StairB": ("u-", 2000.0)}
TECH = {"Electrical", "Generator", "TechRoom", "ServerSector", "UPS_Batteries", "Storage", "Storage_2", "Switchgear",
        "Ventilation", "Engineering", "Workshop", "ToolRoom", "AntennaEquipment", "Utility", "Boiler", "Telemetry",
        "CoreCloset", "Darkroom"}
# Office_3A — тёмное окно 3-го этажа над входом: при отъезде в нём загорается свет (редкое событие).
NEVER_LIT = {"Generator", "ServerSector", "UPS_Batteries", "StairB", "Switchgear", "Darkroom", "Office_3A"}
ALWAYS_LIT = {"Security", "Lobby", "StairA"}

loaded = {}


def asset(path):
    if path not in loaded:
        loaded[path] = unreal.load_asset(path)
    return loaded[path]


# Мебель, игровые объекты, детали фасада и зоны комнат — в отдельной папке: её автор двигает руками,
# скрипт её не сносит (создаёт заново, только если она пустая). Стены/полы/окна — в «Facility».
ROOT_PROPS = "FacilityProps"
PROPS_FOLDERS = ("Props", "Gameplay", "Rooms", "Main/Details")


def root_for(folder):
    if folder.startswith(PROPS_FOLDERS):
        return ROOT_PROPS + "/" + folder
    return ROOT + ("/" + folder if folder else "")


def put(mesh_path, loc, yaw=0.0, scale=(1, 1, 1), folder="", label="", mat=None, roll=0.0, collide=True, pitch=0.0):
    actor = sub.spawn_actor_from_object(asset(mesh_path), unreal.Vector(*loc), unreal.Rotator(roll, pitch, yaw))
    if not actor:
        return None
    actor.set_actor_scale3d(unreal.Vector(*scale))
    actor.set_folder_path(root_for(folder))
    if label:
        actor.set_actor_label(label)
    comp = actor.static_mesh_component
    # Декали площадки (лужи, мокрый асфальт) проецируются сверху и на стенах тянутся полосами.
    comp.set_editor_property("receives_decals", False)
    if mat:
        comp.set_material(0, asset(MAT.get(mat, mat)))
    if not collide:
        comp.set_collision_profile_name("NoCollision")
    return actor


def box(x0, y0, z0, x1, y1, z1, mat, folder, label, collide=True):
    cx, cy, cz = (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2
    sx, sy, sz = abs(x1 - x0) / 100, abs(y1 - y0) / 100, abs(z1 - z0) / 100
    if sx <= 0 or sy <= 0 or sz <= 0:
        return None
    return put(CUBE, (cx, cy, cz), scale=(sx, sy, sz), folder=folder, label=label, mat=mat, collide=collide)


def lbox(u0, v0, z0, u1, v1, z1, mat, folder, label, collide=True):
    x0, y0 = W(min(u0, u1), min(v0, v1))
    x1, y1 = W(max(u0, u1), max(v0, v1))
    return box(x0, y0, z0, x1, y1, z1, mat, folder, label, collide)


def rot2(x, y, yaw):
    a = math.radians(yaw)
    return x * math.cos(a) - y * math.sin(a), x * math.sin(a) + y * math.cos(a)


WALLS = {
    "plain300": ("Walls/SM_Wall_Plain_300", 300, None),
    "plain200": ("Walls/SM_Wall_Plain_200", 200, None),
    "plain100": ("Walls/SM_Wall_Plain_100", 100, None),
    "out300": ("Walls/SM_Wall_Plain_Outward_300", 300, None),
    "win": ("Walls/SM_Wall_Window_Medium_03", 300, (80, 220, 100, 300)),
    "win_small": ("Walls/SM_Wall_Window_Small_02", 300, (80, 220, 100, 250)),
    "win_tall": ("Walls/SM_Wall_Window_Tall_03", 300, (80, 220, 100, 350)),
    "door": ("Walls/SM_Wall_Door_300_01", 300, (80, 220, 0, 240)),
    "door200": ("Walls/SM_Wall_Door_200", 200, (30, 170, 0, 240)),
}
WINDOW_FRAME = {"win": "Windows/SM_Window_Medium_01", "win_small": "Windows/SM_Window_Small_01",
                "win_tall": "Windows/SM_Window_Tall_01"}


def wall_piece(kind, px, py, z, yaw, folder, label, stretch=1.0, lit=False):
    path, width, opening = WALLS[kind]
    put(PP + path, (px, py, z), yaw=yaw, scale=(stretch, 1, 1), folder=folder, label=label)
    frame = WINDOW_FRAME.get(kind)
    if frame:
        ox, oy = rot2(opening[0] * stretch - 5, -10, yaw)
        put(PP + frame, (px + ox, py + oy, z + opening[2]), yaw=yaw, folder=folder, label=label + "_Frame")
        if lit:
            # Тёплый свет за стеклом: односторонняя плоскость у внутренней грани, лицом наружу —
            # снаружи окно «горит», изнутри плоскости не видно (было: блок, заклеивал окно изнутри).
            cx, cy = rot2((opening[0] + opening[1]) / 2 * stretch, -19, yaw)
            g = put(PLANE, (px + cx, py + cy, z + (opening[2] + opening[3]) / 2), yaw=yaw, roll=90.0,
                    scale=((opening[1] - opening[0]) / 100 * stretch, (opening[3] - opening[2]) / 100, 1),
                    folder=folder, label=label + "_Glow", mat="glow", collide=False)
            if g:
                g.static_mesh_component.set_cast_shadow(False)


def wall_run(x0, y0, x1, y1, z, kinds, folder, label, lit=None):
    """Стена от (x0,y0) до (x1,y1); толщина — слева по ходу. kinds — модули, растягиваются под длину."""
    yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
    length = math.hypot(x1 - x0, y1 - y0)
    stretch = length / sum(WALLS[k][1] for k in kinds)
    dx, dy = (x1 - x0) / length, (y1 - y0) / length
    pos = 0.0
    for i, kind in enumerate(kinds):
        wall_piece(kind, x0 + dx * pos, y0 + dy * pos, z, yaw, folder, "%s_%02d" % (label, i), stretch,
                   lit=bool(lit and lit(i)))
        pos += WALLS[kind][1] * stretch


def fill(length):
    """Глухие модули ровно на длину (последний — растянут)."""
    kinds, left = [], length
    while left > 1:
        k = "plain300" if left >= 300 else ("plain200" if left >= 200 else "plain100")
        kinds.append(k)
        left -= WALLS[k][1]
    return kinds or ["plain100"]


def zf(f):
    return Z0 + f * FH


# --- Подготовка ---
def clear():
    # Одним пакетом: по одному с записью в историю отмены на тысячах объектов редактор уходил в своп.
    # Створки дверей ставит наполнение (darc_build_facility_props.py, один раз) — их не трогаем,
    # иначе пересборка коробки оставляла здание без дверей (наполнение повторно не запускается).
    doomed = [a for a in sub.get_all_level_actors()
              if (str(a.get_folder_path()) == ROOT or str(a.get_folder_path()).startswith(ROOT + "/"))
              and "/Doors" not in str(a.get_folder_path())]
    if doomed:
        sub.destroy_actors(doomed)
    unreal.log("[DARC] step: cleared %d" % len(doomed))


def ground_z(x, y):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 20000), unreal.Vector(x, y, -20000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4].z if hit else None


def ensure_emissive(name, color, intensity):
    path = "/Game/DARC/Materials/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, "/Game/DARC/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", unreal.load_asset("/Game/DARC/Materials/M_DarcEmissive"))
    unreal.MaterialEditingLibrary.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "Intensity", intensity)
    eal.save_loaded_asset(mi)


def ensure_tiled(name, parent, tile):
    """Вариант материала из darc_setup с другим шагом рисунка (TileSize, см)."""
    path = "/Game/DARC/Materials/" + name
    if eal.does_asset_exist(path):
        mi = unreal.load_asset(path)
    else:
        mi = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, "/Game/DARC/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", unreal.load_asset(parent))
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "TileSize", tile)
    eal.save_loaded_asset(mi)
    return path


clear()
# Линолеум из пака крупный (рисунок ~2 м) — в кабинетах мельче.
MAT["linoleum"] = ensure_tiled("MI_Linoleum_Small", "/Game/DARC/Imported/Materials/M_old_linoleum_flooring_01", 70.0)
ensure_emissive("MI_WindowGlow", (1.0, 0.5, 0.2, 1), 0.55)
ensure_emissive("MI_ExitSign", (0.1, 1.0, 0.25, 1), 3.0)
samples = [ground_z(x, y) for x in range(int(XW + ANNEX[0]) - 300, int(XW + SIZE) + 300, 500)
           for y in range(int(YS) - 900, int(YS + SIZE) + 300, 500)]
samples = [s for s in samples if s is not None]
G_MIN, G_MAX = min(samples), max(samples)
Z0 = G_MAX + 60.0
unreal.log("[DARC] Facility: ground %.0f..%.0f, floor0 %.0f" % (G_MIN, G_MAX, Z0))


# --- Сетка помещений 1 м ---
N = int(SIZE / 100)


def plan(f):
    """Список (метка, u0, v0, u1, v1) этажа: общее + кольцо помещений + ответвление на юг."""
    rects = list(COMMON)
    for key, u0, v0, u1, v1 in BANDS:
        rects.append((NAMES[f][key], u0, v0, u1, v1))
    # Южное ответвление: вестибюль у входа (1 эт.) / холл с окном (2-3 эт.).
    rects.append((NAMES[f]["Spur"], 1600, 0, 2000, 800))
    return [(NAMES[f].get(r[0], r[0]),) + tuple(r[1:]) for r in rects]


def grid(f):
    g = [[None] * N for _ in range(N)]
    for name, u0, v0, u1, v1 in plan(f):
        for i in range(int(u0 // 100), int(u1 // 100)):
            for j in range(int(v0 // 100), int(v1 // 100)):
                g[i][j] = name
    return g


def is_corridor(name):
    # Только «Cor» целиком: CoreWC / CoreCloset — помещения ядра, не коридор.
    return name is not None and (name == "Cor" or name.startswith("Lobby") or name.startswith("Hall"))


def boundaries(g):
    """Границы между разными помещениями: {(A,B) отсортировано: [(ось, линия, от, до)]}."""
    runs = {}

    def add(pair, axis, line, a, b):
        lst = runs.setdefault(pair, [])
        if lst and lst[-1][0] == axis and lst[-1][1] == line and lst[-1][3] == a:
            lst[-1] = (axis, line, lst[-1][2], b)
        else:
            lst.append((axis, line, a, b))

    for a in range(1, N):            # вертикальные границы u = a*100
        for j in range(N):
            A, B = g[a - 1][j], g[a][j]
            if A and B and A != B and not (is_corridor(A) and is_corridor(B)):
                add(tuple(sorted((A, B))), "u", a * 100.0, j * 100.0, (j + 1) * 100.0)
    for b in range(1, N):            # горизонтальные v = b*100
        for i in range(N):
            A, B = g[i][b - 1], g[i][b]
            if A and B and A != B and not (is_corridor(A) and is_corridor(B)):
                add(tuple(sorted((A, B))), "v", b * 100.0, i * 100.0, (i + 1) * 100.0)
    return runs


def label_key(f, name):
    """Обратно к ключу планировки (SW, S1 …) — для явных дверей."""
    for k, v in NAMES[f].items():
        if v == name:
            return k
    return name


def choose_doors(f, runs):
    """Какие отрезки границы получают дверь: по двери на помещение к коридору + явные."""
    doors = set()
    names = {r[0] for r in plan(f)}
    for name in names:
        if is_corridor(name) or name.startswith("Stair") or name.startswith("Core"):
            continue
        best = None
        for pair, lst in runs.items():
            if name in pair:
                other = pair[0] if pair[1] == name else pair[1]
                if is_corridor(other):
                    for r in lst:
                        if best is None or r[3] - r[2] > best[1][3] - best[1][2]:
                            best = (pair, r)
        if best:
            doors.add((best[0], best[1]))
    for a, b in DOORS:
        for pair, lst in runs.items():
            ka, kb = label_key(f, pair[0]), label_key(f, pair[1])
            match = {ka, kb} == {a, b} or ({a, b} - {"Cor"} <= {ka, kb} and "Cor" in (a, b)
                                           and any(is_corridor(p) for p in pair))
            if not match:
                continue
            cand = [r for r in lst if r[3] - r[2] >= 200]
            if a in STAIR_DIR:   # лестница: только сторона площадки
                axis_line = STAIR_DIR[a][1]
                cand = [r for r in cand if r[0] == "u" and abs(r[1] - axis_line) < 1]
            if cand:
                doors.add((pair, max(cand, key=lambda r: r[3] - r[2])))
    return doors


# Низкие проёмы в стенах (вентиляция): (этаж, ось, линия) → (от, до вдоль стены, высота проёма).
# Обход в серверную: из восточного коридора 2-го этажа через короб, лезть пригнувшись (Ctrl).
# Решётку (DARC.Cover) ставит Tools/darc_add_mechanics.py.
VENT_LINE, VENT_V0, VENT_V1, VENT_H, VENT_LEN = 2900.0, 1700.0, 1800.0, 100.0, 110.0
# Автор 07.10.2026: вентиляции в выезде 1 нет (переносится на другой уровень) — проёмов нет.
WALL_GAPS = {}


def wall_gap_lintel(axis, line, g0, g1, gh, z, folder, label):
    """Перемычка над проёмом — тот же модуль стены, сжатый по высоте."""
    if axis == "u":
        x, y = W(line - 10, g0)
        yaw = 90.0
    else:
        x, y = W(g0, line + 10)
        yaw = 0.0
    put(PP + WALLS["plain100"][0], (x, y, z + gh), yaw=yaw, scale=((g1 - g0) / 100.0, 1, (WALL_H - gh) / WALL_H),
        folder=folder, label=label)


def build_vent_duct(f):
    """Короб вентиляции со стороны серверной: две стенки и крышка над проёмом, выход — в комнату."""
    z = zf(f)
    u1 = VENT_LINE - 10
    u0 = u1 - VENT_LEN
    folder = "Main/F%d/Vent" % (f + 1)
    lbox(u0, VENT_V0 - 6, z, u1, VENT_V0, z + VENT_H + 6, "metal", folder, "VentDuct_S")
    lbox(u0, VENT_V1, z, u1, VENT_V1 + 6, z + VENT_H + 6, "metal", folder, "VentDuct_N")
    lbox(u0, VENT_V0 - 6, z + VENT_H, u1, VENT_V1 + 6, z + VENT_H + 6, "metal", folder, "VentDuct_Top")


def build_walls(f):
    g = grid(f)
    runs = boundaries(g)
    doors = choose_doors(f, runs)
    z = zf(f)
    folder = "Main/F%d/Walls" % (f + 1)
    n = 0
    for pair, lst in runs.items():
        for r in lst:
            axis, line, a, b = r
            length = b - a
            gap = WALL_GAPS.get((f, axis, line))
            if gap and a < gap[0] and gap[1] < b and (pair, r) not in doors:
                # Стена с низким проёмом: глухие куски по сторонам и перемычка над проёмом.
                g0, g1, gh = gap
                base = "W_%s_%s_%03d" % (pair[0][:10], pair[1][:10], n)
                for part, (p0, p1) in enumerate(((a, g0), (g1, b))):
                    if axis == "u":
                        x0, y0 = W(line - 10, p0)
                        x1, y1 = W(line - 10, p1)
                    else:
                        x0, y0 = W(p0, line + 10)
                        x1, y1 = W(p1, line + 10)
                    wall_run(x0, y0, x1, y1, z, fill(p1 - p0), folder, "%s_%d" % (base, part))
                wall_gap_lintel(axis, line, g0, g1, gh, z, folder, base + "_VentLintel")
                n += 1
                continue
            if (pair, r) in doors:
                dk = "door" if length >= 300 else "door200"
                side = (length - WALLS[dk][1]) / 2
                kinds = (fill(side) if side > 1 else []) + [dk] + (fill(side) if side > 1 else [])
            else:
                kinds = fill(length)
            # Толщина 20 см по центру линии.
            if axis == "u":
                x0, y0 = W(line - 10, a)
                x1, y1 = W(line - 10, b)
            else:
                x0, y0 = W(a, line + 10)
                x1, y1 = W(b, line + 10)
            wall_run(x0, y0, x1, y1, z, kinds, folder, "W_%s_%s_%03d" % (pair[0][:10], pair[1][:10], n))
            if (pair, r) in doors:
                # Где оказался дверной модуль (с учётом растяжения) — для створки и устройств у двери.
                widths = [WALLS[k][1] for k in kinds]
                stretch = length / sum(widths)
                i = kinds.index(dk)
                offset = sum(widths[:i]) * stretch
                yaw = math.degrees(math.atan2(y1 - y0, x1 - x0))
                d = ((x1 - x0) / length, (y1 - y0) / length)
                DOOR_LIST.append({"f": f, "pair": pair, "axis": axis, "line": line, "kind": dk, "yaw": yaw,
                                  "pivot": (x0 + d[0] * offset, y0 + d[1] * offset, z), "stretch": stretch,
                                  "center": a + offset + (WALLS[dk][2][0] + WALLS[dk][2][1]) / 2 * stretch})
            n += 1
    return g


DOOR_LIST = []   # все внутренние двери (заполняет build_walls)


# --- Свет: какие помещения освещены ---
LIT = {}
for f in range(FLOORS):
    for r in plan(f) + [("AntennaEquipment" if f == 0 else "AntennaControl", 0, 0, 0, 0)]:
        name = r[0]
        if (f, name) in LIT:
            continue
        LIT[(f, name)] = name in ALWAYS_LIT or (name not in NEVER_LIT and rng.random() < (0.55 if is_corridor(name) else 0.35))


unreal.log("[DARC] step: slabs")
# --- Перекрытия с проёмами лестниц ---
STAIR_RECT = {"StairA": (1100, 1100, 2000, 1600), "StairB": (1100, 1900, 2000, 2500)}
STEPS, RUN = 12, 28.0
RISE = FH / (2 * STEPS)


def stair_hole(name):
    u0, v0, u1, v1 = STAIR_RECT[name]
    d, _ = STAIR_DIR[name]
    arrive = 380.0
    return (u0 + arrive, v0 + 20, u1 - 20, v1 - 20) if d == "u+" else (u0 + 20, v0 + 20, u1 - arrive, v1 - 20)


def rect_minus(rect, holes):
    x0, y0, x1, y1 = rect
    xs = sorted({x0, x1} | {h[0] for h in holes if x0 < h[0] < x1} | {h[2] for h in holes if x0 < h[2] < x1})
    out = []
    for a, b in zip(xs, xs[1:]):
        mid = (a + b) / 2
        cuts = sorted([(h[1], h[3]) for h in holes if h[0] <= mid <= h[2]])
        y = y0
        for c0, c1 in cuts:
            if c0 > y:
                out.append((a, y, b, c0))
            y = max(y, c1)
        if y < y1:
            out.append((a, y, b, y1))
    return out


for f in range(FLOORS + 1):
    holes = []
    if 0 < f < FLOORS:
        holes = [stair_hole("StairA"), stair_hole("StairB")]
    elif f == FLOORS:
        holes = [stair_hole("StairB")]          # выход на крышу — по лестнице B
    folder = "Main/Roof" if f == FLOORS else "Main/F%d/Slab" % (f + 1)
    for i, r in enumerate(rect_minus((0, 0, SIZE, SIZE), holes)):
        lbox(r[0], r[1], zf(f) - SLAB, r[2], r[3], zf(f), "concrete", folder, "Slab_%02d" % i)
    if f <= ANNEX_FLOORS:
        u0, v0, u1, v1 = ANNEX
        lbox(u0, v0, zf(f) - SLAB, u1, v1, zf(f), "concrete", "Annex/Roof" if f == ANNEX_FLOORS else "Annex/F%d" % (f + 1), "AnnexSlab")
lbox(-10, -10, G_MIN - 50, SIZE + 10, SIZE + 10, Z0 - SLAB, "concrete_dark", "Main/Base", "Plinth")
lbox(ANNEX[0] - 10, ANNEX[1] - 10, G_MIN - 50, 0, ANNEX[3] + 10, Z0 - SLAB, "concrete_dark", "Annex", "Plinth")


unreal.log("[DARC] step: walls")
# --- Внутренние стены, полы, потолки ---
GRIDS = []
for f in range(FLOORS):
    g = build_walls(f)
    if f == 1 and WALL_GAPS:
        build_vent_duct(f)
    GRIDS.append(g)
    z = zf(f)
    folder = "Main/F%d/Rooms" % (f + 1)
    for name, u0, v0, u1, v1 in plan(f):
        if name.startswith("Stair"):
            continue
        floor_mat = "floor_tiles" if is_corridor(name) or name.startswith("WC") or name == "CoreWC" else (
            "concrete_dark" if name in TECH else "linoleum")
        lbox(u0 + 10, v0 + 10, z, u1 - 10, v1 - 10, z + 2, floor_mat, folder, name + "_Floor")
        if name not in TECH:
            lbox(u0 + 10, v0 + 10, z + WALL_H - 4, u1 - 10, v1 - 10, z + WALL_H - 2, "ceiling", folder,
                 name + "_Ceiling", collide=False)


def room_at(f, u, v):
    if f >= FLOORS or u < 0 or v < 0 or u >= SIZE or v >= SIZE:
        return None
    return GRIDS[f][int(u // 100)][int(v // 100)]


unreal.log("[DARC] step: facades")
# --- Фасады: модуль 4 м = простенок 1 м + окно 3 м. Двери — по списку. ---
FACADE_DOORS = {  # (сторона, модуль, этаж)
    ("S", 4, 0),          # главный вход — в вестибюль
    ("W", 4, 0), ("W", 4, 1), ("W", 4, 2),   # в пристройку / на её крышу
    ("N", 7, 0),          # служебный выход
    ("E", 2, 0),          # выход из мастерской
}
BLIND = {("S", 0, 0), ("W", 1, 0), ("W", 2, 0), ("N", 0, 1), ("E", 5, 2), ("N", 5, 2)}   # глухие участки
SMALL = {"WC", "WC_2", "WC_3", "Utility", "Boiler", "Darkroom", "Electrical"}


def facade(side, f):
    z = zf(f)
    folder = "Main/F%d/Facade" % (f + 1)
    for m in range(10):
        # Модуль m по ходу обхода; ход — так, чтобы толщина стены была внутри.
        if side == "S":
            px, py, yaw = W(SIZE - m * 400, 0)[0], YS, 180.0
            probe = (SIZE - m * 400 - 200, 50)
            mod = 9 - m
        elif side == "N":
            px, py, yaw = W(m * 400, SIZE)[0], YS + SIZE, 0.0
            probe = (m * 400 + 200, SIZE - 50)
            mod = m
        elif side == "W":
            px, py, yaw = XW, W(0, m * 400)[1], 90.0
            probe = (50, m * 400 + 200)
            mod = m
        else:
            px, py, yaw = XW + SIZE, W(0, SIZE - m * 400)[1], -90.0
            probe = (SIZE - 50, SIZE - m * 400 - 200)
            mod = 9 - m
        room = room_at(f, *probe)
        if (side, mod, f) in FACADE_DOORS:
            kind = "door"
        elif (side, mod, f) in BLIND:
            kind = "out300"
        elif side == "W" and f < ANNEX_FLOORS and 3 <= mod <= 6:
            kind = "out300"            # стык с пристройкой
        elif room in SMALL:
            kind = "win_small"
        elif room and room.startswith("Hall"):
            kind = "win_tall"
        else:
            kind = "win"
        lit = LIT.get((f, room), False)
        # Простенок 1 м, затем окно/дверь 3 м (по ходу).
        dx, dy = rot2(100, 0, yaw)
        wall_piece("plain100", px, py, z, yaw, folder, "%s_%d_Pier" % (side, mod))
        wall_piece(kind, px + dx, py + dy, z, yaw, folder, "%s_%d" % (side, mod), lit=lit)
        if kind == "door":
            DOOR_LIST.append({"f": f, "pair": ("Outside", room or ""), "axis": "facade", "line": 0, "kind": "door",
                              "yaw": yaw, "pivot": (px + dx, py + dy, z), "stretch": 1.0, "center": 0, "side": side})
        if side in ("S", "N") and m % 2 == 0:
            put(PP + "Walls/SM_Pillar_Half_01", (px, py, z), yaw=yaw,
                folder=folder, label="%s_%d_Pilaster" % (side, mod))


for f in range(FLOORS):
    for side in "SNWE":
        facade(side, f)
    if f > 0:   # межэтажные пояса
        z = zf(f)
        lbox(-6, -6, z - SLAB, SIZE + 6, 1, z, "concrete", "Main/F%d/Facade" % (f + 1), "Belt_S")
        lbox(-6, SIZE - 1, z - SLAB, SIZE + 6, SIZE + 6, z, "concrete", "Main/F%d/Facade" % (f + 1), "Belt_N")
        lbox(SIZE - 1, 0, z - SLAB, SIZE + 6, SIZE, z, "concrete", "Main/F%d/Facade" % (f + 1), "Belt_E")

# Карниз и парапет.
zr = zf(FLOORS)
for (u0, v0, u1, v1, name) in [(-20, -20, SIZE + 20, 25, "S"), (-20, SIZE - 25, SIZE + 20, SIZE + 20, "N"),
                               (-20, 0, 25, SIZE, "W"), (SIZE - 25, 0, SIZE + 20, SIZE, "E")]:
    lbox(u0, v0, zr - SLAB - 25, u1, v1, zr, "concrete", "Main/Roof", "Cornice_" + name)
    lbox(u0, v0, zr, u1, v1, zr + 90, "concrete", "Main/Roof", "Parapet_" + name)

# Вход: крыльцо, ступени, козырёк на опорах (модуль 4 южного фасада).
eu0, eu1 = 1600 - 100, 2000 + 100
lbox(eu0, -160, G_MIN - 50, eu1, 0, Z0, "concrete_dark", "Main/Entrance", "Porch")
steps = max(1, int(math.ceil((Z0 - G_MIN) / 18.0)))
for i in range(1, steps + 1):
    lbox(eu0, -160 - i * 32, G_MIN - 50, eu1, -160, Z0 - i * (Z0 - G_MIN) / (steps + 1), "concrete_dark",
         "Main/Entrance", "Step_%d" % i)
lbox(eu0 - 60, -330, Z0 + 330, eu1 + 60, 0, Z0 + 352, "concrete", "Main/Entrance", "Canopy")
for u in (eu0 - 30, eu1 + 30):
    lbox(u - 12, -310, Z0, u + 12, -286, Z0 + 330, "metal", "Main/Entrance", "CanopyPost")


unreal.log("[DARC] step: annex")
# --- Пристройка (запад): 2 этажа, на крыше — антенна ---
au0, av0, au1, av1 = ANNEX
for f in range(ANNEX_FLOORS):
    z = zf(f)
    folder = "Annex/F%d" % (f + 1)
    name = "AntennaEquipment" if f == 0 else "AntennaControl"
    lit = lambda i, f=f, name=name: LIT.get((f, name), False) and i % 2 == 1
    # Юг (с востока на запад), запад (с юга на север), север (с запада на восток).
    xa0, ya0 = W(au0, av0)
    xa1, ya1 = W(au1, av1)
    wall_run(xa1, ya0, xa0, ya0, z, ["plain100", "win", "plain200", "win", "plain100"], folder, "South", lit)
    wall_run(xa0, ya0, xa0, ya1, z, ["plain100", "win", "plain100", "out300" if f == 0 else "win",
                                     "plain100", "win", "plain100", "plain100", "plain100"], folder, "West", lit)
    wall_run(xa0, ya1, xa1, ya1, z, ["plain100", "door" if f == 0 else "win", "plain200", "win", "plain100"],
             folder, "North", lit)
    lbox(au0 + 10, av0 + 10, z, -10, av1 - 10, z + 2, "concrete_dark", folder, name + "_Floor")
# Парапет крыши пристройки — площадка антенны (на уровне 3-го этажа главного корпуса).
za = zf(ANNEX_FLOORS)
for (u0, v0, u1, v1, nm) in [(au0 - 20, av0 - 20, 0, av0 + 25, "S"), (au0 - 20, av1 - 25, 0, av1 + 20, "N"),
                             (au0 - 20, av0, au0 + 25, av1, "W")]:
    lbox(u0, v0, za - SLAB - 25, u1, v1, za, "concrete", "Annex/Roof", "Cornice_" + nm)
    lbox(u0, v0, za, u1, v1, za + 100, "concrete", "Annex/Roof", "Parapet_" + nm)


# Антенна на крыше пристройки: тарелка (склеена из GLB — Tools/blender_join_glb.py), бетонный
# постамент, кабельный лоток к главному корпусу, вертикальная лестница обслуживания по западной стене.
DISH = "/Game/DARC/Imported/Meshes/SatelliteDish/SM_SatelliteDish"
dcx, dcy = W((au0 + au1) / 2, (av0 + av1) / 2)
lbox(au0 + 60, av0 + 120, za, au1 - 60, av1 - 120, za + 40, "concrete_dark", "Annex/Antenna", "DishPedestal")
if eal.does_asset_exist(DISH):
    put(DISH, (dcx, dcy, za + 40), yaw=200.0, scale=(1.2, 1.2, 1.2), folder="Annex/Antenna", label="SatelliteDish")
lbox(-260, (av0 + av1) / 2 - 20, za + 5, au0 + 500, (av0 + av1) / 2 + 20, za + 20, "metal", "Annex/Antenna", "CableTray")
lx, ly = W(au0, av0 + 300)
for i in range(int(math.ceil((za + 100 - G_MIN) / 200.0))):
    put("/Game/SD_Art/Industrial_Infrastructure/Assets/Ladders/SM_Ladder_Module_1", (lx - 15, ly, G_MIN + i * 200),
        yaw=180.0, folder="Annex/Antenna", label="ServiceLadder_%d" % i)


unreal.log("[DARC] step: stairs")
# --- Лестницы (двухмаршевые, марши вдоль u) ---
def stair(name, f):
    u0, v0, u1, v1 = STAIR_RECT[name]
    d, _ = STAIR_DIR[name]
    sgn = 1.0 if d == "u+" else -1.0
    start = u0 + 380.0 if d == "u+" else u1 - 380.0     # конец площадки у двери
    width = (v1 - v0) - 40
    half = (width - 20) / 2
    va, vb = v0 + 20, v0 + 20 + half                     # марш 1
    vc, vd = vb + 20, v1 - 20                            # марш 2
    z = zf(f)
    folder = "Main/Stairs/%s" % name
    # Ступени — плитой по наклону (низ марша параллелен ступеням). Были сплошными до пола: марш
    # этажом выше висел над нижним плоским низом, и у верха нижнего марша голова упиралась в него.
    under = RISE + 25.0
    for i in range(STEPS):
        a = start + sgn * i * RUN
        top = z + (i + 1) * RISE
        lbox(a, va, max(z - SLAB, top - under), a + sgn * RUN, vb, top, "concrete", folder, "F%d_M1_%02d" % (f + 1, i))
    land = start + sgn * STEPS * RUN
    end = u1 - 20 if d == "u+" else u0 + 20
    lbox(land, v0 + 20, z + STEPS * RISE - SLAB, end, v1 - 20, z + STEPS * RISE, "concrete", folder, "F%d_Landing" % (f + 1))
    for i in range(STEPS):
        a = land - sgn * i * RUN
        top = z + (STEPS + i + 1) * RISE
        lbox(a, vc, top - under, a - sgn * RUN, vd, top, "concrete", folder, "F%d_M2_%02d" % (f + 1, i))
    lbox(start, vb, z - SLAB, land, vc, z + FH + 100, "plaster", folder, "F%d_Divider" % (f + 1))


def top_rail(name, z):
    u0, v0, u1, v1 = STAIR_RECT[name]
    d, _ = STAIR_DIR[name]
    a = u0 + 380.0 if d == "u+" else u1 - 380.0
    half = ((v1 - v0) - 60) / 2
    lbox(a - 10, v0 + 20, z, a + 10, v0 + 20 + half, z + 100, "plaster", "Main/Stairs/%s" % name, "TopRail")


for f in range(FLOORS - 1):
    stair("StairA", f)
top_rail("StairA", zf(FLOORS - 1))
for f in range(FLOORS):
    stair("StairB", f)
top_rail("StairB", zf(FLOORS))

# Будка выхода на крышу над лестницей B (дверь на восток).
hu0, hv0, hu1, hv1 = STAIR_RECT["StairB"]
x0, y0 = W(hu0, hv0)
x1, y1 = W(hu1, hv1)
wall_run(x1, y0, x0, y0, zr, fill(hu1 - hu0), "Main/Roof", "Hut_S")
wall_run(x0, y1, x1, y1, zr, fill(hu1 - hu0), "Main/Roof", "Hut_N")
wall_run(x0, y0, x0, y1, zr, fill(hv1 - hv0), "Main/Roof", "Hut_W")
wall_run(x1, y1, x1, y0, zr, ["plain200", "door200", "plain200"], "Main/Roof", "Hut_E")
lbox(hu0 - 20, hv0 - 20, zr + WALL_H, hu1 + 20, hv1 + 20, zr + WALL_H + 25, "concrete", "Main/Roof", "Hut_Roof")


unreal.log("[DARC] step: lights")
# --- Светильники и свет ---
LAMP = "/Game/HospitalCorridor/Meshes/Objects/SM_HospitalLamps_01_B01"
lights_made = 0


INTERIOR_LIGHT_SCALE = 0.25   # автор: «вся яркость в доме в 4 раза меньше»


def lamp(u, v, z_floor, folder, label, on, warm=True, lumens=320.0, radius=850.0, yaw=0.0, circuit=None):
    """Светильник. circuit — лампа на электроцепи (разметка DARC.Lamp): свет создаёт игра, гаснет с цепью."""
    lumens *= INTERIOR_LIGHT_SCALE
    global lights_made
    x, y = W(u, v)
    m = put(LAMP, (x, y, z_floor + WALL_H - 6), yaw=yaw + 90.0, roll=180.0, folder=folder, label=label, collide=False)
    if circuit and m:
        m.tags = [unreal.Name(t) for t in ("DARC.Lamp", "Circuit=" + circuit, "Lumens=%d" % lumens)]
        return
    if not on:
        return
    light = sub.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z_floor + WALL_H - 40))
    light.set_folder_path(root_for(folder))
    light.set_actor_label(label + "_Light")
    comp = light.light_component
    comp.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    comp.set_intensity(lumens)
    comp.set_attenuation_radius(radius)
    comp.set_light_color(unreal.LinearColor(1.0, 0.8, 0.6, 1) if warm else unreal.LinearColor(0.85, 0.92, 1.0, 1))
    comp.set_cast_shadows(True)   # без теней свет проходит сквозь перекрытия и светит на крышу
    comp.set_volumetric_scattering_intensity(0.0)   # иначе свет рассеивается в тумане над крышей пятнами
    # Дальние лампы не рисуются (за стенами их не видно) — каждая с тенями, это главная нагрузка.
    comp.set_editor_property("max_draw_distance", 3000.0)
    comp.set_editor_property("max_distance_fade_range", 600.0)
    lights_made += 1


for f in range(FLOORS):
    z = zf(f)
    folder = "Main/F%d/Lights" % (f + 1)
    for name, u0, v0, u1, v1 in plan(f):
        if name.startswith("Stair"):
            continue
        on = LIT.get((f, name), False)
        if is_corridor(name):
            length = max(u1 - u0, v1 - v0)
            count = max(1, int(length // 700))
            for i in range(count):
                t = (i + 0.5) / count
                u = u0 + (u1 - u0) * (t if u1 - u0 >= v1 - v0 else 0.5)
                v = v0 + (v1 - v0) * (t if v1 - v0 > u1 - u0 else 0.5)
                works = on and rng.random() < 0.6
                lamp(u, v, z, folder, "%s_%d_%d_Lamp" % (name, int(u0), i), works, warm=False,
                     lumens=220.0, radius=900.0, yaw=0.0 if u1 - u0 >= v1 - v0 else 90.0,
                     circuit="Corridor" if (f == 1 and works) else None)
        else:
            # 2-й этаж (маршрут выезда): лампы на цепях — щиток гасит коридор, серверная загорается от питания.
            circuit = None
            if f == 1 and name == NAMES[1]["CoreBlock"]:
                circuit = "ServerLights"   # свет серверной — отдельный ввод (перегрузка генератора)
            elif f == 1 and on:
                circuit = "Building"
            lamp((u0 + u1) / 2, (v0 + v1) / 2, z, folder, name + "_Lamp", on, warm=name not in TECH,
                 lumens=260.0 if name in TECH else 340.0, circuit=circuit)
    for name in ("StairA", "StairB"):
        u0, v0, u1, v1 = STAIR_RECT[name]
        lamp((u0 + u1) / 2, (v0 + v1) / 2, z, folder, name + "_Lamp", LIT.get((f, name), False), warm=False, lumens=200.0)
        # Табличка «выход» над дверью лестницы (со стороны коридора).
        d, line = STAIR_DIR[name]
        side = -12 if d == "u+" else 12
        x, y = W(line + side, (v0 + v1) / 2)
        box(x - 2, y - 30, z + 255, x + 2, y + 30, z + 280, "exit", folder, name + "_ExitSign", collide=False)
for f in range(ANNEX_FLOORS):
    name = "AntennaEquipment" if f == 0 else "AntennaControl"
    lamp((ANNEX[0] + ANNEX[2]) / 2, (ANNEX[1] + ANNEX[3]) / 2, zf(f), "Annex/F%d" % (f + 1), name + "_Lamp",
         LIT.get((f, name), False), warm=False)

def beam(p0, p1, t, mat, folder, label):
    """Балка-брус между двумя мировыми точками (толщина t)."""
    dx, dy, dz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
    length = math.sqrt(dx * dx + dy * dy + dz * dz)
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    c = ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2, (p0[2] + p1[2]) / 2)
    return put(CUBE, c, yaw=yaw, pitch=pitch, scale=(length / 100, t / 100, t / 100), folder=folder, label=label, mat=mat)


def point_light(loc, folder, label, color, lumens, radius, shadows=False):
    light = sub.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc))
    light.set_folder_path(root_for(folder))
    light.set_actor_label(label)
    comp = light.light_component
    comp.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    comp.set_intensity(lumens)
    comp.set_attenuation_radius(radius)
    comp.set_light_color(unreal.LinearColor(*color))
    comp.set_cast_shadows(shadows)
    return light


ensure_emissive("MI_RedBeacon", (1.0, 0.05, 0.03, 1), 8.0)
MAT["beacon"] = "/Game/DARC/Materials/MI_RedBeacon"
MAT["rust"] = "/Game/DARC/Imported/Materials/M_rusty_metal_05"
MAT["corrugated"] = "/Game/DARC/Imported/Materials/M_corrugated_iron_02"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
CYL = "/Engine/BasicShapes/Cylinder.Cylinder"

unreal.log("[DARC] step: tower")
# --- Вышка связи (СВ угол площадки): решётчатая, 26 м, площадка, мачты, красные огни ---
TX, TY = -3800.0, 3650.0
TH, B0, B1, PANELS = 2600.0, 220.0, 70.0, 10
tg = ground_z(TX, TY) or G_MIN
lbox(TX - XW - 260, TY - YS - 260, tg - 30, TX - XW + 260, TY - YS + 260, tg + 30, "concrete_dark", "Tower", "Foundation")


def tower_pt(sx, sy, h):
    half = B0 + (B1 - B0) * h / TH
    return (TX + sx * half, TY + sy * half, tg + 30 + h)


corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
for i, (sx, sy) in enumerate(corners):
    beam(tower_pt(sx, sy, 0), tower_pt(sx, sy, TH), 14, "rust", "Tower", "Leg_%d" % i)
for p in range(PANELS + 1):
    h = TH * p / PANELS
    for i in range(4):
        a, b = corners[i], corners[(i + 1) % 4]
        beam(tower_pt(a[0], a[1], h), tower_pt(b[0], b[1], h), 7, "rust", "Tower", "Ring_%02d_%d" % (p, i))
        if p < PANELS:
            h2 = TH * (p + 1) / PANELS
            beam(tower_pt(a[0], a[1], h), tower_pt(b[0], b[1], h2), 5, "rust", "Tower", "Brace_%02d_%d_a" % (p, i))
            beam(tower_pt(b[0], b[1], h), tower_pt(a[0], a[1], h2), 5, "rust", "Tower", "Brace_%02d_%d_b" % (p, i))
top = tg + 30 + TH
box(TX - 160, TY - 160, top, TX + 160, TY + 160, top + 12, "metal", "Tower", "Platform")
for i, (ox, oy, h) in enumerate([(0, 0, 900), (90, -60, 500), (-80, 70, 420)]):
    put(CYL, (TX + ox, TY + oy, top + h / 2), scale=(0.08, 0.08, h / 100), folder="Tower", label="Mast_%d" % i, mat="metal")
for i, h in enumerate([top + 905, top - TH * 0.5, top - TH * 0.15]):
    put(SPHERE, (TX, TY, h), scale=(0.18, 0.18, 0.18), folder="Tower", label="Beacon_%d" % i, mat="beacon", collide=False)
    point_light((TX, TY, h), "Tower", "BeaconLight_%d" % i, (1.0, 0.05, 0.02, 1), 600.0, 1800.0)
# Панельные антенны на мачте.
for i, ang in enumerate((20, 140, 260)):
    ox, oy = rot2(120, 0, ang)
    put(CUBE, (TX + ox, TY + oy, top - 150), yaw=ang, scale=(0.12, 0.35, 1.4), folder="Tower", label="PanelAntenna_%d" % i, mat="metal")

# --- Гараж / техзона (ЮВ часть площадки): 12 × 9 м, ворота на юг ---
GX0, GY0, GX1, GY1 = -4900.0, 300.0, -3700.0, 1200.0
gg = ground_z((GX0 + GX1) / 2, (GY0 + GY1) / 2) or G_MIN
gz = gg + 10
box(GX0 - 10, GY0 - 10, gg - 40, GX1 + 10, GY1 + 10, gz, "concrete_dark", "Garage", "Floor")
# Южная стена: ворота 5 м между двумя участками стены, над проёмом — перемычка.
wall_run(GX1, GY0, GX1 - 300, GY0, gz, ["plain300"], "Garage", "SouthE")
wall_run(GX1 - 800, GY0, GX0, GY0, gz, ["win_small", "plain100"], "Garage", "SouthW")
box(GX1 - 800, GY0, gz + 300, GX1 - 300, GY0 + 20, gz + WALL_H, "concrete", "Garage", "GateLintel")
wall_run(GX0, GY0, GX0, GY1, gz, ["plain300", "win_small", "plain300"], "Garage", "West")
wall_run(GX0, GY1, GX1, GY1, gz, ["plain300", "plain300", "door", "plain300"], "Garage", "North")
wall_run(GX1, GY1, GX1, GY0, gz, ["plain300", "win_small", "plain300"], "Garage", "East")
put(PP + "Doors/SM_Door_01", (GX1 - 750, GY0 - 25, gz), yaw=0.0, scale=(1.2, 1, 0.94), folder="Garage", label="RollerGate")
box(GX0 - 30, GY0 - 30, gz + WALL_H, GX1 + 30, GY1 + 30, gz + WALL_H + 20, "corrugated", "Garage", "Roof")
point_light(((GX0 + GX1) / 2, (GY0 + GY1) / 2, gz + 330), "Garage", "GarageLight", (0.85, 0.92, 1.0, 1), 250.0, 900.0, True).light_component.set_volumetric_scattering_intensity(0.0)
put("/Game/HospitalCorridor/Meshes/Objects/SM_HospitalLamps_01_B01", ((GX0 + GX1) / 2, (GY0 + GY1) / 2, gz + WALL_H - 6),
    yaw=90.0, roll=180.0, folder="Garage", label="GarageLamp", collide=False)

unreal.log("[DARC] step: props")
# Наполнение — один раз: если автор уже расставлял предметы (папка FacilityProps есть), не трогаем.
import os as _os
if any(str(a.get_folder_path()).startswith(ROOT_PROPS) for a in sub.get_all_level_actors()):
    unreal.log("[DARC] Facility: FacilityProps exists — props kept as placed")
    DOOR_LIST_DONE = True
else:
    exec(open(_os.path.join(_os.path.dirname(__file__), "darc_build_facility_props.py"), encoding="utf-8").read(), globals())

# Отделка: материалы полов/приём декалей — каждый раз (коробку пересоздали), панели и декали — один раз.
exec(open(_os.path.join(_os.path.dirname(__file__), "darc_dress_interior.py"), encoding="utf-8").read(), {})

unreal.log("[DARC] Facility: built, lights %d, doors %d" % (lights_made, len(DOOR_LIST)))
