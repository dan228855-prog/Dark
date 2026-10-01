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
	void AddWall(const FVector2D& A, const FVector2D& B, const TArray<float>& DoorCenters = {});
	void AddRoom(const FVector2D& Min, const FVector2D& Max);

	// --- Игровые объекты (только сервер) ---
	void BuildGameplay();
	AInteractableDoor* SpawnDoor(const FVector2D& WallPoint, bool bWallAlongX, bool bLocked, FName MemoryId);
	ADarcPowerLamp* SpawnLamp(const FVector& Location, FName CircuitId);
	void SpawnRoomVolume(FName RoomId, const FVector2D& Min, const FVector2D& Max);

	template <class T>
	T* SpawnDeferred(const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);
	void Finish(AActor* Actor, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

	bool bGeometryBuilt = false;
};
