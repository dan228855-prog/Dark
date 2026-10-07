# Импорт картинок: RawAssets/LoadingScreens/* → /Game/DARC/UI/LoadingScreens (загрузочные экраны),
# RawAssets/Menu/T_MenuBackground.* → /Game/DARC/UI/Menu (фон главного меню, ставится в MainMenu).
# Без мипов и стриминга: на экране загрузки/меню текстура должна быть сразу полной, не мыльной.
# Запуск в редакторе: py C:/D.A.R.K/DARK/Tools/darc_import_loading_screens.py
import os
import unreal

RAW = os.path.join(unreal.Paths.project_dir(), "RawAssets")
JOBS = [(os.path.join(RAW, "LoadingScreens"), "/Game/DARC/UI/LoadingScreens"),
        (os.path.join(RAW, "Menu"), "/Game/DARC/UI/Menu")]
MENU_WIDGET = "/Game/Characters/UI/MainMenu"
MENU_IMAGE = "Image_76"


def import_dir(src, dst):
    tasks = []
    for name in sorted(os.listdir(src)):
        if name.lower().endswith((".png", ".jpg", ".jpeg")):
            t = unreal.AssetImportTask()
            t.filename = os.path.join(src, name)
            t.destination_path = dst
            t.automated = True
            t.replace_existing = True
            t.save = False
            tasks.append(t)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    result = []
    for t in tasks:
        for p in t.imported_object_paths:
            tex = unreal.load_asset(p)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
            tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            tex.set_editor_property("never_stream", True)
            tex.set_editor_property("srgb", True)
            unreal.EditorAssetLibrary.save_loaded_asset(tex)
            result.append(tex)
            unreal.log("DARCI imported %s" % p)
    return result


for src, dst in JOBS:
    if os.path.isdir(src):
        textures = import_dir(src, dst)
        if dst.endswith("/Menu") and textures:
            # Фон меню: картинка виджета MainMenu.
            wbp = unreal.load_asset(MENU_WIDGET)
            image = unreal.find_object(None, wbp.get_path_name() + ":WidgetTree." + MENU_IMAGE)
            if image:
                brush = image.get_editor_property("brush")
                brush.set_editor_property("resource_object", textures[0])
                image.set_editor_property("brush", brush)
                unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
                unreal.EditorAssetLibrary.save_loaded_asset(wbp)
                unreal.log("DARCI menu background set")
            else:
                unreal.log_warning("DARCI menu image widget not found")
