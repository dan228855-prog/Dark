# -*- coding: utf-8 -*-
"""
D.A.R.C. — импорт моделей автора (07.10.2026) из RawAssets/Meshes в /Game/DARC/Imported/Meshes/<Модель>.
FBX/OBJ: модель одним мешем + материал по текстурам из её папки textures (роль — по имени файла:
BaseColor/Diff/Albedo, Normal, Roughness, Metallic, AO, Emissive; нормали OpenGL — зелёный канал переворачивается).
glTF (Волга, фонари): штатный импорт движка с его материалами.
Лицензии: Волга и фонари — Sketchfab Standard (коммерция можно); остальные без license.txt — проверить на странице.
"""
import os
import re
import unreal

RAW = "C:/D.A.R.K/DARK/RawAssets/Meshes"
DEST = "/Game/DARC/Imported/Meshes"
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
log = []

MODELS = {
    # папка: (файл модели, имя в проекте)
    "data-center-rack": ("source/single server 3d model.obj", "ServerRack"),
    "electric-box-42": ("source/electric_box_42.fbx", "ElectricBox"),
    "fuse": ("source/FYUJ .fbx", "Fuse"),
    "retro-computer-setup-free": ("source/Computer Monitor OLD.fbx", "RetroComputer"),
    "wall-computer": ("source/WallComputer.fbx", "WallComputer"),
    "old-rusted-modular-pipes": ("source/modular_pipes.fbx", "ModularPipes"),
}
GLTF = {"1971_gaz-24_volga_lp": "Volga", "modlarStreetLamp": "StreetLamps"}


def clean(name):
    return re.sub(r"_+", "_", re.sub(r"[^A-Za-z0-9_]", "_", name)).strip("_")


def task(path, dest, name=None, options=None):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", path)
    t.set_editor_property("destination_path", dest)
    if name:
        t.set_editor_property("destination_name", name)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    if options:
        t.set_editor_property("options", options)
    return t


def role_of(fname):
    n = fname.lower()
    for role, keys in (("Normal", ("normal", "_nor", "nrm")), ("Roughness", ("rough",)), ("Metallic", ("metal",)),
                       ("AO", ("_ao", "ambient", "occlusion")), ("Emissive", ("emiss", "emission")),
                       ("Spec", ("specular",)), ("Height", ("displac", "height")), ("Ground", ("internal_ground",)),
                       ("BaseColor", ("basecolor", "diff", "albedo", "base"))):
        if any(k in n for k in keys):
            return role
    return "BaseColor"


def build_material(folder, name, maps):
    path = folder + "/M_" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    m = tools.create_asset("M_" + name, folder, unreal.Material, unreal.MaterialFactoryNew())
    y = 0
    for role, prop in (("BaseColor", unreal.MaterialProperty.MP_BASE_COLOR), ("Normal", unreal.MaterialProperty.MP_NORMAL),
                       ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS), ("Metallic", unreal.MaterialProperty.MP_METALLIC),
                       ("AO", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION), ("Emissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR)):
        tex = maps.get(role)
        if not tex:
            continue
        node = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -400, y)
        node.set_editor_property("texture", tex)
        if role == "Normal":
            node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif role != "BaseColor" and role != "Emissive":
            node.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        mel.connect_material_property(node, "RGB" if role in ("BaseColor", "Normal", "Emissive") else "R", prop)
        y += 260
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def import_textures(model_dir, dest):
    tdir = os.path.join(model_dir, "textures")
    out = {}
    if not os.path.isdir(tdir):
        return out
    tasks = []
    for f in sorted(os.listdir(tdir)):
        if f.lower().endswith((".png", ".jpg", ".jpeg")) and role_of(f) not in ("Ground", "Spec", "Height"):
            tasks.append(task(os.path.join(tdir, f), dest + "/Textures", "T_" + clean(os.path.splitext(f)[0])))
    tools.import_asset_tasks(tasks)
    for t in tasks:
        for p in t.get_editor_property("imported_object_paths"):
            tex = unreal.load_asset(p)
            src = os.path.basename(t.get_editor_property("filename"))
            role = role_of(src)
            if role == "Normal":
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                tex.set_editor_property("srgb", False)
                if "opengl" in src.lower() or "_gl" in src.lower():
                    tex.set_editor_property("flip_green_channel", True)
            elif role in ("Roughness", "Metallic", "AO"):
                tex.set_editor_property("srgb", False)
                tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            eal.save_loaded_asset(tex)
            out[src] = (role, tex)
    return out


for folder, (rel, name) in MODELS.items():
    model_dir = os.path.join(RAW, folder)
    dest = DEST + "/" + name
    texs = import_textures(model_dir, dest)
    opts = unreal.FbxImportUI()
    opts.set_editor_property("import_mesh", True)
    opts.set_editor_property("import_as_skeletal", False)
    opts.set_editor_property("import_materials", True)
    opts.set_editor_property("import_textures", False)
    opts.static_mesh_import_data.set_editor_property("combine_meshes", True)
    opts.static_mesh_import_data.set_editor_property("generate_lightmap_u_vs", True)
    opts.static_mesh_import_data.set_editor_property("auto_generate_collision", True)
    t = task(os.path.join(model_dir, rel), dest, "SM_" + name, opts)
    tools.import_asset_tasks([t])
    mesh = None
    for p in t.get_editor_property("imported_object_paths"):
        a = unreal.load_asset(p)
        if isinstance(a, unreal.StaticMesh):
            mesh = a
    if not mesh:
        log.append("%s: mesh NOT imported" % name)
        continue
    # Наборы текстур: один набор на всю модель или по слотам (имя слота ~ префикс файлов).
    slots = mesh.static_materials
    for i, sm in enumerate(slots):
        slot = str(sm.material_slot_name).lower()
        chosen = {}
        groups = {}
        for src, (role, tex) in texs.items():
            prefix = re.split(r"_(basecolor|diff|albedo|normal|rough|metal|ao|emiss)", src.lower())[0]
            groups.setdefault(prefix, {})[role] = tex
        if len(groups) == 1:
            chosen = list(groups.values())[0]
        else:
            for prefix, maps in groups.items():
                key = re.sub(r"[^a-z]", "", prefix.split(".")[0].split("_")[-1] if "_" in prefix else prefix)
                if key and key in re.sub(r"[^a-z]", "", slot):
                    chosen = maps
                    break
            if not chosen:
                # Текстуры по частям без ролей (Base.png, Meter.png …): имя файла ~ имя слота.
                for src, (role, tex) in texs.items():
                    if os.path.splitext(src)[0].lower() in slot:
                        chosen = {"BaseColor": tex}
                        break
        if chosen:
            mat = build_material(dest, "%s_%d" % (name, i), chosen)
            mesh.set_material(i, mat)
    eal.save_loaded_asset(mesh)
    bb = mesh.get_bounding_box()
    log.append("%s: %d slots, size %.0fx%.0fx%.0f, textures %d" % (name, len(slots), bb.max.x - bb.min.x, bb.max.y - bb.min.y,
                                                                   bb.max.z - bb.min.z, len(texs)))

for folder, name in GLTF.items():
    t = task(os.path.join(RAW, folder, "scene.gltf"), DEST + "/" + name)
    tools.import_asset_tasks([t])
    paths = [str(p) for p in t.get_editor_property("imported_object_paths")]
    meshes = [p for p in paths if isinstance(unreal.load_asset(p), unreal.StaticMesh)]
    log.append("%s (gltf): %d assets, %d meshes: %s" % (name, len(paths), len(meshes), [p.split("/")[-1] for p in meshes][:12]))

unreal.log("[DARC] models: " + " | ".join(log))
