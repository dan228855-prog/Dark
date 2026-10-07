# -*- coding: utf-8 -*-
"""
D.A.R.C. — механики уровня 1 поверх уже расставленного здания (один раз).
Запуск в редакторе: py C:/D.A.R.K/DARK/Tools/darc_add_mechanics.py

Добавляет (папка Outliner «FacilityProps/Mechanics», автор может двигать):
  - инструменты: лом (кладовая 2 эт.), гаечный ключ и тестер (техпомещение), 2 рации (охрана, 1 эт.);
  - решётку вентиляции в восточном коридоре 2 эт. (за ней короб в серверную, лезть пригнувшись);
  - распаечную коробку кабеля сервера над дверью серверной (изнутри): взлом двери ломом её рвёт;
  - второй ввод генератора — свет серверной (генератор не тянет свет и сервер разом).
Правит здание (папка Facility, как это делает и darc_build_facility.py): проём в стене + короб,
лампа серверной — на контур ServerLights; двери серверной — теги взлома.
Повторный запуск ничего не делает, если папка FacilityProps/Mechanics уже есть.
"""
import math
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
XW, YS = -9800.0, 0.0
Z0 = 1593.0 + 60.0
FH, WALL_H = 430.0, 400.0
Z2 = Z0 + FH                      # пол 2-го этажа
FOLDER = "FacilityProps/Mechanics"
PP = "/Game/AbandonedPowerPlant/Assets/"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYL = "/Engine/BasicShapes/Cylinder.Cylinder"
METAL = "/Game/DARC/Imported/Materials/M_metal_plate"
DARK = "/Game/DARC/Imported/Materials/M_concrete_layers"
INLET = "/Game/HospitalCorridor/Meshes/Objects/SM_UtilityElectronics_01_B"
WRENCH = "/Game/Tools/StaticMesh/Wrench"
VENT_LINE, VENT_V0, VENT_V1, VENT_H, VENT_LEN = 2900.0, 1700.0, 1800.0, 100.0, 110.0
log = []


def W(u, v):
    return XW + u, YS + v


def all_actors():
    return sub.get_all_level_actors()


def by_label(label):
    for a in all_actors():
        if a.get_actor_label() == label:
            return a
    return None


def put(path, loc, rot=(0.0, 0.0, 0.0), scale=(1, 1, 1), label="", mat=None, folder=FOLDER, tags=None, collide=True):
    """rot = (pitch, yaw, roll)."""
    a = sub.spawn_actor_from_object(eal.load_asset(path), unreal.Vector(*loc), unreal.Rotator(rot[2], rot[0], rot[1]))
    a.set_actor_scale3d(unreal.Vector(*scale))
    a.set_folder_path(folder)
    if label:
        a.set_actor_label(label)
    comp = a.static_mesh_component
    comp.set_editor_property("receives_decals", False)
    if mat:
        comp.set_material(0, eal.load_asset(mat))
    if not collide:
        comp.set_collision_profile_name("NoCollision")
    if tags:
        a.tags = [unreal.Name(t) for t in tags]
    return a


def lbox(u0, v0, z0, u1, v1, z1, mat, folder, label):
    x0, y0 = W(u0, v0)
    x1, y1 = W(u1, v1)
    return put(CUBE, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2),
               scale=(abs(x1 - x0) / 100, abs(y1 - y0) / 100, abs(z1 - z0) / 100), label=label, mat=mat, folder=folder)


def top_of(actor):
    o, e = actor.get_actor_bounds(False)
    return o.z + e.z


def add_tags(actor, new, replace_prefix=None):
    tags = [str(t) for t in actor.tags]
    if replace_prefix:
        tags = [t for t in tags if not t.startswith(replace_prefix)]
    for t in new:
        if t not in tags:
            tags.append(t)
    actor.tags = [unreal.Name(t) for t in tags]


if any(str(a.get_folder_path()).startswith(FOLDER) for a in all_actors()):
    unreal.log("[DARC] mechanics: FacilityProps/Mechanics already exists — nothing to do")
else:
    # --- 1. Проём вентиляции в восточной стене серверной (2 эт., коридор → серверная) ---
    wall = None
    for a in all_actors():
        if not str(a.get_folder_path()).startswith("Facility/Main/F2/Walls"):
            continue
        loc = a.get_actor_location()
        if abs(loc.x - (XW + VENT_LINE - 10)) < 3 and abs(loc.y - (YS + 1600)) < 3:
            wall = a
    if wall:
        mesh = wall.static_mesh_component.static_mesh
        rot = wall.get_actor_rotation()
        z = wall.get_actor_location().z
        width = mesh.get_bounding_box().max.x - mesh.get_bounding_box().min.x
        sx = wall.get_actor_scale3d().x
        log.append("wall %s %s width %.0f scale %.2f yaw %.0f" % (wall.get_actor_label(), mesh.get_name(), width, sx, rot.yaw))
        plain100 = eal.load_asset(PP + "Walls/SM_Wall_Plain_100")
        hb = plain100.get_bounding_box()
        mesh_h = hb.max.z - hb.min.z
        wf = "Facility/Main/F2/Walls"
        label = wall.get_actor_label()
        sub.destroy_actors([wall])
        x = XW + VENT_LINE - 10
        # Кусок 1600..1900 (модуль 300) → 100 глухой + 100 проём с перемычкой + 100 глухой.
        for v, lbl, zz, sz in ((1600, "_a", z, 1.0), (VENT_V0, "_VentLintel", z + VENT_H, (WALL_H - VENT_H) / mesh_h),
                               (VENT_V1, "_b", z, 1.0)):
            p = sub.spawn_actor_from_object(plain100, unreal.Vector(x, YS + v, zz), rot)
            p.set_actor_scale3d(unreal.Vector(1, 1, sz))
            p.set_folder_path(wf)
            p.set_actor_label(label + lbl)
        # Короб внутри серверной: стенки и крышка, выход в комнату.
        u1 = VENT_LINE - 10
        u0 = u1 - VENT_LEN
        vf = "Facility/Main/F2/Vent"
        lbox(u0, VENT_V0 - 6, Z2, u1, VENT_V0, Z2 + VENT_H + 6, METAL, vf, "VentDuct_S")
        lbox(u0, VENT_V1, Z2, u1, VENT_V1 + 6, Z2 + VENT_H + 6, METAL, vf, "VentDuct_N")
        lbox(u0, VENT_V0 - 6, Z2 + VENT_H, u1, VENT_V1 + 6, Z2 + VENT_H + 6, METAL, vf, "VentDuct_Top")
        log.append("vent opening + duct done (wall mesh z %.0f..%.0f)" % (hb.min.z, hb.max.z))
    else:
        log.append("WARNING: east server wall piece at v=1600 not found — vent opening skipped")

    # Решётка со стороны коридора (снимается ломом громко / ключом тихо).
    gx, gy = W(VENT_LINE + 12, (VENT_V0 + VENT_V1) / 2)
    put(CUBE, (gx, gy, Z2 + VENT_H / 2), scale=(0.04, (VENT_V1 - VENT_V0 + 8) / 100, (VENT_H + 8) / 100), label="VentGrille",
        mat=METAL, tags=["DARC.Cover", "Id=VentGrille", "Screws=4", "Task=EnterServerRoom",
                         "Name=Name_VentGrille", "Hint=Hint_VentGrille"])

    # --- 2. Инструменты ---
    ft = by_label("FuseTable")
    if ft:
        fl = ft.get_actor_location()
        # Лом лежит на полу у стола в кладовой: цилиндр 3 см × 80 см.
        put(CYL, (fl.x + 30, fl.y - 75, Z2 + 2), rot=(90.0, 15.0, 0.0), scale=(0.03, 0.03, 0.8), label="Crowbar",
            mat=METAL, tags=["DARC.Tool", "Type=Crowbar", "Name=Name_Crowbar", "Hint=Hint_Crowbar"])
    mt = by_label("ModuleTable")
    if mt:
        ml = mt.get_actor_location()
        top = top_of(mt)
        wr = eal.load_asset(WRENCH)
        b = wr.get_bounding_box()
        size = max(b.max.x - b.min.x, b.max.y - b.min.y, b.max.z - b.min.z)
        k = 25.0 / size if size > 0 else 1.0
        w = put(WRENCH, (ml.x + 45, ml.y + 10, top + 1), rot=(0.0, 30.0, 0.0), scale=(k, k, k), label="Wrench",
                tags=["DARC.Tool", "Type=Wrench", "Name=Name_Wrench", "Hint=Hint_Wrench"])
        # Модель могла лечь ниже столешницы (точка опоры не снизу) — поставить на стол.
        o, e = w.get_actor_bounds(False)
        w.set_actor_location(w.get_actor_location() + unreal.Vector(0, 0, top + 0.5 - (o.z - e.z)), False, False)
        log.append("wrench mesh size %.1f -> scale %.3f" % (size, k))
        put(CUBE, (ml.x - 45, ml.y - 5, top + 2), rot=(0.0, -20.0, 0.0), scale=(0.07, 0.14, 0.035), label="Tester",
            mat=DARK, tags=["DARC.Tool", "Type=Tester", "Name=Name_Tester", "Hint=Hint_Tester"])
    for i, desk_label in enumerate(("Security_Desk_0", "Security_Desk_2")):
        d = by_label(desk_label)
        if d:
            dl = d.get_actor_location()
            put(CUBE, (dl.x + 45, dl.y + 8, top_of(d) + 9), rot=(0.0, 80.0, 0.0), scale=(0.06, 0.035, 0.17),
                label="Radio_%d" % (i + 1), mat=DARK, tags=["DARC.Tool", "Type=Radio", "Name=Name_Radio", "Hint=Hint_Radio"])

    # --- 3. Кабель сервера над дверью (изнутри): взлом двери ломом его рвёт ---
    door = None
    for a in all_actors():
        if "Id=ServerDoor" in [str(t) for t in a.tags]:
            door = a
    if door:
        o, e = door.get_actor_bounds(False)
        inside_x = XW + 2310 + 8 if o.x < XW + 2600 else XW + 2890 - 8   # дверь на западной стене
        put(INLET, (inside_x, o.y, Z2 + 275), rot=(0.0, 180.0 if o.x < XW + 2600 else 0.0, 0.0), label="ServerJunction",
            tags=["DARC.Junction", "Circuit=Server", "Id=ServerJunction", "Task=RepairServerCable", "Blocks=ConnectInterface",
                  "Name=Name_Junction", "Hint=Hint_Junction"])
        add_tags(door, ["PryBreaks=ServerJunction", "PryTask=EnterServerRoom"])
        log.append("server door retagged: %s" % [str(t) for t in door.tags])
    else:
        log.append("WARNING: ServerDoor not found")

    # --- 4. Свет серверной — отдельный контур с вводом под генератор ---
    lamp = by_label("ServerSector_Lamp")
    if lamp:
        add_tags(lamp, ["Circuit=ServerLights"], replace_prefix="Circuit=")
        log.append("lamp retagged: %s" % [str(t) for t in lamp.tags])
    lx, ly = W(VENT_LINE - 17, 1950)
    put(INLET, (lx, ly, Z2 + 120), rot=(0.0, 0.0, 0.0), label="LightInlet",
        tags=["DARC.Inlet", "Circuit=ServerLights", "Id=LightInlet", "Name=Name_LightInlet", "Hint=Hint_LightInlet"])

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    log.append("saved")
unreal.log("[DARC] mechanics: " + " | ".join(log))
