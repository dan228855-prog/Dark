# D.A.R.C. — чеклист ассетов: текстуры, модели, звуки

Составлено 28.09.2026 под мастер-документ игры (Уровни 1–7 + база). Условия основных сайтов сверены по источникам 2026 года; по шрифтам, иконкам и архиву BBC — по общеизвестным условиям, отдельно не перепроверялись. Лицензии меняются: перед скачиванием смотрите лицензию на странице конкретного ассета.

---

## Как пользоваться

- ★ — нужно для **вертикального среза** (Уровень 1 «Первая работа»: КПП → админблок → служебный коридор → рабочая комната → серверный сектор). Это много (больше сотни позиций) — для самого первого прототипа хватит короткого набора из раздела «Порядок работы», пункт 1.
- ☆ — понадобится позже (Уровни 2–7, база, эффекты).
- ☐ — ставьте галочку, когда нашли и записали в таблицу учёта (шаблон в конце файла).
- **Одна строка = один слот**, его нужно закрыть хотя бы одним ассетом. Косая черта внутри строки («растрескавшийся/грязный») значит «или»: подойдёт любой вариант. Слова в `кавычках` — синонимы для поиска, а не отдельные позиции. Хотите разнообразия (чтобы одна текстура не повторялась на всех стенах) — берите 2–3 на слот, но это по желанию.
- Сайты англоязычные — вводите в поиск то, что в `таких кавычках` (запросы на английском). Русское название — чтобы вы понимали, что это.
- Не качайте «про запас». Пока срез не заработает на серых боксах, берите только ★.
- Визуальный стиль в мастер-документе пока не зафиксирован. Арт меню задаёт тёмный полуреалистичный тон — список ниже под него. Главное правило: не мешать в одной сцене low-poly и фотореализм.

### Что можно и что нельзя (лицензии, коротко)

- ✅ **CC0 / Public Domain** — можно всё, автора указывать не нужно.
- ✅ **Royalty-free / Fab Standard License / Pixabay License / Sonniss** — можно в коммерческой игре. Общий запрет: нельзя перепродавать или раздавать файлы «как есть».
- ⚠️ **CC-BY** — можно, но автора нужно указать в титрах. Записывайте каждого автора.
- ❌ **CC-BY-NC**, «только личное/некоммерческое использование», «editorial only» — в игру на Steam нельзя.
- ❌ Всё, что вытащено из чужих игр, фильмов, YouTube и т.п.
- ❌ Архив BBC Sound Effects — по его условиям только для личного, учебного и исследовательского использования, для коммерческой игры не подходит.
- ⚠️ Если репозиторий Git публичный — не выкладывайте туда исходники ассетов с лицензией «нельзя раздавать» (Fab, Sonniss и т.п.). В приватном — можно.

---

# ГЛАВА 1. ТЕКСТУРЫ

## 1.1 Сайты

- **Poly Haven** — https://polyhaven.com — CC0. Текстуры, HDRI и модели, без регистрации. Лучшее место, чтобы начать.
- **ambientCG** — https://ambientcg.com — CC0. Самая большая библиотека материалов, есть HDRI и раздел декалей (Decals).
- **ShareTextures** — https://www.sharetextures.com — CC0.
- **3DTextures.me** — https://3dtextures.me — CC0.
- **cgbookcase** — https://www.cgbookcase.com — CC0.
- **Kenney** — https://kenney.nl — CC0. Простые прототипные текстуры и сетки для блокаута (серых боксов).
- **Fab** — https://fab.com — встроен в UE5 (плагин Fab в редакторе) и есть в браузере. Включайте фильтр Free (бесплатно). Часть Megascans с 2025 года платная, часть бесплатная — цену и лицензию смотрите на карточке. У ассетов бывает деление Personal / Professional (граница по доходу, порядка $100 тыс. в год) — читайте условия карточки.
- ❌ **FreePBR** — бесплатно только для личного использования; для коммерческой игры платная лицензия. Не брать без покупки.
- ❌ **Poliigon** — бесплатные ассеты по лицензии только для личного/образовательного использования, для игры на Steam нельзя. Полный доступ только по платной подписке.
- ⚠️ **RawCatalog** — в основном платная площадка (продаёт паки текстур и на своём сайте, и на Unity Asset Store по ~$5 за пак). Если в списке есть ссылка на rawcatalog.com — проверяйте на странице цену и лицензию перед использованием, бесплатным может быть не всё.

## 1.2 Как скачивать (для UE5)

- Разрешение: **2K (2048×2048)** для большинства поверхностей. 4K — только для крупных, часто видимых мест. Игра на 5 игроков + процедурные комнаты — экономьте память.
- Карты: Color/Albedo, **Normal**, Roughness, AO, при необходимости Metallic и Height.
- **Normal в UE нужен в формате DirectX (DX).** На Poly Haven и ambientCG есть варианты GL и DX — берите DX.
- Roughness, AO, Metallic, Height — в свойствах текстуры отключайте sRGB (Color/Albedo оставляйте как есть).
- Любой **текст на вывесках и табличках не запекайте в картинку** — у вас RU и EN. Делайте вывески как отдельный материал/декаль с текстом из String Table либо две версии картинки. Иначе локализация сломается.
- Логотип D.A.R.C., вывески, номера помещений, этикетки — лучше нарисовать самому (Krita, GIMP, Inkscape — бесплатно): и права ваши, и версии RU/EN.

## 1.3 Что нужно найти (RU — EN)

### Здания: стены, полы, потолки

- ☐ ★ Бетон гладкий/серый — `concrete wall`, `smooth concrete`, `concrete floor`
- ☐ ★ Бетон растрескавшийся/грязный — `cracked concrete`, `dirty concrete`, `stained concrete`
- ☐ ★ Бетонные панели (промышленный/советский стиль) — `concrete panels`, `prefab concrete panel`
- ☐ ★ Штукатурка / крашеная стена — `plaster wall`, `painted wall`
- ☐ ★ Облупленная краска — `peeling paint`, `flaking paint`
- ☐ ★ Кирпич — `brick wall`, `old brick`
- ☐ ★ Кафельная плитка (санузлы, служебные) — `ceramic tiles`, `wall tiles`, `bathroom tiles`
- ☐ ★ Напольная плитка — `floor tiles`
- ☐ ★ Линолеум / ПВХ-покрытие — `linoleum`, `vinyl flooring`
- ☐ ★ Ковролин офисный — `office carpet`, `commercial carpet`
- ☐ ★ Потолочные панели («армстронг») — `acoustic ceiling tiles`, `drop ceiling`
- ☐ ☆ Паркет / ламинат — `parquet`, `wood floor` (жилые здания)
- ☐ ☆ Обои — `wallpaper` (жилые/старые здания)
- ☐ ☆ Мрамор / гранит (лобби) — `marble`, `granite`

### Металл и промышленное

- ☐ ★ Ржавый металл — `rusty metal`, `rust`
- ☐ ★ Окрашенный металл, облупленный — `painted metal`, `chipped paint metal`
- ☐ ★ Рифлёный лист — `diamond plate`, `tread plate`, `checker plate`
- ☐ ★ Профнастил / гофрированный металл — `corrugated metal`, `corrugated iron`
- ☐ ★ Металлическая решётка — `metal grate`, `steel grating`
- ☐ ☆ Нержавейка / алюминий — `stainless steel`, `brushed metal`
- ☐ ☆ Чугун, люки — `cast iron`, `manhole cover`
- ☐ ☆ Резина, прорезиненные покрытия — `rubber floor`, `rubber mat`
- ☐ ☆ Изоляция кабелей (резина/пластик) — `cable insulation`, `rubber`, `plastic`

### Улица и природа (Уровень 1: лес, КПП, дорога)

- ☐ ★ Мокрый асфальт — `wet asphalt`, `asphalt`
- ☐ ★ Треснувший асфальт — `cracked asphalt`, `damaged road`
- ☐ ★ Гравий — `gravel`, `crushed stone`
- ☐ ★ Земля / грязь — `dirt`, `mud`, `ground`
- ☐ ★ Лесная подстилка (хвоя, листья) — `forest floor`, `pine needles`, `leaf litter`
- ☐ ★ Трава — `grass`
- ☐ ☆ Мох — `moss`
- ☐ ☆ Кора дерева (для ближних деревьев) — `tree bark`, `pine bark`
- ☐ ☆ Камень / скала — `rock`, `cliff rock`
- ☐ ☆ Песок — `sand`
- ☐ ☆ Бетонные блоки / бордюр — `concrete barrier`, `curb`

### Дерево, пластик, ткань, картон

- ☐ ★ Старые доски — `old wood planks`, `weathered wood`
- ☐ ★ Фанера / ДСП / OSB — `plywood`, `particle board`, `OSB`
- ☐ ★ Картон (коробки) — `cardboard`
- ☐ ☆ Пластик матовый/глянцевый — `plastic`, `matte plastic`
- ☐ ☆ Ткань (обивка) — `fabric`, `upholstery`
- ☐ ☆ Кожа / кожзам — `leather`, `faux leather`
- ☐ ☆ Бумага (для документов) — `paper`
- ☐ ☆ Чёрная ткань / занавес (Уровень 4, сцена) — `black curtain`, `velvet curtain`

### Декали и «грязь» (дешёвый способ убрать пластиковость сцены)

Это **текстуры (картинки), не 3D-модели**. В UE их накладывают на стены и пол через Decal Actor, отдельная геометрия не нужна.

**Где искать:**
- **ambientCG** (CC0, бесплатно): https://ambientcg.com → Surfaces → в фильтре Type отметьте Decal. Полезные категории: `Leaking`, `Smear`, `Scratches`, `Chip`, `Asphalt Damage`, `Road Lines`, `Sticker`, `Sign`.
- **Fab**: https://www.fab.com/category/material/damage-grunge → подкатегории Dirt, Leakage, Stain, Grunge, Damage и тип Decals. Большинство паков платные, включайте фильтр цены Free. В разделе Offers → Limited-Time Free иногда раздают платные наборы бесплатно.
- Нужны файлы с прозрачным фоном (альфа-канал), иначе вокруг пятна будет видна рамка.
- В UE: материал с Material Domain = Deferred Decal, ставится на сцену через Decal Actor.

- ☐ ★ Потёки, грязь, разводы — `dirt streaks`, `grime`, `stains`
- ☐ ★ Ржавые подтёки — `rust streaks`
- ☐ ★ Трещины на стенах и полу — `cracks decal`, `wall cracks`
- ☐ ★ Пятна воды / сырости — `water stains`, `damp stains`
- ☐ ★ Лужи / мокрые пятна — `puddle`, `wet decal`
- ☐ ★ Масляные пятна — `oil stains`
- ☐ ☆ Копоть / гарь — `soot`, `burn marks`
- ☐ ☆ Царапины, потёртости — `scratches`, `scuffs`, `wear`
- ☐ ☆ Отвалившаяся штукатурка — `damaged plaster`, `exposed brick decal`
- ☐ ☆ Разметка пола — `floor markings`, `safety lines`
- ☐ ☆ Предупреждающие полосы — `hazard stripes`, `warning stripes`
- ☐ ☆ Трафаретные номера помещений — `stencil numbers`, `door numbers` (лучше сделать самому — см. 1.2)
- ☐ ☆ Пыль / паутина — `dust`, `cobweb`

### Маски и прозрачное

- ☐ ★ Сетка-рабица (забор на КПП) — `chain link fence` (opacity mask)
- ☐ ☆ Решётки вентиляции — `vent grill`
- ☐ ☆ Грязное / треснувшее стекло — `dirty glass`, `cracked glass`
- ☐ ☆ Капли на стекле — `rain drops on glass`, `water droplets`

### Небо и свет (HDRI)

- ☐ ★ Ночное небо с облаками и луной — `HDRI night sky`, `moon`, `cloudy night`
- ☐ ★ Сумерки (вечерний приезд на Уровень 1) — `HDRI dusk`, `twilight`, `overcast evening`
- ☐ ☆ Пасмурное небо — `overcast HDRI`
- ☐ ☆ Интерьерные HDRI (для тестов освещения) — `indoor HDRI`, `warehouse HDRI`, `office HDRI`

### Экраны, терминалы, камеры

- ☐ ★ Строки и шум старого монитора — `CRT scanlines`, `screen noise`, `TV static`
- ☐ ☆ Помехи / глитч — `glitch texture`, `digital noise`, `VHS noise`
- ☐ ☆ Стикеры / этикетки / штрихкоды — `barcode labels`, `asset tag stickers` (генерируйте бесплатными генераторами)
- Текст терминалов, hex-дампы, логи — качать не нужно: это UMG-шрифт (моно) и String Table.

---
# ГЛАВА 2. 3D-МОДЕЛИ

## 2.1 Сайты

- **Fab** — https://fab.com — фильтр Free. Есть прямо в редакторе UE5. Проверяйте лицензию (Standard / Creative Commons) и совместимость с версией UE 5.x.
- **Poly Haven (Models)** — https://polyhaven.com — CC0. Фотореалистичный реквизит: мебель, бытовые предметы, растения.
- **Sketchfab** — https://sketchfab.com — включайте фильтры Downloadable + лицензия CC0 или CC-BY. **Лицензия у каждой модели своя**: NC и «Standard» не брать; CC-BY — записать автора в титры.
- ⚠️ **RawCatalog** — та же площадка, что в главе 1: в основном платная (паки по ~$5 на Unity Asset Store). Ссылки на rawcatalog.com проверяйте на цену и лицензию на странице перед скачиванием.
- **Quaternius** — https://quaternius.com — CC0. Стилизованный low-poly: природа, персонажи, животные, модульные наборы.
- **Kenney** — https://kenney.nl/assets — CC0. Простые low-poly наборы для прототипа.
- **OpenGameArt** — https://opengameart.org — лицензии разные. Берите только CC0 или CC-BY.
- **itch.io (бесплатные ассеты)** — https://itch.io/game-assets/free — лицензия указана на странице каждого набора.
- **Встроенное в UE5** — манекены Manny/Quinn (шаблон Third Person/First Person) и Starter Content. Годятся как заглушки на время прототипа.

## 2.2 Как выбирать

- Формат: FBX или glTF, лучше сразу с PBR-материалами. Масштаб в UE — сантиметры: дверь примерно 200 см высотой, не приходится подгонять.
- Для процедурной генерации комнат берите **модульные наборы** (`modular kit`), а не готовые комнаты: стены, полы, двери на одной сетке.
- Смотрите количество полигонов и наличие LOD. Много мелких предметов в комнате — берите лёгкие.
- Сразу думайте про физические классы из документа: крупное/тяжёлое (A), переносимое (B), мелкий реквизит, который двигает «дух»-полтергейст (C). Мелочи должны быть дешёвыми — их будет много.
- Стиль один на весь проект. Перед массовым скачиванием положите 3–5 моделей в тестовую сцену и посмотрите, что они уживаются.

## 2.3 Что нужно найти (RU — EN)

### Ядро вертикального среза: то, с чем играют

- ☐ ★ Дверь: служебная металлическая / офисная — `metal door`, `office door`, `security door`
- ☐ ★ Дверная рама / стена с проёмом — `door frame`, `wall with doorway`
- ☐ ★ Терминал / старый компьютер (системник + ЭЛТ-монитор) — `old computer`, `CRT monitor`, `retro computer terminal`, `workstation`
- ☐ ★ Клавиатура, мышь (старые) — `keyboard`, `mouse`
- ☐ ★ Серверная стойка и серверные блоки — `server rack`, `server cabinet`, `rack server`, `network switch`
- ☐ ★ **Носитель данных как физический предмет** (ключевой предмет заданий) — `external hard drive`, `USB flash drive`, `data cartridge`, `tape drive`, `HDD`
- ☐ ★ Кейс для носителя — `hard case`, `briefcase`, `pelican case`
- ☐ ★ Электрощит / распределительный щиток — `electrical panel`, `fuse box`, `breaker box`, `distribution board`
- ☐ ★ Предохранители — `fuse`, `cartridge fuse`, `circuit breaker`
- ☐ ★ Рубильник / рычаг — `power lever`, `knife switch`, `industrial switch`
- ☐ ★ Кабели, бухты — `cable`, `power cable`, `cable spool`, `cable reel`
- ☐ ★ Блок питания / генератор — `power supply`, `generator`, `diesel generator`, `UPS`
- ☐ ★ Розетки, вилки, разъёмы — `power outlet`, `plug`, `connector`
- ☐ ★ Лампы: потолочные люминесцентные, аварийные — `fluorescent light`, `ceiling lamp`, `emergency light`
- ☐ ★ Фонарик (инструмент игрока) — `flashlight`, `torch`
- ☐ ★ Ключ-карта и считыватель — `keycard`, `card reader`, `access panel`
- ☐ ★ Кодовая панель / замок — `keypad`, `digital lock`, `padlock`
- ☐ ★ Рация — `walkie talkie`, `handheld radio`
- ☐ ☆ Ключи — `key`, `key ring`

### Уровень 1: территория (лес, КПП, здание с тарелкой)

- ☐ ★ Будка КПП / проходная — `guard booth`, `guardhouse`, `checkpoint booth`
- ☐ ★ Шлагбаум — `boom barrier`, `parking barrier`, `gate arm`
- ☐ ★ Забор-рабица, столбы, колючая проволока — `chain link fence`, `barbed wire`, `fence posts`
- ☐ ★ Ворота — `sliding gate`, `metal gate`
- ☐ ★ Вывеска / щит на столбах — `signboard`, `metal sign` (текст RU/EN — делайте сами)
- ☐ ★ Уличный фонарь — `street lamp`, `lamp post`
- ☐ ★ Столб (деревянный/ЛЭП) — `utility pole`, `wooden pole`, `power lines`
- ☐ ★ Спутниковая тарелка (крупный ориентир) — `satellite dish`, `radio telescope`, `radar dish`
- ☐ ★ Административное здание (крупное, бетонное, старое) — `concrete office building`, `brutalist building`, `old research facility` (проще собрать из модульного набора + фасадных текстур)
- ☐ ★ Хозпостройки — `shed`, `storage shed`, `garage`
- ☐ ★ Хвойные деревья (ель/сосна) — `spruce`, `pine tree`, `conifer forest`
- ☐ ★ Кусты, папоротник, трава — `bush`, `fern`, `grass clumps`, `undergrowth`
- ☐ ★ Бетонные блоки / ограждения — `concrete barrier`, `jersey barrier`, `road block`
- ☐ ☆ Автомобиль (тёмный, как на арте меню) — `car`, `SUV`, `van`
- ☐ ☆ Кот (как на арте меню, необязательно) — `cat`, `animated cat`

### Интерьеры: админблок, коридоры, рабочие комнаты, серверный сектор

- ☐ ★ Модульный набор «офис / коридор» — `modular office kit`, `modular corridor kit`, `abandoned office`
- ☐ ★ Модульный набор «серверная / техпомещение» — `server room kit`, `technical room`, `industrial interior`
- ☐ ★ Стол и стул офисные — `office desk`, `office chair`
- ☐ ★ Шкафы канцелярские / архивные, металлические — `filing cabinet`, `archive cabinet`, `metal locker`
- ☐ ★ Стеллажи — `shelving`, `metal shelf`, `rack`
- ☐ ★ Ящики, коробки, паллеты — `crates`, `cardboard boxes`, `wooden pallets`
- ☐ ★ Мусор и мелочи-декор (класс C) — `trash`, `papers`, `folders`, `stationery`, `coffee mug`, `trash bin`
- ☐ ★ Вентиляция, короба, трубы — `air vent`, `duct`, `ventilation grille`, `pipes`, `cable tray`
- ☐ ★ Огнетушитель, аптечка, знаки эвакуации — `fire extinguisher`, `first aid kit`, `exit sign`
- ☐ ★ Лестницы, перила — `stairs`, `railing`, `ladder`
- ☐ ☆ Бочки, канистры — `barrel`, `oil drum`, `jerrycan`
- ☐ ☆ Кулер, автомат с кофе / снеками — `water cooler`, `vending machine`, `coffee machine`
- ☐ ☆ Диваны, кресла — `sofa`, `armchair`, `couch` (база, жилые здания)
- ☐ ☆ Кровати, шкафы, кухня — `bed`, `wardrobe`, `kitchen furniture`, `fridge` (жилые здания)
- ☐ ☆ Плакаты, доска объявлений, часы — `poster`, `notice board`, `wall clock`

### Уровень 2: промзона, ангар, робот

- ☐ ☆ Модульный набор «ангар / цех» — `industrial hangar kit`, `factory kit`, `warehouse`
- ☐ ☆ Мостовой кран / тельфер — `overhead crane`, `hoist`
- ☐ ☆ Погрузчик, тележки — `forklift`, `pallet jack`, `cart`, `trolley`
- ☐ ☆ Металлоконструкции, мостики, леса — `catwalk`, `scaffolding`, `steel structure`, `gantry`
- ☐ ☆ Станки, конвейер — `industrial machine`, `lathe`, `conveyor belt`
- ☐ ☆ **Огромный старый гуманоидный робот** — решили: целые модели роботов, а не разобранные на части — `industrial humanoid robot`, `giant robot`, `mech`, `broken robot`, `derelict robot`.

### Уровень 3: сенсорная территория

- ☐ ☆ Датчики, антенны, мачты — `sensor tower`, `antenna`, `radar sensor`, `weather station`, `radio mast`
- ☐ ☆ Уличные электрошкафы, подстанции — `outdoor electrical cabinet`, `transformer box`, `substation`
- ☐ ☆ Опоры, линии кабелей — `pylon`, `cable line`

### Уровень 4: зал и сцена

- ☐ ☆ Сцена, световые фермы, прожекторы — `stage`, `lighting truss`, `stage lights`, `spotlight`
- ☐ ☆ Колонки, микшерный пульт, микрофонные стойки — `PA speakers`, `mixing console`, `microphone stand`
- ☐ ☆ Кресла зала — `auditorium seats`, `theatre seats`
- ☐ ☆ Занавес — `stage curtain`

### Уровень 5: наблюдение

- ☐ ☆ Камеры видеонаблюдения — `CCTV camera`, `security camera`
- ☐ ☆ Стена мониторов, пульт оператора — `monitor wall`, `control desk`, `operator console`, `CCTV monitors`
- ☐ ☆ Мониторы, старые телевизоры — `monitor`, `flat screen`, `old TV`

### Уровни 6–7: подземка, диспетчерская, энергокомплекс

- ☐ ☆ Модульный набор «тоннель / бункер / подземный коридор» — `tunnel kit`, `bunker kit`, `underground corridor`, `sewer`
- ☐ ☆ Диспетчерская: пульты, панели, приборы со стрелками — `control room`, `control panel`, `analog gauges`, `switch panels`
- ☐ ☆ Трансформаторы, распределительные узлы — `transformer`, `switchgear`, `power station equipment`, `high voltage`
- ☐ ☆ Переборки, шлюзовые и хранилищные двери — `bulkhead door`, `airlock door`, `vault door`

### Инструменты игрока (набор в документе не зафиксирован — добавляйте под механику)

- ☐ ☆ Универсальный набор — `screwdriver`, `wrench`, `pliers`, `hammer`, `crowbar`, `bolt cutters`, `multimeter`, `duct tape`, `wire cutter`, `soldering iron`, `tool box`, `ladder`

### Персонажи

- ☐ ★ Игрок-заглушка — манекен UE (Manny/Quinn) из шаблона; использовать до финального арта
- ☐ ☆ Охранник КПП (усталый сотрудник) — `security guard`, `tired employee`, `uniform`
- ☐ ☆ Сотрудники D.A.R.C. / куратор — `office worker`, `employee`, `technician`, `man in suit`
- ☐ ☆ Манекены / силуэты для «не персонажей» (Уровни 6–7) — `mannequin`, `silhouette`, `faceless figure`
- Источники персонажей: Mixamo (авторигинг, https://www.mixamo.com), Quaternius (CC0, стилизованные), бесплатные пакеты на Fab.

---
# ГЛАВА 3. ЗВУКИ

## 3.1 Сайты

- **Sonniss GameAudioGDC** — https://sonniss.com (раздел GameAudioGDC) — бесплатные пакеты студийных звуков, выходят каждый год к GDC (есть и пакет 2026 года). Royalty-free, можно в коммерческой игре, авторство не нужно, число проектов не ограничено. Нельзя: перепродавать файлы «как есть» и использовать для обучения ИИ. Лучший источник, чтобы закрыть большую часть эффектов.
- **Freesound** — https://freesound.org — лицензия у каждого звука своя (CC0, CC-BY, CC-BY-NC). **Фильтруйте по CC0** или берите CC-BY с записью автора. CC-BY-NC — не брать.
- **Kenney (аудио)** — https://kenney.nl/assets — CC0. Клики интерфейса, удары, простые эффекты.
- **Pixabay (звуки и музыка)** — https://pixabay.com/sound-effects/ и https://pixabay.com/music/ — Pixabay License: можно в коммерческой игре без указания авторства; нельзя выкладывать сам файл отдельно как свой.
- **Mixkit** — https://mixkit.co — собственная бесплатная лицензия Mixkit (прочитайте условия), удобно для интерфейсных звуков.
- **Incompetech (музыка)** — https://incompetech.com — CC-BY, **нужно указать автора** в титрах.
- **ZapSplat** — https://www.zapsplat.com — бесплатный уровень требует указания «ZapSplat» в титрах.
- **OpenGameArt** — https://opengameart.org — лицензия у каждого файла своя; берите CC0 / CC-BY.
- **itch.io (бесплатные аудиопаки)** — https://itch.io/game-assets/free (тег Sound Effects) — лицензия на странице набора.
- **Fab** — https://fab.com — фильтры Audio + Free.

## 3.2 Технические заметки

- Для звуков **в мире** (двери, шаги, терминалы, электрика) берите/конвертируйте в **моно** — тогда UE применяет расстояние и панораму. Для фонов и музыки — стерео.
- Для часто повторяющихся звуков (шаги, двери, удары) нужны **3–5 вариантов** и лёгкий разброс высоты тона (Sound Cue / MetaSounds).
- Фоны — бесшовные петли (loop).
- Озвучка на двух языках: держите файлы RU и EN раздельно с одинаковыми именами и подключайте через локализацию UE.
- Автоматические звуки из электросистемы (дух мигает светом, отключение питания) делайте из одних и тех же звуков — так «дух» звучит как часть мира.

## 3.3 Что нужно найти (RU — EN)

### Шаги и движение

- ☐ ★ Шаги по бетону / плитке / асфальту — `footsteps concrete`, `footsteps tile`, `footsteps asphalt`
- ☐ ★ Шаги по металлу (лист, решётка) — `footsteps metal`, `footsteps grate`
- ☐ ★ Шаги по ковролину — `footsteps carpet`
- ☐ ★ Шаги по гравию, земле, листве, траве — `footsteps gravel`, `footsteps dirt`, `footsteps leaves`, `footsteps grass`
- ☐ ★ Шаги по луже / мокрому — `footsteps puddle`, `footsteps wet`, `water splash`
- ☐ ☆ Шаги по дереву / линолеуму — `footsteps wood`, `footsteps linoleum`
- ☐ ★ Прыжок, приземление, приседание, шорох одежды — `jump land`, `crouch`, `cloth movement`, `rustle`
- ☐ ☆ Бег, дыхание — `running`, `breathing`

### Двери и замки

- ☐ ★ Металлическая дверь: открыть / закрыть / скрип — `metal door open`, `metal door close`, `door creak`, `door squeak`
- ☐ ★ Офисная дверь — `wooden door open close`, `office door`
- ☐ ★ Замок: щелчок, поворот ключа, «заперто» (дёрнули ручку) — `lock click`, `key turn`, `door locked rattle`, `door handle jiggle`
- ☐ ★ Электрозамок / магнитный — `electronic lock`, `magnetic lock`, `door release buzz`
- ☐ ★ Ключ-карта: успех / отказ — `keycard swipe`, `access granted beep`, `access denied buzz`
- ☐ ★ Кодовая панель: нажатия / ввод / ошибка — `keypad beeps`, `keypad enter`, `keypad error`
- ☐ ★ Шлагбаум — `boom barrier`, `barrier gate motor`
- ☐ ☆ Ворота, решётка, сетка-рабица — `gate rattle`, `chain link fence rattle`, `metal gate slide`
- ☐ ☆ Шлюз / переборка / хранилище — `airlock hiss`, `bulkhead door`, `vault door`

### Электрика (в документе — полноценная система)

- ☐ ★ Рубильник, автомат — щелчок — `breaker switch`, `knife switch`, `switch click`, `lever`
- ☐ ★ Предохранитель: вставить / перегорел — `fuse insert`, `fuse blow`, `pop`
- ☐ ★ Отключение питания (спад гула) — `power down`, `power off`, `shutdown`, `blackout`
- ☐ ★ Включение питания — `power on`, `power up`, `surge`
- ☐ ★ Гул люминесцентных ламп — `fluorescent light hum`, `buzzing light`
- ☐ ★ Мигание, перегорание лампы — `light flicker`, `bulb pop`, `tube flicker`
- ☐ ★ Искры, короткое замыкание — `electric spark`, `short circuit`, `arc zap`
- ☐ ★ Гул трансформатора / щита — `transformer hum`, `electrical box buzz`, `mains hum`
- ☐ ★ Генератор: запуск / работа / остановка — `generator start`, `generator loop`, `generator stop`, `diesel engine`
- ☐ ☆ Высоковольтный треск — `high voltage crackle`, `electric arc`, `corona`
- ☐ ☆ Аварийная сирена — `alarm siren`, `emergency alarm`, `klaxon`
- ☐ ☆ Электрический разряд по игроку — `electric shock`, `zap`

### Электроника, терминалы, данные

- ☐ ★ Клавиатура — `keyboard typing`, `mechanical keyboard`, `old keyboard clicks`
- ☐ ★ Включение старого ПК / ЭЛТ-монитора — `CRT monitor turn on`, `degauss`, `computer startup`, `PC boot`
- ☐ ★ Гул серверной, вентиляторы — `server room ambience`, `fan hum`, `server fans`
- ☐ ★ Диск, копирование данных, прогресс — `hard drive spin`, `HDD seek`, `data transfer`, `progress beeps`
- ☐ ★ Подключить / извлечь накопитель — `USB plug in`, `USB unplug`, `connector click`, `insert cartridge`
- ☐ ★ Сигналы: ошибка / успех / доступ — `system error beep`, `success chime`, `access granted`, `access denied`
- ☐ ☆ Помехи, статика, глитч — `static`, `glitch`, `digital noise`, `signal interference`
- ☐ ☆ Принтер, сканер, факс — `printer`, `scanner`, `fax machine`
- ☐ ☆ Старый телефон — `old telephone ring`, `landline`
- ☐ ☆ Камеры наблюдения (моторчик, поворот) — `camera motor`, `camera servo`, `surveillance camera pan` (Уровень 5)

### Предметы и физика (классы A/B/C)

- ☐ ★ Взять / положить (лёгкое, тяжёлое, металл, картон) — `pick up`, `put down`, `item drop`, `object grab`
- ☐ ★ Удары и падения: металл, дерево, бетон, пластик — `impact metal`, `impact wood`, `impact concrete`, `impact plastic`, `metal clang`, `dropped object thud`
- ☐ ★ Волочение ящика — `drag crate`, `scrape`, `slide on floor`
- ☐ ★ Картон, бумага, ткань — `cardboard`, `paper rustle`, `cloth`
- ☐ ★ Стекло: звон / разбилось — `glass break`, `glass shards`
- ☐ ★ Инструменты — `wrench`, `screwdriver`, `crowbar`, `tool clank`
- ☐ ★ Рация: включить / отбой — `radio click`, `squelch`, `walkie talkie beep`, `PTT`
- ☐ ☆ Ключи звенят — `keys jingle`
- ☐ ☆ Катящийся предмет / бочка — `rolling barrel`, `can roll`

### Опасности

- ☐ ☆ Обвал, осыпание — `rock fall`, `debris collapse`, `cave-in`, `rubble`
- ☐ ☆ Скрежет, стон металла — `metal groan`, `structural creak`, `metal stress`
- ☐ ☆ Пар, газ, утечка — `steam hiss`, `gas leak`, `pipe burst`
- ☐ ☆ Капли, протечка — `water drip`, `leak`, `dripping`
- ☐ ☆ Треск бетона — `concrete crack`, `ceiling crack`

### Атмосфера (ambience, петли)

- ☐ ★ Ночной лес, ветер, лёгкий дождь — `night forest ambience`, `wind in trees`, `light rain`
- ☐ ★ «Комнатный тон» офиса / коридора, вентиляция — `room tone`, `office ambience`, `HVAC hum`, `ventilation`
- ☐ ★ Серверная — `server room ambience`
- ☐ ☆ Промзона / ангар — `industrial ambience`, `hangar reverb`, `factory hum`
- ☐ ☆ Подземелье / тоннель — `tunnel ambience`, `underground drone`, `dripping water`
- ☐ ☆ Большой зал — `large hall ambience`, `empty auditorium`
- ☐ ☆ Жилая квартира — `apartment room tone`, `fridge hum`
- ☐ ☆ База (тихий «домашний» фон, который потом можно искажать) — `calm room tone`, `night ambience`

### Аномалии и «неправильность» (по документу: без дешёвых скримеров, без драматичной музыки)

- ☐ ★ Низкочастотный гул — `low drone`, `sub bass rumble`, `infrasound hum`
- ☐ ☆ Тональный писк / интерференция — `tonal whine`, `high pitch interference`
- ☐ ☆ Шёпот, обрывок голоса — `whisper`, `distant voice`, `murmur`, `voice fragment` (лучше записать самому — нейтрально к языку)
- ☐ ☆ Звук «не оттуда»: шаги в пустой комнате, стук за стеной — `footsteps in empty room`, `knock in wall`, `distant footsteps`
- ☐ ☆ Обратные / замедленные звуки — `reversed sound`, `slowed audio`, `pitched down`
- ☐ ☆ Цифровые артефакты (для E.V.A.) — `data glitch`, `digital artifact`, `modem`, `code`
- ☐ ☆ Помехи рации / искажённый голос — `radio interference`, `radio static`, `distorted voice`
- Внезапная тишина (вырез фона) — качать не нужно, делается в UE.

### Интерфейс (UI)

- ☐ ★ Клик, наведение, подтверждение, отмена — `UI click`, `hover`, `confirm`, `back`, `cancel`
- ☐ ★ Новая задача / цель выполнена — `notification`, `task complete`, `objective complete`
- ☐ ★ Ошибка / недоступно — `UI error`, `denied`
- ☐ ☆ Открыть / закрыть меню — `menu open`, `menu close`
- ☐ ☆ Печать текста в терминале — `terminal typing`, `text scroll`, `teletype`

### Голос и озвучка

- ★ На срезе озвучка не нужна: субтитры + короткие шумы.
- ☐ ☆ Куратор (постоянный голос, выдаёт задания; RU + EN) — `curator voice`
- ☐ ☆ Охранник Уровня 1: «Опять новые работники?» — RU и EN версии реплики
- ☐ ☆ Системный голос E.V.A. — `synthetic voice`, `computer voice`
- ☐ ☆ Объявления по громкой связи — `PA announcement`, `intercom`
- Варианты записи: (1) самому или с друзьями — Audacity, бесплатно: шумоподавление + обрезка тишины; (2) синтез речи — **обязательно проверять лицензию конкретной модели/голоса на коммерческое использование** (многие бесплатные модели только некоммерческие); (3) по документу история идёт без длинных текстов — можно обойтись короткими фразами.
- Эффект рации в Audacity: срезать всё ниже примерно 300 Гц и выше примерно 3000 Гц (фильтр полосы) + лёгкая дисторсия.

### Музыка (в документе: без драматической музыки, местами вообще без музыки — нужно мало)

- ☐ ★ Главное меню — `dark ambient`, `cinematic drone`, `cold ambient`, `industrial ambient`, `tension ambient`
- ☐ ☆ База (тихая, повторяющаяся) — `calm ambient`, `low-key ambient`, `night ambient`
- ☐ ☆ Финальная заставка «TO BE CONTINUED» (после Уровня 7) — `ending sting`, `credits ambient`
- ☐ ☆ Музыка внутри мира (радио, зал Уровня 4, корпоративный джингл) — `old radio music`, `elevator music`, `lounge`, `corporate jingle`, `hold music`
- ☐ ☆ Звук логотипа D.A.R.C. — `logo sting`, `glitch logo` (можно сделать самому)
- Слушайте, чтобы в треке не было резких вступлений и нарастающих ударных — это против принципов документа.

---

# ГЛАВА 4 (дополнительно). АНИМАЦИИ

Для моделей персонажей нужны движения. От камеры зависит объём: если камера от первого лица (в документе не решено) — нужны только руки и их анимации; если от третьего — тело целиком.

## Сайты

- **Mixamo** — https://www.mixamo.com — бесплатно с Adobe ID, авторигинг, royalty-free для коммерческих игр. Нельзя перепродавать. Сервис не развивается (обновлений ждать не стоит), но работает.
- **Quaternius Universal Animation Library 1 и 2** — https://quaternius.com — CC0, 250+ анимаций, ретаргетятся в Unreal.
- **Шаблоны UE5** — анимации Manny/Quinn (Third Person / First Person).
- **Fab** — https://fab.com — бесплатные пакеты анимаций (лицензия на карточке).
- Ретаргет между скелетами в UE5 — инструменты IK Rig / IK Retargeter (встроены, бесплатно).

## Что нужно (RU — EN)

- ☐ ★ Стойка / ходьба / бег — `idle`, `walk`, `run`
- ☐ ★ Приседание и движение в приседе — `crouch idle`, `crouch walk`
- ☐ ★ Прыжок, приземление — `jump`, `land`
- ☐ ★ Взаимодействие: кнопка, рычаг, дверь — `interact`, `press button`, `pull lever`, `open door`
- ☐ ★ Подобрать / положить — `pick up`, `put down`
- ☐ ★ Переноска (лёгкая, тяжёлая, двумя руками) — `carry idle`, `carry walk`, `carry heavy`, `carry box`
- ☐ ★ Работа с инструментом — `use tool`, `repair`, `crouch working`, `hammering`
- ☐ ☆ Толкать / тянуть — `push`, `pull`, `push cart`
- ☐ ☆ Лестница — `climb ladder`
- ☐ ☆ Потеря сознания, падение, лежит (переход в «дух»-спектатор) — `fall`, `knocked out`, `downed`, `lying on floor`
- ☐ ☆ Смерть — `death`, `fall back`
- ☐ ☆ Испуг, оглядывание — `look around`, `startled`, `flinch`
- ☐ ☆ Ввод на терминале — `typing`, `use computer`
- ☐ ☆ Жесты для кооп-игры — `point`, `wave`, `thumbs up`
- ☐ ☆ Рация — `talk radio`, `hold radio`
- ☐ ☆ Охранник: стоит, опирается, проверяет документы — `security guard idle`, `leaning`, `checking papers`

---

# ГЛАВА 5 (дополнительно). ШРИФТЫ, ИКОНКИ, UI

## Шрифты (важно: нужна кириллица)

- **Google Fonts** — https://fonts.google.com — включите фильтр Language → Cyrillic. Лицензия OFL — можно в коммерческой игре; положите файл лицензии рядом со шрифтом.
- Для терминалов и логов (моноширинные): `IBM Plex Mono`, `JetBrains Mono`, `PT Mono`, `Roboto Mono`.
- Для заголовков: `Oswald`, `Exo 2`, `Russo One`, `Jura`.
- Проверяйте, что в шрифте есть **все русские буквы, включая «ё»**. Проверка: вставьте в предпросмотр «Съешь ещё этих мягких французских булок».
- Один шрифт на оба языка — не придётся менять шрифты при переключении RU/EN.

## Иконки

- **Kenney (UI и иконки)** — https://kenney.nl/assets — CC0.
- **Google Material Symbols** — https://fonts.google.com/icons — Apache 2.0.
- **game-icons.net** — https://game-icons.net — CC BY 3.0, **автора нужно указать**.

## Апскейл картинок (UI, арт)

- **Upscayl** — https://upscayl.org/download — бесплатный, с открытым кодом, работает офлайн (Windows, macOS, Linux). Нужна видеокарта с поддержкой Vulkan (на слабых встроенных может не заработать). Есть русский интерфейс. Модели дорисовывают детали, а не просто растягивают пиксели.

## Чтобы картинки в UE не были мыльными

Для фона меню и любой UI-графики в настройках текстуры (двойной клик по ней): **Texture Group = UI**, **Compression Settings = UserInterface2D (RGBA)**, **Mip Gen Settings = NoMipmaps**, галочка **Never Stream**. С настройками по умолчанию даже картинка нормального разрешения может выглядеть мутной.

---

# ГЛАВА 6. ЛИЦЕНЗИИ, STEAM, УЧЁТ

## Steam и ИИ-контент

Если что-то из того, что видит или слышит игрок, создано генеративным ИИ (арт, текстуры, озвучка, тексты), это нужно указать в форме Steamworks (раздел про Generative AI content) — запись публично появится на странице игры. Помощники по коду (как Claude Code) по правилам, обновлённым в январе 2026, указывать не нужно. Перед заполнением страницы проверьте актуальную версию правил Steam.

## Таблица учёта ассетов (копируйте и ведите в отдельном файле CREDITS)

| Ассет | Сайт | Ссылка на страницу | Лицензия | Автор | Нужно указать в титрах? | Путь в проекте |
|---|---|---|---|---|---|---|
| пример: Concrete Wall 03 | Poly Haven | https://polyhaven.com/... | CC0 | — | нет | /Content/Env/Materials/ |

Правила учёта:
- Записывайте сразу при скачивании — потом не вспомните.
- Сохраняйте скриншот страницы с лицензией — условия иногда меняются.
- Всё с CC-BY: автор + название + ссылка → в титры в игре и на странице Steam.

---

# ПОРЯДОК РАБОТЫ

1. **Самый первый заход (прототип: дверь, терминал, носитель данных, щиток) — около 15 позиций:**
   - текстуры: бетон, ржавый и окрашенный металл, мокрый асфальт, декали грязи;
   - модели: дверь + рама, старый ПК/терминал, серверная стойка, накопитель, электрощит + предохранитель, потолочная лампа, фонарик;
   - звуки: шаги по бетону и металлу, дверь открыть/закрыть, ключ-карта или кодовая панель, щелчок рубильника, гул и мигание ламп, клик интерфейса.
2. Дальше остальные ★ — это полный Уровень 1: лес, КПП, забор, здание с тарелкой, админблок, коридор, серверная.
3. Пока ассетов нет — серые боксы и встроенные манекены UE. Заменяйте по мере готовности механик.
4. Один заход на поиск: текстуры — Poly Haven + ambientCG; модели — Fab (Free) + Poly Haven; звуки — Sonniss GDC + Freesound (CC0) + Kenney.
5. Перед массовым скачиванием положите 3–5 ассетов в тестовую сцену и проверьте, что стиль сходится.
6. Ведите таблицу учёта с первого файла.

---

# ВСЕ САЙТЫ ОДНИМ СПИСКОМ

Текстуры: https://polyhaven.com · https://ambientcg.com · https://www.sharetextures.com · https://3dtextures.me · https://www.cgbookcase.com · https://kenney.nl · https://fab.com
Модели: https://fab.com · https://polyhaven.com · https://sketchfab.com · https://quaternius.com · https://kenney.nl/assets · https://opengameart.org · https://itch.io/game-assets/free
Звуки: https://sonniss.com · https://freesound.org · https://kenney.nl/assets · https://pixabay.com/sound-effects/ · https://mixkit.co · https://incompetech.com · https://www.zapsplat.com
Музыка: https://pixabay.com/music/ · https://incompetech.com · https://opengameart.org
Анимации: https://www.mixamo.com · https://quaternius.com
Шрифты и иконки: https://fonts.google.com · https://fonts.google.com/icons · https://kenney.nl/assets · https://game-icons.net
Апскейл: https://upscayl.org/download
