# -*- coding: utf-8 -*-
"""
D.A.R.C. — таблички с названиями комнат у дверей (разметка DARC.Sign, текст — ключ Room_<Имя> из ST_UI,
пишет игра на лицевой стороне таблички — RU/EN вместе с языком). Ставятся со стороны коридора (или
соседней комнаты, если вход только через неё) у дверного проёма, на высоте глаз.
Пересоздаёт папку FacilityProps/Signs при каждом запуске.
"""
import math
import re
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
FOLDER = "FacilityProps/Signs"
CUBE = "/Engine/BasicShapes/Cube.Cube"
PLATE_MAT = "/Game/DARC/Imported/Materials/M_metal_plate"
SIGNED = {(0, "Electrical"), (0, "Security"), (1, "WorkRoom"), (1, "Office_2A"), (1, "TechRoom"), (1, "UPS_Batteries"),
          (1, "Storage_2"), (1, "ServerSector"), (1, "Records")}

old = [a for a in sub.get_all_level_actors() if str(a.get_folder_path()).startswith(FOLDER)]
if old:
    sub.destroy_actors(old)
actors = sub.get_all_level_actors()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

rooms = []
for a in actors:
    m = re.search(r"Facility/Main/F(\d)/Rooms", str(a.get_folder_path()))
    if m and a.get_actor_label().endswith("_Floor"):
        o, e = a.get_actor_bounds(False)
        rooms.append((int(m.group(1)) - 1, a.get_actor_label()[:-6], o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z + e.z))


def room_at(f, x, y):
    for fl, n, x0, x1, y0, y1, fz in rooms:
        if fl == f and x0 <= x <= x1 and y0 <= y <= y1:
            return n
    return None


log = []
for a in actors:
    if not any(str(t).startswith("DARC.Door") for t in a.tags):
        continue
    L = a.get_actor_location()
    yaw = math.radians(a.get_actor_rotation().yaw)
    bb = a.static_mesh_component.static_mesh.get_bounding_box()
    w = (bb.max.x - bb.min.x) * a.get_actor_scale3d().x
    tx, ty = math.cos(yaw), math.sin(yaw)          # вдоль стены (закрытая створка)
    nx, ny = -ty, tx                                # поперёк стены
    cx, cy = L.x + tx * w / 2, L.y + ty * w / 2     # середина проёма
    f = next((fl for fl, n, x0, x1, y0, y1, fz in rooms if abs(fz - L.z) < 60), None)
    if f is None:
        continue
    fz = next(r[6] for r in rooms if r[0] == f)
    side_a = room_at(f, cx + nx * 60, cy + ny * 60)
    side_b = room_at(f, cx - nx * 60, cy - ny * 60)
    for target, other, sgn in ((side_a, side_b, -1), (side_b, side_a, 1)):
        # Табличка комнаты target — на стороне other (снаружи), у проёма со стороны «ручки».
        # Снаружи — коридор/холл; в техпомещение и электрощитовую входят через соседнюю комнату — табличка там.
        if not target or (f, target) not in SIGNED or not other:
            continue
        if not (other.startswith(("Cor", "Hall", "Lobby")) or target in ("TechRoom", "Electrical")):
            continue
        px, py = cx + tx * (w / 2 + 45), cy + ty * (w / 2 + 45)
        ox, oy = nx * sgn, ny * sgn                 # наружу, в other
        # Стена за табличкой: луч из «снаружи» обратно к стене должен упереться в неё в пределах 40 см.
        start = unreal.Vector(px + ox * 40, py + oy * 40, fz + 170)
        hit = unreal.SystemLibrary.line_trace_single(world, start, unreal.Vector(px - ox * 20, py - oy * 20, fz + 170),
                                                     unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                     unreal.DrawDebugTrace.NONE, True)
        if not hit:
            px, py = cx - tx * (w / 2 + 45), cy - ty * (w / 2 + 45)      # с другой стороны проёма
            start = unreal.Vector(px + ox * 40, py + oy * 40, fz + 170)
            hit = unreal.SystemLibrary.line_trace_single(world, start, unreal.Vector(px - ox * 20, py - oy * 20, fz + 170),
                                                         unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                                         unreal.DrawDebugTrace.NONE, True)
            if not hit:
                log.append("no wall for %s" % target)
                continue
        wall = hit.to_tuple()[4]
        pos = unreal.Vector(wall.x + ox * 1.2, wall.y + oy * 1.2, fz + 175)
        s = sub.spawn_actor_from_object(eal.load_asset(CUBE), pos, unreal.Rotator(0, 0, math.degrees(math.atan2(oy, ox))))
        s.set_actor_scale3d(unreal.Vector(0.02, 0.9, 0.16))
        s.static_mesh_component.set_material(0, eal.load_asset(PLATE_MAT))
        s.static_mesh_component.set_collision_profile_name("NoCollision")
        s.set_folder_path(FOLDER)
        s.set_actor_label("Sign_%s_F%d" % (target, f + 1))
        s.set_editor_property("tags", [unreal.Name(t) for t in ("DARC.Sign", "Key=Room_" + target, "Size=9")])
        log.append("%s(F%d) facing %s" % (target, f + 1, other))
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
unreal.log("[DARC] signs: " + " | ".join(log))
