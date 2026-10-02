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
import json
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
# Форматы, которые импортирует Unreal. Раньше .tif/.tiff/.bmp/.psd/.dds тихо пропускались —
# поэтому часть присланных текстур (Poliigon и др. отдают TIFF) в проект не попадала.
TEX_EXT = {".png", ".jpg", ".jpeg", ".tga", ".exr", ".hdr", ".tif", ".tiff", ".bmp", ".psd", ".dds", ".pcx"}
# Картинки, которые Unreal НЕ импортирует: о них — в отчёте, их нужно пересохранить в PNG.
UNSUPPORTED_IMAGE_EXT = {".webp", ".avif", ".heic", ".heif", ".gif", ".jxl", ".svg", ".ktx", ".ktx2", ".jp2"}
MANIFEST_PATH = os.path.join(PROJECT_DIR, "Saved", "DarcImport", "manifest.json")
TEXTURE_REPORT_PATH = os.path.join(PROJECT_DIR, "docs", "texture_report.csv")
SOUND_EXT = {".wav", ".ogg", ".flac", ".mp3"}
ARCHIVE_EXT = {".zip", ".rar", ".7z"}

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
log = unreal.log
report = []
texture_report = []      # строки отчёта по текстурам: файл, статус, ассет
changed_textures = set()  # ассеты текстур, импортированные/обновлённые в этом запуске


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
def make_task(filename, destination, name, replace=False):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", filename)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", replace)
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


def load_manifest():
    try:
        with open(MANIFEST_PATH, encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None  # первого запуска с манифестом ещё не было


def save_manifest(data):
    os.makedirs(os.path.dirname(MANIFEST_PATH), exist_ok=True)
    with open(MANIFEST_PATH, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=0)


def file_stamp(path):
    st = os.stat(path)
    return "%d:%d" % (int(st.st_mtime), st.st_size)


def import_raw_assets():
    if not os.path.isdir(RAW_DIR):
        os.makedirs(os.path.join(RAW_DIR, "Meshes"), exist_ok=True)
        os.makedirs(os.path.join(RAW_DIR, "Textures"), exist_ok=True)
        os.makedirs(os.path.join(RAW_DIR, "Sounds", "Loops"), exist_ok=True)
        say("Папки RawAssets не было — создана (Meshes / Textures / Sounds/Loops). Положи туда файлы и запусти скрипт ещё раз.")
        return

    tasks, archives, looping, unsupported = [], [], [], []
    manifest = load_manifest() or {}
    first_manifest_run = load_manifest() is None
    if first_manifest_run:
        manifest = None
    new_manifest = {}
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
            if ext in UNSUPPORTED_IMAGE_EXT:
                unsupported.append(os.path.relpath(path, RAW_DIR))
                texture_report.append([os.path.relpath(path, RAW_DIR), "НЕ ПОДДЕРЖИВАЕТСЯ: пересохранить в PNG", ""])
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
            asset_path = destination + "/" + asset_name
            stamp = file_stamp(path)
            key = os.path.relpath(path, RAW_DIR)
            new_manifest[key] = stamp
            replace = False
            if eal.does_asset_exist(asset_path):
                # Файл заменили (другая дата/размер) — переимпортировать. Раньше уже
                # импортированное имя пропускалось навсегда, и в игре оставалась старая текстура.
                # Первый запуск с манифестом: текстуры переимпортируются один раз (неизвестно,
                # совпадают ли они с файлами), звуки и модели — нет.
                known = manifest.get(key) if manifest is not None else None
                if known == stamp or (known is None and kind != "Textures"):
                    if kind == "Textures":
                        texture_report.append([key, "уже в проекте", asset_path])
                    continue
                replace = True

            task = make_task(path, destination, asset_name, replace)
            if kind == "Textures":
                texture_report.append([key, "переимпортирована (файл изменился)" if replace else "импортирована", asset_path])
                changed_textures.add(asset_path)
            if ext == ".fbx":
                task.set_editor_property("options", fbx_options())
            tasks.append(task)
            lowered = rel.lower()
            if kind == "Sounds" and ("loop" in lowered or "ambien" in lowered):
                looping.append(destination + "/" + asset_name)

    if archives:
        say("!! Архивы не импортируются — распакуй их в RawAssets: " + ", ".join(archives[:10]))
    if unsupported:
        say("!! Формат картинок не поддерживается Unreal (нужен PNG, TGA, JPG или TIFF): "
            + ", ".join(unsupported[:10]) + (" и ещё %d" % (len(unsupported) - 10) if len(unsupported) > 10 else ""))
    save_manifest(new_manifest)
    if not tasks:
        say("Новых или изменённых файлов для импорта нет.")
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
    ("BaseColor", r"(diff|diffuse|albedo|basecolor|base_color|color|col|bc)$"),
    ("Normal", r"(nor|normal|nrm|nor_gl|normal_gl|normalgl|nor_dx|normaldx|n)$"),
    ("Roughness", r"(rough|roughness|rgh|r)$"),
    ("Gloss", r"(gloss|glossiness)$"),
    ("AO", r"(ao|ambientocclusion|occlusion)$"),
    ("Metallic", r"(metal|metallic|metalness|m)$"),
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
        # Разрешение в конце: _4k или в пикселях — _2048 (набор вида T_Armstrong_BC_2048).
        base = re.sub(r"_(1k|2k|3k|4k|6k|8k|256|512|1024|2048|4096|8192)$", "", name, flags=re.I)
        base = re.sub(r"_var\d+$", "", base, flags=re.I)  # Poliigon: ..._COL_VAR1_4K
        lowered = base.lower()
        for role, pattern in TEX_ROLES:
            m = re.search(r"_" + pattern, lowered)
            if m:
                set_name = base[: m.start()]
                set_name = re.sub(r"^T_", "", set_name)
                maps = sets.setdefault(set_name, {})
                # Нормаль: Unreal нужна DirectX. Если в наборе есть и GL, и DX — берём DX.
                if role == "Normal" and "Normal" in maps and "dx" not in lowered[m.start():]:
                    break
                maps[role] = str(data.package_name)
                break
        else:
            texture_report.append([name, "роль не распознана по имени (нужен суффикс _Color/_Normal/_Roughness/_AO/_Metallic)", str(data.package_name)])

    mel = unreal.MaterialEditingLibrary
    made = 0
    for set_name, maps in sets.items():
        if "BaseColor" not in maps:
            # Частая причина «текстуры не используются»: имя цветовой карты не распознано.
            texture_report.append(["набор " + set_name, "МАТЕРИАЛ НЕ СОБРАН: нет карты цвета (_Color/_BaseColor/_diff/_albedo/_COL); есть: "
                                   + "/".join(sorted(maps)), ""])
            continue
        mat_name = "M_" + clean_name(set_name)
        texture_report.append(["набор " + set_name, "материал: " + "/".join(sorted(maps)), ROOT + "/Imported/Materials/" + mat_name])
        mat_path = ROOT + "/Imported/Materials/" + mat_name
        if eal.does_asset_exist(mat_path):
            material = eal.load_asset(mat_path)
            # Уже собран этой версией и текстуры не менялись — не трогаем.
            params = [str(n) for n in mel.get_scalar_parameter_names(material)]
            textures_changed = any(path.split(".")[0] in changed_textures for path in maps.values())
            vector_params = [str(n) for n in mel.get_vector_parameter_names(material)]
            if "MacroVariation" in params and "BoxSize" in vector_params and not textures_changed:
                continue
            mel.delete_all_material_expressions(material)  # старая версия: текстура растягивалась
        else:
            material = asset_tools.create_asset(mat_name, ROOT + "/Imported/Materials",
                                                unreal.Material, unreal.MaterialFactoryNew())
        build_material_graph(material, maps)
        mel.recompile_material(material)
        eal.save_loaded_asset(material)
        made += 1
    if made:
        say("Собрано/обновлено материалов из наборов текстур: %d" % made)


# Текстурные координаты «по миру»: грань смотрит вверх/вниз — проекция XY, вдоль X — YZ,
# вдоль Y — XZ. Текстура повторяется каждые TileSize см независимо от размера коробки
# (серые стены — растянутые кубы, обычные UV растягивали текстуру на всю стену).
# WorldAligned = 0 — та же проекция, но в осях самой коробки (LocalPosition × BoxSize, см):
# у движущихся объектов (стойка, предметы) текстура едет вместе с ними и не растягивается.
WORLD_UV_CODE = """float3 p = Aligned > 0.5 ? P : LP * BoxSize / 100.0;
float3 n = abs(Aligned > 0.5 ? N : LN);
float2 w = (n.z >= n.x && n.z >= n.y) ? p.xy : (n.x >= n.y ? p.yz : p.xz);
w = w / max(Tile, 1.0);
w.y = -w.y;
return w;"""


def build_material_graph(material, maps):
    mel = unreal.MaterialEditingLibrary
    pos = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1300, -200)
    nrm = mel.create_material_expression(material, unreal.MaterialExpressionVertexNormalWS, -1300, -60)
    local_pos = mel.create_material_expression(material, unreal.MaterialExpressionLocalPosition, -1300, 80)
    local_nrm = mel.create_material_expression(material, unreal.MaterialExpressionPreSkinnedNormal, -1300, 140)
    box_size = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -1300, 560)
    box_size.set_editor_property("parameter_name", "BoxSize")
    box_size.set_editor_property("default_value", unreal.LinearColor(100.0, 100.0, 100.0, 0.0))
    tile = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1300, 200)
    tile.set_editor_property("parameter_name", "TileSize")
    tile.set_editor_property("default_value", 200.0)
    aligned = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -1300, 320)
    aligned.set_editor_property("parameter_name", "WorldAligned")
    aligned.set_editor_property("default_value", 1.0)
    custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -900, 0)
    custom.set_editor_property("code", WORLD_UV_CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    custom.set_editor_property("description", "DarcWorldUV")
    inputs = []
    for name in ["P", "N", "LP", "LN", "BoxSize", "Tile", "Aligned"]:
        item = unreal.CustomInput()
        item.set_editor_property("input_name", name)
        inputs.append(item)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(pos, "", custom, "P")
    mel.connect_material_expressions(nrm, "", custom, "N")
    mel.connect_material_expressions(local_pos, "", custom, "LP")
    mel.connect_material_expressions(local_nrm, "", custom, "LN")
    mel.connect_material_expressions(box_size, "", custom, "BoxSize")
    mel.connect_material_expressions(tile, "", custom, "Tile")
    mel.connect_material_expressions(aligned, "", custom, "Aligned")

    y = 0
    for role, prop in [("BaseColor", unreal.MaterialProperty.MP_BASE_COLOR),
                       ("Normal", unreal.MaterialProperty.MP_NORMAL),
                       ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS),
                       ("Gloss", unreal.MaterialProperty.MP_ROUGHNESS),
                       ("Metallic", unreal.MaterialProperty.MP_METALLIC),
                       ("AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)]:
        if role not in maps or (role == "Gloss" and "Roughness" in maps):
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
        mel.connect_material_expressions(custom, "", node, "UVs")
        output = "RGB" if role in ("BaseColor", "Normal") else "R"
        if role == "BaseColor":
            connect_base_color_with_variation(material, custom, node, texture, y)
        elif role == "Gloss":
            # Glossiness — обратная шероховатость.
            invert = mel.create_material_expression(material, unreal.MaterialExpressionOneMinus, -150, y)
            mel.connect_material_expressions(node, "R", invert, "")
            mel.connect_material_property(invert, "", prop)
        else:
            mel.connect_material_property(node, output, prop)
        y += 260


def connect_base_color_with_variation(material, custom, sample, texture, y):
    """Цвет × крупное пятно той же текстуры (в ~7 раз крупнее): ломает заметный повтор
    узора на больших площадях (трава, дорога). MacroVariation = 0 — выключить."""
    mel = unreal.MaterialEditingLibrary
    scale = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -900, y + 120)
    scale.set_editor_property("r", 0.137)
    big_uv = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -700, y + 120)
    mel.connect_material_expressions(custom, "", big_uv, "A")
    mel.connect_material_expressions(scale, "", big_uv, "B")
    macro = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -550, y + 120)
    macro.set_editor_property("texture", texture)
    mel.connect_material_expressions(big_uv, "", macro, "UVs")
    boost = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -550, y + 260)
    boost.set_editor_property("r", 1.8)
    macro_bright = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -400, y + 160)
    mel.connect_material_expressions(macro, "RGB", macro_bright, "A")
    mel.connect_material_expressions(boost, "", macro_bright, "B")
    one = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -400, y + 280)
    one.set_editor_property("r", 1.0)
    amount = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, y + 360)
    amount.set_editor_property("parameter_name", "MacroVariation")
    amount.set_editor_property("default_value", 0.35)
    lerp = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -250, y + 200)
    mel.connect_material_expressions(one, "", lerp, "A")
    mel.connect_material_expressions(macro_bright, "", lerp, "B")
    mel.connect_material_expressions(amount, "", lerp, "Alpha")
    final = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -120, y)
    mel.connect_material_expressions(sample, "RGB", final, "A")
    mel.connect_material_expressions(lerp, "", final, "B")
    mel.connect_material_property(final, "", unreal.MaterialProperty.MP_BASE_COLOR)


def ensure_emissive_material():
    """M_DarcEmissive — светящийся материал для индикаторов, трубок ламп, окон, огней
    (код игры: слот Emissive, параметры Color и Intensity). Без света от материала —
    только свечение (Unlit), поэтому дёшево и одинаково в любом освещении."""
    path = ROOT + "/Materials/M_DarcEmissive"
    if eal.does_asset_exist(path):
        return
    if not eal.does_directory_exist(ROOT + "/Materials"):
        eal.make_directory(ROOT + "/Materials")
    mel = unreal.MaterialEditingLibrary
    material = asset_tools.create_asset("M_DarcEmissive", ROOT + "/Materials", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    intensity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 200)
    intensity.set_editor_property("parameter_name", "Intensity")
    intensity.set_editor_property("default_value", 5.0)
    mul = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -250, 80)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(intensity, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(material)
    eal.save_loaded_asset(material)
    say("Создан светящийся материал " + path)


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


def write_texture_report():
    """docs/texture_report.csv: что стало с каждым файлом текстуры и каждым набором."""
    with open(TEXTURE_REPORT_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(["file_or_set", "status", "asset"])
        writer.writerows(texture_report)
    say("Отчёт по текстурам: docs/texture_report.csv (%d строк)" % len(texture_report))


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
        ensure_emissive_material()
        slow.enter_progress_frame(1, "Таблицы среза")
        tasks = import_table("DT_Mission_Slice_Tasks.csv", "DT_Mission_Slice_Tasks", "/Script/DARK.DarcTaskDefinition")
        import_table("DT_RareEvents.csv", "DT_RareEvents", "/Script/DARK.DarcRareEventRow")
        if tasks:
            ensure_mission(tasks)
        slow.enter_progress_frame(1, "Список ассетов")
        write_texture_report()
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
