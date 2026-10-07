# darc_place_batteries_voicelogs.py
# Выезд 1: батарейки для фонарика (DARC.Battery) и голосовые записи (DARC.VoiceLog) на столах.
# Повторный запуск сначала удаляет то, что поставил этот скрипт (метка DarcPlacedBy=BatteriesVoiceLogs).
import unreal

MARK = "DarcPlacedBy=BatteriesVoiceLogs"
BATTERY_MESH = "/Game/Sci_Fi_Light/Static_Meshes/SM_Sci_Fi_Battery_Unit/Mesh/SM_Sci_Fi_Battery_Unit"
PHONE_MESH = "/Game/Deko_MatrixDemo/Apartment/Meshes/SM_Phone_A01_N1"

# (метка, x, y, верх поверхности, поворот)
BATTERIES = [
    ("Battery_TechRoom_1", -9345, 425, 2162, 20),     # стол с инструментами (там же лом) — про них говорит охранник
    ("Battery_TechRoom_2", -9375, 385, 2162, 75),
    ("Battery_Security", -7385, 30, 1729, 0),         # стол охраны
    ("Battery_WorkRoom", -8330, 655, 2160, 40),       # стол с кружкой
    ("Battery_Electrical", -9280, 320, 1719, 10),     # коробка в электрощитовой
    ("Battery_Records", -5850, 2865, 2160, 95),       # стол в комнате записей
]
# (метка, Log, сколько реплик, x, y, верх, поворот)
VOICE_LOGS = [
    ("VoiceLog_Security", "Security", 4, -6915, 55, 1729, 90),
    ("VoiceLog_WorkRoom", "WorkRoom", 4, -8785, 55, 2160, 90),
    ("VoiceLog_Records", "Records", 4, -5845, 2655, 2160, 180),
]

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
removed = 0
for a in sub.get_all_level_actors():
    if MARK in [str(t) for t in a.tags]:
        sub.destroy_actor(a)
        removed += 1


def place(label, mesh_path, scale, x, y, top, yaw, tags, lift):
    mesh = unreal.load_asset(mesh_path)
    actor = sub.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(x, y, top + 50), unreal.Rotator(0, 0, yaw))
    actor.static_mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    # Низ модели — на поверхность (+ небольшой запас: физический предмет сам ляжет).
    origin, extent = actor.get_actor_bounds(False)
    loc = actor.get_actor_location()
    actor.set_actor_location(unreal.Vector(x, y, loc.z - (origin.z - extent.z) + top + lift), False, False)
    actor.set_actor_label(label)
    actor.set_folder_path("DARC/Gameplay/BatteriesVoiceLogs")
    actor.modify()
    actor.set_editor_property("tags", [unreal.Name(t) for t in tags + [MARK]])
    return actor


for label, x, y, top, yaw in BATTERIES:
    place(label, BATTERY_MESH, 0.15, x, y, top, yaw, ["DARC.Battery", "Name=Item_Battery"], 1.0)
for label, log_id, lines, x, y, top, yaw in VOICE_LOGS:
    place(label, PHONE_MESH, 1.0, x, y, top, yaw, ["DARC.VoiceLog", "Log=" + log_id, "Lines=%d" % lines], 0.2)

unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log("[DARC] batteries %d, voice logs %d placed (removed old %d)" % (len(BATTERIES), len(VOICE_LOGS), removed))
