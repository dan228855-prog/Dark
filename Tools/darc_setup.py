# -*- coding: utf-8 -*-
"""
D.A.R.C. — настройка проекта одной командой (запускать в редакторе Unreal):
    Tools → Execute Python Script... → выбрать этот файл (Tools/darc_setup.py)

Что делает (можно запускать повторно — уже сделанное пропускает):
  1. Создаёт пустую карту среза /Game/DARC/Maps/L_Slice (срез строится на ней кодом).
  2. Импортирует скачанные файлы из папки RawAssets/ в корне проекта:
       модели  (.fbx .obj .gltf .glb)          → /Game/DARC/Imported/Meshes/...
       текстуры (.png .jpg .jpeg .tga .exr .hdr) → /Game/DARC/Imported/Textures/...
       звуки   (.wav .ogg .flac .mp3)          → /Game/DARC/Imported/Sounds/...
     Подпапки сохраняются. Звуки из папки с "loop" или "ambien" в пути — зацикленные.
     Из наборов текстур (…_diff / _nor / _rough / _ao …) сам собирает простой материал M_<набор>.
  3. Импортирует таблицы из docs/data (задачи среза, редкие события) в /Game/DARC/Data
     и создаёт ассет выезда DA_Mission_Slice.
  4. Составляет список ВСЕХ моделей, материалов и звуков проекта → docs/asset_inventory.csv
     Этот файл нужно запушить в GitHub (или прислать) — по нему Claude расставит ассеты.
"""

import csv
import os
import re

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
RAW_DIR = os.path.join(PROJECT_DIR, "RawAssets")
DATA_DIR = os.path.join(PROJECT_DIR, "docs", "data")
INVENTORY_PATH = os.path.join(PROJECT_DIR, "docs", "asset_inventory.csv")

ROOT = "/Game/DARC"
MAP_PATH = ROOT + "/Maps/L_Slice"
DATA_PATH = ROOT + "/Data"

MESH_EXT = {".fbx", ".obj", ".gltf", ".glb"}
TEX_EXT = {".png", ".jpg", ".jpeg", ".tga", ".exr", ".hdr"}
SOUND_EXT = {".wav", ".ogg", ".flac", ".mp3"}
ARCHIVE_EXT = {".zip", ".rar", ".7z"}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
log = unreal.log
report = []


def say(msg):
    log("[DARC] " + msg)
    report.append(msg)


def clean_name(name):
    """Имя ассета: только латиница/цифры/_ (кириллица и пробелы в путях UE мешают)."""
    name = re.sub(r"[^A-Za-z0-9_]", "_", name)
    return re.sub(r"_+", "_", name).strip("_") or "Asset"


# ---------------------------------------------------------------------------
# 1. Карта
# ---------------------------------------------------------------------------
def ensure_map():
    if eal.does_asset_exist(MAP_PATH):
        say("Карта уже есть: " + MAP_PATH)
    else:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if les.new_level(MAP_PATH):
            say("Создана пустая карта среза: " + MAP_PATH)
        else:
            say("!! Не удалось создать карту " + MAP_PATH)
            return
    ensure_slice_builder()


def ensure_slice_builder():
    """На карте должен стоять актёр DarcSliceBuilder — именно он на Play строит всю
    геометрию и игровые объекты среза (DarcSliceBuilder.cpp, PostInitializeComponents).
    new_level() создаёт СОВСЕМ пустую карту без него, а UCLASS(NotPlaceable) не даёт
    поставить его руками через Place Actors — поэтому без этого шага срез на Play
    оставался буквально пустым: только игрок и свет, ни одной стены (баг, из-за
    которого экран казался «тёмным» — на самом деле просто нечему было освещать)."""
    # load_level безопасно вызвать даже если эта карта уже открыта — просто перечитает её.
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.load_level(MAP_PATH)

    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.DarcSliceBuilder):
            say("DarcSliceBuilder уже стоит на карте.")
            return

    builder = actor_subsystem.spawn_actor_from_class(unreal.DarcSliceBuilder, unreal.Vector(0.0, 0.0, 0.0))
    if builder:
        say("Добавлен DarcSliceBuilder на карту L_Slice (без него срез был пустым).")
        unreal.EditorLevelLibrary.save_current_level()
    else:
        say("!! Не удалось добавить DarcSliceBuilder на карту — срез останется пустым.")


# ---------------------------------------------------------------------------
# 2. Импорт файлов из RawAssets
# ---------------------------------------------------------------------------
def make_task(filename, destination, name):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", False)
    task.set_editor_property("save", True)
    return task


def fbx_options():
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.static_mesh_import_data.set_editor_property("combine_meshes", True)
    options.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", False)
    return options


def import_raw_assets():
    if not os.path.isdir(RAW_DIR):
        os.makedirs(os.path.join(RAW_DIR, "Meshes"), exist_ok=True)
        os.makedirs(os.path.join(RAW_DIR, "Textures"), exist_ok=True)
        os.makedirs(os.path.join(RAW_DIR, "Sounds", "Loops"), exist_ok=True)
        say("Папки RawAssets не было — создана (Meshes / Textures / Sounds/Loops). Положи туда файлы и запусти скрипт ещё раз.")
        return

    tasks, archives, looping = [], [], []
    for folder, _, files in os.walk(RAW_DIR):
        rel = os.path.relpath(folder, RAW_DIR)
        rel_parts = [clean_name(p) for p in rel.split(os.sep) if p not in (".", "")]
        for file in files:
            path = os.path.join(folder, file)
            base, ext = os.path.splitext(file)
            ext = ext.lower()
            name = clean_name(base)

            if ext in ARCHIVE_EXT:
                archives.append(os.path.relpath(path, RAW_DIR))
                continue
            if ext in MESH_EXT:
                kind, prefix = "Meshes", "SM_"
            elif ext in TEX_EXT:
                kind, prefix = "Textures", "T_"
            elif ext in SOUND_EXT:
                kind, prefix = "Sounds", "S_"
            else:
                continue

            # Тип уже задан верхней папкой (RawAssets/Meshes/...) — не дублируем его в пути.
            parts = rel_parts[1:] if rel_parts and rel_parts[0].lower() == kind.lower() else rel_parts
            destination = "/".join([ROOT, "Imported", kind] + parts)
            asset_name = name if name.startswith(prefix) else prefix + name
            if eal.does_asset_exist(destination + "/" + asset_name):
                continue

            task = make_task(path, destination, asset_name)
            if ext == ".fbx":
                task.set_editor_property("options", fbx_options())
            tasks.append(task)
            lowered = rel.lower()
            if kind == "Sounds" and ("loop" in lowered or "ambien" in lowered):
                looping.append(destination + "/" + asset_name)

    if archives:
        say("!! Архивы не импортируются — распакуй их в RawAssets: " + ", ".join(archives[:10]))
    if not tasks:
        say("Новых файлов для импорта нет.")
        return

    say("Импорт файлов: %d ..." % len(tasks))
    asset_tools.import_asset_tasks(tasks)
    imported = sum(len(t.get_editor_property("imported_object_paths") or []) for t in tasks)
    say("Импортировано ассетов: %d" % imported)

    for path in looping:
        sound = eal.load_asset(path)
        if isinstance(sound, unreal.SoundWave):
            sound.set_editor_property("looping", True)
            eal.save_loaded_asset(sound)


# Наборы текстур Poly Haven / ambientCG: имя_набора + суффикс карты.
TEX_ROLES = [
    ("BaseColor", r"(diff|diffuse|albedo|basecolor|base_color|color|col)$"),
    ("Normal", r"(nor|normal|nrm|nor_gl|normal_gl|normalgl|nor_dx|normaldx)$"),
    ("Roughness", r"(rough|roughness|rgh)$"),
    ("AO", r"(ao|ambientocclusion|occlusion)$"),
    ("Metallic", r"(metal|metallic|metalness)$"),
]


def build_materials():
    """Из наборов текстур делает простые материалы M_<набор> (цвет + нормаль + шероховатость)."""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    textures = registry.get_assets_by_path(ROOT + "/Imported/Textures", recursive=True)
    sets = {}
    for data in textures:
        name = str(data.asset_name)
        # Разрешение (_4k и т.п.) часто стоит ПОСЛЕ роли (..._diff_4k) - срезаем его
        # заранее, иначе роль-паттерн с якорем $ не находит совпадение.
        base = re.sub(r"_(1k|2k|4k|8k)$", "", name, flags=re.I)
        lowered = base.lower()
        for role, pattern in TEX_ROLES:
            m = re.search(r"_" + pattern, lowered)
            if m:
                set_name = base[: m.start()]
                set_name = re.sub(r"^T_", "", set_name)
                sets.setdefault(set_name, {})[role] = str(data.package_name)
                break

    mel = unreal.MaterialEditingLibrary
    made = 0
    for set_name, maps in sets.items():
        if "BaseColor" not in maps:
            continue
        mat_path = ROOT + "/Imported/Materials/M_" + clean_name(set_name)
        if eal.does_asset_exist(mat_path):
            continue
        material = asset_tools.create_asset("M_" + clean_name(set_name), ROOT + "/Imported/Materials",
                                            unreal.Material, unreal.MaterialFactoryNew())
        y = 0
        for role, prop in [("BaseColor", unreal.MaterialProperty.MP_BASE_COLOR),
                           ("Normal", unreal.MaterialProperty.MP_NORMAL),
                           ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS),
                           ("Metallic", unreal.MaterialProperty.MP_METALLIC),
                           ("AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)]:
            if role not in maps:
                continue
            texture = eal.load_asset(maps[role])
            if role == "Normal":
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                texture.set_editor_property("srgb", False)
            elif role != "BaseColor":
                texture.set_editor_property("srgb", False)
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            eal.save_loaded_asset(texture)
            node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, y)
            node.set_editor_property("texture", texture)
            if role == "Normal":
                node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
            elif role != "BaseColor":
                node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            output = "RGB" if role in ("BaseColor", "Normal") else "R"
            mel.connect_material_property(node, output, prop)
            y += 260
        mel.recompile_material(material)
        eal.save_loaded_asset(material)
        made += 1
    if made:
        say("Собрано материалов из наборов текстур: %d" % made)


# ---------------------------------------------------------------------------
# 3. Таблицы и ассет выезда
# ---------------------------------------------------------------------------
def import_table(csv_name, asset_name, row_struct_path):
    csv_path = os.path.join(DATA_DIR, csv_name)
    if not os.path.isfile(csv_path):
        say("!! Нет файла " + csv_path)
        return None
    row_struct = unreal.load_object(None, row_struct_path)
    if row_struct is None:
        say("!! Не найдена структура " + row_struct_path + " — проект собран? (C++ модуль DARK)")
        return None

    factory = unreal.CSVImportFactory()
    settings = factory.get_editor_property("automated_import_settings")
    settings.set_editor_property("import_type", unreal.CSVImportType.ECSV_DATA_TABLE)
    settings.set_editor_property("import_row_struct", row_struct)

    task = make_task(csv_path, DATA_PATH, asset_name)
    task.set_editor_property("replace_existing", True)  # таблицы всегда обновляем из CSV
    task.set_editor_property("factory", factory)
    asset_tools.import_asset_tasks([task])
    table = eal.load_asset(DATA_PATH + "/" + asset_name)
    say("Таблица обновлена: %s/%s" % (DATA_PATH, asset_name) if table else "!! Не удалось импортировать " + csv_name)
    return table


def ensure_mission(task_table):
    path = DATA_PATH + "/DA_Mission_Slice"
    mission_class = unreal.load_class(None, "/Script/DARK.DarcMissionDefinition")
    if eal.does_asset_exist(path):
        mission = eal.load_asset(path)
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", mission_class)
        mission = asset_tools.create_asset("DA_Mission_Slice", DATA_PATH, mission_class, factory)
    if not mission:
        say("!! Не удалось создать DA_Mission_Slice")
        return
    mission.set_editor_property("task_table", task_table)
    mission.set_editor_property("level_index", 1)
    mission.set_editor_property("allow_strong_rare_events", False)
    mission.set_editor_property("campaign_facts_on_success", ["Base.ArchiveTerminal"])
    eal.save_loaded_asset(mission)
    say("Ассет выезда готов: " + path)


# ---------------------------------------------------------------------------
# 4. Список ассетов для Claude
# ---------------------------------------------------------------------------
INVENTORY_CLASSES = {
    "StaticMesh": "mesh",
    "SkeletalMesh": "skeletal",
    "Material": "material",
    "MaterialInstanceConstant": "material",
    "SoundWave": "sound",
    "SoundCue": "sound",
    "MetaSoundSource": "sound",
}


def write_inventory():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    rows = []
    for data in registry.get_assets_by_path("/Game", recursive=True):
        try:
            cls = str(data.asset_class_path.asset_name)
        except AttributeError:
            cls = str(data.asset_class)
        kind = INVENTORY_CLASSES.get(cls)
        if not kind:
            continue
        package = str(data.package_name)
        if package.startswith(DATA_PATH) or package.startswith(ROOT + "/Maps"):
            continue
        name = str(data.asset_name)
        info = ""
        if kind in ("mesh", "skeletal"):
            info = str(data.get_tag_value("ApproxSize") or "")  # примерный размер, см
        elif kind == "sound":
            info = str(data.get_tag_value("Duration") or "")
        rows.append([kind, cls, package + "." + name, info])

    rows.sort()
    os.makedirs(os.path.dirname(INVENTORY_PATH), exist_ok=True)
    with open(INVENTORY_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["kind", "class", "object_path", "size_or_duration"])
        writer.writerows(rows)
    counts = {}
    for row in rows:
        counts[row[0]] = counts.get(row[0], 0) + 1
    say("Список ассетов: %s → %s" % (", ".join("%s: %d" % kv for kv in sorted(counts.items())), INVENTORY_PATH))


# ---------------------------------------------------------------------------
def main():
    for path in [ROOT, ROOT + "/Maps", DATA_PATH, ROOT + "/Imported"]:
        if not eal.does_directory_exist(path):
            eal.make_directory(path)

    with unreal.ScopedSlowTask(5, "D.A.R.C.: настройка проекта") as slow:
        slow.make_dialog(True)
        slow.enter_progress_frame(1, "Карта среза")
        ensure_map()
        slow.enter_progress_frame(1, "Импорт файлов из RawAssets")
        import_raw_assets()
        build_materials()
        slow.enter_progress_frame(1, "Таблицы среза")
        tasks = import_table("DT_Mission_Slice_Tasks.csv", "DT_Mission_Slice_Tasks", "/Script/DARK.DarcTaskDefinition")
        import_table("DT_RareEvents.csv", "DT_RareEvents", "/Script/DARK.DarcRareEventRow")
        if tasks:
            ensure_mission(tasks)
        slow.enter_progress_frame(1, "Список ассетов")
        write_inventory()
        slow.enter_progress_frame(1, "Готово")

    log("[DARC] DONE")
    try:
        unreal.EditorDialog.show_message(
            "D.A.R.C. — настройка завершена",
            "\n".join(report) + "\n\nДальше: запушь docs/asset_inventory.csv в GitHub (или пришли Claude), "
            "потом открой карту L_Slice и нажми Play.",
            unreal.AppMsgType.OK)
    except Exception:
        pass  # запуск без окна (из командной строки) — отчёт уже в логе


main()
