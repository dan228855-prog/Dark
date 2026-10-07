# База D.A.R.C. (docs/DARC_base_corridor.md): гараж + прямой коридор с дверями, лобби куратора,
# комнаты, которые открываются по прогрессу, и закрытая дверь со светом у тупика.
# Карта /Game/DARC/Maps/BaseMap. Запуск: в редакторе `py C:/D.A.R.K/DARK/Tools/darc_build_base.py`
# или без окна (UnrealEditor-Cmd ... -run=pythonscript -script=...). Пересборка сносит папку «Base»
# (каркас), папку «BaseProps» (то, что автор расставил руками) не трогает.
import math
import unreal

MAP = "/Game/DARC/Maps/BaseMap"
ROOT = "Base"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CAR_DIR = "/Game/DARC/Imported/Meshes/car/bmw_x7_m60i"
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
IDLE = "/Game/FreeAnimationLibrary/Animations/Idle/anim_Idle"
MAT_DIR = "/Game/DARC/Materials/Base"

# Размеры, см. Ось X — вдоль коридора от гаража к тупику, +Y — правая сторона.
GARAGE = (-1500.0, -700.0, 0.0, 700.0)   # x0, y0, x1, y1
GARAGE_H = 520.0
COR_LEN = 6000.0
COR_HALF = 210.0
COR_H = 360.0
WALL = 30.0
ROOM_DEPTH = 760.0
ROOM_HALF_W = 420.0
DOOR_W, DOOR_H = 120.0, 230.0
DOOR_X = [700.0, 1650.0, 2600.0, 3550.0, 4500.0, 5450.0]

# Двери: (сторона, № по порядку, тип комнаты, ключ таблички, факт открытия / "open" / None — никогда)
DOORS = [
    ("R", 0, "lobby", "Base_DoorLobby", "open"),
    ("L", 0, "rest", "Base_DoorRestRoom", "Base.RestRoom"),
    ("R", 1, "archive", "Base_DoorArchive", "Base.ArchiveTerminal"),
    ("L", 1, "equipment", "Base_DoorEquipment", "Base.Equipment"),
    ("R", 2, "evidence", "Base_DoorEvidence", "Base.Evidence"),
    ("L", 2, "briefing", "Base_DoorBriefing", "Base.Briefing"),
    ("R", 3, "medbay", "Base_DoorMedbay", "Base.Medbay"),
    ("L", 3, None, "Base_DoorTech", None),
    ("R", 4, None, "Base_DoorServer", None),
    ("L", 4, None, "Base_DoorRestricted", None),
    ("R", 5, None, "", None),            # резерв под будущие улучшения
    ("L", 5, "lightdoor", "", None),     # дверь со светом — не открывается в 1-й арке
]

eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
loaded = {}


def asset(path):
    if path not in loaded:
        loaded[path] = unreal.load_asset(path)
    return loaded[path]


# ---------- материалы (серый «мрамор» до настоящих текстур) ----------

def base_material():
    path = MAT_DIR + "/M_DarcBaseSolid"
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset("M_DarcBaseSolid", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "Color")
    rough = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 200)
    rough.set_editor_property("parameter_name", "Roughness")
    metal = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 300)
    metal.set_editor_property("parameter_name", "Metallic")
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def solid(name, color, roughness, metallic=0.0):
    path = MAT_DIR + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", base_material())
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    eal.save_loaded_asset(mi)
    return mi


def emissive(name, color, intensity):
    path = MAT_DIR + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else tools.create_asset(
        name, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", unreal.load_asset("/Game/DARC/Materials/M_DarcEmissive"))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    mel.set_material_instance_scalar_parameter_value(mi, "Intensity", intensity)
    eal.save_loaded_asset(mi)
    return mi


MAT = {
    "wall": solid("MI_Base_MarbleWall", (0.42, 0.43, 0.45, 1), 0.12),
    "panel": solid("MI_Base_MarblePanel", (0.30, 0.31, 0.33, 1), 0.1),
    "floor": solid("MI_Base_MarbleFloor", (0.08, 0.085, 0.09, 1), 0.08),
    "ceiling": solid("MI_Base_Ceiling", (0.55, 0.56, 0.58, 1), 0.7),
    "trim": solid("MI_Base_SteelTrim", (0.6, 0.62, 0.65, 1), 0.25, 1.0),
    "door": solid("MI_Base_Door", (0.035, 0.037, 0.04, 1), 0.3, 0.6),
    "concrete": solid("MI_Base_Concrete", (0.22, 0.22, 0.23, 1), 0.85),
    "furniture": solid("MI_Base_Furniture", (0.12, 0.12, 0.13, 1), 0.45),
    "glass": solid("MI_Base_TintedGlass", (0.01, 0.012, 0.015, 1), 0.05, 0.9),
    "light": emissive("MI_Base_LightStrip", (0.85, 0.92, 1.0, 1), 3.0),
    "screen": emissive("MI_Base_Screen", (0.25, 0.45, 0.9, 1), 1.2),
    "red": emissive("MI_Base_ReaderRed", (1.0, 0.05, 0.05, 1), 4.0),
}

# ---------- карта ----------

# Работает и в редакторе, и без окна (UnrealEditor-Cmd -run=pythonscript).
if eal.does_asset_exist(MAP):
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
else:
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

doomed = [a for a in sub.get_all_level_actors() if str(a.get_folder_path()).split("/")[0] == ROOT]
if doomed:
    sub.destroy_actors(doomed)


def place(actor, folder, label):
    actor.set_folder_path(ROOT + ("/" + folder if folder else ""))
    if label:
        actor.set_actor_label(label)
    return actor


def put(mesh, loc, yaw=0.0, scale=(1, 1, 1), folder="", label="", mat=None, collide=True):
    a = sub.spawn_actor_from_object(asset(mesh), unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    a.set_actor_scale3d(unreal.Vector(*scale))
    comp = a.static_mesh_component
    if mat:
        comp.set_material(0, MAT[mat] if isinstance(mat, str) else mat)
    if not collide:
        comp.set_collision_profile_name("NoCollision")
    return place(a, folder, label)


def box(x0, y0, z0, x1, y1, z1, mat, folder, label, collide=True):
    if abs(x1 - x0) < 1 or abs(y1 - y0) < 1 or abs(z1 - z0) < 1:
        return None
    return put(CUBE, ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2),
               scale=(abs(x1 - x0) / 100, abs(y1 - y0) / 100, abs(z1 - z0) / 100),
               folder=folder, label=label, mat=mat, collide=collide)


def spawn(cls, loc, yaw=0.0, folder="", label=""):
    a = sub.spawn_actor_from_class(cls, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
    return place(a, folder, label)


def rect_light(loc, lumens, width, height, folder, label, color=(0.9, 0.95, 1.0), radius=900.0, shadows=False):
    a = spawn(unreal.RectLight, loc, 0.0, folder, label)
    a.set_actor_rotation(unreal.Rotator(0, -90, 0), False)  # светит вниз
    c = a.get_component_by_class(unreal.RectLightComponent)
    c.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    c.set_intensity(lumens)
    c.set_light_color(unreal.LinearColor(*color, 1))
    c.set_editor_property("source_width", width)
    c.set_editor_property("source_height", height)
    c.set_attenuation_radius(radius)
    c.set_cast_shadows(shadows)
    return a


def text(loc, yaw, key_text, size, folder, label, color=(0.8, 0.82, 0.86), tags=()):
    a = spawn(unreal.TextRenderActor, loc, yaw, folder, label)
    t = a.text_render
    t.set_text(key_text)
    t.set_editor_property("world_size", size)
    t.set_editor_property("horizontal_alignment", unreal.HorizTextAligment.EHTA_CENTER)
    t.set_editor_property("vertical_alignment", unreal.VerticalTextAligment.EVRTA_TEXT_CENTER)
    t.set_text_render_color(unreal.Color(int(color[0] * 255), int(color[1] * 255), int(color[2] * 255), 255))
    if tags:
        a.tags = [unreal.Name(x) for x in tags]
    return a


def tag(actor, tags):
    actor.tags = [unreal.Name(t) for t in tags]


# ---------- гараж ----------
gx0, gy0, gx1, gy1 = GARAGE
box(gx0, gy0, -20, gx1, gy1, 0, "concrete", "Garage", "Floor")
box(gx0, gy0, GARAGE_H, gx1, gy1, GARAGE_H + 20, "ceiling", "Garage", "Ceiling")
box(gx0, gy0 - WALL, 0, gx1, gy0, GARAGE_H, "panel", "Garage", "Wall_S")
box(gx0, gy1, 0, gx1, gy1 + WALL, GARAGE_H, "panel", "Garage", "Wall_N")
# Въезд: ворота во всю ширину торца (закрыты — декорация)
box(gx0 - WALL, gy0, 0, gx0, gy1, GARAGE_H, "panel", "Garage", "Wall_Gate")
box(gx0 + 2, -380, 0, gx0 + 12, 380, 420, "trim", "Garage", "Gate_Shutter")
for i in range(9):
    box(gx0 + 12, -380, 20 + i * 45, gx0 + 16, 380, 24 + i * 45, "door", "Garage", "Gate_Rib_%d" % i, collide=False)
# Стена к коридору с проёмом под коридор
box(gx1, gy0, 0, gx1 + WALL, -COR_HALF, GARAGE_H, "panel", "Garage", "Wall_E_S")
box(gx1, COR_HALF, 0, gx1 + WALL, gy1, GARAGE_H, "panel", "Garage", "Wall_E_N")
box(gx1, -COR_HALF, COR_H, gx1 + WALL, COR_HALF, GARAGE_H, "panel", "Garage", "Wall_E_Top")
# Разметка места машины
for y in (-260, 260):
    box(-1200, y - 6, 0, -200, y + 6, 1, "trim", "Garage", "Marking", collide=False)
for i, x in enumerate((-1250, -750, -250)):
    for j, y in enumerate((-480, 0, 480)):
        rect_light((x, y, GARAGE_H - 5), 450, 160, 30, "Garage/Lights", "Light_%d_%d" % (i, j), radius=1200,
                   shadows=(j == 1))
        box(x - 80, y - 15, GARAGE_H - 4, x + 80, y + 15, GARAGE_H, "light", "Garage/Lights", "Strip_%d_%d" % (i, j),
            collide=False)

# ---------- машина: части модели, вписанные по габаритам (длина ~5 м, носом к коридору) ----------
CAR_CENTER = (-700.0, 0.0)
parts = [p for p in eal.list_assets(CAR_DIR, recursive=True, include_folder=False)
         if eal.find_asset_data(p).asset_class_path.asset_name == "StaticMesh"]
car_actors = [put(p.split(".")[0], (0, 0, 0), folder="Garage/Car", label="Car_" + p.split(".")[-1], collide=False)
              for p in parts]
if car_actors:
    lo = [min(a.get_actor_bounds(False)[0].x - a.get_actor_bounds(False)[1].x for a in car_actors),
          min(a.get_actor_bounds(False)[0].y - a.get_actor_bounds(False)[1].y for a in car_actors),
          min(a.get_actor_bounds(False)[0].z - a.get_actor_bounds(False)[1].z for a in car_actors)]
    hi = [max(a.get_actor_bounds(False)[0].x + a.get_actor_bounds(False)[1].x for a in car_actors),
          max(a.get_actor_bounds(False)[0].y + a.get_actor_bounds(False)[1].y for a in car_actors),
          max(a.get_actor_bounds(False)[0].z + a.get_actor_bounds(False)[1].z for a in car_actors)]
    along_y = (hi[1] - lo[1]) > (hi[0] - lo[0])
    length = max(hi[0] - lo[0], hi[1] - lo[1])
    s = 500.0 / length if length > 0 else 1.0
    yaw = -90.0 if along_y else 0.0
    cx, cy = (lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2
    for a in car_actors:
        # повернуть вокруг центра модели, масштабировать, поставить на пол в центр парковки
        rx, ry, rz = -cx, -cy, -lo[2]
        c, sn = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        vx, vy = (rx * c - ry * sn) * s, (rx * sn + ry * c) * s
        a.set_actor_scale3d(unreal.Vector(s, s, s))
        a.set_actor_rotation(unreal.Rotator(0, 0, yaw), False)
        a.set_actor_location(unreal.Vector(CAR_CENTER[0] + vx, CAR_CENTER[1] + vy, rz * s), False, False)
    unreal.log("DARCI car: %d parts, length %.0f -> scale %.3f yaw %.0f" % (len(car_actors), length, s, yaw))
car = spawn(unreal.DarcBaseCar, (CAR_CENTER[0], CAR_CENTER[1], 90), 0.0, "Garage", "BaseCar")
car.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(260, 115, 90))

# Точки появления: у дверей машины, лицом к коридору — «только что вышли».
for i, (dx, dy) in enumerate(((-60, -190), (60, -190), (-60, 190), (60, 190), (330, 0))):
    spawn(unreal.PlayerStart, (CAR_CENTER[0] + dx, CAR_CENTER[1] + dy, 100), 0.0, "Garage", "Spawn_%d" % i)

# ---------- коридор ----------
box(0, -COR_HALF, -20, COR_LEN, COR_HALF, 0, "floor", "Corridor", "Floor")
box(0, -COR_HALF, COR_H, COR_LEN, COR_HALF, COR_H + 20, "ceiling", "Corridor", "Ceiling")
box(COR_LEN, -COR_HALF - WALL, 0, COR_LEN + WALL, COR_HALF + WALL, COR_H, "panel", "Corridor", "DeadEnd")
text((COR_LEN - 3, 0, 220), 180.0, "D.A.R.C.", 90, "Corridor", "DeadEnd_Logo", color=(0.75, 0.77, 0.8))


def side_y(side):
    return COR_HALF if side == "R" else -COR_HALF


def build_side(side):
    """Стена коридора с проёмами под двери этой стороны."""
    y0 = COR_HALF if side == "R" else -COR_HALF - WALL
    y1 = y0 + WALL
    gaps = sorted(DOOR_X[n] for s, n, *_ in DOORS if s == side)
    x = 0.0
    for gx in gaps:
        box(x, y0, 0, gx - DOOR_W / 2, y1, COR_H, "wall", "Corridor/Walls", "Wall_%s_%d" % (side, int(x)))
        box(gx - DOOR_W / 2, y0, DOOR_H, gx + DOOR_W / 2, y1, COR_H, "wall", "Corridor/Walls", "Lintel_%s_%d" % (side, int(gx)))
        x = gx + DOOR_W / 2
    box(x, y0, 0, COR_LEN, y1, COR_H, "wall", "Corridor/Walls", "Wall_%s_end" % side)
    # стальной плинтус и световая линия под потолком вдоль стены
    yy = COR_HALF - 2 if side == "R" else -COR_HALF + 2
    box(0, yy - 2, 0, COR_LEN, yy + 2, 12, "trim", "Corridor/Walls", "Skirting_" + side, collide=False)


build_side("R")
build_side("L")

# Свет коридора: потолочные линии + динамики
for i, x in enumerate(range(250, int(COR_LEN), 500)):
    rect_light((x, 0, COR_H - 5), 280, 220, 20, "Corridor/Lights", "Light_%02d" % i, radius=800, shadows=(i % 3 == 0))
    box(x - 110, -10, COR_H - 4, x + 110, 10, COR_H, "light", "Corridor/Lights", "Strip_%02d" % i, collide=False)
for i, x in enumerate(range(1200, int(COR_LEN), 1800)):
    box(x - 15, COR_HALF - 25, COR_H - 30, x + 15, COR_HALF - 2, COR_H - 5, "door", "Corridor", "Speaker_%d" % i)

# ---------- двери и комнаты ----------


def build_room(side, x, kind, folder):
    """Комната за дверью: пол, потолок, стены, свет. Внутри — заготовка по типу."""
    sign = 1 if side == "R" else -1
    yi = COR_HALF + WALL if side == "R" else -COR_HALF - WALL     # внутренняя грань коридорной стены
    yo = yi + sign * ROOM_DEPTH
    ya, yb = min(yi, yo), max(yi, yo)
    xa, xb = x - ROOM_HALF_W, x + ROOM_HALF_W
    box(xa, ya, -20, xb, yb, 0, "floor", folder, "Floor")
    box(xa, ya, COR_H, xb, yb, COR_H + 20, "ceiling", folder, "Ceiling")
    box(xa - WALL, ya, 0, xa, yb, COR_H, "wall", folder, "Wall_A")
    box(xb, ya, 0, xb + WALL, yb, COR_H, "wall", folder, "Wall_B")
    yback = (yo, yo + sign * WALL)
    box(xa - WALL, min(yback), 0, xb + WALL, max(yback), COR_H, "wall", folder, "Wall_Back")
    cy = (yi + yo) / 2
    rect_light((x, cy, COR_H - 5), 380, 260, 60, folder, "Light", radius=900, shadows=True)
    box(x - 130, cy - 30, COR_H - 4, x + 130, cy + 30, COR_H, "light", folder, "Strip", collide=False)
    back = yo - sign * 40   # у дальней стены
    yaw_face = 90.0 if side == "L" else -90.0   # лицом к двери

    if kind == "lobby":
        # Стойка, за ней тонированное стекло; за стеклом в полумраке — «куратор» (манекен), его не разглядеть.
        box(x - 220, cy - 40, 0, x + 220, cy + 40, 105, "furniture", folder, "Desk")
        glass_y = cy + sign * 120
        box(xa, glass_y - 3, 0, xb, glass_y + 3, COR_H, "glass", folder, "TintedGlass")
        man = spawn(unreal.SkeletalMeshActor, (x, cy + sign * 260, 0), yaw_face, folder, "Curator_Mannequin")
        smc = man.skeletal_mesh_component
        smc.set_skeletal_mesh_asset(asset(MANNY))
        smc.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        smc.set_editor_property("animation_data", unreal.SingleAnimationPlayData(anim_to_play=asset(IDLE), saved_looping=True, saved_playing=True))
        for d in (-1, 1):
            box(x + d * 340 - 15, yi + sign * 20 - 15, COR_H - 40, x + d * 340 + 15, yi + sign * 20 + 15, COR_H - 5,
                "door", folder, "Speaker_%d" % d)
        cur = spawn(unreal.DarcBaseCurator, (x, cy, COR_H / 2), 0.0, folder, "Curator")
        cur.get_component_by_class(unreal.BoxComponent).set_box_extent(
            unreal.Vector(ROOM_HALF_W - 20, ROOM_DEPTH / 2 - 20, COR_H / 2))
        # Экран выбранного объекта на стене над стойкой (виден из двери) и терминал выбора у боковой стены.
        sx = xa + 4
        box(xa, cy - 160, 140, xa + 4, cy + 160, 300, "screen", folder, "MissionScreen_Panel", collide=False)
        text((sx + 2, cy, 220), 0.0, "ОБЪЕКТ", 36, folder, "MissionScreen_Text", color=(0.9, 0.95, 1.0),
             tags=("DARC.MissionScreen",))
        box(xb - 50, cy - 50, 0, xb - 10, cy + 50, 110, "furniture", folder, "Terminal_Stand")
        box(xb - 45, cy - 40, 110, xb - 15, cy + 40, 170, "screen", folder, "Terminal_Screen", collide=False)
        term = spawn(unreal.DarcMissionTerminal, (xb - 40, cy, 120), 180.0, folder, "MissionTerminal")
        term.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(40, 60, 70))
    elif kind == "rest":
        box(x - 200, back - sign * 80, 0, x + 200, back, 45, "furniture", folder, "Sofa")
        box(x - 60, cy - 40, 0, x + 60, cy + 40, 40, "furniture", folder, "Table")
        box(xa + 10, cy - 60, 0, xa + 70, cy + 60, 100, "furniture", folder, "CoffeeCounter")
    elif kind == "archive":
        for i in range(3):
            box(xa + 60 + i * 260, back - sign * 60, 0, xa + 220 + i * 260, back, 230, "furniture", folder, "Shelf_%d" % i)
        box(x - 40, cy - 30, 0, x + 40, cy + 30, 110, "furniture", folder, "ArchiveTerminal")
        box(x - 35, cy - 25, 110, x + 35, cy + 25, 160, "screen", folder, "ArchiveScreen", collide=False)
    elif kind == "equipment":
        for i in range(5):
            box(xa + 40 + i * 150, back - sign * 55, 0, xa + 170 + i * 150, back, 200, "trim", folder, "Locker_%d" % i)
        box(x - 150, cy - 40, 0, x + 150, cy + 40, 90, "furniture", folder, "ToolBench")
    elif kind == "evidence":
        for i in range(4):
            box(xa + 40 + i * 200, back - sign * 70, 0, xa + 200 + i * 200, back, 250, "trim", folder, "Rack_%d" % i)
    elif kind == "briefing":
        box(x - 220, cy - 80, 0, x + 220, cy + 80, 75, "furniture", folder, "MeetingTable")
        box(xa + 4, cy - 150, 110, xa + 8, cy + 150, 260, "screen", folder, "Projection", collide=False)
    elif kind == "medbay":
        box(x - 100, back - sign * 90, 0, x + 100, back, 70, "ceiling", folder, "Bed")
        box(xb - 60, cy - 50, 0, xb - 10, cy + 50, 180, "furniture", folder, "Cabinet")


for side, n, kind, label_key, fact in DOORS:
    x = DOOR_X[n]
    sign = 1 if side == "R" else -1
    folder = "Rooms/%s%d" % (side, n + 1)
    # Створка — разметка DARC.Door: заперта, ломом не взломать; открывается фактом (UnlockFact=).
    wall_mid = (COR_HALF + WALL / 2) * sign
    leaf = box(x - DOOR_W / 2 + 2, wall_mid - 3, 0, x + DOOR_W / 2 - 2, wall_mid + 3, DOOR_H - 2, "door",
               folder + "/Doors", "DoorLeaf")
    flags = "DARC.Door" if fact == "open" else "DARC.Door.Locked.NoPry"
    tags = [flags, "Id=BaseDoor_%s%d" % (side, n + 1)]
    if fact and fact != "open":
        tags.append("UnlockFact=" + fact)
    tag(leaf, tags)
    # Рамка и табличка
    face = (COR_HALF - 1) * sign
    box(x - DOOR_W / 2 - 8, face - 2, 0, x - DOOR_W / 2, face + 2, DOOR_H + 8, "trim", folder + "/Doors", "Frame_L", collide=False)
    box(x + DOOR_W / 2, face - 2, 0, x + DOOR_W / 2 + 8, face + 2, DOOR_H + 8, "trim", folder + "/Doors", "Frame_R", collide=False)
    box(x - DOOR_W / 2 - 8, face - 2, DOOR_H, x + DOOR_W / 2 + 8, face + 2, DOOR_H + 8, "trim", folder + "/Doors", "Frame_T", collide=False)
    yaw_text = 90.0 if side == "L" else -90.0
    num = "%s%d" % ("Б" if side == "R" else "А", n + 1)
    text((x, face - sign * 2, DOOR_H + 40), yaw_text, num, 26, folder + "/Doors", "Number")
    # Считыватель у двери: красный — заперто (у открываемых двери меняется сам замок, огонёк — декор)
    box(x + DOOR_W / 2 + 25, face - sign * 4, 120, x + DOOR_W / 2 + 40, face, 145, "door", folder + "/Doors", "Reader", collide=False)
    if fact != "open":
        box(x + DOOR_W / 2 + 29, face - sign * 5, 138, x + DOOR_W / 2 + 36, face - sign * 4, 142, "red", folder + "/Doors", "ReaderLed", collide=False)

    if kind == "lightdoor":
        # Щель под дверью — свет из-за двери в коридор.
        ld = spawn(unreal.DarcLightDoor, (x, face - sign * 3, 2), 0.0, folder, "LightDoor")
        ld.set_actor_rotation(unreal.Rotator(0, 0, -90.0 * sign), False)
    elif kind:
        build_room(side, x, kind, folder)

# ---------- освещение и настройки мира ----------
pp = spawn(unreal.PostProcessVolume, (COR_LEN / 2, 0, 200), 0.0, "Lighting", "PostProcess")
pp.set_editor_property("unbound", True)
settings = pp.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_min_brightness", True)
settings.set_editor_property("auto_exposure_min_brightness", 3.0)  # EV100: помещение, без пересвета
settings.set_editor_property("override_auto_exposure_max_brightness", True)
settings.set_editor_property("auto_exposure_max_brightness", 7.0)
pp.set_editor_property("settings", settings)
sky = spawn(unreal.SkyLight, (0, 0, 800), 0.0, "Lighting", "SkyLight")
sky.get_component_by_class(unreal.SkyLightComponent).set_intensity(0.15)

ws = world.get_world_settings()
ws.set_editor_property("default_game_mode", unreal.load_class(None, "/Script/DARK.DarcGameMode"))

unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
unreal.log("DARCI base built: %s" % MAP)
