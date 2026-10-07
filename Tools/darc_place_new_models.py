# -*- coding: utf-8 -*-
"""
D.A.R.C. — новые модели автора (07.10.2026) на уровень 1 вместо заглушек.
  - стойки в серверной (декор) → столбики из 4 стоечных серверов SM_ServerRack, лицом в комнату;
  - запасной предохранитель → SM_Fuse; терминал каталога и консоль сервера → SM_RetroComputer;
  - щитки (3 шт.) → SM_ElectricBox; кодовая панель серверной → SM_WallComputer;
  - «Волга» во дворе между зданием и гаражом.
Разметка (теги DARC.*) остаётся на тех же акторах — меняется только модель. Повторно не меняет уже заменённое.
"""
import math
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
M = "/Game/DARC/Imported/Meshes/"
XW = -9800.0
log = []
actors = sub.get_all_level_actors()


def by_label(label):
    return next((a for a in actors if a.get_actor_label() == label), None)


def bounds(a):
    o, e = a.get_actor_bounds(False)
    return o, e


def swap(a, mesh_path, scale, yaw, face, back_to_wall=True):
    """Сменить модель актора: масштаб, поворот (yaw), центр — на месте старого; face — куда смотрит лицо
    (единичный вектор по оси X/Y); спиной к стене — задняя грань там же, где была у старой модели."""
    if a is None:
        return False
    if a.static_mesh_component.static_mesh and a.static_mesh_component.static_mesh.get_path_name().startswith(mesh_path):
        return False
    o0, e0 = bounds(a)
    a.modify()
    a.static_mesh_component.set_static_mesh(eal.load_asset(mesh_path))
    for i in range(a.static_mesh_component.get_num_materials()):
        a.static_mesh_component.set_material(i, None)   # материалы модели, а не старые
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.set_actor_rotation(unreal.Rotator(0, 0, yaw), False)
    o1, e1 = bounds(a)
    d = unreal.Vector(o0.x - o1.x, o0.y - o1.y, (o0.z - e0.z) - (o1.z - e1.z))   # центр XY, низ по низу
    if back_to_wall and face:
        fx, fy = face
        old_back = (o0.x - fx * e0.x) if fx else (o0.y - fy * e0.y)
        new_back = (o1.x - fx * e1.x) if fx else (o1.y - fy * e1.y)
        if fx:
            d.x = old_back - new_back
        else:
            d.y = old_back - new_back
    a.set_actor_location(a.get_actor_location() + d, False, False)
    return True


# --- Стойки в серверной: столбики из 4 серверов ---
rack = eal.load_asset(M + "ServerRack/SM_ServerRack")
old = [a for a in actors if a.get_actor_label().startswith("ServerSector_Rack")]
for a in old:
    o, e = bounds(a)
    folder = a.get_folder_path()
    base_z = o.z - e.z
    label = a.get_actor_label()
    sub.destroy_actors([a])
    for k in range(4):
        s = sub.spawn_actor_from_object(rack, unreal.Vector(o.x, o.y, base_z), unreal.Rotator(0, 0, 90))  # лицо +X → в комнату (+Y)
        s.set_actor_scale3d(unreal.Vector(0.2, 0.2, 0.2))
        so, se = bounds(s)
        # Задняя грань — к стене (где был задник старой стойки), столбик снизу вверх.
        s.set_actor_location(s.get_actor_location() + unreal.Vector(o.x - so.x, (o.y - e.y) - (so.y - se.y),
                                                                        base_z + k * (2 * se.z + 1) - (so.z - se.z)), False, False)
        s.set_folder_path(folder)
        s.set_actor_label("%s_Srv%d" % (label, k))
log.append("racks: %d → stacks of 4" % len(old))

# --- Запасной предохранитель (у модели точка опоры далеко в стороне — ставим по центру старого места) ---
f = by_label("SpareFuse")
if swap(f, M + "Fuse/SM_Fuse", 0.03, 0.0, None, back_to_wall=False):
    log.append("fuse")

# --- Терминалы: ретро-компьютер, экран у модели смотрит вдоль +Y ---
if swap(by_label("CatalogTerminal"), M + "RetroComputer/SM_RetroComputer", 0.8, 0.0, None, back_to_wall=False):
    log.append("catalog terminal (screen to the chair, +Y)")
if swap(by_label("ServerConsole"), M + "RetroComputer/SM_RetroComputer", 0.8, 90.0, None, back_to_wall=False):
    log.append("server console (screen west, into the room)")

# --- Щитки: SM_ElectricBox (лицо +X); смотрят в ту же сторону, что старые (у тех лицо было −X, доворот 180) ---
for lbl in ("FuseBox_Corridor", "FuseBox_Building", "FuseBox_Server"):
    a = by_label(lbl)
    if not a:
        continue
    yaw = a.get_actor_rotation().yaw - 180.0
    fx, fy = round(math.cos(math.radians(yaw))), round(math.sin(math.radians(yaw)))
    if swap(a, M + "ElectricBox/SM_ElectricBox", 1.0, yaw, (fx, fy)):
        log.append(lbl)

# --- Кодовая панель серверной → настенный компьютер, лицом в коридор (−X: панель снаружи западной стены) ---
k = by_label("ServerKeypad")
if k:
    o, e = bounds(k)
    z_center = o.z
    if swap(k, M + "WallComputer/SM_WallComputer", 0.35, 180.0, (-1, 0)):
        o1, e1 = bounds(k)
        k.set_actor_location(k.get_actor_location() + unreal.Vector(0, 0, (z_center + 10) - o1.z), False, False)
        log.append("keypad → wall computer")

# --- «Волга» во дворе: между главным зданием и гаражом, на земле ---
if not by_label("Volga_Parked"):
    x, y = -5250.0, 1700.0
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 5000), unreal.Vector(x, y, -5000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    z = hit.to_tuple()[4].z if hit else 1593.0
    v = sub.spawn_actor_from_object(eal.load_asset(M + "VolgaCar/SM_Volga"), unreal.Vector(x, y, z + 2), unreal.Rotator(0, 0, 70))
    v.set_folder_path("FacilityProps/Mechanics")
    v.set_actor_label("Volga_Parked")
    log.append("volga at z %.0f" % z)

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
unreal.log("[DARC] placed: " + " | ".join(log))
