# -*- coding: utf-8 -*-
"""
D.A.R.C. — ограничить размер текстур новых моделей (07.10.2026) до 2048: видеопамять переполнялась.
Исходники не трогаются — меняется только «максимальный размер» в настройках текстуры (можно вернуть).
"""
import unreal

eal = unreal.EditorAssetLibrary
FOLDERS = ["ModularPipes", "RetroComputer", "WallComputer", "ElectricBox", "ServerRack", "VolgaCar", "StreetLamps", "Fuse"]
LIMIT = 2048
changed, before_mb = [], 0.0
for f in FOLDERS:
    for p in eal.list_assets("/Game/DARC/Imported/Meshes/" + f, recursive=True):
        t = eal.load_asset(p)
        if not isinstance(t, unreal.Texture2D):
            continue
        w, h = t.blueprint_get_size_x(), t.blueprint_get_size_y()
        cur = t.get_editor_property("max_texture_size") or 0
        if max(w, h) > LIMIT and (cur == 0 or cur > LIMIT):
            t.set_editor_property("max_texture_size", LIMIT)
            eal.save_loaded_asset(t)
            changed.append("%s %dx%d" % (p.split("/")[-1].split(".")[0], w, h))
unreal.log("[DARC] textures capped to %d: %d — %s" % (LIMIT, len(changed), ", ".join(changed)))
