# Blender (фоновый режим): склеить модель из GLB в один меш и сохранить FBX для Unreal.
#   blender -b --python Tools/blender_join_glb.py -- <вход.glb> <выход.fbx>
# Материалы сохраняются (слоты по материалам), масштаб и оси — под Unreal (см, Z вверх).
import sys
import bpy

src, dst = sys.argv[sys.argv.index("--") + 1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
bpy.ops.object.select_all(action="DESELECT")
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
# Точка опоры — центр основания (низ модели).
bpy.ops.object.origin_set(type="ORIGIN_GEOMETRY", center="BOUNDS")
low = min((obj.matrix_world @ v.co).z for v in obj.data.vertices)
bpy.context.scene.cursor.location = (obj.location.x, obj.location.y, low)
bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
obj.location = (0, 0, 0)
dims = obj.dimensions
print("[JOIN] objects=%d size=%.2f x %.2f x %.2f m materials=%d" % (len(meshes), dims.x, dims.y, dims.z, len(obj.data.materials)))
bpy.ops.export_scene.fbx(filepath=dst, use_selection=True, apply_unit_scale=True, axis_forward="-Y", axis_up="Z",
                         mesh_smooth_type="FACE", add_leaf_bones=False, path_mode="COPY", embed_textures=True)
