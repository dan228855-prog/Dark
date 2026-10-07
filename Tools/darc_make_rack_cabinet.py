# -*- coding: utf-8 -*-
"""
D.A.R.C. — шкаф-стойка из моделей автора: 4 стоечных сервера (SM_ServerRack) столбиком + боковины, крыша и цоколь
из тёмного металла, слитые в одну модель /Game/DARC/Imported/Meshes/ServerRack/SM_ServerRackCabinet (лицо — +X).
Ей заменяется модель тяжёлой стойки HeavyServerRack (разметка DARC.Heavy остаётся), лицом в комнату.
"""
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
eal = unreal.EditorAssetLibrary
D = "/Game/DARC/Imported/Meshes/ServerRack"
CUBE = "/Engine/BasicShapes/Cube.Cube"
METAL = "/Game/DARC/Imported/Materials/M_concrete_layers"
log = []

base = unreal.Vector(0, 0, 70000)
srv = eal.load_asset(D + "/SM_ServerRack")
parts = []
z = base.z + 8.0                       # цоколь 8 см
for k in range(4):
    a = sub.spawn_actor_from_object(srv, unreal.Vector(base.x, base.y, z))
    a.set_actor_scale3d(unreal.Vector(0.2, 0.2, 0.2))
    o, e = a.get_actor_bounds(False)
    a.set_actor_location(a.get_actor_location() + unreal.Vector(base.x - o.x, base.y - o.y, z - (o.z - e.z)), False, False)
    o, e = a.get_actor_bounds(False)
    z = o.z + e.z + 1.0
    parts.append(a)
o0, e0 = parts[0].get_actor_bounds(False)
w, d, top = e0.y, e0.x, z                # полуширина (Y), полуглубина (X), верх столбика


def box(cx, cy, cz, sx, sy, sz):
    b = sub.spawn_actor_from_object(eal.load_asset(CUBE), unreal.Vector(cx, cy, cz))
    b.set_actor_scale3d(unreal.Vector(sx / 100, sy / 100, sz / 100))
    b.static_mesh_component.set_material(0, eal.load_asset(METAL))
    parts.append(b)


h = top - base.z + 4
box(base.x, base.y - w - 2, base.z + h / 2, 2 * d + 4, 4, h)         # боковины
box(base.x, base.y + w + 2, base.z + h / 2, 2 * d + 4, 4, h)
box(base.x, base.y, top + 2, 2 * d + 4, 2 * w + 8, 4)                  # крыша
box(base.x, base.y, base.z + 4, 2 * d + 4, 2 * w + 8, 8)               # цоколь
box(base.x - d - 1, base.y, base.z + h / 2, 3, 2 * w + 8, h)           # задняя стенка

opts = unreal.MergeStaticMeshActorsOptions()
opts.set_editor_property("base_package_name", D + "/SM_ServerRackCabinet")
opts.set_editor_property("destroy_source_actors", True)
opts.set_editor_property("spawn_merged_actor", False)
ok = sme.merge_static_mesh_actors(parts, opts)
cab = eal.load_asset(D + "/SM_ServerRackCabinet")
if cab:
    bb = cab.get_bounding_box()
    log.append("cabinet %.0fx%.0fx%.0f" % ((bb.max - bb.min).x, (bb.max - bb.min).y, (bb.max - bb.min).z))
    # Простая коллизия-коробка (для тяжёлого объекта коробка своя, но взгляд/подбор работают и по модели).
    unreal.EditorStaticMeshLibrary.add_simple_collisions(cab, unreal.ScriptingCollisionShapeType.BOX) if hasattr(unreal, "EditorStaticMeshLibrary") else None
    eal.save_loaded_asset(cab)
    hv = next((a for a in sub.get_all_level_actors() if a.get_actor_label() == "HeavyServerRack"), None)
    if hv:
        o, e = hv.get_actor_bounds(False)
        hv.modify()
        hv.static_mesh_component.set_static_mesh(cab)
        for i in range(hv.static_mesh_component.get_num_materials()):
            hv.static_mesh_component.set_material(i, None)
        hv.set_actor_scale3d(unreal.Vector(1, 1, 1))
        hv.set_actor_rotation(unreal.Rotator(0, 0, -90.0), False)    # лицо +X → на юг, в комнату
        o1, e1 = hv.get_actor_bounds(False)
        hv.set_actor_location(hv.get_actor_location() + unreal.Vector(o.x - o1.x, o.y - o1.y, (o.z - e.z) - (o1.z - e1.z)), False, False)
        log.append("HeavyServerRack swapped")
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
unreal.log("[DARC] rack cabinet: merged=%s %s" % (ok, " | ".join(log)))
