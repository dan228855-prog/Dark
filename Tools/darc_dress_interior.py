# -*- coding: utf-8 -*-
"""
D.A.R.C. — отделка интерьера главного здания: разные стены/полы по типу комнаты и «грязь» (декали).
Запуск в редакторе: py C:/D.A.R.K/DARK/Tools/darc_dress_interior.py  (его же вызывает darc_build_facility.py в конце)

Каждый запуск: материалы полов по типу комнаты, приём декалей полом и внутренними стенами, материал камней у дороги
(у них слетел — серая клетка). Это объекты коробки (папка Facility), пересборка их пересоздаёт — поэтому каждый раз.
Один раз (если папки FacilityProps/Dressing ещё нет): панели отделки стен со стороны комнаты (одна стена разделяет
две комнаты — красить её саму нельзя, поменялась бы и сторона коридора) и декали: грязь, бумаги, мусор, лужи,
потёки, трещины, коврик у входа. Автор может двигать/удалять их; чтобы расставить заново — удалить папку Dressing.
"""
import math
import random
import re
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
XW, YS = -9800.0, 0.0
DRESS = "FacilityProps/Dressing"
M = "/Game/DARC/Imported/Materials/"
CUBE = "/Engine/BasicShapes/Cube.Cube"
rng = random.Random(7331)
log = []

# Проёмы модулей стен: (от, до вдоль модуля, низ, верх), см — как WALLS в darc_build_facility.py.
OPEN = {"SM_Wall_Door_300_01": (80, 220, 0, 240), "SM_Wall_Door_200": (30, 170, 0, 240),
        "SM_Wall_Window_Medium_03": (80, 220, 100, 300), "SM_Wall_Window_Small_02": (80, 220, 100, 250),
        "SM_Wall_Window_Tall_03": (80, 220, 100, 350)}


def has(name, *keys):
    return any(name.startswith(k) for k in keys)


def wall_finish(name):
    """Отделка стен комнаты (None — оставить как есть: коридоры, холлы, лестницы, кладовка ядра)."""
    if has(name, "Cor", "Lobby", "Hall", "Stair", "CoreCloset"):
        return None
    if has(name, "WC", "CoreWC"):
        return M + "M_white_rough_plaster"   # плитка у нас напольная (проекция сверху) — на стене идёт полосами
    if has(name, "Director", "Library", "Meeting"):
        return M + "M_4K_walllpaper_11"
    if has(name, "Medical", "Lab", "Laboratory"):
        return M + "M_white_rough_plaster"
    if has(name, "Archive", "Records", "BreakRoom"):
        return M + "M_peeling_painted_wall"
    if has(name, "Boiler", "Generator", "Workshop", "ToolRoom"):
        return M + "M_brick_wall_006"
    if has(name, "Electrical", "Storage", "Switchgear", "Utility", "TechRoom", "UPS", "ServerSector", "Darkroom",
           "Engineering", "Telemetry", "Ventilation"):
        return M + "M_concrete_wall_004"
    return M + "M_painted_plaster_wall"   # кабинеты, охрана, рабочие комнаты


def floor_finish(name):
    if has(name, "Director", "Library", "Meeting"):
        return M + "M_Carpet012_4K_JPG"
    if has(name, "ServerSector", "Telemetry", "SignalProcessing"):
        return M + "M_rubber_tiles"
    return None


def kind(name):
    if has(name, "Cor", "Lobby", "Hall"):
        return "corridor"
    if has(name, "WC", "CoreWC"):
        return "wc"
    if wall_finish(name) in (M + "M_concrete_wall_004", M + "M_brick_wall_006"):
        return "tech"
    if has(name, "Archive", "Records", "Library"):
        return "archive"
    return "office"


# --- Комнаты: полы коробки «<Имя>_Floor» дают имя и прямоугольник (внутренние грани стен) ---
rooms = []
floors_actors = []
walls = []
for a in sub.get_all_level_actors():
    folder = str(a.get_folder_path())
    fm = re.search(r"/F(\d)(/|$)", folder)
    if not folder.startswith("Facility/Main/") or not fm:
        continue
    f = int(fm.group(1)) - 1
    if "/Rooms" in folder and a.get_actor_label().endswith("_Floor"):
        o, e = a.get_actor_bounds(False)
        rooms.append({"name": a.get_actor_label()[:-6], "f": f, "x0": o.x - e.x, "x1": o.x + e.x,
                      "y0": o.y - e.y, "y1": o.y + e.y, "z": o.z + e.z})
        floors_actors.append(a)
    elif isinstance(a, unreal.StaticMeshActor) and a.static_mesh_component.static_mesh \
            and a.static_mesh_component.static_mesh.get_name().startswith("SM_Wall_"):
        walls.append((f, a))


def room_at(f, x, y):
    for r in rooms:
        if r["f"] == f and r["x0"] <= x <= r["x1"] and r["y0"] <= y <= r["y1"]:
            return r
    return None


# --- Каждый запуск: полы, приём декалей, камни ---
for fa in floors_actors:
    name = fa.get_actor_label()[:-6]
    fm = floor_finish(name)
    if fm:
        fa.static_mesh_component.set_material(0, eal.load_asset(fm))
    fa.static_mesh_component.set_editor_property("receives_decals", True)
for f, wa in walls:
    if "/Walls" in str(wa.get_folder_path()):
        wa.static_mesh_component.set_editor_property("receives_decals", True)
rock_mat = None
for a in sub.get_all_level_actors():
    if a.get_actor_label().startswith("RoadRock_") and isinstance(a, unreal.StaticMeshActor):
        c = a.static_mesh_component
        if c.get_material(0) and c.get_material(0).get_name() == "WorldGridMaterial":
            if rock_mat is None:
                rock_mat = eal.load_asset("/Game/Megascans/Environment/Rocks/MI_Rock_05") if eal.does_asset_exist(
                    "/Game/Megascans/Environment/Rocks/MI_Rock_05") else None
                if rock_mat is None:
                    reg = unreal.AssetRegistryHelpers.get_asset_registry()
                    for ad in reg.get_assets_by_path("/Game", recursive=True):
                        if str(ad.asset_name) == "MI_Rock_05":
                            rock_mat = eal.load_asset(str(ad.package_name))
                            break
            if rock_mat:
                for i in range(c.get_num_materials()):
                    c.set_material(i, rock_mat)
log.append("rooms %d walls %d rock material %s" % (len(rooms), len(walls), rock_mat.get_name() if rock_mat else None))

# --- Грани стен по комнатам (для панелей и настенных декалей) ---
faces = {}
for f, wa in walls:
    o, e = wa.get_actor_bounds(False)
    loc = wa.get_actor_location()
    yaw = math.radians(wa.get_actor_rotation().yaw)
    s = wa.get_actor_scale3d().x
    run_x = e.x > e.y                      # стена идёт вдоль X (толщина по Y)
    a0, a1 = (o.x - e.x, o.x + e.x) if run_x else (o.y - e.y, o.y + e.y)
    z0, z1 = o.z - e.z, o.z + e.z
    opening = None
    spec = OPEN.get(wa.static_mesh_component.static_mesh.get_name())
    if spec:
        p = loc.x if run_x else loc.y
        d = math.cos(yaw) if run_x else math.sin(yaw)
        p0, p1 = sorted((p + d * spec[0] * s, p + d * spec[1] * s))
        opening = (p0, p1, loc.z + spec[2], loc.z + spec[3])
    for sgn in (-1, 1):
        fc = (o.y + sgn * e.y) if run_x else (o.x + sgn * e.x)
        mid = (a0 + a1) / 2
        px, py = (mid, fc + sgn * 40) if run_x else (fc + sgn * 40, mid)
        r = room_at(f, px, py)
        if not r:
            continue
        lo, hi = (r["x0"], r["x1"]) if run_x else (r["y0"], r["y1"])
        b0, b1 = max(a0, lo), min(a1, hi)
        if b1 - b0 < 5:
            continue
        key = (f, id(r), run_x, round(fc), sgn)
        fl = faces.setdefault(key, {"room": r, "run_x": run_x, "fc": fc, "sgn": sgn, "segs": [], "open": [],
                                     "z0": z0, "z1": z1})
        fl["segs"].append((b0, b1))
        fl["z0"], fl["z1"] = min(fl["z0"], z0), max(fl["z1"], z1)
        if opening and opening[1] > b0 and opening[0] < b1:
            fl["open"].append(opening)


def merged(segs):
    out = []
    for a, b in sorted(segs):
        if out and a <= out[-1][1] + 2:
            out[-1] = (out[-1][0], max(out[-1][1], b))
        else:
            out.append((a, b))
    return out


def face_rects(fl):
    """Прямоугольники грани за вычетом проёмов: (от, до, низ, верх)."""
    rects = []
    for a, b in merged(fl["segs"]):
        cuts = sorted(op for op in fl["open"] if op[1] > a and op[0] < b)
        cur = a
        for p0, p1, oz0, oz1 in cuts:
            if p0 - cur > 3:
                rects.append((cur, p0, fl["z0"], fl["z1"]))
            if oz0 - fl["z0"] > 5:
                rects.append((max(p0, a), min(p1, b), fl["z0"], oz0))
            if fl["z1"] - oz1 > 5:
                rects.append((max(p0, a), min(p1, b), oz1, fl["z1"]))
            cur = max(cur, p1)
        if b - cur > 3:
            rects.append((cur, b, fl["z0"], fl["z1"]))
    return rects


def spawn_box(center, size, mat, folder, label):
    a = sub.spawn_actor_from_object(eal.load_asset(CUBE), unreal.Vector(*center))
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_folder_path(folder)
    a.set_actor_label(label)
    c = a.static_mesh_component
    c.set_material(0, eal.load_asset(mat))
    c.set_collision_profile_name("NoCollision")   # стена за панелью и так держит
    return a


def decal(mat, center, rot, size, folder, label):
    d = sub.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(*center), rot)
    d.decal.set_decal_material(eal.load_asset(mat))
    d.decal.set_editor_property("decal_size", unreal.Vector(*size))
    d.set_folder_path(folder)
    d.set_actor_label(label)
    return d


placed = any(str(a.get_folder_path()).startswith(DRESS) for a in sub.get_all_level_actors())
if placed:
    log.append("Dressing exists — panels/decals kept as placed")
else:
    # --- Панели отделки ---
    n_panels = 0
    for key, fl in faces.items():
        mat = wall_finish(fl["room"]["name"])
        if not mat:
            continue
        fol = "%s/Walls/F%d" % (DRESS, key[0] + 1)
        c = fl["fc"] + fl["sgn"] * 0.6
        for a, b, z0, z1 in face_rects(fl):
            mid, ln = (a + b) / 2, b - a
            if fl["run_x"]:
                spawn_box((mid, c, (z0 + z1) / 2), (ln, 1.0, z1 - z0), mat, fol, "Finish_%s" % fl["room"]["name"])
            else:
                spawn_box((c, mid, (z0 + z1) / 2), (1.0, ln, z1 - z0), mat, fol, "Finish_%s" % fl["room"]["name"])
            n_panels += 1

    # --- Декали ---
    DIRT = ["/Game/HospitalCorridor/Materials/Decals/MI_DecalDirt_02", "/Game/HospitalCorridor/Materials/Decals/MI_DecalDirt_03",
            "/Game/HospitalCorridor/Materials/Decals/MI_DecalDirt_04"]
    W = "/Game/Scene_Warehouse/Assets/MS/Decals/"
    MUD = [W + "Decal_Stain_Mud_01/MI_Decal_Stain_Mud_01_%s" % k for k in "ABC"]
    PAPERS = [W + "Decal_Papers_Dirty_01/MI_Decal_Papers_Dirty_01"]
    GARBAGE = [W + "Decal_Scatter_Garbage_Small_01/MI_Decal_Scatter_Garbage_Small_01",
               W + "Decal_Scatter_Garbage_Thin_01/MI_Decal_Scatter_Garbage_Thin_01_A",
               W + "Decal_Scatter_Garbage_Thin_01/MI_Decal_Scatter_Garbage_Thin_01_B"]
    PUDDLE = ["/Game/HospitalCorridor/Materials/Decals/M_DecalPuddle_02_A", "/Game/HospitalCorridor/Materials/Decals/M_DecalPuddle_03"]
    LEAKS = [W + "Decal_Leak_Grime_01/MI_Decal_Leak_Grime_01_A", W + "Decal_Leak_Grime_01/MI_Decal_Leak_Grime_01_B",
             W + "Decal_Leak_Grime_02/MI_Decal_Leak_Grime_02_A", W + "Decal_Leak_Grime_02/MI_Decal_Leak_Grime_02_B",
             "/Game/AbandonedPowerPlant/Assets/Decals/MI_Leak_01", "/Game/AbandonedPowerPlant/Assets/Decals/MI_Leak_02"]
    CRACKS = ["/Game/HospitalCorridor/Materials/Decals/MI_DecalWallCrack_01", "/Game/HospitalCorridor/Materials/Decals/MI_DecalDamage_01"]
    MAT_RUBBER = W + "Decal_Mat_Rubber_Worn_01/MI_Decal_Mat_Rubber_Worn_01_A"
    LINE = W + "Decal_Line_Concrete_01/MI_Decal_Line_Concrete_01"
    ok = lambda p: eal.does_asset_exist(p)
    DIRT, MUD, PAPERS, GARBAGE, PUDDLE, LEAKS, CRACKS = [[p for p in L if ok(p)] for L in (DIRT, MUD, PAPERS, GARBAGE, PUDDLE, LEAKS, CRACKS)]
    n_dec = 0

    def floor_decal(r, pool, size, label, margin=60.0):
        global n_dec
        if not pool:
            return
        x = rng.uniform(r["x0"] + margin, max(r["x0"] + margin, r["x1"] - margin))
        y = rng.uniform(r["y0"] + margin, max(r["y0"] + margin, r["y1"] - margin))
        decal(rng.choice(pool), (x, y, r["z"]), unreal.Rotator(0.0, -90.0, rng.uniform(0, 360)),
              (12.0, size * rng.uniform(0.7, 1.2), size * rng.uniform(0.7, 1.2)),
              "%s/Decals/F%d" % (DRESS, r["f"] + 1), "%s_%s" % (label, r["name"]))
        n_dec += 1

    room_faces = {}
    for fl in faces.values():
        room_faces.setdefault(id(fl["room"]), []).append(fl)

    def wall_decal(r, pool, w, h, zc, label):
        """Декаль на стене комнаты: случайная грань, место — мимо проёмов."""
        global n_dec
        cands = room_faces.get(id(r), [])
        if not pool or not cands:
            return
        for _ in range(8):
            fl = rng.choice(cands)
            a, b = rng.choice(merged(fl["segs"]))
            if b - a < w + 20:
                continue
            p = rng.uniform(a + w / 2 + 10, b - w / 2 - 10)
            if any(op[0] - w / 2 < p < op[1] + w / 2 and op[2] < zc + h / 2 for op in fl["open"]):
                continue
            # Декаль смотрит в стену: ось X — против нормали грани (нормаль — в комнату).
            if fl["run_x"]:
                center, yaw = (p, fl["fc"], zc), (-90.0 if fl["sgn"] > 0 else 90.0)
            else:
                center, yaw = (fl["fc"], p, zc), (180.0 if fl["sgn"] > 0 else 0.0)
            decal(rng.choice(pool), center, unreal.Rotator(0.0, 0.0, yaw), (14.0, w / 2, h / 2),
                  "%s/Decals/F%d" % (DRESS, r["f"] + 1), "%s_%s" % (label, r["name"]))
            n_dec += 1
            return

    for r in rooms:
        k = kind(r["name"])
        area = (r["x1"] - r["x0"]) * (r["y1"] - r["y0"]) / 10000.0       # м²
        decay = 1 + r["f"]                                                  # выше — запущеннее
        top = r["z"] + 396.0
        if k == "corridor":
            for _ in range(max(1, int(area / 14))):
                floor_decal(r, DIRT + GARBAGE, 70.0, "Dirt", margin=50.0)
            for _ in range(max(1, int(area / 30))):
                wall_decal(r, CRACKS, rng.uniform(60, 120), rng.uniform(60, 140), r["z"] + rng.uniform(60, 260), "Crack")
            if r["f"] == 2:
                wall_decal(r, LEAKS, 120.0, 220.0, top - 110.0, "Leak")
        elif k == "wc":
            floor_decal(r, PUDDLE, 90.0, "Puddle")
            floor_decal(r, DIRT, 70.0, "Dirt")
            wall_decal(r, LEAKS, 100.0, 200.0, top - 100.0, "Leak")
        elif k == "tech":
            for _ in range(1 + decay // 2 + int(area > 60)):
                floor_decal(r, MUD + DIRT, 110.0, "Mud")
            floor_decal(r, GARBAGE, 80.0, "Garbage")
            wall_decal(r, LEAKS, 130.0, 240.0, top - 120.0, "Leak")
            if has(r["name"], "Boiler", "Ventilation") and PUDDLE:
                floor_decal(r, PUDDLE, 110.0, "Puddle")
            if has(r["name"], "Generator", "Workshop", "Electrical", "UPS") and ok(LINE):
                floor_decal(r, [LINE], 120.0, "FloorLine", margin=90.0)
        else:   # кабинеты, архивы
            for _ in range(decay):
                floor_decal(r, PAPERS if k == "archive" else PAPERS + DIRT, 70.0, "Papers")
            if r["f"] >= 1:
                wall_decal(r, LEAKS, 110.0, 210.0, top - 105.0, "Leak")
            if rng.random() < 0.4:
                wall_decal(r, CRACKS, 80.0, 100.0, r["z"] + rng.uniform(120, 240), "Crack")
    # Коврик у главного входа (вестибюль 1 эт., вход с юга).
    lobby = [r for r in rooms if r["name"] == "Lobby" and r["f"] == 0]
    if lobby and ok(MAT_RUBBER):
        r = lobby[0]
        decal(MAT_RUBBER, ((r["x0"] + r["x1"]) / 2, r["y0"] + 80, r["z"]), unreal.Rotator(0.0, -90.0, 0.0), (12.0, 60.0, 90.0),
              DRESS + "/Decals/F1", "EntranceMat")
        n_dec += 1
    log.append("panels %d, decals %d" % (n_panels, n_dec))

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
unreal.log("[DARC] dress: " + " | ".join(log))
