# Присед: blend space BS_DarcCrouch (/Game/DARC/Anims) из анимаций FreeAnimationLibrary/Animations/Crouch.
# Ось X — направление движения относительно взгляда (-180..180), ось Y — скорость (0..300 см/с).
# Его проигрывает ABP_Unarmed, когда персонаж пригнулся (узел ставит этот же скрипт — см. ниже,
# параметры берутся из UDarcGameplayLibrary::GetLocomotionDirection / GetLocomotionSpeed / IsPawnCrouching).
# Запуск: py C:/D.A.R.K/DARK/Tools/darc_setup_crouch.py
import unreal

SRC = "/Game/FreeAnimationLibrary/Animations/Crouch/"
DIR = "/Game/DARC/Anims"
NAME = "BS_DarcCrouch"
SKELETON = "/Game/FreeAnimationLibrary/Demo/Characters/Mannequins/Meshes/SK_Mannequin"
SPEED = 160.0  # скорость, на которой записаны анимации шага вприсядку (примерно)

# (анимация, направление, скорость)
SAMPLES = [
    ("anim_Crouch_Idle", -180.0, 0.0), ("anim_Crouch_Idle", -90.0, 0.0), ("anim_Crouch_Idle", 0.0, 0.0),
    ("anim_Crouch_Idle", 90.0, 0.0), ("anim_Crouch_Idle", 180.0, 0.0),
    ("anim_Crouch_Fwd_Loop", 0.0, SPEED),
    ("anim_Crouch_Fwd_Left_Loop", -45.0, SPEED),
    ("anim_Crouch_Fwd_Right_Loop", 45.0, SPEED),
    ("anim_Crouch_Left_Loop", -90.0, SPEED),
    ("anim_Crouch_Right_Loop", 90.0, SPEED),
    ("anim_Crouch_Bwd_Left_Loop", -135.0, SPEED),
    ("anim_Crouch_Bwd_Right_Loop", 135.0, SPEED),
    ("anim_Crouch_Bwd_Loop", -180.0, SPEED),
    ("anim_Crouch_Bwd_Loop", 180.0, SPEED),
]

path = "%s/%s" % (DIR, NAME)
if unreal.EditorAssetLibrary.does_asset_exist(path):
    bs = unreal.load_asset(path)
else:
    factory = unreal.BlendSpaceFactoryNew()
    factory.set_editor_property("target_skeleton", unreal.load_asset(SKELETON))
    bs = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, DIR, unreal.BlendSpace, factory)

samples = []
for anim, direction, speed in SAMPLES:
    s = unreal.BlendSample()
    s.set_editor_property("animation", unreal.load_asset(SRC + anim))
    s.set_editor_property("sample_value", unreal.Vector(direction, speed, 0.0))
    s.set_editor_property("rate_scale", 1.0)
    samples.append(s)
bs.set_editor_property("sample_data", samples)

params = list(bs.get_editor_property("blend_parameters"))
x = params[0]; x.set_editor_property("display_name", "Direction"); x.set_editor_property("min", -180.0); x.set_editor_property("max", 180.0); x.set_editor_property("grid_num", 8)
y = params[1]; y.set_editor_property("display_name", "Speed"); y.set_editor_property("min", 0.0); y.set_editor_property("max", 300.0); y.set_editor_property("grid_num", 4)
# Оси — после точек: смена осей пересчитывает сетку blend space (иначе он отдаёт T-позу).
bs.set_editor_property("blend_parameters", params)

unreal.EditorAssetLibrary.save_loaded_asset(bs)
unreal.log("DARCI crouch blendspace: %s samples=%d" % (bs.get_path_name(), len(bs.get_editor_property("sample_data"))))
