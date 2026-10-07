"""
D.A.R.C. — переимпорт машины по частям (запускать в редакторе: Tools → Execute Python Script).

Удаляет ранее импортированную машину (/Game/DARC/Imported/Meshes/car) и заново импортирует
FBX из RawAssets/Meshes/car/... как отдельные статические модели в общих координатах машины:
руль, колёса, сиденья и фары остаются отдельными частями (их двигает ADarcIntroDrive).
"""
import os

import unreal

PROJECT_DIR = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
RAW_CAR_DIR = os.path.join(PROJECT_DIR, "RawAssets", "Meshes", "car")
DEST_ROOT = "/Game/DARC/Imported/Meshes/car"

eal = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def say(msg):
    unreal.log("[DARC] " + msg)


def options():
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", True)
    o.set_editor_property("import_as_skeletal", False)
    o.set_editor_property("automated_import_should_detect_type", False)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    o.set_editor_property("import_materials", True)
    o.set_editor_property("import_textures", True)
    o.set_editor_property("import_animations", False)
    data = o.static_mesh_import_data
    data.set_editor_property("combine_meshes", False)
    data.set_editor_property("transform_vertex_to_absolute", True)
    data.set_editor_property("bake_pivot_in_vertex", False)
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("auto_generate_collision", False)
    return o


def main():
    if not os.path.isdir(RAW_CAR_DIR):
        say("Нет папки " + RAW_CAR_DIR)
        return
    if eal.does_directory_exist(DEST_ROOT):
        eal.delete_directory(DEST_ROOT)
        say("Старая машина удалена: " + DEST_ROOT)

    tasks = []
    for folder, _, files in os.walk(RAW_CAR_DIR):
        for name in files:
            if not name.lower().endswith(".fbx"):
                continue
            rel = os.path.relpath(folder, RAW_CAR_DIR).split(os.sep)[0]
            dest = DEST_ROOT + "/" + "".join(c if c.isalnum() else "_" for c in rel).strip("_")
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", os.path.join(folder, name))
            task.set_editor_property("destination_path", dest)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", True)
            task.set_editor_property("options", options())
            tasks.append(task)

    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        asset_tools.import_asset_tasks(tasks)
    finally:
        unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")

    meshes = [p for p in eal.list_assets(DEST_ROOT, recursive=True)
              if eal.find_asset_data(p).asset_class_path.asset_name == "StaticMesh"]
    say("Машина импортирована: %d частей (статические модели) в %s" % (len(meshes), DEST_ROOT))


main()
