// DarcSliceBuilder.h
// Сборщик серого уровня вертикального среза (docs/DARC_vertical_slice.md) — кодом,
// без ручной работы в редакторе. GameMode создаёт его на пустой карте.
//
// Сеть: сам сборщик реплицируется. Статичная геометрия (пол, стены, потолок, свет неба)
// строится локально на каждой машине одинаково — по сети её гонять незачем. Игровые
// объекты (двери, щиток, терминалы, предметы, NPC) создаёт только сервер, они реплицируются.
//
// Модели/материалы/звуки — по слотам из DarcAssetSettings: пустой слот = серая коробка.
// Когда механики проверены, уровень лучше перенести в обычную карту (это временная сборка).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DarcSliceBuilder.generated.h"

class AInteractableDoor;
class ADarcPowerLamp;
class UStaticMeshComponent;
class UPointLightComponent;

UCLASS(NotPlaceable)
class DARK_API ADarcSliceBuilder : public AActor
{
	GENERATED_BODY()

public:
	ADarcSliceBuilder();

	/** Где появляются игроки (сервер). Index — номер игрока, чтобы не спавнить в одной точке. */
	static FTransform GetPlayerSpawn(int32 Index);

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	// --- Геометрия (на каждой машине) ---
	void BuildGeometry();
	void BuildEnvironment();
	void AddBox(const FVector& Min, const FVector& Max, FName MaterialSlot);
	/** Стена от A до B (по оси X или Y) с проёмами под двери в точках вдоль стены. */
	void AddWall(const FVector2D& A, const FVector2D& B, const TArray<float>& DoorCenters = {}, FName MaterialSlot = TEXT("Wall"));
	void AddRoom(const FVector2D& Min, const FVector2D& Max);

	/** Улица у КПП по референсу docs/reference/mission1_checkpoint.png (на каждой машине). */
	void BuildExterior();
	/** Простая фигура движка (Cube/Cylinder/Cone/Sphere, 100 см) с центром в Center, размером Size, своим цветом. */
	UStaticMeshComponent* AddShape(const TCHAR* Shape, const FVector& Center, const FVector& Size, const FLinearColor& Color,
		const FRotator& Rotation = FRotator::ZeroRotator, bool bCollision = true);
	/** Декоративный свет (не от электросистемы — всегда горит). */
	class UPointLightComponent* AddLight(const FVector& Location, const FLinearColor& Color, float Lumens, float Radius);
	void AddTree(const FVector2D& Location, float Height, float Radius);
	/** Пропс (модель слота, вписанная в BoxSize, низом на BottomCenter) + невидимая коробка-коллизия. */
	UStaticMeshComponent* AddProp(FName MeshSlot, const FVector& BottomCenter, float Yaw, const FVector& BoxSize,
		bool bCollision = true, FName MaterialSlot = NAME_None);
	/** Стена этажа (Z0..Z1) с окнами; LitEvery > 0 — каждое n-е окно светится изнутри. */
	void AddWallWithWindows(const FVector2D& A, const FVector2D& B, float Z0, float Z1, const TArray<float>& WindowCenters,
		FName MaterialSlot, int32 LitEvery);
	void ToggleBlinkLights();

	// --- Игровые объекты (только сервер) ---
	void BuildGameplay();
	AInteractableDoor* SpawnDoor(const FVector2D& WallPoint, bool bWallAlongX, bool bLocked, FName MemoryId);
	ADarcPowerLamp* SpawnLamp(const FVector& Location, FName CircuitId, bool bStreetLight = false);
	void SpawnRoomVolume(FName RoomId, const FVector2D& Min, const FVector2D& Max);

	template <class T>
	T* SpawnDeferred(const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);
	void Finish(AActor* Actor, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);
	/** ВРЕМЕННО: название/подсказка под прицелом (DarcHintComponent). */
	void AddHint(AActor* Actor, FName NameKey, FName HintKey = NAME_None);

	bool bGeometryBuilt = false;

	/** Красные огни на вышке и тарелке — мигают у каждой машины сами. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UPointLightComponent>> BlinkLights;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> BlinkMeshes;

	/** Фоновый звук (музыка, ночь, гул серверной) — у каждой машины свой. */
	void StartAmbience();

	FTimerHandle BlinkTimer;
	bool bBlinkOn = true;
};
