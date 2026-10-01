# Разметка карты тегами — памятка

Строишь уровень в редакторе как обычно: стены, пол, мебель — любые модели. Игровые объекты
(двери, щитки, терминалы, предметы, комнаты) **не настраиваешь вручную**, а просто вешаешь
на них теги. При запуске игры каждый помеченный объект сам превращается в рабочий — с той же
моделью, на том же месте, с тем же размером.

## Как поставить тег

1. Выделить объект на карте.
2. Details → в поиске набрать **Tags** (раздел Actor → Advanced → Tags).
3. **+** → вписать тег. Несколько тегов — несколько строк.

Пример: дверь серверной, запертая, с именем, на которое сошлются считыватель и кодовая панель:

```
DARC.Door.Locked
Id=ServerDoor
```

## Правила

- Первый тег — **что это**: `DARC.<Тип>`. Через точку можно добавить флаги: `DARC.Door.Locked.Open`.
- Остальные теги — **параметры** `Ключ=Значение`: `Circuit=Server`, `Task=CopyArchive`.
- Список — через запятую без пробелов: `Collateral=Corridor,Lobby`.
- Ссылка на другой объект — по его `Id`: `Door=ServerDoor`. Id должен быть уникальным.
- Тексты — **ключами** из `Content/Localization/ST_UI.csv` (`Title=Screen_CatalogTitle`), не словами.
- Регистр не важен. Неизвестный тип или неверная ссылка — предупреждение в Output Log (фильтр `DARC markup`), игра не падает.
- Если на карте есть хоть один тег `DARC.` — серый срез кодом не строится. Нужны свои **Player Start**.
- У движущихся объектов (предметы, тяжёлое, двери) у модели должна быть коллизия (Simple Collision) — без неё физика не работает. Проверить: открыть модель → Show → Simple Collision.

## Типы

### Двери, электрика

| Тег | Параметры | Что получится |
|---|---|---|
| `DARC.Door` | флаги `.Locked`, `.Open`; `Id=`, `Hinge=Left/Right/Pivot` (где петля, по умолчанию Left), `Angle=95` | дверь, открывается E. Помечать **только створку**, не коробку |
| `DARC.Lamp` | `Circuit=Building` | лампа на цепи: гаснет/мигает вместе с цепью |
| `DARC.FuseBox` | `Circuit=Server` (обязательно); флаги `.NoFuse` (без предохранителя), `.Off` (рубильник выключен); `Collateral=Corridor` (что гаснет, если сжечь предохранитель); `TaskOnBlow=RestoreCorridorLight` | щиток с рубильником и предохранителем |
| `DARC.Fuse` | `Id=` | предохранитель (предмет) |
| `DARC.Generator` | `Mass=70` | генератор: тащить ЛКМ, подключить к вводу |
| `DARC.Inlet` | `Circuit=Server`, `Cable=600` | ввод для генератора |

### Предметы и тяжёлое

| Тег | Параметры | Что получится |
|---|---|---|
| `DARC.Item` | `Id=` | обычный переносимый предмет (E — взять) |
| `DARC.Keycard` | `Id=`, `Code=CatalogCode` | карта доступа, на ней 4 цифры кода выезда |
| `DARC.Drive` | `Id=` | носитель данных |
| `DARC.Module` | `Id=` | интерфейсный модуль |
| `DARC.Heavy` | `Mass=80`, флаг `.NoCable`, `Deliver=<Id точки>`, `Radius=150`, `Task=` | тяжёлый физический объект (стойка, шкаф). С кабелем — при рывке кабель срывается |
| `DARC.Point` | `Id=` | просто точка (цель доставки). Удобно — Target Point. В игре скрыта |

### Устройства

| Тег | Параметры | Что получится |
|---|---|---|
| `DARC.Terminal` | `Code=CatalogCode` или `FixedCode=1234`; флаг `.Unlocked`; `Task=`, `Door=<Id двери>`, `Circuit=`, `Title=`, `LockedTitle=`, `Lines=ключ,ключ`, `Words=ключ,ключ` (искажённые слова), `Commands=COPY:TakeExtraFile` | терминал с вводом кода |
| `DARC.Keypad` | как терминал; по умолчанию `Code=ServerDoorCode` | кодовая панель двери |
| `DARC.CardReader` | `Door=`, `Task=`, `Accepts=Keycard` | считыватель карты |
| `DARC.Slot` | `Accepts=Drive/Module/Fuse/Keycard/Item`, `Holds=<Id предмета>` (что уже вставлено), флаг `.Fixed` (нельзя вынуть), `Task=` (вставили), `TaskRemove=` (вынули) | разъём под предмет |
| `DARC.Station` | `Interface=<Id слота>`, `DriveSlot=<Id слота>`, `Task=`, `Circuit=Server`, `Flicker=Corridor`, `Seconds=40` | станция передачи данных |
| `DARC.CodeSign` | `Code=ServerDoorCode`, `Size=20` | табличка: поверх неё — цифры кода (на переднюю сторону, ось X объекта) |

### Люди, комнаты, аномалии

| Тег | Параметры | Что получится |
|---|---|---|
| `DARC.Npc` | `Speaker=Speaker_Guard`, `Greeting=ключи`, `Gives=<Id предмета>`, `TaskGive=`, `GiveLines=`, `Accepts=Drive`, `BeforeReceive=`, `TaskReceive=`, `ReceiveLines=`, `Idle=` | персонаж с репликами (субтитры). Можно ставить на скелетную модель — она останется как есть |
| `DARC.Room` | `Id=ServerRoom` (обязательно) | комната для памяти мира и событий. Ставить на Trigger Box / Box по размеру комнаты |
| `DARC.Anchor` | `Events=Speaker,WindowLight`, `Room=`, `Behavior=SoundOnly/LightOn/Levitate/VanishingRoom`, `Target=<Id>` или `TargetTag=тег`, `Door=<Id двери>`, `Duration=40` | точка редкого события. На модели (динамик, окно) — с моделью; на Target Point — невидимая |

Для левитации: на предмет (кружку) повесить обычный тег, например `LevitationMug`, а у якоря — `TargetTag=LevitationMug`.

## Срез на своей карте — какие Id и задачи

Задачи выезда среза (`docs/data/DT_Mission_Slice_Tasks.csv`) ждут таких Id задач — их и вписывать в `Task=`:

| Объект | Теги |
|---|---|
| Терминал каталога | `DARC.Terminal`, `Code=CatalogCode`, `Task=OpenCatalog`, `Circuit=Building`, `Title=Screen_CatalogTitle`, `Words=File_Corrupted_1,File_Corrupted_2,File_Corrupted_3,File_Corrupted_4`, `Lines=File_VerifyNote,File_MainStorage` |
| Дверь серверной | `DARC.Door.Locked`, `Id=ServerDoor` |
| Считыватель | `DARC.CardReader`, `Door=ServerDoor`, `Task=ServerDoorCard` |
| Кодовая панель | `DARC.Keypad`, `Door=ServerDoor`, `Task=ServerDoorKeypad` |
| Табличка с кодом | `DARC.CodeSign`, `Code=ServerDoorCode` |
| Щитки | `DARC.FuseBox.NoFuse`, `Circuit=Server`, `Collateral=Corridor`, `TaskOnBlow=RestoreCorridorLight` · `DARC.FuseBox`, `Circuit=Corridor` · `DARC.FuseBox`, `Circuit=Building` |
| Стойка | `DARC.Heavy`, `Deliver=RackPoint`, `Task=MoveServerRack` + `DARC.Point`, `Id=RackPoint` |
| Модуль и разъём | `DARC.Module`, `Id=InterfaceModule` · `DARC.Slot`, `Id=InterfaceSlot`, `Accepts=Module`, `Task=ConnectInterface` |
| Носитель и разъём | `DARC.Drive`, `Id=ArchiveDrive` · `DARC.Slot`, `Id=DriveSlot`, `Accepts=Drive`, `Holds=ArchiveDrive`, `TaskRemove=TakeDrive` |
| Станция | `DARC.Station`, `Interface=InterfaceSlot`, `DriveSlot=DriveSlot`, `Task=CopyArchive` |
| Консоль сервера | `DARC.Terminal.Unlocked`, `Circuit=Server`, `Title=Screen_ArchiveTitle`, `Lines=Screen_ArchiveList,Screen_ExtraFileHint`, `Commands=COPY:TakeExtraFile` |
| Карта и охранник | `DARC.Keycard`, `Id=AccessCard` · `DARC.Npc`, `Speaker=Speaker_Guard`, `Greeting=Guard_Greeting`, `Gives=AccessCard`, `TaskGive=GetAccessCard`, `GiveLines=Guard_GiveCard`, `Accepts=Drive`, `BeforeReceive=Guard_Finished`, `TaskReceive=DeliverDrive`, `ReceiveLines=Guard_Good,Guard_GoBack` |
| Генератор и ввод | `DARC.Generator` · `DARC.Inlet`, `Circuit=Server` |
| Комнаты | `DARC.Room` + `Id=` одно из: `Outside`, `Lobby`, `Corridor`, `WorkRoom`, `TechCabinetRoom`, `TechRoom`, `Storage`, `ServerRoom` |

## Проверка

Play → в Output Log строка `DARC markup: N tagged actors, M gameplay objects created`. Предупреждения
(`unknown type`, `object with this Id not found`) — опечатка в теге. Можно попросить Claude Code
на ПК: «проверь разметку на карте» — он пройдёт по логу и тегам.
