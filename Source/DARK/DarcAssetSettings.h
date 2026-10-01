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

	/** Материалы по слотам: Wall, Floor, Ceiling, Ground, ... */
	UPROPERTY(Config, EditAnywhere, Category = "Assets")
	TMap<FName, TSoftObjectPtr<UMaterialInterface>> Materials;

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
	static USoundBase* FindSound(FName Slot);

	/**
	 * Поставить в компонент модель слота, вписав её в коробку BoxSize (см);
	 * если слот пуст — серый куб такого размера. MaterialSlot — необязательный материал.
	 */
	static void ApplyVisual(UStaticMeshComponent* Component, FName Slot, const FVector& BoxSize, FName MaterialSlot = NAME_None);

	/** Проиграть звук слота в точке (у этой машины). Пустой слот — тишина. */
	static void PlaySound(const UObject* WorldContextObject, FName Slot, const FVector& Location);

	/** Зацикленный звук на компоненте (гул ламп, генератор). nullptr, если слота нет. */
	static UAudioComponent* PlayLoopAttached(FName Slot, USceneComponent* AttachTo);
};
