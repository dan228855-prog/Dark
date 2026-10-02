// DarcAssetSettings.h
// Таблица подключения ассетов: «слот → модель / материал / звук». Хранится текстом в
// Config/DefaultGame.ini (секция [/Script/DARK.DarcAssetSettings]), поэтому её можно
// заполнять без редактора — по списку ассетов из Tools/darc_setup.py.
// Пустой слот = серая коробка / тишина: срез играбелен и без единого ассета.
// Также видно в редакторе: Project Settings → Game → DARC Assets.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DarcAssetSettings.generated.h"

class UStaticMesh;
class UMaterialInterface;
class USoundBase;
class USkeletalMesh;
class UAnimationAsset;
class USoundAttenuation;
class UDataTable;
class UDarcMissionDefinition;
class UStaticMeshComponent;
class UAudioComponent;

/**
 * Описание видимой модели объекта: слот + размер коробки. Задаётся на сервере при создании
 * и реплицируется один раз (с первым пакетом), а применяет его каждая машина сама —
 * модель, выставленная на сервере в рантайме, по сети не передаётся.
 */
USTRUCT(BlueprintType)
struct DARK_API FDarcVisualSpec
{
	GENERATED_BODY()

	/** Слот модели в DarcAssetSettings (пусто/нет модели — серая коробка). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FName Slot;

	/** Размер коробки, см. Нулевой — не трогать компонент (модель задана вручную). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector Size = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FName Material;

	/** Сдвиг после вписывания (например, створка двери от петли). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector Offset = FVector::ZeroVector;

	/**
	 * Взять модель у актора карты (разметка тегами): та же модель, материалы, место и масштаб;
	 * сам актор карты скрывается. Он загружен с картой у каждого игрока — по сети идёт только ссылка.
	 * Если у него нет статической модели (скелетный NPC, TargetPoint) — он остаётся видимым,
	 * а компонент становится невидимым «телом» по его габаритам (для взгляда и E).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	TObjectPtr<AActor> CopyFrom;

	/**
	 * Текстура привязана к самой коробке, а не к миру. Ставят объекты, которые двигаются
	 * (стойка, генератор, предметы, двери): иначе при движении текстура «плывёт» по ним.
	 * Локально у каждой машины, по сети не передаётся.
	 */
	UPROPERTY(NotReplicated, Transient)
	bool bLocalUV = false;

	void ApplyTo(UStaticMeshComponent* Component) const;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "DARC Assets"))
class DARK_API UDarcAssetSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDarcAssetSettings();

	/** Модели по слотам: Door, FuseBox, Terminal, ServerRack, Generator, Keycard, Drive, Fuse, ... */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, TSoftObjectPtr<UStaticMesh>> Meshes;

	/** Скелетные модели персонажей по слотам (Guard, ...). Есть — NPC показывается человеком, а не коробкой. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, TSoftObjectPtr<USkeletalMesh>> Characters;

	/** Анимации по слотам: <Персонаж>_Idle (Guard_Idle), ... Скелет должен совпадать с моделью. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, TSoftObjectPtr<UAnimationAsset>> Animations;

	/** Материалы по слотам: Wall, Floor, Ceiling, Ground, ... */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, TSoftObjectPtr<UMaterialInterface>> Materials;

	/**
	 * Размер повтора текстуры по слоту материала, см (параметр TileSize материалов из
	 * Tools/darc_setup.py): Ground=300 — трава повторяется раз в 3 м. Нет записи — 200.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, float> MaterialTiling;

	/**
	 * Доворот модели слота вокруг вертикали, градусы: если «лицо» модели смотрит не туда
	 * (монитор боком, щиток к стене) — 90 / -90 / 180. Лицо объекта — локальная +X.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, float> MeshYaw;

	/** Полный доворот модели слота (например, лампа висит «вверх ногами» — Roll 180). Складывается с MeshYaw. */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, FRotator> MeshRotation;

	/** Крупная вариация цвета по слоту материала (0 — выключить; для плитки/сетки, где она даёт «второй слой»). */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, float> MaterialMacroVariation;

	/** Звуки по слотам: DoorOpen, DoorClose, DoorLocked, FuseBlow, Breaker, GeneratorLoop, LampHum, Morse_RU, ... */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, TSoftObjectPtr<USoundBase>> Sounds;

	/** Ассет выезда среза (создаёт Tools/darc_setup.py). */
	UPROPERTY(Config, EditAnywhere, Category = "Slice")
	TSoftObjectPtr<UDarcMissionDefinition> SliceMission;

	/** Таблица редких событий (создаёт Tools/darc_setup.py). */
	UPROPERTY(Config, EditAnywhere, Category = "Slice")
	TSoftObjectPtr<UDataTable> RareEventTable;

	/** Строить серый уровень среза кодом, если на карте нет своего уровня. */
	UPROPERTY(Config, EditAnywhere, Category = "Slice")
	bool bAutoBuildSlice = true;

	/** Запускать выезд сам через несколько секунд после старта (для теста среза). */
	UPROPERTY(Config, EditAnywhere, Category = "Slice")
	bool bAutoStartMission = true;

	static const UDarcAssetSettings* Get() { return GetDefault<UDarcAssetSettings>(); }

	/** Модель слота или nullptr. Загружается синхронно (срез маленький — допустимо). */
	static UStaticMesh* FindMesh(FName Slot);
	static UMaterialInterface* FindMaterial(FName Slot);
	static USkeletalMesh* FindCharacter(FName Slot);
	/** Один из вариантов Slot, Slot_1 … Slot_8 — выбирается по Seed (у всех машин одинаково). */
	static USkeletalMesh* FindCharacterVariant(FName Slot, uint32 Seed);

	/**
	 * Материал слота для серых коробок с нужным повтором текстуры (MaterialTiling).
	 * Пишет в лог, если материал собран старой версией Tools/darc_setup.py (без наложения
	 * по миру — тогда текстура растягивается на всю коробку).
	 */
	static UMaterialInterface* GetTiledMaterial(FName Slot, UObject* Outer, bool bLocalUV = false, const FVector& BoxSize = FVector(100.f));
	static UAnimationAsset* FindAnimation(FName Slot);
	static USoundBase* FindSound(FName Slot);

	/**
	 * Поставить в компонент модель слота, вписав её в коробку BoxSize (см);
	 * если слот пуст — серый куб такого размера. MaterialSlot — необязательный материал.
	 */
	static void ApplyVisual(UStaticMeshComponent* Component, FName Slot, const FVector& BoxSize, FName MaterialSlot = NAME_None, bool bLocalUV = false);

	/**
	 * Для физических тел: у модели нет простой коллизии — физика с ней не работает (предмет
	 * провалится/станет «призраком»). Тогда ставим серую коробку размера Spec.Size и пишем в лог.
	 */
	static void EnsurePhysicsCollision(UStaticMeshComponent* Component, const FDarcVisualSpec& Spec);

	/**
	 * Цветной материал для простых фигур. Glow > 0 — светится (материал слота Emissive,
	 * его создаёт Tools/darc_setup.py: M_DarcEmissive с параметрами Color и Intensity);
	 * без него — обычный цвет. Для индикаторов, ламп, окон, огней.
	 */
	static UMaterialInterface* MakeColorMaterial(UObject* Outer, const FLinearColor& Color, float Glow = 0.f);

	/** Модель/место/масштаб — как у актора карты (см. FDarcVisualSpec::CopyFrom). */
	static void ApplyCopy(UStaticMeshComponent* Component, AActor* Source);

	/** Случайный из вариантов слота: Slot, Slot_1 … Slot_8. nullptr — ни одного. */
	static USoundBase* FindSoundVariant(FName Slot);

	/** Есть ли у слота хоть один звук (сам слот или Slot_1). */
	static bool HasSound(FName Slot);

	/** Общее объёмное затухание для звуков без своих настроек (nullptr — у звука свои). */
	static USoundAttenuation* GetDefaultAttenuation(USoundBase* Sound);

	/** Проиграть звук слота в точке (у этой машины), объёмно. Пустой слот — тишина. */
	static void PlaySound(const UObject* WorldContextObject, FName Slot, const FVector& Location, float Volume = 1.f);

	/** Зацикленный звук на компоненте (гул ламп, генератор). nullptr, если слота нет. */
	static UAudioComponent* PlayLoopAttached(FName Slot, USceneComponent* AttachTo);
};
