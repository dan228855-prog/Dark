# -*- coding: utf-8 -*-
"""
D.A.R.C. — импорт UI-пака автора (RawAssets/UI/DARC_UI_Pack) в /Game/DARC/Imported/Textures/UI/DARC_UI_Pack.
Настройки как в README пака: группа UI, сжатие UserInterface2D, без мипов, sRGB.
Панели с запечённым текстом (терминал/диалог-образцы, «СИСТЕМА ОШИБКА», брендовые блоки с чужой расшифровкой)
импортируются только как образцы в папку _Reference — в игре текст для игрока идёт из String Table.
Вырезки без текста (фон экрана, сканлайны, шум) — из подпапки Crops.
"""
import os
import re
import unreal

RAW = "C:/D.A.R.K/DARK/RawAssets/UI/DARC_UI_Pack"
DEST = "/Game/DARC/Imported/Textures/UI/DARC_UI_Pack"
REFERENCE = {"00_REFERENCE", "01_Terminal", "02_Dialogue", "03_Decorative", "04_Display", "05_Effects"}
REFERENCE_FILES = {"DARC_HUD_Elements", "Health_Bar_Reference", "Stamina_Bar_Reference", "DARC_Interface_Elements",
                   "DARC_Terminal_Brand_Block", "Warning_System_Error", "DARC_Guard_Portrait_Sheet"}


def clean(name):
    return re.sub(r"_+", "_", re.sub(r"[^A-Za-z0-9_]", "_", name)).strip("_")


tasks = []
for folder in sorted(os.listdir(RAW)):
    path = os.path.join(RAW, folder)
    if not os.path.isdir(path):
        continue
    for f in sorted(os.listdir(path)):
        if not f.lower().endswith(".png"):
            continue
        base = os.path.splitext(f)[0]
        sub = "_Reference" if (folder in REFERENCE or base in REFERENCE_FILES) else re.sub(r"^\d+_", "", folder)
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", path + "/" + f)
        t.set_editor_property("destination_path", DEST + "/" + sub)
        t.set_editor_property("destination_name", "T_" + clean(base))
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
n = 0
for t in tasks:
    for p in t.get_editor_property("imported_object_paths"):
        tex = unreal.load_asset(p)
        if isinstance(tex, unreal.Texture2D):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property("srgb", True)
            unreal.EditorAssetLibrary.save_loaded_asset(tex)
            n += 1
unreal.log("[DARC] ui pack imported %d textures" % n)
