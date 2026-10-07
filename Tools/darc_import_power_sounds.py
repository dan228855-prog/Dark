# Гул рабочих ламп (слот LampHum, тихий) и работающий генератор (GeneratorLoop):
# RawAssets/Sounds/Loops/*.mp3 → /Game/DARC/Imported/Sounds/Loops, зацикленные, класс «Эффекты».
# Запуск в редакторе (не во время Play): py C:/D.A.R.K/DARK/Tools/darc_import_power_sounds.py
import os
import unreal

RAW = os.path.join(unreal.Paths.project_dir(), "RawAssets", "Sounds", "Loops")
DST = "/Game/DARC/Imported/Sounds/Loops"
# файл → (имя ассета, громкость самого звука)
SOUNDS = {
    "soundsforyou_electric_hum_141075.mp3": ("S_soundsforyou_electric_hum_141075", 0.18),  # лампы: еле слышно
    "l3hrja_running_generator_163754.mp3": ("S_l3hrja_running_generator_163754", 0.8),
}
sfx = unreal.load_asset("/Game/DARC/Audio/SC_SFX")
tasks = []
for file_name, (asset_name, _) in SOUNDS.items():
    t = unreal.AssetImportTask()
    t.filename = os.path.join(RAW, file_name)
    t.destination_path = DST
    t.destination_name = asset_name
    t.automated = True
    t.replace_existing = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for file_name, (asset_name, volume) in SOUNDS.items():
    sound = unreal.load_asset("%s/%s" % (DST, asset_name))
    if not sound:
        unreal.log_warning("DARCI not imported: %s" % file_name)
        continue
    sound.set_editor_property("looping", True)
    sound.set_editor_property("volume", volume)
    if sfx:
        sound.set_editor_property("sound_class_object", sfx)
    unreal.EditorAssetLibrary.save_loaded_asset(sound)
    unreal.log("DARCI sound ready: %s vol=%.2f" % (sound.get_path_name(), volume))
