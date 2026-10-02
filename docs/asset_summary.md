# Что уже есть в проекте D.A.R.K. (инвентарь ассетов)

Собрано из `docs/asset_inventory.csv` (сканирование Content Browser, 1915 записей). Полный CSV прикреплён отдельно — его можно отдать Claude Code как есть.

## Итого по типам

| Тип | Класс UE | Количество |
|---|---|---|
| 3D-модели (статика) | StaticMesh | 1030 |
| Материалы (инстансы) | MaterialInstanceConstant | 373 |
| Материалы (базовые) | Material | 275 |
| Звуки | SoundWave | 145 |
| Скелетные модели (персонажи/риги) | SkeletalMesh | 91 |

**Текстуры отдельно не посчитаны** — сканер инвентаря смотрел только меши/материалы/звуки, текстуры он не выделял как отдельный тип (они есть, но внутри материалов). Если нужно точное число текстур — надо отдельно пройтись по `Content/Textures` и материалам.

**Анимации — тоже не в этом отчёте.** В проекте есть папка `Content/FreeAnimationLibrary`, но её не сканировали на AnimSequence — это открытый вопрос из прошлой сессии (нужно было проверить `DarcNpc.h` / `DarcSpiritCharacter.h`, не успели из-за разрыва связи с ПК).

## 3D-модели (StaticMesh) — по паку/источнику

| Пак | Моделей |
|---|---|
| Singapore_Canal | 177 |
| SD_Art | 152 |
| Deko_MatrixDemo | 136 |
| VintageIndustrialWarehouseVol1Deprecated | 122 |
| HospitalCorridor | 97 |
| Construction_VOL1 | 73 |
| Megaplant_Library | 66 |
| AbandonedPowerPlant | 65 |
| FreeFurniturePack | 44 |
| IndustryPropsPack6 | 30 |
| Sci_Fi_Light | 17 |
| NY_Subway | 14 |
| LevelPrototyping | 13 |
| LN3D_Modular_CatWalk | 11 |
| DARC (свои) | 9 |
| StaticMeshes (свободная папка) | 2 |
| SM_Tiles_02 | 1 |
| Urban_Nomad | 1 |

## Скелетные модели (SkeletalMesh) — по паку

| Пак | Моделей |
|---|---|
| Megaplant_Library | 73 |
| Handyman | 14 |
| Characters (свои) | 2 |
| Robot_scout_R_21 | 1 |
| Urban_Nomad | 1 |

## Звуки (SoundWave) — по паку

| Пак | Звуков |
|---|---|
| DARC (свои) | 142 |
| Vefects | 3 |

## Материалы — по паку (для справки)

| Пак | Материалов |
|---|---|
| VintageIndustrialWarehouseVol1Deprecated | 109 |
| Singapore_Canal | 61 |
| HospitalCorridor | 57 |
| FreeFurniturePack | 52 |
| Deko_MatrixDemo | 49 |
| DARC (свои) | 46 |
| SD_Art | 46 |
| Construction_VOL1 | 36 |
| AbandonedPowerPlant | 27 |
| IndustryPropsPack6 | 23 |
| NY_Subway | 21 |
| Sci_Fi_Light | 21 |
| Robot_scout_R_21 | 19 |
| Vefects | 15 |
| Megaplant_Library | 13 |
| Handyman | 11 |
| LevelPrototyping | 10 |
| SM_Tiles_02 | 10 |
| Characters (свои) | 5 |
| Materials (свободная папка) | 5 |
| Variant_Horror | 5 |
| Urban_Nomad | 3 |
| Realistic_Industrial_Stone_Tiled_Pavement_PBR | 2 |
| LN3D_Modular_CatWalk | 1 |
| FirstPerson | 1 |

## Что из этого вытекает

- 3D-моделей и материалов на самом деле очень много (1030 + ~650) — проект не "пустой", просто пустые слоты в `DarcAssetSettings` (FuseBox, Generator, Keypad и т.д.) не назначены на конкретные модели из уже импортированных паков. Это скорее задача "подобрать и привязать", а не "скачать с нуля".
- Звуков 145, но почти все (142) из собственной папки DARC — значит именно GeneratorStart/LampHum (пустые слоты) реально нет готовых, это настоящий пробел.
- Текстуры и анимации — зона, которую не успели сосчитать; лучше отдать этот вопрос прямо Code, пусть сам пройдётся по `Content/Textures` и `Content/FreeAnimationLibrary` через unreal-mcp, когда подключится.

## Файл для Code

Приложен `asset_inventory.csv` (сырые данные: kind, class, object_path) — это можно вставить в контекст Claude Code локально, он сам сможет смотреть точные пути ассетов при привязке к пустым слотам.

## Слоты DarcAssetSettings — что подставлено (02.10.2026)

Список в `docs/asset_inventory.csv` совпал с тем, что уже был в репозитории, — новых ассетов нет,
поэтому пустые слоты заполнены тем, что уже импортировано.

| Слот | Модель | Пак |
|---|---|---|
| FuseBox | SM_UtilityElectronics_01_C | HospitalCorridor |
| Keypad | SM_Sci_Fi_Info_Terminal | Sci_Fi_Light |
| CardReader | SM_UtilityElectronics_01_N | HospitalCorridor |
| Socket | SM_UtilityElectronics_01_E | HospitalCorridor |
| PowerInlet | SM_UtilityElectronics_01_B | HospitalCorridor |
| InterfaceModule | SM_ComputerParts_A04_N1 | Deko_MatrixDemo |
| Generator | SM_LightGenerator_Base_01a | Construction_VOL1 |
| Cabinet | SM_Classic_Cupboard | FreeFurniturePack |
| Window | SM_Window_Small_01 | AbandonedPowerPlant |
| Fence | SM_construction_fence_01a | Construction_VOL1 |
| Guard (Characters) | SK_Handyman_full_ver1 | Handyman |

Материалы: `Ground` — M_Poliigon_GrassPatchyGround_4585, `Road` — M_Ground074, `WallOutside` — M_concrete_wall_008.

**Всё ещё пусто** (подходящих моделей нет — серые фигуры): ServerRack, ServerRackStatic, Keycard, Fuse,
Mug, SatelliteDish, StreetLamp, ели/вышка/машина у КПП. Звуки: GeneratorStart, LampHum.

**Анимация охранника:** слот `Animations` → `Guard_Idle` пуст. Нужна idle-анимация под скелет
Handyman (проверить `Content/FreeAnimationLibrary` и сам пак Handyman). Пока анимации нет, охранник
стоит в исходной позе модели.

**Физика:** у генератора, шкафа и модуля должна быть простая коллизия (Simple Collision). Если её
нет, игра сама поставит серую коробку и напишет в Output Log `DARC: mesh ... has no simple collision`.
