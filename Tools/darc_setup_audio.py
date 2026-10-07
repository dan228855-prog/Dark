# Категории громкости для настроек игрока: классы звука SC_Music / SC_SFX / SC_Voice / SC_Radio и микс
# SM_DarcVolumes в /Game/DARC/Audio (громкость ставит UDarcGameUserSettings::ApplyVolumes).
# Звукам из слотов DefaultGame.ini назначается класс: радио (CarRadio*, RadioVoice*, RadioClick,
# Slice_SpeakerRadio) → SC_Radio, музыка (*Music*) → SC_Music, остальное → SC_SFX.
# Всё без класса берёт SC_SFX (DefaultSoundClassName), голосовой чат — SC_Voice (VoiPSoundClass).
# Запуск: py C:/D.A.R.K/DARK/Tools/darc_setup_audio.py (повторный запуск безопасен).
import os
import re
import unreal

DIR = "/Game/DARC/Audio"
tools = unreal.AssetToolsHelpers.get_asset_tools()


def get_or_create(name, cls, factory):
    path = "%s/%s" % (DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, DIR, cls, factory)


classes = {n: get_or_create(n, unreal.SoundClass, unreal.SoundClassFactory())
           for n in ("SC_Music", "SC_SFX", "SC_Voice", "SC_Radio")}
mix = get_or_create("SM_DarcVolumes", unreal.SoundMix, unreal.SoundMixFactory())
for a in list(classes.values()) + [mix]:
    unreal.EditorAssetLibrary.save_loaded_asset(a)


def category(slot):
    if re.match(r"^(CarRadio|RadioVoice|RadioClick|Slice_SpeakerRadio)", slot):
        return "SC_Radio"
    if "Music" in slot:
        return "SC_Music"
    return "SC_SFX"


ini = open(os.path.join(unreal.Paths.project_config_dir(), "DefaultGame.ini"), encoding="utf-8").read()
assigned = {}
for slot, path in re.findall(r'\("([^"]+)",\s*"(/Game/[^"]+)"\)', ini):
    if "/Sounds/" not in path:
        continue
    cat = category(slot)
    # Радио/музыка важнее «эффектов», если один файл стоит в нескольких слотах.
    if assigned.get(path) in ("SC_Radio", "SC_Music") and cat == "SC_SFX":
        continue
    assigned[path] = cat

count = 0
for path, cat in assigned.items():
    sound = unreal.load_asset(path.split(".")[0])
    if not sound:
        unreal.log_warning("DARCI no sound %s" % path)
        continue
    if sound.get_editor_property("sound_class_object") != classes[cat]:
        sound.set_editor_property("sound_class_object", classes[cat])
        unreal.EditorAssetLibrary.save_loaded_asset(sound)
        count += 1
unreal.log("DARCI audio classes ready, sounds updated: %d of %d" % (count, len(assigned)))
