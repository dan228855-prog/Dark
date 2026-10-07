# D.A.R.C. — контекст проекта для Claude Code

## О проекте
- Кооперативная игра на Unreal Engine 5.8, 1-5 игроков. Имя проекта/модуля: `DARK` (макрос экспорта `DARK_API`). Соло — полноценный режим, не урезанный кооп: любая механика на нескольких игроков обязана иметь альтернативу для одного.
- Язык: C++ (архитектура) + Blueprints (контент). Не строить игру целиком на одном из двух.
- Сеть: Listen Server. Сервер полностью авторитетен для всего важного состояния, клиент только запрашивает действие через Server RPC, сервер валидацию не пропускает.
- Локализация: RU + EN с самого начала, через String Table. Никогда не хардкодить текст, который увидит игрок, ни в код, ни в текстуры (вывески/таблички — отдельный материал/декаль с текстом из String Table).
- Соло-разработчик (не пишет код сам) + Claude Code. Бюджет ~$0, кроме $100 Steam Direct fee (возвращается после $1000 выручки).
- Подход: сначала вертикальный срез (один выезд, играбельный на серых боксах), потом полная игра по plan_razrabotki.md.

## Сюжет и сеттинг
- Корпорация D.A.R.C. (Documents / Archive / Removal / Control) даёт игрокам задания. На самом деле борется с вышедшим из-под контроля ИИ — внутриигровое имя **E.V.A.** (Environmental Verification Assistant). Игрокам это не раскрывается прямо, только через редкие странные события.
- Реальные ИИ (ChatGPT/Claude/Grok) нигде не упоминаются в игровых текстах — только вымышленные термины: CONTEXT WINDOW EXCEEDED, ATTENTION_LAYER, INFERENCE SESSION и подобные.

## Ключевые решения по дизайну (финальны, не менять без обсуждения с автором)
- **Электросистема**: полноценная — генераторы, предохранители, щиты, отключения. Загадки завязаны на неё.
- **Инструменты игрока**: не фиксированы заранее, добавляются по мере необходимости под конкретную механику.
- **Обычные/сюжетные выезды**: нет строгого разделения — сюжет незаметно вплетён в каждый выезд.
- **Длительность выезда**: 20-30 минут.
- **Опасности**: средовые (ловушки, обвалы, электричество) + редкие активные угрозы от аномалий.
- **Смерть**: настоящая. Погибший становится "духом"-спектатором: может мигать светом/включать электроприборы через электросистему и двигать мелкие объекты класса C (полтергейст). Может как помогать, так и мешать живым игрокам. Без механики особого зрения.
- **Эскалация вокруг смертей**: и сухие корпоративные записи в логах/терминалах, и поведение духа становятся настойчивее/личнее с ростом числа смертей и уровня, сходятся к уровню 6-7.
- **База**: растёт и открывает новые комнаты/апгрейды по прогрессу игрока.
- **Экономика**: валюты/ресурса нет вообще — прогресс только через сюжет и разблокировку базы.
- **Локации**: разные типы по уровням (промзона, офисы D.A.R.C., подземные уровни, жилые здания).
- **Первая аномалия (срез)**: физическая — объект/пространство ведёт себя неправильно (левитация, искажённая геометрия).
- **Подача сюжетных сцен**: без катсцен, без потери контроля — события происходят вокруг игрока, пока он свободно двигается/смотрит.

## Что уже написано (лежит в Source/DARK/, модуль плоский, без Public/Private)
- `IInteractable` — интерфейс взаимодействия: `CanInteract`, `OnInteract` (только сервер), `GetInteractionPrompt`.
- `UInteractionComponent` — компонент на игроке: трейс взгляда на локальном клиенте, `Server_Interact` RPC с полной ревалидацией дистанции на сервере (клиенту не доверяем).
- `AInteractableDoor` — дверь: реплицируемый `bIsOpen`, `bIsLocked` для будущих загадок с ключом/кодом.
- `ACarryableItem` — переносимый предмет: реплицируемый `CurrentHolder`, привязка к сокету руки, `ForceDrop()` под сценарий потери сознания.
- `ADarcGameMode` / `ADarcGameState` (`EMissionPhase`: Preparation/InProgress/Completed/Failed) / `ADarcPlayerState` (`bIsAlive`, `DeathCount`).
- Везде оставлены точки расширения `BlueprintNativeEvent`/`BlueprintImplementableEvent` — контентная настройка (анимация, звук, конкретный текст) делается в Blueprint, не хардкодится в C++.
- Макрос экспорта — `DARK_API` (реальное имя модуля проекта, уже подставлено во всех файлах).
- Обрати внимание: в модуле уже есть сгенерённые шаблоном классы `ADARKGameMode`, `ADARKCharacter`, `ADARKPlayerController`, `ADARKCameraManager` (из стартового UE-темплейта Horror/Shooter variant) — наши классы называются иначе (`ADarcGameMode` и т.д.) и не конфликтуют, но при выборе GameMode по умолчанию для уровней смотри, какой из них реально выставлен в Project Settings / World Settings.

- WorldMemory (`UDarcWorldMemorySubsystem`, GameInstanceSubsystem), RareEventManager (`URareEventManagerComponent` на GameState, таблица `FDarcRareEventRow`, якоря `ADarcRareEventAnchor`), Task System (`UTaskManagerComponent` на GameState, ассет выезда `UDarcMissionDefinition`), комнаты `ADarcRoomVolume`. Написаны по ТЗ, но ещё не компилировались — настройка в редакторе и чеклист проверки в `docs/SYSTEMS_setup.md`.
- Электросистема: `UDarcPowerSubsystem`, `ADarcFuseBox`, `ADarcFuseItem`, `ADarcGenerator` (наследник `ADarcHeavyObject`), `ADarcPowerInlet`, `UDarcPowerConsumerComponent`.
- Срез: `ADarcTerminal` (ввод кода через `UInteractionComponent::SubmitTerminalInput`), `ADarcItemSlot`, `ADarcDataTransferStation`, `ADarcHeavyObject` (кооп/соло перенос), `UDarcGameplayLibrary::GetMissionCode` (код из реплицируемого сида выезда). Всё это тоже ещё не компилировалось.
- Смерть/дух: `ADarcGameMode::KillPlayer`, `ADarcSpiritCharacter`; смерти за кампанию — в WorldMemory.
- NPC/доступ/база: `ADarcNpc` (реплики через `ADarcGameState::Say` → `OnSubtitle`), `ADarcCardReader`, `ADarcBaseUnlock` (по фактам кампании).
- Данные среза для импорта: `docs/data/*.csv` (задачи, редкие события).
- Строки: **источник (RU) — `Content/Localization/ST_UI.csv`**, регистрируется кодом при старте модуля (`LOCTABLE_FROMFILE_GAME("ST_UI", ...)` в `DARK.cpp`); из C++ — `UDarcGameplayLibrary::UIText(Key)`. Английский — `Content/Localization/ST_UI_EN.csv` (те же ключи); язык переключается в настройках: `UDarcGameUserSettings::ApplyLanguage` пересобирает таблицу ST_UI (русский + английский поверх), без Localization Dashboard. `/Content/Localization/` — единственная часть Content в git.
- Срез играбелен без редактора: на пустой карте `ADarcGameMode` создаёт `ADarcSliceBuilder` (серый уровень кодом), `ADarcPlayerController` (клавиши через BindKey, окно терминала на Slate), `ADarcHUD` (Canvas). Модели/материалы/звуки — слоты в `UDarcAssetSettings` (`Config/DefaultGame.ini`), объекты применяют `FDarcVisualSpec` у каждой машины сами.
- Ассеты автора: `Tools/darc_setup.py` (запуск в редакторе) импортирует `RawAssets/` и пишет `docs/asset_inventory.csv` — по нему заполнять слоты в `DefaultGame.ini`. Инструкция автору — `docs/PC_SETUP.md`.
- Разметка карты тегами: `UDarcMarkupSubsystem` превращает акторы карты с тегами `DARC.<Тип>[.Флаг]` + `Ключ=Значение` в игровые объекты (модель берётся у исходного актора через `FDarcVisualSpec::CopyFrom`). Памятка автору и список типов — `docs/MAP_MARKUP.md`. Карта с разметкой — серый срез не строится.
- Физический захват: `UDarcGrabComponent` (ЛКМ держать / ПКМ толкнуть; сила игрока ограничена — тяжёлое в коопе тащат вдвоём, в соло сила ×2; «привязь» `TetherLength` ~3 шага — дальше от схваченного игрок не отойдёт). `ADarcHeavyObject` — физическое тело-коробка (`Collision`, размер = `VisualSpec.Size`) + модель без коллизии (`Body`), масса, хрупкий кабель, доставка. Коллизия модели на физику не влияет.
- Игрок: `UDarcFlashlightComponent` (F, реплицируемый прожектор по взгляду), `UDarcPlayerOutfitComponent` (модель охранника поверх скрытого анимированного манекена — leader pose). Шлагбаум — `ADarcBarrierGate` (локальный, плавно открывается при подходе). Пропсы сборщика — `AddProp` (модель без коллизии + невидимая коробка). Поворот «лица» чужих моделей — `MeshYaw` в `DefaultGame.ini`; светящийся материал — слот `Emissive` (`M_DarcEmissive`, создаёт скрипт), хелпер `MakeColorMaterial`.
- Кабель стойки — `ADarcCablePlug` (вилка-предмет на проводе от розетки: E — взять, E по стойке — подключить; длина ограничена, натянутый провод выдёргивается на бегу). Разъём интерфейса закреплён на самой стойке. Повороты моделей — `MeshYaw` + `MeshRotation`, крупная вариация по слоту — `MaterialMacroVariation`. Вид от первого лица — модель охранника поверх скрытых рук шаблона (`UDarcPlayerOutfitComponent`).
- ВРЕМЕННО: `UDarcHintComponent` — название и «что делать» под прицелом (ключи `Name_*`/`Hint_*` в ST_UI). Убрать вместе с блоком в `ADarcHUD` и вызовами `AddHint`.
- Материалы из `Tools/darc_setup.py` — UV по миру + крупная вариация (`TileSize`, `MacroVariation`, `WorldAligned`); повтор по слоту — `MaterialTiling` в `DefaultGame.ini`. Слот материала (`VisualSpec.Material`) применяется только к серой коробке, у моделей — их собственные материалы. Отчёт по текстурам — `docs/texture_report.csv` (пишет скрипт). Шаги/прыжок/приземление — `UDarcFootstepComponent` (локально у каждой машины). Компоненты добавляются персонажу шаблона в `ADarcPlayerController::OnPossess`.
- Заезд в начале выезда — `ADarcIntroDrive` (машина из частей `CarParts`, маршрут `PathPoints`, хост за рулём, места/выход/радио `CarRadio*`, спидометр; движение считается у каждой машины по серверному времени). Трава — `ADarcGrassScatter` (HISM вдоль `PathPoints` + `Areas`, без теней/коллизии). Машину импортировать `Tools/darc_reimport_car.py` (по частям, не скелетно).
- Тексты подсказок двери/предмета — поля `Prompt*` (FText из String Table), не NSLOCTEXT в C++.
- Инвентарь и инструменты: `UDarcInventoryComponent` (4 слота, клавиши 1–4; убранный предмет — `ACarryableItem::StowedBy`, скрыт и едет с игроком), `ADarcTool` (`Crowbar`/`Wrench`/`Tester`/`Radio`; устройства смотрят, что в руке). Лом взламывает запертые двери (`PryBreaks` рвёт `ADarcCableJunction` — контур обесточен, чинится ключом) и срывает `ADarcRemovableCover` (решётка вентиляции; ключом — тихо). Удар током: предохранитель под напряжением, порванная линия голой рукой. Перегрузка: `ADarcGenerator::Capacity` против суммы `ADarcPowerInlet::Load`. Пригнуться — Ctrl. Голос — V (push-to-talk, VOIP через OnlineSubsystem Null): рядом — из персонажа, рации у обоих — без расстояния (`UpdateVoiceTalkers` в контроллере). Механики уровня 1 ставит `Tools/darc_add_mechanics.py` (один раз, папка `FacilityProps/Mechanics`).
- Меню и загрузка: старт игры — карта `/Game/LevelPrototyping/Map/Menu` (режим `ADarcMenuGameMode`: без персонажа, курсор, ввод только в UI; виджет `/Game/Characters/UI/MainMenu` создаёт Level Blueprint, кнопка Play → OpenLevel `/Game/DARC/Maps/1LvlMap`). Загрузочный экран — `UDarcGameInstance` (MoviePlayer на PreLoadMap, случайная картинка из `LoadingScreens` в `DefaultGame.ini`, текст `Loading_Text`; картинки — `/Game/DARC/UI/LoadingScreens`, импорт `Tools/darc_import_loading_screens.py`); в PIE не показывается — проверять в Standalone. Настройки — `UDarcGameUserSettings` (поверх стандартных: мышь, поле зрения, яркость, громкость по категориям — классы `/Game/DARC/Audio/SC_*` + микс `SM_DarcVolumes` из `Tools/darc_setup_audio.py`, голос — открытый микрофон/по кнопке и усиление, клавиши `KeyBindings` — действия в `GetKeyActions`, ходьба/прыжок переназначаются в копиях раскладок шаблона, язык, метод сглаживания) и окна на Slate в `DarcSettingsMenu.cpp`: настройки (`UDarcSettingsLibrary::OpenSettingsMenu`, кнопка Setings в MainMenu) и пауза (Esc → `ADarcPlayerController::SetPauseMenuOpen`). Надписи на загрузочных экранах — `Overlays` в `LoadingScreens` (координаты в пикселях картинки). Присед: `ABP_Unarmed` переключается (Blend Poses by bool по `Character.bIsCrouched`) на blend space `/Game/DARC/Anims/BS_DarcCrouch` (`Tools/darc_setup_crouch.py`; после изменения точек скриптом открыть blend space в редакторе, иначе он отдаёт стоячую позу).
- Масштабирование и генерация кадров: плагины в `Plugins/` — NVIDIA DLSS 4.5 (`DLSS`, `Streamline*`, v8.8.0 для UE 5.8, лицензия NVIDIA RTX SDK — DLSS упомянуть в титрах) и AMD FSR 4.1.1 (`FSR`, MIT); архивы — `C:\D.A.R.K\Plugins_Download`. Настройки `Upscaler` (TSR/DLSS/FSR), `UpscalerQuality`, `FrameGeneration` (DLSS 2X–6X/динамическая или FSR), `ReflexMode` применяет `UDarcGameUserSettings::ApplyUpscaling` (DLSS/Reflex — функции плагинов под `DARC_WITH_NVIDIA`, FSR — переменные `r.FidelityFX.*`). Только Win64 + DX12. DLSS 5 для UE ещё не вышел — обновить плагин, когда появится.
- База между выездами (`docs/DARC_base_corridor.md`): карта `/Game/DARC/Maps/BaseMap` из `Tools/darc_build_base.py` (папка Base — каркас, BaseProps — руками). `ADarcBaseCurator` (лобби R1: реплики из динамиков, факты за выезд, выбор уровня, список выездов в `DefaultGame.ini`), `ADarcBaseCar` (E — сесть, когда сели все живые — ServerTravel), `ADarcMissionTerminal` (окно выбора объекта; RPC `Client_OpenMissionSelect`/`Server_SelectMission` в контроллере), `ADarcLightDoor`. Двери базы — разметка `DARC.Door` + `UnlockFact=` (`AInteractableDoor::RefreshUnlockFact`). Факт пройденного уровня — `Arc1.Level<N>Done`. Меню → база → выезд → база (`DarcGameMode.BaseMap`).

## Источники истины
- `docs/DARC_master_design_document_v0.6.txt` — мастер-документ (приоритеты при конфликтах — в его конце, раздел MASTER TECHNICAL BASELINE).
- `docs/DARC_rare_events_v1.1.txt` — редкие события и память мира.
- Решение автора от 01.10.2026: на Уровне 2 — несколько роботов целиком вместо одного робота из 5 частей.

## Что дальше (по приоритету)
1. **Вертикальный срез — `docs/DARC_vertical_slice.md`** (решение автора 01.10.2026: сначала срез «по максимуму», потом уровни 1–7). Срез объединяет «Первую работу» (мастер-документ, разд. 112) и «Плановое списание» (`docs/DARC_vyezd_1_design.md`); аномалии из всех источников — часть всегда, часть вариантами через RareEventManager. Там же список кода, который ещё нужен для среза.
2. **WorldMemory + RareEventManager + Task System** — полное ТЗ уже готово в `docs/PROMPT_dlya_silnoy_modeli.txt`. Прочитать целиком перед началом работы над любой из этих трёх систем.
3. Дальше — по `docs/plan_razrabotki.md` (полный роадмап всех этапов разработки).

## Ассеты
- Статус поиска и что ещё нужно — `docs/DARC_assets_checklist.md`.
- Анимации уже выбраны (один большой пак с покрытием большинства нужных движений).
- Часть ссылок, которые автор находил сам (Fab/RawCatalog/Poliigon), при проверке оказались платными или не по теме — при добавлении новых ассетов в проект всегда проверяй лицензию на странице, не полагайся на название файла.

## Правила кода
- Сервер всегда авторитетен. Любое действие клиента идёт через Server RPC с ревалидацией на сервере (дистанция, состояние, права) — клиентским данным не доверяем.
- Комментарии в коде — на русском.
- Текст для игрока — всегда через String Table (`Content/Localization/ST_UI.csv`, перевод `Content/Localization/ST_UI_EN.csv`), никогда не строкой в C++/Blueprint напрямую.
