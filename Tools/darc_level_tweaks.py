# -*- coding: utf-8 -*-
"""
D.A.R.C. — правки уровня 1 по замечаниям автора (06.10.2026). Повторный запуск безопасен: каждый
изменённый объект помечается тегом и второй раз не трогается.
  - уличные фонари: в 4 раза тусклее, с тенями (не светят сквозь стены здания), радиус меньше;
  - луна: в 1.5 раза тусклее;
  - генератор: в 2 раза меньше, тяжелее (Mass=110);
  - огнетушители, вставшие у двери, — вдоль стены в сторону от двери;
  - санузлы: унитазы на 90° влево, спиной к стене, кабинки, раковины с зеркалами.
"""
import math
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
XW = -9800.0
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYL = "/Engine/BasicShapes/Cylinder.Cylinder"
M = "/Game/DARC/Imported/Materials/"
log = []
actors = sub.get_all_level_actors()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def tags_of(a):
    return [str(t) for t in a.tags]


def mark(a, tag, extra=None):
    """Тег-метка (через свойство — иначе уровень не считается изменённым и не сохраняется)."""
    a.modify()
    t = tags_of(a) + [tag] + (extra or [])
    a.set_editor_property("tags", [unreal.Name(x) for x in t])


def box(center, size, mat, folder, label, yaw=0.0):
    a = sub.spawn_actor_from_object(eal.load_asset(CUBE), unreal.Vector(*center), unreal.Rotator(0, 0, yaw))
    a.set_actor_scale3d(unreal.Vector(size[0] / 100, size[1] / 100, size[2] / 100))
    a.set_folder_path(folder)
    a.set_actor_label(label)
    a.static_mesh_component.set_material(0, eal.load_asset(mat))
    return a


# --- Уличные фонари ---
n = 0
for a in actors:
    if not str(a.get_folder_path()).startswith("Environment/StreetLamps") or "LvlTweak" in tags_of(a):
        continue
    if isinstance(a, (unreal.PointLight, unreal.SpotLight)):
        c = a.light_component
        c.set_intensity(c.intensity * 0.25)
        c.set_cast_shadows(True)
        c.set_editor_property("shadow_bias", 0.2)
        if isinstance(a, unreal.PointLight):
            c.set_attenuation_radius(min(c.attenuation_radius, 1400.0))
        c.set_editor_property("max_draw_distance", 6000.0)
        c.set_volumetric_scattering_intensity(min(c.volumetric_scattering_intensity, 0.3))
        mark(a, "LvlTweak")
        n += 1
log.append("street lights %d" % n)

# --- Луна ---
for a in actors:
    if isinstance(a, unreal.DirectionalLight) and "LvlTweak" not in tags_of(a):
        c = a.light_component
        c.set_intensity(c.intensity / 1.5)
        mark(a, "LvlTweak")
        log.append("moon -> %.3f" % c.intensity)

# --- Генератор ---
for a in actors:
    if a.get_actor_label() == "Generator" and "LvlTweak" not in tags_of(a):
        o0, e0 = a.get_actor_bounds(False)
        bottom = o0.z - e0.z
        a.modify()
        a.set_actor_scale3d(a.get_actor_scale3d() * 0.5)
        o, e = a.get_actor_bounds(False)
        a.set_actor_location(a.get_actor_location() + unreal.Vector(0, 0, bottom - (o.z - e.z)), False, False)
        t = [x for x in tags_of(a) if not x.startswith("Mass=")] + ["Mass=110"]
        a.set_editor_property("tags", [unreal.Name(x) for x in t])
        mark(a, "LvlTweak")
        log.append("generator scaled, mass 110")

# --- Створки дверей (для огнетушителей и раковин) ---
doors = []
for a in actors:
    if "/Doors" in str(a.get_folder_path()):
        o, e = a.get_actor_bounds(False)
        doors.append((o.x, o.y, o.z))


def near_door(x, y, z, r):
    return any(abs(dz - z) < 300 and math.hypot(dx - x, dy - y) < r for dx, dy, dz in doors)


def trace(p, q, ignore):
    hit = unreal.SystemLibrary.line_trace_single(world, p, q, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore,
                                                 unreal.DrawDebugTrace.NONE, True)
    return hit.to_tuple() if hit else None


# --- Огнетушители у двери — вдоль стены ---
for a in actors:
    lbl = a.get_actor_label()
    if not ("Extinguisher" in lbl or "FireExt" in lbl) or "LvlTweak" in tags_of(a):
        continue
    L = a.get_actor_location()
    o, e = a.get_actor_bounds(False)
    if not near_door(o.x, o.y, o.z, 160):
        continue
    # Стена — ближайшая по одной из 4 осей.
    best = None
    for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        h = trace(unreal.Vector(o.x, o.y, o.z), unreal.Vector(o.x + d[0] * 60, o.y + d[1] * 60, o.z), [a])
        if h and (best is None or h[3] < best[1]):
            best = (d, h[3])
    if not best:
        continue
    d = best[0]
    tangent = (-d[1], d[0])
    for s in (180, -180, 260, -260, 340, -340, 420, -420):
        x, y = o.x + tangent[0] * s, o.y + tangent[1] * s
        if near_door(x, y, o.z, 170):
            continue
        h = trace(unreal.Vector(x, y, o.z), unreal.Vector(x + d[0] * 60, y + d[1] * 60, o.z), [a])
        if not h or h[3] > 45:      # за ним должна быть стена
            continue
        # Путь вдоль стены свободен (не перескочили через стену/шкаф).
        if trace(unreal.Vector(o.x, o.y, o.z), unreal.Vector(x, y, o.z), [a]):
            continue
        a.modify()
        a.set_actor_location(unreal.Vector(L.x + (x - o.x), L.y + (y - o.y), L.z), False, False)
        mark(a, "LvlTweak")
        log.append("extinguisher %s moved %d cm" % (lbl, s))
        break

# --- Санузлы ---
rooms = []
for a in actors:
    f = str(a.get_folder_path())
    lbl = a.get_actor_label()
    if f.startswith("Facility/Main/F") and "/Rooms" in f and lbl.endswith("_Floor"):
        name = lbl[:-6]
        if name == "CoreWC" or name.startswith("WC"):
            o, e = a.get_actor_bounds(False)
            rooms.append((name, f.split("/")[2], o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z + e.z))

PLASTER = M + "M_painted_plaster_wall"
CERAMIC = None
for a in actors:
    if "Toilet" in a.get_actor_label() and isinstance(a, unreal.StaticMeshActor):
        CERAMIC = a.static_mesh_component.get_material(0).get_path_name()
        break
MIRROR = "/Game/UnrealBucket/Library/ArchViz/OrnateMirror/SM_OrnateMirror"

for name, fl, x0, x1, y0, y1, fz in rooms:
    toilets = [a for a in actors if "Toilet" in a.get_actor_label() and isinstance(a, unreal.StaticMeshActor)
               and x0 - 5 <= a.get_actor_location().x <= x1 + 5 and y0 - 5 <= a.get_actor_location().y <= y1 + 5
               and abs(a.get_actor_location().z - fz) < 100 and "LvlTweak" not in tags_of(a)]
    if not toilets:
        continue
    folder = "FacilityProps/Dressing/WC/%s_%s" % (fl, name)
    # Стена, у которой стоят унитазы: ближайшая сторона комнаты к их центру.
    cx = sum(t.get_actor_location().x for t in toilets) / len(toilets)
    cy = sum(t.get_actor_location().y for t in toilets) / len(toilets)
    sides = {"x0": cx - x0, "x1": x1 - cx, "y0": cy - y0, "y1": y1 - cy}
    side = min(sides, key=sides.get)
    along_x = side in ("y0", "y1")                   # ряд идёт вдоль X
    inward = {"x0": (1, 0), "x1": (-1, 0), "y0": (0, 1), "y1": (0, -1)}[side]
    wall = {"x0": x0, "x1": x1, "y0": y0, "y1": y1}[side]
    for t in toilets:
        r = t.get_actor_rotation()
        t.modify()
        t.set_actor_rotation(unreal.Rotator(r.roll, r.pitch, r.yaw - 90.0), False)
        # Спиной к стене: 2 см от неё.
        o, e = t.get_actor_bounds(False)
        L = t.get_actor_location()
        if along_x:
            back = (o.y - e.y) if inward[1] > 0 else (o.y + e.y)
            t.set_actor_location(unreal.Vector(L.x, L.y + (wall + inward[1] * 2 - back), L.z), False, False)
        else:
            back = (o.x - e.x) if inward[0] > 0 else (o.x + e.x)
            t.set_actor_location(unreal.Vector(L.x + (wall + inward[0] * 2 - back), L.y, L.z), False, False)
        mark(t, "LvlTweak")
    # Кабинки: перегородки между унитазами и по краям, передние панели с проходом.
    pos = sorted((t.get_actor_location().x if along_x else t.get_actor_location().y) for t in toilets)
    step = (pos[-1] - pos[0]) / (len(pos) - 1) if len(pos) > 1 else 110.0
    step = max(95.0, min(step, 130.0))
    edges = [pos[0] - step / 2] + [(a + b) / 2 for a, b in zip(pos, pos[1:])] + [pos[-1] + step / 2]
    lo, hi = (x0, x1) if along_x else (y0, y1)
    edges = [min(max(e_, lo + 2), hi - 2) for e_ in edges]
    depth, h, z0 = 150.0, 190.0, fz + 15.0
    for i, e_ in enumerate(edges):
        c_n = wall + inward[0 if not along_x else 1] * depth / 2
        if along_x:
            box((e_, c_n, z0 + h / 2), (3, depth, h), PLASTER, folder, "Stall_Side_%d" % i)
        else:
            box((c_n, e_, z0 + h / 2), (depth, 3, h), PLASTER, folder, "Stall_Side_%d" % i)
    front = wall + inward[0 if not along_x else 1] * depth
    for i, (a_, b_) in enumerate(zip(edges, edges[1:])):
        w = (b_ - a_) - 70.0                         # проход 70 см
        if w < 10:
            continue
        mid = a_ + w / 2
        if along_x:
            box((mid, front, z0 + h / 2), (w, 3, h), PLASTER, folder, "Stall_Front_%d" % i)
        else:
            box((front, mid, z0 + h / 2), (3, w, h), PLASTER, folder, "Stall_Front_%d" % i)
    # Раковины с зеркалами — на боковой стене дальше от двери (перпендикулярной ряду унитазов).
    if along_x:
        cands = [("x0", x0, (1, 0)), ("x1", x1, (-1, 0))]
    else:
        cands = [("y0", y0, (0, 1)), ("y1", y1, (0, -1))]
    # Середина свободной части комнаты: между фронтом кабинок и противоположной стеной.
    far_c = (front + (y0 if inward[1] < 0 else y1)) / 2 if along_x else (front + (x0 if inward[0] < 0 else x1)) / 2
    best = None
    for s_name, coord, nrm in cands:
        px, py = (coord, far_c) if along_x else (far_c, coord)
        dd = min([math.hypot(dx - px, dy - py) for dx, dy, dz in doors if abs(dz - fz) < 300] or [9999])
        if best is None or dd > best[0]:
            best = (dd, coord, nrm)
    if best and best[0] > 120:
        _, coord, nrm = best
        yaw = math.degrees(math.atan2(nrm[1], nrm[0]))
        for k, off in enumerate((-40.0, 40.0)):
            if along_x:
                sx, sy = coord + nrm[0] * 24, far_c + off
            else:
                sx, sy = far_c + off, coord + nrm[1] * 24
            if CERAMIC:
                box((sx, sy, fz + 80), (45, 45, 14) if along_x else (45, 45, 14), CERAMIC, folder, "Sink_%d" % k)
                p = sub.spawn_actor_from_object(eal.load_asset(CYL), unreal.Vector(sx, sy, fz + 38))
                p.set_actor_scale3d(unreal.Vector(0.14, 0.14, 0.72))
                p.static_mesh_component.set_material(0, eal.load_asset(CERAMIC))
                p.set_folder_path(folder)
                p.set_actor_label("SinkPedestal_%d" % k)
            if eal.does_asset_exist(MIRROR):
                mx, my = (coord + nrm[0] * 3, far_c + off) if along_x else (far_c + off, coord + nrm[1] * 3)
                # У модели зеркала лицевая сторона — вдоль её Y: доворот 90°.
                m = sub.spawn_actor_from_object(eal.load_asset(MIRROR), unreal.Vector(mx, my, fz + 160), unreal.Rotator(0, 0, yaw + 90.0))
                bb = m.static_mesh_component.static_mesh.get_bounding_box()
                size = bb.max - bb.min
                k_s = 70.0 / max(size.x, size.y, size.z, 1.0)
                m.set_actor_scale3d(unreal.Vector(k_s, k_s, k_s))
                m.set_folder_path(folder)
                m.set_actor_label("Mirror_%d" % k)
    log.append("WC %s %s: toilets %d, side %s" % (fl, name, len(toilets), side))

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
unreal.log("[DARC] tweaks: " + " | ".join(log))
