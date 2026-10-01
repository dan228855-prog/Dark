# Задание для Claude Code на ПК автора

Ты работаешь на компьютере автора, в папке проекта D.A.R.C. (Unreal Engine 5.8, модуль `DARK`).
Код написан в облачной сессии **без компиляции**. Твоя задача — довести его до рабочего состояния на этой машине.
Сначала прочитай `CLAUDE.md` и `docs/PC_SETUP.md`. Пиши автору по-русски, коротко: он не программист.

## Порядок

1. **Обновиться:** `git pull origin master`.

2. **Найти движок.** Обычно `C:\Program Files\Epic Games\UE_5.8`. Если не там — посмотри в
   `%LOCALAPPDATA%\UnrealEngine\` / реестре `HKLM\SOFTWARE\EpicGames\Unreal Engine\5.8` (InstalledDirectory) или спроси автора.

3. **Собрать (повторять, пока не соберётся без ошибок):**
   ```
   "<UE>\Engine\Build\BatchFiles\Build.bat" DARKEditor Win64 Development -Project="<проект>\DARK.uproject" -WaitMutex -FromMsBuild
   ```
   Ошибки компиляции чини сам в `Source/DARK/`, не ломая архитектуру и правила из `CLAUDE.md`:
   сервер авторитетен, комментарии на русском, текст для игрока — только через String Table (`UDarcGameplayLibrary::UIText`).
   Если не установлен Visual Studio / MSVC — скажи автору, что поставить (VS 2022, workload «Game development with C++»).

4. **Файлы ассетов.** Спроси автора, где лежат скачанные модели/текстуры/звуки. Разложи копии в `RawAssets/`
   (`Meshes/`, `Textures/`, `Sounds/`, зацикленные — `Sounds/Loops/`), архивы распакуй. Папка в `.gitignore` — не коммить.

5. **Настройка в редакторе** (автор должен закрыть редактор, если он открыт):
   ```
   "<UE>\Engine\Binaries\Win64\UnrealEditor.exe" "<проект>\DARK.uproject" -ExecutePythonScript="<проект>\Tools\darc_setup.py"
   ```
   Скрипт создаст карту, импортирует `RawAssets`, таблицы, ассет выезда и запишет `docs/asset_inventory.csv`.
   Лог: `Saved/Logs/DARK.log` (строки `[DARC]`, ошибки Python — `LogPython: Error`). Ошибки скрипта чини в `Tools/darc_setup.py`.

6. **Подключить ассеты.** По `docs/asset_inventory.csv` заполни слоты в `Config/DefaultGame.ini`, секция
   `[/Script/DARK.DarcAssetSettings]`, формат: `Meshes=(("Door", "/Game/Путь/SM_X.SM_X"),("Fuse", "..."))`, так же `Materials=` и `Sounds=`.
   Список слотов — в `Source/DARK/DarcSliceBuilder.cpp` (`Vis(TEXT("…"))`, материалы `Wall/Floor/Ceiling/Ground/Furniture/Booth/DoorMaterial/WindowGlass`)
   и звуков — `DoorOpen, DoorClose, DoorLocked, PickUp, Drop, BreakerSwitch, FuseBlow, GeneratorStart, GeneratorLoop, GeneratorStop,
   LampHum, LampFlicker, AccessGranted, AccessDenied, TransferStart, TransferChecking, TransferComplete, CableAttach, CableDetach,
   SpiritAction`, а звук редкого события = его ID (`Slice_SpeakerMorse` …, см. `docs/data/DT_RareEvents.csv`).
   Подбирай по смыслу и размеру (`size_or_duration`). Ассеты вне `/Game/DARC` добавь в
   `+DirectoriesToAlwaysCook` в `DefaultGame.ini`, иначе не попадут в сборку. Лицензии: см. `docs/DARC_assets_checklist.md`.

7. **Проверка.** Попроси автора открыть проект и нажать Play (соло, затем 2 игрока / Listen Server) и пройти срез по
   разделу «Как пройти срез» в `docs/PC_SETUP.md`. Баги — исправляй.

8. **Сохранить работу:** коммить и пушь в `master` (код, конфиги, `docs/asset_inventory.csv`, `Content/Localization/`).
   Бинарный `Content/` не коммить — он в `.gitignore`.
